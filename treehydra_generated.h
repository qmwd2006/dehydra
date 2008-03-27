#ifndef TREEHYDRA_GENERATED
#define TREEHYDRA_GENERATED
/* This file isn't generated, but it's used by the generated one*/
#include "gcc_cp_headers.h"
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

#include "treehydra.h"

#endif
