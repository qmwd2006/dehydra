#ifndef DEHYDRA_CALLBACKS_H
#define DEHYDRA_CALLBACKS_H

int initDehydra (const char *file, const char *arg);
void visitClass (tree c);
void visitDecl (tree f);
void cp_pre_genericizeDehydra (tree fndecl, bool callJS);
void input_endDehydra ();
void plugin_passDehydra ();
#endif
