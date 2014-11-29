#include <inc/lib.h>
#include <inc/jthread.h>

int
deadball()
{
  cprintf("[%08x] Second thread\n", thisenv->env_id);
  while(1)
    sys_yield();
  return 0xdeadba11;
}

void *
deadbeef(int c)
{
  jthread_t tid;
  void *retval;
  jthread_create(&tid, NULL, (void *(*)(void *))deadball, NULL);
  // jthread_join(tid, &retval);
  // cprintf("[%08x] Return value: %08x\n", thisenv->env_id, (uintptr_t)retval);
  return (void *)0xdeadbeef;//retval;
}

void
umain(int argc, char *argv[])
{
  jthread_t tid1, tid2;
  cprintf("[%08x] jthread: %d\n", thisenv->env_id, jthread_create(&tid1, NULL, (void *(*)(void *))deadbeef, 0));
  cprintf("[%08x] jthread: tid: %d\n", thisenv->env_id, tid1);
  void *retval;
  int i = 0;
  while(true)
  {
    if (i++ == 100)
      break;
    sys_yield();
  }
  // jthread_join(tid1, &retval);
  // cprintf("[%08x] jthread: Got return value! retval: %08x\n", thisenv->env_id, (uintptr_t)retval);
  cprintf("Main thread exiting\n");
}
