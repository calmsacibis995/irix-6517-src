/* tune.h
 *  - Used in conjuction with gconf configuration files to declare tuneable
 *  standaline variables, with defaults.  This should only be used by libsk
 *  programs.
 *
 * "$Revision: 1.4 $"
 */

/* declare malloc arena (part of libsc) */
#ifndef _LIBSK
#ifndef malloc_arena
#define malloc_arena		0x80000
#endif

long long malbuf[(malloc_arena + sizeof(long long) - 1) / sizeof(long long)];
int _max_malloc = sizeof(malbuf);	/* most we can ever allocate at once */
#endif

/* dummy until libsk tuneables arrive */
#ifdef _KERNEL
extern int _ohboy;
#endif
