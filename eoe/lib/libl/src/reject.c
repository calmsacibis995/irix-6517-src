/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.5 $"

# include <stdio.h>
extern FILE 	*yyout, *yyin;
extern int	yyprevious , *yyfnd;
extern char	yyextra[];
extern char	yytext[];
extern int	yyleng;
extern struct	{int *yyaa, *yybb; int *yystops;} *yylstate [], **yylsp, **yyolsp;
extern char	yyinput(void);
extern void	yyunput(int);
extern int	yyback(int *, int);
int	 	yyracc(int);

int
yyreject(void)
{
	for( ; yylsp < yyolsp; yylsp++)
		yytext[yyleng++] = yyinput();
	if (*yyfnd > 0)
		return(yyracc(*yyfnd++));
	while (yylsp-- > yylstate) {
		yyunput(yytext[yyleng-1]);
		yytext[--yyleng] = 0;
		if (*yylsp != 0 && (yyfnd= (*yylsp)->yystops) && *yyfnd > 0)
			return(yyracc(*yyfnd++));
	}
	if (yytext[0] == 0)
		return(0);
	yyleng=0;
	return(-1);
}

int
yyracc(int m)
{
	yyolsp = yylsp;
	if (yyextra[m]) {
		while (yyback((*yylsp)->yystops, -m) != 1 && yylsp>yylstate) {
			yylsp--;
			yyunput(yytext[--yyleng]);
		}
	}
	yyprevious = yytext[yyleng-1];
	yytext[yyleng] = 0;
	return(m);
}
