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
#include <time.h>
#include "main.h"

__uint64_t MEM_SIZE;
__uint64_t PAGE_SIZE;
__uint64_t PHYS_PAGES;
// page mask for 64bit architecture

__uint64_t temp = 0;
__uint64_t shared = 0;

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
    __uint64_t vaddr_list_size = 0;
    __uint64_t vaddr_list_capacity = 1024;

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
        __uint64_t start, end;
        sscanf(line, "%lx-%lx", &start, &end); // Get the virtual address from /proc/pid/maps

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
    __uint64_t pfn_list_size = 0;
    __uint64_t pfn_list_capacity = 1024;

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
        __uint64_t start = vaddr_list[i].start;
        __uint64_t end = vaddr_list[i].end;

        __uint64_t start_page = start / PAGE_SIZE; // Get the virtual page number from virtual address
        __uint64_t end_page = end / PAGE_SIZE;

        for (__uint64_t page = start_page; page <= end_page; page++)
        {
            __uint64_t offset = page * sizeof(__uint64_t); // Get the offset from page
            __uint64_t data;                               // Get the data from offset

            if (fseek(fp, offset, SEEK_SET) != 0)
            {
                printf("Failed to seek %s\n", path);
                exit(1);
            }

            if (fread(&data, sizeof(__uint64_t), 1, fp) != 1) //
            {
                // MAYBE REGION OF VSYSCALL OR SOMETHING CANNOT BE READ
                // printf("Failed to read pid:%d offset: %ld in %s\n", pid, offset, path);
                continue;
            }

            if (data & (1ULL << 63)) // Check the page is present
            {
                __uint64_t p_num = data & (((__uint64_t)1 << 55) - 1); // 55/64 bit is page frame number

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
int get_free_pages_num_and_set_bitmap(physical_memory *p_memory, __uint64_t p_memory_size, __uint64_t *bitmap)
{
    __uint64_t used_pages = 0;
    // Set the bitmap
    for (int i = 0; i < p_memory_size; i++)
    {
        p_info *pfn_list = p_memory[i].pinfo;
        __uint64_t pfn_list_size = pfn_list->pfn_list_size;

        for (int j = 0; j < pfn_list_size; j++)
        {
            __uint64_t pfn = pfn_list->pfn_list[j].number;
            __uint64_t index = pfn / 64;
            __uint64_t offset = pfn % 64;

            if (bitmap[index] & (1ULL << offset))
            {
                shared++;
                used_pages--;
            }

            bitmap[index] |= (1ULL << offset);

            used_pages++;
        }
    }

    return PHYS_PAGES - used_pages;
}

// Get the free pages in /proc/vmstat
// return the free pages number

__uint64_t count_free_pages(void)
{
    __uint64_t nr_free_pages = 0;
#if defined(_SC_AVPHYS_PAGES)
    {
        nr_free_pages = sysconf(_SC_AVPHYS_PAGES);
    }
#else
    {

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
                sscanf(line, "%*s %ld", &nr_free_pages);
                break;
            }
        }

        fclose(fp);
    }
#endif

    return nr_free_pages;
}

// Make image file using bitmap
// each page is represented by 1 pixel
// gray scale image no rgb
// the color of pixel is black or white
// 0: used, 255: out of range, 150: free
// image size is 2048 * 2048
// image name is current time.bmp

void make_image(__uint64_t *bitmap, __uint64_t bitmap_size, __uint64_t bitmap_last_size)
{
    unsigned char *image = (unsigned char *)malloc(sizeof(unsigned char) * 2048 * 2048);
    memset(image, 255, sizeof(unsigned char) * 2048 * 2048);

    for (int i = 0; i < bitmap_size; i++)
    {
        for (int j = 0; j < 64; j++)
        {
            if (i == bitmap_size - 1 && j == bitmap_last_size)
            {
                break;
            }
            if (bitmap[i] & (1ULL << j))
            {
                image[i * 64 + j] = 0;
            }
            else
            {
                image[i * 64 + j] = 150;
            }
        }
    }

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    char filename[1024];
    sprintf(filename, "%d-%d-%d-%d-%d-%d.bmp", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    FILE *fp = fopen(filename, "wb");
    if (fp == NULL)
    {
        printf("Failed to open %s\n", filename);
        exit(1);
    }

    unsigned char header[54] = {
        0x42,        // identity : B
        0x4d,        // identity : M
        0, 0, 0, 0,  // file size
        0, 0,        // reserved1
        0, 0,        // reserved2
        54, 0, 0, 0, // RGB data offset
        40, 0, 0, 0, // struct BITMAPINFOHEADER size
        0, 0, 0, 0,  // bmp width
        0, 0, 0, 0,  // bmp height
        1, 0,        // planes
        24, 0,       // bit per pixel
        0, 0, 0, 0,  // compression
        0, 0, 0, 0,  // data size
        0, 0, 0, 0,  // h resolution
        0, 0, 0, 0,  // v resolution
        0, 0, 0, 0,  // used colors
        0, 0, 0, 0   // important colors
    };

    __uint64_t file_size = 54 + 2048 * 2048 * 3;
    header[2] = (unsigned char)(file_size & 0x000000ff);
    header[3] = (file_size >> 8) & 0x000000ff;
    header[4] = (file_size >> 16) & 0x000000ff;
    header[5] = (file_size >> 24) & 0x000000ff;

    __uint64_t width = 2048;
    header[18] = width & 0x000000ff;
    header[19] = (width >> 8) & 0x000000ff;
    header[20] = (width >> 16) & 0x000000ff;

    __uint64_t height = 2048;
    header[22] = height & 0x000000ff;
    header[23] = (height >> 8) & 0x000000ff;
    header[24] = (height >> 16) & 0x000000ff;

    fwrite(header, sizeof(unsigned char), 54, fp);
    fwrite(image, sizeof(unsigned char), 2048 * 2048, fp);

    fclose(fp);

    free(image);

    return;
}

// Get the physical memory information
// by using pid and virtual address
physical_memory *get_phys_mem_infos(pid_list *pid_list, __uint64_t pid_list_size, __uint64_t *p_memory_size, __uint64_t *p_memory_capacity)
{
    physical_memory *p_memory = NULL; // physical_memory structure has pid and pinfo
    __uint64_t p_mem_size = 0;

    p_memory = (physical_memory *)malloc(sizeof(physical_memory) * *p_memory_capacity);

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

        p_memory[p_mem_size].pid = pid;
        p_memory[p_mem_size++].pinfo = pfn_list;
    }

    *p_memory_size = p_mem_size;

    return p_memory;
}

// Main function
int main(int argc, char *argv[])
{
    physical_memory *p_memory = NULL; // physical_memory structure has pid and pinfo
    __uint64_t p_memory_size = 0;
    __uint64_t p_memory_capacity = 0;

    __uint64_t total_free_pages;

    // Get the memory size and number of pages
    get_memory_size();
    get_num_of_pages();

    // Get the current pid list
    pid_list *pid_list = get_pid_list();
    __uint64_t pid_list_size = pid_list->size;

    p_memory_capacity = pid_list_size;

    // Get the physical memory information
    // by using pid and virtual address
    p_memory = get_phys_mem_infos(pid_list, pid_list_size, &p_memory_size, &p_memory_capacity);
    p_memory = (physical_memory *)realloc(p_memory, sizeof(physical_memory) * p_memory_size);

    // Count the error rate
    // using _SC__SC_AVPHYS_PAGES or /proc/vmstat
    // first line nr_free_pages
    __uint64_t nr_free_pages = 0;
    nr_free_pages = count_free_pages();

    // Print the physical memory status
    // by using physical frame number

    // We need to make the bitmap
    // to represent the physical memory status
    // 0: free, 1: used

    // We need to make the bitmap size
    // by using PHYS_PAGES
    // 1 bit represent 1 page
    // 1 byte represent 8 pages
    // 1 __uint64_t represent 64 pages

    // So we need to make the bitmap size
    // by using PHYS_PAGES / 64
    __uint64_t bitmap_size = (PHYS_PAGES / 64) + 1;
    __uint64_t bitmap_last_size = PHYS_PAGES % 64;
    __uint64_t *bitmap = (__uint64_t *)malloc(sizeof(__uint64_t) * bitmap_size);
    memset(bitmap, 0, sizeof(__uint64_t) * bitmap_size);

    // Get the free pages number and make the bitmap
    total_free_pages = get_free_pages_num_and_set_bitmap(p_memory, p_memory_size, bitmap);

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
    printf("TOTAL PAGES NUMBERS: %ld\n", PHYS_PAGES);
    printf("ERROR RATE: %f\n", (double)(total_free_pages - nr_free_pages) / nr_free_pages * 100);
    printf("ERROR PAGES: %ld\n", total_free_pages - nr_free_pages);
    printf("ERROR MB: %fMB\n", (double)(total_free_pages - nr_free_pages) * PAGE_SIZE / 1024 / 1024);
    printf("TOTAL COUNTED FREE PAGES: %ld\n", total_free_pages);
    printf("USED MB: %fMB\n", (double)(PHYS_PAGES - total_free_pages) * PAGE_SIZE / 1024 / 1024);
    printf("AVAILABLE PHYSICAL FREE PAGES: %ld\n", nr_free_pages);
    printf("TEMP(pfn could exceed the range of PHYS_PAGES): %ld\n", temp);
    printf("SHARED pfns: %ld\n", shared);

    // Make the image file
    // make_image(bitmap, bitmap_size, bitmap_last_size);

    // Free the memory
    free(bitmap);
    for (int i = 0; i < p_memory_size; i++)
    {
        if (p_memory[i].pinfo->pfn_list != NULL)
            free(p_memory[i].pinfo->pfn_list);
        if (p_memory[i].pinfo != NULL)
            free(p_memory[i].pinfo);
    }
    free(p_memory);
    free(pid_list->pid_list);
    free(pid_list);

    return 0;
}

