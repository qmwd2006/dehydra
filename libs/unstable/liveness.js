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

/** Liveness analysis.. */

function LivenessAnalysis() {
  BackwardAnalysis.apply(this, arguments);
}

LivenessAnalysis.prototype = new BackwardAnalysis;

LivenessAnalysis.prototype.flowState = function(isn, state) {
  switch (gimple_code(isn)) {
  case RETURN_EXPR:
    // 4.3 issue: Need to specially handle embedded trees within RETURN_EXPRs
    let gms = TREE_OPERAND(isn, 0);
    if (gms) {
      // gms is usually a GIMPLE_MODIFY_STMT but can be a RESULT_DECL
      if (TREE_CODE(gms) == GIMPLE_MODIFY_STMT) {
        let v = GIMPLE_STMT_OPERAND(gms, 1);
        state.add(v);
      } else if (TREE_CODE(gms) == RESULT_DECL) {
        // TODO figure out what really happens here
        // Presumably we already saw the assignment to it.
      }
    }
    break;
  case GIMPLE_RETURN:
  case GIMPLE_ASSIGN:
  case GIMPLE_COND:
  case GIMPLE_SWITCH:
  case GIMPLE_CALL:
    for (let e in isn_defs(isn, 'strong')) {
      if (DECL_P(e)) {
        state.remove(e);
      }
    }
    for (let e in isn_uses(isn)) {
      if (DECL_P(e)) {
        state.add(e);
      }
    }
    break;
  default:
    break;
  }
};
