#include <sys/types.h>
#include <sys/systm.h>
#include <libsc.h>
#include <ctype.h>

#include "dbgmon.h"

#define ASTKSIZE	16
#define OSTKSIZE	6

#define VALUE		__psunsigned_t

static	VALUE		astk[ASTKSIZE], *asp;
static	char		ostk[OSTKSIZE], *osp;
static	int		err;

static int pri(int op)
{
    static char	       *pristr	= "(   ~U  */% +-  <>  &   ^   |   )   ";
    int			i;

    for (i = 0; pristr[i]; i++)
	if (pristr[i] == op)
	    return i / 4;

    err = -1;
    return 0;
}

static void push(VALUE v)
{
    if (asp == &astk[ASTKSIZE])
	err = -1;
    else
	*asp++ = v;
}

static VALUE pop(void)
{
    if (asp == &astk[0]) {
	err = -1;
	return 0;
    } else {
	--asp;
	return *asp;
    }
}

static void doop(int op)
{
    VALUE v;

    if (op == '(' || op == ')')
	return;

    v = pop();

    switch (op) {
    case '~':		v = ~v;			break;
    case 'U':		v = 0 - v;		break;
    case '*':		v = pop() * v;		break;
    case '/':		v = pop() / v;		break;
    case '%':		v = pop() % v;		break;
    case '+':		v = pop() + v;		break;
    case '-':		v = pop() - v;		break;
    case '<':		v = pop() << v;		break;
    case '>':		v = pop() >> v;		break;
    case '&':		v = pop() & v;		break;
    case '^':		v = pop() ^ v;		break;
    case '|':		v = pop() | v;		break;
    }

    push(v);
}

static VALUE symtoull(char **sp)
{
    char		buf[60], *d = buf;
    numinp_t		addr;

    while ((isalnum(**sp) || **sp == '_') && d < &buf[sizeof(buf) - 1])
	*d++ = *(*sp)++;

    *d = 0;

    if (parse_sym(buf, &addr)) {	/* Prints message on error */
	addr = 0;
	err = -1;
    }

    return (VALUE) addr;
}

/*ARGSUSED*/
int _calc(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
    int			i, p, prev_was_op = 1;
    char	       *s;
    VALUE		v;

    asp = &astk[0];
    osp = &ostk[0];
    err = 0;

    for (argv++, argc--; argc; argv++, argc--) {
	s = *argv;
	while ((i = *s) != 0) {
	    if (isspace(i))
		s++;
	    else if (isalnum(i) || i == '_') {
		push(symtoull(&s));
		prev_was_op = 0;
	    } else {
		s++;
		if (i == '<' || i == '>') {
		    if (*s != i)
			i = 'x';
		    else
			s++;
		}
		p = pri(i);
		if (i == '-' && prev_was_op)
		    i = 'U';
		else
		    while (osp > ostk && pri(osp[-1]) <= p)
			doop(*--osp);
		*osp++ = i;
		prev_was_op = (i != ')');
	    }
	}
    }

    while (osp > ostk)
	doop(*--osp);

    v = pop();

    if (asp != astk)
	err = -1;

    if (err)
	printf("px: Invalid expression\n");
    else
	printf("  0x%llx  %lld  %llu  0%llo\n", v, v, v, v);

    return 0;
}
