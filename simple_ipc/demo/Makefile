LINUX_SRC=/usr/src/arm-linux-2.6.28/linux-2.6.28-fa
LINUX_INC=$(LINUX_SRC)/include

CROSS_COMPILE=arm-none-linux-gnueabi-
CC=$(CROSS_COMPILE)gcc
CPP=$(CC) -E
LD=$(CROSS_COMPILE)ld
AS=$(CROSS_COMPILE)as
MAKE=make
PROGS=simple_ipc
OUTPUT_DIR=../release

HFLAGS = -O3 -Wall -I/$(LINUX_SRC)/../module/include/dvr
HFLAGS +=-I/usr/src/arm-linux-2.6.28/module/include/vcap200_v3
HFLAGS +=-I/$(LINUX_SRC)/../module/include/isp
HFLAGS += -I../inc

TARGETS := $(PROGS)

.PHONY: $(TARGETS)

all: $(TARGETS)

$(PROGS):
	$(CC) $(HFLAGS) simple_ipc.c cap_osd-mask.c cap_osd.c rtspd.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(PROGS)
	cp $(PROGS) $(OUTPUT_DIR)

clean:
	rm -f  *.o $(PROGS)



