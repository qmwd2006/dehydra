// { 'test': 'treehydra', 'input': 'templ.cc', 'output': 'unit_test', 'lang': 'c++' }

// test lang_type union stuff

require({werror: true });
include('unit_test.js');

const assertEquals = TestCase.prototype.assertEquals

function process_cp_pre_genericize(fndecl) {
  var counter = 0;
  function checker (t, depth) {
    if (t.tree_code() != PARM_DECL) return
    // code loosely based on dehydra_attachTemplateStuff
    const record_type = TREE_TYPE(TREE_TYPE(t))
    var tpl = CLASSTYPE_TI_TEMPLATE(record_type)
    while (DECL_TEMPLATE_INFO (tpl))
      tpl = DECL_TI_TEMPLATE (tpl)
    const name = DECL_NAME (tpl)
    assertEquals (IDENTIFIER_POINTER (name), "templ")
    const info = TYPE_TEMPLATE_INFO (record_type);
    const args = TI_ARGS (info)
    if (TMPL_ARGS_HAVE_MULTIPLE_LEVELS (args))
      args = TREE_VEC_ELT (args, TREE_VEC_LENGTH (args) - 1);

    assertEquals(TREE_VEC_LENGTH (args), 1)
    const arg = TREE_VEC_ELT (args, 0)
    assertEquals (decl_name(TYPE_NAME( arg)), "Foo")
    print("OK")
  }
  walk_tree (DECL_SAVED_TREE (fndecl), checker, new Map())
}
