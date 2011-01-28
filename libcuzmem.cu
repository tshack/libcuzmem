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

#define DEBUG

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


//------------------------------------------------------------------------------
// CUDA RUNTIME REPLACEMENTS
//------------------------------------------------------------------------------

cudaError_t
cudaMalloc (void **devPtr, size_t size)
{
    int use_global;
    CUresult driver_return;
    CUdeviceptr dev_mem;
    void* host_mem;

    // Decide what to do with current knob
    if (CUZMEM_RUN == op_mode) {
        // 1) Load plan for this project
//        plan_load (plan_name, project_name);

        // 2) Using current_knob, determine malloc location
        use_global = call_tuner (CUZMEM_TUNER_LOOKUP);

        // 3) Allocate either pinned host or global device memory
    }
    else if (CUZMEM_TUNE == op_mode) {
        // 1) Load plan draft for this iteration

        // 2) Lookup current_knob in plan draft, determine malloc location
        use_global = call_tuner (CUZMEM_TUNER_LOOKUP);

        // 3) Allocate either pinned host or global device memory
    }

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
    printf ("libcuzmem: %s:%s | %i KB  [%i/%i] ondev:%i\n",
            project_name, plan_name, (unsigned int)(size / 1024),
            current_knob, num_knobs-1, use_global);
#endif

    // Get ready for next knob
    current_knob++;

    // Morph CUDA Driver return codes into CUDA Runtime codes
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
            // For now, do nothing special.
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
