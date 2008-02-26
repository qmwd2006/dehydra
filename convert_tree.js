var files = {}

function getLine(loc) {
  var loc = loc.split (":")
  var fname = loc[0]
  var fbody = files[fname]
  if (!fbody)
    files[fname] = fbody = read_file(fname).split("\n")
  return fbody[loc[1] - 1]
}

function skipTypeWrappers (type) {
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

function Field (type, name, tag, isPointer, arrayLengthExpr) {
  this.type = type
  this.name = name
  this.tag = tag
  this.isPointer = isPointer
  this.arrayLengthExpr = arrayLengthExpr
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
  return "#define GENERATED_C2JS 1\n" + str + bodies.join ("\n")
}

Unit.prototype.addUnion = function (fields, type_name, type_code_name) {
  var ls = [""]
  ls.push ("switch (code) {");
  for each (var f in fields) {
    ls.push ("case " + f.tag + ":");
    ls.push ("  dehydra_defineProperty(this, obj, \"" + f.name
             + "\" , convert_" + f.type + " (this, &var->" + f.name + "));")
    ls.push ("  break;")
  }
  ls.push ("default:")
  ls.push ("  break;")
  ls.push ("}")
  ls.push ("return OBJECT_TO_JSVAL (obj);")
  this.functions.push (
    new Function ("convert_" + type_name
                  + "_union (Dehydra *this, enum " + type_code_name
                  + " code, union " + type_name + " *var, JSObject *obj)",
                  ls.join ("\n  ")));
}

Unit.prototype.addStruct = function (fields, type_name, prefix, isGTY) {
  var ls = []
  if (isGTY)
    ls.push ("void **v")
  ls.push ("if (!var) return JSVAL_VOID")
  if (isGTY) {
    ls.push ("v = pointer_map_contains (jsobjMap, var)")
    ls.push ("if (v) return (jsval) *v")
  }
  ls.push ("JSObject *obj = JS_ConstructObject (this->cx, &js_ObjectClass, NULL, this->globalObj)")
  if (isGTY)
    ls.push ("*pointer_map_insert (jsobjMap, var) = obj")
  ls.push ( (isGTY ? "int key = ":"") + "dehydra_rootObject (this, OBJECT_TO_JSVAL (obj))")
  for each (var f in fields) {
    var deref = f.isPointer ? "" : "&";
    if (!f.arrayLengthExpr) {
        ls.push ("dehydra_defineProperty (this, obj, \"" + f.name + "\", " 
        + "convert_" + f.type + " (this, " + deref + "var->" + f.name + "))")
    } else {
      var lls = ["  {", "size_t i;"]
      lls.push ("JSObject *destArray = JS_NewArrayObject (this->cx, 0, NULL);")
      lls.push ("dehydra_defineProperty (this, obj, \"" 
                + f.name + "\", OBJECT_TO_JSVAL (destArray));")
      lls.push ("for (i = 0; i < " + f.arrayLengthExpr + "; i++) {");
      lls.push ("  jsval val = convert_" + f.type + " (this, "
                + deref + "var->" + f.name + "[i]);");
      lls.push ("  JS_DefineElement (this->cx, destArray, i, val, NULL, NULL, JSPROP_ENUMERATE);");
      lls.push ("}")
      lls.push ("}")
      ls.push (lls.join("\n    "))
    }
  }
  if (isGTY)
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
  else if (type.isArray)
    return isPointer (type.type)
  return type.isPointer;
}

// should probly combine attribute extraction into an eval-happy general solution
function getUnionTag (attributes) {
  for each (var a in attributes) {
    if (a.name != "user")
      continue;
    var m = /tag\s*\("([A-Z0-9_]+)"\)/(a.value)
    if (m) return m[1]
  }
}

function getLengthExpr (attributes) {
  for each (var a in attributes) {
    if (a.name != "user")
      continue;
    var m = /length\s*\(\s*"(.+)"\s*\)/(a.value);
    if (m) return m[1].replace(/%h/, "(*var)") 
  }  
}

function isSpecial (attributes) {
  for each (var a in attributes) {
    if (a.name != "user")
      continue;
    if (/special/(a.value)) return true
  }  
}

function convert (unit, aggr) {
  if (!aggr || aggr.kind == "enum" || unit.guard(aggr)) {
    return
  }
  var ls = []
  var isUnion = aggr.kind == "union"
  for each (var m in aggr.members) {
    var type = skipTypeWrappers (m.type)
    if (type.kind != "struct"
        && type.name != "tree_node")  {
      //print(type.name +" " +  m.name + " " + m.loc)
      continue
    }
    if (isSpecial (m.attributes)) {
      print (aggr.name + "::" + m.name + " is special.Skipping...")
      continue
    }
    var subf  = convert (unit, type)
    if (subf)
      subf.addComment("Used in " + m.loc + ";")
    
    var tag;
    var lengthExpr;
    if (isUnion) {
      // GTY tags help figure out which field of a union to use
      tag = getUnionTag(m.attributes);
    } else {
      lengthExpr = getLengthExpr (m.attributes)
    }
    
    var name = /([^:]+)$/(m.name)[1]
    ls.push (new Field (type.name,
                        name,
                        tag,
                        isPointer(m.type),
                        lengthExpr))
  }
  var ret
  var isGTY = aggr.attributes && aggr.attributes.length
  if (isUnion) {
    ret = unit.addUnion (ls, aggr.name, "tree_node_structure_enum", isGTY)
  } else if (aggr.kind == "struct") {
    ret = unit.addStruct (ls, aggr.name, getPrefix(aggr), isGTY)
  }
  return ret
}

function process_var(v) {
  if (v.name != "global_namespace")
    return
  var unit = new Unit();
  convert(unit, skipTypeWrappers(v.type))
  var str = unit.toString()
  var fname = "treehydra_generated.h"
  write_file (fname, str)
  print ("Generated " + fname)
}

function process_tree (fn, tree) {
//  print (uneval(tree))
}

