/*
 * $Header: /proj/irix6.5.7m/isms/irix/lib/libirixpmda/src/RCS/no-malloc-audit.h,v 1.2 1997/01/27 00:02:14 chatz Exp $
 *
 * Remove the MALLOC AUDIT wrapper definitions.
 */

#ifdef MALLOC_AUDIT
#ifdef MALLOC_AUDIT_ON
#undef malloc
#undef free
#undef realloc
#undef calloc
#undef memalign
#undef valloc
#undef strdup
#undef _persistent_
#undef MALLOC_AUDIT_ON
#endif
#endif
