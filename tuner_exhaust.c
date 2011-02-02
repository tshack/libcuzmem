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
#include <math.h>
#include <limits.h>
#include "tuner.h"
#include "tuner_exhaust.h"
#include "plans.h"

#define WORD_SIZE 8

// returns number of bits required to express n combinations
unsigned int
num_bits (unsigned long long n)
{
    unsigned int exp = 0;
    unsigned long long p = 2;
    while (p < n && p != 0) {
        ++exp;
        p *= 2;
    }
    return exp + 1;
}

unsigned int
detect_inloop (cuzmem_plan** entry, size_t size)
{
    while (*entry != NULL) {
        if (((*entry)->size == size) && ((*entry)->gpu_pointer == NULL)) {
            // found a malloc/free loop within tuning loop
            return 1;
        }
        *entry = (*entry)->next;
    }

    return 0;
}

//------------------------------------------------------------------------------
// TUNER INTERFACE
//------------------------------------------------------------------------------
cuzmem_plan*
cuzmem_tuner_exhaust (enum cuzmem_tuner_action action, void* parm)
{
    if (CUZMEM_TUNER_START == action) {
        // For now, do nothing special.
        if (tune_iter == 0) {
            // if we are in the 0th tuning cycle, do nothing here.
            // CUZMEM_TUNER_LOOKUP is building a base plan draft and
            // is also determining the search space.
            printf ("libcuzmem: starting exhaustive tune\n");
            return NULL;
        }
        else {

        }

        // Return value currently has no meaning
        return NULL;
    }
    else if (CUZMEM_TUNER_LOOKUP == action) {
        // parm: pointer to size of allocation
        size_t size = *(size_t*)(parm);

        CUresult ret;
        int is_inloop = 0;
        cuzmem_plan* entry = NULL;

        // For the 0th iteration, build a base plan draft that
        // first fills GPU global memory and then spills over
        // into pinned CPU memory.
        if (tune_iter == 0) {
            // 1st try to detect if this allocation is an inloop entry.
            entry = plan;
            is_inloop = detect_inloop (&entry, size);

            if (is_inloop) {
                entry->inloop = 1;
                ret = alloc_mem (entry, size);
                if (ret != CUDA_SUCCESS) {
                    // Note, cudaMalloc() will report a NULL return value
                    // from call_tuner(LOOKUP) as cudaErrorMemoryAllocation
                    entry = NULL;
                }
            } else {
                entry = (cuzmem_plan*) malloc (sizeof(cuzmem_plan));
                entry->id = current_knob;
                entry->size = size;
                entry->loc = 1;
                entry->inloop = 0;
                entry->cpu_pointer = NULL;
                entry->gpu_pointer = NULL;

                ret = alloc_mem (entry, size);
                if (ret != CUDA_SUCCESS) {
                    // out of gpu global memory: move to pinned CPU
                    entry->loc = 0;
                    ret = alloc_mem (entry, size);
                    if (ret != CUDA_SUCCESS) {
                        // not enough CPU memory: return failure
                        free (entry);
                        entry = NULL;
                    }
                }

                // Insert successful entry into plan draft
                entry->next = plan;
                plan = entry;

                current_knob++;
            }
        } else {
            // tuning iteration is greater than zero

            // 1st try to detect if this allocation is an inloop entry.
            entry = plan;
            is_inloop = detect_inloop (&entry, size);

            if (is_inloop) {
                entry->inloop = 1;
                ret = alloc_mem (entry, size);
                if (ret != CUDA_SUCCESS) {
                    // Note, cudaMalloc() will report a NULL return value
                    // from call_tuner(LOOKUP) as cudaErrorMemoryAllocation
                    entry = NULL;
                }
            } else {
                entry = plan;
                while (entry != NULL) {
                    if (entry->id == current_knob) {
                        break;
                    }
                    entry = entry->next;
                }

                entry->loc = (tune_iter >> current_knob) & 0x0001;

                ret = alloc_mem (entry, size);
                if (ret != CUDA_SUCCESS) {
                    // This plan was bad.
                    // We will move this allocation just to finish the tuning
                    // cycle.  We also invalidate this plan.
                    entry->loc ^= 0x0001;

                    ret = alloc_mem (entry, size);
                    if (ret != CUDA_SUCCESS) {
                        // not enough CPU memory: return failure
                        free (entry);
                        entry = NULL;
                    }

                    // TODO
                    // add large value to timer to invalidate this plan 
                }
                current_knob++;
            }
        }

        return entry;
    }
    else if (CUZMEM_TUNER_END == action) {
        cuzmem_plan* entry = NULL;
        int all_global = 1;

        // do special stuff @ end of tune iteration zero
        if (tune_iter == 0) {

            // check all entries for pinned host memory usage
            entry = plan;
            while (entry != NULL) {
                if (entry->loc != 1) {
                    all_global = 0;
                    break;
                }
                entry = entry->next;
            }

            // quit now if everything fits in gpu memory
            if (all_global) {
                printf ("libcuzmem: auto-tuning complete.\n");
                op_mode = CUZMEM_RUN;
                write_plan (plan, project_name, plan_name);
                return NULL;
            }

            // if everything didn't fit, size up the search space
            num_knobs = current_knob + 1;

            if (num_knobs <= sizeof(unsigned long long) * WORD_SIZE) {
                tune_iter_max = (unsigned long long)pow (2, num_knobs);
            } else {
                fprintf (stderr, "libcuzmem: memory allocation limit exceeded!\n");
                exit(0);
            }
        }

        // reset current knob for next tune iteration
        current_knob = 0;

        // have we exhausted the search space?
        if (tune_iter >= tune_iter_max) {
            // if so, stop iterating
            printf ("libcuzmem: auto-tuning complete.\n");
            op_mode = CUZMEM_RUN;

            // ...and write out the plan
            write_plan (plan, "plastimatch", "foobaz");
        }

        // return value currently has no meaning
        return NULL;
    }
    else {
        printf ("libcuzmem: tuner asked to perform unknown action!\n");
        exit (1);
        return NULL;
    }
}
