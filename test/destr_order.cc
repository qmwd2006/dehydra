class A {
public:  A(int& x) : mValue(x) {}
  ~A() { mValue--; }
  operator char**() { return 0; }
private:  int& mValue;
};
void func(char **arg) {}
int m=2;
void test() {
  func(A(m));
  if (m==1) m = 0;
}
int main() {
  test();
  return(m);
}
