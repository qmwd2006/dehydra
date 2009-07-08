class C
{
public:
  C() {  }
  
  C(const C&) {  }

  ~C() { }
};

const C& bar(const C& c)
{
  return c;
}

void foo()
{
  bar(C(bar(C(bar(C())))));
}
