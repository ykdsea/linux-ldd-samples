#include <linux/module.h>


static int hello_world_init(void) {
    printk("hello world init \n");
    return 0;
}

static void hello_world_exit(void) {
    printk("hello world exit \n");
}


module_init(hello_world_init);
module_exit(hello_world_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("sky.zhou (ykdsea@gmail.com)");
MODULE_DESCRIPTION("first module sample");
