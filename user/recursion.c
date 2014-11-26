#include <inc/lib.h>
#include <inc/jthread.h>

void *
deadbeef(int c)
{
  return (void *)0xdeadbeef;
}

int
deadball()
{
  return 0xdeadba11;
}

void
umain(int argc, char *argv[])
{
  // cprintf("In recursion.c\n");
  // deadbeef(0);
  // cprintf("We counted to 100!\n");
  jthread_t tid1, tid2;
  cprintf("jthread: %d\n", jthread_create(&tid1, NULL, (void *(*)(void *))deadbeef, 0));
  cprintf("jthread: tid: %d\n", tid1);
  cprintf("jthread: %d\n", jthread_create(&tid2, NULL, (void *(*)(void *))deadball, NULL));
  void *retval;
  jthread_join(tid1, &retval);
  cprintf("jthread: Got return value! retval: %08x\n", (uintptr_t)retval);
}
