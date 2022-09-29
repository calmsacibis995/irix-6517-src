#ident "$Revision: 1.4 $"

struct field;

extern const struct field	agi_flds[];
extern const struct field	agi_hfld[];

extern void	agi_init(void);
extern int	agi_size(void *obj, int startoff, int idx);
