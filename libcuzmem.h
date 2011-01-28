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
int cuzmem_search_exhaustive ();

enum cuzmem_search_mode {
    CUZMEM_EXHAUSTIVE,
    CUZMEM_MAGIC
};

// libcuzmem modes
enum cuzmem_mode {
    CUZMEM_RUN,
    CUZMEM_TUNE
};

#if defined __cplusplus
extern "C" {
#endif
    void cuzmem_start (enum cuzmem_mode m);
    void cuzmem_end ();
    void cuzmem_project (char* project);
    void cuzmem_plan (char* plan);
    void cuzmem_search (enum cuzmem_search_mode mode);
//    void cuzmem_plan (int count, ...);
#if defined __cplusplus
};
#endif


#define CUZMEM_LOAD_SYMBOL(sym, lib)                                   \
    *(void **)(&sym) = dlsym (lib, #sym);                               


#define CUZMEM_HOOK_CUDA_MALLOC                                        \
    cudaError_t (*cudaMalloc)(void**, size_t);                         \
                                                                       \
    void* libcuzmem = dlopen ("./libcuzmem.so", RTLD_LAZY);            \
    if (!libcuzmem) { printf ("Error Loading libcuzmem\n"); exit(1); } \
    *(void **)(&cudaMalloc) = dlsym (libcuzmem, "cudaMalloc");          


#define CUZMEM_BENCH_INIT                                              \
    void (*cuzmem_start)();                                            \
    void (*cuzmem_end)();                                              \
    void (*cuzmem_project)(char*);                                     \
    void (*cuzmem_plan)(char*);                                        \
                                                                       \
    void* libcuzmem = dlopen ("./libcuzmem.so", RTLD_LAZY);            \
    if (!libcuzmem) { printf ("Error Loading libcuzmem\n"); exit(1); } \
    CUZMEM_LOAD_SYMBOL (cuzmem_start, libcuzmem);                      \
    CUZMEM_LOAD_SYMBOL (cuzmem_end, libcuzmem);                        \
    CUZMEM_LOAD_SYMBOL (cuzmem_project, libcuzmem);                    \
    CUZMEM_LOAD_SYMBOL (cuzmem_plan, libcuzmem);                        


#define CUZMEM_START(mode)   cuzmem_start_label: cuzmem_start(mode) ;

#define CUZMEM_END                             \
    cuzmem_end ();                             \
    goto cuzmem_start_label;                    
    

#endif // #ifndef __cuzmem_h__
