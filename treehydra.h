/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef TREEHYDRA_H
#define TREEHYDRA_H

int treehydra_startup (struct Dehydra *this);
void treehydra_plugin_pass (struct Dehydra *this);
int set_after_gcc_pass(const char *pass);

typedef void (*treehydra_handler)(struct Dehydra *this, void *structure, struct JSObject *obj);
jsval get_lazy (struct Dehydra *this, treehydra_handler handler, void *v,
               struct JSObject *parent, const char *propname);
jsval get_existing_or_lazy (struct Dehydra *this, treehydra_handler handler, void *v,
                            struct JSObject *parent, const char *propname);
jsval get_enum_value (struct Dehydra *this, const char *name);

void lazy_tree_node (struct Dehydra *this, void *structure, struct JSObject *obj);

struct JSObject *dehydra_defineArrayProperty (struct Dehydra *this,
                                                  struct JSObject *obj,
                                                  char const *name,
                                                  int length);

void convert_tree_node_union (struct Dehydra *this, enum tree_node_structure_enum code,
                              union tree_node *var, struct JSObject *obj);
void convert_int (struct Dehydra *this, struct JSObject *parent, const char *propname, int i);
void convert_location_t (struct Dehydra *this, struct JSObject *parent,
                         const char *propname, location_t loc);
void convert_char_star (struct Dehydra *this, struct JSObject *parent,
                        const char *propname, const char *str);
#endif
