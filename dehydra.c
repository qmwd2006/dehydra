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
  xassert((this->rt = JS_NewRuntime(0x9000000L)));
  xassert((this->cx = JS_NewContext(this->rt, 8192)));

  JS_SetContextPrivate(this->cx, this);
  
  this->globalObj = JS_NewObject(this->cx, &global_class, 0, 0);
  JS_InitStandardClasses(this->cx, this->globalObj);
  /* register error handler */
  JS_SetErrorReporter(this->cx, ReportError);
  xassert(JS_DefineFunctions(this->cx, this->globalObj, shell_functions));
  this->rootedArgDestArray = 
    this->destArray = JS_NewArrayObject(this->cx, 0, NULL);
  xassert(this->destArray && JS_AddRoot(this->cx, &this->rootedArgDestArray));
  // this is to be added at function_decl time
  this->statementHierarchyArray = NULL;
  
  //stateArray = JS_NewArrayObject(cx, 1, NULL);
  //xassert(stateArray && JS_AddRoot(cx, &stateArray));
  //state = JSVAL_VOID;

  //typeArrayArray = JS_NewArrayObject(cx, 0, NULL);
  //xassert(typeArrayArray && JS_AddRoot(cx, &typeArrayArray));

  JS_SetVersion(this->cx, (JSVersion) 170);
  //  loadScript(libScript);
  if (dehydra_loadScript (this, "system.js")) return 1;
  return dehydra_loadScript (this, script);
}

jsuint dehydra_getArrayLength(Dehydra *this, JSObject *array) {
  jsuint length = 0;
  xassert(JS_GetArrayLength(this->cx, array, &length));
  return length;
}

void dehydra_defineProperty(Dehydra *this, JSObject *obj,
                       char const *name, jsval value)
{
  xassert(JS_DefineProperty(this->cx, obj, name, value,
                            NULL, NULL, JSPROP_ENUMERATE));
}

void dehydra_defineStringProperty(Dehydra *this, JSObject *obj,
                             char const *name, char const *value)
{
  JSString *str = JS_NewStringCopyZ(this->cx, value);
  xassert(str);
  dehydra_defineProperty(this, obj, name, STRING_TO_JSVAL(str));
}

static int dehydra_loadScript(Dehydra *this, const char *filename) {
  long size = 0;
  char *buf = readFile(filename, this->dir, &size);
  if (!buf) {
    error ("Could not load dehydra script: %s", filename);
    return 1;
  }
  jsval rval;
  xassert(JS_EvaluateScript(this->cx, this->globalObj, buf, size,
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

JSObject* dehydra_addVar(Dehydra *this, tree v, JSObject *parentArray) {
  if (!parentArray) parentArray = this->destArray;
  unsigned int length = dehydra_getArrayLength(this, parentArray);
  JSObject *obj = JS_ConstructObject(this->cx, &js_ObjectClass, NULL, 
                                     this->globalObj);
  //append object to array(rooting it)
  xassert(obj && JS_DefineElement(this->cx, parentArray, length,
                                  OBJECT_TO_JSVAL(obj),
                                  NULL, NULL, JSPROP_ENUMERATE));
  if (TYPE_P(v)) {
    char const *name = type_as_string (v, 0);
    dehydra_defineStringProperty (this, obj, NAME, 
                                  name);
  } else if (DECL_P(v)) {
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
    }
    tree typ = TREE_TYPE (v);
    /*tree type_name = TYPE_NAME (typ);*/
    dehydra_defineProperty (this, obj, TYPE, 
                            dehydra_convertType (this, typ));
  }
  dehydra_setLoc(this, obj, v);
  return obj;
}

void dehydra_addAttributes (Dehydra *this, JSObject *destArray,
                            tree attributes) {
  int i = 0;
  tree a;
  for (a = attributes; a; a = TREE_CHAIN (a)) {
    tree name = TREE_PURPOSE (a);
    tree args = TREE_VALUE (a);
    JSObject *obj = JS_NewObject(this->cx, &js_ObjectClass, 0, 0);
    JS_DefineElement (this->cx, this->destArray, i++,
                      OBJECT_TO_JSVAL (obj),
                      NULL, NULL, JSPROP_ENUMERATE);
    dehydra_defineStringProperty (this, obj, NAME, IDENTIFIER_POINTER (name));
    JSObject *array = JS_NewArrayObject (this->cx, 0, NULL);
    dehydra_defineProperty (this, obj, VALUE, OBJECT_TO_JSVAL (array));
    int i = 0;
    for (; args; args = TREE_CHAIN (args)) {
      const char *val = TREE_STRING_POINTER (TREE_VALUE (args));
      JSString *str = 
        JS_NewStringCopyZ(this->cx, val);
      xassert(JS_DefineElement(this->cx, array, i++, 
                               STRING_TO_JSVAL(str),
                               NULL, NULL, JSPROP_ENUMERATE));
    }
  }
}


int dehydra_visitClass (Dehydra *this, tree c) {
  jsval process_class = dehydra_getCallback(this, "process_class");
  if (process_class == JSVAL_VOID) return true;

  unsigned int length = dehydra_getArrayLength (this, this->rootedArgDestArray);

  JSObject *objClass = dehydra_addVar (this, c, this->rootedArgDestArray);

  this->destArray = JS_NewArrayObject (this->cx, 0, NULL);
  dehydra_defineProperty (this, objClass, BASES, 
                         OBJECT_TO_JSVAL(this->destArray));

  tree binfo = TYPE_BINFO (c);
  int n_baselinks = BINFO_N_BASE_BINFOS (binfo);
  int i;
  for (i = 0; i < n_baselinks; i++)
    {
      tree base_binfo = BINFO_BASE_BINFO(binfo, i);
      JSString *str = 
        JS_NewStringCopyZ(this->cx, type_as_string(BINFO_TYPE(base_binfo),0));
      xassert(JS_DefineElement(this->cx, this->destArray, i, 
                               STRING_TO_JSVAL(str),
                               NULL, NULL, JSPROP_ENUMERATE));
    }
  
  this->destArray = JS_NewArrayObject(this->cx, 0, NULL);
  dehydra_defineProperty(this, objClass, MEMBERS,
                         OBJECT_TO_JSVAL(this->destArray));
  tree func;
  /* Output all the method declarations in the class.  */
  for (func = TYPE_METHODS (c) ; func ; func = TREE_CHAIN (func)) {
    if (DECL_ARTIFICIAL(func)) continue;
    /* Don't output the cloned functions.  */
    if (DECL_CLONED_FUNCTION_P (func)) continue;
    dehydra_addVar (this, func, this->destArray);
  }

  tree field;
  for (field = TYPE_FIELDS (c) ; field ; field = TREE_CHAIN (field)) {
    if (DECL_ARTIFICIAL(field) && !DECL_IMPLICIT_TYPEDEF_P(field)) continue;
    // ignore typedef of self field
    // my theory is that the guard above takes care of this one too
    if (TREE_CODE (field) == TYPE_DECL 
        && TREE_TYPE (field) == c) continue;
    if (TREE_CODE (field) != FIELD_DECL) continue;
    JSObject *obj = dehydra_addVar (this, field, this->destArray);
    if (DECL_BIT_FIELD_TYPE (field))
      {
        tree size_tree = DECL_SIZE (field);
        if (size_tree && host_integerp (size_tree, 1))
          {
            unsigned HOST_WIDE_INT bits = tree_low_cst(size_tree, 1);
            char buf[100];
            sprintf (buf, "%lu", bits);
            dehydra_defineStringProperty (this, obj, BITFIELD, buf);
          }
      }

  }

  this->destArray = JS_NewArrayObject (this->cx, 0, NULL);
  dehydra_defineProperty (this, objClass, ATTRIBUTES,
                          OBJECT_TO_JSVAL (this->destArray));
  /* first add attributes from template */
  tree decl_template_info = TYPE_TEMPLATE_INFO (c);
  if (decl_template_info) {
    tree template_decl = TREE_PURPOSE (decl_template_info);
    tree record_type = TREE_TYPE (template_decl);
    tree attributes = TYPE_ATTRIBUTES (record_type);
    dehydra_addAttributes (this, this->destArray, attributes);
  }
  tree attributes = TYPE_ATTRIBUTES (c);
  dehydra_addAttributes (this, this->destArray, attributes);
  /* drop the attributes array if there are none */
  if (! dehydra_getArrayLength (this, this->destArray)) {
    JS_DeleteProperty (this->cx, objClass, ATTRIBUTES);
  }

  this->destArray = NULL;
  jsval rval, argv[1];
  argv[0] = OBJECT_TO_JSVAL(objClass);
  xassert(JS_CallFunctionValue(this->cx, this->globalObj, process_class,
                               1, argv, &rval));
  xassert (JS_SetArrayLength(this->cx, this->rootedArgDestArray, length));
  return true;
}

int dehydra_visitFunction (Dehydra *this, tree f) {
  jsval process_function = dehydra_getCallback (this, "process_function");
  if (process_function == JSVAL_VOID) return true;

  void **v = pointer_map_contains(this->fndeclMap, f);
  if (!v) {
    /*fprintf (stderr, "%s: ", loc_as_string (location_of (f)));
      fprintf (stderr, "No body for %s\n", decl_as_string (f, 0));*/
    return true;
  }
  this->statementHierarchyArray = (JSObject*) *v;
  
  /* temporarily add a variable to this->statementHierarchyArray;
   this way is it is rooted while various properties are defined */
  unsigned int length = dehydra_getArrayLength (this,
                                                this->statementHierarchyArray);
  JSObject *fobj = dehydra_addVar (this, f, this->statementHierarchyArray);
  /* hopefully reducing size of an array does not trigger gc */
  /* TODO setup a temp rooted jsval */
  xassert (JS_SetArrayLength (this->cx, this->statementHierarchyArray, length));
  jsval rval, argv[2];
  argv[0] = OBJECT_TO_JSVAL (fobj);
  argv[1] = OBJECT_TO_JSVAL (this->statementHierarchyArray);
  // the array is now rooted as a function argument
  xassert (JS_RemoveRoot (this->cx, &this->statementHierarchyArray));
  xassert (JS_CallFunctionValue (this->cx, this->globalObj, process_function,
                                 sizeof (argv)/sizeof (argv[0]), argv, &rval));
  return true;
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
    xassert (JS_GetElement (this->cx, this->statementHierarchyArray, length - 1,
                          &val));
    obj = JSVAL_TO_OBJECT (val);
    xassert (JS_GetProperty(this->cx, obj, STATEMENTS, &val));
    this->destArray = JSVAL_TO_OBJECT (val);
    int destLength = dehydra_getArrayLength (this, this->destArray);
    /* last element is already setup & empty, we are done */
    if (destLength != 0) {
      this->destArray = NULL;
    }
  }
  if (!this->destArray) {
    obj = JS_NewObject(this->cx, &js_ObjectClass, 0, 0);
    xassert(obj
            && JS_DefineElement(this->cx, this->statementHierarchyArray, length,
                                OBJECT_TO_JSVAL(obj),
                                NULL, NULL, JSPROP_ENUMERATE));
    this->destArray = JS_NewArrayObject(this->cx, 0, NULL);
    xassert (this->destArray);
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
  xassert(JS_CallFunctionValue(this->cx, this->globalObj, print,
                               1, argv, &rval));
}

void dehydra_input_end (Dehydra *this) {
  jsval input_end = dehydra_getCallback(this, "input_end");
  if (input_end == JSVAL_VOID) return;
  
  jsval rval;
  xassert(JS_CallFunctionValue(this->cx, this->globalObj, input_end,
                               0, NULL, &rval));
}
