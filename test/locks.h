struct mutex {};

void lock(mutex *m);
void unlock(mutex *m);
void create_mutex(mutex *m);
