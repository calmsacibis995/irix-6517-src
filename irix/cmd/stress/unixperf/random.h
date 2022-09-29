
/* operating system independent (OSI) random number generation routines */

extern long OSIrandom(void);
extern void OSIsrandom(unsigned int seed);
extern char *OSIinitstate(unsigned int seed, char *state, int n);
extern char *OSIsetstate(char *state);
