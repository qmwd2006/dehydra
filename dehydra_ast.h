/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef DEHYDRA_AST_H
#define DEHYDRA_AST_H

void dehydra_cp_pre_genericize(Dehydra *this, tree fndecl, bool callJS);
JSObject* dehydra_makeVar (Dehydra *this, tree t, 
                           char const *prop, JSObject *attachToObj);
#endif
