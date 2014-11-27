#include <inc/lib.h>
#include <inc/jthread.h>

typedef struct {
  int data;
  jthread_mutex_t lock;
} resource;

resource r;

void waitncycles(int n)
{
  int i = 0;
  while (i != n)
  {
    i++;
    sys_yield();
  }
}

void *
acquire_and_hold(void *arg)
{
  cprintf("acquire_and_hold: lock memory location: %08x\n", (uint32_t)&r.lock.locked);
  cprintf("acquire_and_hold: getting resource\n");
  jthread_mutex_lock(&r.lock);
  cprintf("acquire_and_hold: r.lock.locked: %s\n", r.lock.locked ? "true" : "false");
  cprintf("acquire_and_hold: r.locked.owner: %x\n", r.lock.owner);
  waitncycles(100);
  cprintf("acquire_and_hold: releasing resource\n");
  jthread_mutex_unlock(&r.lock);
  return NULL;
}

void
umain(int argc, char *argv[])
{
  jthread_t tid;
  cprintf("umain: creating thread\n");
  jthread_create(&tid, 0, acquire_and_hold, NULL);
  waitncycles(3);
  cprintf("umain: trying to acquire resource\n");
  jthread_mutex_lock(&r.lock);
  cprintf("umain: acquired resource!\n");
  void *retval;
  jthread_join(tid, &retval);
  return;
}
