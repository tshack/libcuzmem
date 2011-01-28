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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cuda.h>
#include "libcuzmem.h"

#define DEBUG

char plan_name[FILENAME_MAX];
char project_name[FILENAME_MAX];
int (*search)() = cuzmem_search_exhaustive;
unsigned int current_knob = 0;
unsigned int num_knobs = 0;
enum cuzmem_mode mode = CUZMEM_RUN;


cudaError_t
cudaMalloc (void **devPtr, size_t size)
{
    int use_global;
    CUresult driver_return;
    CUdeviceptr dev_mem;
    void* host_mem;

    // decide what to do with this... knob
    use_global = search();

    if (use_global) {
        // allocate gpu global memory
        driver_return = cuMemAlloc (&dev_mem, (unsigned int)size);
    } else {
        // allocate pinned host memory (probably broken for now)
        driver_return = cuMemAllocHost ((void **)&host_mem, (unsigned int)size);
        if (driver_return != CUDA_SUCCESS) { return cudaErrorMemoryAllocation; };
        driver_return = cuMemHostGetDevicePointer (&dev_mem, host_mem, 0);
    }
    *devPtr = (void *)dev_mem;

#if defined (DEBUG)
    printf ("*** | %s : %s | [%i KB] - #%i/%i\n",
            project_name, plan_name, (unsigned int)(size / 1024),
            current_knob, num_knobs-1);
#endif

    current_knob++;

    switch (driver_return)
    {
    case CUDA_SUCCESS:
        return (cudaSuccess);
    case CUDA_ERROR_DEINITIALIZED:
    case CUDA_ERROR_NOT_INITIALIZED:
    case CUDA_ERROR_INVALID_CONTEXT:
    case CUDA_ERROR_INVALID_VALUE:
    case CUDA_ERROR_OUT_OF_MEMORY:
    default:
        return (cudaErrorMemoryAllocation);
    }


}


int
cuzmem_search_exhaustive ()
{
#if defined (DEBUG)
    printf ("** Going with Global!\n");
#endif

    // use gpu global memory
    return 1;
}


void
cuzmem_search (enum cuzmem_search_mode mode)
{
    switch (mode)
    {
    case CUZMEM_EXHAUSTIVE:
    case CUZMEM_MAGIC:
    default:
        search = cuzmem_search_exhaustive;
    }
}


void
cuzmem_start (enum cuzmem_mode m)
{
    mode = m;
    current_knob = 0;
    printf ("Mode is: %i\n", mode);
}


void
cuzmem_end ()
{
    if (current_knob > num_knobs) {
        num_knobs = current_knob;
    }
}


void
cuzmem_plan (char* plan)
{
    strcpy (plan_name, plan);
}


void
cuzmem_project (char* project)
{
    strcpy (project_name, project);
}

#if defined (commentout)
void
cuzmem_plan (int count, ...)
{
    int i;
    va_list knobs;

    va_start (knobs, count);

    for (i=0; i<count; i++) {
        
    }

    va_end (knobs);
}
#endif
