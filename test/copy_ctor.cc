struct t {
  t(const char* v) { 
  }

  t(const t& v){ 
    //  cerr << "copy2" << endl;
  }
  const char* data;
};

t f(t v) {}
t g(const t& v) {}

int main(int argc, char **) {
  t a("test");

  f(a);

  g(f(a));

  a = f(a);
}
