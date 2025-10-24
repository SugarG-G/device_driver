#include <linux/init.h>
#include <linux/module.h>


static int __init hello_init(void)
{
    pr_info("Hello, Raspberry Pi kernel module loaded\n");
    return 0;
}

static void __exit hello_exit(void)
{
    pr_info("Goodbye, Raspberry Pi kernel module unloaded\n");
}

module_init(hello_init);
module_exit(hello_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("chi");
MODULE_DESCRIPTION("Hello World test module");
MODULE_VERSION("0.1.0");
