class Class;

template<typename A> int TemplateFunction(A a) {
 Class* k;
 k = 0;
 return a == A() ? 1 : 0;
}

void foo(void)
{
  TemplateFunction(1);
}
