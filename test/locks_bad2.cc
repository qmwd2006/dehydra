#include "locks.h"

void bad2(int doBadStuff)
{
  mutex *m = new mutex;
  create_mutex(m);
  lock(m);

  if (doBadStuff) {
    switch (doBadStuff) {
    case 2:
      lock(m);
    }
  }
}

