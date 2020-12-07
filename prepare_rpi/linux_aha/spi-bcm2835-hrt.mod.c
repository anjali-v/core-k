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
	{ 0xb077e70a, "clk_unprepare" },
	{ 0x815588a6, "clk_enable" },
	{ 0x2e5810c6, "__aeabi_unwind_cpp_pr1" },
	{ 0xb6e6d99d, "clk_disable" },
	{ 0xcabdb02d, "devm_ioremap_resource" },
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
	{ 0xa98896d4, "devm_spi_register_controller" },
	{ 0x556e4390, "clk_get_rate" },
	{ 0x1ee1c008, "__spi_alloc_controller" },
	{ 0x797ef069, "__platform_driver_register" },
	{ 0x526c3a6c, "jiffies" },
	{ 0xe707d823, "__aeabi_uidiv" },
	{ 0x7c32d0f0, "printk" },
	{ 0x2b4a59be, "platform_get_resource" },
	{ 0xb024e90c, "_dev_err" },
	{ 0x2631c33a, "put_device" },
	{ 0x7c9a7371, "clk_prepare" },
	{ 0x7c29a9e0, "devm_clk_get" },
	{ 0xc37335b0, "complete" },
	{ 0xae829fb0, "platform_get_irq" },
	{ 0xaf4b8832, "platform_driver_unregister" },
	{ 0xffd1e578, "devm_request_threaded_irq" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("of:N*T*Cbrcm,bcm2835-spi");
MODULE_ALIAS("of:N*T*Cbrcm,bcm2835-spiC*");

MODULE_INFO(srcversion, "F2F74CD5916828CE32ACDC0");
