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

#ifndef TREEHYDRA_H
#define TREEHYDRA_H

int treehydra_startup (struct Dehydra *this);
int set_after_gcc_pass(const char *pass);

typedef void (*treehydra_handler)(struct Dehydra *this, void *structure, struct JSObject *obj);
jsval get_lazy (struct Dehydra *this, treehydra_handler handler, void *v,
               struct JSObject *parent, const char *propname);
jsval get_existing_or_lazy (struct Dehydra *this, treehydra_handler handler, void *v,
                            struct JSObject *parent, const char *propname);
jsval get_enum_value (struct Dehydra *this, const char *name);

void lazy_tree_node (struct Dehydra *this, void *structure, struct JSObject *obj);

// these are generated code called from treehydra.c
void lazy_cgraph_node (struct Dehydra *this, void* void_var, struct JSObject *obj);
void lazy_tree_common (struct Dehydra *this, void* void_var, struct JSObject *obj);
// This stuff became important in gcc 4.5
void lazy_gimple_statement_d (struct Dehydra *this, void* void_var, struct JSObject *obj);

struct JSObject *dehydra_defineArrayProperty (struct Dehydra *this,
                                                  struct JSObject *obj,
                                                  char const *name,
                                                  int length);

void convert_int (struct Dehydra *this, struct JSObject *parent, const char *propname, HOST_WIDE_INT i);
void convert_char_star (struct Dehydra *this, struct JSObject *parent,
                        const char *propname, const char *str);
void treehydra_call_js (struct Dehydra *this, const char *callback,
                                   union tree_node *treeval);

extern int treehydra_debug;
#endif
