/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* A workaround for weird conflict between Apple GCC 4.2 Preview 1's cp-tree.h
   and SpiderMonkey's jsotypes.h */
#if !defined(TREEHYDRA_CONVERT_JS) && !defined(TREEHYDRA_GENERATED)
// do not include JS in special and therefore fragile files of treehydra
#include "jsapi.h"
#endif

#ifdef JSVAL_OBJECT
// workaround only when JS was actually included
#ifndef uint32
#define DEHYDRA_DEFINED_uint32
#define uint32 uint32
#endif
#endif // JSVAL_OBJECT
#include <cp/cp-tree.h>
#ifdef JSVAL_OBJECT
#ifdef DEHYDRA_DEFINED_uint32
#undef uint32
#undef DEHYDRA_DEFINED_uint32
#endif
#endif // JSVAL_OBJECT
