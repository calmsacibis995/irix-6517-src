/* Header file for intel hex format output */

extern int	IntelhexInitialize(int);
extern int	IntelhexConvert(char *, int);
extern int	StepConvert(char *, int);
extern int	IntelhexWrite(void);
extern int	IntelhexTerminate(void);
