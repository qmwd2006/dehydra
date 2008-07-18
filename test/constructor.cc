struct A
{
  A();
  void foo();
};

struct B : A
{
  void bar();
};

void Foo()
{
  B b;
  b.foo();
  //b.bar();
}
