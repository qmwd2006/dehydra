#include "locks.h"

class C {
public:
  ~C() {}
};

void good(int x) 
{
  mutex *m = new mutex;
  create_mutex(m);
  if (x) lock(m);
  C c;
  if (!x) return;
  unlock(m);
}

