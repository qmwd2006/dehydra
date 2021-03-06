/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Dehydra and Treehydra scriptable static analysis tools
 * Copyright (C) 2007-2010 The Mozilla Foundation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef DEHYDRA_H
#define DEHYDRA_H

#ifndef JS_TYPED_ROOTING_API
#define JS_AddObjectRoot(cx, obj) JS_AddRoot(cx, obj)
#define JS_AddStringRoot(cx, str) JS_AddRoot(cx, str)
#define JS_AddNamedObjectRoot(cx, obj, name) JS_AddNamedRoot(cx, obj, name)
#define JS_RemoveObjectRoot(cx, obj) JS_RemoveRoot(cx, obj)
#define JS_RemoveStringRoot(cx, str) JS_RemoveRoot(cx, str)
#endif

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
extern const char *DH_EXPLICIT;
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
extern const char *HAS_DEFAULT;
extern const char *TEMPLATE;
extern const char *SYS;
extern const char *ACCESS;
extern const char *PUBLIC;
extern const char *PRIVATE;
extern const char *PROTECTED;

extern JSClass js_type_class;

/* Drop-in replacement for JS_DefineObject, required as a workaround
 * because JS_DefineObject always sets the parent property. */
JSObject *definePropertyObject (JSContext *cx, JSObject *obj,
                               const char *name, JSClass *clasp,
                               JSObject *proto, uintN flags);

jsuint dehydra_getArrayLength(Dehydra *this, JSObject *array);
void dehydra_defineProperty(Dehydra *this, JSObject *obj,
                            char const *name, jsval value);
jsval dehydra_defineStringProperty(Dehydra *this, JSObject *obj,
                                  char const *name, char const *value);
JSObject *dehydra_defineArrayProperty (Dehydra *this, JSObject *obj,
                                       char const *name, int length);
JSObject *dehydra_defineObjectProperty (Dehydra *this, JSObject *obj,
                                        char const *name);
void dehydra_init(Dehydra *this, const char *file, const char *version);
int dehydra_startup (Dehydra *this);
int dehydra_visitType(Dehydra *this, tree c);
void dehydra_visitDecl (Dehydra *this, tree f);
void dehydra_moveDefaults (Dehydra *this, JSObject *obj);
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
