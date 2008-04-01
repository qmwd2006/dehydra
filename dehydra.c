#include <jsapi.h>
#include <unistd.h>
#include <stdio.h>

#include <config.h>
#include <system.h>
#include <coretypes.h>
#include <tm.h>
#include <tree.h>
#include <cp-tree.h>
#include <cxx-pretty-print.h>
#include <tree-iterator.h>
#include <pointer-set.h>
#include <toplev.h>

#include "xassert.h"
#include "dehydra_builtins.h"
#include "util.h"
#include "dehydra.h"
#include "dehydra_types.h"

const char *NAME = "name";
const char *LOC = "loc";
const char *BASES = "bases";
const char *DECL = "isDecl";
const char *ASSIGN = "assign";
const char *VALUE = "value";
const char *TYPE = "type";
const char *FUNCTION = "isFunction";
const char *RETURN = "isReturn";
const char *FCALL = "isFcall";
const char *ARGUMENTS = "arguments";
const char *DH_CONSTRUCTOR = "isConstructor";
const char *FIELD_OF = "fieldOf";
const char *MEMBERS = "members";
const char *PARAMETERS = "parameters";
const char *ATTRIBUTES = "attributes";
const char *STATEMENTS = "statements";
const char *BITFIELD = "bitfieldBits";
const char *METHOD_OF = "methodOf";
static const char *STATIC = "isStatic";
static const char *VIRTUAL = "isVirtual";

#ifdef JS_GC_ZEAL
static JSBool
GCZeal(JSContext *cx, uintN argc, jsval *vp)
{
    uintN zeal;

    if (!JS_ValueToECMAUint32(cx, vp[2], &zeal))
        return JS_FALSE;
    JS_SetGCZeal(cx, zeal);
    *vp = JSVAL_VOID;
    return JS_TRUE;
}
#endif /* JS_GC_ZEAL */

void dehydra_init(Dehydra *this, const char *file) {
  static JSClass global_class = {
    "global", JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,  JS_PropertyStub,
    JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,
    JS_ConvertStub,   JS_FinalizeStub
  };

  static JSFunctionSpec shell_functions[] = {
    {"_print",          Print,          0},
    {"include",         Include,        1},
    {"write_file",      WriteFile,      1},
    {"read_file",       ReadFile,       1},
    {"error",           Error,          0},
    {"warning",         Warning,        0},
    {"version",         Version,        0},
#ifdef JS_GC_ZEAL
    JS_FN("gczeal",     GCZeal,     1,1,0),
#endif
    {0}
  };

  char *c = strrchr(file, '/');
  if (c) {
    // include / in the copy
    int len = c - file + 1;
    this->dir = xmalloc(len + 1);
    strncpy(this->dir, file, len);
    this->dir[len] = 0;
  }

  this->fndeclMap = pointer_map_create ();
  this->rt = JS_NewRuntime (0x9000000L);
  this->cx = JS_NewContext (this->rt, 8192);

  JS_SetContextPrivate (this->cx, this);
  
  this->globalObj = JS_NewObject (this->cx, &global_class, 0, 0);
  JS_InitStandardClasses (this->cx, this->globalObj);
  /* register error handler */
  JS_SetErrorReporter (this->cx, ReportError);
  xassert (JS_DefineFunctions (this->cx, this->globalObj, shell_functions));
  if (dehydra_getToplevelFunction(this, "error") == JSVAL_VOID) {
    fprintf (stderr, "Your version of spidermonkey has broken JS_DefineFunctions, upgrade it or ./configure with another version\n");
    exit(1);
  }
  this->rootedArgDestArray = 
    JS_NewArrayObject (this->cx, 0, NULL);
  JS_AddRoot (this->cx, &this->rootedArgDestArray);
  // this is to be added at function_decl time
  this->rootedFreeArray = JS_NewArrayObject (this->cx, 0, NULL);
  JS_DefineElement (this->cx, this->rootedArgDestArray, 0,
                    OBJECT_TO_JSVAL (this->rootedFreeArray),
                    NULL, NULL, JSPROP_ENUMERATE);
  JS_SetVersion (this->cx, (JSVersion) 170);
  if (aux_base_name) {
    jsval strval = STRING_TO_JSVAL (JS_NewStringCopyZ (this->cx, 
                                                       dump_base_name));
    dehydra_defineProperty (this, this->globalObj, "aux_base_name", strval);
  }
  xassert (JS_DefineFunction (this->cx, JS_GetPrototype(this->cx, this->globalObj),
                              "hashcode", obj_hashcode, 0, 0));
}

int dehydra_startup (Dehydra *this, const char *script) {
  if (dehydra_loadScript (this, "system.js")) return 1;
  return dehydra_loadScript (this, script);
}


jsuint dehydra_getArrayLength (Dehydra *this, JSObject *array) {
  jsuint length = 0;
  xassert (JS_GetArrayLength (this->cx, array, &length));
  return length;
}

void dehydra_defineProperty (Dehydra *this, JSObject *obj,
                             char const *name, jsval value)
{
  JS_DefineProperty (this->cx, obj, name, value,
                     NULL, NULL, JSPROP_ENUMERATE);
}

void dehydra_defineStringProperty (Dehydra *this, JSObject *obj,
                                   char const *name, char const *value)
{
  JSString *str = JS_NewStringCopyZ (this->cx, value);
  dehydra_defineProperty (this, obj, name, STRING_TO_JSVAL(str));
}

JSObject *dehydra_defineArrayProperty (Dehydra *this, JSObject *obj,
                                       char const *name, int length) {
  JSObject *destArray = JS_NewArrayObject (this->cx, length, NULL);
  dehydra_defineProperty (this, obj, name, OBJECT_TO_JSVAL (destArray));
  return destArray;  
}

JSObject *dehydra_defineObjectProperty (Dehydra *this, JSObject *obj,
                                       char const *name) {
  return JS_DefineObject(this->cx, obj,
                  name, NULL, NULL,
                  JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
}

/* Load and execute a Javascript file. 
 * Return:    0 on success
 *            1 on failure if a Javascript exception is pending
 *            does not return if a Javascript error is reported
 * The general behavior of (De|Tree)hydra is to print a message and
 * exit if a JS error is reported at the top level. But if this function
 * is called from JS, then the JS_* functions will instead set an
 * exception, which will be propgated back up to the callers for
 * eventual handling or exit. */
int dehydra_loadScript (Dehydra *this, const char *filename) {
  /* Read the file. There's a JS function for this, but Dehydra
     wants to search for the file in different dirs. */
  long size = 0;
  char *buf = readFile (filename, this->dir, &size);
  if (!buf) {
    fprintf (stderr, "Dehydra error: failed loading script file: %s\n", 
             filename);
    exit(1);    /* To match ReportError behavior */
  }

  JSScript *script = JS_CompileScript(this->cx, this->globalObj,
                                      buf, size, filename, 1);
  if (script == NULL) {
    xassert(JS_IsExceptionPending(this->cx));
    return 1;
  }

  JSObject *sobj = JS_NewScriptObject(this->cx, script);
  JS_AddNamedRoot(this->cx, &sobj, filename);
  jsval rval;
  JSBool rv = JS_ExecuteScript(this->cx, this->globalObj, script, &rval);
  if (!rv) {
    xassert(JS_IsExceptionPending(this->cx));
    return 1;
  }

  JS_RemoveRoot(this->cx, &sobj);
  return 0;
}

jsval dehydra_getToplevelFunction(Dehydra *this, char const *name) {
  jsval val = JSVAL_VOID;
  return (JS_GetProperty(this->cx, this->globalObj, name, &val)
          && val != JSVAL_VOID
          && JS_TypeOfValue(this->cx, val) == JSTYPE_FUNCTION) ? val : JSVAL_VOID;
}

void dehydra_setLoc(Dehydra *this, JSObject *obj, tree t) {
  location_t loc = location_of (t);
  if (!loc || loc == UNKNOWN_LOCATION) return;

  char const *strLoc = loc_as_string (loc);
  /* Don't attach empty locations */
  if (strLoc && *strLoc)
    dehydra_defineStringProperty (this, obj, LOC, strLoc);
}

void dehydra_addAttributes (Dehydra *this, JSObject *destArray,
                            tree attributes) {
  int i = 0;
  tree a;
  for (a = attributes; a; a = TREE_CHAIN (a)) {
    tree name = TREE_PURPOSE (a);
    tree args = TREE_VALUE (a);
    JSObject *obj = JS_NewObject(this->cx, NULL, 0, 0);
    JS_DefineElement (this->cx, destArray, i++,
                      OBJECT_TO_JSVAL (obj),
                      NULL, NULL, JSPROP_ENUMERATE);
    dehydra_defineStringProperty (this, obj, NAME, IDENTIFIER_POINTER (name));
    JSObject *array = JS_NewArrayObject (this->cx, 0, NULL);
    dehydra_defineProperty (this, obj, VALUE, OBJECT_TO_JSVAL (array));
    int i = 0;
    for (; args; args = TREE_CHAIN (args)) {
      tree t = TREE_VALUE (args);
      const char *val = TREE_CODE (t) == STRING_CST 
        ? TREE_STRING_POINTER (t)
        : expr_as_string (t, 0);
      JSString *str = 
        JS_NewStringCopyZ(this->cx, val);
      JS_DefineElement(this->cx, array, i++, 
                       STRING_TO_JSVAL(str),
                       NULL, NULL, JSPROP_ENUMERATE);
    }
  }
}

/* Add a Dehydra variable to the given parent array corresponding to
 * the GCC tree v, which must represent a declaration (e.g., v can
 * be a DECL_*. */
JSObject* dehydra_addVar (Dehydra *this, tree v, JSObject *parentArray) {
  if (!parentArray) parentArray = this->destArray;
  unsigned int length = dehydra_getArrayLength (this, parentArray);
  JSObject *obj = JS_ConstructObject (this->cx, NULL, NULL, 
                                      this->globalObj);
  //append object to array(rooting it)
  JS_DefineElement (this->cx, parentArray, length,
                    OBJECT_TO_JSVAL(obj),
                    NULL, NULL, JSPROP_ENUMERATE);
  if (!v) return obj;
  if (DECL_P(v)) {
    /* Common case */
    char const *name = decl_as_string (v, 0);
    dehydra_defineStringProperty (this, obj, NAME, 
                                  name);
    tree typ = TREE_TYPE (v);
    if (TREE_CODE (v) == FUNCTION_DECL) {
      dehydra_defineProperty (this, obj, FUNCTION, JSVAL_TRUE);  
      if (DECL_CONSTRUCTOR_P (v))
        dehydra_defineProperty (this, obj, DH_CONSTRUCTOR, JSVAL_TRUE);

      if (TREE_CODE (typ) == METHOD_TYPE || DECL_CONSTRUCTOR_P (v)) {
        dehydra_defineProperty (this, obj, METHOD_OF, 
                                dehydra_convertType (this, DECL_CONTEXT (v)));
      }

      if (DECL_PURE_VIRTUAL_P (v))
        dehydra_defineStringProperty (this, obj, VIRTUAL, "pure");
      else if (DECL_VIRTUAL_P (v))
        dehydra_defineProperty (this, obj, VIRTUAL, JSVAL_TRUE);
    }
    dehydra_defineProperty (this, obj, TYPE, 
                            dehydra_convertType (this, typ));
    tree attributes = DECL_ATTRIBUTES (v);
    if (attributes) {
      JSObject *tmp = JS_NewArrayObject (this->cx, 0, NULL);
      dehydra_defineProperty (this, obj, ATTRIBUTES, OBJECT_TO_JSVAL (tmp));
      dehydra_addAttributes (this, tmp, attributes);
    }
    if (TREE_STATIC (v))
      dehydra_defineProperty (this, obj, STATIC, JSVAL_TRUE);
  } else if (TREE_CODE(v) == CONSTRUCTOR) {
    /* Special case for this node type */
    tree type = TREE_TYPE(v);
    dehydra_defineStringProperty(this, obj, NAME, 
                                 type_as_string(type, 0));
    dehydra_defineProperty(this, obj, DH_CONSTRUCTOR, JSVAL_TRUE);
    dehydra_defineProperty(this, obj, METHOD_OF, 
                           dehydra_convertType(this, type));
  } else {
    /* Invalid argument tree code */
    xassert(0);
  }

  dehydra_setLoc(this, obj, v);
  return obj;
}

int dehydra_visitType (Dehydra *this, tree t) {
  jsval process_type = dehydra_getToplevelFunction(this, "process_type");
  if (process_type == JSVAL_VOID) return true;
  
  jsval rval, argv[1];
  argv[0] = dehydra_convertType (this, t);
  xassert (JS_CallFunctionValue (this->cx, this->globalObj, process_type,
                                 1, argv, &rval));
  return true;
}

static void dehydra_visitFunctionDecl (Dehydra *this, tree f) {
  jsval process_function = dehydra_getToplevelFunction (this, "process_function");
  if (process_function == JSVAL_VOID) return;

  void **v = pointer_map_contains (this->fndeclMap, f);
  if (!v) {
    return;
  }
  int key = (int) *v;
  this->statementHierarchyArray = 
    JSVAL_TO_OBJECT (dehydra_getRootedObject (this, key));
  *v = NULL;
  
  int fnkey = dehydra_getArrayLength (this,
                                      this->rootedArgDestArray);
  JSObject *fobj = dehydra_addVar (this, f, this->rootedArgDestArray);
  jsval rval, argv[2];
  argv[0] = OBJECT_TO_JSVAL (fobj);
  argv[1] = OBJECT_TO_JSVAL (this->statementHierarchyArray);
  xassert (JS_CallFunctionValue (this->cx, this->globalObj, process_function,
                                 sizeof (argv)/sizeof (argv[0]), argv, &rval));
  dehydra_unrootObject (this, key);
  dehydra_unrootObject (this, fnkey);
  this->statementHierarchyArray = NULL;
}

static void dehydra_visitVarDecl (Dehydra *this, tree d) {
  jsval process_var = dehydra_getToplevelFunction (this, "process_var");
  if (process_var == JSVAL_VOID) return;

  /* this is a hack,basically does dehydra_rootObject manually*/
  int key = dehydra_getArrayLength (this, this->rootedArgDestArray);
  JSObject *obj = dehydra_addVar (this, d, this->rootedArgDestArray);
  jsval rval, argv[1];
  argv[0] = OBJECT_TO_JSVAL (obj);
  xassert (JS_CallFunctionValue (this->cx, this->globalObj, process_var,
                                 sizeof (argv)/sizeof (argv[0]), argv, &rval));
  dehydra_unrootObject (this, key);
}

int dehydra_rootObject (Dehydra *this, jsval val) {
  /* positions start from 1 since rootedFreeArray is always the first element */
  int pos;
  int length = dehydra_getArrayLength (this, this->rootedFreeArray);
  if (length) {
    jsval val;
    length--;
    JS_GetElement(this->cx, this->rootedFreeArray, length, &val);
    JS_SetArrayLength (this->cx, this->rootedFreeArray, length);
    pos = JSVAL_TO_INT (val);
  } else {
    pos = dehydra_getArrayLength (this, this->rootedArgDestArray);
  }
  JS_DefineElement (this->cx, this->rootedArgDestArray, pos,
                    val, NULL, NULL, JSPROP_ENUMERATE);
  return pos;
}

void dehydra_unrootObject (Dehydra *this, int pos) {
  int length = dehydra_getArrayLength (this, this->rootedFreeArray);
  JS_DefineElement (this->cx, this->rootedFreeArray, length,
                    INT_TO_JSVAL (pos),
                    NULL, NULL, JSPROP_ENUMERATE);
  JS_DefineElement (this->cx, this->rootedArgDestArray, pos,
                    JSVAL_VOID,
                    NULL, NULL, JSPROP_ENUMERATE);

}

jsval dehydra_getRootedObject (Dehydra *this, int pos) {
  jsval v = JSVAL_VOID;
  JS_GetElement(this->cx, this->rootedArgDestArray, pos, &v);
  return v;
}

void dehydra_visitDecl (Dehydra *this, tree d) {
  if (TREE_CODE (d) == FUNCTION_DECL) {
    if (DECL_SAVED_TREE(d))
      dehydra_visitFunctionDecl (this, d);
  } else {
    dehydra_visitVarDecl (this, d);
  }
}

void dehydra_print(Dehydra *this, jsval arg) {
  jsval print = dehydra_getToplevelFunction(this, "user_print");
  if (print == JSVAL_VOID) {
    fprintf(stderr, "function user_print() not defined in JS\n");
    return;
  }
  jsval rval;
  xassert (JS_CallFunctionValue(this->cx, this->globalObj, print,
                               1, &arg, &rval));
}

void dehydra_input_end (Dehydra *this) {
  jsval input_end = dehydra_getToplevelFunction(this, "input_end");
  if (input_end == JSVAL_VOID) return;
  
  jsval rval;
  xassert (JS_CallFunctionValue(this->cx, this->globalObj, input_end,
                               0, NULL, &rval));
}
