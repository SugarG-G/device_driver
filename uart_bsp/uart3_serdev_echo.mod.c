#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
	{ 0xacd1c964, "__serdev_device_driver_register" },
	{ 0x29d9f26e, "cancel_delayed_work_sync" },
	{ 0x2d222da1, "misc_deregister" },
	{ 0xdb760f52, "__kfifo_free" },
	{ 0xefd40e94, "serdev_device_close" },
	{ 0xe2e8065e, "memdup_user" },
	{ 0x11e8972f, "serdev_device_write_buf" },
	{ 0x37a0cba, "kfree" },
	{ 0x9b1485da, "__kmalloc_noprof" },
	{ 0xae577d60, "_raw_spin_lock" },
	{ 0x13d0adf7, "__kfifo_out" },
	{ 0x51a910c0, "arm_copy_to_user" },
	{ 0x800473f, "__cond_resched" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0x1000e51, "schedule" },
	{ 0x647af474, "prepare_to_wait_event" },
	{ 0x49970de8, "finish_wait" },
	{ 0x2cfde9a2, "warn_slowpath_fmt" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0xb2c220de, "_dev_info" },
	{ 0xde55e795, "_raw_spin_lock_irqsave" },
	{ 0xf23fcb99, "__kfifo_in" },
	{ 0xf3d0b495, "_raw_spin_unlock_irqrestore" },
	{ 0x5b88060a, "_dev_warn" },
	{ 0x637493f3, "__wake_up" },
	{ 0x55475dff, "device_property_read_u32_array" },
	{ 0x35165206, "device_property_present" },
	{ 0x933d25b9, "devm_kmalloc" },
	{ 0x5bbe49f4, "__init_waitqueue_head" },
	{ 0xd39fa6ab, "__kfifo_alloc" },
	{ 0x9c423512, "serdev_device_open" },
	{ 0x3d105cc0, "serdev_device_set_flow_control" },
	{ 0x84199790, "serdev_device_set_baudrate" },
	{ 0xffeedf6a, "delayed_work_timer_fn" },
	{ 0xc6f46339, "init_timer_key" },
	{ 0x7f02188f, "__msecs_to_jiffies" },
	{ 0x2d3385d3, "system_wq" },
	{ 0xcf86cdac, "queue_delayed_work_on" },
	{ 0xa2928db5, "misc_register" },
	{ 0x1ba75851, "_dev_err" },
	{ 0x5d014e70, "driver_unregister" },
	{ 0x281823c5, "__kfifo_out_peek" },
	{ 0x314b20c8, "scnprintf" },
	{ 0xb1e1e627, "noop_llseek" },
	{ 0x8a3dbe73, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Ccodex,uart3-echo");
MODULE_ALIAS("of:N*T*Ccodex,uart3-echoC*");

MODULE_INFO(srcversion, "AC2EE4CB1155294A4ED1894");
