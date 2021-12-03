# --- Makefile for simple Adder example.

# ARM cross-compiler
ARCH	= arm64
PROC	= aarch64
TYPE	= linux-gnu
CC	= $(PROC)-$(TYPE)-gcc
CCFLAGS	= # -static
STRIP	= $(PROC)-$(TYPE)-strip

UNAME 	= $(shell uname -r)
#KDIR	= /home/projects/gerstl/ece382m/plnx_kernel
KDIR	= /usr/src/plnx_kernel
KFLAGS	= -DDEBUG # -DNO_DTS

obj-m	= fpga_drv.o


all: fpga_drv.ko example

fpga_drv.ko: fpga_drv.c
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(PROC)-$(TYPE)- EXTRA_CFLAGS="$(KFLAGS)" -C $(KDIR) M=$(PWD) modules

clean:
	$(RM) .skeleton* .*.cmd *.o *.ko *.mod.c
	$(RM) modules.order Module.symvers
	$(RM) -R .tmp*
	$(RM) example *~
