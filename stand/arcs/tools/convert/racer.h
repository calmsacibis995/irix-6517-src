/* racer.h */
/* Header file for pure binary data dump format */

extern int	RacerConvert(char *, int);
extern int	RacerRead(char *, int, int);
extern int	RacerClose(int);
extern int	RacerSetVersion(int);

extern int	RacerRpromInitialize(int);
extern int	RacerRpromWrite(void);
extern int	RacerRpromTerminate(void);

extern int	RacerFpromInitialize(int);
extern int	RacerFpromWrite(void);
extern int	RacerFpromTerminate(void);
