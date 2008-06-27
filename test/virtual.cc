class Base {
public:
  virtual int foo() = 0;
};

class Der:public Base {
public:
  int foo();
};

void f (Base *d) {
  int i = d->foo();
}
