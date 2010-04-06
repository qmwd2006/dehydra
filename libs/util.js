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

// Generic Dehydra utilities

/**
 * Dump a Dehydra object in a readable tree format.
 *  @param o        object to dump
 *  @param indent   number of spaces to indent by
 *  @param depth    max nesting depth to dump
 *
 * Printing Dehydra objects tends to produce a big messy JSON-like
 * notation that's hard to read. This flattens things out and doesn't
 * go too deep, in order to reduce the amount of stuff to look at.
 */
function do_dehydra_dump(o, indent, depth) {
  if (depth == 0) return;

  try {
    for (var k in o) {
      var v = o[k];
      if (typeof v == "string") {
        print_indented(k + ": '" + v + "'", indent);
      } else if (typeof v == "number") {
        print_indented(k + ": " + v, indent);
      } else if (typeof v == "boolean") {
        print_indented(k + ": " + v, indent);
      } else if (typeof v == "undefined") {
        print_indented(k + ": undefined", indent);
      } else {
        print_indented(k + ": " + 'object', indent);
        //print_indented(k + ": " + v.constructor.name, indent);
        do_dehydra_dump(v, indent + 1, depth - 1);
      }
    }
  } catch (e) {
    print_indented(e, indent);
  }
}

/**
 * Print string 's' indented by 'indent' spaces.
 */
function print_indented(s, indent) {
  for (var i = 0; i < indent; ++i) {
    s = "  " + s;
  }
  print(s);
}

/** Pad the given string on the right with spaces to a total
 *  length of 'pad'. */
function rpad(s, pad) {
  let n = pad - s.length;
  for (let i = 0; i < n; ++i) {
    s += ' ';
  }
  return s;
}

/**
 * Simpler API for dehydra dumping. @see do_dehydra_dump.
 */
function dehydra_dump(o) {
  print(typeof o);
  do_dehydra_dump(o, 1, 3);
}

// Need to have treehydra.h included for this to work

/** Search a dehydra/treehydra JS object graph in BFS order. This is
 *  useful to find the property access path to a given value that
 *  you know is present, but is deeply buried. If search is successful,
 *  the path the destination is printed to stdout.
 *
 *  @param start  object to start searching from. 
 *  @param test   function of one argument--the search is for an
 *                object for which this returns true.
 *  @param count  maximum number of objects to search. */
function dehydra_bfs(start, test, count) {
  if (!count) count = 100;
  let nodeMap = new Map();
  let pdStart = { node: null, pred: null, field: null, dist: -1 };
  let work = [ [ pdStart, null, start ] ];
  while (work.length) {
    let [ pd, field, o ] = work.shift();
    let d = nodeMap.get(o);
    if (d == undefined) {
      d = { node: o, pred: pd, field: field, dist: pd.dist + 1 };
      nodeMap.put(o, d);
      if (test(o)) {
        shortest_path_dump(d);
        if (!--count)
          return;
      }
      
      try {
        for (var k in o) {
          var v = o[k];
          work.push([d, k, v]);
        }
      } catch (e) {
        print(e);
      }
    }
  }
  print("not found");
}

/** Helper for dehydra_bfs. */
function shortest_path_dump(d) {
  let code = '';
  let path = [];

  while (d.field) {
    path.unshift(d);
    code = '.' + d.field + code;
    d = d.pred;
  }
  
  //print(code);

  code = '';
  for each (let d in path) {
    code += '.' + d.field;
    print(code);
    print('  ' + dehydra_repr_short(d.node));
  }
}

/** Return a short string representation of a dehydra/treehydra object,
 *  giving just what kind of object it is. */
function dehydra_repr_short(o) {
  if (typeof o == 'string') return '"' + o + '"';

  try {
    return o._struct_name;
  } catch (e) {
  }
  
  return 'tree ' + o.base.code.name;
}
