/*
 * Dehydra and Treehydra scriptable static analysis tools
 * Copyright (C) 2007-2010 The Mozilla Foundation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
