#ifndef BUILTINS_H
#define BUILTINS_H

JSBool Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval);

JSBool Error(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval);


JSBool Warning(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval);

JSBool Version(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval);

void ReportError(JSContext *cx, const char *message, JSErrorReport *report);

#endif
