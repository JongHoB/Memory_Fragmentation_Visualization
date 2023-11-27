// I will make the visualizer of kernel physical memory
// It will show whole address space of the physical memory
// and represent that each page is used or not.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

unsigned long long MEM_SIZE;
unsigned long long PAGE_SIZE;
unsigned long long PHYS_PAGES;
#define PAGE_MASK 0xfffff000

// We need to get the physical memory size
// So we need to read the file /proc/meminfo
// and get the size of physical memory
// The file /proc/meminfo has the information of memory
// and the first line is the total memory size
// So we need to read the first line and get the size of memory
// The size of memory is in the unit of KB
// So we need to convert the size of memory to the unit of byte
// and return the size of memory

#define MEMINFO_PATH "/proc/meminfo"

void get_memory_size(void)
{
    FILE *fp;
    char buf[256];
    char *ptr;
    unsigned long long mem_size;

    fp = fopen(MEMINFO_PATH, "r");
    if (fp == NULL)
    {
        printf("Failed to open the file %s\n", MEMINFO_PATH);
        exit(1);
    }

    // read the first line
    fgets(buf, sizeof(buf), fp);

    // get the size of memory
    ptr = strtok(buf, " ");
    ptr = strtok(NULL, " ");
    mem_size = atoi(ptr);

    // convert the unit of memory size from KB to byte
    mem_size = mem_size * 1024;

    fclose(fp);

    MEM_SIZE = mem_size;

    return;
}

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

// We need to get the status of each page
// So we need to read the file /proc/kpageflags
// and get the status of each page
// The file /proc/kpageflags has the information of each page
// and the first line is the status of each page
// So we need to read the first line and get the status of each page
// The status of each page is in the unit of hex
// So we need to convert the status of each page to the unit of binary
// and return the status of each page

#define KPAGEFLAGS_PATH "/proc/kpageflags"

unsigned long long get_page_status(unsigned long long num_of_pages)
{
    FILE *fp;
    char buf[256];
    char *ptr;
    unsigned long long page_status;

    fp = fopen(KPAGEFLAGS_PATH, "r");
    if (fp == NULL)
    {
        printf("Failed to open the file %s\n", KPAGEFLAGS_PATH);
        exit(1);
    }

    // read the first line
    fgets(buf, sizeof(buf), fp);

    // get the status of each page
    ptr = strtok(buf, " ");
    ptr = strtok(NULL, " ");
    page_status = strtoull(ptr, NULL, 16);

    fclose(fp);

    return page_status;
}
