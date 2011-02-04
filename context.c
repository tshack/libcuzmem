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
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include "context.h"

//------------------------------------------------------------------------------
// CUZMEM CONTEXT STATE
//------------------------------------------------------------------------------
cuzmem_context* context[MAX_CONTEXTS] = { NULL };
pid_t context_lut[MAX_CONTEXTS] = { 0 };


cuzmem_context*
create_context ()
{
    int i=0;

    while (context[i] != NULL) {
        i++;
        if (i >=MAX_CONTEXTS) { return NULL; }
    }

    // create context
    context[i] = (cuzmem_context*)malloc(sizeof(cuzmem_context));
    context_lut[i] = getpid();

    // populate context with default values
    strcpy (context[i]->plan_name, "phantom_plan");
    strcpy (context[i]->project_name, "phantom_project");
    context[i]->current_knob = 0;
    context[i]->num_knobs = 0;
    context[i]->tune_iter = 0;
    context[i]->tune_iter_max = 0;
    context[i]->op_mode = CUZMEM_RUN;
    context[i]->plan = NULL;
    context[i]->start_time = 0;
    context[i]->best_time = ULONG_MAX;
    context[i]->best_plan = 0;
    context[i]->gpu_mem_percent = 90;
    context[i]->cuda_context = NULL;
    context[i]->call_tuner = NULL;

    return context[i];
}


// get context id for current thread
cuzmem_context*
get_context ()
{
    int i;
    pid_t pid = getpid();

    for (i=0; i<MAX_CONTEXTS; i++) {
        if (context_lut[i] == pid) {
            return context[i];
        }
    }

    // no context, so create one
    return create_context();
}


// destroy context for current thread
void
destroy_context ()
{
    int i;
    pid_t pid = getpid();

    for (i=0; i<MAX_CONTEXTS; i++) {
        if (context_lut[i] == pid) {
            free (context[i]);
        }
    }
}
