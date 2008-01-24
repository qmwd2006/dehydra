#include <config.h>
#include <system.h>
#include <coretypes.h>
#include <tm.h>
#include <tree.h>
#include <cp-tree.h>
#include <toplev.h>
#include <jsapi.h>

#include "builtins.h"

JSBool Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval)
{
  uintN i;
  /* don't touch stdout if it's being piped to assembler */
  FILE *out = (!strcmp(asm_file_name, "-") && ! flag_syntax_only)
    ? stderr : stdout;
  for (i = 0; i < argc; i++) {
    JSString *str = JS_ValueToString(cx, argv[i]);
    if (!str)
      return JS_FALSE;
    fprintf(out, "%s", JS_GetStringBytes(str));
  }
  fprintf(out, "\n");
  return JS_TRUE;
}

JSBool Error(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval)
{
  uintN i;
  for (i = 0; i < argc; i++) {
    JSString *str = JS_ValueToString(cx, argv[i]);
    if (!str)
      return JS_FALSE;
    error ("%s", JS_GetStringBytes(str));
  }
  return JS_TRUE;
}

JSBool Warning(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval)
{
  uintN i;
  int code = 0;
  for (i = 0; i < argc; i++) {
    if (i == 0 && JSVAL_IS_INT (argv[i])) {
      code = JSVAL_TO_INT (argv[i]);
      continue;
    }
    JSString *str = JS_ValueToString(cx, argv[i]);
    if (!str)
      return JS_FALSE;
    warning (code, "%s", JS_GetStringBytes(str));
  }
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

void
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
