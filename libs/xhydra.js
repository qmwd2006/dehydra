// Code common to Treehydra and Dehydra.
// Location is initialized from C and doesn't actually run
function Location(_source_location, file, line, col) {
}

Location.prototype.toString = function() {
  with (this) {
    return file + ':' + line + 
      (this.column != undefined ? ':' + column : '');
  }
}

/** Report an error diagnostic in GCC. loc is optional. */
function error(msg, loc) {
  diagnostic(true, msg, loc ? loc : this._loc);
}
  
/** Report a warning diagnostic in GCC. loc is optional. */
function warning(msg, loc) {
  diagnostic(false, msg, loc ? loc : this._loc);
}

/** EnumValue kept here so that convert_tree.js sees it. */
function EnumValue (name, value) {
  this.name = name
  this.value = value
}

function print (msg, loc) {
  // this._loc is used by process in dehydra
  // scripts can set it to customize error reporting
  if (!loc)
    loc = this._loc
  if (loc)
    _print (loc + ": " + msg);
  else
    _print (msg);
}
