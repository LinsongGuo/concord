LLVM_VERSION=9
# CC=clang-$(LLVM_VERSION)
CC=gcc
CFLAGS= -lpthread -O3

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
LOG_FILE:=${ROOT_DIR}/concord_timestamps.log

# Check if an argument is provided
ifeq ($(filter uintr concord clean,$(firstword $(MAKECMDGOALS))),)
    $(error Invalid argument. Please use 'make uint' or 'make concord')
endif

# Set the variable based on the argument
ifeq ($(firstword $(MAKECMDGOALS)),uintr)
    preempt_option := -DUINTR -muintr
endif
ifeq ($(firstword $(MAKECMDGOALS)),concord)
    preempt_option := -DCONCORD
endif

uintr: all
	@echo "Building with option UINTR"

concord: all
	@echo "Building with option concord"

all: clean concord.a concord_accuracy.a concord_pthread.a concord_base.a

concord.a: concord.o
	ar rcs concord.a concord.o

concord_accuracy.a: concord_accuracy.o
	ar rcs concord_accuracy.a concord_accuracy.o

concord_pthread.a: concord_pthread.o
	ar rcs concord_pthread.a concord_pthread.o

concord_base.a: concord_base.o
	ar rcs concord_base.a concord_base.o

concord.o: concord.c
	$(CC) $(CFLAGS) -DPATH=\"${LOG_FILE}\" -c concord.c $(preempt_option) -o concord.o 

concord_accuracy.o: concord.c
	$(CC) $(CFLAGS) -DPATH=\"${LOG_FILE}\" -c concord.c $(preempt_option) -DACCURACY_TEST -o concord_accuracy.o 

concord_pthread.o: concord.c
	$(CC) $(CFLAGS) -DPATH=\"${LOG_FILE}\" -c concord.c $(preempt_option) -DPTHREAD_SUPPORT -o concord_pthread.o 

concord_base.o: concord.c
	$(CC) $(CFLAGS) -DPATH=\"${LOG_FILE}\" -c concord.c -o concord_base.o 

clean:
	$(RM) *.o *.a