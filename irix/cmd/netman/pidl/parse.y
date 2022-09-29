%{
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * Yacc grammar for PIDL.
 */
#define __my_yyerror 1
#include "pidl.h"
#include "scope.h"
#include "treenode.h"

int	indecl;		/* exported to scan.l for typename recognition */
%}

%union {
	typeword_t	tw;		/* type descriptor */
	Symbol		*sym;		/* symbol scope entry */
	long		val;		/* integer constant */
	Address		addr;		/* address byte-string */
	enum operator	op;		/* operator code */
	TreeNode	*tn;		/* tree node pointer */
}

%token		CONST DEFINE EXPECT EXPORT PRAGMA PROTOCOL STRUCT TYPEDEF
%token	<op>	ENUM MACHREG
%token		IF ELSE SWITCH CASE DEFAULT FOOTER NEST UNION PROGRAM VERSION

%token	<sym>	IMPORT NAME STRING TYPE RPCLTYPE
%token	<val>	DREP LEVEL NUMBER
%token	<addr>	ADDRESS

%left		','
%right		'='
%right		'?' ':'
%left		OR			/* || */
%left		AND			/* && */
%left		'|'
%left		'^'
%left		'&'
%left	<op>	EQOP			/* ==, != */
%left	<op>	'<' '>' RELOP		/* <=, >= */
%left	<op>	SHOP			/* <<, >> */
%left		'+' '-'
%left	<op>	'*' DIVOP		/* /, % */
%right	<op>	UNOP SIZEOF		/* !, ~, (-), sizeof */
%right		'[' '.'

%type	<tn>	outer_definitions outer_definition yaccable_import
		protocol protocol_name opt_formals formals formal
		expr aexpr cexpr sexpr opt_expr opt_sexpr mexpr
		abstract_type abstract_declarator nbf_abstract_declarator
		opt_arguments arguments opt_bases bases base typecode
		opt_members members member
		definition enumerators enumerator
		exportable_definition exportable_names
		declaration init_declaration init_declarator level_declarator
		declarator nbf_declarator opt_declarator_name declarator_name
		typedef_declaration typedef_declarator nbf_typedef_declarator
		typedef_name
		cases case labels label opt_footer pragmas pragma
		rpcl_program rpcl_versions rpcl_version
		rpcl_procedures rpcl_procedure
%type	<val>	opt_drep
%type	<tw>	opt_type type
%type	<sym>	block_tag

%%

/*
 * PIDL input is a sequence of external definitions.
 */
input:
	outer_definitions			{ parsetree = $1; }
;

outer_definitions:
	/* empty */				{ $$ = list(0); }
|	outer_definitions ';'
|	outer_definitions outer_definition	{ $$ = append($1, $2); }
|	outer_definitions error ';'		{ yyerrok; }
;

/*
 * An external definition is an import directive, a protocol definition,
 * or a constant, macro, or type definition.  PIDL optionally recognizes
 * RPCL input intended for Sun's rpcgen compiler.
 */
outer_definition:
	yaccable_import '$'
|	protocol
|	EXPORT protocol			{ $$ = unary(opExport, 0, $2); }
|	definition ';'
|	PRAGMA pragmas ';'		{ $$ = $2; }
|	rpcl_program
;

/*
 * We would like to parse imported modules after recognizing their names
 * in import statements, but yacc parsers are not reentrant.  The scanner
 * instead expands each module's contents (including nested imports) into
 * a block of outer_definitions.
 */
yaccable_import:
	IMPORT '{' outer_definitions '}'
					{ $$ = unary(opImport, $1, $3); }
;

/*
 * A protocol may begin with a data representation (byte order), formal
 * arguments (see below), and bases that tell in which protocols this one
 * is embedded.  It then defines all fields in its scope.
 */
protocol:
	opt_drep PROTOCOL protocol_name opt_formals opt_bases
					{ $$ = openprotocol((enum datarep)$1,
							$3, $4, $5); }
	'{' opt_members opt_footer '}'	{ $$ = closeprotocol($<tn>6, $8, $9); }
;

opt_drep:
	/* empty */			{ $$ = drNil; }
|	DREP
;

/*
 * A protocol name is a C-like identifier and possibly a string title.
 */
protocol_name:
	NAME			{ $$ = declaration($1, 0, stProtocol); }
|	NAME STRING		{ $$ = declaration($1,$2->s_name,stProtocol); }
;

/*
 * Protocol formal arguments express operations on fields in this protocol
 * and its base protocols.  Each formal tells which fields to fetch in order
 * to compute a typecode distinguishing this protocol when matching higher
 * layer protocols.  A formal may have an optional expression that qualifies
 * a match of the first expression against a nested layer's typecode.
 */
opt_formals:
	/* empty */		{ $$ = 0; }
|	'(' formals ')'		{ $$ = $2; }
;

formals:
	formal
|	formals ',' formal	{ $$ = append($1, $3); }
;

formal:
	cexpr
|	cexpr ':' cexpr		{ $$ = binary(opQualify, $1, $3); }
;

/*
 * The top-level expression nonterminal is a comma expression.
 */
expr:
	expr ',' expr		{ $$ = binary(opComma, $1, $3); }
|	aexpr
;

/*
 * Assignment, stratified from comma expression to disambiguate the commas
 * in argument lists.
 */
aexpr:
	aexpr '=' aexpr		{ $$ = binary(opAssign, $1, $3); }
|	cexpr
;

/*
 * Conditional expression, stratified from expr to disambiguate comma's
 * use in an argument, enumerator, or declarator list from its use in an
 * initializer expression by requiring parenthese for the last.
 */
cexpr:
	cexpr '?' cexpr ':' cexpr	{ $$ = ternary(opCond, $1, $3, $5); }
|	cexpr OR cexpr			{ $$ = binary(opOr, $1, $3); }
|	cexpr AND cexpr			{ $$ = binary(opAnd, $1, $3); }
|	cexpr '|' cexpr			{ $$ = binary(opBitOr, $1, $3); }
|	cexpr '^' cexpr			{ $$ = binary(opBitXor, $1, $3); }
|	cexpr '&' cexpr			{ $$ = binary(opBitAnd, $1, $3); }
|	cexpr EQOP cexpr		{ $$ = binary($2, $1, $3); }
|	cexpr '<' cexpr			{ $$ = binary(opLT, $1, $3); }
|	cexpr '>' cexpr			{ $$ = binary(opGT, $1, $3); }
|	cexpr RELOP cexpr		{ $$ = binary($2, $1, $3); }
|	sexpr
;

/*
 * Shift expression, stratified from cexpr to disambiguate '<' and '>' in
 * relationals from their use as variable-length array dimension brackets
 * (see abstract_declarator, below).
 */
sexpr:
	sexpr SHOP sexpr		{ $$ = binary($2, $1, $3); }
|	sexpr '+' sexpr			{ $$ = binary(opAdd, $1, $3); }
|	sexpr '-' sexpr			{ $$ = binary(opSub, $1, $3); }
|	sexpr '*' sexpr			{ $$ = binary(opMul, $1, $3); }
|	sexpr DIVOP sexpr		{ $$ = binary($2, $1, $3); }
|	'*' sexpr %prec UNOP		{ $$ = unary(opDeref, 0, $2); }
|	'&' sexpr %prec UNOP		{ $$ = unary(opAddrOf, 0, $2); }
|	'-' sexpr %prec UNOP		{ $$ = unary(opNeg, 0, $2); }
|	UNOP sexpr			{ $$ = unary($1, 0, $2); }
|	SIZEOF sexpr			{ $$ = unary(opSizeOf, 0, $2); }
|	SIZEOF abstract_type		{ $$ = unary(opSizeOf, 0, $2); }
|	abstract_type sexpr %prec UNOP	{ $$ = binary(opCast, $1, $2); }
|	sexpr '[' expr ']'		{ $$ = binary(opIndex, $1, $3); }
|	NAME '(' opt_arguments ')'	{ $$ = unary(opCall, $1, $3); }
|	block_tag '{' arguments '}'	{ $$ = block($1, $3); }
|	mexpr
|	NUMBER				{ $$ = number($1); }
|	ADDRESS				{ $$ = address($1); }
|	STRING				{ $$ = string($1); }
|	MACHREG				{ $$ = treenode($1, 0); }
|	'(' expr ')'			{ $$ = $2; }
;

/*
 * Abstract type, in parentheses.
 */
abstract_type:
	'(' type abstract_declarator ')'
					{ $$ = declaretype($2, $3); }
;

opt_type:
	/* empty */			{ $$.tw_word = 0; }
|	type
;

type:
	TYPE				{ $$ = typeword($1, 0, 0, 0, drNil); }
|	DREP TYPE			{ $$ = typeword($2, 0, 0, 0,
							(enum datarep)$1); }
|	DREP TYPE TYPE			{ $$ = typeword($2, $3, 0, 0,
							(enum datarep)$1); }
|	DREP TYPE TYPE TYPE		{ $$ = typeword($2, $3, $4, 0, 
							(enum datarep)$1); }
|	DREP TYPE TYPE TYPE TYPE	{ $$ = typeword($2, $3, $4, $5, 
							(enum datarep)$1); }
|	opt_drep ENUM TYPE		{ $$ = tagtype($3, (enum datarep)$1,
						       btEnum); }
|	opt_drep STRUCT TYPE		{ $$ = tagtype($3, (enum datarep)$1,
						       btStruct); }
|	RPCLTYPE			{ $$ = typeword($1, 0, 0, 0, drXDR);
					  indecl = 1; }
;

/*
 * Abstract declarator, used in cast and sizeof expressions.  Unlike C, PIDL
 * has no function (prototype) declarator.
 */
abstract_declarator:
	nbf_abstract_declarator
|	':' cexpr			{ $$ = declaration(0, 0, stNil);
					  bitfield($$, $2); }
;

nbf_abstract_declarator:
	/* empty */			{ $$ = declaration(0, 0, stNil); }
|	'*' nbf_abstract_declarator %prec UNOP
					{ $2->tn_type = qualify($2->tn_type,
								tqPointer);
					  $$ = $2; }
|	nbf_abstract_declarator '[' opt_expr ']'
					{ array($1, tqArray, $3); }
|	nbf_abstract_declarator '<' opt_sexpr '>'
					{ array($1, tqVarArray, $3); }
|	'(' abstract_declarator ')'	{ $$ = $2; }
;

opt_expr:
	/* empty */			{ $$ = treenode(opNil, 0); }
|	expr
;

opt_sexpr:
	/* empty */			{ $$ = treenode(opNil, 0); }
|	sexpr
;

mexpr:
	mexpr '.' mexpr			{ $$ = binary(opMember, $1, $3); }
|	NAME				{ $$ = name($1); }
;

opt_arguments:
	/* empty */			{ $$ = list(0); }
|	arguments
;

arguments:
	aexpr				{ $$ = list($1); }
|	arguments ',' aexpr		{ $$ = append($1, $3); }
;

block_tag:
	NAME
|	TYPE
;

/*
 * If a protocol nests in lower layers, its name and formals are followed
 * by base protocol specifiers.
 */
opt_bases:
	/* empty */			{ $$ = 0; }
|	':' bases			{ $$ = $2; }
|	error bases			{ yyerrok; $$ = $2; }
;

bases:
	base
|	bases ',' base			{ $$ = append($1, $3); }
;

/*
 * A base protocol specifier names the protocol in which to embed this one,
 * and the typecode and optional qualifier with which to discriminate among
 * this protocol and others embedded in the same base.
 */
base:
	NAME				{ $$ = base($1, 0); }
|	NAME '(' typecode ')'		{ $$ = base($1, $3); }
;

typecode:
	cexpr
|	cexpr ':' cexpr			{ $$ = binary(opQualify, $1, $3); }
;

opt_members:
	/* empty */			{ $$ = 0; }
|	members
;

members:
	member
|	members member			{ $$ = append($1, $2); }
|	members error ';'		{ yyerrok; }
;

/*
 * A PIDL member either defines a constant, macro, struct, or a type based
 * on other types; or it declares new bases for a protocol, conditioned by
 * enclosing if or switch members; or it declares a field in a protocol or
 * a struct.  Protocols can be nested dynamically.
 */
member:
	expr ';'
|	definition ';'
|	declaration ';'
|	declaration error ';'		{ yyerrok; }
|	IF '(' expr ')' member		{ $$ = ternary(opIfElse, $3, $5, 0); }
| 	IF '(' expr ')' member ELSE member
					{ $$ = ternary(opIfElse, $3, $5, $7); }
|	SWITCH '(' expr ')' '{' cases '}'
					{ $$ = binary(opSwitch, $3, $6); }
|	'{' opt_members '}'		{ $$ = $2; }
|	NEST NAME ':' bases ';'		{ $$ = unary(opNest, $2, $4); }
;

definition:
	EXPORT exportable_names		{ $$ = unary(opExport, 0, $2); }
|	EXPORT exportable_definition	{ $$ = unary(opExport, 0, $2); }
|	EXPORT declaration		{ $$ = unary(opExport, 0, $2); }
|	DEFINE NAME '=' STRING		{ $$ = macro($2, $4); }
|	exportable_definition
;

exportable_names:
	mexpr
|	exportable_names ',' mexpr	{ $$ = append($1, $3); }
;

exportable_definition:
	CONST opt_type enumerators	{ $$ = constdefs($2, $3); }
|	opt_drep ENUM NAME '{' enumerators '}'
					{ $$ = enumdef($2, (enum datarep)$1,
						       $3, $5); }
|	opt_drep STRUCT NAME		{ $$ = openstruct((enum datarep)$1,
							$3); }
	'{' opt_members '}'		{ $$ = closestruct($<tn>4, $6); }
|	UNION NAME SWITCH		{ $$ = openstruct(drXDR, $2); }
	'(' type declarator ')'		{ indecl = 0; }
	'{' cases '}'			{ $$ = closestruct($<tn>4,
						append(declaretype($6, $7),
						       binary(opSwitch,
							      name($7->tn_sym),
							      $11)));
					}
|	TYPEDEF typedef_declaration	{ $$ = $2; indecl = 0; }
;

enumerators:
	enumerator			{ $$ = list($1); }
|	enumerators ',' enumerator	{ $$ = append($1, $3); }
;

enumerator:
	NAME				{ $$ = unary(opEnumerator, $1, 0); }
|	NAME '=' cexpr			{ $$ = unary(opEnumerator, $1, $3); }
;

/*
 * Typedef declaration, separated from declaration so that new type names
 * can be defined at parse-time.
 */
typedef_declaration:
	type typedef_declarator		{ $$ = declaretype($1, $2); }
|	typedef_declaration ',' typedef_declarator
					{ $$ = append($1,
					  declaretype(first($1)->tn_type,$3)); }
;

typedef_declarator:
	nbf_typedef_declarator
|	typedef_name ':' cexpr		{ bitfield($1, $3); }
;

nbf_typedef_declarator:
	typedef_name
|	'*' nbf_typedef_declarator %prec UNOP
					{ $2->tn_type = qualify($2->tn_type,
								tqPointer);
					  $$ = $2; }
|	nbf_typedef_declarator '[' opt_expr ']'
					{ array($1, tqArray, $3); }
|	nbf_typedef_declarator '<' opt_sexpr '>'
					{ array($1, tqVarArray, $3); }
|	'(' typedef_declarator ')'	{ $$ = $2; }
;

typedef_name:
	NAME				{ $$ = declaration($1, 0, stTypeDef); }
;

declaration:
	init_declaration		{ indecl = 0; }
;

init_declaration:
	type init_declarator		{ $$ = declaretype($1, $2); }
|	init_declaration ',' init_declarator
					{ $$ = append($1,
					  declaretype(first($1)->tn_type,$3)); }
;

/*
 * An optionally initialized declarator.
 */
init_declarator:
	level_declarator
|	level_declarator '=' cexpr	{ $1->tn_right = $3; }
|	level_declarator EXPECT cexpr	{ $3->tn_flags |= tfExpected;
					  $1->tn_right = $3; }
;

/*
 * Declarator with optional verbosity level.
 */
level_declarator:
	declarator
|	declarator LEVEL		{ $1->tn_sym->s_level = $2; }
;

/*
 * Simple field declarator.
 */
declarator:
	nbf_declarator
|	opt_declarator_name ':' cexpr	{ bitfield($1, $3); }
;

/*
 * Non-bitfield declarator.
 */
nbf_declarator:
	opt_declarator_name
|	'*' nbf_declarator %prec UNOP	{ $2->tn_type = qualify($2->tn_type,
								tqPointer);
					  $$ = $2; }
|	nbf_declarator '[' opt_expr ']'	{ array($1, tqArray, $3); }
|	nbf_declarator '<' opt_sexpr '>'
					{ array($1, tqVarArray, $3); }
|	'(' declarator ')'		{ $$ = $2; }
;

/*
 * Declarators need not be named, e.g., "void;" for an empty switch case.
 */
opt_declarator_name:
	/* empty */		{ $$ = declaration(0, 0, stField); }
|	declarator_name
;

/*
 * A declarator name is just like a protocol name.  We use a separate rule
 * in order to pass top-down information (the symbol type) cleanly.
 */
declarator_name:
	NAME			{ $$ = declaration($1, 0, stField); }
|	NAME STRING		{ $$ = declaration($1, $2->s_name, stField); }
;

cases:
	case			{ $$ = list($1); }
|	cases case		{ $$ = append($1, $2); }
;

case:
	labels members		{ $$ = binary(opCase, $1, $2); }
;

labels:
	label
|	labels label		{ $$ = append($1, $2); }
;

label:
	CASE expr ':'		{ $$ = $2; }
|	DEFAULT ':'		{ $$ = treenode(opDefault, 0); }
;

opt_footer:
	/* empty */		{ $$ = 0; }
|	FOOTER ':' opt_members	{ $$ = $3; }
;

pragmas:
	pragma
|	pragmas ',' pragma	{ $$ = append($1, $3); }
;

pragma:
	NAME			{ $$ = unary(opPragma, $1, number(1)); }
|	NAME '=' cexpr		{ $$ = unary(opPragma, $1, $3); }
|	NAME '(' arguments ')'	{ $$ = unary(opPragma, $1, $3); }
;

rpcl_program:
	PROGRAM NAME '{' rpcl_versions '}' '=' NUMBER ';'
				{ $$ = 0; }
;

rpcl_versions:
	rpcl_version
				{ $$ = 0; }
|	rpcl_versions rpcl_version
				{ $$ = 0; }
;

rpcl_version:
	VERSION NAME '{' rpcl_procedures '}' '=' NUMBER ';'
				{ $$ = 0; }
;

rpcl_procedures:
	rpcl_procedure
				{ $$ = 0; }
|	rpcl_procedures rpcl_procedure
				{ $$ = 0; }
;

rpcl_procedure:
	RPCLTYPE NAME '(' RPCLTYPE ')' '=' NUMBER ';'
				{ $$ = 0; }
;

%%
