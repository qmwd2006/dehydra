// { 'test': 'dehydra', 'input': 'templ-func3.cc', 'output': 'unit_test', 'lang': 'c++' }

function process_function(f)
{
  if (f.name == "TemplateFunction(A) [with A = int]")
    print('OK');
  else
    print(f.name)
}
