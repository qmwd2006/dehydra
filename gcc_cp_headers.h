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

#if !defined(TREEHYDRA_CONVERT_JS) && !defined(TREEHYDRA_GENERATED)
#include <jsapi.h>
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

/*C++ headers*/
#include "cp-tree-jsapi-workaround.h"
#include <cp/cxx-pretty-print.h>
#include <tree-iterator.h>
#include <pointer-set.h>
#include <toplev.h>

#include <pointer-set.h>
#include <diagnostic.h>

#endif
