struct A
{
  A();
  A(const A &other);
  ~A();
};

void FunctionTakesA(A a);

void TestFunc(A &a)
{
  // this should create a stack temporary A and invoke the copy-constructor
  // and the destructor.
  FunctionTakesA(a);
}
