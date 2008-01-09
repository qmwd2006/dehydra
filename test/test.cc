namespace boo {
  class book2 {
  public:
    book2(int i);
  };
  
  class book2;
  typedef book2 book;
  int foo (int param) {
    //int i = 9, x = 6;
    book b = foo();
    int i = foo(param);
    return param;
  }
}
