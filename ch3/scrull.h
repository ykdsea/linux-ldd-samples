#ifndef _LINUX_STUDY_SCRULL_H
#define _LINUX_STUDY_SCRULL_H
#include <linux/ioctl.h>

//scrull ioctl command
#define SCRULL_DUMP_INFO _IO('z', 1)
#define SCRULL_GET_LEN _IOR('z', 2, __u32)
#define SCRULL_WRITE_OWNER _IOW('z', 3, char[32])

#endif//_LINUX_STUDY_SCRULL_H