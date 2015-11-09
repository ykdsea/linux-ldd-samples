#define DEBUG
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/compat.h>

#include "scrull.h"


#define SCRULL_MAX_LEN (40960)
#define BUF_BLK_LEN (4096)
static unsigned int scrullmajor = 0;
static bool support_udev = true;
static unsigned int devnum = 4;
static char* devname = "scrull";
static int read_dev_minor = 3;

struct scrulldata {
	struct list_head entry;
	char* pbuf;
};

typedef struct scrulldata* scrulldatap;

static struct scrulldev {
	struct cdev dev;
	struct list_head data_list;
	uint32_t data_blks;
	uint32_t content_len;
	struct mutex devlock;
	char owner[32];

	struct class * scrcls;
	struct device * scrdvs[4];
} scrull_dev; 


static struct file_operations scrull_global_fop = {0};
static struct file_operations scrull_read_fop = {0};

/*
 scrull_global_op is for scrull0 ~ scrull2.
 the threee devices uses the same memory area, each block in this area is 4k len, and linked into one list.
*/
int scrull_global_open(struct inode *nodep, struct file *filp)
{
	int minor = MINOR(nodep->i_rdev);
	if (minor >= read_dev_minor)
	{
		filp->f_op = &scrull_read_fop;
		pr_debug("set file fop to read only\n");
	}
	return 0;
}

int scrull_global_flush(struct file *filp, fl_owner_t id)
{
	pr_debug("nothing to do with FLUSH command!\n");
	return 0;
}

int scrull_global_release(struct inode *nodep, struct file* filp)
{
	pr_debug("nothing to do with RELEASE command \n");
	return 0;
}

//blkindx start from 0
struct scrulldata* search_scrull_blk(int blkidx) {
	struct scrulldata * pdata;
	if (list_empty(&scrull_dev.data_list)) 
		return NULL;

	list_for_each_entry(pdata, &(scrull_dev.data_list), entry) {
		blkidx --;
		if (blkidx < 0) {
			pr_debug("search_scrull_blk (%d) successful (%p)!\n", blkidx + 1, pdata);
			return pdata;
		}
	}

	return NULL;
}

ssize_t scrull_global_read(struct file *filp, char __user *data, size_t len, loff_t *ppos)
{
	struct scrulldata * pdata = 0;
	loff_t blkused = 0, startblk = 0;
	ssize_t readlen = 0, retval = 0, totalread = 0;

	mutex_lock(&scrull_dev.devlock);
	if (*ppos > scrull_dev.content_len) {
		retval = 0;
		goto ERR_EXIT;
	}

	if (*ppos + len > scrull_dev.content_len)
		len = scrull_dev.content_len - *ppos;
	pr_debug("scrull_global_read to read (%zd) from (%lld)\n", len, *ppos);

	startblk = *ppos / BUF_BLK_LEN;
	blkused = *ppos % BUF_BLK_LEN;
	pdata = search_scrull_blk(startblk);
	while(len > 0) {
		if (blkused == BUF_BLK_LEN) {
			//to get new blk;
			pdata = list_next_entry(pdata, entry);
			if (pdata == NULL) {
				retval = 0;
				pr_debug("read to EOF\n");
				break;
			}
			blkused = 0;
		}

		readlen = BUF_BLK_LEN - blkused;
		if (readlen > len)
			readlen = len;
		if (copy_to_user(data + totalread, pdata->pbuf + blkused, readlen)) {
			pr_err("scrull_global_read copy_to_user fail(%p to %p)\n", data, pdata->pbuf);
			retval =  -EFAULT;
			goto ERR_EXIT;
		}

		len -= readlen;
		blkused += readlen;
		totalread += readlen;
	}

	*ppos += totalread;
	retval = totalread;
	pr_debug("scrull_global_read read (%zd) done\n", retval);

ERR_EXIT:
	mutex_unlock(&scrull_dev.devlock);
	return retval;
}

struct scrulldata* add_new_block(void)
{
	struct scrulldata* newdata = (struct scrulldata *)kmalloc(sizeof(struct scrulldata), 0);
	newdata->pbuf = (char*)kmalloc(BUF_BLK_LEN, 0);
	if (newdata->pbuf == NULL) {
		kfree(newdata);
		return 0;
	}
	memset(newdata->pbuf, 1, BUF_BLK_LEN);
	list_add_tail(&(newdata->entry), &(scrull_dev.data_list));
	scrull_dev.data_blks ++;

	pr_debug("add_new_block(%p), total blks(%d) \n", newdata, scrull_dev.data_blks);
	return newdata;
}

ssize_t scrull_no_write(struct file *filp, const char __user *data, size_t len, loff_t *ppos)
{
	pr_debug("scrull_no_write can't write to scrull device\n");
	return -EACCES;
}

ssize_t scrull_global_write(struct file *filp, const char __user *data, size_t len, loff_t *ppos)
{
	struct scrulldata *pdata = 0, * newdata = 0;
	ssize_t writelen = 0, totalwrite = 0, blkused = 0;
	loff_t woffset = 0, offsetblk = 0;
	ssize_t retval = 0;

	pr_debug("enter scrull_global_write\n");

	mutex_lock(&scrull_dev.devlock);

	woffset = *ppos;
	
	if ((woffset + len) >= SCRULL_MAX_LEN) {
		retval = -EOVERFLOW;
		goto ERR_EXIT;
	}

	/*seek to offset.*/
	offsetblk = woffset / BUF_BLK_LEN + 1;
	pr_debug("seek to block(%lld) \n", offsetblk - 1);
	if (scrull_dev.data_blks >= offsetblk) {
		pdata = search_scrull_blk(offsetblk - 1);
		offsetblk = 0;
		if (pdata == NULL) {
			panic("should not happen, get %lld blk fail\n", offsetblk);
		}
	} else {
		if (!list_empty(&(scrull_dev.data_list))) {
			pdata = list_last_entry(&(scrull_dev.data_list), struct scrulldata, entry);
			offsetblk -= scrull_dev.data_blks;
			pr_debug("Write search to (%d) block, need to create (%lld) data block\n", scrull_dev.data_blks - 1, offsetblk);
		} else {
			pdata = NULL;
			pr_debug("Scrull is empty, need to create (%lld) data block\n", offsetblk);
		}
	}

	//alloc new empty blocks
	while (offsetblk > 0) {
		newdata = add_new_block();
		if (newdata == NULL) {
			retval =  -ENOMEM;
			goto ERR_EXIT;
		}
		pdata = newdata;
		offsetblk--;
	}

	//now, we are at the block to write.
	blkused = woffset % BUF_BLK_LEN;

	//to write
	while (len) {
		if (blkused == BUF_BLK_LEN) {
			//to get next block
			//if next is null, alloc new block
			if (pdata->entry.next == &scrull_dev.data_list) {
				newdata = add_new_block();
				if (newdata == NULL) {
					retval =  -ENOMEM;
					goto ERR_EXIT;
				}
				pdata = newdata;
			} else {
				pdata = list_next_entry(pdata, entry);
			}
			blkused = 0;
		}

		writelen = BUF_BLK_LEN - blkused;
		if (writelen > len)
			writelen = len;
		if (copy_from_user((pdata->pbuf+blkused), data + totalwrite, writelen)) {
			pr_err("copy_from_user fail(%p to %p)\n", data,  pdata->pbuf);
			retval =  -EFAULT;
			goto ERR_EXIT;
		}
		len -= writelen;
		blkused += writelen;
		totalwrite += writelen;

		pr_debug("write (%zd) remaining(%zd) to blk(%p,%zd)\n", writelen, len, pdata, blkused);
	}

	//chk if need update the content_len
	if (scrull_dev.content_len < (woffset + totalwrite))
		scrull_dev.content_len = woffset + totalwrite;

	//update fs pos, should NOT update struct file.f_pos directlly!!
	*ppos += totalwrite;
	retval = totalwrite;

ERR_EXIT:
	mutex_unlock(&scrull_dev.devlock);
	return retval;
}

loff_t scrull_global_lseek(struct file *filp, loff_t offset, int whence)
{
	pr_debug("scrull_global_lseek");
	mutex_lock(&scrull_dev.devlock);
	switch (whence) {
		case SEEK_SET:
			if (offset < scrull_dev.content_len)
				filp->f_pos = offset;
			break;
		case SEEK_CUR:
			if (filp->f_pos + offset < scrull_dev.content_len)
				filp->f_pos += offset;
			break;
		case SEEK_END:
			filp->f_pos = scrull_dev.content_len;
			break;
		default:
			pr_debug("lseek policy (%d) doesn't supported\n", whence);
			break;
	};

	pr_debug("scrull_global_lseek seek to new position (%lld)\n", filp->f_pos);
	mutex_unlock(&scrull_dev.devlock);

	return 0;
}


static long scrull_global_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	long retval = 0;
	__u32* retlen = 0;
	char* ubuf = 0;
	pr_debug("scrull_global_ioctl enter\n");

	switch (cmd) {
		case SCRULL_DUMP_INFO:
			pr_info("scrull_global_ioctl to dump info!\n");
			pr_info("scrull now support: \n"\
				"1, insmod with the speci major: scrullmajor=xxx ;\n"\
				"2, can specific if use udev to create device file: support_udev=true;\n"\
				"3, ioctl; \n"\
				"4, use different fops for minor=3 device can only read;\n");
			pr_info("scrull dev major (%d), devnum(%d), support uevent(%d)\n"\
				" blks (%d), content len(%d), owner(%s)",\
				scrullmajor,
				devnum,
				support_udev,
				scrull_dev.data_blks,
				scrull_dev.content_len,
				scrull_dev.owner);
			break;

		case SCRULL_GET_LEN:
			retlen = (__u32*)args;
			if (copy_to_user(retlen, &(scrull_dev.content_len), sizeof(scrull_dev.content_len))) {
				pr_err("SCRULL_GET_LEN copy_to_user fail(%p)\n", (void*)args);
				retval =  -EFAULT;
				goto ERR_EXIT;
			}
			pr_debug("SCRULL_GET_LEN get (%d)\n", *retlen);
			break;

		case SCRULL_WRITE_OWNER:
			ubuf = (char*)args;
			if (copy_from_user(&(scrull_dev.owner), ubuf, 32)) {
				pr_err("SCRULL_WRITE_OWNER copy_from_user fail(%p)\n", (void*)args);
				retval =  -EFAULT;
				goto ERR_EXIT;
			}
			pr_debug("SCRULL_WRITE_OWNER set owner (%s)\n", scrull_dev.owner);
			break;

		default:
			pr_info("not supported cmd %d\n", cmd);
			break;
	};

ERR_EXIT:
	return retval;
}


static int scrull_init(void)
{
	int ret = 0;
	/*init file operations*/
	scrull_global_fop.owner = THIS_MODULE;
	scrull_global_fop.open = scrull_global_open;
	scrull_global_fop.flush = scrull_global_flush;
	scrull_global_fop.release = scrull_global_release;
	scrull_global_fop.read = scrull_global_read;
	scrull_global_fop.write = scrull_global_write;
	scrull_global_fop.llseek = scrull_global_lseek;
	scrull_global_fop.unlocked_ioctl = scrull_global_ioctl;
	scrull_global_fop.compat_ioctl = scrull_global_ioctl;

	scrull_read_fop.owner = THIS_MODULE;
	scrull_read_fop.open = scrull_global_open;
	scrull_read_fop.flush = scrull_global_flush;
	scrull_read_fop.release = scrull_global_release;
	scrull_read_fop.read = scrull_global_read;
	scrull_read_fop.write = scrull_no_write;
	scrull_read_fop.llseek = scrull_global_lseek;
	scrull_read_fop.unlocked_ioctl = scrull_global_ioctl;
	scrull_read_fop.compat_ioctl = scrull_global_ioctl;

	INIT_LIST_HEAD(&(scrull_dev.data_list));
	scrull_dev.data_blks = 0;
	scrull_dev.content_len = 0;
	mutex_init(&scrull_dev.devlock);

	//allocat device major dynamicly
	if (scrullmajor == 0)
	{
		dev_t devsn;
		alloc_chrdev_region(&devsn, 0, devnum, devname);
		scrullmajor = MAJOR(devsn);
	}
	else
	{
		//register device
		register_chrdev_region(MKDEV(scrullmajor, 0), devnum, devname);
	}

	//create&register char device	
	cdev_init(&(scrull_dev.dev), &scrull_global_fop);
	ret = cdev_add(&(scrull_dev.dev), MKDEV(scrullmajor, 0), devnum);

	if (support_udev) {
		scrull_dev.scrcls = class_create(THIS_MODULE, "scrullDevCls");

		scrull_dev.scrdvs[0] = device_create(scrull_dev.scrcls, NULL, MKDEV(scrullmajor, 0), NULL, "scrull0");
		scrull_dev.scrdvs[1] = device_create(scrull_dev.scrcls, NULL, MKDEV(scrullmajor, 1), NULL, "scrull1");
		scrull_dev.scrdvs[2] = device_create(scrull_dev.scrcls, NULL, MKDEV(scrullmajor, 2), NULL, "scrull2");
		scrull_dev.scrdvs[3] = device_create(scrull_dev.scrcls, NULL, MKDEV(scrullmajor, 3), NULL, "scrull3");
	}

	pr_debug("scrull major(%d) init, add dev return (%d)\n", scrullmajor, ret);
	if (!support_udev) 
		pr_debug("Scrull dev fill should create by yourself!!\n");
	else
		pr_debug("Scrull dev file will create by udev!\n");

	return 0;
}


static int scrull_uninit(void)
{
	//release resources
	struct list_head* datalist = &(scrull_dev.data_list);
	struct scrulldata *data ;
	while (!list_empty(datalist))
	{
		data = list_first_entry(datalist, struct scrulldata, entry);
		list_del_init(&(data->entry));
		if (data->pbuf)
			kfree(data->pbuf);
		kfree(data);
	}

	if (support_udev) {
		device_destroy(scrull_dev.scrcls, MKDEV(scrullmajor, 0));
		device_destroy(scrull_dev.scrcls, MKDEV(scrullmajor, 1));
		device_destroy(scrull_dev.scrcls, MKDEV(scrullmajor, 2));
		device_destroy(scrull_dev.scrcls, MKDEV(scrullmajor, 3));

		class_destroy(scrull_dev.scrcls);
	}

	//delete&unregister cdev
	cdev_del(&(scrull_dev.dev));

	//unregister device major
	unregister_chrdev_region(MKDEV(scrullmajor, 0), devnum);

	mutex_destroy(&scrull_dev.devlock);
	pr_debug("scrull uninit \n");
	return 0;
}


static int __init scrull_module_init(void)
{
	scrull_init();
	pr_debug("scrull module init\n");
	return 0;
}

static void __exit scrull_module_exit(void)
{
	scrull_uninit();
	pr_debug("scrull module exit\n");
}


module_param(scrullmajor, uint, S_IRUGO|S_IWUSR|S_IWGRP);
module_param(support_udev, bool, S_IRUGO|S_IWUSR|S_IWGRP);

module_init(scrull_module_init);
module_exit(scrull_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sky.zhou (ykdsea@gmail.com)");
MODULE_DESCRIPTION("scrull module");
