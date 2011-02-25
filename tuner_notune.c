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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "context.h"
#include "plans.h"
#include "tuner_util.h"
#include "tuner_notune.h"


//#define DEBUG

// -- State Macros -----------------------
#define SAVE_STATE(state_ptr)            \
    (ctx->tuner_state = (void*)state_ptr) 

#define RESTORE_STATE(state_ptr)         \
    (state_ptr = ctx->tuner_state)        
// ---------------------------------------


//------------------------------------------------------------------------------
// "NO TUNE" TUNER
//------------------------------------------------------------------------------
cuzmem_plan*
cuzmem_tuner_notune (enum cuzmem_tuner_action action, void* parm)
{
    CUZMEM_CONTEXT ctx = get_context();

    // =========================================================================
    //  TUNER START
    // =========================================================================
    if (CUZMEM_TUNER_START == action) {

        // this tuner only has the 0th iteration... it is for people
        // who just want the thing to run without any optimization
        // but still spill memory allocations into pinned CPU memory.

        // start timing the iteration
        ctx->start_time = get_time ();

        // Return value currently has no meaning
        return NULL;
    }
    // =========================================================================
    //  TUNER LOOKUP
    // =========================================================================
    else if (CUZMEM_TUNER_LOOKUP == action) {
        // parm: pointer to size of allocation
        size_t size = *(size_t*)(parm);

        // default 0th tuning iteration handling
        return zeroth_lookup_handler (ctx, size);
    }
    // =========================================================================
    //  TUNER END
    // =========================================================================
    else if (CUZMEM_TUNER_END == action) {

        return NULL;

    }
}

