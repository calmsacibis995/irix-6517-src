/* libmain - flex run-time support library "main" function */

/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/flex/flex-2.5.4/RCS/libmain.c,v 1.1 1997/01/24 05:36:36 leedom Exp $ */

extern int yylex();

int main( argc, argv )
int argc;
char *argv[];
	{
	while ( yylex() != 0 )
		;

	return 0;
	}
