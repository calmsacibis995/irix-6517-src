/* Copyright (C) 1996  TCX DataKonsult AB & Monty Program KB & Detron HB
   For a more info consult the file COPYRIGHT distributed with this file */

/* This is defines strtoul() if neaded */

#include <global.h>
#if !defined(MSDOS) && !defined(HAVE_STRTOUL)
#define UNSIGNED
#include "strto.c"
#endif
