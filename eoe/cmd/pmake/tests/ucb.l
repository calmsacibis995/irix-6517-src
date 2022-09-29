BW [ \t\n]
EW [ \t.,;!?]
%%
\n			printf("\n");fflush(stdout);
{BW}UCB{EW}		printf(" University of California, Berkeley");plastc();
%%
plastc() { putchar(yytext[yyleng-1]); }

yywrap() { return(1); }

main() { yylex(); }

