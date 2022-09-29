#ident "$Revision: 1.2 $"

typedef struct dir_stack_elem  {
	xfs_ino_t		ino;
	struct dir_stack_elem	*next;
} dir_stack_elem_t;

typedef struct dir_stack  {
	int			cnt;
	dir_stack_elem_t	*head;
} dir_stack_t;


void		dir_stack_init(dir_stack_t *stack);

void		push_dir(dir_stack_t *stack, xfs_ino_t ino);
xfs_ino_t	pop_dir(dir_stack_t *stack);
