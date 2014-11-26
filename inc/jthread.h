// JOS Threading
// Hugh O'Cinneide
// November 2014

#ifndef JOS_INC_JTHREAD_H
#define JOS_INC_JTHREAD_H

#include <inc/types.h>

typedef int32_t jthread_t;
typedef uint32_t jthread_attr_t;

enum {
  THREAD_NOT_RUNNABLE = 0,
  THREAD_RUNNABLE,
  THREAD_ZOMBIE,
  THREAD_DONE
};

int
jthread_create(jthread_t *thread,
               const jthread_attr_t *attr,
               void *(*start_routine)(void *),
               void *arg);

int
jthread_join(jthread_t th,
             void **thread_return);

void
jthread_exit(void *retval);

#endif // !JOS_INC_JTHREAD_H
