#include "locks.h"
void good1() 
{
  mutex *m = new mutex;
  create_mutex(m);
  lock(m);
  unlock(m);

  if (0) {
    unlock(m);
  }
}


void foo();
void bar();
void good2(int doLock) 
{
  mutex *m = new mutex;
  if (doLock) {
    create_mutex(m);
    lock(m);
  }

  foo();
  bar();

  if (doLock) {
    unlock(m);
  }
}
