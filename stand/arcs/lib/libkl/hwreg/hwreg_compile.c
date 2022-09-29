#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <stdarg.h>

#include "hwreg.h"

#define TAB		"\t"

extern	int		errno;
extern	char	       *sys_errlist[];

#define STRERR		sys_errlist[errno]

char			opt_name[80];

char			opt_reg_file[256];
char			opt_output_file[80];

FILE		       *out_fp;
int			out_keep;

hwreg_t		       *reg_list;
int			reg_count;
int			reg_alloc;

hwreg_field_t	       *field_list;
int			field_count;
int			field_alloc;

/*
 * Utility routines
 */

void fatal(char *fmt, ...)
{
    va_list		ap;
    va_start(ap,fmt);
    fputs("hwreg_compile: ", stderr);
    vfprintf(stderr, fmt, ap);
    fflush(stderr);
    exit(1);
}

void perr(char *fmt, ...)
{
    va_list		ap;
    va_start(ap,fmt);
    fputs("hwreg_compile: ", stderr);
    vfprintf(stderr, fmt, ap);
    fflush(stderr);
}

void pmsg(char *fmt, ...)
{
    va_list		ap;
    va_start(ap,fmt);
    vfprintf(stderr, fmt, ap);
    fflush(stderr);
}

void pout(char *fmt, ...)
{
    va_list		ap;
    va_start(ap,fmt);
    vfprintf(out_fp, fmt, ap);
}

/*
 * String table routines
 */

#define STRTAB_MAX	10000

typedef struct stent_s {
    char	       *string;
    int			offset;
} stent_t;

stent_t			stab[STRTAB_MAX];
int			stab_count;
int			stab_offset;

int stab_insert(char *s)
{
    int			i;

    for (i = 0; i < stab_count; i++)
	if (strcmp(s, stab[i].string) == 0)
	    return stab[i].offset;

    stab[stab_count].string = strdup(s);
    stab[stab_count].offset = stab_offset;

    stab_count++;

    i = stab_offset;

    stab_offset += strlen(s) + 1;

    return i;
}

int stab_dump(char *name)
{
    int		i;

    pout("char hwreg_%s_stab[] =\n", name);

    for (i = 0; i < stab_count; i++)
	pout(TAB "\"%s\\000\"\n", stab[i].string);

    pout(";\n\n");
}

/*
 * Register definitions file parsing
 */

int parse_curline;

void parse_space(char **s)
{
    while (isspace(**s))
	(*s)++;
}

void parse_word(char **s, char *buf)
{
    parse_space(s);

    while (**s && ! isspace(**s))
	*buf++ = toupper(*(*s)++);

    *buf = 0;
}

int parse_line(FILE *fp, char *buf)
{
    char		*s;

    do {
	if (fgets(buf, 256, fp) == 0)
	    return 0;
	if ((s = strchr(buf, '\n')) != 0)
	    *s = 0;
	parse_curline++;
    } while (buf[0] == 0 || buf[0] == '#');

    return 1;
}

int parse_accmode(char *s)
{
    int			i;

    for (i = 0; hwreg_accmodes[i].name; i++)
	if (strcmp(s, hwreg_accmodes[i].name) == 0)
	    return i;

    perr("Bad access mode %s, assuming %s, file %s line %d\n",
	 s, hwreg_accmodes[0].name, opt_reg_file, parse_curline);

    return 0;
}

void parse_reg(FILE *fp, hwreg_t *r)
{
    char		buf[256], tmp1[128], tmp2[128];
    char	       *s;
    hwreg_field_t      *f;
    int			i;

    r->nfield 	= 0;
    r->sfield	= field_count;

    while (1) {
	if (parse_line(fp, buf) == 0)
	    fatal("EOF inside register definition\n");

	s = buf;

	parse_word(&s, tmp1);

	if (strcmp(tmp1, "END") == 0)
	    break;
	else if (strcmp(tmp1, "ADDRESS") == 0) {
	    parse_word(&s, tmp1);
	    r->address = hwreg_ctoi(tmp1, 0);
	} else if (strcmp(tmp1, "FIELD") == 0) {
	    if (field_count == field_alloc)
		field_list = realloc(field_list,
				     (field_alloc *= 2) *
				     sizeof (hwreg_field_t));
	    
	    f = &field_list[field_count++];

	    parse_word(&s, tmp1);
	    hwreg_getbits(tmp1, &f->topbit, &f->botbit);
	    parse_word(&s, tmp1);
	    f->accmode = parse_accmode(tmp1);
	    parse_word(&s, tmp1);
	    f->reset = (tmp1[0] != '-');
	    f->resetval = f->reset ? hwreg_ctoi(tmp1, 0) : 0;
	    parse_word(&s, tmp1);
	    f->nameoff = stab_insert(tmp1);
	    r->nfield++;
	} else if (strcmp(tmp1, "SEE") == 0) {
	    parse_word(&s, tmp1);

	    for (i = 0; i < reg_count - 1; i++)
		if (strcmp(tmp1, reg_list[i].name) == 0) {
		    r->sfield = reg_list[i].sfield;
		    r->nfield = reg_list[i].nfield;
		    break;
		}

	    if (i == reg_count - 1)
		fatal("Unknown register name, file %s line %d\n",
		      opt_reg_file, parse_curline);
	} else if (strcmp(tmp1, "NOTE") == 0) {
	    parse_space(&s);
	    r->noteoff = stab_insert(s);
	}
    }
}

void parse_defs(void)
{
    FILE		*fp;
    char		buf[256], tmp[256], *s;
    hwreg_t		*r;

    if ((fp = fopen(opt_reg_file, "r")) == 0)
	fatal("Unable to open register definitions file %s: %s",
	      opt_reg_file, STRERR);

    reg_alloc = 100;
    reg_count = 0;
    reg_list = malloc(reg_alloc * sizeof (hwreg_t));

    field_alloc = 100;
    field_count = 0;
    field_list = malloc(field_alloc * sizeof (hwreg_field_t));

    parse_curline = 0;

    while (parse_line(fp, buf)) {
	s = buf;

	parse_word(&s, tmp);

	if (strcmp(tmp, "REGISTER") == 0) {
	    if (reg_count == reg_alloc)
		reg_list = realloc(reg_list,
				   (reg_alloc *= 2) * sizeof (hwreg_t));

	    r = &reg_list[reg_count++];

	    parse_word(&s, tmp);

	    if (tmp[0] == 0)
		fatal("Null register name, file %s line %d\n",
		      opt_reg_file, parse_curline);

	    (void) stab_insert(tmp);
	    r->name = strdup(tmp);
	    r->noteoff = 0;

	    parse_reg(fp, r);
	} else
	    fatal("Unknown keyword, file %s line %d\n",
		  opt_reg_file, parse_curline);
    }

    fclose(fp);
}

void usage(void)
{
    pmsg("Usage: hwreg_compile NAME\n");
    pmsg("  Reads input file:     hwreg_NAME.hwreg\n");
    pmsg("  Creates output file:  hwreg_NAME.c\n");
    exit(1);
}

void exit_routine(void)
{
    if (out_fp) {
	fclose(out_fp);

	if (! out_keep)
	    unlink(opt_output_file);
    }
}

main(int argc, char **argv)
{
    int			i;

    if (argc != 2)
	usage();

    strcpy(opt_name, argv[1]);

    sprintf(opt_reg_file, "hwreg_%s.hwreg", opt_name);
    sprintf(opt_output_file, "hwreg_%s.c", opt_name);

    atexit(exit_routine);

    if ((out_fp = fopen(opt_output_file, "w")) == 0)
	fatal("Could not open output file: %s\n", STRERR);

    pout("#include <sys/types.h>\n");
    pout("#include <hwreg.h>\n");
    pout("\n");

    (void) stab_insert("x");	/* Avoid having a legal offset 0 */

    parse_defs();

    stab_dump(opt_name);

    pout("hwreg_field_t hwreg_%s_fields[] = {\n", opt_name);

    for (i = 0; i < field_count; i++) {
	hwreg_field_t  	       *f	= &field_list[i];

	pout(TAB "{ %d, %d, %d, %d, %d, 0x%llxLL },\n",
	     f->nameoff, f->topbit, f->botbit, f->accmode,
	     f->reset, f->resetval);
    }

    pout("};\n\n");

    pout("hwreg_t hwreg_%s_regs[] = {\n", opt_name);

    for (i = 0; i < reg_count; i++) {
	hwreg_t		*r;

	r = &reg_list[i];

	pout(TAB "{ %d, %d, 0x%llxLL, %d, %d },\n",
	     stab_insert(r->name), r->noteoff,
	     r->address, r->sfield, r->nfield);
    }

    pout(TAB "{ 0, 0, 0, 0 }\n");
    pout("};\n\n");

    pout("hwreg_set_t hwreg_%s = {\n"
	 TAB "hwreg_%s_regs,\n"
	 TAB "hwreg_%s_fields,\n"
	 TAB "hwreg_%s_stab,\n"
	 TAB "%d,\n"
	 "};\n",
	 opt_name, opt_name, opt_name, opt_name,
	 reg_count);

    fclose(out_fp);

    out_keep = 1;

    exit(0);
}
