NAME           	:= kernel
SRCS           	:= framework/main.c $(shell find -L ./src/ -name "*.c")
INC_PATH       	:= include/ framework/

export AM_HOME := $(PWD)/../abstract-machine
ifeq ($(ARCH),)
export ARCH    	:= x86_64-qemu
endif

ifeq ($(ARCH), x86_64-qemu)
export QEMU		:= qemu-system-x86_64
else ifeq ($(ARCH), x86-qemu)
export QEMU 	:= qemu-system-i386
endif


ifeq ($(smp),)
export smp		:= 4
endif


COMMA	:=,
ifneq ($(test),)
export CFLAGS += $(patsubst %,-DTEST%, $(subst $(COMMA), ,$(test)))
endif

ifneq ($(debug),)
export CFLAGS += $(patsubst %,-DDEBUG%, $(subst $(COMMA), ,$(debug)))
endif

ifeq ($(rand),)
export CFLAGS += -DRANDOM=$(shell date +%N | head -c 6)
endif

include $(AM_HOME)/Makefile
include ../Makefile.lab
image: git

gdb: image
	$(QEMU) -gdb tcp::1737 -S -serial stdio -smp $(smp) -drive format=raw,file=./build/kernel-$(ARCH) -no-reboot -no-shutdown