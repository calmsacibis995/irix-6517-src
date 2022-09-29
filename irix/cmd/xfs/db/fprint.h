#ident	"$Revision: 1.5 $"

typedef int (*prfnc_t)(void *obj, int bit, int count, char *fmtstr, int size,
		       int arg, int base, int array);

extern int	fp_charns(void *obj, int bit, int count, char *fmtstr, int size,
			  int arg, int base, int array);
extern int	fp_num(void *obj, int bit, int count, char *fmtstr, int size,
		       int arg, int base, int array);
extern int	fp_sarray(void *obj, int bit, int count, char *fmtstr, int size,
			  int arg, int base, int array);
extern int	fp_time(void *obj, int bit, int count, char *fmtstr, int size,
			int arg, int base, int array);
extern int	fp_uuid(void *obj, int bit, int count, char *fmtstr, int size,
			int arg, int base, int array);
