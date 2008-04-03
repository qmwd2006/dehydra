#ifndef DEHYDRA_BUILTINS_H
#define DEHYDRA_BUILTINS_H

/* JS Natives */

JSBool Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval);

JSBool Error(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval);


JSBool Warning(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval);

JSBool Version(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval);

void ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report);

JSBool WriteFile (JSContext *cx, JSObject *obj, uintN argc,
                  jsval *argv, jsval *rval);

JSBool ReadFile(JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval);

JSBool Include(JSContext *cx, JSObject *obj, uintN argc,
               jsval *argv, jsval *rval);

JSBool obj_hashcode(JSContext *cx, JSObject *obj, uintN argc,
                    jsval *argv, jsval *rval);

/* Related C functions */

char *readFile(const char *path, long *size);
FILE *findFile(const char *filename, const char *dir, char **realname);
char *readEntireFile(FILE *f, long *size);
void  reportError(JSContext *cx, const char *file, int line, 
                  const char *fmt, ...);

#define REPORT_ERROR_0(cx, fmt) reportError(cx, __FILE__, __LINE__, fmt);
#define REPORT_ERROR_1(cx, fmt, arg1) reportError(cx, __FILE__, __LINE__, fmt, arg1);
#define REPORT_ERROR_2(cx, fmt, arg1, arg2) reportError(cx, __FILE__, __LINE__, fmt, arg1, arg1);

#endif
