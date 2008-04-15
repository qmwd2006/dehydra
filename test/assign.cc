class book {
public:
  book(int i);
  int x;
};

void foo () {
  book b(0);
  b = book(2);
  int i;
  i = b.x;
}

