#ident "$Revision: 1.7 $"

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include "sim.h"
#include "command.h"
#include "data.h"
#include "input.h"
#include "output.h"
#include "sig.h"
#include "malloc.h"
#include "init.h"

int	inputstacksize;
FILE	**inputstack;
FILE	*curinput;

static void	popfile(void);
static int	source_f(int argc, char **argv);

static const cmdinfo_t	source_cmd =
	{ "source", NULL, source_f, 1, 1, 0, "source-file",
	  "get commands from source-file", NULL };

/* our homegrown strtok that understands strings */

static char *
tokenize(
	char        *inp)
{
	static char *last_place = NULL;
	char        *start;
	char        *walk;
	int         in_string = 0;
	int         in_escape = 0;

	if (inp) {
		start = inp;
	} else {
		if (last_place == NULL)
			return NULL;

		/* we're done */
		if (*last_place != '\0')
			return NULL;

		start = last_place + 1;
	}
	last_place = NULL;

	/* eat whitespace */
	while (*start == ' ' || *start == '\t')
		start++;

	walk = start;
	for (;*walk != '\0'; walk++) {
		if (in_escape) {
			in_escape = 0;
			continue;
		}
		if (*walk == '\\')
			in_escape = 1;
		else if (*walk == '\"')
			in_string ^= 1;

		if (!in_string && !in_escape &&
		    (*walk == ' ' || *walk == '\t')) {
			last_place = walk;
			*last_place = '\0';
			break;
		}
	}
	if (walk == start)
		return NULL;

	return start;
}

char **
breakline(
	char	*input,
	int	*count)
{
	int	c;
	char	*inp;
	char	*p;
	char	**rval;

	c = 0;
	inp = input;
	rval = xcalloc(sizeof(char *), 1);
	for (;;) {

		p = tokenize(inp);

		if (p == NULL)
			break;
		inp = NULL;
		c++;
		rval = xrealloc(rval, sizeof(*rval) * (c + 1));
		rval[c - 1] = p;
		rval[c] = NULL;
	}
	*count = c;
	return rval;
}

void
doneline(
	char	*input,
	char	**vec)
{
	xfree(input);
	xfree(vec);
}

char *
getline(void)
{
	char	buf[1024];
	int	iscont;
	size_t	len;
	size_t	rlen;
	char	*rval;

	rval = NULL;
	for (rlen = iscont = 0; ; ) {
		if (inputstacksize == 1) {
			if (iscont)
				dbprintf("... ");
			else
				dbprintf("%s: ", progname);
			fflush(stdin);
		}
		if (seenint() ||
		    (!fgets(buf, sizeof(buf), curinput) &&
		     ferror(curinput) && seenint())) {
			clearint();
			dbprintf("^C\n");
			clearerr(curinput);
			if (iscont) {
				iscont = 0;
				rlen = 0;
				if (rval) {
					xfree(rval);
					rval = NULL;
				}
			}
			continue;
		}
		if (ferror(curinput) || feof(curinput) ||
		    (len = strlen(buf)) == 0) {
			popfile();
			if (curinput == NULL) {
				dbprintf("\n");
				return NULL;
			}
			iscont = 0;
			rlen = 0;
			if (rval) {
				xfree(rval);
				rval = NULL;
			}
			continue;
		}
		if (inputstacksize == 1)
			logprintf("%s", buf);
		rval = xrealloc(rval, rlen + len + 1);
		if (rlen == 0)
			rval[0] = '\0';
		rlen += len;
		strcat(rval, buf);
		if (buf[len - 1] == '\n') {
			if (len > 1 && buf[len - 2] == '\\') {
				rval[rlen - 2] = ' ';
				rval[rlen - 1] = '\0';
				rlen--;
				iscont = 1;
			} else {
				rval[rlen - 1] = '\0';
				rlen--;
				break;
			}
		}
	}
	return rval;
}

void
input_init(void)
{
	add_command(&source_cmd);
}

static void
popfile(void)
{
	if (inputstacksize == 0) {
		curinput = NULL;
		return;
	}
	if (curinput != stdin)
		fclose(curinput);
	inputstack =
		xrealloc(inputstack,
			(inputstacksize - 1) * sizeof(*inputstack));
	inputstacksize--;
	curinput = inputstack[inputstacksize - 1];
}

void
pushfile(
	FILE	*file)
{
	inputstack =
		xrealloc(inputstack,
			(inputstacksize + 1) * sizeof(*inputstack));
	inputstacksize++;
	curinput = inputstack[inputstacksize - 1] = file;
}

/* ARGSUSED */
static int
source_f(
	int	argc,
	char	**argv)
{
	FILE	*f;

	f = fopen(argv[0], "r");
	if (f == NULL)
		dbprintf("can't open %s\n", argv[0]);
	else
		pushfile(f);
	return 0;
}
