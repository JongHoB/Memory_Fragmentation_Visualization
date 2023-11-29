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
#include "main.h"

unsigned long long MEM_SIZE;
unsigned long long PAGE_SIZE;
unsigned long long PHYS_PAGES;
// page mask for 64bit architecture

unsigned long long temp = 0;

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

pid_list *get_pid_list(void)
{
    pid_list *p_list = (pid_list *)malloc(sizeof(p_list));
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
        int pid = atoi(entry->d_name); // Get the pid from /proc
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

    p_list->size = pid_list_size;
    p_list->pid_list = pid_list;

    return p_list;
}

// Get the Virtual address from /proc/pid/maps
// return the list of virtual address

v_info *get_vaddr_list(int pid)
{
    v_info *vainfo = (v_info *)malloc(sizeof(v_info)); // v_info structure has vaddr_list and vaddr_list_size
    vaddr *vaddr_list = NULL;
    unsigned long long vaddr_list_size = 0;
    unsigned long long vaddr_list_capacity = 1024;

    vaddr_list = (vaddr *)malloc(sizeof(vaddr) * vaddr_list_capacity);

    char path[1024];
    sprintf(path, "/proc/%d/maps", pid);

    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        printf("Failed to open %s\n", path);
        vaddr_list_size = 0;
        goto OUT;
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        unsigned long long start, end;
        sscanf(line, "%llx-%llx", &start, &end); // Get the virtual address from /proc/pid/maps

        if (vaddr_list_size >= vaddr_list_capacity)
        {
            vaddr_list_capacity *= 2;
            vaddr_list = (vaddr *)realloc(vaddr_list, sizeof(vaddr) * vaddr_list_capacity);
        }
        vaddr_list[vaddr_list_size].start = start;
        vaddr_list[vaddr_list_size++].end = end;
    }

    fclose(fp);

OUT:
    if (vaddr_list_size == 0)
    {
        free(vaddr_list);
        vainfo->vaddr_list_size = 0;
    }
    else
    {
        vaddr_list = (vaddr *)realloc(vaddr_list, sizeof(vaddr) * vaddr_list_size);

        vainfo->vaddr_list = vaddr_list;
        vainfo->vaddr_list_size = vaddr_list_size;
    }

    return vainfo;
}

// Get the physical page frame numbers from /proc/pid/pagemap
// return the list of physical frame number

p_info *get_pfn_list(int pid, vaddr *vaddr_list, int vaddr_list_size)
{
    p_info *pinfo = (p_info *)malloc(sizeof(p_info)); // p_info structure has pfn_list and pfn_list_size
    pfn *pfn_list = NULL;
    unsigned long long pfn_list_size = 0;
    unsigned long long pfn_list_capacity = 1024;

    pfn_list = (pfn *)malloc(sizeof(pfn) * pfn_list_capacity);

    char path[1024];
    sprintf(path, "/proc/%d/pagemap", pid);

    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        printf("Failed to open %s\n", path);
        free(pfn_list);
        pinfo->pfn_list_size = 0;
        return pinfo;
    }

    for (int i = 0; i < vaddr_list_size; i++)
    {
        unsigned long long start = vaddr_list[i].start;
        unsigned long long end = vaddr_list[i].end;

        unsigned long long start_page = start / PAGE_SIZE; // Get the virtual page number from virtual address
        unsigned long long end_page = end / PAGE_SIZE;

        for (unsigned long long page = start_page; page <= end_page; page++)
        {
            unsigned long long offset = page * sizeof(unsigned long long); // Get the offset from page
            unsigned long long data;                                       // Get the data from offset

            if (fseek(fp, offset, SEEK_SET) != 0)
            {
                printf("Failed to seek %s\n", path);
                exit(1);
            }

            if (fread(&data, sizeof(unsigned long long), 1, fp) != 1) //
            {
                // MAYBE REGION OF VSYSCALL OR SOMETHING CANNOT BE READ
                // printf("Failed to read pid:%d offset: %lld in %s\n", pid, offset, path);
                continue;
            }

            if (data & (1ULL << 63)) // Check the page is present
            {
                unsigned long long p_num = data & (((unsigned long long)1 << 55) - 1); // 55/64 bit is page frame number

                // pfn could exceed the range of PHYS_PAGES
                // pfn could be not in the region of SYSTEM_RAM
                // like I/O....
                if (p_num >= PHYS_PAGES)
                {
                    temp++;
                    continue;
                }

                if (pfn_list_size >= pfn_list_capacity)
                {
                    pfn_list_capacity *= 2;
                    pfn_list = (pfn *)realloc(pfn_list, sizeof(pfn) * pfn_list_capacity);
                }
                pfn_list[pfn_list_size++].number = p_num;
            }
        }
    }

    fclose(fp);

    pfn_list = (pfn *)realloc(pfn_list, sizeof(pfn) * pfn_list_size);

    pinfo->pfn_list = pfn_list;
    pinfo->pfn_list_size = pfn_list_size;

    return pinfo;
}

// Set the bitmap and return the free pages number
int free_pages_and_bitmap(physical_memory *p_memory, unsigned long long p_memory_size, unsigned long long *bitmap)
{
    unsigned long long used_pages = 0;
    // Set the bitmap
    for (int i = 0; i < p_memory_size; i++)
    {
        p_info *pfn_list = p_memory[i].pinfo;
        unsigned long long pfn_list_size = pfn_list->pfn_list_size;

        for (int j = 0; j < pfn_list_size; j++)
        {
            unsigned long long pfn = pfn_list->pfn_list[j].number;
            unsigned long long index = pfn / 64;
            unsigned long long offset = pfn % 64;

            bitmap[index] |= (1ULL << offset);

            used_pages++;
        }
    }

    return PHYS_PAGES - used_pages;
}

// Get the free pages in /proc/vmstat
// return the free pages number

unsigned long long count_free_pages(void)
{
    unsigned long long nr_free_pages = 0;

    char path[1024];
    sprintf(path, "/proc/vmstat");

    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        printf("Failed to open %s\n", path);
        exit(1);
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (strncmp(line, "nr_free_pages", 13) == 0)
        {
            sscanf(line, "%*s %lld", &nr_free_pages);
            break;
        }
    }

    fclose(fp);

    return nr_free_pages;
}

// Main function
int main(int argc, char *argv[])
{
    physical_memory *p_memory = NULL; // physical_memory structure has pid and pinfo
    unsigned long long p_memory_size = 0;
    unsigned long long p_memory_capacity = 0;

    unsigned long long total_free_pages;

    // Get the memory size and number of pages
    get_memory_size();
    get_num_of_pages();

    // Get the current pid list
    pid_list *pid_list = get_pid_list();
    unsigned long long pid_list_size = pid_list->size;

    p_memory_capacity = pid_list_size;
    p_memory = (physical_memory *)malloc(sizeof(physical_memory) * p_memory_capacity);

    // Get the physical memory information
    // by using pid and virtual address
    for (int i = 0; i < pid_list_size; i++)
    {
        int pid = pid_list->pid_list[i];
        v_info *vaddr_list = get_vaddr_list(pid);

        if (vaddr_list->vaddr_list_size == 0)
        {
            continue;
        }

        // Get the physical frame number list
        p_info *pfn_list = get_pfn_list(pid, vaddr_list->vaddr_list, vaddr_list->vaddr_list_size);

        p_memory[p_memory_size].pid = pid;
        p_memory[p_memory_size++].pinfo = pfn_list;
    }
    printf("PAGES NUMBERS: %lld\n", PHYS_PAGES);

    p_memory = (physical_memory *)realloc(p_memory, sizeof(physical_memory) * p_memory_size);

    // Print the physical memory status
    // by using physical frame number

    // We need to make the bitmap
    // to represent the physical memory status
    // 0: free, 1: used

    // We need to make the bitmap size
    // by using PHYS_PAGES
    // 1 bit represent 1 page
    // 1 byte represent 8 pages
    // 1 unsigned long long represent 64 pages

    // So we need to make the bitmap size
    // by using PHYS_PAGES / 64
    unsigned long long bitmap_size = (PHYS_PAGES / 64) + 1;
    unsigned long long bitmap_last_size = PHYS_PAGES % 64;
    unsigned long long *bitmap = (unsigned long long *)malloc(sizeof(unsigned long long) * bitmap_size);
    memset(bitmap, 0, sizeof(unsigned long long) * bitmap_size);

    // Get the free pages number and make the bitmap
    total_free_pages = free_pages_and_bitmap(p_memory, p_memory_size, bitmap);

    unsigned long long nr_free_pages = 0;

    // Count the error rate
    // using /proc/vmstat
    // first line nr_free_pages
    nr_free_pages = count_free_pages();

    // Print the bitmap
    // for (int i = 0; i < bitmap_size; i++)
    // {
    //     for (int j = 0; j < 64; j++)
    //     {
    //         if (i == bitmap_size - 1 && j == bitmap_last_size)
    //         {
    //             break;
    //         }
    //         if (bitmap[i] & (1ULL << j))
    //         {
    //             printf("1");
    //         }
    //         else
    //         {
    //             printf("0");
    //         }
    //     }
    //     printf("\n");
    // }

    // Print the error rate %
    printf("ERROR RATE: %f\n", (double)(total_free_pages - nr_free_pages) / nr_free_pages * 100);
    printf("TOTAL FREE PAGES: %lld\n", total_free_pages);
    printf("NR_FREE_PAGES: %lld\n", nr_free_pages);
    printf("TEMP: %lld\n", temp);

    return 0;
}
