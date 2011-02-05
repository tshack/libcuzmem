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
