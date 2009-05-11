/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef XASSERT_H
#define XASSERT_H

#define xassert(cond)                                                   \
  if (!(cond)) {                                                        \
    fprintf(stderr, "%s:%d: Assertion failed:" #cond                    \
            ". \nIf the file compiles correctly without invoking "      \
            "dehydra please file a bug, include a testcase or .ii file" \
            " produced with -save-temps\n",                             \
            __FILE__, __LINE__);                                        \
    _exit(1);                                                           \
  }

#endif
