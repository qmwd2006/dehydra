require({ after_gcc_pass: 'cfg'});

include('treehydra.js');
include('unstable/analysis.js');
include('gcc_util.js');
include('util.js');
include('gcc_print.js');

function iterate_decls(cfg) {
  for (let bb in cfg_bb_iterator(cfg)) {
    for (let isn in bb_isn_iterator(bb)) {
      for (let decl in isn_uses(isn)) {
        if (DECL_P(decl))
            yield decl;
      }
      for (let decl in isn_defs(isn)) {
        if (DECL_P(decl))
            yield decl;
      }
    }
  }

}
results = {}

function process_tree(fndecl) {
  let cfg = function_decl_cfg(fndecl);
//  cfg_dump(cfg);
  for (let decl in iterate_decls(cfg)) {
    let context = DECL_CONTEXT(decl);
    if (!DECL_NAME(decl))
      continue;
    if (context)
      switch (context.tree_code()) {
      case FUNCTION_DECL:
      case RECORD_TYPE:
        // static class and function vars are special
        if (TREE_STATIC(decl))
          break;
        continue;
      }

    results[IDENTIFIER_POINTER(DECL_ASSEMBLER_NAME(fndecl)) + " uses "
            + IDENTIFIER_POINTER(DECL_ASSEMBLER_NAME(decl))
           + " " + decl_name(decl)] = 1;
  }

  //isn_uses;
}

function input_end() {
  for (let v in results)
    print(v)
}

