#ident "$Revision: 1.5 $"

typedef int (*cfunc_t)(int argc, char **argv);
typedef void (*helpfunc_t)(void);

typedef struct cmdinfo
{
	const char	*name;
	const char	*altname;
	cfunc_t		cfunc;
	int		argmin;
	int		argmax;
	int		canpush;
	const char	*args;
	const char	*oneline;
	helpfunc_t      help;
} cmdinfo_t;

extern cmdinfo_t	*cmdtab;
extern int		ncmds;

extern void		add_command(const cmdinfo_t *ci);
extern int		command(int argc, char **argv);
extern const cmdinfo_t	*find_command(const char *cmd);
extern void		init_commands(void);
