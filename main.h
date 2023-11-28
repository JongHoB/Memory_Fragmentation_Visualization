#ifndef HEADER_H
#define HEADER_H
#endif

#include <stdio.h>
#include <stdlib.h>

typedef struct _p_list
{
    int size;
    int *pid_list;
} pid_list;

typedef struct _vaddr
{
    unsigned long long start;
    unsigned long long end;
} vaddr;

typedef struct _v_info
{
    vaddr *vaddr_list;
    unsigned long long vaddr_list_size;
} v_info;

typedef struct _paddr
{
    unsigned long long start;
    unsigned long long end;
} paddr;

typedef struct _p_info
{
    paddr *paddr_list;
    unsigned long long paddr_list_size;
} p_info;

typedef struct _physical_memory
{
    int pid;
    p_info *pinfo;
} physical_memory;