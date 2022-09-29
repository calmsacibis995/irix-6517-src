/*
 * Definitions shared among profiler utilities
 */

typedef uint64_t 	symaddr_t;

extern int rdsymtab(int, void (*)(char *, symaddr_t));
extern int rdelfsymtab(int, void (*)(char *, symaddr_t));
extern int list_repeated(symaddr_t);

extern void error(char *);
