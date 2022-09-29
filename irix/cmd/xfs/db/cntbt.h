#ident "$Revision: 1.4 $"

struct field;

extern const struct field	cntbt_flds[];
extern const struct field	cntbt_hfld[];
extern const struct field	cntbt_key_flds[];
extern const struct field	cntbt_rec_flds[];

extern int	cntbt_size(void *obj, int startoff, int idx);
