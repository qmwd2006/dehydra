#include "gcc_cp_headers.h"
/*js*/
#include <jsapi.h>
#include "xassert.h"
#include "dehydra.h"
#include "dehydra_tree.h"

static Dehydra dehydra = {0};

int gcc_plugin_init(const char *file, const char* arg) {
  if (!arg) {
    error ("Use -fplugin-arg=<scriptname> to specify the dehydra script to run");
    return 1;
  }
  return dehydra_init (&dehydra, file,  arg);
}

void gcc_plugin_pass (void) {
  dehydra_plugin_pass (&dehydra);
}
