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
	{ 0x2e5810c6, "__aeabi_unwind_cpp_pr1" },
	{ 0xcabdb02d, "devm_ioremap_resource" },
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
	{ 0x556e4390, "clk_get_rate" },
	{ 0x797ef069, "__platform_driver_register" },
	{ 0xd1774b97, "i2c_add_adapter" },
	{ 0xe707d823, "__aeabi_uidiv" },
	{ 0x607ec658, "_dev_warn" },
	{ 0x7c32d0f0, "printk" },
	{ 0x73e20c1c, "strlcpy" },
	{ 0x2b4a59be, "platform_get_resource" },
	{ 0xd6b8e852, "request_threaded_irq" },
	{ 0xb024e90c, "_dev_err" },
	{ 0xd166d4dd, "i2c_del_adapter" },
	{ 0xee43fd9b, "___ratelimit" },
	{ 0x7c29a9e0, "devm_clk_get" },
	{ 0x870d5a1c, "__init_swait_queue_head" },
	{ 0xc37335b0, "complete" },
	{ 0xaf4b8832, "platform_driver_unregister" },
	{ 0xe459395b, "of_property_read_variable_u32_array" },
	{ 0x367a6dbb, "devm_kmalloc" },
	{ 0xa16b21fb, "wait_for_completion_timeout" },
	{ 0x32b2cf5d, "param_ops_uint" },
	{ 0xc1514a3b, "free_irq" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("of:N*T*Cbrcm,bcm2835-i2c");
MODULE_ALIAS("of:N*T*Cbrcm,bcm2835-i2cC*");

MODULE_INFO(srcversion, "A1693B7EDC1FC5E56755921");
