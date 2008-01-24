#ifndef DEHYDRA_CALLBACKS_H
#define DEHYDRA_CALLBACKS_H

int initDehydra (const char *file, const char *arg);
void visitClass (tree c);
void visitFunction (tree f);
void cp_pre_genericizeDehydra (tree fndecl);
void input_endDehydra ();
#endif
