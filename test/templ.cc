template <class T>
struct templ
{
  int i;
};
class Foo {
};
int test (templ<Foo> *f) {
  return f->i;
}
