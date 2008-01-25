#include <jsapi.h>
#include <jsobj.h>
#include <unistd.h>
#include <stdio.h>

#include <config.h>
#include <system.h>
#include <coretypes.h>
#include <tm.h>
#include <tree.h>
#include <cp-tree.h>
#include <cxx-pretty-print.h>
#include <tree-iterator.h>
#include <pointer-set.h>
#include <toplev.h>

#include "xassert.h"
#include "dehydra.h"
#include "dehydra_ast.h"
#include "dehydra_callbacks.h"
#include "dehydra_tree.h"

static Dehydra dehydra = {0};

void visitClass (tree c) {
  dehydra_visitClass (&dehydra, c);
}

void visitFunction (tree f) {
  dehydra_visitFunction (&dehydra, f);
}

int initDehydra (const char *file, const char *script)  {
  return dehydra_init (&dehydra, file,  script);
}

void cp_pre_genericizeDehydra (tree fndecl) {
  dehydra_cp_pre_genericize(&dehydra, fndecl);
}

void input_endDehydra () {
  dehydra_input_end (&dehydra);
}

void plugin_passDehydra () {
  dehydra_plugin_pass (&dehydra);
}
