
KERNEL_DIR = /opt/FriendlyARM/mini210/linux/linux-3.0.8

all:
	make -C $(KERNEL_DIR) M=`pwd` modules

clean:
	make -C $(KERNEL_DIR) M=`pwd` modules clean

obj-m := buttons_drv_tiny210.o

