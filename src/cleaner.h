#ifndef _CLEANER_H_
#define _CLEANER_H_

typedef int (*dtor_fp)(void *);

struct sCleanup
{
  dtor_fp fp;			/*< Function pointer */
  void *ptr;			/*< Optional parameter */
  struct sCleanup *next;	/*< Linked list pointer */
  struct sCleanup *prev;	/*< Linked list pointer */
};

/**
 * register_dtor_cleanup
 *
 */
int register_dtor_cleanup (dtor_fp fp, void *ptr);
void exit_cleanup (void);
#endif

