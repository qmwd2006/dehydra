class C
{
public:
  C();

  C(int);

  explicit
  C(char);

  C(double, int i = 5);

  explicit
  C(double, char c = 5);

  C(int, char);

  explicit
  C(int, float);
};
