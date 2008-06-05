// { 'test': 'dehydra', 'input': 'members.cc', 'output': 'stdout_has("members.cc:10: Outer::a")' }
function process_type(t) {
  if (t.name != "Outer") return
  for each (var m in t.members) {
    print(m.name, m.loc)
  }
}
