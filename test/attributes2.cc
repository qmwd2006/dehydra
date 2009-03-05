#pragma GCC visibility push(hidden)

template<class T>
class __attribute__((user("default"))) A
{
public:
  T  __attribute__((user(0)))  i;
};
struct Klass {
};

struct Klass * __attribute__((user("value")))  varAttrOnType;

A<Klass> foo;

typedef void MySpecialVoidType __attribute__((user("NS_ImSoSpecial")));
MySpecialVoidType *s;
