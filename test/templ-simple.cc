class Klass {
};
template<int Count, class C>
class AutoFoo { };

AutoFoo<40, AutoFoo<60, Klass> > c;
