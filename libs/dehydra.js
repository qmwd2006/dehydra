include ("xhydra.js")

this.__defineGetter__("arguments", function() { return this._arguments.split(/ +/); });

Object.prototype.toString = function () {return uneval(this)}
/*
A clone function is required to clone
state on block transitions
if this one does not work for you,
provide your own in your script
*/
function clone(myObj)
{
  if(typeof(myObj) != 'object') return myObj;
  if(myObj.constructor == Array) {
    return myObj.map(clone)
  }
  var myNewObj = new myObj.constructor();

  for(var i in myObj)
    myNewObj[i] = clone(myObj[i]);

  return myNewObj;
}

function structurally_equal(o1, o2)
{
  const t1 = typeof o1
  if(t1 != typeof o2) {
    return false
  }
  if(t1 != 'object') return o1 === o2;

  if(o1.constructor !== o2.constructor) return false;
  if(o1.constructor === Array && o1.length !== o2.length) {
    return false
  }
  // accumulate a union of properties
  var fields = new Array()
  // the following is done twice in case one is a superset of the other
  for(var i in o1)
    fields[i] = 0

  for(var i in o2)
    fields[i] = 0

  for(var i in fields)
    if(!structurally_equal(o1[i], o2[i]))
      return false

  return true
}

/* Uno-style programming support */
/*

ALIAS address taken &symbol
ARRAY_DECL appears as basename in an array declaration struct X symbol[8]
DECL symbol appears as scalar in a declaration int symbol
DEREF dereferenced *symbol
FCALL used as name of function symbol(x)
PARAM formal parameter of a function function(int symbol) { ... }
REF0 dereferenced to access structure symbol->x
REF1 used as structure name symbol.x
REF2 used as structure element x->symbol, x.symbol
USE evaluated to derive value x = symbol
USEafterdef both DEF and USE, but USE occurs after DEF if ((symbol = x) > 0)
USEbeforedef both DEF and USE, but USE occurs before DEF symbol++ _ ________________________________________________________________________________________
*/
// The following are supported
const FCALL = "isFcall", USE = "isUse", ALIAS = "isAlias" ,
    DECL = "isDecl" , DEREF = "isDeref" , PARAM = "isArg", RETURN = "isReturn"

// should change the following to be regexps
// this allows to match by types
function TYPE(x) {
  return "type=" + x
}

function PARAM_OF(x) {
  return "paramOf=" + x
}

function varField_matches (field, value,v) {
  if(!value.length)
    return true
  return value == v[field]
}

function varFlags_match (include, flags, x) {
  for(var f in flags) {
    const a = flags[f]
    const b = x[f]
    if(include) {
      if (a != b)
        return false
    } else if (a == b) {
      return false
    }
  }
  return true
}

function curry(func) {
  const new_args = Array.slice(arguments, 1, arguments.length)

  return function (a) {
    const args = new_args.concat(Array.slice(arguments))
    return func.apply(this, args)
  }
}

function compose () {
  var funcs = Array.slice(arguments)
  return function () {
    var rval = funcs[funcs.length - 1].apply(this, arguments)
    for(var i = funcs.length - 2;i >= 0;i--) {
      rval = funcs[i](rval)
    }
    return rval
  }
}

function negate (f) {
  return function () { return !f.apply(this, arguments) }
}

/* @param include indicates whether return false if a member does not match
 or to return false if a member matches */
function flags2members(flags, include) {
  var members = new Array()
  for each (var f in flags) {
    var value = true
    var name = f
    const pos = f.indexOf('=')
    if(pos != -1) {
      name = f.substring(0, pos)
      value = f.substring(pos + 1)
    }
    members[name] = value
  }
  return members
}
  
function filterVars(vars, name, flags, nflags) {
  // pull out the magic type flags and stick them into type or the negation one, ntype
  var flags = flags2members(flags, true)
  if(name)
    flags["name"] = name
  var nflags = flags2members(nflags, false)
  return vars.filter(function(x) {return varFlags_match(true, flags, x)}).filter(function(x) {return varFlags_match(false, nflags, x)})
}

/*
Selects symbols from the current statement, based on their name or on def-use
tags from the dataflow analysis. The call erases earlier results of selections.
This is a boolean function that returns true if the resulting selection is nonempty,
and otherwise returns false.
*/
function select(name, flags, nflags, v) {
  if (!v)
    v = _vars
  _selected = filterVars(v, name, flags, nflags)
  return _selected.length > 0
}

/*
Like select, but this time the selection refines the result of earlier selections,
picking a subset of the symbols that match the additional criteria specified
here.
*/
function refine(name, flags, nflags) {
  if(!_selected)
    return false
  _selected = filterVars(_selected, name, flags, nflags)
  return _selected.length > 0
}

function display(vars) {
  print(uneval(vars))
}
  
/*
Reduces the current selection to those symbols that match the additional criteria
and that have a specified (non-zero) mark, assigned through an earlier use
of the mark primitive.
*/
function match(tag, flags, nflags) {
  if(!_state)
    return false
  const tagged = _state[tag]
  if(!tagged || !refine("", flags, nflags))
    return false
  const filtered = _selected.filter(
  function (selectedV) {
    return tagged.some(
      function (v) {
//         return selectedV.name == v.name
        // id avoids trouble due to variables having the same name, but IDing code isn't finished yet
        return selectedV.id == v.id
      }
    )
  } )
  if(filtered.length > 0) {
    _selected = filtered
    return true
  }
  return false
}

/*
Removes the markings for all currently selected
symbols.
*/
function unmark(tag) {
  if(!_selected || !_state)
    return

  var removes = _selected.map(function (x) {return x.name})
  for(var x in _state) {
    if(typeof tag != "undefined" && tag != x)
      continue
    _state[x] = _state[x].filter(function (v) { return removes.indexOf(v.name) == -1 })
  }
}

/*
Assigns an integer marking N to all currently selected symbols. Since the default
marking is zero, the marking should be non-zero to be detectable later.
The marking in effect assigns an individual state to a symbol of interest, so
that the evolution of its state can be tracked and checked in the property.
*/
function mark(tag) {
  if(!_state) {
    _state = new Array()
  }
  if(! _state[tag]) {
    _state[tag] = _selected.slice()
  }
  else {
    var old = _state[tag]
    for each(var v in _selected) {
      old = old.filter(function(x) { x.id != v.id })
    }
    _state[tag] = old.concat(_selected)
  }
}

/*
Triggers the printing of a list of all symbols that appear on the depth-first
search stack at the point of call, and all symbols that appear in the def-use list
for the current statement. It also shows all symbols that were selected through
use of the primitives select, refine, and unselect.
*/
function list() {
  if(typeof _selected == "object") {
    print("SELECTED:")
    display(_selected)
  }
  
  for(var i in _state) {
    print("MARKED " +i +":")
    display(_state[i])
  }
}

/*
Returns true if symbols exist that match the criteria specified, and that have
 the specified (non-zero) mark
*/
function marked(tag, flags, nflags) {
  if(!_state || !_state[tag])
    return false
  _selected = filterVars(_state[tag], "", flags, nflags)

  return _selected.length > 0
}  

function known_zero() {
  if(!_selected)
    return false
  return _selected.every(function(x) {return is_zero(x.id)})
}

function known_nonzero() {
  if(!_selected)
    return false
  return _selected.every(function(x) {return is_nonzero(x.id)})
}

function known_global() {
  if(!_selected)
    return false
  return _selected.every(function(x) {return is_global(x.id)})
}

/* Compacts fieldOf chains and/or produces shallow copies */
function flatten_item(item, copy) {
  if(item.fieldOf) {
    var new_item = flatten_item(item.fieldOf, true)
    for(var i in item)
      if(i != "fieldOf" && i != "name")
        new_item[i] = item[i]
    new_item.name += "." + item.name
    return new_item
  } else if (copy) {
    var new_item = new item.constructor()
    for(var i in item)
      new_item[i] = item[i]
    return new_item
  }
  return item
}

function extract(item) {
  var accum = [item]
  function extract_from(children) {
    for each (var v in item.arguments) {
      newv = flatten_item(v, true)
      newv.paramOf = item.name
      accum.push(extract(newv));
    }
  }
  if(item.arguments) {
    extract_from(item.arguments)
    delete item.arguments
  }
  if(item.assign) {
    extract_from(item.assign)
    delete item.assign
  }
  return accum;
}

// couple use splice to optimize here
function flatten_list(ls, dest) {
  if(!dest)
    dest = []
  for each (var i in ls)
    if (i.constructor == Array)
      flatten_list(i, dest)
    else
      dest.push(i)
  return dest
}

/* converted the dehydra nested "ast" into uno-style
 flat list */
function flatten(lsa) {
  return flatten_list(lsa.map(function (x) {return extract(flatten_item(x, true))}));
}

function select_item(item) {
  item = flatten_item(item, true)
  delete item.arguments
  if(item.assign) {
    delete item.assign
    item.isDef = true
  }
  _selected = [item]
  return item
}

// recurses up to the source of the fcall
function getVar(v) {
  var upper = v.fieldOf
  if(upper)
    return getVar(upper)
  return v
}

function iter(f, vars) {
  function var_iter(f, v) {
    f(v)
    for each(var va in v.assign)
      var_iter(f, va)
    for each(var vp in v.arguments)
      var_iter(f, vp)
    for each (var vi in v.inits)
      var_iter(f, vi)
  }
  for each(var v in vars)
    var_iter(f, v)
}

function process_function (decl, statements) {
  this._function = decl
  if (!this.process) return
  for each (var o in statements) {
    this._loc = o.loc
    process (o.statements)
  }
  delete this._loc
}

function sameVar (v1, v2) {
  // allow both to be undefined or both to be defined
  if (!v1 && !v2) return true;
  else if (! (v1 && v2)) return false;

  if (!(v1.name == v2.name && v1.type == v2.type)) return false;
  
  return sameVar(v1.fieldOf, v2.fieldOf);
}

function process_type (t) {
  if (this.process_class &&
      (t.kind == "struct" || t.kind == "class")) {
    print ("Warning: process_class is deprecated. Use process_type")
    process_class (t)
  }
}
