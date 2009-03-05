#include "locks.h"

void bad3(int doBadStuff)
{
  mutex *m = new mutex;
  create_mutex(m);
  lock(m);

  doBadStuff = 3;
  if (doBadStuff == 0) {
    unlock(m);
  } else {
    lock(m);
  }
}


