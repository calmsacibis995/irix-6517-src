#ifndef __EVCTR_UTIL_H__
#define __EVCTR_UTIL_H__
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>

#ifndef HWPERF_EVENTMAX
#define HWPERF_EVENTMAX 32
#endif

/*
 * describe all of the events, their internal code, a short mneumonic
 * and the full text of the counter's description
 */

typedef struct {
    int		ident;
    char	*name;
    char	*text;
} EventDesc_t;

extern EventDesc_t	*EventDesc;

/*
 * results of evctr_global_sample() ...
 */
extern int		ActiveChanged;
extern int		Active[HWPERF_EVENTMAX];
extern int		Mux[HWPERF_EVENTMAX];
extern int		Sem[HWPERF_EVENTMAX];
extern __uint64_t	OldCount[HWPERF_EVENTMAX];
extern __uint64_t	Count[HWPERF_EVENTMAX];
extern int		Use[2];		/* mux count for each physical ctr */

/*
 * CPU inventory by type
 */
typedef struct {
    int		r10k;
    int		r10k_rev2;
    int		r10k_rev3;
    int		r12k;
} cpucount_t;

extern cpucount_t cpucount;

/*
 * for Sem[]
 */
#define EVCTR_SEM_BAD		-1
#define EVCTR_SEM_OK		0
#define EVCTR_SEM_R10K_REV	1
#define EVCTR_SEM_R10K_R12K	2

/* global debug */
extern int		evctr_debug;
/* semantic warnings control */

void evctr_init(char *);
void evctr_opt(char *, int);
void evctr_trustme(void);

int evctr_global_set(void);
int evctr_global_status(FILE *);
int evctr_global_sample(int);
int evctr_global_release(void);

#endif /* !__EVCTR_UTIL_H__ */
