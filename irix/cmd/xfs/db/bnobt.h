#ident "$Revision: 1.4 $"

struct field;

extern const struct field	bnobt_flds[];
extern const struct field	bnobt_hfld[];
extern const struct field	bnobt_key_flds[];
extern const struct field	bnobt_rec_flds[];

extern int	bnobt_size(void *obj, int startoff, int idx);
