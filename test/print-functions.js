function cleanup (v) {
  delete v.type
  delete v.loc
  if (v.fieldOf)
    cleanup (v.fieldOf)
  if (v.arguments)
    v.arguments = Array.map (v.arguments, cleanup)
  return v
}

function processVar(v) {
  cleanup(v)
  print(v);
}

function iter_over_inits(vars)
{
  var v, va;

  for each(v in vars)
  {
    if (v.isFcall)
      processVar(v);
    iter_over_inits(v.assign);
    iter_over_inits(v.arguments);
    for each (va in v.inits) {
      iter_over_inits(v.assign);
      iter_over_inits(v.arguments);
    }
  }
}
      
/** called by dehydra on every "statement" */
function process(vars, state) {
  iter_over_inits(vars);
}
