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

#ifndef __NR_uintr_register_handler
#define __NR_uintr_register_handler	471
#define __NR_uintr_unregister_handler	472
#define __NR_uintr_create_fd		473
#define __NR_uintr_register_sender	474
#define __NR_uintr_unregister_sender	475
#define __NR_uintr_wait			476
#endif

#define uintr_register_handler(handler, flags)	syscall(__NR_uintr_register_handler, handler, flags)
#define uintr_unregister_handler(flags)		syscall(__NR_uintr_unregister_handler, flags)
#define uintr_create_fd(vector, flags)		syscall(__NR_uintr_create_fd, vector, flags)
#define uintr_register_sender(fd, flags)	syscall(__NR_uintr_register_sender, fd, flags)
#define uintr_unregister_sender(ipi_idx, flags)	syscall(__NR_uintr_unregister_sender, ipi_idx, flags)
#define uintr_wait(flags)			syscall(__NR_uintr_wait, flags)

#define DISPATCHER_CORE 2
#define PAGE_SIZE 4096
#define FUNC_ACTION CONCORD_ACT_NONE
#define PIN_DISPATCHER 1

pthread_t dispatcher_thread;
uint8_t finish_dispatcher = 0;

// Data structures per thread.
#define MAX_THREAD_NUM 16
#define mb() __asm volatile("mfence" ::: "memory")
unsigned preempt_thread_num = 0;
__thread int preempt_thread_id;
int __pthread_init = 0;
unsigned long long preempt_sent_perthread[MAX_THREAD_NUM];
unsigned long long preempt_recv_perthread[MAX_THREAD_NUM];
volatile int preempt_state_perthread[MAX_THREAD_NUM];
enum State {
    UNREADY,
    READY,
    DEAD
};

// Data structures for Concord
int preempt_protection;
volatile int *cpu_preempt_point[MAX_THREAD_NUM];
__thread int concord_preempt_now = 0;
__thread uint64_t concord_start_time;

// Data structures for UINTR
int uintr_fd[MAX_THREAD_NUM];
int uipi_index[MAX_THREAD_NUM];

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
    preempt_recv_perthread[preempt_thread_id]++;

    return;
}

void __attribute__ ((interrupt))
     __attribute__((target("general-regs-only","inline-all-stringops")))
     ui_handler(struct __uintr_frame *ui_frame,
		unsigned long long vector) {
    ++preempt_recv_perthread[vector];
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
#define quantum 50000000
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
            if (preempt_state_perthread[i] == DEAD)
                continue;

#ifdef UINTR
            if (preempt_state_perthread[i] == UNREADY) {
                uipi_index[i] = uintr_register_sender(uintr_fd[i], 0);
                preempt_state_perthread[i] = READY;
                // printf("uipi_index %d : %d\n", i, uipi_index[i]);
            }
#endif

            // if (*(cpu_preempt_point[i]) == 1)
            //     continue;

            preempt_sent_perthread[i]++;

#ifdef CONCORD
            *(cpu_preempt_point[i]) = 1;
#elif UINTR
            _senduipi(uipi_index[i]);
            // asm volatile("senduipi %0" : : "rm" (uipi_index[i]));
#endif

            nop100();
        }
        
        if (unlikely(finish_dispatcher == 1)) {
            break;
        }
    }

    printf("Dispatcher finished\n");

    return NULL;
}

#ifdef CONCORD
void preempt_init_perthread() {
    // printf("concord_set_preempt_point: %d\n", preempt_thread_num);
    preempt_thread_id = preempt_thread_num;
    preempt_state_perthread[preempt_thread_id] = READY;
    preempt_recv_perthread[preempt_thread_id] = 0;
    
    cpu_preempt_point[preempt_thread_id] = &concord_preempt_now;
    
    mb();
    preempt_thread_num++;
}

void preempt_destory_perthread() {
    // printf("concord_remove_preempt_point: %d\n", preempt_thread_id);
    cpu_preempt_point[preempt_thread_id] = &preempt_protection;
    mb();
    preempt_state_perthread[preempt_thread_id] = DEAD;
}

#elif defined(UINTR)
void preempt_init_perthread() {
    // printf("uintr preempt_init_perthread: %d\n", preempt_thread_num);

    preempt_thread_id = preempt_thread_num;
    preempt_state_perthread[preempt_thread_id] = UNREADY;
    preempt_recv_perthread[preempt_thread_id] = 0;
    
    if (uintr_register_handler(ui_handler, 0))
		exit(-1);

	uintr_fd[preempt_thread_id] = uintr_create_fd(preempt_thread_id, 0);
	// printf("uintr_fd %d : %d\n", preempt_thread_id, uintr_fd[preempt_thread_id]);
    if (uintr_fd[preempt_thread_id] < 0)
		exit(-1);
    

    mb();
    preempt_thread_num++;

    _stui();
    // asm volatile("stui"); 
}

void preempt_destory_perthread() {
    // printf("uintr preempt_destory_perthread: %d\n", preempt_thread_id);
    mb();
    preempt_state_perthread[preempt_thread_id] = DEAD;
}
#endif

#ifdef PTHREAD_SUPPORT
struct routine_arg_t {
    void *(*routine)(void *);
    void *arg;
}; 
void *new_routine(void *ra) {
    void *(*routine)(void *) = ((struct routine_arg_t *)ra)->routine;
    void *arg = ((struct routine_arg_t *)ra)->arg;

    preempt_init_perthread();
    
    void* res = routine(arg);

    preempt_destory_perthread();

    free((struct routine_arg_t*)ra);

    return res;
}

extern int __real_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*routine)(void *), void *arg);
int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*routine)(void *), void *arg) {
    // printf("__wrap_pthread_create\n");
    return __real_pthread_create(thread, attr, routine, arg);

    // if (!__pthread_init)
    //     return __real_pthread_create(thread, attr, routine, arg);

    // // printf("***__wrap_pthread_create***\n");

    // struct routine_arg_t *ra = malloc(sizeof(struct routine_arg_t));
    // ra->routine = routine;
    // ra->arg = arg;

    // return __real_pthread_create(thread, attr, new_routine, ra);
}
#endif

void preempt_init_dispatcher() {
    __pthread_init = 1;
    int i;
    for (i = 0; i < MAX_THREAD_NUM; ++i) {
        preempt_state_perthread[i] = UNREADY;
        preempt_sent_perthread[i] = 0;
    }
}

void before_main(void) __attribute__((constructor));

void before_main(void)
{
    concord_register_dispatcher();

    preempt_init_dispatcher();

    // preempt_init_perthread();

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
        printf("Thread %d: %llu sent, %llu received\n", i, preempt_sent_perthread[i], preempt_recv_perthread[i]);
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