#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
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

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x2c635209, "module_layout" },
	{ 0x53363e9, "single_release" },
	{ 0x11b64b24, "seq_lseek" },
	{ 0x223983ec, "seq_read" },
	{ 0x60a13e90, "rcu_barrier" },
	{ 0x8e4b90cd, "unregister_shrinker" },
	{ 0x5c5f6242, "remove_proc_entry" },
	{ 0x2d5f69b3, "rcu_read_unlock_strict" },
	{ 0x367b5b62, "proc_create" },
	{ 0x8006b741, "kmem_cache_destroy" },
	{ 0x17427c29, "register_shrinker" },
	{ 0x4b1c40fe, "kmem_cache_create" },
	{ 0x15ba50a6, "jiffies" },
	{ 0x6c3777ff, "kmem_cache_alloc" },
	{ 0xba8fbd64, "_raw_spin_lock" },
	{ 0x71038ac7, "pv_ops" },
	{ 0x296695f, "refcount_warn_saturate" },
	{ 0x28aa6a67, "call_rcu" },
	{ 0x92997ed8, "_printk" },
	{ 0x2e1637a6, "kmem_cache_free" },
	{ 0xcbd4898c, "fortify_panic" },
	{ 0xdd64e639, "strscpy" },
	{ 0xa916b694, "strnlen" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0xaa44a707, "cpumask_next" },
	{ 0x9e683f75, "__cpu_possible_mask" },
	{ 0xb19a5453, "__per_cpu_offset" },
	{ 0x17de3d5, "nr_cpu_ids" },
	{ 0xc876a99f, "seq_printf" },
	{ 0x10ea38cd, "single_open" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "AE0E8527D124DCA17A833A9");
