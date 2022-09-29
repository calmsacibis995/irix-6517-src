/* type-definitions
   (c) 1992 John Tromp
*/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#define allocate(N, T)  ((T *)calloc((unsigned)N,sizeof(T)))
#define UNK		2
#define WIN2		3
#define DRIN2		4
#define DRAW		5
#define DRIN1		6
#define WIN1		7
#define EXACT(S)	((S)==WIN2 || (S)==DRAW || (S)==WIN1)
#define SCORE(X)	((X) & 7)
#define WORK(X)		((X) >> 3)
#define LASTMOVE        moves[plycnt-1]

#define SMALL /* for unixperf, use small data types; slightly faster with -DSMALL */
#ifdef SMALL
typedef u_char uint8;
typedef u_short uint16;
#else
typedef int uint8;
typedef int uint16;
#endif
typedef uint8 B8[8];
typedef u_char hashentry;
#ifdef MOD64
typedef long long uint64;
#endif

extern int window;
extern u_int nodes;
extern void initbest(void), inittrans(void), initplay(void);
extern void cleantrans(void);
extern void newgame(void);
extern int depth(int n);
extern void makemovx(int n), makemovo(int n);
extern int haswon(void);
