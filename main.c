// I will make the visualizer of kernel physical memory
// It will show whole address space of the physical memory
// and represent that each page is used or not.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <dirent.h>

unsigned long long MEM_SIZE;
unsigned long long PAGE_SIZE;
unsigned long long PHYS_PAGES;
#define PAGE_MASK 0xfffff000

// We need to get the physical memory size

#define MEMINFO_PATH "/proc/meminfo"

void get_memory_size(void)
{
    struct sysinfo info;

    if (sysinfo(&info) == 0)
    {
        MEM_SIZE = info.totalram;
    }
    else
    {
        printf("Failed to get the memory size\n");
        exit(1);
    }
    return;
}

// We need to get the number of pages
void get_num_of_pages(void)
{

#if defined(_SC_PAGESIZE)
    {
        const long long size = sysconf(_SC_PAGESIZE);
        if (size > 0)
        {
            PAGE_SIZE = size;
        }
    }
#else
    {
        PAGE_SIZE = 4096; // DEFAULT PAGE SIZE
    }
#endif

#if defined(_SC_PHYS_PAGES)
    {
        const long long size = sysconf(_SC_PHYS_PAGES);
        if (size > 0)
        {
            PHYS_PAGES = size;
        }
    }
#else
    {
        PHYS_PAGES = MEM_SIZE / PAGE_SIZE;
    }
#endif

    return;
}

// Get pid list from /proc
// return the list of pid

int *get_pid_list(void)
{
    int *pid_list = NULL;
    int pid_list_size = 0;
    int pid_list_capacity = 1024;

    pid_list = (int *)malloc(sizeof(int) * pid_list_capacity);

    DIR *dir = NULL;
    struct dirent *entry = NULL;

    dir = opendir("/proc");
    if (dir == NULL)
    {
        printf("Failed to open /proc\n");
        exit(1);
    }

    while ((entry = readdir(dir)) != NULL)
    {
        int pid = atoi(entry->d_name);
        if (pid != 0)
        {
            if (pid_list_size >= pid_list_capacity)
            {
                pid_list_capacity *= 2;
                pid_list = (int *)realloc(pid_list, sizeof(int) * pid_list_capacity);
            }
            pid_list[pid_list_size++] = pid;
        }
    }

    closedir(dir);

    pid_list = (int *)realloc(pid_list, sizeof(int) * pid_list_size);

    return pid_list;
}
