#include <jsapi.h>
#include <jsobj.h>
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
#include "builtins.h"
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
static const char *STATIC = "isStatic";
static const char *VIRTUAL = "isVirtual";

static int dehydra_loadScript(Dehydra *this, const char *filename);

static char *readFile(const char *filename, const char *dir, long *size) {
  char *buf;
  FILE *f = fopen(filename, "r");
  if (!f) {
    if (dir && *filename && filename[0] != '/') {
      buf = xmalloc(strlen(dir) + strlen(filename) + 2);
      sprintf(buf, "%s/%s", dir, filename);
      f = fopen(buf, "r");
      free(buf);
    }
    if (!f) {
      return NULL;
    }
  }
  xassert(!fseek(f, 0, SEEK_END));
  *size = ftell(f);
  xassert(!fseek(f, 0, SEEK_SET));
  buf = xmalloc(*size + 1);
  xassert(*size == fread(buf, 1, *size, f));
  buf[*size] = 0;
  fclose(f);
  return buf;
}


int dehydra_init(Dehydra *this, const char *file, const char *script) {
  static JSClass global_class = {
    "global", JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,  JS_PropertyStub,
    JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,
    JS_ConvertStub,   JS_FinalizeStub
  };

  static JSFunctionSpec shell_functions[] = {
    /* {"write_file",      WriteFile,      1},
       {"read_file",       ReadFile,       1},*/
    {"_print",           Print,          0},
    {"error",           Error,          0},
    {"warning",         Warning,          0},
    {"version",         Version,        0},
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
  JS_DefineFunctions (this->cx, this->globalObj, shell_functions);
  this->rootedArgDestArray = 
    this->destArray = JS_NewArrayObject (this->cx, 0, NULL);
  JS_AddRoot (this->cx, &this->rootedArgDestArray);
  // this is to be added at function_decl time
  this->statementHierarchyArray = NULL;
  
  //stateArray = JS_NewArrayObject(cx, 1, NULL);
  //xassert(stateArray && JS_AddRoot(cx, &stateArray));
  //state = JSVAL_VOID;

  //typeArrayArray = JS_NewArrayObject(cx, 0, NULL);
  //xassert(typeArrayArray && JS_AddRoot(cx, &typeArrayArray));
#ifdef DEBUG
  JS_SetGCZeal (this->cx, 2);
#endif
  JS_SetVersion (this->cx, (JSVersion) 170);
  //  loadScript(libScript);
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

static int dehydra_loadScript (Dehydra *this, const char *filename) {
  long size = 0;
  char *buf = readFile (filename, this->dir, &size);
  if (!buf) {
    error ("Could not load dehydra script: %s", filename);
    return 1;
  }
  jsval rval;
  xassert (JS_EvaluateScript (this->cx, this->globalObj, buf, size,
                              filename, 1, &rval));
  return 0;
}

jsval dehydra_getCallback(Dehydra *this, char const *name) {
  jsval val = JSVAL_VOID;
  return (JS_GetProperty(this->cx, this->globalObj, name, &val)
          && val != JSVAL_VOID
          && JS_TypeOfValue(this->cx, val) == JSTYPE_FUNCTION) ? val : JSVAL_VOID;
}

static void dehydra_setLoc(Dehydra *this, JSObject *obj, tree t) {
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
    JSObject *obj = JS_NewObject(this->cx, &js_ObjectClass, 0, 0);
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

JSObject* dehydra_addVar (Dehydra *this, tree v, JSObject *parentArray) {
  if (!parentArray) parentArray = this->destArray;
  unsigned int length = dehydra_getArrayLength (this, parentArray);
  JSObject *obj = JS_ConstructObject (this->cx, &js_ObjectClass, NULL, 
                                      this->globalObj);
  //append object to array(rooting it)
  JS_DefineElement (this->cx, parentArray, length,
                    OBJECT_TO_JSVAL(obj),
                    NULL, NULL, JSPROP_ENUMERATE);
  if (!v) return obj;
  xassert (DECL_P (v));
  char const *name = decl_as_string (v, 0);
  dehydra_defineStringProperty (this, obj, NAME, 
                                name);
  if (TREE_CODE (v) == FUNCTION_DECL) {
    dehydra_defineProperty (this, obj, FUNCTION, JSVAL_TRUE);  
    if (DECL_CONSTRUCTOR_P (v))
      dehydra_defineProperty (this, obj, DH_CONSTRUCTOR, JSVAL_TRUE);
    else
      dehydra_defineStringProperty (this, obj, TYPE, 
                                    type_as_string (TREE_TYPE (TREE_TYPE (v)), 0));
    if (DECL_PURE_VIRTUAL_P (v))
      dehydra_defineStringProperty (this, obj, VIRTUAL, "pure");
    else if (DECL_VIRTUAL_P (v))
      dehydra_defineProperty (this, obj, VIRTUAL, JSVAL_TRUE);
  }
  tree typ = TREE_TYPE (v);
  dehydra_defineProperty (this, obj, TYPE, 
                          dehydra_convertType (this, typ));
  tree attributes = DECL_ATTRIBUTES (v);
  if (attributes) {
    JSObject *tmp = JS_NewArrayObject (this->cx, 0, NULL);
    dehydra_defineProperty (this, obj, ATTRIBUTES, OBJECT_TO_JSVAL (tmp));
    dehydra_addAttributes (this, obj, attributes);
  }
  if (TREE_STATIC (v))
    dehydra_defineProperty (this, obj, STATIC, JSVAL_TRUE);

  dehydra_setLoc(this, obj, v);
  return obj;
}

int dehydra_visitClass (Dehydra *this, tree c) {
  jsval process_class = dehydra_getCallback(this, "process_class");
  if (process_class == JSVAL_VOID) return true;
  
  jsval rval, argv[1];
  argv[0] = dehydra_convertType (this, c);
  xassert (JS_CallFunctionValue (this->cx, this->globalObj, process_class,
                                 1, argv, &rval));
  return true;
}

static void dehydra_visitFunctionDecl (Dehydra *this, tree f) {
  jsval process_function = dehydra_getCallback (this, "process_function");
  if (process_function == JSVAL_VOID) return;

  void **v = pointer_map_contains(this->fndeclMap, f);
  if (!v) {
    /*fprintf (stderr, "%s: ", loc_as_string (location_of (f)));
      fprintf (stderr, "No body for %s\n", decl_as_string (f, 0));*/
    return;
  }
  this->statementHierarchyArray = (JSObject*) *v;
  
  /* temporarily add a variable to this->statementHierarchyArray;
   this way is it is rooted while various properties are defined */
  unsigned int length = dehydra_getArrayLength (this,
                                                this->statementHierarchyArray);
  JSObject *fobj = dehydra_addVar (this, f, this->statementHierarchyArray);
  /* hopefully reducing size of an array does not trigger gc */
  dehydra_defineProperty (this, this->globalObj, "_function",
                          OBJECT_TO_JSVAL (fobj));
  JS_SetArrayLength (this->cx, this->statementHierarchyArray, length);
  jsval rval, argv[2];
  argv[0] = OBJECT_TO_JSVAL (fobj);
  argv[1] = OBJECT_TO_JSVAL (this->statementHierarchyArray);
  // the array is now rooted as a function argument
  JS_RemoveRoot (this->cx, &this->statementHierarchyArray);
  xassert (JS_CallFunctionValue (this->cx, this->globalObj, process_function,
                                 sizeof (argv)/sizeof (argv[0]), argv, &rval));
  JS_DeleteProperty (this->cx, this->globalObj, "_function");
  return;
}

static void dehydra_visitVarDecl (Dehydra *this, tree d) {
  jsval process_var = dehydra_getCallback (this, "process_var");
  if (process_var == JSVAL_VOID) return;

  unsigned int length = dehydra_getArrayLength (this, this->rootedArgDestArray);
  JSObject *obj = dehydra_addVar (this, d, this->rootedArgDestArray);
  jsval rval, argv[1];
  argv[0] = OBJECT_TO_JSVAL (obj);
  JS_RemoveRoot (this->cx, &this->statementHierarchyArray);
  xassert (JS_CallFunctionValue (this->cx, this->globalObj, process_var,
                                 sizeof (argv)/sizeof (argv[0]), argv, &rval));
  JS_SetArrayLength (this->cx, this->rootedArgDestArray, length);
}

void dehydra_visitDecl (Dehydra *this, tree d) {
  if (TREE_CODE (d) == FUNCTION_DECL) {
    if (DECL_SAVED_TREE(d))
      dehydra_visitFunctionDecl (this, d);
  } else {
    dehydra_visitVarDecl (this, d);
  }
}

/* Creates next array to dump dehydra objects onto */
void dehydra_nextStatement(Dehydra *this, location_t loc) {
  unsigned int length = dehydra_getArrayLength (this,
                                                this->statementHierarchyArray);
  xassert (!this->inExpr);
  this->loc = loc;
  this->destArray = NULL;
  JSObject *obj = NULL;
  /* Check that the last statement array was used, otherwise reuse it */
  if (length) {
    jsval val;
    JS_GetElement (this->cx, this->statementHierarchyArray, length - 1,
                   &val);
    obj = JSVAL_TO_OBJECT (val);
    JS_GetProperty (this->cx, obj, STATEMENTS, &val);
    this->destArray = JSVAL_TO_OBJECT (val);
    int destLength = dehydra_getArrayLength (this, this->destArray);
    /* last element is already setup & empty, we are done */
    if (destLength != 0) {
      this->destArray = NULL;
    }
  }
  if (!this->destArray) {
    obj = JS_NewObject (this->cx, &js_ObjectClass, 0, 0);
    JS_DefineElement (this->cx, this->statementHierarchyArray, length,
                      OBJECT_TO_JSVAL(obj),
                      NULL, NULL, JSPROP_ENUMERATE);
    this->destArray = JS_NewArrayObject(this->cx, 0, NULL);
    dehydra_defineProperty (this, obj, STATEMENTS, 
                            OBJECT_TO_JSVAL (this->destArray));
  }
  /* always update location */
  if (this->loc) {
    const char *loc_str = loc_as_string (this->loc);
    const char *s = strrchr (loc_str, '/');
    if (s) loc_str = s + 1;
    dehydra_defineStringProperty (this, obj, LOC, loc_str);
  } else {
    dehydra_defineProperty (this, obj, LOC, JSVAL_VOID);
  }
}

void dehydra_print(Dehydra *this, JSObject *obj) {
  jsval print = dehydra_getCallback(this, "print");
  jsval rval, argv[1];
  argv[0] = OBJECT_TO_JSVAL(obj);
  xassert (JS_CallFunctionValue(this->cx, this->globalObj, print,
                               1, argv, &rval));
}

void dehydra_input_end (Dehydra *this) {
  jsval input_end = dehydra_getCallback(this, "input_end");
  if (input_end == JSVAL_VOID) return;
  
  jsval rval;
  xassert (JS_CallFunctionValue(this->cx, this->globalObj, input_end,
                               0, NULL, &rval));
}
