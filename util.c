/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#include <config.h>
#include <system.h>
#include <coretypes.h>
#include <tm.h>
#include <tree.h>
#include <cp-tree.h>
#include <cxx-pretty-print.h>
#include <tree-iterator.h>
#include <pointer-set.h>
#include <toplev.h>

#include "util.h"

static char locationbuf[PATH_MAX];

/* stolen from gcc/cp/error.h */
location_t
location_of (tree t)
{
  if (TREE_CODE (t) == PARM_DECL && DECL_CONTEXT (t))
    t = DECL_CONTEXT (t);
  else if (TYPE_P (t))
    t = TYPE_MAIN_DECL (t);
  else if (TREE_CODE (t) == OVERLOAD)
    t = OVL_FUNCTION (t);

  if (DECL_P(t))
    return DECL_SOURCE_LOCATION (t);
  else if (EXPR_P(t) && EXPR_HAS_LOCATION(t))
    return EXPR_LOCATION(t);
  else
    return UNKNOWN_LOCATION;
}

char const * loc_as_string (location_t loc) {
  if (loc_is_unknown(loc)) return NULL;
  expanded_location eloc = expand_location(loc);
#ifdef __APPLE__ 
  // XXX remove once Apple GCC supports columns
  sprintf(locationbuf, "%s:%d", eloc.file, eloc.line);
#else
  // a function, so locations are likely to be mapped, so we have columns
  sprintf(locationbuf, "%s:%d:%d", eloc.file, eloc.line, eloc.column);
#endif
  return locationbuf;
}
