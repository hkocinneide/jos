#include <inc/lib.h>
#include <inc/jthread.h>

void *
countTo100(int c)
{
  cprintf("In countTo100\n");
  return (void *)0xdeadbeef;
}

void
umain(int argc, char *argv[])
{
  // cprintf("In recursion.c\n");
  // countTo100(0);
  // cprintf("We counted to 100!\n");
  jthread_t tid;
  cprintf("jthread: %d\n", jthread_create(&tid, NULL, (void *(*)(void *))countTo100, 0));
  cprintf("jthread: tid: %d\n", tid);
  void *retval;
  jthread_join(tid, &retval);
  cprintf("jthread: Got return value! retval: %08x\n", (uintptr_t)retval);
}
