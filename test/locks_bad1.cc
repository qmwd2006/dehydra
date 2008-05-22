#include "locks.h"

void bad1(int doBadStuff)
{
  mutex *m = new mutex;
  create_mutex(m);
  lock(m);
  if (doBadStuff) {
    unlock(m);
  }
  unlock(m);
}
