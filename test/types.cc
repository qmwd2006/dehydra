union UnionNode;

typedef union UnionNode Union;
struct Boo;

void func(struct Boo*) {
}

struct Boo {
  unsigned int i:31;
} foo;

void func2(struct Boo*b) {
  b->i;
}

union UnionNode {
  struct Boo boo;
};

static Union global_namespace;
static int array[10];
class Foo;
volatile const Foo * c;
