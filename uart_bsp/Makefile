obj-m += uart3_serdev_echo.o

# Adjust KDIR to your kernel tree if needed
KDIR ?= /home/ubuntu/pi_kernel/linux

ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
