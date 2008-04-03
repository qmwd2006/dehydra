/* A workaround for weird conflict between Apple GCC 4.2 Preview 1's cp-tree.h
   and SpiderMonkey's jsotypes.h */
#include "jsapi.h"

#ifndef uint32
#define DEHYDRA_DEFINED_uint32
#define uint32 uint32
#endif
#include <cp-tree.h>
#ifdef DEHYDRA_DEFINED_uint32
#undef uint32
#undef DEHYDRA_DEFINED_uint32
#endif
