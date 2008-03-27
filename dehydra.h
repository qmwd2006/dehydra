#ifndef DEHYDRA_H
#define DEHYDRA_H

struct Dehydra {
  char* dir;
  JSRuntime *rt;
  JSContext *cx;
  JSObject *globalObj;
  JSObject *destArray;
  JSObject *rootedArgDestArray;
  JSObject *rootedFreeArray;
  // Where the statements go;
  JSObject *statementHierarchyArray;
  //keeps track of function decls to map gimplified ones to verbose ones
  struct pointer_map_t *fndeclMap;
  location_t loc;
  int inExpr;
};

typedef struct Dehydra Dehydra;

extern const char *NAME;
extern const char *LOC;
extern const char *BASES;
extern const char *DECL;
extern const char *ASSIGN;
extern const char *VALUE;
extern const char *TYPE;
extern const char *FUNCTION;
extern const char *RETURN;
extern const char *FCALL;
extern const char *ARGUMENTS;
extern const char *DH_CONSTRUCTOR;
extern const char *DH_EXTERN;
extern const char *FIELD_OF;
extern const char *MEMBERS;
extern const char *PARAMETERS;
extern const char *ATTRIBUTES;
extern const char *STATEMENTS;
extern const char *BITFIELD;

jsuint dehydra_getArrayLength(Dehydra *this, JSObject *array);
void dehydra_defineProperty(Dehydra *this, JSObject *obj,
                            char const *name, jsval value);
void dehydra_defineStringProperty(Dehydra *this, JSObject *obj,
                                  char const *name, char const *value);
JSObject *dehydra_defineArrayProperty (Dehydra *this, JSObject *obj,
                                       char const *name, int length);
void dehydra_init(Dehydra *this, const char *file);
int dehydra_startup (Dehydra *this, const char *script);
int dehydra_visitType(Dehydra *this, tree c);
void dehydra_visitDecl (Dehydra *this, tree f);
JSObject* dehydra_addVar(Dehydra *this, tree v, JSObject *parentArray);
void dehydra_input_end (Dehydra *this);
void dehydra_print(Dehydra *this, jsval val);
jsval dehydra_getToplevelFunction(Dehydra *this, char const *name);
void dehydra_addAttributes (Dehydra *this, JSObject *destArray,
                            tree attributes);
int dehydra_rootObject (Dehydra *this, jsval val);
void dehydra_unrootObject (Dehydra *this, int pos);
jsval dehydra_getRootedObject (Dehydra *this, int pos);
void dehydra_setLoc(Dehydra *this, JSObject *obj, tree t);
int dehydra_loadScript(Dehydra *this, const char *filename);
#endif
