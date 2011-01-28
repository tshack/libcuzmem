/*  This file is part of libcuzmem
    Copyright (C) 2011  James A. Shackleford

    libcuzmem is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __cuzmem_h__
#define __cuzmem_h__

#include <cuda.h>
#include <dlfcn.h>

#if defined (commentout)
typedef struct plan_entry_struct plan_entry;
struct plan_entry_struct
{
    int knob_id;

    void* gpu_pointer;
    void* cpu_pointer;

    size_t size;

    plan_entry* next;
};
#endif


// Search algorithms... currently only the one.
int cuzptune_search_exhaustive ();

enum cuzptune_search_mode {
    CUZPTUNE_EXHAUSTIVE,
    CUZPTUNE_MAGIC
};


#if defined __cplusplus
extern "C" {
#endif
    void cuzptune_start ();
    void cuzptune_end ();
    void cuzptune_project (char* project);
    void cuzptune_plan (char* plan);
    void cuzptune_search (enum cuzptune_search_mode mode);
//    void cuzptune_plan (int count, ...);
#if defined __cplusplus
};
#endif


#define CUZP_LOAD_SYMBOL(sym, lib)                                         \
    *(void **)(&sym) = dlsym (lib, #sym);                                   

#define CUZP_HOOK_CUDA_MALLOC                                              \
    cudaError_t (*cudaMalloc)(void**, size_t);                             \
                                                                           \
    void* libcuzptune = dlopen ("./libcuzptune.so", RTLD_LAZY);            \
    if (!libcuzptune) { printf ("Error Loading libcuzptune\n"); exit(1); } \
    *(void **)(&cudaMalloc) = dlsym (libcuzptune, "cudaMalloc");            


#define CUZP_BENCH_INIT                                                    \
    void (*cuzptune_start)();                                              \
    void (*cuzptune_end)();                                                \
    void (*cuzptune_project)(char*);                                       \
    void (*cuzptune_plan)(char*);                                          \
                                                                           \
    void* libcuzptune = dlopen ("./libcuzptune.so", RTLD_LAZY);            \
    if (!libcuzptune) { printf ("Error Loading libcuzptune\n"); exit(1); } \
    CUZP_LOAD_SYMBOL (cuzptune_start, libcuzptune);                        \
    CUZP_LOAD_SYMBOL (cuzptune_end, libcuzptune);                          \
    CUZP_LOAD_SYMBOL (cuzptune_project, libcuzptune);                      \
    CUZP_LOAD_SYMBOL (cuzptune_plan, libcuzptune);                          


#define CUZP_START   cuzp_start_label: cuzptune_start() ;

#define CUZP_END                             \
    cuzptune_end ();                         \
    goto cuzp_start_label;                    
    

#endif // #ifndef __cuzptune_h__
