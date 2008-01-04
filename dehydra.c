#include "dehydra.h"
#include <jsapi.h>
#include <jsobj.h>
#include <unistd.h>
#include <stdio.h>
#include "cp-tree.h"
#include "cxx-pretty-print.h"

#define xassert(cond) \
  if (!(cond)) {      \
    fprintf(stderr, "%s:%d: Assertion failed:" #cond "\n",  __FILE__, __LINE__); \
    _exit(1);         \
   }

struct Dehydra {
  JSRuntime *rt;
  JSContext *cx;
  JSObject *globalObj;
  JSObject *destArray;
  JSObject *rootedArgDestArray;
};

typedef struct Dehydra Dehydra;

static const char *NAME = "name";
static const char *LOC = "loc";
static const char *BASES = "bases";

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

static void dehydra_init(Dehydra *this, const char *script) {
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

  xassert((this->rt = JS_NewRuntime(0x9000000L)));
  xassert((this->cx = JS_NewContext(this->rt, 8192)));

  JS_SetContextPrivate(this->cx, this);
  
  this->globalObj = JS_NewObject(this->cx, &global_class, 0, 0);
  JS_InitStandardClasses(this->cx, this->globalObj);
  /* register error handler */
  //JS_SetErrorReporter(cx, printError);
  xassert(JS_DefineFunctions(this->cx, this->globalObj, shell_functions));
  this->rootedArgDestArray = 
    this->destArray = JS_NewArrayObject(this->cx, 0, NULL);
  xassert(this->destArray && JS_AddRoot(this->cx, &this->rootedArgDestArray));

  //stateArray = JS_NewArrayObject(cx, 1, NULL);
  //xassert(stateArray && JS_AddRoot(cx, &stateArray));
  //state = JSVAL_VOID;

  //typeArrayArray = JS_NewArrayObject(cx, 0, NULL);
  //xassert(typeArrayArray && JS_AddRoot(cx, &typeArrayArray));

  JS_SetVersion(this->cx, (JSVersion) 170);
  //  loadScript(libScript);
  dehydra_loadScript(this, script);
}

static void dehydra_loadScript(Dehydra *this, const char *filename) {
  long size = 0;
  char *buf = readFile(filename, NULL, &size);
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
  dehydra_defineStringProperty(this, obj, LOC, loc(t));
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
  if (TYPE_P(v)) {
    dehydra_defineStringProperty(this, obj, NAME, 
                                 type_as_string(v, 0));
  }
  dehydra_setLoc(this, obj, v);
  return obj;
}

// TODO: add methods, members
static int dehydra_visitClass(Dehydra *this, tree c) {
  jsval process_class = dehydra_getCallback(this, "process_class");
  if (process_class == JSVAL_VOID) return true;
  
  JSObject *objClass = dehydra_addVar(this, c, NULL);

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
  return true;
}

static tree
statement_walker (tree *tp, int *walk_subtrees, void *data)
{
  //  Dehydra *this = data;
  if (TREE_CODE(*tp) == STATEMENT_LIST) return NULL_TREE;

  printf("%s:", "loc(*tp)");
  printf("walking tree element: %s. %s\n", tree_code_name[TREE_CODE(*tp)],
         expr_as_string(*tp, 0xff));
  print_generic_stmt_indented(stderr, *tp, 0, 2);
  /*  if (TYPE_P (*tp))
    *walk_subtrees = 0;

  else if (DECL_P (*tp)
	   && auto_var_in_fn_p (*tp, fn))
           return *tp;*/

  return NULL_TREE;
}

static int dehydra_visitFunction(Dehydra *this, tree f) {
  printf("b)%p: %s %s\n", f, decl_as_string(f, 0xff), loc(f));
  /*tree body_chain = DECL_SAVED_TREE(f);
  if (body_chain && TREE_CODE (body_chain) == BIND_EXPR) {
    body_chain = BIND_EXPR_BODY (body_chain);
  }

  printf("%s: %s\n", "loc(f)", decl_as_string(f, 0xff));
  //walk_tree_without_duplicates(&body_chain, statement_walker, this);
  // dump_function_to_file(f, stderr, 0xff);
  print_generic_stmt_indented(stderr, body_chain, 0, 2);*/
  return true;
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


void dehydra_cp_pre_genericize(tree fndecl) {
  tree body_chain = DECL_SAVED_TREE(fndecl);
  if (body_chain && TREE_CODE (body_chain) == BIND_EXPR) {
    body_chain = BIND_EXPR_BODY (body_chain);
  }
  
  printf("a) %p %s %s\n", fndecl, decl_as_string(fndecl, 0xff), loc(fndecl));
  //walk_tree_without_duplicates(&body_chain, statement_walker, this);
  // dump_function_to_file(f, stderr, 0xff);
  //  print_generic_stmt_indented(stderr, body_chain, 0, 2);
}

void initDehydra(const char *script)  {
  dehydra_init(&dehydra, script);
}