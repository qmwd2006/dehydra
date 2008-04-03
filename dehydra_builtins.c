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

JSBool require_version(JSContext *cx, jsval val) {
  JSString *version_str = JS_ValueToString(cx, val);
  if (!version_str) return JS_FALSE;
  JS_AddRoot(cx, &version_str);
  char *version_cstr = JS_GetStringBytes(version_str);
  JSVersion version = JS_StringToVersion(version_cstr);
  JSBool retval;
  if (version == JSVERSION_UNKNOWN) {
    JS_ReportError(cx, "Invalid version '%s'", version_cstr);
    retval = JS_FALSE;
  } else {
    JS_SetVersion(cx, version);
    retval = JS_TRUE;
  }
  JS_RemoveRoot(cx, &version_str);
  return retval;
}

JSBool require_option(JSContext *cx, jsval val, uint32 option) {
  JSBool flag;
  if (!JS_ValueToBoolean(cx, val, &flag)) return JS_FALSE;
  if (flag) {
    JS_SetOptions(cx, JS_GetOptions(cx) | option);
  } else {
    JS_SetOptions(cx, JS_GetOptions(cx) & ~option);
  }
  return JS_TRUE;
}

JSBool dispatch_require(JSContext *cx, const char *prop_name, jsval prop_val) {
  if (strcmp(prop_name, "version") == 0) {
    return require_version(cx, prop_val);
  } else if (strcmp(prop_name, "strict") == 0) {
    return require_option(cx, prop_val, JSOPTION_STRICT);
  } else if (strcmp(prop_name, "werror") == 0) {
    return require_option(cx, prop_val, JSOPTION_WERROR);
  } else {
    JS_ReportWarning(cx, "Unrecognized require keyword '%s'", prop_name);
    return JS_TRUE;
  }
}

/* Helper to return the current version as a JS string. */
jsval get_version(JSContext *cx)
{
  const char *version_cstr = JS_VersionToString(JS_GetVersion(cx));
  if (version_cstr == NULL) {
    return JSVAL_VOID;
  }
  JSString *version_str = JS_NewStringCopyZ(cx, version_cstr);
  return STRING_TO_JSVAL(version_str);
}

JSBool Require(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval)
{
  JSObject *args;
  if (!JS_ConvertArguments(cx, argc, argv, "o", &args)) return JS_FALSE;
  JSIdArray *prop_ids = JS_Enumerate(cx, args);
  if (!prop_ids) return JS_FALSE;

  /* Apply the options. */
  JSBool retval = JS_TRUE;
  int i;
  for (i = 0; i < prop_ids->length; ++i) {
    jsval prop;
    JSBool rv = JS_IdToValue(cx, prop_ids->vector[i], &prop);
    xassert(rv);
    JS_AddRoot(cx, &prop);
    JSString *prop_str = JSVAL_TO_STRING(prop);
    char *prop_name = JS_GetStringBytes(prop_str);
    jsval prop_val;
    rv = JS_GetProperty(cx, args, prop_name, &prop_val);
    xassert(rv);
    JS_AddRoot(cx, &prop_val);

    rv = dispatch_require(cx, prop_name, prop_val);
    if (rv == JS_FALSE) retval = JS_FALSE;

    JS_RemoveRoot(cx, &prop_val);
    JS_RemoveRoot(cx, &prop);
  }
  JS_DestroyIdArray(cx, prop_ids);
  if (!retval) return retval;

  /* Report the now-current options. */
  JSObject *rvalo = JS_NewObject(cx, NULL, NULL, NULL);
  if (!rvalo) return JS_FALSE;
  *rval = OBJECT_TO_JSVAL(rvalo);
  JS_DefineProperty(
      cx, rvalo, "version", get_version(cx), NULL, NULL, JSPROP_ENUMERATE);
  uint32 options = JS_GetOptions(cx);
  JS_DefineProperty(
      cx, rvalo, "strict", (options | JSOPTION_STRICT) ? JS_TRUE : JS_FALSE, 
      NULL, NULL, JSPROP_ENUMERATE);
  JS_DefineProperty(
      cx, rvalo, "werror", (options | JSOPTION_WERROR) ? JS_TRUE : JS_FALSE, 
      NULL, NULL, JSPROP_ENUMERATE);
  return JS_TRUE;
}

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

/* Report an error.
 * If we are currently inside JS, we'll report an error to JS. But
 * otherwise, we'll report it to the user and then exit. */
void reportError(JSContext *cx, const char *file, int line, 
                 const char *fmt, ...) 
{
  char msg[1024];
  const int size = sizeof(msg) / sizeof(msg[0]);
  va_list ap;
  va_start(ap, fmt);
  int nw = vsnprintf(msg, size, fmt, ap);
  va_end(ap);
  if (nw >= size) msg[size-1] = '\0';
  
  if (JS_IsRunning(cx)) {
    JS_ReportError(cx, "%s (from %s:%d)", msg, file, line);
  } else {
    fflush(stdout);
    fprintf(stderr, "%s:%d: Error: %s\n", file, line, msg);
    exit(1);
  }
}

void
ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
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
  const char *filename;
  JSBool rv = JS_ConvertArguments(cx, argc, argv, "s", &filename);
  if (!rv) return JS_FALSE;

  long size = 0;
  char *buf = readFile (filename, &size);
  if(!buf) {
    REPORT_ERROR_2(cx, "read_file: error opening file '%s': %s",
                   filename, strerror(errno));
    return JS_FALSE;
  }
  *rval = STRING_TO_JSVAL(JS_NewString(cx, buf, size));
  return JS_TRUE;
}

JSBool WriteFile(JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval) {
  const char *filename;
  JSString *str;
  JSBool rv = JS_ConvertArguments(cx, argc, argv, "sS", &filename, &str);
  if (!rv) return JS_FALSE;

  FILE *f = fopen (filename, "w");
  if (!f) {
    REPORT_ERROR_2(cx, "write_file: error opening file '%s': %s",
                   filename, strerror(errno));
    return JS_FALSE;
  }
  fwrite (JS_GetStringBytes(str), 1, JS_GetStringLength(str), f);
  fclose (f);
  return JS_TRUE;
}

char *readEntireFile(FILE *f, long *size) {
  xassert(f);
  if (fseek(f, 0, SEEK_END)) return NULL;
  *size = ftell(f);
  if (fseek(f, 0, SEEK_SET)) return NULL;
  char *buf = xmalloc(*size + 1);
  xassert(*size == fread(buf, 1, *size, f));
  buf[*size] = 0;
  fclose(f);
  return buf;
}

/* Read the entire contents of a file.
 *      path   path of the file to read
 *      size   (out) number of bytes of file data read
 *    return   null-terminated file contents, or NULL on error. Caller
 *             should free when done. */
char *readFile(const char *path, long *size) {
  FILE *f = fopen(path, "r");
  if (!f) return NULL;
  return readEntireFile(f, size);
}

/* Find a file, searching another dir if necessary.  If the file is
 * found, return a file handle open for reading and store the malloc'd
 * name where the file was found in realname. Otherwise, return
 * NULL. */
FILE *findFile(const char *filename, const char *dir, char **realname) {
  FILE *f = fopen(filename, "r");
  if (f) {
    *realname = xstrdup(filename);
    return f;
  }
  if (dir && dir[0] && filename[0] && filename[0] != '/') {
    char *buf = xmalloc(strlen(dir) + strlen(filename) + 2);
    /* Doing a little extra work here to get rid of unneeded '/'. */
    char *sep = dir[strlen(dir)-1] == '/' ? "" : "/";
    sprintf(buf, "%s%s%s", dir, sep, filename);
    f = fopen(buf, "r");
    if (f) {
      *realname = buf;
      return f;
    } else {
      free(buf);
    }
  }
  return NULL;
}

JSBool Include(JSContext *cx, JSObject *obj, uintN argc,
               jsval *argv, jsval *rval) {
  const char *filename;
  JSBool rv = JS_ConvertArguments(cx, argc, argv, "s", &filename);
  if (!rv) return JS_FALSE;

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
