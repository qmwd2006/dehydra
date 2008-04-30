const isGCCApple = (sys.gcc_version.indexOf('Apple') != -1);

//Note sure if this is the best place to keep this
function EnumValue (name, value) {
  this.name = name
  this.value = value
}
