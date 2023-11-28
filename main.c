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

// Get the Virtual address from /proc/pid/maps
// return the list of virtual address

typedef struct _vaddr
{
    unsigned long long start;
    unsigned long long end;
} vaddr;

vaddr *get_vaddr_list(int pid)
{
    vaddr *vaddr_list = NULL;
    int vaddr_list_size = 0;
    int vaddr_list_capacity = 1024;

    vaddr_list = (vaddr *)malloc(sizeof(vaddr) * vaddr_list_capacity);

    char path[1024];
    sprintf(path, "/proc/%d/maps", pid);

    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        printf("Failed to open %s\n", path);
        exit(1);
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        unsigned long long start, end;
        sscanf(line, "%llx-%llx", &start, &end);

        if (vaddr_list_size >= vaddr_list_capacity)
        {
            vaddr_list_capacity *= 2;
            vaddr_list = (vaddr *)realloc(vaddr_list, sizeof(vaddr) * vaddr_list_capacity);
        }
        vaddr_list[vaddr_list_size].start = start;
        vaddr_list[vaddr_list_size++].end = end;
    }

    fclose(fp);

    vaddr_list = (vaddr *)realloc(vaddr_list, sizeof(vaddr) * vaddr_list_size);

    return vaddr_list;
}

// Get the Physical address from /proc/pid/pagemap
// return the list of physical address

typedef struct _paddr
{
    unsigned long long start;
    unsigned long long end;
} paddr;

paddr *get_paddr_list(int pid, vaddr *vaddr_list, int vaddr_list_size)
{
    paddr *paddr_list = NULL;
    int paddr_list_size = 0;
    int paddr_list_capacity = 1024;

    paddr_list = (paddr *)malloc(sizeof(paddr) * paddr_list_capacity);

    char path[1024];
    sprintf(path, "/proc/%d/pagemap", pid);

    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        printf("Failed to open %s\n", path);
        exit(1);
    }

    for (int i = 0; i < vaddr_list_size; i++)
    {
        unsigned long long start = vaddr_list[i].start;
        unsigned long long end = vaddr_list[i].end;

        unsigned long long start_page = start / PAGE_SIZE;
        unsigned long long end_page = end / PAGE_SIZE;

        for (unsigned long long page = start_page; page <= end_page; page++)
        {
            unsigned long long offset = page * sizeof(unsigned long long);
            unsigned long long data;

            if (lseek(fd, offset, SEEK_SET) != offset)
            {
                printf("Failed to seek %s\n", path);
                exit(1);
            }

            if (read(fd, &data, sizeof(unsigned long long)) != sizeof(unsigned long long))
            {
                printf("Failed to read %s\n", path);
                exit(1);
            }

            if (data & (1ULL << 63)) // Check the page is present
            {
                unsigned long long p_addr = data & (((unsigned long long)1 << 55) - 1);
                p_addr *= PAGE_SIZE;

                if (paddr_list_size >= paddr_list_capacity)
                {
                    paddr_list_capacity *= 2;
                    paddr_list = (paddr *)realloc(paddr_list, sizeof(paddr) * paddr_list_capacity);
                }
                paddr_list[paddr_list_size].start = p_addr;
                paddr_list[paddr_list_size++].end = p_addr + PAGE_SIZE;
            }
        }
    }

    return paddr_list;
}

// Main function
int main(int argc, char *argv[])
{
    // Get the memory size and number of pages
    get_memory_size();
    get_num_of_pages();

    // Get the current pid list
    int *pid_list = get_pid_list();
    int pid_list_size = sizeof(pid_list) / sizeof(int);

    // Get the virtual address list
    for (int i = 0; i < pid_list_size; i++)
    {
        int pid = pid_list[i];
        vaddr *vaddr_list = get_vaddr_list(pid);
        int vaddr_list_size = sizeof(vaddr_list) / sizeof(vaddr);

        // Get the physical address list
        paddr *paddr_list = get_paddr_list(pid, vaddr_list, vaddr_list_size);
        int paddr_list_size = sizeof(paddr_list) / sizeof(paddr);

        // Print the result
        printf("pid: %d\n", pid);
        for (int j = 0; j < paddr_list_size; j++)
        {
            printf("start: %llx, end: %llx\n", paddr_list[j].start, paddr_list[j].end);
        }
        printf("\n");
    }

    return 0;
}