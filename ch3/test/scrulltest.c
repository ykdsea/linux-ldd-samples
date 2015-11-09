#include "scrull.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
 #include <sys/ioctl.h>

#define SCRULL_0 "/dev/scrull0"
#define SCRULL_1 "/dev/scrull1"
#define SCRULL_2 "/dev/scrull2"
#define SCRULL_3 "/dev/scrull3"


//only test ioctl
int main(void)
{
	int fd0;
	int32_t ret;
	char ownerstr[32] = "sky-write-test-module";

	fd0 = open(SCRULL_0, O_RDWR);
	ioctl(fd0, SCRULL_GET_LEN, &ret);
	printf("SCRULL_GET_LEN %d\n",ret);
	ioctl(fd0, SCRULL_WRITE_OWNER, &ownerstr);
	ioctl(fd0, SCRULL_DUMP_INFO);

	return 0;
}
