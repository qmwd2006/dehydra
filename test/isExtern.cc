struct A
{
  int a;
  static int b;
  static int c;
  void f();
  void g();
};

int A::c;
void A::g()
{
}

