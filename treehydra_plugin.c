#include "gcc_cp_headers.h"
/*js*/
#include <jsapi.h>
#include "xassert.h"
#include "dehydra.h"
#include "treehydra.h"

static Dehydra dehydra = {0};

int gcc_plugin_init(const char *file, const char* arg, char **pass) {
  *pass = "useless";
  if (!arg) {
    error ("Use -fplugin-arg=<scriptname> to specify the dehydra script to run");
    return 1;
  }
  return dehydra_init (&dehydra, file,  arg);
}

void gcc_plugin_pass (void) {
  treehydra_plugin_pass (&dehydra);
}
