var files = {}

function getLine(fname, line) {
  var fbody = files[fname]
  if (!fbody)
    files[fname] = fbody = read_file(fname).split("\n")
  return fbody[line - 1]
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

function Field (type, name, tag, isAddrOf, arrayLengthExpr, cast, isPrimitive) {
  this.type = type
  this.name = name
  this.tag = tag
  this.isAddrOf = isAddrOf
  this.arrayLengthExpr = arrayLengthExpr
  this.cast = cast
  this.isPrimitive = isPrimitive
}

Field.prototype.toString = function () {
  return this.type + " " + this.name + " = " + this.tag
}

function Function (isStatic, name, body, type) {
  this.name = name
  this.body = body
  this.comment = ""
  this.type = type ? type : "void"
  this.prefix = isStatic ? "static " : ""
}

Function.prototype.addComment = function (comment) {
  this.comment += " " + comment
}

function Unit () {
  this.functions = []
  this.guarded = {}
  this.enumValues = {}
}

Unit.prototype.toString = function () {
  var forwards = this.functions.map (function (x) { return x.prefix + x.type + " " + x.name })
  forwards.push("")
  var str = forwards.join (";\n");
  var bodies = this.functions.map (function (x) { 
    return "// " + x.comment + "\n"
      + x.prefix + x.type + " " + x.name + " {\n  " + x.body + "\n}\n";
  })
  return ["#define GENERATED_C2JS 1",
          "#include \"treehydra_generated.h\"",
          str, bodies.join ("\n")].join("\n")
}

function callGetLazy (type, name) {
  var expr = "&var->" + name 
  var func = "get_lazy"
  return func + " (this, lazy_" + type + ", " 
    + expr + ", obj, \"" + name + "\")"
}

Unit.prototype.addUnion = function (fields, type_name, type_code_name) {
  var ls = []
  ls.push ("switch (code) {");
  for each (var f in fields) {
    ls.push ("case " + f.tag + ":");
    ls.push ("  " + callGetLazy (f.type, f.name) + ";")
    ls.push ("  break;")
  }
  ls.push ("default:")
  ls.push ("  break;")
  ls.push ("}")
  this.functions.push (
    new Function (false, "convert_" + type_name
                  + "_union (struct Dehydra *this, enum " + type_code_name
                  + " code, union " + type_name + " *var, struct JSObject *obj)",
                  ls.join ("\n  ")));
}

Unit.prototype.registerEnumValue = function (name, value) {
  this.enumValues[name] = value
}

Unit.prototype.saveEnums = function (fname) {
  var constants = ["// generated by convert_tree.js"]
  for (var name in this.enumValues) {
    constants.push ("const " + name + " = new EnumValue(\"" 
                    + name + "\", " + this.enumValues[name] + ")")
  }
  write_file (fname, constants.join("\n"))
  print ("Generated " + fname)
}

Unit.prototype.addEnum = function (fields, type_name) {
  var ls = []
  ls.push ("switch (var) {");
  for each (var f in fields) {
    ls.push ("case " + f.name + ":");
    ls.push ("  return get_enum_value (this, \"" + f.name + "\");")
    this.registerEnumValue (f.name, f.value)
  }
  ls.push ("default:")
  ls.push ("  return JSVAL_NULL;")
  ls.push ("}")
  this.functions.push (
    new Function (true, "convert_" + type_name
                  + " (struct Dehydra *this, enum " + type_name + " var)",
                  ls.join ("\n  "), "jsval"));
}

function callGetExistingOrLazy (type, name, deref, cast, isPrimitive, isArrayItem) {
  var index = isArrayItem ? "[i]" : "";
  var dest = !isArrayItem ? "obj" : "destArray";
  var propValue = !isArrayItem ? '"' + name + '"' : "buf";
  if (!cast) cast = ""
  else cast = "(" + cast + ") "
  var expr = cast + deref + "var->" + name + index
  if (isPrimitive) {
    var convert =  "convert_" + type + "(this, " + expr + ")";
    return "dehydra_defineProperty (this, " + dest + ", " + propValue + ", " + convert + ")"
  }
  return "get_existing_or_lazy (this, lazy_" + type + ", " 
    + expr + ", " + dest + ", " + propValue + ")"
}

Unit.prototype.addStruct = function (fields, type_name, prefix, isGTY) {
  var ls = []
  var type = prefix + " " + type_name
  ls.push (type + " *var = ("+ type + "*) void_var")
  ls.push ("if (!var) return")
  ls.push ("dehydra_defineStringProperty (this, obj, \"_struct_name\", \"" + type_name + "\")")
  for (var i in fields) {
    var f = fields[i]
    var deref = f.isAddrOf ? "&" : "";
    if (!f.arrayLengthExpr) {
      // Only structs need their addresses to be taken
      // Assume that that also means they are unique to the structure
      // containing them
      const func = f.isAddrOf ? callGetLazy : callGetExistingOrLazy
      ls.push (func (f.type, f.name, deref, f.cast, f.isPrimitive))
    } else {
      var lls = ["  {", "size_t i;"]
      lls.push ("char buf[128];")
      lls.push ("const size_t len = " + f.arrayLengthExpr + ";")
      lls.push ("struct JSObject *destArray =  dehydra_defineArrayProperty (this, obj, \"" 
                + f.name + "\", len);")
      lls.push ("for (i = 0; i < len; i++) {");
      lls.push ("  sprintf (buf, \"%d\", i);")
      lls.push ("  " + callGetExistingOrLazy (f.type, f.name, deref, f.cast, f.isPrimitive, true) + ";")
      lls.push ("}")
      lls.push ("}")
      ls.push (lls.join("\n    "))
    }
  }
  var f = new Function (true,
    "lazy_" + type_name + " (struct Dehydra *this, void* void_var, struct JSObject *obj)",
    ls.join (";\n  ") + ";")
  this.functions.push (f)
  return f
}

Unit.prototype.guard = function (key) {
  if (this.guarded[key]) return true;
  this.guarded[key] = key
  return false
}

function getPrefix (aggr) {
  var aloc = aggr.loc.split(":")
  var file = aloc[0]
  var line = aloc[1]*1
  for (var i = 0;i < 5;i++) {
    // first find the begining of struct ... {} part
    var str = getLine (file, line)
    var pos = str.indexOf (aggr.kind)
    if (pos == -1) {
      line--;
      continue
    }
    // capture multiline struct decls
    for (var x = 1; x <= 5; x++) {
      str += getLine(file, line + x);
    }
    
    //typedef struct struct_name..means have to use struct on var
    var bracePos = str.indexOf ("{", pos);
    var namePos = str.indexOf (aggr.name, pos);
    if (namePos != -1 && namePos < bracePos)
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

const tagRegexp = /tag\s*\("([A-Z0-9_]+)"\)/
// should probly combine attribute extraction into an eval-happy general solution
function getUnionTag (attributes) {
  for each (var a in attributes) {
    if (a.name != "user")
      continue;
    var m = tagRegexp.exec(a.value)
    if (m) return m[1]
  }
}

const lengthRegexp = /length\s*\(\s*"(.+)"\s*\)/
function getLengthExpr (attributes) {
  for each (var a in attributes) {
    if (a.name != "user")
      continue;
    var m = lengthRegexp.exec (a.value);
    if (m) return m[1].replace(/%h/, "(*var)") 
  }  
}

const specialRegexp = /special/
function isSpecial (attributes) {
  for each (var a in attributes) {
    if (a.name != "user")
      continue;
    if (specialRegexp.test(a.value)) return true
  }  
}

const skipRegexp = /skip/
function isSkip (attributes) {
  for each (var a in attributes) {
    if (a.name != "user")
      continue;
    if (skipRegexp.test(a.value)) return true
  }  
}

var type_guard = {}

const charRegexp = /char/
function isCharStar (type) {
  while (type.typedef)
    type = type.typedef

  if (!type.isPointer) return false
  type = type.type
  while (type.typedef)
    type = type.typedef
  return type.name && charRegexp.test(type.name)
}

const unsignedIntRegexp = /unsigned|int/
function isUnsignedOrInt (type) {
  while (type.typedef)
    type = type.typedef
  return type.name && unsignedIntRegexp.test (type.name)
}

const stripPrefixRegexp = /([^:]+)$/;
const arraySizeRegexp = /^(\d)u$/;
// meaty part of the script
function convert (unit, aggr, unionTopLevel) {
  if (!aggr || unit.guard(aggr)) {
    return
  }
  var ls = []
  var isUnion = aggr.kind == "union"
  var isEnum = aggr.kind == "enum"
  var aggr_ls = undefined
  if (!isEnum)
    aggr_ls = aggr.members

  var oldloc = this._loc
  for each (var m in aggr_ls) {
    var type = skipTypeWrappers (m.type)
    var type_name = stripPrefixRegexp.exec(type.name)[1]
    var isAddrOf = false
    var type_kind = type.kind
    var tag = undefined
    var lengthExpr = undefined
    /* Extract size if type is an array */
    var lengthResults = arraySizeRegexp.exec (m.type.size);
    /* arrays of size 1 are actually funky var length arrays */
    if (lengthResults && lengthResults[1] != "u")
      lengthExpr = lengthResults[1]*1 + 1
    var cast = undefined
    if (m.name == "tree_base::code") {
      type_kind = "enum"
      type = this.tree_code_type
      type_name = type.name
    }
    this._loc = m.loc
    if (m.name == "emit_status::x_regno_reg_rtx") {
      print ("Skipping m.name because it causes issues I don't feel like dealing with")
      continue;
    }

    var isPrimitive = false
    if (type_kind == "struct"
        || type.name == "tree_node") {
      isAddrOf = !isPointer(m.type)
      if (type.isIncomplete) {
        print (m.name + "' type is incomplete. Skipping.");
        continue;
      }
      var subf  = convert (unit, type, isUnion)
      if (subf)
        subf.addComment("Used in " + m.loc + ";")

      if (isUnion) {
        // GTY tags help figure out which field of a union to use
        tag = getUnionTag(m.attributes);
      } else {
        lengthExpr = getLengthExpr (m.attributes)
      }
    } else if (type_kind == "enum") {
      isPrimitive = true
      convert (unit, type)
    } else if (isCharStar (m.type)) {
      type_name = "char_star"
      cast = "char *"
      isPrimitive = true
    } else if (isUnsignedOrInt(m.type)) {
      type_name = "int"
      cast = "int"
      isPrimitive = true
    } else {
      if (!type_guard[type_name]) {
        type_guard[type_name] = type_name
        print("Unhandled " + type_name + " " +  m.name + " " + m.loc)
      }
      continue
    }
    if (isSpecial (m.attributes)) {
      if (m.name == "tree_exp::operands")
        lengthExpr = "TREE_OPERAND_LENGTH ((tree) &(*var))";
      else {
        print (m.name + " is special. Skipping...")
        continue
      }
    } else if (m.name == "ssa_use_operand_d::use") {
      print ("Skipping " + m.name + ". For some reason can't see GTY(skip) for it")
      continue;
    }
    var name = stripPrefixRegexp.exec(m.name)[1]
    ls.push (new Field (type_name,
                        name,
                        tag,
                        isAddrOf,
                        lengthExpr,
                        cast, isPrimitive))
  }
  this._loc = oldloc
  var ret = undefined
  var isGTY = !unionTopLevel && aggr.attributes && aggr.attributes.length
  if (isUnion) {
    ret = unit.addUnion (ls, aggr.name, "tree_node_structure_enum", isGTY)
  } else if (aggr.kind == "struct") {
    ret = unit.addStruct (ls, aggr.name, getPrefix(aggr), isGTY)
  } else if (isEnum) {
    ret = unit.addEnum (aggr.members.map (function (x) {
      return {name : x.name, value: x.value}
    }), stripPrefixRegexp.exec(aggr.name)[1])
  } 
  return ret
}

var unit = new Unit();

function process_type(type) {
  if (type.name == "tree_code") {
    // needed because doing enumType foo:1
    // makes gcc loose enum info in the bitfield
    this.tree_code_type = type
  } else if (type.name == "tree_node") {
    if (!type.attributes) {
      throw new Error (type.name + " doesn't have attributes defined. GTY as attribute stuff must be busted.")
    }
    this.tree_node = type
  } else if (type.name == "tree_code_class") {
    this.tree_code_class = type
    // this enum occurs last, so do the generation here
  }
  // got all the ingradients, time to cook
  if (this.tree_code_class && this.tree_code_type && this.tree_node) {
    for each (var m in this.tree_code_class.members) {
      unit.registerEnumValue (m.name, m.value)
    }
    convert(unit, this.tree_node)
    var str = unit.toString()
    var fname = "treehydra_generated.c";
    write_file (fname, str)
    print ("Generated " + fname)
    unit.saveEnums ("enums.js")
    delete this.tree_code_class
    delete this.tree_node
    delete this.tree_code_type
  }
}

