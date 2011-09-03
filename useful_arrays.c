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

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) TYPE,
#define END_OF_BASE_TREE_CODES

const enum tree_code_class tree_code_type[] = {
#include "all-tree.def"
};

#undef DEFTREECODE
#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) LENGTH,

const unsigned char tree_code_length[] = {
#include "all-tree.def"
};

#undef DEFTREECODE
#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) NAME,

const unsigned char tree_code_name[] = {
#include "all-tree.def"
};

#undef DEFTREECODE
