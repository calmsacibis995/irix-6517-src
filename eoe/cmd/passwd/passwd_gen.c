/*
 * Generate (hopefully) pronounceable random passwords.  These can often be
 * remembered more easily than completely random passwords, and are immune to
 * dictionary searches, etc.
 *
 * The original version of this program was written in BASIC on an OSI
 * Superboard II SBC.  That version is long gone (the SB2's cassette drive
 * was never trustworthy, it eventually scrambled the tape), but the basic
 * (pardon the pun) idea lives on here, with a few modification like basing
 * the selection on "graphs" (actually, they are a restricted set of phonemes)
 * and having randomly-selected spellings for those graphs.
 */

#include <stdio.h>
#include <math.h>

#define RANDOM(c)	((int) (random() % (c)))

char *spelling[] = {
/*a*/	"a",				(char *) 0,	/* 2*/
/*A*/	"a",	"ae",	"ai",		(char *) 0,	/* 6*/
/*b*/	"b",				(char *) 0,	/* 8*/
/*ch*/	"ch",				(char *) 0,	/*10*/
/*d*/	"d",				(char *) 0,	/*12*/
/*e*/	"e",				(char *) 0,	/*14*/
/*E*/	"e",	"ee",	"ie",		(char *) 0,	/*18*/
/*f*/	"f",	"ph",	"gh",		(char *) 0,	/*22*/
/*g*/	"g",				(char *) 0,	/*24*/
/*h*/	"h",				(char *) 0,	/*26*/
/*i*/	"i",	"e",			(char *) 0,	/*29*/
/*I*/	"i",	"ai",			(char *) 0,	/*32*/
/*i'*/	"i",	"ei",			(char *) 0,	/*35*/
/*j*/	"j",	"g",			(char *) 0,	/*38*/
/*k*/	"k",	"c",			(char *) 0,	/*41*/
/*l*/	"l",				(char *) 0,	/*43*/
/*m*/	"m",				(char *) 0,	/*45*/
/*n*/	"n",				(char *) 0,	/*47*/
/*ng*/	"ng",				(char *) 0,	/*49*/
/*o*/	"o",	"a",	"ah",		(char *) 0,	/*53*/
/*O*/	"o",	"oh",			(char *) 0,	/*56*/
/*oo*/	"oo",	"u",			(char *) 0,	/*59*/
/*OO*/	"oo",	"w",			(char *) 0,	/*62*/
/*p*/	"p",				(char *) 0,	/*64*/
/*qu*/	"qu",				(char *) 0,	/*66*/
/*r*/	"r",				(char *) 0,	/*68*/
/*s*/	"s",	"c",			(char *) 0,	/*71*/
/*sh*/	"sh",	"s",			(char *) 0,	/*74*/
/*t*/	"t",				(char *) 0,	/*76*/
/*th*/	"th",				(char *) 0,	/*78*/
/*TH*/	"th",				(char *) 0,	/*80*/
/*u*/	"u",				(char *) 0,	/*82*/
/*U*/	"u",	"oo",			(char *) 0,	/*85*/
/*v*/	"v",				(char *) 0,	/*87*/
/*x*/	"x",				(char *) 0,	/*89*/
/*y*/	"y",				(char *) 0,	/*91*/
/*z*/	"z",	"s",			(char *) 0,	/*94*/
};

struct graph {
	char *graph;
	char type;
#define CONSONANT	0
#define VOWEL_LONG	1
#define VOWEL_SHORT	2
#define VOWEL_OTHER	3
#define VOWEL_MASK	3
#define iscons(c)	(((c)->type & VOWEL_MASK) == 0)
#define isvowel(c)	(((c)->type & VOWEL_MASK) != 0)
/*	char frequency;			*/	/* unused for now */
	char **spellings;
/*	struct graph **following;	*/	/* maybe later */
} graph[] = {
	"a",	VOWEL_SHORT,	&spelling[0],
	"A",	VOWEL_LONG,	&spelling[2],
	"b",	CONSONANT,	&spelling[6],
	"ch",	CONSONANT,	&spelling[8],
	"d",	CONSONANT,	&spelling[10],
	"e",	VOWEL_SHORT,	&spelling[12],
	"E",	VOWEL_LONG,	&spelling[14],
	"f",	CONSONANT,	&spelling[18],
	"g",	CONSONANT,	&spelling[22],
	"h",	CONSONANT,	&spelling[24],
	"i",	VOWEL_SHORT,	&spelling[26],
	"I",	VOWEL_LONG,	&spelling[29],
	"i'",	VOWEL_OTHER,	&spelling[32],
	"j",	CONSONANT,	&spelling[35],
	"k",	CONSONANT,	&spelling[38],
	"l",	CONSONANT,	&spelling[41],
	"m",	CONSONANT,	&spelling[43],
	"n",	CONSONANT,	&spelling[45],
	"ng",	CONSONANT,	&spelling[47],
	"o",	VOWEL_SHORT,	&spelling[49],
	"O",	VOWEL_LONG,	&spelling[53],
	"oo",	VOWEL_SHORT,	&spelling[56],
	"OO",	VOWEL_LONG,	&spelling[59],
	"p",	CONSONANT,	&spelling[62],
	"qu",	CONSONANT,	&spelling[64],
	"r",	CONSONANT,	&spelling[66],
	"s",	CONSONANT,	&spelling[68],
	"sh",	CONSONANT,	&spelling[71],
	"t",	CONSONANT,	&spelling[74],
	"th",	CONSONANT,	&spelling[76],
	"TH",	CONSONANT,	&spelling[78],
	"u",	VOWEL_SHORT,	&spelling[80],
	"U",	VOWEL_LONG,	&spelling[82],
	"v",	CONSONANT,	&spelling[85],
	"x",	CONSONANT,	&spelling[87],
	"y",	CONSONANT,	&spelling[89],
	"z",	CONSONANT,	&spelling[91],
	0,	0,		&spelling[94],
};

struct graph *vowel[] = {
	&graph[0],	&graph[1],	&graph[5],	&graph[6],
	&graph[10],	&graph[11],	&graph[12],	&graph[19],
	&graph[20],	&graph[21],	&graph[22],	&graph[31],
	&graph[32],
	(struct graph *) 0,
};

struct graph *consonant[] = {
	&graph[2],	&graph[3],	&graph[4],	&graph[7],
	&graph[8],	&graph[9],	&graph[13],	&graph[14],
	&graph[15],	&graph[16],	&graph[17],	&graph[18],
	&graph[23],	&graph[24],	&graph[25],	&graph[26],
	&graph[27],	&graph[28],	&graph[29],	&graph[30],
	&graph[33],	&graph[34],	&graph[35],	&graph[36],
	(struct graph *) 0,
};

/*
 * Randomly select a graph from the specifield array.  Eventually, this should
 * account for graph frequencies as well.
 */

struct graph *selgraph(graphs)
	struct graph **graphs;
{
	register int cnt;

	for (cnt = 0; graphs[cnt] != (struct graph *) 0; cnt++)
		;
	return graphs[RANDOM(cnt)];
}

/*
 * Randomly select a spelling for the specified graph.  This is not linear:
 * earlier spellings are preferred over later ones, but the latter do
 * sometimes sneak in.
 */

char *selspell(graph)
	struct graph *graph;
{
	register int cnt, sel;

	for (cnt = 0; graph->spellings[cnt] != (char *) 0; cnt++)
		;
	if (cnt == 0) {
		fprintf(stderr, "PANIC: selspell(%s) got count(spellings) == 0\n", graph->graph);
		exit(2);
	}
	if (cnt == 1)
		return *graph->spellings;
/*
 * This may not be the best way to do it... maybe Weemba'd care to lend a
 * hand here?  After all, my specialty is programming, NOT math.
 */
	if ((sel = cnt - (int) sqrt((double) RANDOM(cnt * cnt) + 1) - 1) < 0 || sel >= cnt) {
#ifdef BUGCATCH
		fprintf(stderr, "PANIC: selspell(%s) got nlrand(%d) == %d\n", graph->graph, cnt, sel);
		exit(2);
#else
		sel = 0;
#endif
	}
	return graph->spellings[sel];
}

/*
 * Choose the next source for a graph.  The rules are:  a consonant MUST be
 * followed by a vowel; a vowel may be followed by a vowel of a different
 * type or by a consonant, but never more than two consecutive vowel graphs.
 */

choosenext(cur, prev)
{
	if (cur == CONSONANT)
		return VOWEL_MASK;
	else if (prev == -1 || (prev & VOWEL_MASK) != 0)
		return CONSONANT;
	else if (RANDOM(10) == 5)
		return VOWEL_MASK;
	else
		return CONSONANT;
}

/*
 * We are passed an array of (struct graph *); choose an entry randomly and
 * assemble a string fitting the size constraint.  We use the original (OSI)
 * paradigm:  alternate consonants and vowels, with the option of two vowels
 * in a row occasionally.  The only difference is that they must be different
 * *types* of vowels, a distinction that the OSI version didn't consider.
 */

static void
pwgen(initial, pw, maxlen)
	struct graph **initial;
	char *pw;
{
	int pwlen, state, prev, tmp;
	struct graph *graph;
	char *spelling;

	pwlen = 0;
	state = initial[0]->type;
	prev = -1;
	while (pwlen < maxlen) {
		do {
			graph = selgraph(initial);
		} while (state != CONSONANT && graph->type == prev);
		if ((spelling = selspell(graph)) == (char *) 0) {
			fprintf(stderr, "PANIC: got NULL in selspell(%s)\n", graph->graph);
			exit(2);
		}
		strcpy(pw, spelling);
		if (pwlen + strlen(pw) > maxlen) {
			*pw = '\0';
			break;
		}
		while (*pw != '\0')
			pwlen++, pw++;
		tmp = prev;
		prev = graph->type;
		if ((state = choosenext(prev, tmp)) == CONSONANT)
			initial = consonant;
		else
			initial = vowel;
	}
}

main(argc, argv)
	char **argv;
{
	int cnt, len;
	char buf[20];

	if (argc == 1) {
		len = 8;
		cnt = 5;
	} else {
		if (argc < 2 || argc > 3) {
			fprintf(stderr, "usage: %s length [count]\n", argv[0]);
			exit(1);
		}
		if ((len = atoi(argv[1])) < 4 || len > 16) {
			fprintf(stderr, "%s: invalid length %s\n",
			    argv[0], argv[1]);
			exit(1);
		}
		if (argc == 2)
			cnt = 1;
		else if ((cnt = atoi(argv[2])) < 1) {
			fprintf(stderr, "%s: invalid count %s\n",
			    argv[0], argv[2]);
			exit(1);
		}
	}

	srandom(time(0) + (getpgrp() << 8) + getpid());
	while (cnt-- != 0) {
		pwgen((RANDOM(10) < 4? vowel: consonant), buf, len);
		printf("%s\n", buf);
	}
	/*putchar('\n');*/
	exit(0);
}
