#ident "$Revision: 1.5 $"

void	io_init(void);

int	verify_sb(xfs_sb_t		*sb,
		int			is_primary_sb);
int	verify_set_primary_sb(xfs_sb_t	*root_sb,
			int		sb_index,
			int		*sb_modified);
int	get_sb(xfs_sb_t			*sbp,
		off64_t			off,
		int			size,
		xfs_agnumber_t		agno);
void	write_primary_sb(xfs_sb_t	*sbp,
			int		size);

int	find_secondary_sb(xfs_sb_t	*sb);

int	check_growfs(off64_t off, int bufnum, xfs_agnumber_t agnum);

void	get_sb_geometry(fs_geometry_t	*geo,
			xfs_sb_t	*sbp);

char	*alloc_ag_buf(int size);

void	print_inode_list(xfs_agnumber_t i);
char *	err_string(int err_code);

