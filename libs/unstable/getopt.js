/**
 * Take an array of arguments (or this.arguments if nothing is passed)
 * and split it into options and arguments: --option=value arg...
 * @returns [options, arguments]
 */
function getopt(a) {
  if (a === undefined)
    a = this.arguments;

  let options = {};
  let args = [];
  let optionFinder = /^--([a-z_-]+)=(.*)$/i;

  for each (arg in a) {
    let m = optionFinder(arg);
    if (m)
      options[m[1]] = m[2];
    else
      args.push(arg);
  }

  return [options, args];
}
