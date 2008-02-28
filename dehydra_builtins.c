#include <config.h>
#include <system.h>
#include <coretypes.h>
#include <tm.h>
#include <tree.h>
#include <cp-tree.h>
#include <toplev.h>
#include <jsapi.h>

#include "dehydra_builtins.h"
#include "xassert.h"

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
  int error = JSREPORT_IS_EXCEPTION(report->flags);
  jsval exn;
  fflush(stdout);
  fprintf(stderr, "%s:%d: ", (report->filename ? report->filename : "NULL"),
          report->lineno);
  if (JSREPORT_IS_WARNING(report->flags)) fprintf(stderr, "JS Warning");
  if (JSREPORT_IS_STRICT(report->flags)) fprintf(stderr, "JS STRICT");
  if (error) fprintf(stderr, "JS Exception");
 
  fprintf(stderr, ": %s\n", message);
  if (report->linebuf) {
    fprintf(stderr, "%s\n", report->linebuf);
  }
  if (error && JS_GetPendingException(cx, &exn)) {
    jsval stack;
    JS_GetProperty(cx, JSVAL_TO_OBJECT (exn), "stack", &stack);
    char *str = JS_GetStringBytes (JSVAL_TO_STRING (stack));
    int counter = 0;
    do {
      char *eol = strchr (str, '\n');
      if (eol)
        *eol = 0;
      char *at = strrchr (str, '@');
      if (!at) break;
      *at = 0;
      if (!*str) break;
      fprintf (stderr, "%s:\t#%d: %s\n", at+1, counter++, str);
      *at = '@';
      if (eol) {
        *eol = '\n';
        str = eol + 1;
      } else {
        break;
      }
    } while (*str);
  }
  fflush(stderr);
  exit(1);
}

JSBool ReadFile(JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval) {
  if (!(argc == 1 && JSVAL_IS_STRING(argv[0]))) return JS_TRUE;
  long size = 0;
  char *buf = readFile (JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), NULL, &size);
  if(!buf)
  {
    return JS_TRUE;
  }
  *rval = STRING_TO_JSVAL(JS_NewString(cx, buf, size));
  return JS_TRUE;
}

JSBool WriteFile(JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval) {
  if (!(argc == 2 && JSVAL_IS_STRING(argv[0]) && JSVAL_IS_STRING(argv[1]))) {
    return JS_TRUE;
  }
  FILE *f = fopen (JS_GetStringBytes (JSVAL_TO_STRING (argv[0])), "w");
  JSString *str = JSVAL_TO_STRING(argv[1]);
  fwrite (JS_GetStringBytes(str), 1, JS_GetStringLength(str), f);
  fclose (f);
  return JS_TRUE;
}

char *readFile(const char *filename, const char *dir, long *size) {
  char *buf;
  FILE *f = fopen(filename, "r");
  if (!f) {
    if (dir && *filename && filename[0] != '/') {
      buf = xmalloc(strlen(dir) + strlen(filename) + 2);
      sprintf(buf, "%s/%s", dir, filename);
      f = fopen(buf, "r");
      free(buf);
    }
    if (!f) {
      return NULL;
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
