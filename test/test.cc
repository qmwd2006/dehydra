 int id;
namespace boo {
  class book2 {
  public:
    book2(int i);
  };
  
  class book2;
  typedef book2 book;
  int foo () {
    static int fook=0;
    id-=fook++;
    int i = 9, x = 6;
    book b(foo());
    return 0;
  }
}
