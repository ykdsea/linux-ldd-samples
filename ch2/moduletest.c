#include <linux/module.h>
#include <linux/stat.h>

extern char* hello_world_get_owner(void);

static int __init moduld_test_init(void)
{
    printk("I get the owner of helloworld is %s\n",hello_world_get_owner());
    return 0;
}

static void __exit moduld_test_exit(void)
{
    printk("Finish test module\n");
}


module_init(moduld_test_init);
module_exit(moduld_test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sky.zhou (ykdsea@gmail.com)");
MODULE_DESCRIPTION("I will check the module!!!");
