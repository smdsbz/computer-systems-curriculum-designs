obj-m	:= mycharflipdev.o

KVER	:=$(shell uname -r)
KDIR	:=/lib/modules/$(KVER)/build

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

test:
	echo -n a > /dev/mycharflip0
	cat /dev/mycharflip0
