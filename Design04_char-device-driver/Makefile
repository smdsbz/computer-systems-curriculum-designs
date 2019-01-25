obj-m += helloworldmod.o

PWD	:=$(shell pwd)
KVER	:=$(shell uname -r)
KDIR	:=/lib/modules/$(KVER)/build

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
