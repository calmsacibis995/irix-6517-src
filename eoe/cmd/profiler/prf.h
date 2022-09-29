/*
 * Definitions shared among profiler utilities
 */

typedef __psunsigned_t 	symaddr_t;

#ifdef DOELF
extern int rdsymtab(int, void (*)(char *, symaddr_t));
extern int rdelfsymtab(int, void (*)(char *, symaddr_t));
extern int list_repeated(symaddr_t);
#endif

extern void error(char *);
