#ifndef GCC_CP_HEADERS
#define GCC_CP_HEADERS
/* Purpose of this header is to have 
   a) Easily generated js bindings
   b) have a short way to include all of the needed headers
*/
#include <config.h>
#include <system.h>
#include <coretypes.h>
#define GTY(x) __attribute__((user (("gty:"#x))))
#include <tm.h>
#include <tree.h>

/* special variables for convert_tree.js */
enum tree_code tree_code_var;

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
