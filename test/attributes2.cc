#pragma GCC visibility push(hidden)

template<class T>
class __attribute__((user("default"))) A
{
public:
  T  __attribute__((user(0)))  i;
};
class Klass {
};

void i() {
  A<Klass> foo;
}
