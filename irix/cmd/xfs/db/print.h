#ident "$Revision: 1.7 $"

struct field;
struct flist;

extern void	print_flist(struct flist *flist);
extern void	print_init(void);
extern void	print_sarray(void *obj, int bit, int count, int size, int base,
			     int array, const field_t *flds, int skipnms);
extern void	print_struct(const struct field *fields, int argc, char **argv);
extern void	print_string(const struct field *fields, int argc, char **argv);
