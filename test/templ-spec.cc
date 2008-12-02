template<typename T, int e>
class Array {
  T a[e];
};

template<typename E>
class Array<E, 0> {
  E *a;
};

template<>
class Array<int, 0> {
};

class B {
};

Array<int, 5> p;
