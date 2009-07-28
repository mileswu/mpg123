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
 * Register a function for cleanup when exiting mpg123, the functions are
 * excuted in a way that the first function passed to it will be run last and
 * last function passed to it will be run first.
 * @param[in] fp Pointer to a function  type int fp (void *);
 * @param[in] ptr Optional parameter, up to cleanup function to interpret it.
 * @return 0 if successfully registered.
 */
int register_dtor_cleanup (dtor_fp fp, void *ptr);

/**
 * exit_cleanup
 * Run registered cleanup routines. Should only be called by atexit().
 * @see register_dtor_cleanup
 */
void exit_cleanup (void);
#endif

