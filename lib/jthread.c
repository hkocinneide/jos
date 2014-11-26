// JOS Threading
// Hugh O'Cinneide
// November 2014

#include <inc/lib.h>
#include <inc/jthread.h>

void *
jthread_main(void *(*start_routine)(void *), void *arg)
{
  cprintf("In jthread_main\n");
  void *ret = start_routine(arg);
  jthread_exit(ret);
  // Should not reach here
  return NULL;
}

int
jthread_create(jthread_t *thread,
               const jthread_attr_t *attr,
               void *(*start_routine)(void *),
               void *arg)
{
  if (start_routine == NULL || thread == NULL)
    return -1;

  jthread_t tid;
  if ((tid = (jthread_t)sys_exofork()) < 0)
    return tid;

  if (sys_kthread_create(tid, (void *)jthread_main, (void *)start_routine, arg) < 0)
    return -1;
  
  *thread = tid;

  return 0;
}

int
jthread_join(jthread_t th, void **thread_return)
{
  cprintf("In jthread_join\n");
  void *ret = 0;
  while ((int)(ret = (void *)sys_kthread_join(th, thread_return)) < 0)
  {
    sys_yield();
  }
  cprintf("Our of jthread_join\n");
  return 0;
}

void
jthread_exit(void *retval)
{
  sys_kthread_exit(retval);
}
