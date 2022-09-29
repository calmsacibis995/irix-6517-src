#ident "$Revision: 1.3 $"

extern void	add_strvec(char ***vecp, char *str);
extern char	**copy_strvec(char **vec);
extern void	free_strvec(char **vec);
extern char	**new_strvec(int count);
extern void	print_strvec(char **vec);
