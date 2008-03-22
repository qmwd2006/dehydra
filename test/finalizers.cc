namespace MMgc {
  class GCFinalizable {
  };
};

struct nsFoo
{
  ~nsFoo();
};

class nsBar : public MMgc::GCFinalizable
{
  void v();
  void __attribute__((user("finalizer_safe"))) safe_func() {
    v();
  }
public:
  ~nsBar() {
    safe_func();
  }

  nsFoo foo;
};

void v () {
  nsBar bar;
}
