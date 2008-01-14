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
  location_t location_of (tree t);
  char const * loc_as_string (location_t loc);

  void initDehydra(const char *file, const char *arg);
  VISIT_DECL(Class, tree c);
  VISIT_DECL(Function, tree f);
  void dehydra_cp_pre_genericize(tree fndecl);
#ifdef __cplusplus
 }
#endif

#endif // DEHYDRA_H
