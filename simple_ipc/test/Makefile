BASE_SRC=/home/byteman/tmp/ipc/gm8126/arm-linux-2.6.28
LINUX_SRC=$(BASE_SRC)/linux-2.6.28-fa
LINUX_INC=$(LINUX_SRC)/include
CROSS_COMPILE=arm-none-linux-gnueabi-
CC=$(CROSS_COMPILE)gcc
CPP=$(CC) -E
LD=$(CROSS_COMPILE)ld
AS=$(CROSS_COMPILE)as
MAKE=make

SAMPLE2=record

OUTPUT_DIR=../release
LIB=../lib

HFLAGS = -O3 -Wall -I/$(LINUX_SRC)/../module/include/dvr
HFLAGS +=-I$(BASE_SRC)/module/include/vcap200_v3
HFLAGS +=-I$(BASE_SRC)/module/include/security
HFLAGS +=-I$(BASE_SRC)/module/include/ahb_dma
HFLAGS += -I../inc
HFLAGS +=-I$(BASE_SRC)/module/include/isp
TARGETS := $(LIB) $(SAMPLE2) 

.PHONY: $(TARGETS)

all: $(TARGETS)

$(LIB) :
	$(MAKE) -C $(LIB)
	

$(SAMPLE2):
	$(CC) $(HFLAGS) main-bitstream-record.c ../lib/librtsp.a ../lib/libipc.a -lpthread -lm -lrt -o $(SAMPLE2)
	/bin/cp $(SAMPLE2) $(OUTPUT_DIR)


clean:
	rm -f  *.o  $(SAMPLE2)
	
