#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include <config.h>
#include <system.h>
#include <coretypes.h>
#include <tm.h>
#include <tree.h>
#include <cp-tree.h>
#include <toplev.h>
#include <jsapi.h>

#include "dehydra.h"
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
  if (error && JS_GetPendingException(cx, &exn)
      && JS_TypeOfValue (cx, exn) == JSTYPE_OBJECT) {
    jsval stack;
    /* reformat the spidermonkey stack */
    JS_GetProperty(cx, JSVAL_TO_OBJECT (exn), "stack", &stack);
    if (JS_TypeOfValue (cx, stack) == JSTYPE_STRING) {
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
  }
  fflush(stderr);
  exit(1);
}

JSBool ReadFile(JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval) {
  if (!(argc == 1 && JSVAL_IS_STRING(argv[0]))) {
    JS_ReportError(cx, "read_file: invalid arguments, requires string");
    return JS_FALSE;
  }
  const char *filename = JS_GetStringBytes(JSVAL_TO_STRING(argv[0])); 
  long size = 0;
  char *buf = readFile (filename, NULL, &size);
  if(!buf) {
    JS_ReportError(cx, "read_file: error opening file '%s': %s",
                filename, strerror(errno));
    return JS_FALSE;
  }
  *rval = STRING_TO_JSVAL(JS_NewString(cx, buf, size));
  return JS_TRUE;
}

JSBool WriteFile(JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval) {
  if (!(argc == 2 && JSVAL_IS_STRING(argv[0]) && JSVAL_IS_STRING(argv[1]))) {
    JS_ReportError(cx, "write_file: invalid arguments, requires (string, string)");
    return JS_FALSE;
  }
  const char *filename = JS_GetStringBytes (JSVAL_TO_STRING (argv[0]));
  FILE *f = fopen (filename, "w");
  if (!f) {
    JS_ReportError(cx, "write_file: error opening file '%s': %s",
                   filename, strerror(errno));
    return JS_FALSE;
  }
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

JSBool Include(JSContext *cx, JSObject *obj, uintN argc,
               jsval *argv, jsval *rval) {
  // The following looks kind of funny -- should probably generate an error
  if (!(argc == 1 && JSVAL_IS_STRING(argv[0]))) return JS_TRUE;
  const char *filename = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
  Dehydra *this = JS_GetContextPrivate (cx);
  if (dehydra_loadScript (this, filename)) {
    return JS_FALSE;
  }
  *rval = JS_TRUE;
  return JS_TRUE;
}

/* author: tglek
   The ES4 spec says that it shouldn't be a pointer(with good reason).
   A counter is morally wrong because in theory it could loop around and bite me,
   but I lack in moral values and don't enjoy abusing pointers any further */
JSBool obj_hashcode(JSContext *cx, JSObject *obj, uintN argc,
                    jsval *argv, jsval *rval)
{
  JSBool has_prop;
  /* Need to check for property first to keep treehydra from getting angry */
  if (JS_AlreadyHasOwnProperty(cx, obj, "_hashcode", &has_prop) && has_prop) {
    JS_GetProperty(cx, obj, "_hashcode", rval);
  } else {
    static int counter = 0;
    char str[256];
    jsval val;
    snprintf (str, sizeof (str), "%x", ++counter);
    val = STRING_TO_JSVAL (JS_NewStringCopyZ (cx, str));
    JS_DefineProperty (cx, obj, "_hashcode", val,
                       NULL, NULL, JSPROP_PERMANENT | JSPROP_READONLY);
    *rval = val;
  }
  return JS_TRUE;
}
