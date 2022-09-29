extern int nopkg(void);
void	pwexit () {}
int	pwsysmips () { return nopkg(); }
int	postwait_installed () { return 0; }
void	postwait_getparams () {}
