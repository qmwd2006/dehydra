namespace boo {
  class book2 {
  public:
    book2(int i);
  };

  class book2;
  typedef book2 book;
  int foo () {
    //int i = 9, x = 6;
    book b = foo();
    return 0;
  }
}
/*class foo;
class boo {
public:
  boo(int i){}
};
class base {
};
struct foo:public base{
  foo() :b(1){
    
  }
  boo b;
  int i;
  
  boo *asa;
};

int f(struct foo*x) {
  if (x->i)
    x->i++;
  return x->i;
}
*/
