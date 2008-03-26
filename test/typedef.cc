typedef unsigned int PRUint32;
typedef int PRInt32;
typedef PRInt32 PRInt32_2;
typedef PRInt32* PRInt322;
typedef PRUint32 TypedefTypedef;
class s {
  typedef PRUint32 size_type;
  size_type foo;
  PRUint32 pru32;
  PRInt322 i;
  TypedefTypedef tt;
};

typedef long long __m64 __attribute__ ((__vector_size__ (8), __may_alias__));

static __inline __m64
_mm_setzero_si64 (void)
{
  return (__m64)0LL;
}


