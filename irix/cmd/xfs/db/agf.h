#ident "$Revision: 1.4 $"

struct field;

extern const struct field	agf_flds[];
extern const struct field	agf_hfld[];

extern void	agf_init(void);
extern int	agf_size(void *obj, int startoff, int idx);
