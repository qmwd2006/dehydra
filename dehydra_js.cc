#include "dehydra_js.h"
#include <iostream>
#include <fstream>

void
Dehydra::PrintError(JSContext *cx, const char *message, JSErrorReport *report)
{
  fflush(stdout);
  bool error = JSREPORT_IS_EXCEPTION(report->flags);
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

JSBool Dehydra::ReadFile(JSContext *cx, JSObject *obj, uintN argc,
                           jsval *argv, jsval *rval) {
  if (!(argc == 1 && JSVAL_IS_STRING(argv[0]))) return JS_TRUE;
  std::ifstream f(JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), std::ios::binary);
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
  *rval = STRING_TO_JSVAL(JS_NewString(cx, buf, len));
  return JS_TRUE;
}

JSBool Dehydra::WriteFile(JSContext *cx, JSObject *obj, uintN argc,
                            jsval *argv, jsval *rval) {
  if (!(argc == 2 && JSVAL_IS_STRING(argv[0]) && JSVAL_IS_STRING(argv[1]))) {
    return JS_TRUE;
  }
  JSString *str = JSVAL_TO_STRING(argv[1]);
  std::ofstream out(JS_GetStringBytes(JSVAL_TO_STRING(argv[0])),
                    std::ios::out | std::ios::binary);
  out.write(JS_GetStringBytes(str), JS_GetStringLength(str));
  out.close();
  return JS_TRUE;
}

JSBool Dehydra::Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
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
      std::cout << JS_GetStringBytes(str);
  }
  std::cout << std::endl;
  return JS_TRUE;
}


JSBool Dehydra::Version(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                          jsval *rval)
{
  if (argc > 0 && JSVAL_IS_INT(argv[0]))
    *rval = INT_TO_JSVAL(JS_SetVersion(cx, (JSVersion) JSVAL_TO_INT(argv[0])));
  else
    *rval = INT_TO_JSVAL(JS_GetVersion(cx));
  return JS_TRUE;
}

JSBool Dehydra::Error(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                        jsval *rval)
{
  std::cerr << "Error ";
  /*Dehydra* c = (Dehydra*) JS_GetContextPrivate(cx);
  if(c->loc != SL_UNKNOWN) {
    cerr << toString(c->loc);
    } */
  std::cerr << ": ";
  if (argc > 0 && JSVAL_IS_STRING(argv[0]))
  {
    std::cerr << JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
  } else
  {
    std::cerr << "Unspecified error";
  }
  std::cerr << std::endl;
  exit(1);
  return JS_TRUE;
}
