#include "dehydra.h"
#include <jsapi.h>
#include <jsobj.h>
#include <unistd.h>
#include <stdio.h>
#include "cp-tree.h"
#include "cxx-pretty-print.h"
#include "tree-iterator.h"
#include "pointer-set.h"

#define xassert(cond)                                                   \
  if (!(cond)) {                                                        \
    fprintf(stderr, "%s:%d: Assertion failed:" #cond "\n",  __FILE__, __LINE__); \
    _exit(1);                                                           \
  }

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
  int inReturn;
  JSObject *lastVar;
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
static const char *USE = "isUse";
static const char *RETURN = "isReturn";
static const char *FCALL = "isFcall";
static const char *PARAMETERS = "parameters";
static const char *DH_CONSTRUCTOR = "isConstructor";
static const char *FIELD_OF = "fieldOf";

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

static JSBool ReadFile(JSContext *cx, JSObject *obj, uintN argc,
                       jsval *argv, jsval *rval) {
  /*  if (!(argc == 1 && JSVAL_IS_STRING(argv[0]))) return JS_TRUE;
      fopen(JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), std::ios::binary);
      if(!f.is_open())
      {
      return JS_TRUE;
      }
      f.seekg(0, std::ios::end);
      size_t len = f.tellg();
      char *buf = (char*)JS_malloc(cx, len);
      f.seekg(0, std::ios::beg);
      f.read(buf, len);
      f.close();
      *rval = STRING_TO_JSVAL(JS_NewString(cx, buf, len));*/
  return JS_TRUE;
}

static JSBool WriteFile(JSContext *cx, JSObject *obj, uintN argc,
                        jsval *argv, jsval *rval) {
  /*if (!(argc == 2 && JSVAL_IS_STRING(argv[0]) && JSVAL_IS_STRING(argv[1]))) {
    return JS_TRUE;
    }
    JSString *str = JSVAL_TO_STRING(argv[1]);
    std::ofstream out(JS_GetStringBytes(JSVAL_TO_STRING(argv[0])),
    std::ios::out | std::ios::binary);
    out.write(JS_GetStringBytes(str), JS_GetStringLength(str));
    out.close();*/
  return JS_TRUE;
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

JSBool Error(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval)
{
  fprintf(stderr, "Error ");
  /*Dehydra* c = (Dehydra*) JS_GetContextPrivate(cx);
    if(c->loc != SL_UNKNOWN) {
    cerr << toString(c->loc);
    } */
  fprintf(stderr,  ": %s\n",
          (argc > 0 && JSVAL_IS_STRING(argv[0]))
          ? JS_GetStringBytes(JSVAL_TO_STRING(argv[0]))
          : "Unspecified error");
  _exit(1);
  return JS_TRUE;
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
    {"write_file",      WriteFile,      1},
    {"read_file",       ReadFile,       1},
    {"print",           Print,          0},
    {"version",         Version,        0},
    {"error",           Error,          1},
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
  char const *strLoc = loc(t);
  /* Don't attach empty locations */
  if (strLoc && *strLoc)
    dehydra_defineStringProperty(this, obj, LOC, strLoc);
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
  if (this->inReturn)
    dehydra_defineProperty(this, obj, RETURN, JSVAL_TRUE);

  if (!v) return obj;
  if (TYPE_P(v)) {
    dehydra_defineStringProperty(this, obj, NAME, 
                                 type_as_string(v, 0));
  } else if (DECL_P(v)) {
    dehydra_defineStringProperty(this, obj, NAME, 
                                 decl_as_string(v, 0));
    dehydra_defineStringProperty(this, obj, TYPE,
                                 type_as_string(TREE_TYPE(v), 0));
  }
  dehydra_setLoc(this, obj, v);
  this->lastVar = obj;
  return obj;
}

// TODO: add methods, members
static int dehydra_visitClass(Dehydra *this, tree c) {
  jsval process_class = dehydra_getCallback(this, "process_class");
  if (process_class == JSVAL_VOID) return true;

  unsigned int length = dehydra_getArrayLength(this, this->rootedArgDestArray);

  JSObject *objClass = dehydra_addVar(this, c, this->rootedArgDestArray);

  JSObject *basesArray = JS_NewArrayObject(this->cx, 0, NULL);
  dehydra_defineProperty(this, objClass, BASES, OBJECT_TO_JSVAL(basesArray));

  tree binfo = TYPE_BINFO (c);
  int n_baselinks = BINFO_N_BASE_BINFOS (binfo);
  int i;
  for (i = 0; i < n_baselinks; i++)
    {
      tree base_binfo = BINFO_BASE_BINFO(binfo, i);
      JSString *str = 
        JS_NewStringCopyZ(this->cx, type_as_string(BINFO_TYPE(base_binfo),0));
      xassert(JS_DefineElement(this->cx, basesArray, i, STRING_TO_JSVAL(str),
                               NULL, NULL, JSPROP_ENUMERATE));
    }

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

  if (!DECL_SAVED_TREE(f)) return true;
  
  void **v = pointer_map_contains(this->fndeclMap, f);
   if (!v) {
    fprintf(stderr, "%s: ", loc(f));
    fprintf(stderr, "%s is empty\n", decl_as_string(f, 0));
    return true;
    }
  xassert(v);
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
static void dehydra_nextStatement(Dehydra *this) {
  unsigned int length = dehydra_getArrayLength(this,
                                               this->statementHierarchyArray);
  if (length) {
    unsigned int destLength = 0;
    jsval obj;
    xassert(JS_GetElement(this->cx, this->statementHierarchyArray, length - 1,
                          &obj));
    this->destArray = JSVAL_TO_OBJECT(obj);
    destLength = dehydra_getArrayLength(this, this->destArray);
    /* last element is already setup & empty, we are done */
    if (destLength == 0) return;
  }
  this->destArray = JS_NewArrayObject(this->cx, 0, NULL);
  xassert(this->destArray
          && JS_DefineElement(this->cx, this->statementHierarchyArray, length,
                              OBJECT_TO_JSVAL(this->destArray),
                              NULL, NULL, JSPROP_ENUMERATE));
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

static void dehydra_iterate_statementlist (Dehydra *, tree);
static tree statement_walker (tree *, int *walk_, void *);

/* messes with dest-array */
static void dehydra_attachNestedFields(Dehydra *this, JSObject *obj, char const *name, tree t) {
  JSObject *tmp = this->destArray;
  this->destArray = JS_NewArrayObject(this->cx, 0, NULL);
  dehydra_defineProperty(this, obj, name,
                         OBJECT_TO_JSVAL(this->destArray));
  cp_walk_tree_without_duplicates(&t, statement_walker, this);
  this->destArray = tmp;
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
        int i, len;
        tree e = *tp;
        len = TREE_OPERAND_LENGTH (e);
        xassert(len == 1);
        for (i = 0; i < len; ++i) {
          tree decl = GENERIC_TREE_OPERAND (e, i);
          tree init = DECL_INITIAL (decl);
          JSObject *obj = dehydra_addVar(this, decl, NULL);
          dehydra_defineProperty(this, obj, DECL, JSVAL_TRUE);
          if (init) {
            dehydra_attachNestedFields(this, obj, ASSIGN, init);
          }
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
      /* For constructors INIT_EXPR above contains TARGET_EXPR here */
      tree aggr_init = TARGET_EXPR_INITIAL (*tp);
      xassert (TREE_CODE (aggr_init) == AGGR_INIT_EXPR);
      /* operand 1 is func that performs init .3+ are params */
      int paramCount = TREE_INT_CST_LOW (GENERIC_TREE_OPERAND (aggr_init, 0));
      tree init = GENERIC_TREE_OPERAND (aggr_init, 1);
      xassert (TREE_CODE (init) == ADDR_EXPR);
      init = GENERIC_TREE_OPERAND (init, 0);

      JSObject *obj = dehydra_addVar (this, init, NULL);      
      if (DECL_CONSTRUCTOR_P (init))
        dehydra_defineProperty (this, obj, DH_CONSTRUCTOR, JSVAL_TRUE);        
      dehydra_defineProperty (this, obj, FCALL, JSVAL_TRUE);
      JSObject *tmp = this->destArray;
      this->destArray = JS_NewArrayObject (this->cx, 0, NULL);
      dehydra_defineProperty (this, obj, PARAMETERS,
                              OBJECT_TO_JSVAL (this->destArray));
      /* iterate through constructor params */
      int i;
      /* TODO, figure sup with paramCount and all of of the strange INTEGER_CST nodes */
      for (i = 4; i < paramCount; ++i) {
        cp_walk_tree_without_duplicates(&GENERIC_TREE_OPERAND(aggr_init, i),
                                        statement_walker, this);        
      }
      this->destArray = tmp;
      *walk_subtrees = 0;
      break;
    }
  case CALL_EXPR:
    {
      int paramCount = TREE_INT_CST_LOW (GENERIC_TREE_OPERAND (*tp, 0));
      tree fn = CALL_EXPR_FN (*tp);
      /* index of first param */
      int i = 3;
      xassert (TREE_CODE (fn) == ADDR_EXPR);
      fn = GENERIC_TREE_OPERAND (fn, 0);
      
      JSObject *obj = dehydra_addVar (this, fn, NULL);
      if (TREE_TYPE (fn) != NULL_TREE && TREE_CODE (TREE_TYPE (fn)) == METHOD_TYPE) {
        if (DECL_CONSTRUCTOR_P (fn))
          dehydra_defineProperty (this, obj, DH_CONSTRUCTOR, JSVAL_TRUE);        
     
        tree o = GENERIC_TREE_OPERAND(*tp, i);
        ++i;
        unsigned int length = dehydra_getArrayLength(this, this->destArray);
        cp_walk_tree_without_duplicates(&o,
                                        statement_walker, this);        
        jsval v;
        xassert(JS_GetElement(this->cx, this->destArray, length, &v));
        dehydra_defineProperty (this, obj, FIELD_OF, v);
        xassert (JS_SetArrayLength(this->cx, this->destArray, length));
      }

      dehydra_defineProperty (this, obj, FCALL, JSVAL_TRUE);
      JSObject *tmp = this->destArray;
      this->destArray = JS_NewArrayObject (this->cx, 0, NULL);
      dehydra_defineProperty (this, obj, PARAMETERS,
                              OBJECT_TO_JSVAL (this->destArray));
      for (; i < paramCount;i++) {
        tree e = GENERIC_TREE_OPERAND(*tp, i);
        cp_walk_tree_without_duplicates(&e,
                                        statement_walker, this);        
      }
      this->destArray = tmp;
      *walk_subtrees = 0;
      break;
    }
  case INTEGER_CST:
    {
      JSObject *obj = dehydra_addVar(this, *tp, NULL);
      dehydra_defineStringProperty(this, 
                                   obj, VALUE, 
                                   expr_as_string(*tp, 0));
    }
    break;
  case RETURN_EXPR: 
    {
      int len = TREE_OPERAND_LENGTH (*tp);
      if (len == 1) {
        xassert(!this->inReturn);
        this->inReturn = true;;
        tree expr = GENERIC_TREE_OPERAND (*tp, 0);
        xassert (TREE_CODE (expr) == INIT_EXPR);
        expr = GENERIC_TREE_OPERAND (expr, 1);
        cp_walk_tree_without_duplicates(&expr, statement_walker, this);
        this->inReturn = false;
      } else {
        xassert (!len);
      }
      *walk_subtrees = 0;
      break;
    }
  case VAR_DECL:
  case FUNCTION_DECL:
  case PARM_DECL:
    {
      JSObject *obj = dehydra_addVar(this, *tp, NULL);
      dehydra_defineProperty(this, obj, USE, JSVAL_TRUE);
      break;
    }
    /*magic compiler stuff we probably couldn't care less about */
  case CLEANUP_POINT_EXPR:
  case RESULT_DECL:
    /* this isn't magic, but breaks pretty-printing */
  case LABEL_DECL:
    break;
  default:
    fprintf(stderr, "%s:", loc(*tp));
    fprintf(stderr, "walking tree element: %s. %s\n", tree_code_name[TREE_CODE(*tp)],
      expr_as_string(*tp, 0));
  }
  return NULL_TREE;
}

static void dehydra_iterate_statementlist (Dehydra *this, tree statement_list) {
  xassert(TREE_CODE (statement_list) == STATEMENT_LIST);
  tree_stmt_iterator i;
  for (i = tsi_start (statement_list); !tsi_end_p (i); tsi_next (&i)) {
    dehydra_nextStatement (this);
    cp_walk_tree_without_duplicates(tsi_stmt_ptr (i), statement_walker, this);
  }
}

void dehydra_cp_pre_genericize(tree fndecl) {
  Dehydra *this = &dehydra;
  this->statementHierarchyArray = JS_NewArrayObject(this->cx, 0, NULL);
  xassert(this->statementHierarchyArray 
          && JS_AddRoot(this->cx, &this->statementHierarchyArray));
  *pointer_map_insert(this->fndeclMap, fndecl) = 
    (void*) this->statementHierarchyArray; 
  dehydra_nextStatement (this);

  tree body_chain = DECL_SAVED_TREE(fndecl);
  if (body_chain && TREE_CODE (body_chain) == BIND_EXPR) {
    body_chain = BIND_EXPR_BODY (body_chain);
  }
  JSObject *obj = dehydra_addVar (this, fndecl, NULL);
  dehydra_defineProperty (this, obj, FUNCTION, JSVAL_TRUE);
  cp_walk_tree_without_duplicates(&body_chain, statement_walker, this);
}

void initDehydra(const char *file, const char *script)  {
  dehydra_init(&dehydra, file,  script);
}
