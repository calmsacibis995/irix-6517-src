/*
 * Unifdef symbol table implementation.
 */
#include <string.h>
#include "unifdef.h"

#define	HASHSIZE	23

struct symbol	*symhash[HASHSIZE];

#ifdef METERING
static struct hashmeter {
	unsigned int	hits;		/* lookups which hit */
	unsigned int	misses;		/* lookups which missed */
	unsigned int	steps;		/* hash chains traversed by lookup */
	unsigned int	maxchain;	/* maximum hash chain length */
	unsigned int	installs;	/* symbols installed explicitly */
	unsigned int	removes;	/* final dereference which removes */
	unsigned int	drops;		/* nonterminal dereferences */
} hashmeter;
#endif

static struct symbol **
hash(char *cp)
{
	unsigned int h;

	for (h = 0; *cp != '\0'; cp++)
		h = (h << 1) ^ *cp;
	return &symhash[h % HASHSIZE];
}

struct symbol *
lookup(char *name)
{
	struct symbol **head;
	struct symbol *sym;
#ifdef METERING
	int chainlen = 0;
#endif

	head = hash(name);
	for (sym = *head; sym; sym = sym->next) {
		METER(chainlen++);
		if (!strcmp(sym->name, name)) {
			METER(hashmeter.hits++);
			sym->refcnt++;
			return sym;
		}
		METER(hashmeter.steps++);
	}
	METER(hashmeter.misses++);
#ifdef METERING
	if (++chainlen > hashmeter.maxchain)
		hashmeter.maxchain = chainlen;
#endif
	sym = new(struct symbol);
	sym->name = strdup(name);
	sym->type = SDONTCARE;
	sym->value = 0;
	sym->defn = 0;
	sym->refcnt = 1;
	sym->next = *head;
	*head = sym;
	return sym;
}

void
install(char *name, enum symtype type, long value, char *defn)
{
	struct symbol *sym;

	METER(hashmeter.installs++);
	sym = lookup(name);
	sym->type = type;
	sym->value = value;
	sym->defn = defn;
}

void
dropsym(struct symbol *sym)
{
	struct symbol **sp;

	for (sp = hash(sym->name); *sp; sp = &(*sp)->next) {
		if (*sp != sym)
			continue;
		if (--sym->refcnt == 0) {
			METER(hashmeter.removes++);
			*sp = sym->next;
			delete(sym->name);
			delete(sym);
			return;
		}
		METER(hashmeter.drops++);
	}
}

#ifdef METERING
#include <stdio.h>

static char *typename[] = {
	"reserved",
	"dontcare",
	"defined",
	"undefined"
};

dumphashmeter()
{
	int maxchain, chainlen;
	struct symbol **head, *sym, *maxhead;

	fprintf(stderr, "Hash table statistics:\n");
	fprintf(stderr, "hits      %u\n", hashmeter.hits);
	fprintf(stderr, "misses    %u\n", hashmeter.misses);
	fprintf(stderr, "steps     %u\n", hashmeter.steps);
	fprintf(stderr, "maxchain  %u\n", hashmeter.maxchain);
	fprintf(stderr, "lookups   %u\n", hashmeter.hits + hashmeter.misses);
	fprintf(stderr, "installs  %u\n", hashmeter.installs);
	fprintf(stderr, "removes   %u\n", hashmeter.removes);
	fprintf(stderr, "drops     %u\n", hashmeter.drops);
	fprintf(stderr, "Symbols still referenced:\n");
	fprintf(stderr, "refcnt    value type      name\n");
	maxchain = 0;
	for (head = &symhash[0]; head < &symhash[HASHSIZE]; head++) {
		chainlen = 0;
		for (sym = *head; sym; sym = sym->next) {
			fprintf(stderr, "%6u %8lu %-9.9s %s\n",
				sym->refcnt, sym->value,
				typename[(int)sym->type], sym->name);
			chainlen++;
		}
		if (chainlen > maxchain) {
			maxchain = chainlen;
			maxhead = *head;
		}
	}
	fprintf(stderr, "Longest hash chain:\n");
	for (sym = maxhead; sym; sym = sym->next) {
		fprintf(stderr, "%6u %8lu %-9.9s %s\n",
			sym->refcnt, sym->value,
			typename[(int)sym->type], sym->name);
	}
}
#endif
