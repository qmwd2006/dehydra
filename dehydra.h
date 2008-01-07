#ifndef DEHYDRA_H
#define DEHYDRA_H
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"

#define VISIT_DECL(name, param)                 \
  int visit##name(param);                       \
  void postvisit##name(param);

#ifdef __cplusplus
extern "C" {
#endif
  // defined in dehydra_main
  char const * loc(tree t);

  void initDehydra(const char *file, const char *arg);
  VISIT_DECL(Class, tree c);
  VISIT_DECL(Function, tree f);
  void dehydra_cp_pre_genericize(tree fndecl);
#ifdef __cplusplus
 }
#endif

#endif // DEHYDRA_H
