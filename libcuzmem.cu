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
#include <cuda.h>
#include "libcuzmem.h"
#include "plans.h"
#include "tuner_exhaust.h"

//#define DEBUG

extern "C"
CUresult alloc_mem (cuzmem_plan* entry, size_t size);

//------------------------------------------------------------------------------
// STATE SYMBOLS                            ...I know!
//------------------------------------------------------------------------------
char plan_name[FILENAME_MAX];
char project_name[FILENAME_MAX];
cuzmem_plan* (*call_tuner)(enum cuzmem_tuner_action, void*) = cuzmem_tuner_exhaust;
unsigned int current_knob = 0;
unsigned int num_knobs = 0;
unsigned long long tune_iter = 0;
unsigned long long tune_iter_max = 0;
enum cuzmem_op_mode op_mode = CUZMEM_RUN;

cuzmem_plan *plan = NULL;

//------------------------------------------------------------------------------
// CUDA RUNTIME REPLACEMENTS
//------------------------------------------------------------------------------

cudaError_t
cudaMalloc (void **devPtr, size_t size)
{
    CUresult ret;
    cuzmem_plan *entry = NULL;

    *devPtr = NULL;

    // Decide what to do with current knob
    if (CUZMEM_RUN == op_mode) {
        // 1) Load plan for this project
        entry = plan;

        // 2) Lookup malloc type for this knob & allocate
        while (entry != NULL) {
            if (entry->id == current_knob) {
                ret = alloc_mem (entry, size);
                *devPtr = entry->gpu_pointer;
                break;
            }
            entry = entry->next;
        }

        // Knob id exceeds those found in plan... must be in a malloc/free loop
        if (*devPtr == NULL) {
            // Look for a free()ed "inloop" marked plan entry 
            entry = plan;
            while (1) {
                if (entry == NULL) {
                    fprintf (stderr,"libcuzmem: unable to deduce inloop allocation from plan!\n");
                    exit (1);
                }
                if ((entry->inloop == 1)         &&
                    (entry->gpu_pointer == NULL) &&
                    (entry->size == size))
                {
                        ret = alloc_mem (entry, size);
                        if (ret != CUDA_SUCCESS) {
                            fprintf (stderr, "libcuzmem: inloop alloc_mem() failed [%i]\n", ret);
                        }
                        *devPtr = entry->gpu_pointer;
                        break;
                }
                entry = entry->next;
            }
        } else {
            // Don't increment current_knob for inloop allocations,
            // they are knobs that we have already counted!
            current_knob++;
        }
    }
    else if (CUZMEM_TUNE == op_mode) {
        // 1) Load plan draft for this iteration
        // 2) Lookup current_knob in plan draft, determine malloc location
        entry = call_tuner (CUZMEM_TUNER_LOOKUP, &size);
        if (entry == NULL) {
            ret = CUDA_ERROR_NOT_INITIALIZED;
        } else {
            *devPtr = entry->gpu_pointer;
        }
    }

#if defined (DEBUG)
    printf ("libcuzmem: %s:%s | %i Bytes  [%i/%i]\n",
            project_name, plan_name, (unsigned int)(size),
            current_knob, num_knobs);
#endif

    // Morph CUDA Driver return codes into CUDA Runtime codes
    switch (ret)
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

cudaError_t
cudaFree (void *devPtr)
{
    CUresult ret;
    cuzmem_plan *entry = NULL;

    // Decide how to free this chunk of gpu mapped memory
//    if (CUZMEM_RUN == op_mode) {
        entry = plan;

        // Lookup plan entry for this gpu pointer
        while (1) {
            if (entry == NULL) {
                fprintf (stderr, "libcuzmem: attempt to free invalid pointer (%p).\n", devPtr);
                exit (1);
            }
            if (entry->gpu_pointer == devPtr) {
                break;
            }
            entry = entry->next;
        }

        // Was it pinned cpu memory or real gpu memory?
        if (entry->cpu_pointer == NULL) {
            // real gpu memory
#if defined (DEBUG)
            printf ("libcuzmem: freeing %i\n", entry->id);
#endif
            ret = cuMemFree (entry->gpu_dptr);
            entry->gpu_pointer = NULL;
        } else {
            // pinned cpu memory
            // NOT YET IMPLEMENTED !
        }
//    }
//    else if (CUZMEM_TUNE == op_mode) {
        // NOT YET IMPLEMENTED !
//    }

    if (ret != CUDA_SUCCESS) {
        fprintf (stderr, "libcuzmem: cudaFree() failed\n");
    } 

    // Morph CUDA Driver return codes into CUDA Runtime codes
    switch (ret)
    {
    case CUDA_SUCCESS:
        return (cudaSuccess);
    case CUDA_ERROR_DEINITIALIZED:
    case CUDA_ERROR_NOT_INITIALIZED:
        return (cudaErrorInitializationError);
    case CUDA_ERROR_INVALID_CONTEXT:
    case CUDA_ERROR_INVALID_VALUE:
    case CUDA_ERROR_OUT_OF_MEMORY:
    default:
        return (cudaErrorInvalidDevicePointer);
    }
}



//------------------------------------------------------------------------------
// CUDA RUNTIME REPLACEMENT HELPERS
//------------------------------------------------------------------------------

CUresult
alloc_mem (cuzmem_plan* entry, size_t size)
{
    CUresult ret;
    CUdeviceptr dev_mem;
    void* host_mem = NULL;

    if (entry->loc == 1) {
        // allocate gpu global memory
        ret = cuMemAlloc (&dev_mem, (unsigned int)size);

        // record in entry entry for cudaFree() later on
        if (ret == CUDA_SUCCESS) {
            entry->gpu_pointer = (void *)dev_mem;
            entry->gpu_dptr = dev_mem;
        }
    }
    else if (entry->loc == 0) {
        // allocate pinned host memory (probably broken for now)
        ret = cuMemHostAlloc ((void **)&host_mem, size,
                CU_MEMHOSTALLOC_PORTABLE |
                CU_MEMHOSTALLOC_DEVICEMAP |
                CU_MEMHOSTALLOC_WRITECOMBINED);
        if (ret != CUDA_SUCCESS) {
            fprintf (stderr, "libcuzmem: failed to pin cpu memory [%i]\n", ret);
            return CUDA_ERROR_INVALID_VALUE;
        };
        ret = cuMemHostGetDevicePointer (&dev_mem, host_mem, 0);

        // record in entry for cudaFree() later on
        if (ret == CUDA_SUCCESS) {
            entry->cpu_pointer = (void *)host_mem;
            entry->gpu_pointer = (void *)dev_mem;
            entry->gpu_dptr = dev_mem;
        } else {
            fprintf (stderr, "libcuzmem: failed to map pinned cpu memory\n");
        }
    }
    else {
        // unspecified memory location
        fprintf (stderr, "libcuzmem: entry specifed malloc to neither pinned nor global memory!\n");
        exit (1);
    }

    return ret;
}



//------------------------------------------------------------------------------
// FRAMEWORK FUNCTIONS
//------------------------------------------------------------------------------

// Called at start of each plan invocation
void
cuzmem_start (enum cuzmem_op_mode m)
{
#if defined (DEBUG)
    char debug_mode[20];
#endif

    // This state info is modified for all engines.
    current_knob = 0;
    op_mode = m;

#if defined (DEBUG)
    if (CUZMEM_RUN == op_mode) { strcpy (debug_mode, "CUZMEM_RUN"); }
    else if (CUZMEM_TUNE == op_mode) { strcpy (debug_mode, "CUZMEM_TUNE"); }
    else { printf ("libcuzmem: unknown operation mode specified! (exiting)\n"); exit (1); }
    printf ("libcuzmem: mode is %s\n", debug_mode);
#endif

    if (CUZMEM_RUN == op_mode) {
        plan = read_plan (project_name, plan_name);
    }
    // Invoke Tuner's "Start of Plan" routine.
    else if (CUZMEM_TUNE == op_mode) {
        call_tuner (CUZMEM_TUNER_START, NULL);
    }
    else {
        fprintf (stderr, "libcuzmem: unknown operation mode specified!\n");
    }
}


// Called at end of each plan invocation
cuzmem_op_mode
cuzmem_end ()
{
    // Ask the selected Tuner Engine what to do.
    if (CUZMEM_TUNE == op_mode) {
        call_tuner (CUZMEM_TUNER_END, NULL);
        tune_iter++;
    }

    // Return this back to calling program so that the
    // framework will know what to do next: next iteration
    // or stop iterating.
    return op_mode;
}



//------------------------------------------------------------------------------
// USER INTERFACE FUNCTIONS
//------------------------------------------------------------------------------

// Used to select/define CUZMEM project
void
cuzmem_set_project (char* project)
{
#if defined (DEBUG)
    fprintf (stderr, "libcuzmem: cuzmem_set_project() called\n");
#endif

    strcpy (project_name, project);
}


// Used to select/define CUZMEM plan
void
cuzmem_set_plan (char* plan)
{
#if defined (DEBUG)
    fprintf (stderr, "libcuzmem: cuzmem_set_plan() called\n");
#endif

    strcpy (plan_name, plan);
}


// Used to select Tuning Engine
void
cuzmem_set_tuner (enum cuzmem_tuner t)
{
    switch (t)
    {
    case CUZMEM_EXHAUSTIVE:
    default:
        call_tuner = cuzmem_tuner_exhaust;
    }
}

