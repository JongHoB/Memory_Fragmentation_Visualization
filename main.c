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

// Get the page frame number of each pid from /proc/pid/maps
// return the list of physical addresses

typdef struct pfn
{
    unsigned long long start;
    unsigned long long end;
};

unsigned long long *get_paddr_list(int pid)
{

    unsigned long long *paddr_list = NULL;
    int paddr_list_size = 0;
    int paddr_list_capacity = 1024;

    paddr_list = (unsigned long long *)malloc(sizeof(unsigned long long) * paddr_list_capacity);

    char path[1024];
    sprintf(path, "/proc/%d/maps", pid);

    FILE *mapf = fopen(path, "r");
    if (mapf == NULL)
    {
        printf("Failed to open %s\n", path);
        exit(1);
    }

    char line[1024];
    while (fgets(line, sizeof(line), mapf) != NULL)
    {
        unsigned long long start, end;
        char r, w, x, p;
        int offset, dev_major, dev_minor, inode;
        char pathname[1024];

        int ret = sscanf(line, "%llx-%llx %c%c%c%c %x %x:%x %d %s\n",
                         &start, &end, &r, &w, &x, &p,
                         &offset, &dev_major, &dev_minor, &inode, pathname);

        // There also can be no pathname , which inode is 0
        if (ret == 11 || ret == 10)
        {
            if (paddr_list_size >= paddr_list_capacity)
            {
                paddr_list_capacity *= 2;
                paddr_list = (unsigned long long *)realloc(paddr_list, sizeof(unsigned long long) * paddr_list_capacity);
            }
            paddr_list[paddr_list_size++] = start;
        }
    }

    //---------------------------------------------
    fclose(mapf);

    return paddr_list;
}
