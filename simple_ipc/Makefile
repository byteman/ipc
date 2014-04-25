BASE_SRC=/home/byteman/tmp/ipc/gm8126/arm-linux-2.6.28
LINUX_SRC=$(BASE_SRC)/linux-2.6.28-fa
LINUX_INC=$(LINUX_SRC)/include
CROSS_COMPILE=/home/byteman/tmp/ipc/gm8126/arm-none-linux-gnueabi-4.4.0_ARMv5TE/bin/arm-none-linux-gnueabi-
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
CPP=$(CC) -E
LD=$(CROSS_COMPILE)ld
AS=$(CROSS_COMPILE)as
MAKE=make

TARGET=ipc

OUTPUT_DIR=../release

LIBS = -L../../rtmpdump/librtmp
LINKS = -lpthread -lm -lrt -ldl -lrtmp -lssl -lcrypto -lz 
HFLAGS = -O3 -Wall -I/$(LINUX_SRC)/../module/include/dvr
HFLAGS +=-I$(BASE_SRC)/module/include/vcap200_v3
HFLAGS +=-I$(BASE_SRC)/module/include/security
HFLAGS +=-I$(BASE_SRC)/module/include/ahb_dma
HFLAGS +=-I$(BASE_SRC)/module/include/isp
HFLAGS +=-I../rtmpdump/librtmp
TARGETS := $(TARGET) 

.PHONY: $(TARGETS)

all: $(TARGETS)

$(LIB) :
	$(MAKE) -C $(LIB)
	

$(TARGET):
	$(CXX) $(HFLAGS) $(LIBS) RTMPStream.cpp SpsDecode.cpp IPCMain.cpp  $(LINKS) -o $(TARGET)


clean:
	rm -f  *.o  $(TARGET)
	
