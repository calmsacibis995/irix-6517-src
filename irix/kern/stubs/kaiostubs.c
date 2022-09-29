extern int nopkg(void);
void	kaio_init () {}
void	kaio_done () {}
void	kaio_exit () {}
int	kaio_rw () { return nopkg(); }
int	kaio_user_init () { return nopkg(); }
void	kaio_stats () { }
void	kaio_clearstats () { }
int	kaio_suspend () { return nopkg(); }
int	kaio_installed () { return 0; }
void	kaio_getparams () {}
void	kaio_setparams () {}
