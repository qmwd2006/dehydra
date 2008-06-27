class Base
{
public:
  virtual void F() = 0;
};

template<class T>
class A : public Base
{
public:
  virtual void F();
};

template<class T>
void
A<T>::F()
{
  int i = 1 + 1;
}

Base* testfunc()
{
  return new A<int>();
}
