#ident "$Revision: 1.4 $"

struct field;

extern const struct field	sb_flds[];
extern const struct field	sb_hfld[];

extern void	sb_init(void);
extern int	sb_size(void *obj, int startoff, int idx);
