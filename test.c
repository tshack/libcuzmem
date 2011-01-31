#include <stdio.h>
#include <string.h>
#include "libcuzmem.h"
#include "plans.h"

int
main (void)
{
    char* buffer;
    cuzmem_plan *plan = NULL;
    cuzmem_plan *curr = NULL;

    plan = read_plan ("plastimatch", "foobar");
    curr = plan;

    write_plan (plan, "plastimatch", "foobaz");

#if defined (commentout)
    while (curr != NULL) {
        printf ("begin\n");
        printf ("  id %i\n", curr->id);
        printf ("  size %i\n", curr->size);
        printf ("  loc %i\n", curr->loc);

        curr = curr->next;
    }
#endif

    return(0);
}
