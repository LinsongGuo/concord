LLVM_VERSION=9
CC=clang-$(LLVM_VERSION)
CFLAGS= -lpthread -O3

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
LOG_FILE:=${ROOT_DIR}/concord_timestamps.log

all: clean concord.a concord_accuracy.a concord_pthread.a

concord.a: concord.o
	ar rcs concord.a concord.o

concord_accuracy.a: concord_accuracy.o
	ar rcs concord_accuracy.a concord_accuracy.o

concord_pthread.a: concord_pthread.o
	ar rcs concord_pthread.a concord_pthread.o

concord.o: concord.c
	$(CC) $(CFLAGS) -DPATH=\"${LOG_FILE}\" -c concord.c -o concord.o 

concord_accuracy.o: concord.c
	$(CC) $(CFLAGS) -DPATH=\"${LOG_FILE}\" -c concord.c -DACCURACY_TEST -o concord_accuracy.o 

concord_pthread.o: concord_pthread.c
	$(CC) $(CFLAGS) -DPATH=\"${LOG_FILE}\" -c concord_pthread.c -DACCURACY_TEST -o concord_pthread.o 

clean:
	$(RM) *.o *.a