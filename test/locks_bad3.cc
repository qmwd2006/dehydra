#include "locks.h"

void bad3(int doBadStuff)
{
  mutex *m = new mutex;
  create_mutex(m);
  lock(m);

  while (doBadStuff--) {
    unlock(m);
  }
}


