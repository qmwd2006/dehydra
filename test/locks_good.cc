#include "locks.h"
void good1(int x) 
{
  mutex *m = new mutex;
  create_mutex(m);
  lock(m);
  unlock(m);

  if (!x) {
    switch (x) {
    case 3:
      unlock(m);
      break;
    default:
      break;
    }
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
