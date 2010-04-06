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

#include "gcc_compat.h"

#include "xassert.h"
#include "util.h"

#include <inttypes.h>

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

  if (!t)
    return UNKNOWN_LOCATION;

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

/* this is to idicate that global_namespace isn't linked in */
tree global_namespace = NULL;

bool isGPlusPlus() {
  return global_namespace != NULL;
}

const char *
class_key_or_enum_as_string (tree t)
{
  if (TREE_CODE (t) == ENUMERAL_TYPE)
    return "enum";
  else if (TREE_CODE (t) == UNION_TYPE)
    return "union";
  else if (isGPlusPlus() && TYPE_LANG_SPECIFIC (t) && CLASSTYPE_DECLARED_CLASS (t))
    return "class";
  else
    return "struct";
}

const char *
decl_as_string (tree decl, int flags)
{
  tree aname = DECL_P (decl) ? DECL_NAME (decl) : decl;
  return aname ? IDENTIFIER_POINTER (aname) : "";
}

const char *
expr_as_string (tree decl, int flags)
{
  static char buf[256];
  if (!decl)
    return "";
  else if (TREE_CODE (decl) == INTEGER_CST) {
    return dehydra_intCstToString(decl);
  } else {
    sprintf(buf, "?%s?", tree_code_name[TREE_CODE(decl)]);
  }
  return buf;
}

const char *
type_as_string (tree typ, int flags)
{
  return decl_as_string (TYPE_NAME(typ), 0);
}

void
lang_check_failed (const char* file, int line, const char* function)
{
  internal_error ("lang_* check: failed in %s, at %s:%d",
                  function, trim_filename (file), line);
}

/* Convert an INTEGER_CST to a string representation. This is used
 * because GCC expr_as_string is broken for unsigned ints. */
const char *dehydra_intCstToString(tree int_cst) 
{
  static char buf[32];  // holds repr of up to 64-bit ints
  xassert(TREE_CODE(int_cst) == INTEGER_CST);
  tree type = TREE_TYPE(int_cst);
  int is_unsigned = TYPE_UNSIGNED(type);
#if HOST_BITS_PER_WIDE_INT > 32
  // TREE_INT_CST_LOW(int_cst) is a 64-bit integer here
  sprintf(buf, is_unsigned ? "%"PRIu64"u" : "%"PRId64,
          TREE_INT_CST_LOW(int_cst));
#else
  int high = TREE_INT_CST_HIGH(int_cst);
  int low = TREE_INT_CST_LOW(int_cst);
  if (high == 0 || (high == -1 && !is_unsigned)) {
    /* GCC prints negative signed numbers in hex, we print using %d.
       GCC prints unsigned numbers as if signed, we really do unsigned. */
    sprintf(buf, is_unsigned ? "%uu" : "%d", low);
  } else {
    /* GCC prints negative 64-bit constants in hex, we want %d.
       GCC prints large positive unsigned 64-bit constants in hex, we want %u */
    sprintf(buf, is_unsigned ? "%lluu" : "%lld",
            ((long long)high << 32) | (0xffffffffll & low));
  }
#endif

  if (type == long_integer_type_node || type == long_unsigned_type_node)
    strcat(buf, "l");
  else if (type == long_long_integer_type_node ||
           type == long_long_unsigned_type_node)
    strcat(buf, "ll");

  return buf;
}

// compatability shim
enum cp_tree_node_structure_enum
cp_tree_node_structure (union lang_tree_node *_) {
  xassert(false);
  return LAST_TS_ENUM;
}

tree *
decl_cloned_function_p (const_tree decl, bool just_testing) {
  xassert (0);
}
