// test pointer-to-member-fn and pointer-to-member
struct s1 {
  void m1(int);
  int i;
};

typedef int t1;
typedef bool t2;
typedef long double t3;
typedef const long int t4;
typedef s1 t5;
typedef const s1* const & t6;
typedef const int __attribute__ ((__vector_size__ (8))) t7;
typedef long double __complex__ const t8;
typedef volatile unsigned short* const * t9[];
typedef t9* const & t10;
typedef void (s1::*t11)(int);
typedef t11 (s1::*t12)(int);
typedef int s1::*t13;

