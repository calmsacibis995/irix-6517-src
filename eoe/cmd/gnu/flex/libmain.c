/* libmain - flex run-time support library "main" function */

/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/gnu/flex/RCS/libmain.c,v 1.1 1992/05/05 22:30:31 wicinski Exp $ */

extern int yylex();

int main( argc, argv )
int argc;
char *argv[];

    {
    return yylex();
    }
