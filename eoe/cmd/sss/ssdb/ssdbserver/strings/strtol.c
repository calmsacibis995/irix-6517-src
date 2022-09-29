/* Copyright (C) 1996  TCX DataKonsult AB & Monty Program KB & Detron HB
   For a more info consult the file COPYRIGHT distributed with this file */

/* This defines strtol() if neaded */

#include <global.h>
#if !defined(MSDOS) && !defined(HAVE_STRTOUL) && !defined(__WIN32__)
#include "strto.c"
#endif
