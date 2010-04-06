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
/* A workaround for weird conflict between Apple GCC 4.2 Preview 1's cp-tree.h
   and SpiderMonkey's jsotypes.h */
#if !defined(TREEHYDRA_CONVERT_JS) && !defined(TREEHYDRA_GENERATED)
// do not include JS in special and therefore fragile files of treehydra
#include <jsapi.h>
#endif

#ifdef JSVAL_OBJECT
// workaround only when JS was actually included
#ifndef uint32
#define DEHYDRA_DEFINED_uint32
#define uint32 uint32
#endif
#endif // JSVAL_OBJECT
#include <cp/cp-tree.h>
#ifdef JSVAL_OBJECT
#ifdef DEHYDRA_DEFINED_uint32
#undef uint32
#undef DEHYDRA_DEFINED_uint32
#endif
#endif // JSVAL_OBJECT
