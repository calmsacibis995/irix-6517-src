%{
/*
 * ide_gram - ide yacc grammar
 *
 */

#include "libsc.h"
#include "libsk.h"
#include "ide.h"

#ident "$Revision: 1.19 $"

node_t	*makeenode();
void yyerror(const char *s);
int psignal(int);
static node_t *makebop(node_t *, node_t *, int);
static void checkcase(node_t *, int, int);
static node_t *makeassop(sym_t *, node_t *, int);

int badflag   = 0;

int inloop   = 0;
int inblock  = 0;
int inswitch = 0;
int inrawcmd = 0;
int inif     = 0;

/* we don't want no stinkin' POSIX I/O in this standalone code */
#define __UNISTD_H__
#define __MALLOC_H__

%}

%union	{
	int	intval;
	char	*strval;
	sym_t	*psym;
	node_t	*pnode;
	}

%token KWFOR KWWHILE KWIF KWELSE KWPRINT KWRETURN KWBREAK KWCONTINUE KWFI KWDO
%token KWCASE KWDEFAULT KWSWITCH KWREPEAT KWFOREVER
%token LO LA LE GE EQ NE DECR INCR SHL SHR
%token ADDASS SUBASS MULASS DIVASS MODASS SHLASS SHRASS ANDASS ORASS XORASS
%token <intval> INTCONST
%token <strval> STRCONST IDENT
%token <psym>	VAR CMD
%type <pnode>	expr simple stmt stmtblock arg arglist epart ifte fexpr for
%type <pnode>	while return continue break do switch switchlist switchcase
%type <pnode>	casetype casebody cmd repeat carglist arglist2
%type <psym>	var dollarvar range cmdbeg

%right '=' ADDASS SUBASS MULASS DIVASS MODASS SHLASS SHRASS ANDASS ORASS XORASS
%left LO
%left LA
%left '^'
%left '|'
%left '&'
%left EQ NE
%left '<' '>' GE LE
%left SHL SHR
%left '+'
%left '-'
%left '*'
%left '/'
%left '%'
%right '!' '~'
%left '('

%%
session: /* empty */		{ inif=inblock=inswitch=inloop=badflag=0;}
	| session stmt  	{ if ( ! badflag && $2)
					dostmt( $2 );
				  else
					badflag = 0;
				  destroytree($2);
				  inif=inblock=inswitch=inloop=badflag=0;}
	| session error		{ inif=inblock=inswitch=inloop=badflag=0;}
	| session fdef
	| session '?'		{ doqhelp();				}
	| session '\n'
	;
stmt: simple
	| stmtblock '}' 	{ inblock--;				}
	;
stmtblock: '{' { inblock++; } onls stmt
				{ $$ = $4;				}
	| stmtblock stmt	{ if ( $1 && ! $2 )
					$$ = $1;
				  else
				  if ( ! $1 && $2 )
					$$ = $2;
				  else	{
					$$ = makenode();
					$$->node_right = $1;
					$$->node_left  = $2;
					$$->node_type  = OP_STMT;
					}
									}
	| stmtblock '\n'
	;
fdef: IDENT stmtblock '}'	{ sym_t *psym = makesym();
				  inblock--;
				  psym->sym_type = SYM_CMD;
				  psym->sym_stmt = $2;
				  psym->sym_name = $1;
				  psym->sym_flags |= SYM_UDEF;
				  addsym(psym);				}
	;
onl:	/* empty */		/* optional newline */
	| '\n'
	;
onls:	onl
	| onls '\n';
	;
st:	';' 			/* statement terminator */
	| '\n'
	;
simple: ';' 	 		{ $$ = 0;				}
	| expr st
	| ifte
	| while
	| do st
	| switch
	| for
	| repeat
	| return st
	| continue st
	| break st
	| KWPRINT expr st	{ $$ = makenode();
				  $$->node_right = $2;
				  $$->node_type = OP_PRINT;		}
	| KWPRINT STRCONST expr st
				{ $$ = makenode();
				  $$->node_right = $3;
				  $$->node_psym = makessym($2);
				  $$->node_type = OP_PRINT;		}
	;
return: KWRETURN expr		{ $$ = makenode();
				  $$->node_type = OP_RETURN;
				  $$->node_right = $2;			}
	| KWRETURN		{ $$ = makenode();
				  $$->node_type = OP_RETURN;		}
	;
continue: KWCONTINUE		{ $$ = makenode();
				  $$->node_type = OP_CONTINUE;
				  if ( ! inloop )
					yyerror("continue outside of loop");
									}
	;
break: KWBREAK 			{ $$ = makenode();
				  $$->node_type = OP_BREAK;
				  if ( ! inloop && ! inswitch )
				    yyerror("break outside of loop or switch");
									}
	;
while: KWWHILE '(' expr ')' { inloop++; } onl stmt
				{ inloop--;
				  $$ = makenode();
				  $$->node_left = $3;
				  $$->node_right = $7;
				  $$->node_type = OP_WHILE;		}
	;
do: KWDO { inloop++; } onl stmt onl KWWHILE '(' expr ')'
				{ inloop--;
				  $$ = makenode();
				  $$->node_left = $8;
				  $$->node_right = $4;
				  $$->node_type = OP_DO;		}
	;
repeat: KWREPEAT INTCONST stmt	{ $$ = makenode();
				  $$->node_psym  = makeisym($2);
				  $$->node_right = $3;
				  $$->node_type  = OP_REPEAT;		}
	| KWREPEAT stmt		{ $$ = makenode();
				  $$->node_psym  = makeisym(0);
				  $$->node_right = $2;
				  $$->node_type  = OP_REPEAT;		}
	| KWFOREVER stmt	{ $$ = makenode();
				  $$->node_psym  = makeisym(0);
				  $$->node_right = $2;
				  $$->node_type  = OP_REPEAT;		}
	;
switch: KWSWITCH '(' expr ')' onl switchlist '}'
				{ inswitch--;
				  $$ = makenode();
				  $$->node_type = OP_SWITCH;
				  $$->node_left = $3;
				  $$->node_right= $6;			}
	;
switchlist: { inswitch++; } '{' onl switchcase
				{ $$ = $4;				}
	| switchlist switchcase
				{ $$ = makenode();
				  $$->node_type = OP_CASELIST;
				  $$->node_right= $1;
				  $$->node_left = $2;			}
	;
switchcase: casetype ':'	{ $$ = $1;				} 
	| casetype ':' casebody	{ $1->node_right = $3; $$ = $1;		} 
	;
casetype: KWCASE INTCONST	{ $$ = makenode();
				  $$->node_type = OP_CASE;
				  $$->node_psym = makeisym($2);		}
	| KWDEFAULT		{ $$ = makenode();
				  $$->node_type = OP_CASE;		}
	;
casebody: simple 		{ $$ = $1;				}
	| casebody simple
				{ $$ = makenode();
				  $$->node_right = $1;
				  $$->node_left  = $2;
				  $$->node_type  = OP_STMT;		}
	;
ifte: KWIF '(' expr ')' { inif++; } onl stmt onl epart eif
				{ inif--;
				  $$ = makenode();
				  $$->node_type = OP_IF;
				  $$->node_left = $3;
				  if ( $9 ) {
					$$->node_right = makenode();
					$$->node_right->node_left = $7;
					$$->node_right->node_type = OP_ELSE;
					$$->node_right->node_right =$9;
					}
				  else
					$$->node_right = $7;		}
	;
eif: /* empty */
	| KWFI
	;
epart: /* empty */		{ $$ = 0;				}
	| KWELSE onl stmt onl	{ $$ = $3;				}
	;
for: KWFOR '(' fexpr ';' fexpr ';' fexpr ')' { inloop++; } onl stmt
				{ inloop--;
				  $$ = makenode();
				  $$->node_type = OP_FOR;
				  $$->node_left = makenode();
				  $$->node_right= makenode();
				  $$->node_left->node_left = $3;
				  $$->node_left->node_right = $5;
				  $$->node_right->node_left = $7;
				  $$->node_right->node_right = $11;	}
	;
fexpr: /* empty */		{ $$ = 0;				}
	| expr
	;
expr: '(' expr ')'		{ $$ = $2;				}
	| var '='    expr	{ $$ = makeassop($1, $3, OP_ASSIGN);	}
	| var MULASS expr	{ $$ = makeassop($1, $3, OP_MULASS);	}
	| var SUBASS expr	{ $$ = makeassop($1, $3, OP_SUBASS);	}
	| var DIVASS expr	{ $$ = makeassop($1, $3, OP_DIVASS);	}
	| var ADDASS expr	{ $$ = makeassop($1, $3, OP_ADDASS);	}
	| var MODASS expr	{ $$ = makeassop($1, $3, OP_MODASS);	}
	| var SHLASS expr	{ $$ = makeassop($1, $3, OP_SHLASS);	}
	| var SHRASS expr	{ $$ = makeassop($1, $3, OP_SHRASS);	}
	| var ANDASS expr	{ $$ = makeassop($1, $3, OP_ANDASS);	}
	| var XORASS expr	{ $$ = makeassop($1, $3, OP_XORASS);	}
	| var ORASS  expr	{ $$ = makeassop($1, $3, OP_ORASS);	}
	| expr '+' expr		{ $$ = makebop($1, $3, OP_ADD );	}
	| expr '-' expr		{ $$ = makebop($1, $3, OP_SUB );	}
	| expr '*' expr		{ $$ = makebop($1, $3, OP_MUL );	}
	| expr '/' expr		{ $$ = makebop($1, $3, OP_DIV );	}
	| expr '%' expr		{ $$ = makebop($1, $3, OP_MOD );	}
	| expr '<' expr		{ $$ = makebop($1, $3, OP_LT );		}
	| expr LE  expr		{ $$ = makebop($1, $3, OP_LE );		}
	| expr '>' expr		{ $$ = makebop($1, $3, OP_GT );		}
	| expr GE  expr		{ $$ = makebop($1, $3, OP_GE );		}
	| expr EQ  expr		{ $$ = makebop($1, $3, OP_EQ );		}
	| expr NE  expr		{ $$ = makebop($1, $3, OP_NE );		}
	| expr LO  expr		{ $$ = makebop($1, $3, OP_OROR );	}
	| expr LA  expr		{ $$ = makebop($1, $3, OP_ANDAND );	}
	| expr '|' expr		{ $$ = makebop($1, $3, OP_OR );		}
	| expr '&' expr		{ $$ = makebop($1, $3, OP_AND );	}
	| expr '^' expr		{ $$ = makebop($1, $3, OP_XOR );	}
	| expr SHL expr		{ $$ = makebop($1, $3, OP_SHL );	}
	| expr SHR expr		{ $$ = makebop($1, $3, OP_SHR );	}
	| '~' expr		{ $$ = makenode();
				  $$->node_right = $2;
				  $$->node_type = OP_COM;		}
	| '!' expr		{ $$ = makenode();
				  $$->node_right = $2;
				  $$->node_type = OP_NOT;		}
	| INTCONST		{ $$ = makenode();
				  $$->node_type = OP_VAR;
				  $$->node_psym = makeisym( $1 );	}
	| STRCONST		{ $$ = makenode();
				  $$->node_type = OP_VAR;
				  $$->node_psym = makessym( $1 );	}
	| var			{ $$ = makenode();
				  $$->node_psym = $1;
				  $$->node_type = OP_VAR;		}
	| var DECR		{ $$ = makenode();
				  $$->node_type = OP_POSTD;
				  $$->node_psym = $1;			}
	| var INCR		{ $$ = makenode();
				  $$->node_psym = $1;
				  $$->node_type = OP_POSTI;		}
	| DECR var		{ $$ = makenode();
				  $$->node_psym = $2;
				  $$->node_type = OP_PRED;		}
	| INCR var		{ $$ = makenode();
				  $$->node_psym = $2;
				  $$->node_type = OP_PREI;		}
	| cmd			{ $$ = $1; inrawcmd = 0;		}
	;
var:	VAR			{ $$ = $1;				}
	| dollarvar
	;
dollarvar: '$' VAR		{ $$ = $2;				}
	| '$' IDENT		{ $$ = makesym();
				  $$->sym_type = SYM_VAR;
				  $$->sym_name = $2;
				  addsym($$);				}
	| '$' '$'		{ $$ = makesym();
				  $$->sym_type = SYM_PARAM;
				  $$->sym_indx = MAXARGC+1;		}
	| '$' INTCONST		{ $$ = makesym();
				  $$->sym_type = SYM_PARAM;
				  $$->sym_indx =  $2;
				  if ( $2 > MAXARGC || $2 < 0 )
					yyerror("parameter out of range");
									}
	;
cmdbeg: CMD			{ if ( $1->sym_flags & SYM_CHARARGV )
					inrawcmd = 1;			}
	;
cmd: cmdbeg arglist 		{ $$ = makenode();
				  $$->node_psym = $1;
				  $$->node_right= $2;
				  $$->node_type = OP_CMD;		}
	| cmdbeg '(' arglist2 ')'	{ $$ = makenode();
				  $$->node_psym = $1;
				  $$->node_right= $3;
				  $$->node_type = OP_CMD;		}
	| IDENT arglist		{ char buf[128];

				  if ( inblock ) {
				      sprintf(buf,"\nERROR: bad cmd \"%s\"",$1);
				      yyerror(buf);
				      psignal(SIGINT);
				  } else
				    sprintf(buf,"unknown command \"%s\"",$1);
				  yyerror(buf);
				  destroyargv($2);
				  destroystr($1);
				  $$ = 0;
				  YYERROR;				}
	;
arglist: /*empty*/		{ $$ = 0;				}
	| arglist arg		{ $$ = makenode();
				  $$->node_type  = OP_ARGLIST;
				  $$->node_right = $1;
				  $$->node_left  = $2;			}
	;
arglist2: /* empty */		{ $$ = 0;				}
	| carglist		{ $$ = $1;				}
	;
carglist: arg			{ $$ = $1;				}
	| carglist ',' arg	{ $$ = makenode();
				  $$->node_type  = OP_ARGLIST;
				  $$->node_right = $1;
				  $$->node_left  = $3;			}
	;
arg:	dollarvar		{ $$ = makenode();
				  $$->node_type = OP_VAR;
				  $$->node_psym = $1;			}
	| INTCONST		{ $$ = makenode();
				  $$->node_type = OP_VAR;
				  $$->node_psym = makeisym( $1 );	}
	| STRCONST		{ $$ = makenode();
				  $$->node_type = OP_VAR;
				  $$->node_psym = makessym( $1 );	}
	| IDENT			{ $$ = makenode();
				  $$->node_type = OP_VAR;
				  $$->node_psym = makesym();
				  $$->node_psym->sym_s = makestr($1);
				  $$->node_psym->sym_basetype = SYMB_STR;}
	| '-' IDENT		{ $$ = makenode();
				  $$->node_type = OP_VAR;
				  $$->node_psym = makesym();
				  $$->node_psym->sym_s = makestr($2);
				  $$->node_psym->sym_basetype = SYMB_STR;
				  $$->node_psym->sym_type = SYM_CMDFLAG;}
	| '`' expr '`'		{ $$ = $2;				}
	| range			{ $$ = makenode();
				  $$->node_type = OP_VAR;
				  $$->node_psym = $1;			}
	;
range: INTCONST '#' INTCONST	{ $$ = makesym();
				  $$->sym_basetype = SYMB_RANGE;
				  $$->sym_type  = SYM_CON;
				  $$->sym_start = $1;
				  $$->sym_count = $3;			}
	| INTCONST ':' INTCONST	{ $$ = makesym();
				  $$->sym_basetype = SYMB_RANGE;
				  $$->sym_type 	= SYM_CON;
				  $$->sym_start = $1;
				  $$->sym_count = $3-$1;		}
	;
%%
#define gettxt(msgid,dflt_str)		(dflt_str)

node_t *
makebop(node_t *lnode, node_t *rnode, int op)
{
	node_t *tmp = makenode();

	tmp->node_right = rnode;
	tmp->node_left  = lnode;
	tmp->node_type  = op;

	return tmp;
}

node_t *
makeassop(sym_t *psym, node_t *rnode, int op)
{
	node_t *tmp = makenode();

	tmp->node_right = rnode;
	tmp->node_psym  = psym;
	tmp->node_type  = op;

	return tmp;
}

void
checkcase(node_t *pnode, int val, int flag)
{
	if ( ! pnode )
		return;

	if ( pnode->node_type == OP_CASELIST ) {
		checkcase(pnode->node_right, val, flag);
		pnode = pnode->node_left;
		}
	
	if ( flag ) {
		if ( pnode->node_psym )
			return;
		yyerror("duplicate default case in switch");
		return;
		}

	if ( pnode->node_psym && (pnode->node_psym->sym_i == val) )
		yyerror("duplicate case in switch");
	
}
