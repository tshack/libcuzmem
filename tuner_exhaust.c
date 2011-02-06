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
#include "context.h"
#include "plans.h"
#include "tuner_util.h"
#include "tuner_exhaust.h"

#define WORD_SIZE 8



//------------------------------------------------------------------------------
// TUNER INTERFACE
//------------------------------------------------------------------------------
cuzmem_plan*
cuzmem_tuner_exhaust (enum cuzmem_tuner_action action, void* parm)
{
    CUZMEM_CONTEXT ctx = get_context();

    // =========================================================================
    //  TUNER START
    // =========================================================================
    if (CUZMEM_TUNER_START == action) {
        // For now, do nothing special.
        if (ctx->tune_iter == 0) {
            // if we are in the 0th tuning cycle, do nothing here.
            // CUZMEM_TUNER_LOOKUP is determining the search space
            return NULL;
        }
        else {
            // start timing the iteration
            ctx->start_time = get_time ();
        }

        // Return value currently has no meaning
        return NULL;
    }
    // =========================================================================
    //  TUNER LOOKUP
    // =========================================================================
    else if (CUZMEM_TUNER_LOOKUP == action) {
        // parm: pointer to size of allocation
        size_t size = *(size_t*)(parm);

        CUresult ret;
        int loopy = 0;
        cuzmem_plan* entry = NULL;

        // default 0th tuning iteration handling
        if (ctx->tune_iter == 0) {
            return zeroth_lookup_handler (ctx, size);
        }

        // handle looping allocations & get current entry
        if (loopy_entry (ctx, &entry, size)) {
            return loopy_entry_handler (entry, size);
        }

        // exhaustive tuning
        // ---------------------------------------------------------------------
        entry->loc = (ctx->tune_iter >> ctx->current_knob) & 0x0001;

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

            // add large value to timer to invalidate this plan 
            ctx->start_time -= (0.50*ctx->start_time);
        }
        // ---------------------------------------------------------------------

        ctx->current_knob++;

        return entry;
    }
    // =========================================================================
    //  TUNER END
    // =========================================================================
    else if (CUZMEM_TUNER_END == action) {
        int i;
        double time;
        cuzmem_plan* entry = NULL;
        int all_global = 1;
        int satisfied = 0;
        unsigned int gpu_mem_free, gpu_mem_total, gpu_mem_req;
        unsigned int gpu_mem_min;
        CUresult ret;

        //------------------------------------------------------------
        // TUNE ITERATION ZERO
        //------------------------------------------------------------
        if (ctx->tune_iter == 0) {

            // check all entries for pinned host memory usage
            entry = ctx->plan;
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
                ctx->op_mode = CUZMEM_RUN;
                write_plan (ctx->plan, ctx->project_name, ctx->plan_name);
                return NULL;
            }

            // if everything didn't fit, size up the search space
            ctx->num_knobs = ctx->current_knob + 1;

            if (ctx->num_knobs <= sizeof(unsigned long long) * WORD_SIZE) {
                ctx->tune_iter_max = (unsigned long long)pow (2, ctx->num_knobs);
            } else {
                fprintf (stderr, "libcuzmem: allocation symbol limit exceeded!\n");
                exit(0);
            }
        }

        //------------------------------------------------------------
        // ALL TUNE ITERATIONS
        //------------------------------------------------------------
        // get the time to complete this iteration
        time = get_time() - ctx->start_time;

        if (time < ctx->best_time) {
            ctx->best_time = time;
            ctx->best_plan = ctx->tune_iter;
        }

        printf ("libcuzmem: best plan is #%i of %i\n", ctx->best_plan, ctx->tune_iter_max);

        // reset current knob for next tune iteration
        ctx->current_knob = 0;

        // pull down GPU global memory usage from CUDA driver
        ret = cuMemGetInfo (&gpu_mem_free, &gpu_mem_total);
        if (ret != CUDA_SUCCESS) {
            fprintf (stderr, "libcuzmem: could not retrieve GPU memory info from CUDA Driver!\n");
            exit (1);
        } else {
            // NOTE: cuMemGetInfo /seems/ to be over reporting free memory.
            //       *Perhaps* it is not counting memory allocated to the framebuffer
            //       or there is some built in limit for overhead or something.
            //       I just had an 149587200 byte allocation fail with
            //       CUerror = 2 (out of mem) when cuMemGetInfo reported
            //       164347904 bytes available!
            //
            //       So, I will say that 20MB must remain free.
            gpu_mem_min = (unsigned int)((float)gpu_mem_free * (float)ctx->gpu_mem_percent * 0.01f);
            gpu_mem_free -= 20000000;
            fprintf (stderr, "  Free   GPU Memory: %i\n", gpu_mem_free);
            fprintf (stderr, "  Total  GPU Memory: %i\n", gpu_mem_total);
            fprintf (stderr, "  Specified Percent: %i\n", ctx->gpu_mem_percent);
            fprintf (stderr, "  Specified Minimum: %i\n", gpu_mem_min);
        }

        // check to make sure the next iteration's plan draft meets the GPU
        // global memory utilization constraint if it doesn't, we will skip it
        // and subsequent plans until we find one that does.
        //
        // we also use this oppurtunity to clear out all of our inloop entry's
        // 1st hit flags
        i = ctx->tune_iter + 1;
        do {
            gpu_mem_req = 0;
            entry = ctx->plan;
            while (entry != NULL) {
                entry->loc = (i >> entry->id) & 0x0001;
                entry->first_hit = 1;
                if (entry->loc == 1) {
                    gpu_mem_req += entry->size;
                }
                entry = entry->next;
            }

            fprintf (stderr,
                        "  Request for Plan %i of %llu: %i (min: %i)\n",
                        i,
                        ctx->tune_iter_max,
                        gpu_mem_req,
                        gpu_mem_min
            );

            if ((gpu_mem_req >= gpu_mem_min) && (gpu_mem_req < gpu_mem_free)) {
                // we subtract one beacuse tune_iter is auto-increment after this
                // function returns (before the next tune iterations starts)
                ctx->tune_iter = i - 1;
                satisfied = 1;
            } else {
                i++;
            }
        } while (!satisfied);

        // have we exhausted the search space?
        if (ctx->tune_iter >= ctx->tune_iter_max) {
            // if so, stop iterating
            printf ("libcuzmem: auto-tuning complete.\n");
            ctx->op_mode = CUZMEM_RUN;

            // ...and write out the best plan
            entry = ctx->plan;
            while (entry != NULL) {
                entry->loc = (ctx->best_plan >> entry->id) & 0x0001;
                entry = entry->next;
            }
            write_plan (ctx->plan, ctx->project_name, ctx->plan_name);
        }

        // return value currently has no meaning
        return NULL;
    }
    // =========================================================================
    // TUNER: UNKNOWN ACTION SPECIFIED
    // =========================================================================
    else {
        printf ("libcuzmem: tuner asked to perform unknown action!\n");
        exit (1);
        return NULL;
    }
    // =========================================================================
}
