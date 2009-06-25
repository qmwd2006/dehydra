class A
{
public:
  A();

protected:
  void f();

private:
  ~A();
  static int static_member;
};

class B : public A
{
private:
  using A::f;
  void f(int);
};

class C : private A
{
  int i;
};

class D : A
{
  int i;
};

A* getA()
{
  return 0;
}

B* getB()
{
  return 0;
}

C *getC()
{
  return 0;
}

D *getD()
{
  return 0;
}
