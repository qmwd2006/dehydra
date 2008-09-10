/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef XASSERT_H
#define XASSERT_H

#define xassert(cond)                                                   \
  if (!(cond)) {                                                        \
    fprintf(stderr, "%s:%d: Assertion failed:" #cond "\n",  __FILE__, __LINE__); \
    _exit(1);                                                           \
  }

#endif
