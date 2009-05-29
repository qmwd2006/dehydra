/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

// lazy_cgraph_node and lazy_gimple_statement_d are union-access utility functions done by hand
void lazy_cgraph_node (struct Dehydra *this, void* void_var, struct JSObject *obj);
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
#endif
