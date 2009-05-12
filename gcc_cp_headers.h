/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef GCC_CP_HEADERS
#define GCC_CP_HEADERS
/* Purpose of this header is to have 
   a) Easily generated js bindings
   b) have a short way to include all of the needed headers
      b1) This file is included by generated code which can't stomach JS headers
      b2) This file is included by other code which does want JS headers
*/
#ifdef TREEHYDRA_CONVERT_JS
#define GTY(x) __attribute__((user (("gty:"#x))))
#endif

#include <config.h>
#include <system.h>
#include <coretypes.h>
#include <tm.h>
#include <tree.h>

#if defined(TREEHYDRA_CONVERT_JS) || defined(TREEHYDRA_GENERATED)
/* this header conflicts with spidermonkey. sorry for above code */
#include <basic-block.h>
#include <tree-flow.h>
#endif

#include "cp-tree-jsapi-workaround.h"
#include <cxx-pretty-print.h>
#include <tree-iterator.h>
#include <pointer-set.h>
#include <toplev.h>

#include <pointer-set.h>
#include <diagnostic.h>
/*C++ headers*/
#include <cp-tree.h>
#include <cxx-pretty-print.h>

#endif
