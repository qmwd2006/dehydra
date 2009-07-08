// { 'test': 'dehydra', 'input': 'iterate_vars.cc', 'output': 'unit_test' }

let i = 0;

function process_function(decl, statements)
{
  if (decl.name != "foo()") return;
  for each (let v in iterate_vars(statements))
    i++;
}

function input_end() {
  if (i != 6)
    throw new Error("iterate_vars is busted!");
  else
    _print("OK");
}
