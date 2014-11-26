#include <inc/lib.h>
#include <inc/jthread.h>

void *
countTo100(int c)
{
  cprintf("In countTo100\n");
  if (c == 100)
    return (void *)1;
  return (void *)countTo100(c++);
}

void
umain(int argc, char *argv[])
{
  // cprintf("In recursion.c\n");
  // countTo100(0);
  // cprintf("We counted to 100!\n");
  jthread_t tid;
  cprintf("jthread: %d\n", jthread_create(&tid, NULL, (void *(*)(void *))countTo100, 0));
  while(true)
    sys_yield();
}
