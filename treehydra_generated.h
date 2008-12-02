/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef TREEHYDRA_GENERATED
#define TREEHYDRA_GENERATED
/* This file isn't generated, but it's used by the generated one*/
#include "gcc_cp_headers.h"
#include "gcc_compat.h"
// cleanup some gcc polution in GCC trunk as of Mar 5, 2008
#ifdef in_function_try_handler
#undef in_function_try_handler
#endif

#ifdef in_base_initializer
#undef in_base_initializer
#endif

#ifdef profile_status
#undef profile_status
#endif

struct JSObject;
typedef void* jsval;
#define JSVAL_NULL NULL

struct Dehydra;
void dehydra_defineProperty(struct Dehydra *this, struct JSObject *obj,
                            char const *name, jsval value);
void dehydra_defineStringProperty(struct Dehydra *this, struct JSObject *obj,
                                  char const *name, char const *value);
struct JSObject *dehydra_defineObjectProperty (struct Dehydra *this, struct JSObject *obj,
                                               char const *name);
void convert_location_t (struct Dehydra *this, struct JSObject *parent,
                         const char *propname, location_t loc);

#include "treehydra.h"

#endif
