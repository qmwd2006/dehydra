/**
 * gClassMap maps class names to an object with the following properties:
 *
 * .final = true   if the class has been annotated as final, and may not be
 *                 subclassed
 * .stack = true   if the class has been annotated as a stack class,
 *                 derives from a stack class, or has a member that is a
 *                 stack class
 * .gc    = true   if the class is a GC base class, is annotated as a GC class
 *                 derives from a gc class, or has a member that is a gc class.
 * .gcfinalizable = true
 *                 if the left-most base of this class other than
 *                 XPCOMGCFinalizedObject is MMgc::GCFinalizable
 * .xpcomgcfinalizedobject = true
 *                 if the class derives from XPCOMGCFinalizedObject
 */
var gClassMap = {};

function ClassType()
{
}

ClassType.prototype = {
  final: false,
  stack: false,
  gc: false,
  gcfinalizable: false,
  xpcomgcfinalizedobject: false
};

/* Set up primitive types to reduce warnings */

for each (t in ['bool',
                'char',
                'unsigned char',
                'signed char',
                'short int',
                'unsigned short int',
                'int',
                'unsigned int',
                'long int',
                'unsigned long int',
                'long long int',
                'unsigned long long int',
                'float',
                'double']) {
  gClassMap[t] = new ClassType();
}

function process_class(c) {
  var classattr, base, member, type, realtype;

  print("Processing class: '" + c.name + "'");

  if (gClassMap.hasOwnProperty(c.name)) {
    print("ERROR: Class name declared twice: " + c.name);
  }

  for each (base in c.bases) {
    if (!gClassMap.hasOwnProperty(base)) {
      print("ERROR: Class '" + c.name + "' has unknown base '" + base + "'.");
      return;
    }
  }

  function hasAttribute(attrname)
  {
    var attr;

    if (!c.attributes)
      return false;

    for each (attr in c.attributes) {
      if (attrname == attr) {
        return true;
      }
    }

    return false;
  }

  classattr = new ClassType();
  gClassMap[c.name] = classattr;

  // check for .final

  if (hasAttribute('final')) {
    classattr.final = true;
  }

  // check for .stack

  if (hasAttribute('stack')) {
    classattr.stack = true;
  }
  else {
    for each (base in c.bases) {
      if (gClassMap[base].stack) {
        classattr.stack = true;
        break;
      }
    }
  }

  if (!classattr.stack) {
    // Check members
    for each (member in c.members) {
      if (member.isFunction)
        continue;

      /* Ignore pointers and references and arrays of such */
      if (/[*&](\[\d*\]| const )?$/.test(member.type)) {
        continue;
      }

      /* Ignore function-pointer members */
      if (/\((\w::)?\*\)\(.*\)$/.test(member.type)) {
        continue;
      }
      
      type = /^((?:\w|:| |<.*>)+?)( const| volatile)?( (\[\d*\])+)?$/(member.type);
      if (!type) {
        print("WARNING: member type '" + member.type + "' unrecognized.");
        continue;
      }

      realtype = type[1];

      if (/^enum /.test(realtype)) {
        continue;
      }

      if (!gClassMap.hasOwnProperty(realtype)) {
        print("WARNING: type '" + realtype + "' wasn't in class map. Is it a primitive? Member: '" + member + "'");
        continue;
      }

      if (gClassMap[realtype].stack) {
        classattr.stack = true;
        break;
      }
    }
  }

  // check for .gc

  if (hasAttribute('GC')) {
    classattr.gc = true;
  }
  else if (c.name == "MMgc::GCObject" ||
           c.name == "MMgc::GCFinalizable" ||
           c.name == "XPCOMGCObject" ||
           c.name == "XPCOMGCFinalizedObject" ||
           /^nsCOMPtr/.test(c.name)) {
    classattr.gc = true;
  }
  else if (c.name == "nsIFrame") {
    // Do nothing, nsIFrame is a figment of your imagination
  }
  else {
    for each (base in c.bases) {
      if (gClassMap[base].gc) {
        classattr.gc = true;
        break;
      }
    }
  }

  // check for .gcfinalizable

  if (c.name == "MMgc::GCFinalizable") {
    classattr.gcfinalizable = true;
  }
  else {
    if (c.bases.length > 0 &&
        gClassMap[c.bases[0]].gcfinalizable) {
      classattr.gcfinalizable = true;
    }
    else if (c.bases.length > 1 &&
             c.bases[0] == "XPCOMGCFinalizedObject" &&
             gClassMap[c.bases[1]].gcfinalizable) {
      classattr.gcfinalizable = true;
    }
  }

  // check for .xpcomgcfinalizedobject

  if (c.name == "XPCOMGCFinalizedObject") {
    classattr.xpcomgcfinalizedobject = true;
  }
  else {
    for each (base in c.bases) {
      if (gClassMap[base].xpcomgcfinalizedobject) {
        classattr.xpcomgcfinalizedobject = true;
        break;
      }
    }
  }

  // print("Class '" + c.name + "' is " + classattr);

  // Check for errors at declaration-time

  for each (base in c.bases) {
    if (gClassMap[base].final) {
      print("ERROR: class '" + c.name + "' inherits from final class '" + base + "'.");
    }
  }

  if (classattr.gc && classattr.stack) {
    print("ERROR: class '" + c.name + "' must be GC and stack allocated?");
  }

  if (classattr.xpcomgcfinalizedobject &&
      !classattr.gcfinalizable &&
      c.name != "XPCOMGCFinalizedObject") {
    print("ERROR: class '" + c.name + "' is XPCOMGCFinalizedObject but not GCFinalizable.");
  }
}

function processVar(v) {
  // undeclared constructors return the type itself, not a reference
  // constructors will assign to the type directly as a fieldOf
  if (!v.isConstructor &&
      !v.fieldOf &&
      gClassMap.hasOwnProperty(v.type)) {
    if (gClassMap[v.type].gc) {
      print("ERROR: class '" + v.type + "' allocated on the stack." + v);
    }
  }

  if (v.name == "operator new") {
    var realtype = /^(.*) \*$/(v.type);
    if (!realtype) {
      print("ERROR: unexpected return type from operator new: '" + v.type + "'");
      return;
    }

    if (realtype[1] == 'void')
      return;

    if (!gClassMap.hasOwnProperty(realtype[1])) {
      print("WARNING: return type from operator new not in gClassMap: '" + realtype[1] + "'");
      return;
    }

    if (gClassMap[realtype[1]].stack) {
      print("Creating stack object '" + realtype[1] + "' via operator new.");
      return;
    }
  }
}

function iter_over_inits(vars)
{
  var v, va;

  for each(v in vars)
  {
    processVar(v);
    iter_over_inits(v.assign);
    iter_over_inits(v.arguments);
    for each (va in v.inits) {
      iter_over_inits(v.assign);
      iter_over_inits(v.arguments);
    }
  }
}
      
/** called by dehydra on every "statement" */
function process(vars, state) {
  // print("Processing: " + vars);
  iter_over_inits(vars);
}
