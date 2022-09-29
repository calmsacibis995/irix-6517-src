%{
#include "libdplace.c"
int yylex(void);
#define malloc(x) __dplace_malloc_check(x)
#define realloc(x,y) __dplace_realloc_check(x,y)
#define calloc(x,y) __dplace_calloc_check(x,y)
%}


%union{
  __int64_t x;
  __int64_t r[2];
  int val[4];
  char	c;
  char	*s;
  struct list f_list;
}
 
%start	list
%token	COMMENT NUMBER NODES NODE IN NEAR PAGESIZE 
%token	FILENAME CUBE NONE CLUSTER RANGE ON THREADS THREAD 
%token	PLACE TOPOLOGY PERCENT 
%token	RUN LINK TO WITH K M TOPOLOGY_TYPE GLOBAL POLICY MIGRATION
%token	MIGRATE MODE VERBOSE MOVE STACK DATA TEXT PID TOGGLE OFF
%token  DISTRIBUTE ACROSS BLOCK CYCLIC STRING PLACEMENT
%token  USING CPU LEVEL PAGEWAIT
%left  '+' '-'
%left  '*' '/' '%'
%left UMINUS
%%
list	:	/* empty */
|	list stat '\n'
{ d->line_count++;}
|	list COMMENT '\n'
{ d->line_count++;}
|	list stat COMMENT '\n'
{ d->line_count++;}
|	list '\n'
{ d->line_count++;}
|	list error '\n'
{ d->line_count++; yyerrok;}
;
stat	:	nexpr
{ place_nodes(); }
|	texpr
|	rexpr
|	mexpr
|	lexpr
|	pexpr
|	dexpr
|	modeset
;
modeset	:	MODE VERBOSE
{ d->verbose |= 1; }
;
|	MODE VERBOSE TOGGLE
{ d->verbose ^= 1; }
;
|	MODE VERBOSE ON
{ d->verbose = 1; }
;
|	MODE VERBOSE OFF
{ d->verbose = 0; }
;
pexpr	:	POLICY DATA PAGESIZE expr
{ set_dpagesize((int)$4.x);}
|	POLICY DATA PAGESIZE expr K
{ set_dpagesize((int)$4.x*1024);}
|	POLICY DATA PAGESIZE expr M
{ set_dpagesize((int)$4.x*1024*1024);}
|	POLICY STACK PAGESIZE expr
{ set_spagesize((int)$4.x);}
|	POLICY STACK PAGESIZE expr K
{ set_spagesize((int)$4.x*1024);}
|	POLICY STACK PAGESIZE expr M
{ set_spagesize((int)$4.x*1024*1024);}
|	POLICY TEXT PAGESIZE expr
{ set_tpagesize((int)$4.x);}
|	POLICY TEXT PAGESIZE expr K
{ set_tpagesize((int)$4.x*1024);}
|	POLICY TEXT PAGESIZE expr M
{ set_tpagesize((int)$4.x*1024*1024);}
|	POLICY DATA PAGEWAIT OFF
{ set_dpagewait(0);}
|	POLICY DATA PAGEWAIT ON
{ set_dpagewait(1);}
|	POLICY STACK PAGEWAIT OFF
{ set_spagewait(0);}
|	POLICY STACK PAGEWAIT ON
{ set_spagewait(1);}
|	POLICY TEXT PAGEWAIT OFF
{ set_tpagewait(0);}
|	POLICY TEXT PAGEWAIT ON
{ set_tpagewait(1);}
|	POLICY MIGRATION ON
{ d->migration_enabled = 1;}
|	POLICY MIGRATION OFF
{ d->migration_enabled = 0;}
|	POLICY MIGRATION LEVEL expr
{ set_migration((int)($4.x));}
|	POLICY MIGRATION LEVEL expr PERCENT
{ set_migration((int)($4.x));}
|       POLICY MIGRATION expr /* Back compatibility... */
{ if($3.x){  d->migration_enabled = 1; set_migration((int)(100-$3.x));}
  else { d->migration_enabled = 0;}}
|       POLICY MIGRATION expr PERCENT /* Back compatibility... */
{ if($3.x){  d->migration_enabled = 1; set_migration((int)(100-$3.x));}
  else { d->migration_enabled = 0;}}
|	POLICY DATA PLACEMENT STRING
{ strncpy(d->data_policy,$4.s,PM_NAME_SIZE); }
|	POLICY STACK PLACEMENT STRING
{ strncpy(d->stack_policy,$4.s,PM_NAME_SIZE); }
|	POLICY TEXT PLACEMENT STRING
{ strncpy(d->text_policy,$4.s,PM_NAME_SIZE); }
;
lexpr	:	RUN THREAD expr ON NODE expr
{ add_link((int)$3.x,(int)$6.x); }
|		RUN THREAD expr ON NODE expr USING CPU expr
{ add_cpulink((int)$3.x,(int)$6.x, (int)$9.x ); }
|	MOVE THREAD expr TO NODE expr
{ move_link((int)$3.x,(int)$6.x); }
|	MOVE PID expr TO NODE expr
{ move_pid((int)$3.x,(int)$6.x); }
;
rexpr	:	PLACE RANGE range ON NODE expr 
{ add_range($3.r[0],$3.r[1],(int)$6.x,(int)d->dpagesize); }
|	PLACE RANGE range ON NODE expr pspec
{ add_range($3.r[0],$3.r[1],(int)$6.x,(int)$7.x); }
;
mexpr	:	MIGRATE RANGE range TO NODE expr 
{ migrate_range($3.r[0],$3.r[1],(int)$6.x); }
;
range	:	expr TO expr
{ $$.r[0] = $1.x ; $$.r[1] = $3.x;}
;
expr    :       '(' expr ')'
{ $$.x = $2.x ; }
|       expr '+' expr
{ $$.x = $1.x + $3.x ; }
|       expr '-' expr
{ $$.x = $1.x - $3.x ; }
|       expr '*' expr
{ $$.x = $1.x * $3.x ; }
|       expr '/' expr
{ $$.x = $1.x / $3.x ; }
|       expr '%' expr
{ $$.x = $1.x % $3.x ; }
|	'-' expr
{ $$.x = -$2.x ; }
|       NUMBER
{ $$.x = $1.x ; }
;

texpr	:	THREADS expr
{ set_threads((int)$2.x); }
;
nexpr	:	NODES expr
{ set_nodes((int)$2.x); }
|	NODES expr qual
{ set_nodes((int)$2.x); }
;
qual	:	tspec
|	tspec qual
|	aspec
|	aspec qual
;
tspec	:	IN TOPOLOGY_TYPE
{ set_topology((int)$2.x); }
|	TOPOLOGY TOPOLOGY_TYPE
{ set_topology((int)$2.x); }
|	IN TOPOLOGY TOPOLOGY_TYPE
{ set_topology((int)$3.x); }
;
aspec	:	NEAR file_list
{ set_affinity($2.f_list); }
;
pspec	:	PAGESIZE expr
{ $$.x = $2.x ; }
|	PAGESIZE expr K
{ $$.x = $2.x*1024 ; }
|	PAGESIZE expr M
{ $$.x = $2.x*1024*1024 ; }
|	WITH PAGESIZE expr
{ $$.x = $3.x ; }
|	WITH PAGESIZE expr K
{ $$.x = $3.x*1024 ; }
|	WITH PAGESIZE expr M
{ $$.x = $3.x*1024*1024 ; }
;
file_list:	FILENAME
{ $$.f_list.name = $1.s ; $$.f_list.next = 0;}
| 		STRING
{ $$.f_list.name = $1.s ; $$.f_list.next = 0;}
|	FILENAME file_list
{ $$.f_list.name = $1.s ; $$.f_list.next = &$2.f_list;}
|	STRING   file_list
{ $$.f_list.name = $1.s ; $$.f_list.next = &$2.f_list;}
;
trange	:	THREADS nrange
{ $$ = $2;}
|	THREADS
{ $$.val[3] = 0;}
;
mrange	:	NODES nrange
{ $$ = $2;}
|	NODES
{ $$.val[3] = 0;}
;
nrange	:	expr ':' expr
{ $$.val[0] = $1.x ; $$.val[1] = $3.x ; $$.val[2] = 1; $$.val[3] = 1;}
|	expr ':' expr ':' expr
{ $$.val[0] = $1.x ; $$.val[1] = $3.x ; $$.val[2] = $5.x ; $$.val[3] = 1;}	;

dexpr	:	DISTRIBUTE trange ACROSS mrange
{ distribute_threads((int)dist_default,(int)0,$2.val[0],$2.val[1],$2.val[2],$2.val[3],$4.val[0],$4.val[1],$4.val[2],$4.val[3]); }
|	DISTRIBUTE trange ACROSS mrange BLOCK
{ distribute_threads((int)dist_default,(int)0, $2.val[0],$2.val[1],$2.val[2],$2.val[3],$4.val[0],$4.val[1],$4.val[2],$4.val[3]); }
|	DISTRIBUTE trange ACROSS mrange BLOCK expr
{ distribute_threads((int)block,(int)$6.x, $2.val[0],$2.val[1],$2.val[2],$2.val[3],$4.val[0],$4.val[1],$4.val[2],$4.val[3]); }
|	DISTRIBUTE trange ACROSS mrange CYCLIC
{ distribute_threads((int)cyclic,(int)1, $2.val[0],$2.val[1],$2.val[2],$2.val[3],$4.val[0],$4.val[1],$4.val[2],$4.val[3]); }
|	DISTRIBUTE trange ACROSS mrange CYCLIC expr
{ distribute_threads((int)cyclic,(int)$6.x, $2.val[0],$2.val[1],$2.val[2],$2.val[3],$4.val[0],$4.val[1],$4.val[2],$4.val[3]); }
;

%%
 
