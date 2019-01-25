#include <linux/kernel.h>
#include <linux/module.h>


static int helloworldmod_init(void) {
    printk(KERN_DEBUG "%s: initializing\n", __func__);
    return 0;   // 0 means healthy
}


static void helloworldmod_cleanup(void) {
    printk(KERN_DEBUG "%s: cleaning up\n", __func__);
}


module_init(helloworldmod_init);
module_exit(helloworldmod_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiaoguang Zhu");
MODULE_DESCRIPTION("A simple hello world module");
