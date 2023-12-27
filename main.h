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
    __uint64_t start;
    __uint64_t end;
} vaddr;

typedef struct _v_info
{
    vaddr *vaddr_list;
    __uint64_t vaddr_list_size;
} v_info;

typedef struct _pfn
{
    __uint64_t number;
} pfn;

typedef struct _p_info
{
    pfn *pfn_list;
    __uint64_t pfn_list_size;
} p_info;

typedef struct _physical_memory
{
    int pid;
    p_info *pinfo;
} physical_memory;
