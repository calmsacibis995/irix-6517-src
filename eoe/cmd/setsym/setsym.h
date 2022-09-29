/*
 * setsym.h
 *
 */

struct dbstbl64 {
	__uint64_t	addr;		/* kernel address */
	__uint32_t	noffst;		/* offset into name table */
};

struct dbstbl32 {
	__uint32_t	addr;		/* kernel address */
	__uint32_t	noffst;		/* offset into name table */
};

/*
 * These second arguments of xerror encode what needs to be closed.
 * The comment to the right is the ordering of the args, according to
 * the function invoked to do the opening.
 *
 */
#define NONE		0x0		/* "..." = <nothing>		*/
#define OP		0x1		/* "..." = open()		*/
#define OPELF		0x2		/* "..." = elf_begin(), open()	*/
#define OPDW		0x4		/* "..." = dwarf_init(), open()	*/
#define OPELFDW		0x8		/* "..." = dwarf_init(), elf_begin(), open() */

extern void	xerror(char *, int, ...);
