#include <linux/module.h>
#include <linux/stat.h>

static char *who = "sky";
static char *content = "hello world";
static char *exitcontent = "Good Bye!";


module_param(who, charp, 0);
module_param(content, charp, S_IRUGO|S_IWUSR|S_IWGRP);
module_param(exitcontent, charp, S_IRUGO);


static int __init hello_world_init(void)
{
    printk("%s Says \'%s\' \n", who, content);
    return 0;
}

static void __exit hello_world_exit(void)
{
    printk("%s\n", exitcontent);
}


static char* hello_world_get_owner(void)
{
    return who;
}

EXPORT_SYMBOL(hello_world_get_owner);

module_init(hello_world_init);
module_exit(hello_world_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("sky.zhou (ykdsea@gmail.com)");
MODULE_DESCRIPTION("first module sample");
