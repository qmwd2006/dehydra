include ("map.js")
include ("platform.js")
var files = {}

function getLine(fname, line) {
  var fbody = files[fname]
  if (!fbody)
    files[fname] = fbody = read_file(fname).split("\n")
  return fbody[line - 1]
}

function skipTypeWrappers (type) {
  while (type) {
    if (type.typedef) {
      let next_type = type.typedef
      /* Workaround: C frontend doesn't give names to structs declared with a typedef:
       * typedef struct {...} foo; 
       * on the other hand, that makes life easier for getPrefix() */
      if (!next_type.name && type.name)
        next_type.name = type.name
      if(next_type.name == type.name) {
        next_type.hasTypedef = true
      }

      type = next_type
    }
    else if (type.type)
      type = type.type
    else
      break;
  }
  return type
}

/* Used to toggle names, name depends on whether it's a toplevel scope or nested deeper */
function getVarName(isTopmost) {
  return isTopmost ? "topmost" : "var"
}

function Field (type, name, tag, isAddrOf, arrayLengthExpr, cast, isPrimitive, unionResolver) {
  this.type = type
  this.name = name
  this.tag = tag
  this.isAddrOf = isAddrOf
  this.arrayLengthExpr = arrayLengthExpr
  this.cast = cast
  this.isPrimitive = isPrimitive
  this.unionResolver = unionResolver
}

Field.prototype.toString = function () {
  return this.type + " " + this.name + " = " + this.tag
}

// different function levels
const FN_STATIC = new EnumValue ("FN_STATIC")
const FN_TOPMOST = new EnumValue ("FN_TOPMOST")
const FN_NESTED = new EnumValue ("FN_NESTED")

function Function (fn_level, name, params, bodyFunc, subFunctions) {
  this.name = name
  this.params = params
  this.bodyFunc = bodyFunc
  this.comment = ""
  this.type = "void"
  this.fn_level = fn_level
  this.subFunctions = subFunctions
}

Function.prototype.addComment = function (comment) {
  this.comment += " " + comment
}

Function.prototype.signature = function () {
  return (this.fn_level === FN_STATIC ? "static " : "") + this.type + " "
    + this.name + " " + this.params
}

Function.prototype.toString = function (indent, outerindent) {
  if (!indent)
    indent = "  "
  if (!outerindent)
    outerindent = ""
  var str = outerindent + "// " + this.comment + "\n"
    + outerindent + this.signature() + " {\n";
  var extra = ""
  if (this.subFunctions)
    extra = this.subFunctions.map (function(x) {return x.toString(indent+"  ", indent)}).join("\n")
  str += indent + this.bodyFunc(extra, indent) 
    + "\n"+ outerindent + "}\n";
  return str
}

function Unit () {
  this.functions = []
  // tglek: could combine functionNames, functions, guarded into a singl 
  // data structure but it wouldn't make a big perf difference so I didn't bother.
  // this hardcodes a func that is implemented in C
  this.functionNames = {lazy_tree_node:{}}
  this.guarded = new Map()
  this.enumValues = {}
}

const unit = new Unit();

Unit.prototype.hasFunction = function (name) {
  return this.functionNames.hasOwnProperty(name)
}

Unit.prototype.toString = function (indent) {
  if (!indent)
    indent = "  "
  var forwards = this.functions.map (function (x) { return x.signature() })
  forwards.push("")
  var str = forwards.join (";\n");
  return ["#define GENERATED_C2JS 1",
          "#include \"treehydra_generated.h\"",
          str, this.functions.join ("\n")].join("\n")
}

Unit.prototype.addFunction = function (f) {
  this.functions.push (f)
  this.functionNames[f.name] = f
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

Unit.prototype.guard = function (key) {
  if (this.guarded.has(key)) return true;
  this.guarded.put(key)
  return false
}

function callGetExistingOrLazy (type, name, isAddrOf, cast, isPrimitive, isArrayItem, isTopmost) {
  const varDeref = getVarName(isTopmost) + "->"
  const lazy_call = "lazy_" + type
  if (isAddrOf && !isArrayItem) {
    var expr = "&" + varDeref + name 
    if (unit.hasFunction(lazy_call)) {
      var func = "get_lazy";
      return func + " (this, " + lazy_call + ", " 
        + expr + ", obj, \"" + name + "\")"
    }
    // do lazy call directly
    return lazy_call + " (this, " + expr 
      + ", dehydra_defineObjectProperty (this, obj, \"" + name + "\"))";
  }
  var deref = isAddrOf ? "&" : "";
  var index = isArrayItem ? "[i]" : "";
  var dest = !isArrayItem ? "obj" : "destArray";
  var propValue = !isArrayItem ? '"' + name + '"' : "buf";
  if (!cast) cast = ""
  else cast = "(" + cast + ") "
  var expr = cast + deref + varDeref + name + index
  if (isPrimitive) {
      return "convert_" + type + "(this, " +  dest + ", " + propValue + ", " + expr + ");";
  }
  if (unit.hasFunction(lazy_call)) {
    return "get_existing_or_lazy (this, " + lazy_call + ", " 
      + expr + ", " + dest + ", " + propValue + ")"
  }
  throw new Error("eager getexisting/orlazy not implemented");
}

function makeUnionBody (fields, isTopmost, indent) {
  var noTagLs = []
  var ls = []
  ls.push ("switch (code) {");
  for each (var f in fields) {
    if (!f.tag) {
      noTagLs.push(f)
      continue
    }
    ls.push ("case " + f.tag + ":");
    ls.push ("  " + callGetExistingOrLazy (f.type, f.name, f.isAddrOf, f.cast, f.isPrimitive, false, isTopmost) + ";")
    ls.push ("  break;")
  }
  ls.push ("default:")
  ls.push ("  break;")
  ls.push ("}")
  for each (var f in noTagLs) {
    ls.push (callGetExistingOrLazy (f.type, f.name, f.isAddrOf, f.cast, f.isPrimitive, false, isTopmost) + ";")
  }
  return ls.join("\n" + indent)
}

function makeUnion (fields, type_name, type_code_name, subFunctions, fn_level) {
  const isTopmost = fn_level != FN_NESTED
  function bodyFunc (extra, indent) {
    return extra + "\n" + indent + makeUnionBody (fields, isTopmost, indent)
  }
  const fname = "convert_" + type_name + "_union"
  const params = "(struct Dehydra *this, "
    + type_code_name + " code, union " + type_name + " *"
    + getVarName (isTopmost) + ", struct JSObject *obj)"
  return new Function (fn_level, fname, params,
                       bodyFunc, subFunctions)
}

function makeEnum (unit, fields, type_name, enum_inherit, fn_level) {
  var ls = []
  const enumNames = {}
  ls.push ("jsval v;");
  ls.push ("switch (var) {");
  for each (var f in fields) {
    unit.registerEnumValue (f.name, f.value)
    if (enumNames[f.value])
      continue;
    enumNames[f.value] = f.name

    ls.push ("case " + f.name + ":");
    ls.push ("  v = get_enum_value (this, \"" + f.name + "\");")
    ls.push ("  break;")
  }
  ls.push ("default:")
  if (enum_inherit) {
    ls.push ("  convert_" + enum_inherit + " (this, parent, propname, (enum "
             + enum_inherit +") var);");
    ls.push ("  return;");
  } else {
    ls.push ('  error( "Treehydra Implementation: generating dummy tree code object for unimplemented tree_code %s\\n", tree_code_name[var]);');
    ls.push ("  v = get_enum_value (this, \"LAST_AND_UNUSED_TREE_CODE\");")
  }
  ls.push ("}")
  ls.push ("dehydra_defineProperty (this, parent, propname, v);")
  function bodyFunc (extra, indent) {
    return extra + "\n" + indent + ls.join ("\n" + indent)
  }
  return new Function (fn_level, "convert_" + type_name,
                       "(struct Dehydra *this, struct JSObject *parent, const char *propname, enum " + type_name + " var)",
                       bodyFunc);
}

/* F is a Field */
function callUnion (type, name, unionResolver, isTopmost, indent) {
  const obj = "obj_" + name
  var str = "struct JSObject *" + obj
    + " = dehydra_defineObjectProperty (this, obj, \"" + name + "\");\n"
  str += indent + "convert_" + type + "_union (this, " 
    + unionResolver + ", " + "&" + getVarName(isTopmost) + "->" + name 
    + ", " + obj + ")";
  return str
}

function makeStructBody (fields, indent, isTopmost) {
  var ls = []
  for (var i in fields) {
    var f = fields[i]
    if (f.unionResolver) {
      ls.push (callUnion (f.type, f.name, f.unionResolver, isTopmost, indent))
    } else if (!f.arrayLengthExpr) {
      // Only structs need their addresses to be taken
      // Assume that that also means they are unique to the structure
      // containing them
      ls.push (callGetExistingOrLazy (f.type, f.name, f.isAddrOf, f.cast, f.isPrimitive, false, isTopmost))
    } else {
      var lls = ["{", "size_t i;"]
      lls.push ("char buf[128];")
      lls.push ("const size_t len = " + f.arrayLengthExpr + ";")
      lls.push ("struct JSObject *destArray =  dehydra_defineArrayProperty (this, obj, \"" 
                + f.name + "\", len);")
      lls.push ("for (i = 0; i < len; i++) {");
      lls.push ("  sprintf (buf, \"%d\", i);")
      lls.push ("  " + callGetExistingOrLazy (f.type, f.name, f.isAddrOf, f.cast, f.isPrimitive, true, isTopmost) + ";")
      lls.push ("}")
      ls.push (lls.join("\n" + indent + "  ") + "\n" + indent+ "}")
    }
  }
  return ls.join (";\n" + indent) + ";"
}

function makeStruct (fields, type_name, prefix, subFunctions, fn_level) {
  const isTopmost = fn_level != FN_NESTED
  function printer (extra, indent) {
    var type = prefix + " " + type_name
    const varName = getVarName (isTopmost)
    var body = "if (!void_var) return;\n"
      body += indent + type + " *" + varName + " = ("+ type + "*) void_var;\n";
    body += extra + "\n"
    body += indent + "dehydra_defineStringProperty (this, obj, \"_struct_name\", \"" + type_name + "\");\n"
    return body + indent + makeStructBody (fields, indent, isTopmost)
  }
  var f = new Function (fn_level,
    "lazy_" + type_name, "(struct Dehydra *this, void* void_var, struct JSObject *obj)",
    printer, subFunctions)
  return f
}

function getPrefix (aggr) {
  /* if there is a typedef that was encounted while skipTypeWrappers()ing
   * then just use that...onfortunately C++ FE eats some typedefs
   * This works great in C
   */
  if (aggr.hasTypedef)
    return ""
  /* if this is C FE, then we know what we want to know 
   * aka no typedef...with C++ there might still be a need to use the typedef 
   * that was eaten
   */
  if (sys.frontend == "GNU C") 
    return aggr.kind
  
  if (aggr.kind != "struct")
    return aggr.kind + " "
  var file = aggr.loc.file;
  var line = aggr.loc.line;
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

const percentH_Regexp = /%h/g
const percent0_Regexp = /%0/g
const percent1_Regexp = /%1/g
const descRegexp = /desc\s*\("(.*)"\)/;

function getUnionResolver(attributes, fieldName, isTopmost) {
  for each (var a in attributes) {
    if (a.name != "user")
      continue;
    var m = descRegexp.exec(a.value)
    // took a guess about %h in this context
    if (m) {
      const varName = getVarName(isTopmost)
      var str = m[1].replace(percent0_Regexp, "(*topmost)")
      str = str.replace(percent1_Regexp, "(*" + varName + ")")
      str = str.replace(percentH_Regexp, varName + "->" + fieldName)
      return str
    }
  }
}

const lengthRegexp = /length\s*\(\s*"(.+)"\s*\)/;
function getLengthExpr (attributes, isTopmost) {
  for each (var a in attributes) {
    if (a.name != "user")
      continue;
    var m = lengthRegexp.exec (a.value);
    if (m) return m[1].replace(percentH_Regexp, "(*" + getVarName(isTopmost) + ")") 
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

const charRegexp = /char/
function isCharStar (type) {
  while (type.typedef)
    type = type.typedef

  if (!(type.isPointer || (type.isArray && type.size == "0u"))) return false
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
const location_tRegexp = /location_t|location_s|source_locus/;

/* meaty part of the script
* Unit is what holds the result
* Aggr is the data type to convert
* zeroTh(%0), is the outermost aggr type
* first(%1) is the parent of current type
*/
const tree_code_name = isGCC42 ? "tree_common::code" : "tree_base::code";
function convert (unit, aggr) {
  if (!aggr || unit.guard(aggr)) {
    return
  }
  var ls = []
  const isUnion = aggr.kind == "union"
  const isEnum = aggr.kind == "enum"
  const isToplevelType = aggr.name.indexOf(":") == -1
  const aggr_name = stripPrefixRegexp.exec(aggr.name)[1]
  // Keep nested structs/unions here
  var subFunctions = []
  var aggr_ls = undefined
  if (!isEnum)
    aggr_ls = aggr.members

  var oldloc = this._loc
  for each (var m in aggr_ls) {
    this._loc = m.loc
    var name = stripPrefixRegexp.exec(m.name)[1]
    var type = skipTypeWrappers (m.type)
    var type_name = stripPrefixRegexp.exec(type.name)[1]
    var isAddrOf = false
    var type_kind = type.kind
    var tag = undefined
    var lengthExpr = undefined
    /* Extract size if type is an array */
    var lengthResults = arraySizeRegexp.exec (m.type.size);
    var unionResolver = undefined
    var subf = undefined
    /* arrays of size 1 are actually funky var length arrays */
    if (lengthResults && lengthResults[1] != "u")
      lengthExpr = lengthResults[1]*1 + 1
    var cast = undefined
    if (m.name == tree_code_name) {
      type_kind = "enum"
      type = this.tree_code
      type_name = type.name
    }
    switch (m.name) {
    case "tree_type::symtab":
    case "emit_status::x_regno_reg_rtx":
    case "ssa_use_operand_d::use":
      print ("Skipping " + m.name + "because it causes issues I don't feel like dealing with")
      continue;
    default:
      if (m.name != "lang_type::lang_type_u::h"
          && (isSkip(m.attributes) || isSkip(m.type.attributes))) {
          print ("Skipping " + m.name + ". ");
          continue;
      }
    }
    var isPrimitive = false
    if (isUnion) {
      // GTY tags help figure out which field of a union to use
      tag = getUnionTag (m.attributes);
      if (!tag)
        tag = getUnionTag (m.type.attributes)
    }
    
    if (type_kind == "struct" || type.name == "tree_node"
        || (type_kind == "union" && type.name.indexOf("::") != -1)) {
      isAddrOf = !isPointer(m.type)
      if (type.isIncomplete) {
        print (m.name + "' type is incomplete. Skipping.");
        continue;
      }
      subf = convert (unit, type, isUnion)
      
      if (!isUnion) {
        lengthExpr = getLengthExpr (m.attributes, isToplevelType)
        if (type.name != "tree_node") 
          unionResolver = getUnionResolver (m.type.attributes, name, isToplevelType)
      }
    } else if (type_kind == "enum") {
      isPrimitive = true
      subf = convert (unit, type)
    } else if (isCharStar (m.type)) {
      type_name = "char_star"
      cast = "char *"
      isPrimitive = true
      lengthExpr = undefined
    } else if (location_tRegexp(m.type.name)) {
      // This must appear before isUnsignedOrInt because location_t is an
      // int (if we are not using gcc 4.2), but we want to convert it to a
      // formatted location.
      type_name = 'location_t';
      cast = isGCC42 ? 'location_s' : 'location_t';
      isPrimitive = true;
    } else if (isUnsignedOrInt(m.type)) {
      type_name = "int"
      cast = "HOST_WIDE_INT"
      isPrimitive = true
    } else {
      print("Unhandled " + type_name + " " +  m.name + " " + m.loc)
      continue
    }
    if (isSpecial (m.attributes)) {
      if (m.name == "tree_exp::operands")
        lengthExpr = "TREE_OPERAND_LENGTH ((tree) &(*topmost))";
      else {
        print (m.name + " is special. Skipping...")
        continue
      }
    } 
    ls.push (new Field (type_name,
                        name,
                        tag,
                        isAddrOf,
                        lengthExpr,
                        cast, isPrimitive, unionResolver))
    if (subf)
      subFunctions.push(subf)
  }
  this._loc = oldloc
  var ret = undefined
  if (isUnion) {
    ret = makeUnion (ls, aggr_name,
                         (aggr_name == "tree_node" 
                          ? "enum tree_node_structure_enum" 
                          : "unsigned int"), subFunctions, 
                     isToplevelType ? FN_TOPMOST : FN_NESTED)
  } else if (aggr.kind == "struct") {
    let level
    if (aggr_name == "cgraph_node")
      level == FN_TOPMOST
    else if (isToplevelType)
      level = FN_STATIC
    else
      level = FN_NESTED
    ret = makeStruct (ls, aggr_name, getPrefix(aggr), subFunctions, level)
  } else if (isEnum) {
    var enum_inherit = undefined
    if (aggr_name == "tree_code") {
      // do inheritance stuff
      convert (unit, this.cplus_tree_code)
      enum_inherit = this.cplus_tree_code.name
    }
    ret = makeEnum (unit, aggr.members, aggr_name, enum_inherit,
                    isToplevelType ? FN_STATIC : FN_NESTED)
  } 
  if (ret) {
    ret.addComment (aggr.loc)
    if (!isToplevelType)
      return ret
    // don't add nested functions to toplevel
    unit.addFunction (ret);
  }
}

function process_type(type) {
  while (type.variantOf)
    type = type.variantOf

  if (type.name == "tree_code") {
    // needed because doing enumType foo:1
    // makes gcc loose enum info in the bitfield
    this.tree_code = type
  } else if (type.name == "cgraph_node") {
    if (!type.attributes) {
      throw new Error (type.name + " doesn't have attributes defined. GTY as attribute stuff must be busted.")
    }
    this.cgraph_node = type
  } else if (type.name == "tree_code_class") {
    this.tree_code_class = type
  } else if (type.name == "cplus_tree_code") {
    this.cplus_tree_code = type
  } else if (type.name == "integer_type_kind") {
    this.integer_type_kind = type
  }
}

function input_end () {
  // got all the ingradients, time to cook
  if (!(this.tree_code_class && this.tree_code
      && this.cgraph_node && this.integer_type_kind)) {
    print ("Dehydra didn't find required types needed to generate Treehydra")
    return
  }
  if (!this.cplus_tree_code)
    this.cplus_tree_code = this.tree_code

  for each (var m in this.tree_code_class.members) {
    unit.registerEnumValue (m.name, m.value)
  }

  for each (var m in this.integer_type_kind.members) {
    unit.registerEnumValue (m.name, m.value)
  }
  
  convert(unit, this.cgraph_node)
  var str = unit.toString()
  var fname = "treehydra_generated.c";
  write_file (fname, str)
  print ("Generated " + fname)
  unit.saveEnums ("enums.js")
  delete this.tree_code_class
  delete this.cgraph_node
  delete this.tree_code
}
