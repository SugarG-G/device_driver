KDIR := /home/ubuntu/pi_kernel/linux
ARCH := arm
CROSS_COMPILE := arm-linux-gnueabihf-

obj-m := hello.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
