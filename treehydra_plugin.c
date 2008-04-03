#include "gcc_cp_headers.h"
/*js*/
#include <jsapi.h>
#include "xassert.h"
#include "dehydra.h"
#include "treehydra.h"

static Dehydra dehydra = {0};

/* True if initialization has been completed. */
static int init_finished = 0;

/* Plugin pass position. This is kept here so that JS can set it. */
static char *after_gcc_pass = 0;

/* API function to set the plugin pass position. This should be called
 * only before/during plugin initialization. 
 * Return nonzero on failure. */
int set_after_gcc_pass(const char *pass) {
  if (init_finished) return 1;
  if (after_gcc_pass) free(after_gcc_pass);
  after_gcc_pass = xstrdup(pass);
  return 0;
}

int gcc_plugin_init(const char *file, const char* arg, char **pass) {
  if (!arg) {
    error ("Use -fplugin-arg=<scriptname> to specify the treehydra script to run");
    return 1;
  }
  dehydra_init (&dehydra, file);
  int rv = treehydra_startup (&dehydra, arg);

  init_finished = 1;
  if (after_gcc_pass)
    *pass = after_gcc_pass;
  return rv;
}

void gcc_plugin_pass (void) {
  // TODO  This is rather painful, but we really don't have any other
  //       place to put it.
  if (after_gcc_pass) {
    free(after_gcc_pass);
    after_gcc_pass = 0;
  }

  treehydra_plugin_pass (&dehydra);
}
