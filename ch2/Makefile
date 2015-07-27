
KERNEL_ROOT := /mnt/fileroot/sky.zhou/905-l-amlogic/common/
KERNEL_OUT := /mnt/fileroot/sky.zhou/905-l-amlogic/out/target/product/p200/obj/KERNEL_OBJ
PREFIX_CROSS_COMPILE := aarch64-linux-gnu-

obj-m := helloworld.o

helloworld-objs := hello-world.o  

default:
	make -C $(KERNEL_ROOT) O=$(KERNEL_OUT) M=$(shell pwd) CROSS_COMPILE=$(PREFIX_CROSS_COMPILE) ARCH=arm64 modules
clean:
	rm -f ./*.o ./*.mod.c ./*.ko ./Module.symvers ./modules.order ./*~
