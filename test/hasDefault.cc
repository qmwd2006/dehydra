class C
{
public:
  void
  a();

  void
  b(int i);

  void
  c(int i = 1);

  void
  d(int i, char f = 2, float g = 3);

  static void
  s_a();

  static void
  s_b(int i);

  static void
  s_c(int i = 1);

  static void
  s_d(int i, char f = 2, float g = 3);
};

void
f(int i) { }

void
g(int i = 1) { }

void
h(int i, char f = 2, float g = 3) { }

void
i(int i = 1, char f = 2, float g = 3) { }
