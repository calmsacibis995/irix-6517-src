
#include "unixperf.h"

/* test_cpu.c */

extern unsigned int DoBcopy256(void);
extern unsigned int DoBcopy16K(void);
extern unsigned int DoBcopy100K(void);
extern unsigned int DoBcopyMeg(void);
extern unsigned int DoBcopy10Meg(void);
extern unsigned int DoBcopy10Mega(void);
extern unsigned int DoBcopy10Megu(void);
extern unsigned int DoBcopy10Megudw(void);

extern unsigned int DoBzero256(void);
extern unsigned int DoBzero16K(void);
extern unsigned int DoBzero100K(void);
extern unsigned int DoBzeroMeg(void);
extern unsigned int DoBzero10Meg(void);
extern unsigned int DoBzero10Mega(void);
extern unsigned int DoBzero10Megu(void);
extern unsigned int DoBzero10Megudw(void);

extern unsigned int DoDeepRecursion(void);

extern unsigned int DoMatrixMultFloat(void);
extern unsigned int DoMatrixMultDouble(void);

extern Bool InitQsort400(Version version);
extern Bool InitQsort16K(Version version);
extern Bool InitQsort64K(Version version);
extern unsigned int DoQsort(void);
extern void CleanupQsort(void);

extern unsigned int DoTrigFuncsFloat(void);
extern unsigned int DoTrigFuncsDouble(void);

extern unsigned int DoHanoi10(void);
extern unsigned int DoHanoi15(void);

/* Really in dhry_1.c (with assistance from dhry_2.c) */
extern Bool InitDhrystone(Version version);
extern unsigned int DoDhrystone(void);
extern void CleanupDhrystone(void);

