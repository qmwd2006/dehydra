template <int T>
union foo
{
    enum {value = T};
};
int main() {return foo<42>::value;}
