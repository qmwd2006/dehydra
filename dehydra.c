#include "dehydra.h"
#include <jsapi.h>
#include <jsobj.h>
#include <unistd.h>
#include <stdio.h>
#include "cp-tree.h"
#include "cxx-pretty-print.h"
#include "tree-iterator.h"
#include "pointer-set.h"

struct Dehydra {
  char* dir;
  JSRuntime *rt;
  JSContext *cx;
  JSObject *globalObj;
  JSObject *destArray;
  JSObject *rootedArgDestArray;
  // Where the statements go;
  JSObject *statementHierarchyArray;
  //keeps track of function decls to map gimplified ones to verbose ones
  struct pointer_map_t *fndeclMap;
  /* last var added by dehydra_addVar */
  JSObject *lastVar;
  location_t loc;
};

typedef struct Dehydra Dehydra;

static const char *NAME = "name";
static const char *LOC = "loc";
static const char *BASES = "bases";
static const char *DECL = "isDecl";
static const char *ASSIGN = "assign";
static const char *VALUE = "value";
static const char *TYPE = "type";
static const char *FUNCTION = "isFunction";
static const char *RETURN = "isReturn";
static const char *FCALL = "isFcall";
static const char *ARGUMENTS = "arguments";
static const char *DH_CONSTRUCTOR = "isConstructor";
static const char *FIELD_OF = "fieldOf";
static const char *MEMBERS = "members";
static const char *PARAMETERS = "parameters";
static const char *ATTRIBUTES = "attributes";
static const char *STATEMENTS = "statements";
static const char *BITFIELD = "bitfieldBits";

static void dehydra_loadScript(Dehydra *this, const char *filename);

static char *readFile(const char *filename, const char *dir, long *size) {
  char *buf;
  FILE *f = fopen(filename, "r");
  if (!f) {
    if (dir && *filename && filename[0] != '/') {
      buf = xmalloc(strlen(dir) + strlen(filename) + 1);
      sprintf(buf, "%s/%s", dir, filename);
      f = fopen(buf, "r");
      free(buf);
    }
    if (!f) {
      fprintf(stderr, "Could not open %s\n", filename);
      _exit(1);
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

static JSBool Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                    jsval *rval)
{
  uintN i;
  /*Dehydra* jsv = (Dehydra*) JS_GetContextPrivate(cx);
  
    if(jsv->loc != SL_UNKNOWN) {
    char const * file(NULL); 
    int line(0); 
    int col(0);  
    sourceLocManager->decodeLineCol(jsv->loc, file, line, col);
   
    char *c = strrchr(file, '/');
    if(c) file = c + 1;
    cout << file << ":"<< line << ": ";
    }*/
  for (i = 0; i < argc; i++) {
    JSString *str = JS_ValueToString(cx, argv[i]);
    if (!str)
      return JS_FALSE;
    fprintf(stdout, "%s", JS_GetStringBytes(str));
  }
  fprintf(stdout, "\n");
  return JS_TRUE;
}


JSBool Version(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval)
{
  if (argc > 0 && JSVAL_IS_INT(argv[0]))
    *rval = INT_TO_JSVAL(JS_SetVersion(cx, (JSVersion) JSVAL_TO_INT(argv[0])));
  else
    *rval = INT_TO_JSVAL(JS_GetVersion(cx));
  return JS_TRUE;
}

static void
ReportError(JSContext *cx, const char *message, JSErrorReport *report)
{
  fflush(stdout);
  int error = JSREPORT_IS_EXCEPTION(report->flags);
  fprintf(stderr, "%s:%d: ", (report->filename ? report->filename : "NULL"),
          report->lineno);
  if (JSREPORT_IS_WARNING(report->flags)) fprintf(stderr, "JS Warning");
  if (JSREPORT_IS_STRICT(report->flags)) fprintf(stderr, "JS STRICT");
  if (error) fprintf(stderr, "JS Exception");
 
  fprintf(stderr, ": %s\n", message);
  if (report->linebuf) {
    fprintf(stderr, "%s\n", report->linebuf);
  }
  fflush(stderr);
  exit(1);
}

static void dehydra_init(Dehydra *this, const char *file, const char *script) {
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
  dehydra_loadScript(this, "system.js");
  dehydra_loadScript(this, script);
}

static void dehydra_loadScript(Dehydra *this, const char *filename) {
  long size = 0;
  char *buf = readFile(filename, this->dir, &size);
  jsval rval;
  xassert(JS_EvaluateScript(this->cx, this->globalObj, buf, size,
                            filename, 1, &rval));
}

static jsval dehydra_getCallback(Dehydra *this, char const *name) {
  jsval val = JSVAL_VOID;
  return (JS_GetProperty(this->cx, this->globalObj, name, &val)
          && val != JSVAL_VOID
          && JS_TypeOfValue(this->cx, val) == JSTYPE_FUNCTION) ? val : JSVAL_VOID;
}

static jsuint dehydra_getArrayLength(Dehydra *this, JSObject *array) {
  jsuint length = 0;
  xassert(JS_GetArrayLength(this->cx, array, &length));
  return length;
}

static void 
dehydra_defineProperty(Dehydra *this, JSObject *obj,
                       char const *name, jsval value)
{
  xassert(JS_DefineProperty(this->cx, obj, name, value,
                            NULL, NULL, JSPROP_ENUMERATE));
}

static void 
dehydra_defineStringProperty(Dehydra *this, JSObject *obj,
                             char const *name, char const *value)
{
  JSString *str = JS_NewStringCopyZ(this->cx, value);
  xassert(str);
  dehydra_defineProperty(this, obj, name, STRING_TO_JSVAL(str));
}

static void dehydra_setLoc(Dehydra *this, JSObject *obj, tree t) {
  location_t loc = location_of (t);
  if (!loc || loc == UNKNOWN_LOCATION) return;

  char const *strLoc = loc_as_string (loc);
  /* Don't attach empty locations */
  if (strLoc && *strLoc)
    dehydra_defineStringProperty (this, obj, LOC, strLoc);
}

static char const * identifierName (tree t) {
  tree n = DECL_NAME (t);
  return n ? IDENTIFIER_POINTER (DECL_NAME (t)) : NULL;
}

static JSObject* dehydra_addVar(Dehydra *this, tree v, JSObject *parentArray) {
  if(!parentArray) parentArray = this->destArray;
  unsigned int length = dehydra_getArrayLength(this, parentArray);
  JSObject *obj = JS_ConstructObject(this->cx, &js_ObjectClass, NULL, 
                                     this->globalObj);
  //append object to array(rooting it)
  xassert(obj && JS_DefineElement(this->cx, parentArray, length,
                                  OBJECT_TO_JSVAL(obj),
                                  NULL, NULL, JSPROP_ENUMERATE));
  
  if (!v) return obj;
  if (TREE_CODE (v) == FUNCTION_DECL) {
    dehydra_defineStringProperty(this, obj, NAME, 
                                 decl_as_string (v, 0));
    dehydra_defineProperty (this, obj, FUNCTION, JSVAL_TRUE);  
    if (DECL_CONSTRUCTOR_P (v))
      dehydra_defineProperty (this, obj, DH_CONSTRUCTOR, JSVAL_TRUE);
    else
      dehydra_defineStringProperty (this, obj, TYPE, 
                                    type_as_string (TREE_TYPE (TREE_TYPE (v)), 0));
    tree arg = DECL_ARGUMENTS (v);
    tree arg_type = TYPE_ARG_TYPES (TREE_TYPE (v));
    if (DECL_NONSTATIC_MEMBER_FUNCTION_P (v))
      {
        /* Skip "this" argument.  */
        if(arg) arg = TREE_CHAIN (arg);
        arg_type = TREE_CHAIN (arg_type);
      }
    JSObject *params = JS_NewArrayObject (this->cx, 0, NULL);
    dehydra_defineProperty (this, obj, PARAMETERS, OBJECT_TO_JSVAL (params));
    int i = 0;
    while (arg_type && (arg_type != void_list_node))
      {
        //xml_output_argument (xdi, arg, arg_type, dn->complete);
        JSObject *objArg = JS_ConstructObject(this->cx, &js_ObjectClass, NULL, 
                                              this->globalObj);
        xassert(objArg && JS_DefineElement(this->cx, params, i++,
                                        OBJECT_TO_JSVAL(objArg),
                                        NULL, NULL, JSPROP_ENUMERATE));
        if(arg) {
          dehydra_defineStringProperty (this, objArg, NAME, 
                                        identifierName (arg));
          arg = TREE_CHAIN (arg);
        }
        dehydra_defineStringProperty (this, objArg, TYPE,
                                      type_as_string (TREE_VALUE (arg_type), 0));
        arg_type = TREE_CHAIN (arg_type);
      }
  } else if (TYPE_P(v)) {
    char const *name = type_as_string (v, 0);
    dehydra_defineStringProperty (this, obj, NAME, 
                                  name);
  } else if (DECL_P(v)) {
    char const *name = decl_as_string (v, 0);
    dehydra_defineStringProperty (this, obj, NAME, 
                                  name);
    tree typ = TREE_TYPE (v);
    /*tree type_name = TYPE_NAME (typ);*/
    char const *type = type_as_string (typ, 0/*TFF_CHASE_TYPEDEF*/);
    dehydra_defineStringProperty (this, obj, TYPE,
                                  type);
  }
  dehydra_setLoc(this, obj, v);
  this->lastVar = obj;
  return obj;
}


static void dehydra_iterate_statementlist (Dehydra *, tree);
static tree statement_walker (tree *, int *walk_, void *);

// TODO: add methods, members
static int dehydra_visitClass(Dehydra *this, tree c) {
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
  
  tree attributes = TYPE_ATTRIBUTES (c);
  if (attributes) {
    this->destArray = JS_NewArrayObject (this->cx, 0, NULL);
    dehydra_defineProperty (this, objClass, ATTRIBUTES,
                           OBJECT_TO_JSVAL (this->destArray));
  }
  i = 0;
  tree a;
  for (a = attributes; a; a = TREE_CHAIN (a)) {
    tree name = TREE_PURPOSE (a);
    /* tree args = TREE_VALUE (a);*/
    JSString *str = JS_NewStringCopyZ (this->cx, IDENTIFIER_POINTER (name));
    xassert (str 
             && JS_DefineElement (this->cx, this->destArray, i++,
                                  STRING_TO_JSVAL(str),
                                  NULL, NULL, JSPROP_ENUMERATE));
  }
  
  this->destArray = NULL;
  jsval rval, argv[1];
  argv[0] = OBJECT_TO_JSVAL(objClass);
  xassert(JS_CallFunctionValue(this->cx, this->globalObj, process_class,
                               1, argv, &rval));
  xassert (JS_SetArrayLength(this->cx, this->rootedArgDestArray, length));
  return true;
}

static int dehydra_visitFunction(Dehydra *this, tree f) {
  jsval process_function = dehydra_getCallback(this, "process_function");
  if (process_function == JSVAL_VOID) return true;

  void **v = pointer_map_contains(this->fndeclMap, f);
  if (!v) {
    fprintf (stderr, "%s: ", loc_as_string (location_of (f)));
    fprintf (stderr, "No body for %s\n", decl_as_string (f, 0));
    return true;
  }
  this->statementHierarchyArray = (JSObject*) *v;
  
  jsval rval, argv[1];
  argv[0] = OBJECT_TO_JSVAL(this->statementHierarchyArray);
  // the array is now rooted as a function argument
  xassert(JS_RemoveRoot(this->cx, &this->statementHierarchyArray));
  xassert(JS_CallFunctionValue(this->cx, this->globalObj, process_function,
                               1, argv, &rval));
  return true;
}

/* Creates next array to dump dehydra objects onto */
static void dehydra_nextStatement(Dehydra *this, location_t loc) {
  unsigned int length = dehydra_getArrayLength (this,
                                                this->statementHierarchyArray);
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
  const char *loc_str = loc_as_string (this->loc);
  const char *s = strrchr (loc_str, '/');
  if (s) loc_str = s + 1;
  dehydra_defineStringProperty (this, obj, LOC, loc_str);
}

static Dehydra dehydra = {0};
int visitClass(tree c) {
  return dehydra_visitClass(&dehydra, c);
}

void postvisitClass(tree c) {
}

int visitFunction(tree f) {
  return dehydra_visitFunction(&dehydra, f);
}

void postvisitFunction(tree f) {
}

static void dehydra_print(Dehydra *this, JSObject *obj) {
  jsval print = dehydra_getCallback(this, "print");
  jsval rval, argv[1];
  argv[0] = OBJECT_TO_JSVAL(obj);
  xassert(JS_CallFunctionValue(this->cx, this->globalObj, print,
                               1, argv, &rval));
}

/* messes with dest-array */
static void dehydra_attachNestedFields(Dehydra *this, JSObject *obj, char const *name, tree t) {
  JSObject *tmp = this->destArray;
  this->destArray = JS_NewArrayObject(this->cx, 0, NULL);
  dehydra_defineProperty(this, obj, name,
                         OBJECT_TO_JSVAL(this->destArray));
  cp_walk_tree_without_duplicates(&t, statement_walker, this);
  this->destArray = tmp;
}

static void 
dehydra_fcallDoArgs (Dehydra *this, JSObject *obj, tree expr,
                                 int i, int count) {
  JSObject *tmp = this->destArray;
  this->destArray = JS_NewArrayObject (this->cx, 0, NULL);
  dehydra_defineProperty (this, obj, ARGUMENTS,
                          OBJECT_TO_JSVAL (this->destArray));
  for (; i < count;i++) {
    tree e = GENERIC_TREE_OPERAND(expr, i);
    cp_walk_tree_without_duplicates(&e,
                                    statement_walker, this);        
  }
  this->destArray = tmp;
}

static JSObject* dehydra_makeVar (Dehydra *this, tree t, 
                                  char const *prop, JSObject *attachToObj) {
  unsigned int length = dehydra_getArrayLength (this, this->destArray);
  cp_walk_tree_without_duplicates (&t, statement_walker, this);        
  xassert (length < dehydra_getArrayLength (this, this->destArray));
  jsval v;
  xassert (JS_GetElement (this->cx, this->destArray, length, &v));
  JSObject *obj = v == JSVAL_VOID ? NULL : JSVAL_TO_OBJECT (v);
  if (prop && attachToObj && obj) {
    dehydra_defineProperty (this, attachToObj, prop, v);
    xassert (JS_SetArrayLength (this->cx, this->destArray, length));
  }
  return obj;
}

static tree
statement_walker (tree *tp, int *walk_subtrees, void *data) {
  Dehydra *this = data;
  enum tree_code code = TREE_CODE(*tp); 
  switch (code) {
  case STATEMENT_LIST:
    *walk_subtrees = 0;
    dehydra_iterate_statementlist(this, *tp);
    return NULL_TREE;
  case DECL_EXPR:
    {
      tree e = *tp;
      tree decl = GENERIC_TREE_OPERAND (e, 0);
      tree init = DECL_INITIAL (decl);
      JSObject *obj = dehydra_addVar(this, decl, NULL);
      dehydra_defineProperty(this, obj, DECL, JSVAL_TRUE);
      if (init) {
        dehydra_attachNestedFields(this, obj, ASSIGN, init);
      }
      *walk_subtrees = 0;
      break;
    }
  case INIT_EXPR:
    {
      JSObject *tmp = NULL;
      xassert(2 == TREE_OPERAND_LENGTH (*tp) && this->lastVar);
      
      /* now add constructor */
      tmp = this->destArray;
      this->destArray = JS_NewArrayObject (this->cx, 0, NULL);
      /* note here we are assuming that last addVar as the last declaration */
      dehydra_defineProperty (this, this->lastVar, ASSIGN,
                             OBJECT_TO_JSVAL (this->destArray));

      /* op 0 is an anonymous temporary..i think..so use last var instead */
      cp_walk_tree_without_duplicates(&GENERIC_TREE_OPERAND(*tp, 1),
                                      statement_walker, this);        
      
      this->destArray = tmp;
      *walk_subtrees = 0;
      break;
    }
   case TARGET_EXPR:
     {
       /* this is a weird initializer tree node, however it's usually with INIT_EXPR
          so info in it is redudent */
       cp_walk_tree_without_duplicates(&TARGET_EXPR_INITIAL(*tp),
                                      statement_walker, this);        
       *walk_subtrees = 0;
       break;
     }
  case AGGR_INIT_EXPR:
    {
      /* another weird initializer */
      int paramCount = aggr_init_expr_nargs(*tp) + 3;
      tree fn = AGGR_INIT_EXPR_FN(*tp);
      JSObject *obj = dehydra_makeVar (this, fn, NULL, NULL);
      dehydra_defineProperty (this, obj, FCALL, JSVAL_TRUE);
      dehydra_fcallDoArgs (this, obj, *tp, 4, paramCount);
      *walk_subtrees = 0;
      break;
    }
    /* these wrappers are pretty similar */
  case POINTER_PLUS_EXPR:
  case ADDR_EXPR:
  case OBJ_TYPE_REF:
  case INDIRECT_REF:
    cp_walk_tree_without_duplicates (&GENERIC_TREE_OPERAND (*tp, 0),
                                     statement_walker, this);        
    *walk_subtrees = 0;
    break;
  case CALL_EXPR:
    {
      int paramCount = TREE_INT_CST_LOW (GENERIC_TREE_OPERAND (*tp, 0));
      tree fn = CALL_EXPR_FN (*tp);
      /* index of first param */
      int i = 3;
      
      JSObject *obj = dehydra_makeVar (this, fn, NULL, NULL);
      dehydra_defineProperty (this, obj, FCALL, JSVAL_TRUE);
        
      if (TREE_TYPE (fn) != NULL_TREE && TREE_CODE (TREE_TYPE (fn)) == METHOD_TYPE) {
        if (DECL_CONSTRUCTOR_P (fn))
          dehydra_defineProperty (this, obj, DH_CONSTRUCTOR, JSVAL_TRUE);        
     
        tree o = GENERIC_TREE_OPERAND(*tp, i);
        ++i;
        xassert (dehydra_makeVar (this, o, FIELD_OF, obj));
      }
      dehydra_fcallDoArgs (this, obj, *tp, i, paramCount);     
      *walk_subtrees = 0;
      break;
    }
  case MODIFY_EXPR:
    {
      JSObject *obj = dehydra_makeVar (this, GENERIC_TREE_OPERAND (*tp, 0),
                                       NULL, NULL);
      if (obj) {
        dehydra_attachNestedFields (this, obj,
                                    ASSIGN, GENERIC_TREE_OPERAND (*tp, 1));
      }
      *walk_subtrees = 0;
      break;
    }
  case COMPONENT_REF:
    {
      JSObject *obj = dehydra_addVar (this, GENERIC_TREE_OPERAND(*tp, 1), 
                                      NULL);
      xassert (dehydra_makeVar (this, GENERIC_TREE_OPERAND (*tp, 0),
                                FIELD_OF, obj));
    }
    *walk_subtrees = 0;
    break;
  case INTEGER_CST:
  case REAL_CST:
  case STRING_CST:
    {
      JSObject *obj = dehydra_addVar(this, *tp, NULL);
      dehydra_defineStringProperty(this, 
                                   obj, VALUE, 
                                   expr_as_string(*tp, 0));
    }
    break;
  case RETURN_EXPR: 
    {
      tree expr = GENERIC_TREE_OPERAND (*tp, 0);
      /* it would be nice if there was a better place to reset lastVar */
      this->lastVar = NULL;

      if (expr && TREE_CODE (expr) != RESULT_DECL) {
        if (TREE_CODE (expr) == INIT_EXPR) {
          expr = GENERIC_TREE_OPERAND (expr, 1);
        } 
        JSObject *obj = dehydra_makeVar (this, expr, NULL, NULL);
        xassert (obj);
        dehydra_defineProperty (this, obj, RETURN, JSVAL_TRUE);
      }
      *walk_subtrees = 0;
      break;
    }
  case VAR_DECL:
  case FUNCTION_DECL:
  case PARM_DECL:
    {
      dehydra_addVar(this, *tp, NULL);
      break;
    }
    /*magic compiler stuff we probably couldn't care less about */
  case CLEANUP_POINT_EXPR:
  case RESULT_DECL:
    /* this isn't magic, but breaks pretty-printing */
  case LABEL_DECL:
    break;
  default:
    //fprintf(stderr, "%s:", loc_as_string(this->loc));
    //    fprintf(stderr, "walking tree element: %s. %s\n", tree_code_name[TREE_CODE(*tp)],
    //expr_as_string(*tp, 0));
    break;
  }
  return NULL_TREE;
}

static void dehydra_iterate_statementlist (Dehydra *this, tree statement_list) {
  xassert (TREE_CODE (statement_list) == STATEMENT_LIST);
  tree_stmt_iterator i;
  for (i = tsi_start (statement_list); !tsi_end_p (i); tsi_next (&i)) {
    tree s = *tsi_stmt_ptr (i);
    dehydra_nextStatement (this, location_of (s));
    cp_walk_tree_without_duplicates (&s, statement_walker, this);
  }
}

void dehydra_cp_pre_genericize(tree fndecl) {
  Dehydra *this = &dehydra;
  this->statementHierarchyArray = JS_NewArrayObject (this->cx, 0, NULL);
  xassert (this->statementHierarchyArray 
          && JS_AddRoot (this->cx, &this->statementHierarchyArray));
  *pointer_map_insert (this->fndeclMap, fndecl) = 
    (void*) this->statementHierarchyArray; 
  dehydra_nextStatement (this, location_of (fndecl));

  tree body_chain = DECL_SAVED_TREE (fndecl);
  if (body_chain && TREE_CODE (body_chain) == BIND_EXPR) {
    body_chain = BIND_EXPR_BODY (body_chain);
  }
  //  fprintf (stderr, "dehydra_cp_pre_genericize: %s\n", decl_as_string (fndecl, 0));
  JSObject *obj = dehydra_addVar (this, fndecl, NULL);
  dehydra_defineProperty (this, obj, FUNCTION, JSVAL_TRUE);
  cp_walk_tree_without_duplicates (&body_chain, statement_walker, this);
}

void initDehydra(const char *file, const char *script)  {
  dehydra_init(&dehydra, file,  script);
}

void dehydra_input_end () {
  jsval input_end = dehydra_getCallback(&dehydra, "input_end");
  if (input_end == JSVAL_VOID) return;
  
  jsval rval;
  xassert(JS_CallFunctionValue(dehydra.cx, dehydra.globalObj, input_end,
                               0, NULL, &rval));
}
