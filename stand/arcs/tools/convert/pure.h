/* pure.h */
/* Header file for pure binary data dump format */

extern int	PureInitialize(int);
extern int	PureConvert(char *, int);
extern int	PureRead(char *, int, int);
extern int	PureWrite(void);
extern int	PureClose(int);
extern int	PureTerminate(void);
