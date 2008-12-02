int plain_var;

typedef int some_typedef;

template<typename T, typename X>
class FooTemplate {
};


template <class T>
class FooTemplate<T, int> {
};

template <>
class FooTemplate<char, char> {
};

void forward_func_decl();
