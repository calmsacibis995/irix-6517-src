#ident "$Revision: 1.4 $"

struct field;

extern const struct field	agfl_flds[];
extern const struct field	agfl_hfld[];

extern void	agfl_init(void);
extern int	agfl_size(void *obj, int startoff, int idx);
