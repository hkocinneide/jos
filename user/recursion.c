#include <inc/lib.h>
#include <inc/jthread.h>

void *
countTo100(int c)
{
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
  cprintf("jthread: %d\n", jthread_create(NULL, NULL, NULL, NULL));
}
