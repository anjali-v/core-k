#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xcecd155a, "module_layout" },
	{ 0xd6771ce1, "hrtimer_cancel" },
	{ 0x31a08020, "hrtimer_start_range_ns" },
	{ 0x63c43c16, "hrtimer_init" },
	{ 0x6df1aaf1, "kernel_sigaction" },
	{ 0xce6790ea, "bcm2835_i2c_get_adapter" },
	{ 0xa95c3e7e, "bcm2835aux_spi_get_spi" },
	{ 0x253671a, "bcm2835aux_spi_get_master" },
	{ 0xb84e51d4, "bcm2835_spi_get_spi" },
	{ 0xf2d16952, "bcm2835_spi_get_master" },
	{ 0xdf6ce82e, "gpiod_direction_input" },
	{ 0xc1d3e87f, "rt_spin_trylock" },
	{ 0x2fa8df23, "hrtimer_forward" },
	{ 0x66fa14, "bcm2835_spi_transfer_one" },
	{ 0x1f10bcb4, "bcm2835_spi_prepare_message" },
	{ 0xedae10b9, "bcm2835aux_spi_transfer_one" },
	{ 0xc5a0dfa6, "bcm2835aux_spi_prepare_message" },
	{ 0x658c84d7, "proc_create" },
	{ 0x9e4ba241, "proc_mkdir" },
	{ 0x899223e4, "remove_proc_entry" },
	{ 0x8e865d3c, "arm_delay_ops" },
	{ 0x940c73c4, "bcm2835_i2c_xfer" },
	{ 0xfe46064c, "gpiod_get_raw_value" },
	{ 0x8d9c9c1e, "gpio_to_desc" },
	{ 0xe44ac6ed, "rt_spin_unlock" },
	{ 0xaa17765f, "rt_spin_lock" },
	{ 0x5f754e5a, "memset" },
	{ 0x1e047854, "warn_slowpath_fmt" },
	{ 0x28cc25db, "arm_copy_from_user" },
	{ 0xf4fa543b, "arm_copy_to_user" },
	{ 0x2e5810c6, "__aeabi_unwind_cpp_pr1" },
	{ 0x7c32d0f0, "printk" },
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=i2c-bcm2835-hrt,spi-bcm2835aux-hrt,spi-bcm2835-hrt";


MODULE_INFO(srcversion, "619219F8217799C4DA7819A");
