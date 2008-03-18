#ifndef GCC_CP_HEADERS
#define GCC_CP_HEADERS
/* Purpose of this header is to have 
   a) Easily generated js bindings
   b) have a short way to include all of the needed headers
*/
#ifdef TREEHYDRA_CONVERT_JS
#define GTY(x) __attribute__((user (("gty:"#x))))
#endif

#include <config.h>
#include <system.h>
#include <coretypes.h>
#include <tm.h>
#include <tree.h>
#include <basic-block.h>
#include <cp-tree.h>
#include <cxx-pretty-print.h>
#include <tree-iterator.h>
#include <pointer-set.h>
#include <toplev.h>

#include <pointer-set.h>
#include <errors.h>
#include <diagnostic.h>
/*C++ headers*/
#include <cp-tree.h>
#include <cxx-pretty-print.h>

#endif
