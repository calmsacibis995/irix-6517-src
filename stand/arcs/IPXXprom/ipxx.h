
/* Use weak symbols for system calls */
#define l_open(a,b,c)		_open(a,b,c)
#define l_ioctl(a,b,c)		_ioctl(a,b,c)
#define l_select(a,b,c,d,e)	_select(a,b,c,d,e)
#define l_read(a,b,c)		_read(a,b,c)
#define l_write(a,b,c)		_write(a,b,c)
#define l_pipe(a)		_pipe(a)
#define l_close(a)		_close(a)
#define l_dup(a)		_dup(a)
#define l_exit(a)		exit(a)
#define l_execve(a,b,c)		_execve(a,b,c)
#define l_sginap(a)		_sginap(a)
#define l_fork()		_fork()

/* libc prototypes */
int _open(char *, int, int);
int _write(int,void *,int);
int _read(int,void *,int);
int exit(int);
int _dup(int);
int _close(int);
int _execve(char *, char **, char **);
int _sginap(int);
int _pipe(int *);
int _fork(void);
int _ioctl(int,int,void *);
int _select(int,void *, void *, void *, void *);

/* prom prototypes */
void _init_promgl(void);
int pon_morediags(void);
char *getversion(void);
void init_consenv(int);
void _init_htp(void);

/* gl-style prototypes */
void color(int);
void sboxfi(int,int,int,int);
void pnt2i(int,int);
void prefposition(int,int,int,int);
void prefsize(int,int);
void winopen(char *);
