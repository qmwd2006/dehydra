struct foo {
  int i:30;
  unsigned char c:2;
};

int f (foo *fo) {
  return fo->c;
}
