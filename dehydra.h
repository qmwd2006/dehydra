#ifndef DEHYDRA_H
#define DEHYDRA_H
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"

#ifdef __cplusplus
extern "C" {
#endif
  void initDehydra(const char *arg);
  int visitClass(tree c);

#ifdef __cplusplus
 }
#endif

#endif // DEHYDRA_H
