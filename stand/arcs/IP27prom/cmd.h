typedef struct cmd_s {
    char       *name;
    int		token;
    char       *help;
} cmd_t;

extern cmd_t	cmds[];

extern cmd_t   *cmd_name2cmd(char *name);
extern int	cmd_name2token(char *name);
extern char    *cmd_name2help(char *name);
extern cmd_t   *cmd_token2cmd(int token);
