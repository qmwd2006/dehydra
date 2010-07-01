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

/**
 * Take an array of arguments (or this.arguments if nothing is passed)
 * and split it into options and arguments: --option=value arg...
 * @returns [options, arguments]
 */
function getopt(o) {
  if (o === undefined)
    o = this.options.split(/ +/);

  let opts = {};
  let args = [];
  let optionFinder = /^--([a-z_-]+)=(.*)$/i;

  for each (arg in o) {
    let m = optionFinder(arg);
    if (m)
      opts[m[1]] = m[2];
    else
      args.push(arg);
  }

  return [opts, args];
}
