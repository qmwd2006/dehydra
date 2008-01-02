#include "dehydra.h"
#include "jsapi.h"
#include <unistd.h>
#include <stdio.h>

#define xassert(cond) \
  if (!(cond)) {      \
    fprintf(stderr, "%s:%d: Assertion failed:" #cond "\n",  __FILE__, __LINE__); \
    _exit(1);         \
   }

struct Dehydra {
 JSRuntime *rt;
 JSContext *cx;
 JSObject *globalObj;
};

typedef struct Dehydra Dehydra;

static void dehydra_loadScript(Dehydra *this, const char *filename);

char *readFile(const char *filename, const char *dir, long *size) {
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

void
PrintError(JSContext *cx, const char *message, JSErrorReport *report)
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
  _exit(1);
}

JSBool ReadFile(JSContext *cx, JSObject *obj, uintN argc,
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

JSBool WriteFile(JSContext *cx, JSObject *obj, uintN argc,
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

JSBool Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
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

void dehydra_init(Dehydra *this, const char *script) {
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
  //rootedArgDestArray = destArray = JS_NewArrayObject(cx, 0, NULL);
  //xassert(destArray && JS_AddRoot(cx, &rootedArgDestArray));

  //stateArray = JS_NewArrayObject(cx, 1, NULL);
  //xassert(stateArray && JS_AddRoot(cx, &stateArray));
  //state = JSVAL_VOID;

  //typeArrayArray = JS_NewArrayObject(cx, 0, NULL);
  //xassert(typeArrayArray && JS_AddRoot(cx, &typeArrayArray));

 // JS_SetVersion(cx, (JSVersion) 170);
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

static int dehydra_visitClass(Dehydra *this, tree c) {
  jsval process_class = dehydra_getCallback(this, "process_class");
  if (process_class == JSVAL_VOID) return true;

  jsval rval, argv[1];
  //argv[0] = OBJECT_TO_JSVAL(objClass);
  xassert(JS_CallFunctionValue(this->cx, this->globalObj, process_class,
                               0, argv, &rval));
  return true;
}

static Dehydra dehydra = {0};
int visitClass(tree c) {
  return dehydra_visitClass(&dehydra, c);
}

void initDehydra(const char *script)  {
  dehydra_init(&dehydra, script);
}
