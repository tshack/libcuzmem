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

// -- Tuning Engine stuff ------------------------
enum cuzmem_tuner_action {
    CUZMEM_TUNER_START,
    CUZMEM_TUNER_LOOKUP,
    CUZMEM_TUNER_END
};

enum cuzmem_tuner {
    CUZMEM_EXHAUSTIVE
};

int cuzmem_tuner_exhaustive (enum cuzmem_tuner_action action);
// -----------------------------------------------


// -- libcuzmem operation modes ------------------
enum cuzmem_op_mode {
    CUZMEM_RUN,
    CUZMEM_TUNE
};
// -----------------------------------------------


// -- Export symbols -----------------------------
#if defined __cplusplus
extern "C" {
#endif
    // Framework symbols
    void cuzmem_start (enum cuzmem_op_mode m);
    enum cuzmem_op_mode cuzmem_end ();
    // User symbols
    void cuzmem_set_project (char* project);
    void cuzmem_set_plan (char* plan);
    void cuzmem_set_tuner (enum cuzmem_tuner t);
//    void cuzmem_plan (int count, ...);
#if defined __cplusplus
};
#endif
// -----------------------------------------------


// -- Some fairly nasty Framework Macros ---------
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
    enum cuzmem_op_mode (*cuzmem_end)();                               \
    void (*cuzmem_set_project)(char*);                                 \
    void (*cuzmem_set_plan)(char*);                                    \
    void (*cuzmem_set_tuner)(enum cuzmem_tuner);                       \
                                                                       \
    void* libcuzmem = dlopen ("./libcuzmem.so", RTLD_LAZY);            \
    if (!libcuzmem) { printf ("Error Loading libcuzmem\n"); exit(1); } \
    CUZMEM_LOAD_SYMBOL (cuzmem_start, libcuzmem);                      \
    CUZMEM_LOAD_SYMBOL (cuzmem_end, libcuzmem);                        \
    CUZMEM_LOAD_SYMBOL (cuzmem_set_project, libcuzmem);                \
    CUZMEM_LOAD_SYMBOL (cuzmem_set_plan, libcuzmem);                   \
    CUZMEM_LOAD_SYMBOL (cuzmem_set_tuner, libcuzmem);                   


#define CUZMEM_START(mode)   cuzmem_start_label: cuzmem_start(mode) ;


#define CUZMEM_END                             \
    if (CUZMEM_TUNE == cuzmem_end()) {         \
        goto cuzmem_start_label;               \
    }                                           
// -----------------------------------------------

    

#endif // #ifndef __cuzmem_h__
