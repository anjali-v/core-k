/*
 * Driver for Broadcom BCM2835 SPI Controllers
 *
 * Copyright (C) 2012 Chris Boot
 * Copyright (C) 2013 Stephen Warren
 * Copyright (C) 2015 Martin Sperl
 *
 * This driver is inspired by:
 * spi-ath79.c, Copyright (C) 2009-2011 Gabor Juhos <juhosg@openwrt.org>
 * spi-atmel.c, Copyright (C) 2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <asm/page.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/spi/spi.h>

/* SPI register offsets */
#define BCM2835_SPI_CS			0x00
#define BCM2835_SPI_FIFO		0x04
#define BCM2835_SPI_CLK			0x08
#define BCM2835_SPI_DLEN		0x0c
#define BCM2835_SPI_LTOH		0x10
#define BCM2835_SPI_DC			0x14

/* Bitfields in CS */
#define BCM2835_SPI_CS_LEN_LONG		0x02000000
#define BCM2835_SPI_CS_DMA_LEN		0x01000000
#define BCM2835_SPI_CS_CSPOL2		0x00800000
#define BCM2835_SPI_CS_CSPOL1		0x00400000
#define BCM2835_SPI_CS_CSPOL0		0x00200000
#define BCM2835_SPI_CS_RXF		0x00100000
#define BCM2835_SPI_CS_RXR		0x00080000
#define BCM2835_SPI_CS_TXD		0x00040000
#define BCM2835_SPI_CS_RXD		0x00020000
#define BCM2835_SPI_CS_DONE		0x00010000
#define BCM2835_SPI_CS_LEN		0x00002000
#define BCM2835_SPI_CS_REN		0x00001000
#define BCM2835_SPI_CS_ADCS		0x00000800
#define BCM2835_SPI_CS_INTR		0x00000400
#define BCM2835_SPI_CS_INTD		0x00000200
#define BCM2835_SPI_CS_DMAEN		0x00000100
#define BCM2835_SPI_CS_TA		0x00000080
#define BCM2835_SPI_CS_CSPOL		0x00000040
#define BCM2835_SPI_CS_CLEAR_RX		0x00000020
#define BCM2835_SPI_CS_CLEAR_TX		0x00000010
#define BCM2835_SPI_CS_CPOL		0x00000008
#define BCM2835_SPI_CS_CPHA		0x00000004
#define BCM2835_SPI_CS_CS_10		0x00000002
#define BCM2835_SPI_CS_CS_01		0x00000001

#define BCM2835_SPI_POLLING_LIMIT_US	30
#define BCM2835_SPI_POLLING_JIFFIES	2
#define BCM2835_SPI_DMA_MIN_LENGTH	96
#define BCM2835_SPI_MODE_BITS	(SPI_CPOL | SPI_CPHA | SPI_CS_HIGH \
				| SPI_NO_CS | SPI_3WIRE)

#define DRV_NAME	"spi-bcm2835"


/* shit code */
static struct spi_master *pMaster=NULL;
static struct spi_device *pSpi=NULL;

struct bcm2835_spi {
	void __iomem *regs;
	struct clk *clk;
	int irq;
	const u8 *tx_buf;
	u8 *rx_buf;
	int tx_len;
	int rx_len;
	bool dma_pending;
};

static inline u32 bcm2835_rd(struct bcm2835_spi *bs, unsigned reg)
{
	return readl(bs->regs + reg);
}

static inline void bcm2835_wr(struct bcm2835_spi *bs, unsigned reg, u32 val)
{
	writel(val, bs->regs + reg);
}

static inline void bcm2835_rd_fifo(struct bcm2835_spi *bs)
{
	u8 byte;

	while ((bs->rx_len) &&
	       (bcm2835_rd(bs, BCM2835_SPI_CS) & BCM2835_SPI_CS_RXD)) {
		byte = bcm2835_rd(bs, BCM2835_SPI_FIFO);
		if (bs->rx_buf)
			*bs->rx_buf++ = byte;
		bs->rx_len--;
	}
}

static inline void bcm2835_wr_fifo(struct bcm2835_spi *bs)
{
	u8 byte;

	while ((bs->tx_len) &&
	       (bcm2835_rd(bs, BCM2835_SPI_CS) & BCM2835_SPI_CS_TXD)) {
		byte = bs->tx_buf ? *bs->tx_buf++ : 0;
		bcm2835_wr(bs, BCM2835_SPI_FIFO, byte);
		bs->tx_len--;
	}
}

static void bcm2835_spi_reset_hw(struct spi_master *master)
{
	struct bcm2835_spi *bs = spi_master_get_devdata(master);
	u32 cs = bcm2835_rd(bs, BCM2835_SPI_CS);
	printk(KERN_INFO "%s\n", __FUNCTION__);

	/* Disable SPI interrupts and transfer */
	cs &= ~(BCM2835_SPI_CS_INTR |
		BCM2835_SPI_CS_INTD |
		BCM2835_SPI_CS_DMAEN |
		BCM2835_SPI_CS_TA);
	/* and reset RX/TX FIFOS */
	cs |= BCM2835_SPI_CS_CLEAR_RX | BCM2835_SPI_CS_CLEAR_TX;

	/* and reset the SPI_HW */
	bcm2835_wr(bs, BCM2835_SPI_CS, cs);
	/* as well as DLEN */
	bcm2835_wr(bs, BCM2835_SPI_DLEN, 0);
}

static irqreturn_t bcm2835_spi_interrupt(int irq, void *dev_id)
{
	struct spi_master *master = dev_id;
	struct bcm2835_spi *bs = spi_master_get_devdata(master);
	printk(KERN_INFO "%s\n", __FUNCTION__);

	/* Read as many bytes as possible from FIFO */
	bcm2835_rd_fifo(bs);
	/* Write as many bytes as possible to FIFO */
	bcm2835_wr_fifo(bs);

	/* based on flags decide if we can finish the transfer */
	if (bcm2835_rd(bs, BCM2835_SPI_CS) & BCM2835_SPI_CS_DONE) {
		/* Transfer complete - reset SPI HW */
		bcm2835_spi_reset_hw(master);
		/* wake up the framework */
		complete(&master->xfer_completion);
	}

	return IRQ_HANDLED;
}

static int bcm2835_spi_transfer_one_poll(struct spi_master *master,
					 struct spi_device *spi,
					 struct spi_transfer *tfr,
					 u32 cs,
					 unsigned long long xfer_time_us)
{
	struct bcm2835_spi *bs = spi_master_get_devdata(master);
	unsigned long timeout;
	printk(KERN_INFO "%s\n", __FUNCTION__);
	/* enable HW block without interrupts */
	bcm2835_wr(bs, BCM2835_SPI_CS, cs | BCM2835_SPI_CS_TA);

	/* fill in the fifo before timeout calculations
	 * if we are interrupted here, then the data is
	 * getting transferred by the HW while we are interrupted
	 */
	bcm2835_wr_fifo(bs);

	/* set the timeout */
	timeout = jiffies + BCM2835_SPI_POLLING_JIFFIES;

	/* loop until finished the transfer */
	while (bs->rx_len) {
		/* fill in tx fifo with remaining data */
		bcm2835_wr_fifo(bs);

		/* read from fifo as much as possible */
		bcm2835_rd_fifo(bs);

		/* if there is still data pending to read
		 * then check the timeout
		 */
		if (bs->rx_len && time_after(jiffies, timeout)) {
			printk(KERN_ERR "timeout period reached: jiffies: %lu remaining tx/rx: %d/%d - falling back to interrupt mode\n",
					    jiffies - timeout, bs->tx_len, bs->rx_len);
			return -1;
		}
	}

	/* Transfer complete - reset SPI HW */
	bcm2835_spi_reset_hw(master);
	/* and return without waiting for completion */
	return 0;
}

static int dummy_bcm2835_spi_transfer_one(struct spi_master *master,
				    struct spi_device *spi,
				    struct spi_transfer *tfr)
{
	return 0;
}

static int bcm2835_spi_transfer_one(struct spi_master *master,
				    struct spi_device *spi,
				    struct spi_transfer *tfr)
{
	struct bcm2835_spi *bs = spi_master_get_devdata(master);
	unsigned long spi_hz, clk_hz, cdiv;
	unsigned long spi_used_hz;
	unsigned long long xfer_time_us;
	u32 cs = bcm2835_rd(bs, BCM2835_SPI_CS);
	printk(KERN_INFO "%s\n", __FUNCTION__);
	/* set clock */
	spi_hz = tfr->speed_hz;
	clk_hz = clk_get_rate(bs->clk);

	if (spi_hz >= clk_hz / 2) {
		cdiv = 2; /* clk_hz/2 is the fastest we can go */
	} else if (spi_hz) {
		/* CDIV must be a multiple of two */
		cdiv = DIV_ROUND_UP(clk_hz, spi_hz);
		cdiv += (cdiv % 2);

		if (cdiv >= 65536)
			cdiv = 0; /* 0 is the slowest we can go */
	} else {
		cdiv = 0; /* 0 is the slowest we can go */
	}
	spi_used_hz = cdiv ? (clk_hz / cdiv) : (clk_hz / 65536);
	bcm2835_wr(bs, BCM2835_SPI_CLK, cdiv);

	/* handle all the 3-wire mode */
	if ( spi && (spi->mode & SPI_3WIRE) && (tfr->rx_buf))
		cs |= BCM2835_SPI_CS_REN;
	else
		cs &= ~BCM2835_SPI_CS_REN;

	/* for gpio_cs set dummy CS so that no HW-CS get changed
	 * we can not run this in bcm2835_spi_set_cs, as it does
	 * not get called for cs_gpio cases, so we need to do it here
	 */
	if (spi && (gpio_is_valid(spi->cs_gpio) || (spi->mode & SPI_NO_CS)))
		cs |= BCM2835_SPI_CS_CS_10 | BCM2835_SPI_CS_CS_01;

	/* set transmit buffers and length */
	bs->tx_buf = tfr->tx_buf;
	bs->rx_buf = tfr->rx_buf;
	bs->tx_len = tfr->len;
	bs->rx_len = tfr->len;

	/* calculate the estimated time in us the transfer runs */
	xfer_time_us = (unsigned long long)tfr->len
		* 9 /* clocks/byte - SPI-HW waits 1 clock after each byte */
		* 1000000;
	do_div(xfer_time_us, spi_used_hz);

	return bcm2835_spi_transfer_one_poll(master, spi, tfr,
						     cs, xfer_time_us);
}


static int dummy_bcm2835_spi_prepare_message(struct spi_master *master,
				       struct spi_message *msg)
{
	printk(KERN_INFO "%s: %s\n", DRV_NAME, __FUNCTION__);
	if (msg) pSpi = msg->spi;
	return 0;
}

int bcm2835_spi_prepare_message(struct spi_master *master,
				       struct spi_message *msg)
{
	struct spi_device *spi = msg->spi;
	struct bcm2835_spi *bs = spi_master_get_devdata(master);
	u32 cs = bcm2835_rd(bs, BCM2835_SPI_CS);
	printk(KERN_INFO "%s\n", __FUNCTION__);

	cs &= ~(BCM2835_SPI_CS_CPOL | BCM2835_SPI_CS_CPHA);

	if (spi && (spi->mode & SPI_CPOL))
		cs |= BCM2835_SPI_CS_CPOL;
	if (spi && (spi->mode & SPI_CPHA))
		cs |= BCM2835_SPI_CS_CPHA;

	bcm2835_wr(bs, BCM2835_SPI_CS, cs);

	return 0;
}

static void dummy_bcm2835_spi_handle_err(struct spi_master *master,
				   struct spi_message *msg)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);
}

static void dummy_bcm2835_spi_set_cs(struct spi_device *spi, bool gpio_level)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);
}

static void bcm2835_spi_set_cs(struct spi_device *spi, bool gpio_level)
{
	/*
	 * we can assume that we are "native" as per spi_set_cs
	 *   calling us ONLY when cs_gpio is not set
	 * we can also assume that we are CS < 3 as per bcm2835_spi_setup
	 *   we would not get called because of error handling there.
	 * the level passed is the electrical level not enabled/disabled
	 *   so it has to get translated back to enable/disable
	 *   see spi_set_cs in spi.c for the implementation
	 */

	struct spi_master *master = spi->master;
	struct bcm2835_spi *bs = spi_master_get_devdata(master);
	u32 cs = bcm2835_rd(bs, BCM2835_SPI_CS);
	bool enable;
	printk(KERN_INFO "%s\n", __FUNCTION__);
	/* calculate the enable flag from the passed gpio_level */
	enable = (spi->mode & SPI_CS_HIGH) ? gpio_level : !gpio_level;

	/* set flags for "reverse" polarity in the registers */
	if (spi->mode & SPI_CS_HIGH) {
		/* set the correct CS-bits */
		cs |= BCM2835_SPI_CS_CSPOL;
		cs |= BCM2835_SPI_CS_CSPOL0 << spi->chip_select;
	} else {
		/* clean the CS-bits */
		cs &= ~BCM2835_SPI_CS_CSPOL;
		cs &= ~(BCM2835_SPI_CS_CSPOL0 << spi->chip_select);
	}

	/* select the correct chip_select depending on disabled/enabled */
	if (enable) {
		/* set cs correctly */
		if (spi->mode & SPI_NO_CS) {
			/* use the "undefined" chip-select */
			cs |= BCM2835_SPI_CS_CS_10 | BCM2835_SPI_CS_CS_01;
		} else {
			/* set the chip select */
			cs &= ~(BCM2835_SPI_CS_CS_10 | BCM2835_SPI_CS_CS_01);
			cs |= spi->chip_select;
		}
	} else {
		/* disable CSPOL which puts HW-CS into deselected state */
		cs &= ~BCM2835_SPI_CS_CSPOL;
		/* use the "undefined" chip-select as precaution */
		cs |= BCM2835_SPI_CS_CS_10 | BCM2835_SPI_CS_CS_01;
	}

	/* finally set the calculated flags in SPI_CS */
	bcm2835_wr(bs, BCM2835_SPI_CS, cs);
}

static int dummy_bcm2835_spi_setup(struct spi_device *spi)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);
	return 0;
}

static int bcm2835_spi_setup(struct spi_device *spi)
{
	/*
	 * sanity checking the native-chipselects
	 */
	printk(KERN_INFO "%s\n", __FUNCTION__);
	if (spi->mode & SPI_NO_CS)
		return 0;
	if (gpio_is_valid(spi->cs_gpio))
		return 0;
	if (spi->chip_select > 1) {
		/* error in the case of native CS requested with CS > 1
		 * officially there is a CS2, but it is not documented
		 * which GPIO is connected with that...
		 */
		dev_err(&spi->dev,
			"setup: only two native chip-selects are supported\n");
		return -EINVAL;
	}

	return 0;
}

static int bcm2835_spi_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct bcm2835_spi *bs;
	struct resource *res;
	int err;
	printk(KERN_INFO "%s\n", __FUNCTION__);
	master = spi_alloc_master(&pdev->dev, sizeof(*bs));
	if (!master) {
		dev_err(&pdev->dev, "spi_alloc_master() failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, master);
	printk(KERN_INFO "%s\n", __FUNCTION__);
	master->mode_bits = BCM2835_SPI_MODE_BITS;
	master->bits_per_word_mask = SPI_BPW_MASK(8);
	master->num_chipselect = 3;
	master->setup = dummy_bcm2835_spi_setup;
	master->set_cs = dummy_bcm2835_spi_set_cs;
	master->transfer_one = dummy_bcm2835_spi_transfer_one;
	master->handle_err = dummy_bcm2835_spi_handle_err;
	master->prepare_message = dummy_bcm2835_spi_prepare_message;
	master->dev.of_node = pdev->dev.of_node;

	bs = spi_master_get_devdata(master);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	bs->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(bs->regs)) {
		err = PTR_ERR(bs->regs);
		goto out_master_put;
	}

	bs->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(bs->clk)) {
		err = PTR_ERR(bs->clk);
		dev_err(&pdev->dev, "could not get clk: %d\n", err);
		goto out_master_put;
	}

	bs->irq = platform_get_irq(pdev, 0);
	if (bs->irq <= 0) {
		dev_err(&pdev->dev, "could not get IRQ: %d\n", bs->irq);
		err = bs->irq ? bs->irq : -ENODEV;
		goto out_master_put;
	}

	clk_prepare_enable(bs->clk);

	/* initialise the hardware with the default polarities */
	bcm2835_wr(bs, BCM2835_SPI_CS,
			BCM2835_SPI_CS_CLEAR_RX | BCM2835_SPI_CS_CLEAR_TX);

	err = devm_request_irq(&pdev->dev, bs->irq, bcm2835_spi_interrupt, 0,
			dev_name(&pdev->dev), master);
	if (err) {
		dev_err(&pdev->dev, "could not request IRQ: %d\n", err);
		goto out_clk_disable;
	}

	err = devm_spi_register_master(&pdev->dev, master);
	if (err) {
		dev_err(&pdev->dev, "could not register SPI master: %d\n", err);
		goto out_clk_disable;
	}

	pMaster = master;

	return 0;

out_clk_disable:
	clk_disable_unprepare(bs->clk);
out_master_put:
	spi_master_put(master);
	return err;
}

static int bcm2835_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct bcm2835_spi *bs = spi_master_get_devdata(master);
	printk(KERN_INFO "%s\n", __FUNCTION__);

	/* Clear FIFOs, and disable the HW block */
	bcm2835_wr(bs, BCM2835_SPI_CS,
		   BCM2835_SPI_CS_CLEAR_RX | BCM2835_SPI_CS_CLEAR_TX);

	clk_disable_unprepare(bs->clk);

	return 0;
}

int bcm2835_spi_get_master(struct spi_master ** ppMaster)
{
	if (pMaster) *ppMaster = pMaster;
	else return -1;
	return 0;
}

int bcm2835_spi_get_spi(struct spi_device ** ppSpi)
{
	if (pSpi) *ppSpi = pSpi;
		else return -1;
	return 0;
}

static const struct of_device_id bcm2835_spi_match[] = {
	{ .compatible = "brcm,bcm2835-spi", },
	{}
};
MODULE_DEVICE_TABLE(of, bcm2835_spi_match);

static struct platform_driver bcm2835_spi_driver = {
	.driver		= {
		.name		= DRV_NAME,
		.of_match_table	= bcm2835_spi_match,
	},
	.probe		= bcm2835_spi_probe,
	.remove		= bcm2835_spi_remove,
};
module_platform_driver(bcm2835_spi_driver);

EXPORT_SYMBOL(bcm2835_spi_get_master);
EXPORT_SYMBOL(bcm2835_spi_get_spi);
EXPORT_SYMBOL(bcm2835_spi_prepare_message);
EXPORT_SYMBOL(bcm2835_spi_transfer_one);


MODULE_DESCRIPTION("SPI controller driver for Broadcom BCM2835");
MODULE_AUTHOR("Chris Boot <bootc@bootc.net>");
MODULE_LICENSE("GPL v2");
