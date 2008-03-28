#ifndef DEHYDRA_BUILTINS_H
#define DEHYDRA_BUILTINS_H

JSBool Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval);

JSBool Error(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval);


JSBool Warning(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval);

JSBool Version(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval);

void ReportError(JSContext *cx, const char *message, JSErrorReport *report);

JSBool WriteFile (JSContext *cx, JSObject *obj, uintN argc,
                  jsval *argv, jsval *rval);

JSBool ReadFile(JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval);

char *readFile(const char *filename, const char *dir, long *size);

JSBool Include(JSContext *cx, JSObject *obj, uintN argc,
               jsval *argv, jsval *rval);

JSBool obj_hashcode(JSContext *cx, JSObject *obj, uintN argc,
                    jsval *argv, jsval *rval);
#endif
