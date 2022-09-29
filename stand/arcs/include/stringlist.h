#ident	"include/stringlist.h:  $Revision: 1.9 $"

#ifndef STRINGLIST_H
#define STRINGLIST_H

/*
 * stringlist.h -- definitions for stringlist routines
 */

#define	MAXSTRINGS	64		/* max number of strings */
#define	STRINGBYTES	MAXSTRINGS*20	/* max total length of strings */

/*
 * string lists are used to maintain argv and environment string lists
 */
struct string_list {
	char *strptrs[MAXSTRINGS];	/* vector of string pointers */
	char strbuf[STRINGBYTES];	/* strings themselves */
	char *strp;			/* free ptr in strbuf */
	int strcnt;			/* number of strings in strptrs */
};

extern void	init_str(struct string_list *);
extern void	replace_str(char *, char *, struct string_list *);
extern void	delete_str(char *, struct string_list *);
extern char *	find_str(char *, struct string_list *);
extern int	new_str1(char *, struct string_list *);
extern int	new_str2(char *, char *, struct string_list *);
extern int	set_str(char *, int, struct string_list *);
extern int	delete_strnum(int, struct string_list *);
extern char **	_copystrings(char **, struct string_list *);
#endif
