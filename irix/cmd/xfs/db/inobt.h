#ident "$Revision: 1.4 $"

struct field;

extern const struct field	inobt_flds[];
extern const struct field	inobt_hfld[];
extern const struct field	inobt_key_flds[];
extern const struct field	inobt_rec_flds[];

extern int	inobt_size(void *obj, int startoff, int idx);
