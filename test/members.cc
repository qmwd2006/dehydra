class Outer {
  class Inner {
    int a;

    class MostInner {
      int b;
    };
  };

  int a;
  static int sa;
  void foo();
  static void sfoo();

  enum { DO_IT = 1 };
  int Outer::* ptrto_mem() {
    return &Outer::a;
  }
};
