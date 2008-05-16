struct mutex {};

void lock(mutex *m);
void unlock(mutex *m);
void create_mutex(mutex *m);

void foo();
void bar();

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
