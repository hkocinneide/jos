// JOS Threading
// Hugh O'Cinneide
// November 2014

#include <inc/lib.h>
#include <inc/jthread.h>

int
jthread_create(jthread_t *thread,
               const jthread_attr_t *attr,
               void *(*start_routine)(void *),
               void *arg)
{
  return 1;
}
