// Code common to Treehydra and Dehydra.

function Location(_source_location, file, line, col) {
  this._source_location = _source_location;
  this.file = file;
  this.line = line;
  this.col = col;
}

Location.prototype.toString = function() {
  return this.file + ':' + this.line + 
    (this.col != undefined ? ':' + this.col : '');
}

/** Report an error diagnostic in GCC. loc is optional. */
function error(msg, loc) {
  diagnostic(true, msg, loc);
}
  
/** Report a warning diagnostic in GCC. loc is optional. */
function warning(msg, loc) {
  diagnostic(false, msg, loc);
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
