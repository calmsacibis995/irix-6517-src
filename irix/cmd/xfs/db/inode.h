#ident "$Revision: 1.10 $"

#if VERS >= V_62
extern const struct field	inode_a_flds[];
#endif
extern const struct field	inode_core_flds[];
extern const struct field	inode_flds[];
extern const struct field	inode_hfld[];
extern const struct field	inode_u_flds[];
extern const struct field	timestamp_flds[];

extern int	fp_dinode_fmt(void *obj, int bit, int count, char *fmtstr,
			      int size, int arg, int base, int array);
#if VERS >= V_62
extern int	inode_a_size(void *obj, int startoff, int idx);
#endif
extern void	inode_init(void);
extern typnm_t	inode_next_type(void);
extern int	inode_size(void *obj, int startoff, int idx);
extern int	inode_u_size(void *obj, int startoff, int idx);
extern void	set_cur_inode(xfs_ino_t ino);
