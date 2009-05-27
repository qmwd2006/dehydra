/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef DEHYDRA_H
#define DEHYDRA_H

struct Dehydra {
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
extern const char *MEMBER_OF;
extern const char *PRECISION;
extern const char *UNSIGNED;
extern const char *SIGNED;
extern const char *MIN_VALUE;
extern const char *MAX_VALUE;
extern const char *TEMPLATE;
extern const char *SYS;

extern JSClass js_type_class;

/* Drop-in replacement for JS_DefineObject, required as a workaround
 * because JS_DefineObject always sets the parent property. */
JSObject *definePropertyObject (JSContext *cx, JSObject *obj,
                               const char *name, JSClass *clasp,
                               JSObject *proto, uintN flags);

jsuint dehydra_getArrayLength(Dehydra *this, JSObject *array);
void dehydra_defineProperty(Dehydra *this, JSObject *obj,
                            char const *name, jsval value);
void dehydra_defineStringProperty(Dehydra *this, JSObject *obj,
                                  char const *name, char const *value);
JSObject *dehydra_defineArrayProperty (Dehydra *this, JSObject *obj,
                                       char const *name, int length);
JSObject *dehydra_defineObjectProperty (Dehydra *this, JSObject *obj,
                                        char const *name);
void dehydra_init(Dehydra *this, const char *file);
int dehydra_startup (Dehydra *this);
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
void convert_location_t (struct Dehydra *this, struct JSObject *parent,
                         const char *propname, location_t loc);

int dehydra_includeScript(Dehydra *this, const char *filename);
void dehydra_appendToPath (Dehydra *this, const char *dir);
void dehydra_appendDirnameToPath (Dehydra *this, const char *filename);
FILE *dehydra_searchPath (Dehydra *this, const char *filename, char **realname);
bool isGPlusPlus ();
void dehydra_setFilename(Dehydra *this);
#endif
