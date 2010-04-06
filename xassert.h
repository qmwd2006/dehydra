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

#ifndef XASSERT_H
#define XASSERT_H

#define xassert(cond)                                                   \
  if (!(cond)) {                                                        \
    fprintf(stderr, "%s:%d: Assertion failed:" #cond                    \
            ". \nIf the file compiles correctly without invoking "      \
            "dehydra please file a bug, include a testcase or .ii file" \
            " produced with -save-temps\n",                             \
            __FILE__, __LINE__);                                        \
    _exit(1);                                                           \
  }

#endif
