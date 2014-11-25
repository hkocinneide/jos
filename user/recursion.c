#include <inc/lib.h>

int
countTo100(int c)
{
  if (c == 100)
    return 1;
  return countTo100(c++);
}

void
umain(int argc, char *argv[])
{
  cprintf("In recursion.c\n");
  countTo100(0);
  cprintf("We counted to 100!\n");
}
