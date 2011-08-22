class Base {
public:
  virtual void b();
  virtual void a();
};

class Derived : public Base {
  double z;
 public:
  virtual void a();
  int e();
  int x;
  int y;
  virtual void b();
  virtual void c();
  virtual double d(int x);
};

class NotDerived {
 public:
  virtual void a(int x);
  virtual void b(double y);
  virtual int c();
};

typedef decltype(&Base::a) aPtrBase;
typedef decltype(&Base::b) bPtrBase;
typedef decltype(&Derived::a) aPtrDerived;

struct A
{
  aPtrBase ptr;
};

struct B {
  A array[2];
};

A a_arr_glob[] = { { reinterpret_cast<aPtrBase>(&Derived::a) }, { &Base::a } };
static B b_arr_glob[] = { { { &Base::a, reinterpret_cast<aPtrBase>(&Derived::a) } } };

aPtrBase a_ptr_glob = &Base::a;
aPtrBase b_ptr_glob = reinterpret_cast<aPtrBase>(&NotDerived::b);

void bar(aPtrBase);

void foo() {
  static A a_arr[] = { { reinterpret_cast<aPtrBase>(&Derived::a) }, { &Base::a } };
  A a_arr2[] = { { reinterpret_cast<aPtrBase>(&Derived::a) }, { &Base::a } };
  aPtrBase aptr = reinterpret_cast<aPtrBase>(&Derived::a);
  aPtrBase really_b_ptr = reinterpret_cast<aPtrBase>(&NotDerived::b);
  static B b_arr[] = { { { &Base::a, reinterpret_cast<aPtrBase>(&Derived::a) } } };
  B b_arr2[] = { { { reinterpret_cast<aPtrBase>(&Derived::a), &Base::a } } };
  static B b_arr3[] = { { { reinterpret_cast<aPtrBase>(&Derived::d), reinterpret_cast<aPtrBase>(&Derived::c) } } };
  aPtrDerived derived = reinterpret_cast<aPtrBase>(&NotDerived::b);

  // In GENERIC but not GIMPLE, this call arg is of type PTRMEM_CST.
  bar(reinterpret_cast<aPtrBase>(&NotDerived::c));
}
