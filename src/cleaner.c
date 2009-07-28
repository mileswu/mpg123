#include "config.h"
#include "cleaner.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>

static struct sCleanup *list, *current, *prev;
static void add_dtor_cleanup (dtor_fp fp, void *ptr);

int
register_dtor_cleanup (dtor_fp fp, void *ptr)
{
  struct sCleanup *temp;
  if (!list) /* Init list */
  {
    list = (struct sCleanup *)calloc (sizeof (struct sCleanup), 1); /* New context */
    if (!list) return 1; /* Failed to allocate list.*/
    current = list; /* Put to current context. */
  }
  else
  {
    temp = (struct sCleanup *)calloc (sizeof (struct sCleanup), 1);
    if (!temp) return 1; /* Failed to allocate list. */
    current -> next = temp; /* Current context points to next. */
    prev = current; /* Save pointer to current context. */
    current = temp; /* Shift context */
  }
  add_dtor_cleanup(fp, ptr); /* Add destructor */
  return 0;
}

void
add_dtor_cleanup (dtor_fp fp, void *ptr)
{
  current -> fp = fp;
  current -> ptr = ptr;
  current -> next = NULL;
  current -> prev = prev;
}

void exit_cleanup (void)
{
  debug ("Running cleanup!");
  debug1 ("Entering context = %p\n", &(current));
  while (current)
  {
    debug1 ("current -> prev = %p\n", &(current -> prev));
    prev = current -> prev; /* Get previous context. */
    debug2 ("Running current = %p with ptr at %p\n", &(current), current->ptr);
    current->fp(current->ptr); /* Run destructor */
    debug ("Freeing Current\n");
    free (current); /* Free list entry */
    current = prev; /* Put saved context as current */
  }
}

