#ifndef DEHYDRA_JS_H
#define DEHYDRA_JS_H
#include "jsapi.h"

class Dehydra {
public:
  Dehydra();

  static void
  PrintError(JSContext *cx, const char *message, JSErrorReport *report);
  static JSBool
  ReadFile(JSContext *cx, JSObject *obj, uintN argc,
           jsval *argv, jsval *rval);
  static JSBool 
  WriteFile(JSContext *cx, JSObject *obj, uintN argc,
            jsval *argv, jsval *rval);
  static JSBool 
  Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
        jsval *rval);
  static JSBool 
  Version(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
          jsval *rval);
  static JSBool 
  Error(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
        jsval *rval);
  
private:
  JSRuntime *rt;
  JSContext *cx;
  JSObject *globalObj;
};

// i'll set this up later
inline void xassert(bool b) {
  if (!b) {
    int *i = 0;
    *i = b;
  }
}

#endif //DEHYDRA_JS_H
