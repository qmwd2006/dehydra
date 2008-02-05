class Klass {
public:
  Klass (const char *);
};

void foo() {
  new Klass("ARGUMENT");
}
