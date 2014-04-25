LINUX_SRC=/usr/src/arm-linux-2.6.28/linux-2.6.28-fa
LINUX_INC=$(LINUX_SRC)/include

CROSS_COMPILE=arm-none-linux-gnueabi-
CC=$(CROSS_COMPILE)gcc
CPP=$(CC) -E
LD=$(CROSS_COMPILE)ld
AS=$(CROSS_COMPILE)as
MAKE=make
DEMO=demo
PROG1=samples
LIB=lib

HFLAGS = -O3 -Wall -I/$(LINUX_SRC)/../module/include/dvr
HFLAGS +=-I/usr/src/arm-linux-2.6.28/module/include/vcap200_v3
HFLAGS += -Iinc

TARGETS := $(LIB) $(DEMO) $(PROG1)

.PHONY: $(TARGETS)

all: $(TARGETS)

$(LIB) : 
	$(MAKE) -C $(LIB)

$(DEMO):
	$(MAKE) -C $(DEMO) 
	#$(CC) $(HFLAGS) simple_ipc.c cap_osd-mask.c cap_osd.c rtspd.c librtsp.a libipc.a -lpthread -lm -lrt -o $(PROGS)

$(PROG1):
	$(MAKE) -C $(PROG1) 

clean:
	$(MAKE) -C $(LIB) $@
	$(MAKE) -C $(DEMO) $@
	$(MAKE) -C $(PROG1) $@
	find ./release -maxdepth 1 -type f | xargs rm -f

version_release: 
	echo "make a release"
	$(MAKE) -C $(LIB) clean 
	$(MAKE) -C $(DEMO) clean
	$(MAKE) -C $(PROG1) clean
	find ./release -maxdepth 1 -type f | xargs rm -f
	find -name 'CVS' | xargs rm -Rf
	

