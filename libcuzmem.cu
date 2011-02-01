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
#include <math.h>
#include <cuda.h>
#include "libcuzmem.h"
#include "plans.h"

//#define DEBUG

//------------------------------------------------------------------------------
// STATE SYMBOLS                            ...I know!
//------------------------------------------------------------------------------
char plan_name[FILENAME_MAX];
char project_name[FILENAME_MAX];
int (*call_tuner)(enum cuzmem_tuner_action) = cuzmem_tuner_exhaustive;
unsigned int current_knob = 0;
unsigned int num_knobs = 0;
unsigned int tune_iter = 0;
unsigned int tune_iter_max = 0;
enum cuzmem_op_mode op_mode = CUZMEM_RUN;

cuzmem_plan *plan = NULL;

//------------------------------------------------------------------------------
// CUDA RUNTIME REPLACEMENTS
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
        entry->gpu_pointer = (void *)dev_mem;
        entry->gpu_dptr = dev_mem;
    }
    else if (entry->loc == 0) {
        // allocate pinned host memory (probably broken for now)
        ret = cuMemAllocHost ((void **)&host_mem, (unsigned int)size);
        if (ret != CUDA_SUCCESS) { return CUDA_ERROR_INVALID_VALUE; };
        ret = cuMemHostGetDevicePointer (&dev_mem, host_mem, 0);

        // record in entry entry for cudaFree() later on
        entry->cpu_pointer = (void *)host_mem;
        entry->gpu_pointer = (void *)dev_mem;
        entry->gpu_dptr = dev_mem;
    }
    else {
        // unspecified memory location
        fprintf (stderr, "libcuzmem: entry specifed malloc to neither pinned nor global memory!\n");
        exit (1);
    }

    return ret;
}


cudaError_t
cudaMalloc (void **devPtr, size_t size)
{
    CUresult ret;
    int use_global;
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
#if defined (DEBUG)
            fprintf (stderr, "libcuzmem: malloc/free loop detected\n");
#endif

            // Look for a free()ed "inloop" marked plan entry 
            entry = plan;
            while (1) {
                if (entry == NULL) {
                    fprintf (stderr, "libcuzmem: unable to deduce allocation from plan!\n");
                    exit (1);
                }
                if ((entry->inloop == 1)         &&
                    (entry->gpu_pointer == NULL) &&
                    (entry->size == size)) {
#if defined (DEBUG)
                        printf ("libcuzmem: looking for %i byte plan entry ...found (%i).\n", (int)entry->size, entry->id);
#endif
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
            current_knob++;
        }
    }
    else if (CUZMEM_TUNE == op_mode) {
        // 1) Load plan draft for this iteration

        // 2) Lookup current_knob in plan draft, determine malloc location
        use_global = call_tuner (CUZMEM_TUNER_LOOKUP);

        // 3) Allocate either pinned host or global device memory

        // Get ready for next knob
        current_knob++;
    }

#if defined (DEBUG)
    printf ("libcuzmem: %s:%s | %i Bytes  [%i/%i] ondev:%i\n",
            project_name, plan_name, (unsigned int)(size),
            current_knob, num_knobs-1, use_global);
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
    if (CUZMEM_RUN == op_mode) {
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
    }
    else if (CUZMEM_TUNE == op_mode) {
        // NOT YET IMPLEMENTED !
    }

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
// FRAMEWORK FUNCTIONS
//------------------------------------------------------------------------------

// Called at start of each plan invocation
void
cuzmem_start (enum cuzmem_op_mode m)
{
    char debug_mode[20];

    // This state info is modified for all engines.
    current_knob = 0;
    op_mode = m;

#if defined (DEBUG)
    if (CUZMEM_RUN == op_mode) {
        strcpy (debug_mode, "CUZMEM_RUN");
    }
    else if (CUZMEM_TUNE == op_mode) {
        strcpy (debug_mode, "CUZMEM_TUNE");
    }
    else {
        printf ("libcuzmem: unknown operation mode specified! (exiting)\n");
        exit (1);
    }

    printf ("libcuzmem: mode is %s\n", debug_mode);
#endif

    plan = read_plan (project_name, plan_name);

    // Invoke Tuner's "Start of Plan" routine.
    call_tuner (CUZMEM_TUNER_START);
}


// Called at end of each plan invocation
cuzmem_op_mode
cuzmem_end ()
{
    // Ask the selected Tuner Engine what to do.
    call_tuner (CUZMEM_TUNER_END);

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
    strcpy (project_name, project);
}


// Used to select/define CUZMEM plan
void
cuzmem_set_plan (char* plan)
{
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
        call_tuner = cuzmem_tuner_exhaustive;
    }
}



//------------------------------------------------------------------------------
// TUNING ENGINES
//------------------------------------------------------------------------------

// The default Tuning Engine
int
cuzmem_tuner_exhaustive (enum cuzmem_tuner_action action)
{
    if (CUZMEM_RUN == op_mode) {
        if (CUZMEM_TUNER_START == action) {
            // For now, do nothing special.
            return 0;
        }
        else if (CUZMEM_TUNER_LOOKUP == action) {
            // For now, just blindly put everything in device global
            return 1;
        }
        else if (CUZMEM_TUNER_END == action) {
            current_knob = 0;
            return 0;
        }
        else {
            printf ("libcuzmem: tuner asked to perform unknown run action!\n");
            exit (1);
        }
    }
    else if (CUZMEM_TUNE == op_mode) {
        if (CUZMEM_TUNER_START == action) {
            // For now, do nothing special.
            printf ("libcuzmem: iteration %i/%i\n", tune_iter, tune_iter_max);
            return 0;
        }
        else if (CUZMEM_TUNER_LOOKUP == action) {
            // For now, just blindly put everything in device global
            return 1;
        }
        else if (CUZMEM_TUNER_END == action) {
            // Record # of malloc encounters
            if (current_knob > num_knobs) {
                num_knobs = current_knob;
                tune_iter_max = (unsigned int)pow (2, num_knobs);
            }

            // Reset current knob for next iteration
            current_knob = 0;

            // Increment tune iteration count
            tune_iter++;

            // Have we exhausted the search space?
            if (tune_iter >= tune_iter_max) {
                // If so, stop iterating
                op_mode = CUZMEM_RUN;
            }
                return 0;
            }
        else {
            printf ("libcuzmem: tuner asked to perform unknown tune action!\n");
            exit (1);
        }
    }

    // We should never get to here
    exit (1);
    return 0;
}
