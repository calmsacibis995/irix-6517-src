#ident "$Revision: 1.3 $"

struct field;

extern void	write_init(void);
extern void	write_block(const field_t *fields, int argc, char **argv);
extern void	write_string(const field_t *fields, int argc, char **argv);
extern void	write_struct(const field_t *fields, int argc, char **argv);
