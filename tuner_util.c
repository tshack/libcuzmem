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

#include <cuda.h>
#include <stdlib.h>
#include "context.h"
#include "plans.h"

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

// detect if requested malloc is recurring within a single
// optimization iteration loop
unsigned int
detect_inloop (cuzmem_plan** entry, size_t size)
{
    while (*entry != NULL) {
        if (((*entry)->size == size) && ((*entry)->gpu_pointer == NULL)) {
            // found a malloc/free loop within tuning loop
            (*entry)->inloop = 1;
            return 1;
        }
        *entry = (*entry)->next;
    }

    return 0;
}

// checks if the requested malloc is known to reoccur within
// a single optimization iteration
unsigned int
check_inloop (cuzmem_plan** entry, size_t size)
{
    while (*entry != NULL) {
        if (((*entry)->size == size)        &&
            ((*entry)->gpu_pointer == NULL) &&
            ((*entry)->inloop == 1)) {
            // found a matching (& unused) inloop entry
            return 1;
        }
        *entry = (*entry)->next;
    }

    return 0;
}

// standard 0th iteration logic
// * powers through the first iteration by whatever means necessary
// * attempts to detect "loopy" allocations
// * builds the (0th generation) plan draft
// * determines the search space
// * returns entry (if NULL, we failed somehow - probably too little host mem)
cuzmem_plan*
zeroth_lookup_handler (CUZMEM_CONTEXT ctx, size_t size)
{
    if (ctx->tune_iter == 0) {
        CUresult ret = CUDA_SUCCESS;
        cuzmem_plan* entry = ctx->plan;
        int loopy = detect_inloop (&entry, size);

        if (loopy) {
            ret = alloc_mem (entry, size);
            if (ret != CUDA_SUCCESS) {
                // Note, cudaMalloc() will report a NULL return value
                // from call_tuner(LOOKUP) as cudaErrorMemoryAllocation
                entry = NULL;
            }
        } else {
            entry = (cuzmem_plan*) malloc (sizeof(cuzmem_plan));
            entry->id = ctx->current_knob;
            entry->size = size;
            entry->loc = 1;
            entry->inloop = 0;
            entry->first_hit = 1;
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
            entry->next = ctx->plan;
            ctx->plan = entry;

            ctx->current_knob++;
        }

        return entry;
    }
}
