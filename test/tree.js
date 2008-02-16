var files = {}

function getLine(loc) {
  var loc = loc.split (":")
  var fname = loc[0]
  var fbody = files[fname]
  if (!fbody)
    files[fname] = fbody = read_file(fname).split("\n")
  return fbody[loc[1] - 1]
}

function skipTypedef (type) {
  while (type) {
    if (type.typedef)
      type = type.typedef
    else if (type.type)
      type = type.type
    else
      break;
  }
  return type
}

function Field (type, name, tag, isPointer) {
  this.type = type
  this.name = name
  this.tag = tag
  this.isPointer = isPointer
}

Field.prototype.toString = function () {
  return this.type + " " + this.name + " = " + this.tag
}

function Function (name, body) {
  this.name = name
  this.body = body
  this.comment = ""
}

Function.prototype.addComment = function (comment) {
  this.comment += " " + comment
}

function Unit () {
  this.functions = []
  this.guarded = {}
}

Unit.prototype.toString = function () {
  var prefix = "static jsval "
  var forwards = this.functions.map (function (x) { return prefix + x.name })
  forwards.push("")
  var str = forwards.join (";\n");
  var bodies = this.functions.map (function (x) { 
    return "// " + x.comment + "\n"
      + prefix + x.name + " {" + x.body + "\n}\n";
  })
  return str + bodies.join ("\n")
}

Unit.prototype.addUnion = function (fields, type_name, type_code_name) {
  var ls = [""]
  ls.push ("switch (code) {");
  for each (var f in fields) {
    ls.push ("case " + f.tag + ":");
    ls.push ("  dehydra_defineProperty(this, obj, \"" + f.name
             + "\" , convert_" + f.type + " (this, &var." + f.name + "));")
    ls.push ("  break;")
  }
  ls.push ("default:")
  ls.push ("  break;")
  ls.push ("}")
  ls.push ("return OBJECT_TO_JSVAL (obj);")
  this.functions.push (
    new Function ("convert_" + type_name
                  + "_union (Dehydra *this, enum " + type_code_name
                  + " code, union " + type_name + " var, JSObject *obj)",
                  ls.join ("\n  ")));
}

Unit.prototype.addStruct = function (fields, type_name, prefix) {
  var ls = [""]
  ls.push ("JSObject *obj = JS_ConstructObject (this->cx, &js_ObjectClass, NULL, this->globalObj)")
  ls.push ("int key = dehydra_rootObject (this, OBJECT_TO_JSVAL (obj))")
  for each (var f in fields) {
    var deref = f.isPointer ? "" : "&"
    ls.push ("dehydra_defineProperty (this, obj, \"" + f.name + "\", " 
             + "convert_" + f.type + " (this, " + deref + "var->" + f.name + "))")
  }
  ls.push ("dehydra_unrootObject (this, key)")
  ls.push ("return OBJECT_TO_JSVAL (obj);")
  var f = new Function (
    "convert_" + type_name + " (Dehydra *this, " + prefix + " " + type_name + "* var)",
    ls.join (";\n  "))
  this.functions.push (f)
  return f
}

Unit.prototype.guard = function (key) {
  if (this.guarded[key]) return true;
  this.guarded[key] = key
  return false
}

function getPrefix (aggr) {
  if (aggr.isIncomplete || !aggr.members.length) return aggr.kind
  var aloc = aggr.members[0].loc.split(":")
  var file = aloc[0]
  var line = aloc[1]*1
  for (var i = 0;i < 5;i++) {
    var str = getLine(file + ":" + (line - i))
    var pos = str.lastIndexOf(aggr.kind)
    if (pos == -1) continue
    //typedef struct struct_name..means have to use struct on var
    if (str.indexOf (aggr.name) != -1)
      break;
    //typedef struct{  is the opposite
    if (str.lastIndexOf ("typedef", pos) != -1)
      return ""
    break
  }
  return aggr.kind
}

function isPointer (type) {
  if (type.typedef)
    return isPointer (type.typedef)
  return type.isPointer || type.isArray
}

function convert (unit, aggr) {
  if (!aggr || aggr.kind == "enum" || unit.guard(aggr)) {
    return
  }
  var ls = []
  var isUnion = aggr.kind == "union"
  for each (var m in aggr.members) {
    var type = skipTypedef(m.type)
    if (type.kind != "struct") 
      continue
    var subf  = convert (unit, type)
    if (subf)
      subf.addComment("Used in " + m.loc + ";")
    
    var tag;
    if (isUnion) {
      // GTY tags help figure out which field of a union to use
      var match = /"(.+)"/(getLine(m.loc))
      if (match)
        tag = match[1]
      else
        print (m.loc)
    }
    var name = /([^:]+)$/(m.name)[1]
    ls.push (new Field (type.name,
                        name,
                        tag,
                        isPointer(m.type)))
  }
  var ret
  if (isUnion) {
    ret = unit.addUnion (ls, aggr.name, "tree_node_structure_enum")
  } else if (aggr.kind == "struct") {
    ret = unit.addStruct (ls, aggr.name, getPrefix(aggr))
  }
  return ret
}

function process_var(v) {
  if (v.name != "global_namespace")
    return
  var unit = new Unit();
  convert(unit, skipTypedef(v.type))
  var str = unit.toString()
  var fname = this.base_name + ".auto.h"
  write_file (fname, str)
  print ("Generated " + fname)
}

function process_tree (fn, tree) {
//  print (uneval(tree))
}
