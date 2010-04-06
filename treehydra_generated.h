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
void lazy_tree_string (struct Dehydra *this, void* void_var, struct JSObject *obj);

#include "treehydra.h"

#endif
