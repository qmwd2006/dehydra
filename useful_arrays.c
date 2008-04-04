/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) TYPE,

const enum tree_code_class tree_code_type[] = {
#include "tree.def"
  tcc_exceptional,
#include "c-common.def"
};

#undef DEFTREECODE
#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) LENGTH,

const unsigned char tree_code_length[] = {
#include "tree.def"
  0,
#include "c-common.def"
};

#undef DEFTREECODE
#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) NAME,

const unsigned char tree_code_name[] = {
#include "tree.def"
  0,
#include "c-common.def"
};

#undef DEFTREECODE
