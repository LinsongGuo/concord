#define _GNU_SOURCE
#include "concord.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <x86intrin.h>

#define DISPATCHER_CORE 2
#define PAGE_SIZE 4096

#define FUNC_ACTION CONCORD_ACT_NONE
#define PIN_DISPATCHER 1

#ifndef ACCURACY_TEST
__thread uint64_t concord_preempt_after_cycle = 8600000000000000;
#else
__thread uint64_t concord_preempt_after_cycle = 16000;
#endif

pthread_t dispatcher_thread;
uint8_t finish_dispatcher = 0;

__thread int concord_preempt_now = 0;
__thread uint64_t concord_start_time;

#define MAX_THREAD_NUM 16
#define mb() __asm volatile("mfence" ::: "memory")
unsigned preempt_thread_num = 0;
__thread int preempt_thread_id;
int preempt_protection;
volatile int *cpu_preempt_point[MAX_THREAD_NUM];
volatile int preempt_point_dead[MAX_THREAD_NUM];
unsigned long long preempt_sent[MAX_THREAD_NUM], preempt_recv[MAX_THREAD_NUM];

int concord_timer_reset = 0;
int concord_lock_counter = 0;

void *dispatcher();
void initial_setup();

uint64_t concord_timestamps[1000000];
uint64_t concord_timestamps_counter = 0;
uint64_t concord_timestamp_break_flag = 0;

unsigned long long *mmap_file;

void measurement_init() {
    FILE *fp = fopen(PATH, "w+");
    if (fp == 0) assert(0 && "fopen failed");
    fseek(fp, (1000000 * sizeof(long)) - 1, SEEK_SET);
    fwrite("", 1, sizeof(char), fp);
    fflush(fp);
    fclose(fp);

    int fd = open(PATH, O_RDWR);
    if (fd < 0) assert(0 && "open failed");
    mmap_file = (long long *)mmap(NULL, 1000000 * sizeof(long long), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mmap_file == MAP_FAILED) assert(0 && "mmap failed");
    close(fd);
    long gap = PAGE_SIZE / sizeof(long long);
    // Touch each page to load them to the TLB.
    for (long i = 0; i < 1000000; i += gap) mmap_file[i] = 0;
}

int first_time_init = 1;
int concord_enable_log = 1;

void concord_rdtsc_func() {
    if (unlikely(!concord_enable_log)) {
        return;
    }

    if (unlikely(first_time_init)) {
        measurement_init();
        first_time_init = 0;
    }

    concord_start_time = __rdtsc();
    mmap_file[concord_timestamps_counter++] = concord_start_time;
}

void concord_func() {
#if FUNC_ACTION == CONCORD_ACT_LOG
    if (unlikely(!concord_enable_log)) {
        return;
    }

    if (unlikely(first_time_init)) {
        measurement_init();
        first_time_init = 0;
    }

    concord_start_time = __rdtsc();
    mmap_file[concord_timestamps_counter++] = concord_start_time;
#endif

    // printf("concord_func\n");
    concord_preempt_now = 0;
    preempt_recv[preempt_thread_id]++;

    return;
}

void concord_register_dispatcher() {
    printf("Registering concord dispatcher\n");
    initial_setup();

#if PIN_DISPATCHER == 1
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(DISPATCHER_CORE, &cpuset);
    pthread_create(&dispatcher_thread, NULL, dispatcher, NULL);
    pthread_setaffinity_np(dispatcher_thread, sizeof(cpu_set_t), &cpuset);
#endif
}

void concord_unregister_dispatcher() {
    finish_dispatcher = 1;
    // pthread_join(dispatcher_thread, NULL);
    printf("Dispatcher unregistered\n");
}

void concord_set_preempt_flag(int flag) { concord_preempt_now = flag; }

void concord_disable() {
    // printf("Disabling concord\n");
    concord_lock_counter -= 1;
}

void concord_enable() {
    // printf("Enabling concord\n");
    concord_lock_counter += 1;
}

static inline long long get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

void __attribute__((optimize("O0"))) initial_setup() {
    for (size_t i = 0; i < 300; i++) {
        asm volatile("nop");
        get_time();
        int k = __rdtsc();
    }
}

extern void nop100();

#define GHz 2
#define quantum 5000
// #define quantum 10000

void *dispatcher() {
    // printf("-----------------------------dispatcher starts\n");
    uint64_t last_time = __rdtsc();

    while (1) {
        while (__rdtsc() - last_time < quantum*GHz) {
            asm volatile("nop");
            asm volatile("nop");
            asm volatile("nop");
        };
        
        last_time = __rdtsc();

        int i;
        for (i = 0; i < preempt_thread_num; ++i) {
            if (preempt_point_dead[i])
                continue;
            // if (*(cpu_preempt_point[i]) == 1)
            //     continue;
            preempt_sent[i]++;
            *(cpu_preempt_point[i]) = 1;
            nop100();
        }
        
        if (unlikely(finish_dispatcher == 1)) {
            break;
        }
    }

    printf("Dispatcher finished\n");

    return NULL;
}

void concord_set_preempt_point() {
    // printf("concord_set_preempt_point: %d\n", preempt_thread_num);
    preempt_thread_id = preempt_thread_num;
    cpu_preempt_point[preempt_thread_id] = &concord_preempt_now;
    preempt_point_dead[preempt_thread_id] = 0;
    preempt_recv[preempt_thread_id] = 0;
    preempt_sent[preempt_thread_id] = 0;
    mb();
    preempt_thread_num++;
}

void concord_remove_preempt_point() {
    // printf("concord_remove_preempt_point: %d\n", preempt_thread_id);
    cpu_preempt_point[preempt_thread_id] = &preempt_protection;
    mb();
    preempt_point_dead[preempt_thread_id] = 1;
}

struct routine_arg_t {
    void *(*routine)(void *);
    void *arg;
}; 

void *new_routine(void *ra) {
    void *(*routine)(void *) = ((struct routine_arg_t *)ra)->routine;
    void *arg = ((struct routine_arg_t *)ra)->arg;

    concord_set_preempt_point();
    
    void* res = routine(arg);

    concord_remove_preempt_point();

    free((struct routine_arg_t*)ra);

    return res;
}

int __pthread_init = 0;
extern int __real_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*routine)(void *), void *arg);

int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*routine)(void *), void *arg) {
    if (!__pthread_init)
        return __real_pthread_create(thread, attr, routine, arg);

    // printf("***__wrap_pthread_create***\n");

    struct routine_arg_t *ra = malloc(sizeof(struct routine_arg_t));
    ra->routine = routine;
    ra->arg = arg;

    return __real_pthread_create(thread, attr, new_routine, ra);
}

void before_main(void) __attribute__((constructor));

void before_main(void)
{
    concord_register_dispatcher();

    __pthread_init = 1;
    concord_set_preempt_point();

    // cpu_set_t mask;
	// CPU_ZERO(&mask);
	// CPU_SET(4, &mask);
	// sched_setaffinity(0, sizeof(mask), &mask);
}

void after_main(void) __attribute((destructor));

void after_main(void)
{
    concord_unregister_dispatcher();

    int i;
    for (i = 0; i < preempt_thread_num; ++i) {
        printf("Thread %d: %d sent, %d received\n", i, preempt_sent[i], preempt_recv[i]);
    }
}

void nop100() {
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
}