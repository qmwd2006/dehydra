struct C {
  void f(int a, int b);
  static void g(C *o, int a);
};

C f() {
}

typedef void (*fp)(int a, int b);

fp g() {
}
