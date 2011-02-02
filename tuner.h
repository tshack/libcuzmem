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

#ifndef _tuner_h_
#define _tuner_h_

#include <stdio.h>
#include <cuda.h>
#include "plans.h"

// import necessary symbols from libcuzmem.o

#if defined __cplusplus
extern "C" {
#endif

CUresult
alloc_mem (cuzmem_plan* entry, size_t size);

#if defined __cplusplus
}
#endif

extern char plan_name[];
extern char project_name[];
extern unsigned int tune_iter;
extern unsigned int num_knobs;
extern unsigned int current_knob;
extern unsigned int tune_iter_max;
extern enum cuzmem_op_mode op_mode;
extern cuzmem_plan *plan;

#endif
