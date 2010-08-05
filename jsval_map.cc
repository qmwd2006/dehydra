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

#include <map>
#include <jsapi.h>

extern "C" {
#include "jsval_map.h"

typedef std::map<void*, jsval> Map;

jsval_map* jsval_map_create() {
  Map* m = new Map;
  return reinterpret_cast<jsval_map*> (m);
}

void jsval_map_destroy(jsval_map* map) {
  Map* m = reinterpret_cast<Map*> (map);
  delete m;
}

void jsval_map_put(jsval_map* map, void* k, jsval v) {
  Map* m = reinterpret_cast<Map*> (map);
  m->insert(std::pair<void*, jsval>(k, v));
}

bool jsval_map_get(jsval_map* map, void* k, jsval* ret) {
  Map* m = reinterpret_cast<Map*> (map);
  Map::const_iterator it = m->find(k);
  if (it != m->end()) {
    *ret = it->second;
    return true;
  }
  return false;
}

}
