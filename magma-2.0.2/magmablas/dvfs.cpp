 #include <cstdlib>
#include <cmath>
#include "stdio.h"
#include "nvml.h"
#include <sys/time.h>
#include <signal.h>
#include <sched.h>
#include <unistd.h>
#include "dvfs.h"

static struct itimerval itv;
double interrupt;

// NVIDIA NVML library function wrapper for GPU DVFS.
int SetGPUFreq(unsigned int clock_mem, unsigned int clock_core) {
     nvmlDevice_t device;//int device;
     nvmlReturn_t result;
     result = nvmlInit();
     result = nvmlDeviceGetHandleByIndex(0, &device);//cudaGetDevice(&device);
    result = nvmlDeviceSetApplicationsClocks(device, clock_mem, clock_core);//(nvmlDevice_t)device
    if(result != NVML_SUCCESS)
    {
        printf("Failed to set GPU core and memory frequencies: %s\n", nvmlErrorString(result));
        return 1;
    }
    else
    {
        nvmlDeviceGetApplicationsClock(device, NVML_CLOCK_GRAPHICS, &clock_core);
        nvmlDeviceGetApplicationsClock(device, NVML_CLOCK_MEM, &clock_mem);
        //printf("GPU core frequency is now set to %d MHz; GPU memory frequency is now set to %d MHz\n", clock_core, clock_mem);
        return 0;

    }
}


void restore_gpu_handler (int signal) {
    SetGPUFreq(2600, 705);//SetGPUFreq(2600, 758);//758 is not stable, it changes to 705 if temp. is high.
}

void dvfs_gpu_handler (int signal) {
    initialize_handler(1);
    set_alarm(interrupt);
    SetGPUFreq(324, 324);//SetGPUFreq(2600, 758);//758 is not stable, it changes to 705 if temp. is high.
    
}

void restore_cpu_handler(int signal) {
    system("echo 2500000 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed");
}

void dvfs_cpu_handler(int signal) {
    initialize_handler(3);
    set_alarm(interrupt);
    system("echo 1200000 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed");
}

void dvfs_adjust(double s, char type) {
    interrupt = s;
    if (type == 'g') //GPU DVFS
        initialize_handler(0);
    else //CPU DVFS
        initialize_handler(2);

    set_alarm(0.001);
}

void r2h_adjust(double s1, double s2, char type) {
    interrupt = s2;
    if (type == 'g') //GPU DVFS
        initialize_handler(0);
    else //CPU DVFS
        initialize_handler(2);

    set_alarm(s1);
}

void set_alarm(double s) {
    s = s / 1000;
    itv.it_value.tv_sec = (suseconds_t)s;
    itv.it_value.tv_usec = (suseconds_t) ((s-floor(s))*1000000.0);

    // printf("sec:%d\n", itv.it_value.tv_sec);
    // printf("usec:%d\n", itv.it_value.tv_usec);

    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = 0;
    int res = setitimer(ITIMER_REAL, &itv, NULL);
    if (res != 0) {
        printf("setitimer error! \n");
    }
}

void initialize_handler(int type) {
    sigset_t sig;
    struct sigaction act;
    int res = sigemptyset(&sig);
    if (res != 0) {
        printf("sigemptyset error! \n");
    }
    if (type == 0)
        act.sa_handler = dvfs_gpu_handler;
    else if (type == 1)
        act.sa_handler = restore_gpu_handler;
    else if (type == 2) 
        act.sa_handler = dvfs_cpu_handler;
    else if (type == 3) 
        act.sa_handler = restore_cpu_handler;

    act.sa_flags = SA_RESTART;
    act.sa_mask = sig;
    res = sigaction(SIGALRM, &act, NULL);
    if (res != 0) {
        printf("sigaction error! \n");
    }
}


/*
 *  Forces computation to be done on a given CPU.
 *  @param: cpu - core for work to be done on
 *  @return: 0 if successful, -1 if not
 */
int map_cpu(int cpu) {
    int ret, nprocs;
    cpu_set_t cpu_mask;

    nprocs = sysconf(_SC_NPROCESSORS_CONF);//return 32, should be 16, does not matter.
    CPU_ZERO(&cpu_mask);
    CPU_SET(cpu%nprocs, &cpu_mask);

    ret = sched_setaffinity(0, sizeof(cpu_mask), &cpu_mask);
    ////int affinity = sched_getcpu();printf("Running on CPU %d\n", affinity);
    if(ret == -1) {
        perror("sched_setaffinity");
        return -1;
    }
    return 0;
}