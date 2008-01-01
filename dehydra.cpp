#include "dehydra.h"
#include "dehydra_js.h"

static JSClass global_class = {
  "global", JSCLASS_NEW_RESOLVE,
  JS_PropertyStub,  JS_PropertyStub,
  JS_PropertyStub,  JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub,
  JS_ConvertStub,   JS_FinalizeStub
};

static JSFunctionSpec shell_functions[] = {
    {"write_file",      Dehydra::WriteFile,      1},
    {"read_file",       Dehydra::ReadFile,       1},
    {"print",           Dehydra::Print,          0},
    {"version",         Dehydra::Version,        0},
    {"error",           Dehydra::Error,          1},
    {0}
};

Dehydra::Dehydra() {
  xassert((rt = JS_NewRuntime(0x9000000L)));
  xassert((cx = JS_NewContext(rt, 8192)));

  JS_SetContextPrivate(cx, this);
  
  globalObj = JS_NewObject(cx, &global_class, 0, 0);
  JS_InitStandardClasses(cx, globalObj);
  /* register error handler */
  //JS_SetErrorReporter(cx, printError);
  xassert(JS_DefineFunctions(cx, globalObj, shell_functions));
  //rootedArgDestArray = destArray = JS_NewArrayObject(cx, 0, NULL);
  //xassert(destArray && JS_AddRoot(cx, &rootedArgDestArray));

  //stateArray = JS_NewArrayObject(cx, 1, NULL);
  //xassert(stateArray && JS_AddRoot(cx, &stateArray));
  //state = JSVAL_VOID;

  //typeArrayArray = JS_NewArrayObject(cx, 0, NULL);
  //xassert(typeArrayArray && JS_AddRoot(cx, &typeArrayArray));

 // JS_SetVersion(cx, (JSVersion) 170);
 // loadScript(cmd.libScript);
 // loadScript(cmd.script);
}

static Dehydra dehydra;


int visitClass(tree c) {
  
  return 1;
}
