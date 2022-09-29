 /*____________________________________________________________________________
//
// Copyright (C) 1995 Silicon Graphics, Inc
//
// All Rights Reserved Worldwide.
//
//____________________________________________________________________________
//
// FILE NAME :          loc.y
//
// DESCRIPTION :        source for parser generator used to validate input
//                      POSIX-format locale definition and translate to
//                      AT&T-format locale definition
//
//
//____________________________________________________________________________
*/
%{
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/file.h>
#include <limits.h>
#include <locale.h>

#include "localedef.h"
#include "tables.h"
#include "collate.h"

extern char yytext[];
extern int yylineno;

extern char *output_filename;
extern int wflag;

static char *lname = NULL;

/*
 * These global flags are set in tables.c;  used
 * to understand the structure of the chrtbl based on 
 * the format of values in the charmap.  Note that they
 * won't necessarily be used...
 */

extern int ctype_width;
extern int ctype1;
extern int ctype2;
extern int ctype3;

#define BUFLEN   100              /* XXX arbitrary length */ 
#define FBUFLEN  256              /* XXX assume lines no longer than 256 characters */
#define BIGBUFLEN 4096            

static char buf[BUFLEN + 1];      /* Buffer for processing string tokens */
static char fbuf[FBUFLEN + 1];    /* Buffer for file line i/o (fgets/fputs). */
static char conv_buf[BIGBUFLEN];  /* Buffer for converting CHARSYMBOL strings */
static char elem_buf[BUFLEN + 1]; /* Buffer for converting elem-lists */

static encs_t * weights [ COLL_WEIGHTS_MAX ];
static int weights_counter = 0;

#define clear_buf(b)       { memset(b, '\0', sizeof(b)); }

/* the file nl_lang_* will have seven lines, */
/* while the file LC_TIME will have 45 lines */
/* plus about 4 due to the era keywords      */

#define NL_LANG_SIZE 8
#define LC_TIME_SIZE 45 /* 12 mnths + 12 abv. mnths + 7 day + 7 abdays + 7 formats */
#define LC_TIME_OPT_SIZE 5

/* defines for which lines LC_MESSAGES strings go on */
#define TIME_POS   0
#define DATE_POS   1
#define T_D_POS    2
#define YESSTR_POS 3
#define NOSTR_POS  4
#define YESEXP_POS 5
#define NOEXP_POS  6
#define FMT_AMPM_POS 7

#define MBWC_POS 0
#define CODESET_POS 1
#define ICONV_SPEC_SIZE 2

#define IRIXWC "irix-wc"
#define LIBCSO "libc.so.1"
#define MBWCSTRING "MBWC none none WCMB none none ;"


/* and nllang_buf is an array of buffers for each line */
static char nllang_buf[NL_LANG_SIZE][BUFLEN];
static char lctime_buf[LC_TIME_SIZE][BUFLEN];
static char lctime_temp[12][BUFLEN];
static char *lctime_opt[LC_TIME_OPT_SIZE];

static int current_kwd;
static int i, indxx = 0, toggle = 0;
static int count = 0;
static int toupper_kwd = 0;
static int unsupported = 0;
static int lctime_opt_ctr = 0;
static int collorder_count = 0;

/* counter for AT&T EXTENSIONS - if this counter != NUM_EXTENSIONS
 *
 */
 
static int att_extensions = 0;

#define NUM_EXTENSIONS 4

char escape_char = '\\';
static char first_entry[BUFLEN + 1];      
static char second_entry[BUFLEN + 1];    
static char ubuf[BUFLEN + 1];      
static char lbuf[BUFLEN + 1];     

static int alt_keywords = 0;
static symbol_ptr lctype1_list[MAX_KEYWORDS];
static symbol_ptr lctype2_list[MAX_KEYWORDS];
static symbol_ptr lctype3_list[MAX_KEYWORDS];

char *keywords[MAX_KEYWORDS] =
{"upper", "lower", "alpha", "digit", "punct", "xdigit", "space", "print",
 "graph", "blank", "cntrl", NULL, NULL, "ideogram", "phonogram", "english",
 "special"};

%}

/* 
 * Return value of token parsed by lexical analyzer. 
 */
%union {
    int		ival;
    char  *	str;
    encs_t *	encs;
    collorder_t	collorder;
}

/* Tokens common to several locale categories. */
%token <str>		LOC_NAME
%token <str>		COLLSYMBOL COLLELEMENT
%token <str>		CHARSYMBOL CHARCLASS
%token <str>		CHAR OCTAL_CHAR HEX_CHAR DECIMAL_CHAR
%token <str>		ELLIPSIS 
%token <str>		EXTENDED_REG_EXP
%token <ival>		NUMBER
%token <ival>		EOL

/* Used wherever an symbolic name (<...>) is defined */
%token <str>		SYMBOLIC_NAME

/* Tokens representing literal characters. */
%token <str> DOUBLE_QUOTE SEMI_COLON OPEN_PAREN CLOSE_PAREN COMMA 

/* Tokens representing specific locale categories. */
%token <str> LC_CTYPE_TOK LC_COLLATE_TOK LC_MESSAGES_TOK 
%token <str> LC_MONETARY_TOK LC_NUMERIC_TOK LC_TIME_TOK

/* Tokens representing literal strings. */
%token <str> ESCAPE_CHAR COMMENT_CHAR CODE_SET_NAME END FROM UNDEFINED
%token <str> IGNORE NEG_ONE DQS EMPTY_STRING COPY GROUPING

/* Tokens and keywords used in LC_TIME category rules. */
%token <str> TIME_KWD_NAME TIME_KWD_FMT TIME_KWD_OPT 
%type <str>  TIME_KWD_NAME TIME_KWD_FMT TIME_KWD_OPT 
%type <str>  time_keyword_name time_keyword_fmt time_keyword_opt time_string
%type <str>  time_list 

/* Tokens and keywords used in LC_CTYPE category rules. */
%token <str> CHCLASS CHCLASS_KWD CHCONV_KWD 
%type <str>  locale_name char_symbol char_list charclass_list charconv_keyword

/* Tokens and keywords used in LC_COLLATE category rules. */
%token <str> COLL_SYM COLL_ELMT   
%token <str> ORDER_START ORDER_END FORWARD BACKWARD POSITION
%type  <collorder> opt_word
%type <str>  collation_element weight_list
%type <encs> weight_symbol elem_list symb_list

/* Tokens and keywords used in LC_MESSAGES category rules. */
%token <str> MESSAGES_KWD YESEXPR NOEXPR YESSTR NOSTR

/* Tokens and keywords used in LC_MONETARY category rules. */
%token <str> MONETARY_KWD_STR MONETARY_KWD_CHAR MONETARY_GRP
%type <str>  mon_group_list mon_string mon_keyword_string mon_keyword_char 
%type <str>  mon_keyword_grouping mon_group_number

/* Tokens and keywords used in LC_NUMERIC category rules. */
%token <str> NUM_KWD_STR 
%type <str>  numeric_keyword numeric_keywords
%type <str>  num_string num_keyword_string 
%type <str>  num_keyword_grouping num_group_list num_group_number

%start locale_definition

%%
locale_definition	: global_statements locale_categories
			|		    locale_categories
                        {
			    printf ( GETMSG ( MSGID_PARSING_OK ) );
                        }
			;

global_statements	: global_statements symbol_redefine
			|		    symbol_redefine
			;

symbol_redefine		: ESCAPE_CHAR CHAR EOL
                        {
  			  escape_char = $2[0];
			}
			| COMMENT_CHAR CHAR EOL
			;

locale_categories	: locale_categories locale_category
			| locale_category	
			;

locale_category		: lc_ctype | lc_collate | lc_messages 
			| lc_monetary | lc_numeric | lc_time
			;

/* 
 * The following grammar rules are common to all categories 
 */
 
char_list		: char_list char_symbol
                        {
			    /* All strings arriving here have been strdup'ed */
			    if ( ( $$ = realloc ( $1, strlen ( $1 ) + strlen ( $2 ) + 1 ) ) == NULL )
				exit_malloc_error();
			    strcat ( $$, $2 );
                        } 
			| char_symbol
                        {
			    $$ = $1;
                        } 
			;

char_symbol		: CHAR
                        | CHARSYMBOL
			| OCTAL_CHAR | HEX_CHAR | DECIMAL_CHAR
                        { 
                        }
			;

elem_list		: elem_list char_symbol
                        {
			    enc_t e = convert_string_to_enc ( $2, 0 );
			    if ( e != ENC_ILLEGAL )
				$$ = encs_append ( $1, e );
			}
			| elem_list COLLSYMBOL
                        {
			    enc_t e = convert_collsym_to_enc ( $2 );
			    if ( e != ENC_ILLEGAL )
				$$ = encs_append ( $1, e );
                        }
			| elem_list COLLELEMENT
                        { 
			    encs_t * e = encs_from_collelt ( 0, $2 );
			    if ( e )
				$$ = encs_append_encs ( $1, e );
                        }
			| char_symbol
                        { 
			    $$ = encs_from_string ( 0, $1 );
                        }
			| COLLSYMBOL
                        { 
			    $$ = encs_from_collsym ( 0, $1 );
                        }
			| COLLELEMENT
                        {
			    $$ = encs_from_collelt ( 0, $1 );
                        }
			;

symb_list		: symb_list COLLSYMBOL
			{
			    encs_t * e = encs_from_collsym ( 0, $2 );
			    if ( e )
				$$ = encs_append_encs ( $1, e );
			}
			| COLLSYMBOL 
                        {
			    $$ = encs_from_collsym ( 0, $1 );
                        }
			;

/* !!! note that LOC_NAME in COPY has been short ciruited */
locale_name		: char_list
			| '\"' char_list '\"'
			{
			    $$ = $2;
			}
			;

/* 
 * The following is the LC_CTYPE category grammar 
 */
 
lc_ctype		: ctype_hdr ctype_keywords ctype_tlr
                        {
			    has_content [ LC_CTYPE ] = 1;
                        }

			| ctype_hdr COPY locale_name EOL ctype_tlr
                        {
                        }
			;

ctype_hdr		: LC_CTYPE_TOK EOL
                        {
			    /* Because there has to be a LC_CTYPE file to allow
			     * setlocale() to succeed, an input to [w]chrtbl
			     * is always generated. It contains at least the
			     * character in each class required by POSIX locales
			     */
			    write_posix_ctype ( att_file[LC_CTYPE] );
			    count = 0;
                        }
			;

ctype_keywords		: ctype_keywords ctype_keyword
			| ctype_keyword
			;

ctype_keyword		: charclass_keyword charclass_list EOL
                        {
			    fprintf ( att_file[LC_CTYPE], "\n" );
                        }
			| charconv_keyword charconv_list EOL
                        {
			    fprintf ( att_file[LC_CTYPE], "\n" );
                        }
			| CHCLASS charclass_namelist EOL
                        /* XXX Not supported in ATT? */
			;

charclass_namelist 	: charclass_namelist SEMI_COLON CHARCLASS
                        {
                        }
			| CHARCLASS
                        {
                        }
			;

charclass_keyword	: CHCLASS_KWD
                        {
                          count = 0;
			  unsupported = 0;
                          clear_buf(buf);

                          if (strcmp($1, "upper") == 0)
                              {
			      sprintf(buf, "%s         ", "isupper");
			      current_kwd = KWD_UPPER;
			      }
                          else if (strcmp($1, "lower") == 0)
                              {
			      sprintf(buf, "%s         ", "islower");
			      current_kwd = KWD_LOWER;
			      }
                          else if (strcmp($1, "digit") == 0)
                              {
			      sprintf(buf, "%s         ", "isdigit");
			      current_kwd = KWD_DIGIT;
			      }
                          else if (strcmp($1, "punct") == 0)
                              {
			      sprintf(buf, "%s         ", "ispunct");
			      current_kwd = KWD_PUNCT;
			      }
                          else if (strcmp($1, "space") == 0)
                              {
			      sprintf(buf, "%s         ", "isspace");
			      current_kwd = KWD_SPACE;
			      }
                          else if (strcmp($1, "cntrl") == 0)
                              {
			      sprintf(buf, "%s         ", "iscntrl");
			      current_kwd = KWD_CNTRL;
			      }
                          else if (strcmp($1, "blank") == 0)
                              {
			      sprintf(buf, "%s         ", "isblank");
			      current_kwd = KWD_BLANK;
			      }
                          else if (strcmp($1, "xdigit") == 0)
                              {
			      sprintf(buf, "%s         ", "isxdigit");
			      current_kwd = KWD_XDIGIT;
			      }
                          else if (strcmp($1, "isideogram") == 0)
                              {
			      current_kwd = KWD_IDEOGRAM;
			      /* increment att support counter */
			      att_extensions++;
			      }
                          else if (strcmp($1, "isphonogram") == 0)
                              {
			      current_kwd = KWD_PHONOGRAM;
			      /* increment att support counter */
			      att_extensions++;
			      }
                          else if (strcmp($1, "isenglish") == 0)
                              {
			      current_kwd = KWD_ENGLISH;
			      /* increment att support counter */
			      att_extensions++;
			      }
                          else if (strcmp($1, "isspecial") == 0)
                              {
			      current_kwd = KWD_SPECIAL;
			      /* increment att support counter */
			      att_extensions++;
			      }
			   else if (strcmp($1, "alpha") == 0)
			      {
			      sprintf(buf, "%s         ", "isalpha");
                              current_kwd = KWD_ALPHA;
                              }
			   else if (strcmp($1, "graph") == 0)
			      {
			      sprintf(buf, "%s         ", "isgraph");
                              current_kwd = KWD_GRAPH;
                              }
			   else if (strcmp($1, "print") == 0)
			      {
			      sprintf(buf, "%s         ", "isprint");
                              current_kwd = KWD_PRINT;
                              }
			   else
			   {
			       fprintf ( stderr, "Internal error: no such character class\n" );
			       unsupported = 1;
			   }
                          if (!unsupported)
                              fputs((const char *)buf, att_file[LC_CTYPE]);
                        }
/*			| CHARCLASS
                        {
                        } */
			;

charclass_list		: charclass_list SEMI_COLON char_symbol
                        {

			  copy_list_elt($3, CONTINUE);
                        }
			| charclass_list SEMI_COLON ELLIPSIS SEMI_COLON char_symbol
                        {

			  copy_list_elt($5, ELLIPSOID);
                        }
			| char_symbol
                        {
			  
			  copy_list_elt($1, SINGLETON);
			}
			;

charconv_keyword	: CHCONV_KWD
                        {
                          if (strcmp($1, "toupper") == 0)
                              {
			      toupper_kwd = 1;
			      current_kwd = KWD_TOUPPER;
			      }
                          count = 0;
                          clear_buf(buf);
                          sprintf(buf, "%s           ", "ul");
                          fputs((const char *)buf, att_file[LC_CTYPE]);
                        }
			;

charconv_list		: charconv_list SEMI_COLON charconv_entry

                        {
			    ul_convert();
                        }
			| charconv_entry
                        {
			    ul_convert();
			}
			;

charconv_entry		: OPEN_PAREN char_symbol COMMA char_symbol CLOSE_PAREN
                        {
                          strncpy(first_entry, $2, BUFLEN);
                          strncpy(second_entry, $4, BUFLEN);
                        }
			;

ctype_tlr		: END LC_CTYPE_TOK EOL
                        {
			    if (alt_keywords)
			     {
			     /* print out cswidth string */
			     if (ctype1+ctype2+ctype3)
  			       {
			       /* at least one flag is true */
			       if (!ctype1)
			         ctype1 = 1;
			       if (!ctype2) 
			         ctype2 = 1;
			       if (!ctype3)
			         ctype3 = 1;
				 
			       clear_buf(buf);
			       sprintf(buf, "cswidth     %d:%d,%d:%d,%d:%d\n",
			          ctype1, ctype1, ctype2, ctype2, ctype3, ctype3);
			       fputs((const char *)buf, att_file[LC_CTYPE]);
			       }
			       
			      print_lctype(lctype1_list, "LC_CTYPE1");
			      print_lctype(lctype2_list, "LC_CTYPE2");
			      print_lctype(lctype3_list, "LC_CTYPE3");
			       
			  }
			  
                        }
			;

/* 
 * The following is the LC_COLLATE category grammar 
 */
lc_collate		: collate_hdr collate_keywords     collate_tlr
			| collate_hdr COPY locale_name EOL collate_tlr
                        {
                        }
			;

collate_hdr		: LC_COLLATE_TOK EOL
                        { 
                        }
			;

collate_keywords	:                order_statements 
                        { 
                        }
			| opt_statements order_statements
                        { 
                        }
			;

opt_statements		: opt_statements collating_symbols
                        { 
                        }
			| opt_statements collating_elements
                        { 
                        }
			| collating_symbols
                        { 
                        }
			| collating_elements
                        { 
                        }
			;
			
collating_symbols	: COLL_SYM SYMBOLIC_NAME EOL
                        {
			    add_collsym ( $2 );
			}
			;
			
collating_elements	: COLL_ELMT SYMBOLIC_NAME
                          FROM '\"' elem_list '\"' EOL
                        { 
			    add_collelt ( $2, $5 );
			}
			;
			
order_statements	: order_start collation_order order_end
                        { 
			}
			;

order_start		: ORDER_START EOL
                        {
			    collorder[0] = e_forward;
			    collorder_len = 1;
			}
			| ORDER_START order_opts EOL
                        {
			    collorder_len = collorder_count;
			}
			;

order_opts		: order_opts SEMI_COLON order_opt
                        {
			    collorder_count ++;
			   
			    if ( collorder_count > COLL_WEIGHTS_MAX )
				fprintf ( stderr, GETMSG ( MSGID_MAX_WEIGHTS ),
					  COLL_WEIGHTS_MAX );
			}
			| order_opt
                        {
			    collorder_count ++;
			}
			;

order_opt		: order_opt COMMA opt_word
                        {
			    collorder [ collorder_count ] |= $3;
			}
			| opt_word
                        { 
			    collorder [ collorder_count ] = $1;
			}
			;

opt_word		: FORWARD
                        {
			    $$ = e_forward;
			}
			| BACKWARD
			{
			    $$ = e_backward;
			}
			| POSITION
			{
			    $$ = e_position;
			}
			;

collation_order		: collation_order collation_entry
                        { 
			}
			| collation_entry
                        { 
			}
			;

collation_entry		: COLLSYMBOL EOL
                        {
			    coll_add_collsym ( $1 );
			}
			| collation_element weight_list EOL
			{
			    coll_add_collid ( $1, weights, weights_counter );
			    /* Reinitialize weight table */
			    weights_counter = 0;
			}

			| collation_element		EOL
                        { 
			    coll_add_collid ( $1, NULL, 0 );
			    weights_counter = 0;
			}
			;

collation_element	: char_symbol
                        { 
			}
			| COLLELEMENT
                        { 
			}
			| ELLIPSIS
                        {
			}
			| UNDEFINED
                        {
			}
			;

weight_list		: weight_list SEMI_COLON weight_symbol
                        {
			    if ( weights_counter >= COLL_WEIGHTS_MAX )
				fprintf ( stderr, GETMSG ( MSGID_TOO_MANY_WEIGHTS ),
					  yylineno );

			    else if ( weights_counter > collorder_len )
				fprintf ( stderr, GETMSG ( MSGID_TOO_MANY_WEIGHTS ), yylineno );

			    else
				weights [ weights_counter ++ ] = $3;
			}
			| weight_list SEMI_COLON
                        {
			}
			| weight_symbol
                        {
			    weights [ weights_counter ++ ] = $1;
			}

			;

weight_symbol		: /* empty list */
                        {
			    $$ = 0;
			}
			| char_symbol
                        {
			    $$ = encs_from_string ( 0, $1 );
			}
			| COLLSYMBOL 
                        { 
			    $$ = encs_from_collsym ( 0, $1 );
			}
			| COLLELEMENT
                        {
			    /* Contrary to the syntax definition in
			       the XSI document, it appears from the
			       example that the at least this line is
			       required */
			    $$ = encs_from_collelt ( 0, $1 );
			}
			| '\"' elem_list '\"'
			{
			    $$ = $2;
			    encs_set_string ( $$, 1 );
			}
  			| '\"' symb_list '\"'
                        {
			    $$ = $2;
			    encs_set_string ( $$, 1 );
			}
			| ELLIPSIS
                        {
			    $$ = encs_append ( 0, ENC_IGNORE );
			}
			| IGNORE
                        {
			    $$ = encs_append ( 0, ENC_IGNORE );
			}
			;

order_end		: ORDER_END EOL
                        { 
			}
			;

collate_tlr		: END LC_COLLATE_TOK EOL
                        {
			    has_content [ LC_COLLATE ] = cotable_build();
			    
			    if ( has_content [ LC_COLLATE ] )
				cotable_output ( att_file[LC_COLLATE] );
			}
			;

/*
 * The following is the LC_MESSAGES category grammar
 */
lc_messages		: messages_hdr messages_keywords      messages_tlr
			| messages_hdr COPY locale_name EOL messages_tlr
                        {
                        }
			;
			
messages_hdr		: LC_MESSAGES_TOK EOL
                        {
			
			/* initialize the buffers for nl_lang_* */
			
			   nllang_buf[YESSTR_POS][0] = '\n';
			   nllang_buf[NOSTR_POS][0] = '\n';
			   nllang_buf[YESEXP_POS][0] = '\n';
			   nllang_buf[NOEXP_POS][0] = '\n';
			   
                        }
			;
			
messages_keywords	: messages_keywords messages_keyword
			| messages_keyword
			;

/* !!! changed EXTEND_REG_EXP to char_list for time being */

messages_keyword	: YESEXPR '\"' char_list '\"' EOL 
                        { 

                          /* YESEXPR goes on line #6, buf position 5 */
			  clear_buf(nllang_buf[YESEXP_POS]);
			  sprintf(nllang_buf[YESEXP_POS], "%s\n", 
			           convert_charsyms($3));
			  
			  /* Extract default YESSTR in case none is entered */
			  if (nllang_buf[YESSTR_POS][0] == NL)
			     {
			     nllang_buf[YESSTR_POS][1] = NL;
			     nllang_buf[YESSTR_POS][0] =
			           extract_default_msg(nllang_buf[YESEXP_POS]);
			     }
			}
			| NOEXPR '\"' char_list '\"' EOL 
                        { 
			  
                          /* NOEXPR goes on line #7, buf position 6 */
			  clear_buf(nllang_buf[NOEXP_POS]);
			  sprintf(nllang_buf[NOEXP_POS], "%s\n", 
			            convert_charsyms($3));

			  /* Extract default NOSTR in case none is entered */
			  if (nllang_buf[NOSTR_POS][0] == NL)
			     {
			     nllang_buf[NOSTR_POS][1] = NL;
			     nllang_buf[NOSTR_POS][0] =
			           extract_default_msg(nllang_buf[NOEXP_POS]);
			     }
			}
			| YESSTR '\"' char_list '\"' EOL 
                        { 

                          /* YESSTR goes on line #4, buf position 3 */
			  clear_buf(nllang_buf[YESSTR_POS]);
			  sprintf(nllang_buf[YESSTR_POS], "%s\n", $3);
			}
			| NOSTR '\"' char_list '\"' EOL
                        {
			  
                          /* NOSTR goes on line #5, buf position 4 */
			  clear_buf(nllang_buf[NOSTR_POS]);
			  sprintf(nllang_buf[NOSTR_POS], "%s\n", $3);
			}
			;
			
messages_tlr		: END LC_MESSAGES_TOK EOL
                        {
			/* since we don't know in which order the different
			 * sections will be parsed, save buffer off both
			 * here and at the end of LC_TIME 
			 */
			 
			if (toggle)
			   /* LC_TIME has filled the other data lines */
			   /* which implies that it's okay to write the file */
			   {
			   
   			   /* !!! remember in the absence of YESSTR/NOSTR, strip */
			   /* out characters from YESEXP/NOEXP */

                           for (i = 0 ; i < NL_LANG_SIZE; i++)
			       fputs((const char *)nllang_buf[i], att_file[LC_MESSAGES]);
			   }
			else
			   {
			   nllang_buf[TIME_POS][0] = '\n';
			   nllang_buf[DATE_POS][0] = '\n';
			   nllang_buf[T_D_POS][0] = '\n';
			   nllang_buf[FMT_AMPM_POS][0] = '\n';
			   
//			   /* !!! just do for now, remove later */
//                           for (i = 0 ; i < NL_LANG_SIZE; i++)
//			       fputs((const char *)nllang_buf[i], att_file[LC_MESSAGES]);

			   toggle = 1; /* we've put our stuff in */
			   }
			has_content [ LC_MESSAGES ] = 1;

			}
			;

/* 
 * The following is the LC_MONETARY category grammar 
 */
lc_monetary		: monetary_hdr monetary_keywords monetary_tlr
			| monetary_hdr COPY locale_name EOL monetary_tlr
                        {
                        }
			;

monetary_hdr		: LC_MONETARY_TOK EOL
                        { 
			  has_content [ LC_MONETARY ] = 1;
                          clear_buf(buf);
                          sprintf(buf,"# %s\n", "LC_MONETARY");
                          fputs((const char *)buf, att_file[LC_MONETARY]);
                        }
			;

monetary_keywords	: monetary_keywords monetary_keyword
                        {
                        }
			| monetary_keyword
                        {
                        }
			;

monetary_keyword	: mon_keyword_string mon_string EOL
                        {
			  clear_buf(buf);
			  sprintf(buf,"%s\t\"%s\"\n", $1, ( $2 ? $2 : "" ) );
                          fputs((const char *)buf, att_file[LC_MONETARY]);
                        }

			| mon_keyword_char NUMBER EOL
                        { 
                          clear_buf(buf);
			  
			  /* Some limits on AT&T values are different
			   * than XPG4.  We do a few checks here.
			   */
			   
			  if ((strncmp($1, "p_sep_by_space", strlen("p_sep_by_space")) == 0) ||
 			      (strncmp($1, "n_sep_by_space", strlen("n_sep_by_space")) == 0))
			     if ($2 > 1)
			        {
				    $2 = 1;
				    fprintf ( stderr, GETMSG ( MSGID_MONTBL_LIMIT ),
					      $1 );
				}
			  
                          sprintf(buf,"%s\t%d\n", $1, $2);
                          fputs((const char *)buf, att_file[LC_MONETARY]);
                        }

			| mon_keyword_char NEG_ONE EOL
                        { 
                        }

			| mon_keyword_grouping mon_group_list EOL
                        {
			    fputs( "\"\n", att_file[LC_MONETARY] ); /* End of line */

                        }
			;

mon_keyword_string	: MONETARY_KWD_STR
                        { 
                        }
			;

mon_string		: '\"' char_list '\"'
                        { 
			  $$ = convert_charsyms($2);
                        }
			| EMPTY_STRING
                        { 
			  
			  $$ = NULL;
                        }
			;

mon_keyword_char	: MONETARY_KWD_CHAR 
                        { 
                        }
			;

mon_keyword_grouping	: MONETARY_GRP 
                        { 
			    clear_buf ( buf );
			    sprintf(buf, "%s\t\"", $1);
			    fputs((const char *)buf, att_file[LC_MONETARY] );

                        }
			;

mon_group_list		: mon_group_number
                        { 
                        }
			| mon_group_list SEMI_COLON mon_group_number
                        { 
                        }
			;

mon_group_number:	NUMBER
			{
			    clear_buf(buf);
			    if ( $1 == -1 )
				sprintf ( buf, "\\%o", CHAR_MAX );
			    else
				sprintf ( buf ,"\\%o", $1 ) ;
			    fputs ( buf, att_file[LC_MONETARY] );
			}				
			;

monetary_tlr		: END LC_MONETARY_TOK EOL
                        { 
                        }
			;	

/* 
 * The following is the LC_NUMERIC category grammar 
 */
lc_numeric		: numeric_hdr numeric_keywords       numeric_tlr
			{
			    has_content [ LC_NUMERIC ] = 1;
			}
			| numeric_hdr COPY locale_name EOL numeric_tlr
                        {
                        }
			;

numeric_hdr		: LC_NUMERIC_TOK EOL
                        { 
                          count = 0;
			  fprintf ( att_file[LC_NUMERIC], "LC_NUMERIC\tLC_NUMERIC\n");
                        }
			;

numeric_keywords	: numeric_keywords numeric_keyword
			| numeric_keyword 
			;

numeric_keyword		: num_keyword_string num_string EOL
                        {
			    fprintf ( att_file[LC_NUMERIC], "%s   %s\n", $1,
				      convert_charsyms($2));
                        }

			| num_keyword_grouping num_group_list EOL
                        {
			    fprintf ( att_file[LC_NUMERIC] , "\"\n");
                        }
			;

num_keyword_string	: NUM_KWD_STR 
                        { 
                        }
                        ;

num_string		: '\"' char_list '\"'
                        {
			    $$ = $2;
                        }
                        | EMPTY_STRING 
                        { 
                        }
			;

num_keyword_grouping	: GROUPING
                        {
			    clear_buf(buf);
			    sprintf(buf, "%s\t\"", $1);
			    fputs((const char *)buf, att_file[LC_NUMERIC] );
                        }
			;

num_group_list		: num_group_number
                        { 
                        }
			| num_group_list SEMI_COLON num_group_number
                        { 
                        }
			;

num_group_number:	NUMBER
			{
			    clear_buf(buf);
			    if ( $1 == -1 )
				sprintf ( buf, "\\%o", CHAR_MAX );
			    else
				sprintf ( buf ,"\\%o", $1 ) ;
			    fputs ( buf, att_file[LC_NUMERIC] );
			}				
			;

numeric_tlr		: END LC_NUMERIC_TOK EOL
                        {
                        }
                        ;

/* 
 * The following is the LC_TIME category grammar  
 */
lc_time			: time_hdr time_keywords          time_tlr
			| time_hdr COPY locale_name EOL time_tlr
                        {
                        }
                        ;

time_hdr		: LC_TIME_TOK EOL
                        { 
			  
			/* initialize the buffers for nl_lang_* */
			
			   nllang_buf[TIME_POS][0] = '\n';
			   nllang_buf[DATE_POS][0] = '\n';
			   nllang_buf[T_D_POS][0] = '\n';
			   nllang_buf[FMT_AMPM_POS][0] = '\n';
			   
			/* initialize the buffers for LC_TIME */
			
			for (i = 0; i < LC_TIME_SIZE; i++)
			   lctime_buf[i][0] = '\n';
			
                        }
			;

time_keywords		: time_keywords time_keyword
			| time_keyword
			;

time_keyword		: time_keyword_name time_list EOL
                        {
			  time_key_print($1);
			}
			| time_keyword_fmt time_string EOL 
                        {
			  time_fmt_print($1, $2);
			}
			| time_keyword_opt time_list EOL
                        {
			  if ((lctime_opt[lctime_opt_ctr] =
			        malloc(strlen($2))) == NULL)
			      exit_malloc_error ();
                          else
			     {
			     if (lctime_opt_ctr > LC_TIME_OPT_SIZE)
			        fprintf ( stderr, GETMSG ( MSGID_TOO_MANY_TIME ),
					  yylineno );
			     else
				 sprintf(lctime_opt[lctime_opt_ctr], "%s\n", $2);
			     lctime_opt_ctr++;
			     }
			}
			;

time_keyword_name	: TIME_KWD_NAME
			;

time_keyword_fmt	: TIME_KWD_FMT
			;

time_keyword_opt	: TIME_KWD_OPT
			;

time_list		: time_list SEMI_COLON time_string 
                        { 
//    		          sprintf(lctime_temp[indxx++], "%s\n", $1);
			}
			| time_string
			{
//    		          sprintf(lctime_temp[indxx++], "%s\n", $1);
			}
			;

time_string		: '\"' char_list '\"'
                        {
/*	CONVERT!!! */	  sprintf(lctime_temp[indxx++], "%s\n",
                                  convert_charsyms($2));
			  $$ = conv_buf;
			}
			| EMPTY_STRING
                        { 
			 
			  sprintf(lctime_temp[indxx++], "\n");
			  $$ = "\n";
                        }
			;

time_tlr		: END LC_TIME_TOK EOL
                        {
			/* Print out LC_TIME info */
                        for (i = 0 ; i < LC_TIME_SIZE; i++)
			   fputs((const char *)lctime_buf[i], att_file[LC_TIME]);

			/* Print out any time_key_opt formats at the end */
                        if (lctime_opt_ctr > 0)
                           for (i = 0 ; i < lctime_opt_ctr; i++)
   			      fputs((const char *)lctime_opt[i], att_file[LC_TIME]);
			
			/* 
			 * Some LC_TIME info also must saved to nl_lang_*.
			 * Since we don't know in which order the different
			 * sections will be parsed, save buffer off both
			 * here and at the end of LC_MESSAGES
			 */
			 
			if (toggle)
			   /* LC_MESSAGES has filled the other data lines */
			   /* which implies that it's okay to write the file */
			   {
                           for (i = 0 ; i < NL_LANG_SIZE; i++)
			       fputs((const char *)nllang_buf[i], att_file[LC_MESSAGES]);
			   }
			else
			   {
			   nllang_buf[YESSTR_POS][0] = '\n';
			   nllang_buf[NOSTR_POS][0] = '\n';
			   nllang_buf[YESEXP_POS][0] = '\n';
			   nllang_buf[NOEXP_POS][0] = '\n';
			   
//			   /* !!! just do for now, remove later */
//                           for (i = 0 ; i < NL_LANG_SIZE; i++)
//			       fputs((const char *)nllang_buf[i], att_file[LC_MESSAGES]);

			   toggle = 1; /* we've put our stuff in */
			   }

			}
			;
%%

/* defines for which lines LC_TIME strings go on */

#define TIM_AMON1   0
#define TIM_AMON2   1
#define TIM_AMON3   2
#define TIM_AMON4   3
#define TIM_AMON5   4
#define TIM_AMON6   5
#define TIM_AMON7   6
#define TIM_AMON8   7
#define TIM_AMON9   8
#define TIM_AMON10  9
#define TIM_AMON11  10
#define TIM_AMON12  11
#define TIM_MON1    12
#define TIM_MON2    13
#define TIM_MON3    14
#define TIM_MON4    15
#define TIM_MON5    16
#define TIM_MON6    17
#define TIM_MON7    18
#define TIM_MON8    19
#define TIM_MON9    20
#define TIM_MON10   21
#define TIM_MON11   22
#define TIM_MON12   23
#define TIM_ADAY1   24
#define TIM_ADAY2   25
#define TIM_ADAY3   26
#define TIM_ADAY4   27
#define TIM_ADAY5   28
#define TIM_ADAY6   29
#define TIM_ADAY7   30
#define TIM_DAY1    31
#define TIM_DAY2    32
#define TIM_DAY3    33
#define TIM_DAY4    34
#define TIM_DAY5    35
#define TIM_DAY6    36
#define TIM_DAY7    37
#define TIM_TIME    38
#define TIM_DATE    39
#define TIM_T_D     40
#define TIM_AM      41
#define TIM_PM      42
#define TIM_FMT     43
#define TIM_FMT_AMPM 44

void time_key_print(char *fmt)
{
   int i;


   if (strcmp(fmt, "abday") == 0)
      {
      for (i = TIM_ADAY1; i <= TIM_ADAY7; i++)
          {
	  strcpy(lctime_buf[i], lctime_temp[i - TIM_ADAY1]);
	  clear_buf(lctime_temp[i - TIM_ADAY1]);
	  }
      }
   else if (strcmp(fmt, "day") == 0)
      {
      for (i = TIM_DAY1; i <= TIM_DAY7; i++)
          {
	  strcpy(lctime_buf[i], lctime_temp[i - TIM_DAY1]);
	  clear_buf(lctime_temp[i - TIM_DAY1]);
	  }
      }
   else if (strcmp(fmt, "abmon") == 0)
      {
      for (i = TIM_AMON1; i <= TIM_AMON12; i++)
          {
	  strcpy(lctime_buf[i], lctime_temp[i - TIM_AMON1]);
	  clear_buf(lctime_temp[i - TIM_AMON1]);
	  }
      }
   else if (strcmp(fmt, "mon") == 0)
      {
      for (i = TIM_MON1; i <= TIM_MON12; i++)
          {
	  strcpy(lctime_buf[i], lctime_temp[i - TIM_MON1]);
	  clear_buf(lctime_temp[i - TIM_MON1]);
	  }
      }
   else if (strcmp(fmt, "am_pm") == 0)
      {
      strcpy(lctime_buf[TIM_PM], lctime_temp[1]);
      strcpy(lctime_buf[TIM_AM], lctime_temp[0]);


   
      clear_buf(lctime_temp[0]);
      clear_buf(lctime_temp[1]);
      }

   indxx = 0;
   
   return;
}

void time_fmt_print(char *fmt, char *str)
{
   if (strcmp(fmt, "d_t_fmt") == 0)
      {
      clear_buf(nllang_buf[T_D_POS]);
      clear_buf(lctime_buf[TIM_T_D]);
      sprintf(nllang_buf[T_D_POS], "%s\n", str);
      sprintf(lctime_buf[TIM_T_D], "%s\n", str);
      
      // if new att_d_t_fmt not defined, use this string for t_fmt_ampm also
      clear_buf(lctime_buf[TIM_FMT]);
      sprintf(lctime_buf[TIM_FMT], "%s\n", str);
      }
   else if (strcmp(fmt, "t_fmt") == 0)
      {
      clear_buf(nllang_buf[TIME_POS]);
      clear_buf(lctime_buf[TIM_TIME]);
      sprintf(nllang_buf[TIME_POS], "%s\n", str);
      sprintf(lctime_buf[TIM_TIME], "%s\n", str);
      }
   else if (strcmp(fmt, "d_fmt") == 0)
      {
      clear_buf(nllang_buf[DATE_POS]);
      clear_buf(lctime_buf[TIM_DATE]);
      sprintf(nllang_buf[DATE_POS], "%s\n", str);
      sprintf(lctime_buf[TIM_DATE], "%s\n", str);
      }
   else if (strcmp(fmt, "am_pm") == 0) /* !!! placeholder for now */
      {
      clear_buf(lctime_buf[TIM_AM]);
      clear_buf(lctime_buf[TIM_PM]);
      sprintf(lctime_buf[TIM_AM], "AM\n");
      sprintf(lctime_buf[TIM_PM], "PM\n");
      }
   else if (strcmp(fmt, "att_d_t_fmt") == 0)
      {
      clear_buf(lctime_buf[TIM_FMT]);
      sprintf(lctime_buf[TIM_FMT], "%s\n", str);
      }
   else if (strcmp(fmt, "t_fmt_ampm") == 0)
      {
      clear_buf(nllang_buf[FMT_AMPM_POS]);
      clear_buf(lctime_buf[TIM_FMT_AMPM]);
      sprintf(nllang_buf[FMT_AMPM_POS], "%s\n", str);
      sprintf(lctime_buf[TIM_FMT_AMPM], "%s\n", str);
      }
   
   indxx = 0;
   return;
}

// map2hex:  converts encoding of charmap values
//   to a string of hexadecimal digits
//   Blatant assumption:  encodings are short, and fit within buffer
//
char *map2hex(char *buffer, char *encoding, int ctype)
{
   int num1, num2, num3;
   char *d1, *d2, *cur = encoding;

   /*
    * There are three possibilities for the elements
    * of the encoding string:
    *  \d<decimal digits>
    *  \x<hexadecimal digits>
    *  \<octal digits>
    */
    
    /* Convert charmap value to a CHAR if possible
     * ASSUMPTIONS:  one, two, or three byte encodings ONLY;
     *  the length is flagged by the input ctype
     * Format is <escape><base><digits> */

    if (ctype == 1)
       {
       /* length equals number of '\' in string */
       ctype = 0;
       while ((cur = strchr(cur, (int) BACK_SLASH)) != NULL)
         {
	 ctype++;
	 cur = cur+1;
	 }
	 
       /* reset cur to the beginning of the encoding string */
       cur = encoding;
       }
    else if (ctype == 2)
       ctype = ctype2;
    else
       ctype = ctype3;
       
    if (ctype == 3)
       {
       /* three byte encoding */
       if (cur[0] == BACK_SLASH && cur[1] == 'd')
          {
	  /* decimal: convert digits to hex */
          num1 = (int) strtol(encoding+2, &d1, 10);
	  num1 <<= 16;
	  num2 = (int) strtol(d1+2, &d2, 10);
	  num2 <<= 8;
	  num3 = (int) strtol(d2+2, NULL, 10);
	  }
       else if (cur[0] == BACK_SLASH && cur[1] == 'x')
          {
	  /* hexadecimal */
	  num1 = (int) strtol(encoding+2, &d1, 16);
	  num1 <<= 16;
          num2 = (int) strtol(d1+2, &d2, 16);
	  num2 <<= 8;
	  num3 = (int) strtol(d2+2, NULL, 16);
	  }
       else
          {
          /* octal byte value: drop escape chars */
          num1 = (int) strtol(encoding+1, &d1, 8);
	  num1 <<= 16;
          num2 = (int) strtol(d1+1, &d2, 8);
	  num2 <<= 8;
	  num3 = (int) strtol(d2+2, NULL, 8);
	  }
       sprintf(buffer, "0x%x", num1 + num2 + num3);
       }
    else if (ctype == 2)
       {
       if (cur[0] == BACK_SLASH && cur[1] == 'd')
          {
	  /* decimal: convert digits to hex */
          num1 = (int) strtol(encoding+2, &d1, 10);
	  num1 <<= 8;
	  num2 = (int) strtol(d1+2, NULL, 10);
	  }
       else if (cur[0] == BACK_SLASH && cur[1] == 'x')
          {
	  /* hexadecimal */
	  num1 = (int) strtol(encoding+2, &d1, 16);
	  num1 <<= 8;
          num2 = (int) strtol(d1+2, NULL, 16);
	  }
       else
          {
          /* octal byte value: drop escape chars */
          num1 = (int) strtol(encoding+1, &d1, 8);
	  num1 <<= 8;
          num2 = (int) strtol(d1+1, NULL, 8);
	  }
       sprintf(buffer, "0x%x", num1 + num2);
       }
    else
       {
       if (cur[0] == BACK_SLASH && cur[1] == 'd')
          /* decimal: convert digits to hex */
          sprintf(buffer, "0x%x", atoi(encoding+2));
       else
          /* octal byte value: drop escape and add leading '0' */
	  /* hex is same */
          sprintf(buffer, "0%s", encoding+1);
       }
       
   return(buffer);
}

/* note that this function depends on it's arguemtn ending in newline */

char extract_default_msg(char *expr)
{
   char *ch = expr;
   
  
   while (*ch != NL)
      if (isalpha(*ch))
         return(*ch);
      else
         ch++;
	 
  return(SPACE);
}

/* 
 * CONVERT_CHARSYMS - converts CHARSYMBOLS embedded in strings to their
 *  associated encodings
 */
 
char *convert_charsyms(char *list)
{
   int i, j;
   long conv_num, conv_num1, conv_num2;
   symbol_ptr p = NULL;
   char chr[2], *cur, *ptr = NULL, *temp;


   clear_buf(conv_buf);
   
   conv_num = conv_num1 = conv_num2 = 0L;

   /* if the string contains L_ANGLE, chances are that
    * it's a CHARSYMBOL.  Convert the whole string, watching
    * for embedded continuation characters.
    */
   
   for (j = 0, cur = list; *cur != NULL_CHAR; j++, cur++)
      {
      // DEBUG!
      
      /* better check that conv_buf not overflowing! */
      if (j == sizeof(conv_buf))
         {
         fprintf(stderr, GETMSG ( MSGID_BUFLEN ) );
	 conv_buf[j-1] = NULL_CHAR;
	 break;
	 }
	 
      /* if a continuation character is embedded between
       * character symbols, ignore everything to the end of
       * line character
       */
      if (*cur == escape_char)
         {
	 /* guarantees that *cur == NL when leaving this block */
	 while (*cur != NL)
	    cur++;
	 /* correct for autoincrementing j without storing in buffer */
	 j--;
         }
      else if (*cur == L_ANGLE)
         {
	 clear_buf(ubuf);
	 chr[1] = NULL_CHAR;
      
         /* copy to the end of the symbol */
         for (i = 0; *cur != R_ANGLE; i++, cur++)
            ubuf[i] = *cur;

         ubuf[i] = R_ANGLE;
	 ubuf[i+1] = NULL_CHAR;

 	 
	 if ((p = search_list(charmap_list, ubuf)) != NULL)
            {
	    ptr = map2hex(ubuf, p->mapping, p->ctype);

	    /* Convert charmap value to a CHAR if possible
	     * ASSUMPTIONS:  number of bytes in encodings given
	     * in p->ctype.  Format is <escape><base><digits> 
	     */


  /* NOTE: the code in this switch that manipulates the 
   *  contents of the long integers conv_num and conv_num2 is specific
   *  to the integer format on the SGI Indigo MIPS processors.
   *  This code is NOT particularly portable
   */
	 switch (p->ctype)
	    {
            case LC_BYTE:
	       {
	       conv_num = strtol(ptr+2, NULL, 16);
  	       conv_buf[j] = (char) conv_num;
	       break;
	       }
	    case LC_CTYPE1:
	       {
	       /* double byte not LC_CTYPE2 */
	       conv_num = strtol(ptr+2, NULL, 16);
	       conv_num1 = conv_num >> 8;
	       conv_buf[j] = (char) (conv_num1 & 0x000000FF);
	       j++;
	       conv_buf[j] = (char) (conv_num & 0x000000FF);
	       break;
	       }
	    case LC_CTYPE2:
	       {
	       /* LC_CTYPE2, one or three bytes */
	       conv_buf[j] = LC2_HEADER;
	       j++;
	       if (ctype2 == 3)
	          {
		  conv_num = strtol(ptr+2, &temp, 16);
		  conv_num1 = conv_num >> 8;
		  conv_num2 = conv_num >> 16;
		  conv_buf[j] = (char) (conv_num2 & 0x000000FF);
		  j++;
		  conv_buf[j] = (char) (conv_num1 & 0x000000FF);
		  j++;	       
		  conv_buf[j] = (char) (conv_num  & 0x000000FF);
		  }
	       else
	          {
		  conv_num = strtol(ptr+2, NULL, 16);
		  conv_buf[j] = (char) conv_num;
		  }
	       break;
	       }
	    case LC_CTYPE3:
	       {
	       /* LC_CTYPE3, one or three bytes */
	       conv_buf[j] = LC3_HEADER;
	       j++;
	       if (ctype2 == 3)
	          {
		  conv_num = strtol(ptr+2, &temp, 16);
		  conv_num1 = conv_num >> 8;
		  conv_num2 = conv_num >> 16;
		  conv_buf[j] = (char) (conv_num2 & 0x000000FF);
		  j++;
		  conv_buf[j] = (char) (conv_num1 & 0x000000FF);
		  j++;	       
		  conv_buf[j] = (char) (conv_num  & 0x000000FF);
		  }
	       else
	          {
		  conv_num = strtol(ptr+2, NULL, 16);
		  conv_buf[j] = (char) conv_num;
		  }
	       break;
	       }
	    }
  	    }
	 else
            fprintf(stderr, GETMSG ( MSGID_NOCODE ), ubuf);
	 }
      else
	 conv_buf[j] = *cur;
      }
   
//      strcpy(conv_buf, list);
      
   return(conv_buf);
}

void handle_ideogram(char *symbol, int output_type)
{
   symbol_ptr p = NULL;
   char *ptr = NULL;
   symbol_ptr cm;

   /* If this is a language-dependent symbol for one of 
    * LC_CTYPE1, LC_CTYPE2, or LC_CTYPE3, it will have a
    * multibyte encoding...
    */
    
   
   clear_buf(buf);
   clear_buf(ubuf);
   count++;
   
   strncpy(ubuf, symbol, BUFLEN);
   
   if (ubuf[0] != '<')
      return;
   else 
      {
      if (p = search_list(charmap_list, symbol))
         {
	 /* convert string value to hex digits */
	 clear_buf(ubuf);
	 ptr = map2hex(ubuf, p->mapping, p->ctype);
	 

	 /* When a multibyte character encoding is returned 
	  * this needs to be defered to later output.
	  */
	  
	 if (p->ctype == 0)
	    {
	    /* not a multibyte, we don't handle... */
	       return;
	    }
	 else
	    {
		/* 
		fprintf ( stderr, "Warning: POSIX LC_CTYPE section does not support"
			  "all AT&T categories.\nCoercing symbol %s to be of type"
			  "\"isideogram\".\n\n", symbol );
			  */

	    /* search the existing chain first, to ensure that
	     * one keyword does not duplicate another's work;
	     * i.e., if found, bail out...
	     */
	    if ((p->ctype == 1) &&
   	        (search_list( lctype1_list[current_kwd], ptr) != NULL))
		return;
            else if ((p->ctype == 2) && 
		     (search_list( lctype2_list[current_kwd], ptr) != NULL))
		return;
            else if ((p->ctype == 3) && 
		     (search_list( lctype3_list[current_kwd], ptr) != NULL))
		return; 
	       
            if ((cm = malloc(sizeof(struct symbol_node))) == NULL)
		exit_malloc_error ();
	       
	   if ((cm->mapping = malloc(strlen(ptr))) == NULL)
	       exit_malloc_error ();
	   else
	       strcpy(cm->mapping, ptr);

 	   if ((cm->symbol = malloc(5)) == NULL)
	       exit_malloc_error ();
	   else
	       sprintf(cm->symbol, "%d", output_type);

            if (p->ctype == 1)
   	       insert_list( & (lctype1_list[current_kwd]), cm);
            else if (p->ctype == 2)
   	       insert_list( & (lctype2_list[current_kwd]), cm);
            else
   	       insert_list( & (lctype3_list[current_kwd]), cm);
	       
	    alt_keywords = 1;
	    }
	 }
      else
         {
	 fprintf(stderr,  GETMSG ( MSGID_UNKNOWN_SYM ), symbol );
	 if (output_type == ELLIPSOID);
  	    {
	    sprintf(buf, " - xxx");
            fputs((const char *)buf, att_file[LC_CTYPE]);
	    }
	 }
      }
   
   
   return;
}


void copy_list_elt(char *symbol, int output_type)
{
   symbol_ptr p = NULL;
   char *ptr = NULL;
   symbol_ptr cm;

   /* the lists following POSIX "graph" and "print" keywords
    * are not normally used since these are not supported
    * in AT&T formats.  However, we may need to pull some
    * language-dependent characters out of these for LC_CTYPE1, 
    * LC_CTYPE2, or LC_CTYPE3
    */
    
   if (unsupported)
      {
      if (!att_extensions)
         handle_ideogram(symbol, output_type);
      return;
      }

   
   clear_buf(buf);
   clear_buf(ubuf);
   count++;
   
   strncpy(ubuf, symbol, BUFLEN);
   
   if (strlen(ubuf) == 1)
      {
      if (output_type == CONTINUE)
         {
         if (count%6 == 1)
            sprintf(buf, "   \\\n             0x%x", ubuf[0]); 
         else
            sprintf(buf, "   0x%x", ubuf[0]);
	 }
      else if (output_type == ELLIPSOID)
         sprintf(buf, " - 0x%x", ubuf[0]);
      else if (output_type == SINGLETON)
         sprintf(buf, "0x%x", ubuf[0]);
      }
   else if (ubuf[0] == '\\')
      {
      ptr = map2hex(ubuf, symbol, strlen(ubuf)/4);
      if (output_type == CONTINUE)
         {
	 if (count%6 == 1)
            sprintf(buf, "   \\\n             %s", ubuf);
         else
            sprintf(buf, "   %s", ubuf);
	 }
      else
         strncpy(buf, ptr, BUFLEN);
      }
   else if (ubuf[0] == '<')
      {
      p = search_list(charmap_list, symbol);
      if (p)
         {
	 /* convert string value to hex digits */
	 clear_buf(ubuf);
	 ptr = map2hex(ubuf, p->mapping, p->ctype);
	 
	 /* In some locales, supplemental information may need 
	  * to be output to supplemental LC_TYPE1 sections of 
	  * the tables.  When a multibyte character encoding is
	  * returned, this needs to be defered to later output.
	  */
	  
	 if (p->ctype != 0)
	    {
            if ((cm = malloc(sizeof(struct symbol_node))) == NULL)
		exit_malloc_error ();
	       
	   if ((cm->mapping = malloc(strlen(ptr))) == NULL)
	       exit_malloc_error();
	   else
  	      strcpy(cm->mapping, ptr);

 	   if ((cm->symbol = malloc(5)) == NULL)
	       exit_malloc_error();
	   else
	      sprintf(cm->symbol, "%d", output_type);

            if (p->ctype == 1)
   	       insert_list( & (lctype1_list[current_kwd]), cm);
            else if (p->ctype == 2)
   	       insert_list( & (lctype2_list[current_kwd]), cm);
            else
   	       insert_list( & (lctype3_list[current_kwd]), cm);
	       
	    alt_keywords = 1;
	    }
	 else if (output_type == CONTINUE)
	    {
	    if (count%6 == 1)
               sprintf(buf, "   \\\n             %s", ptr); 
	    else
               sprintf(buf, "   %s", ptr);
	    }
	 else if (output_type == ELLIPSOID)
  	    sprintf(buf, " - %s", ptr);
	 else if (output_type == SINGLETON)
	    sprintf(buf, "%s", ptr);
	 }
      else
         {
	 fprintf(stderr,  GETMSG ( MSGID_UNKNOWN_SYM ), symbol );
	 if (output_type == ELLIPSOID);
  	    sprintf(buf, " - xxx");
	 }
      }
   else 
       fprintf(stderr,  GETMSG ( MSGID_UNKNOWN_SYM ), symbol );
   
   fputs((const char *)buf, att_file[LC_CTYPE]);
   return;
}

void ul_convert(void)
{  
   int flag = 0;
   symbol_ptr u = NULL;
   symbol_ptr l = NULL;
   symbol_ptr cm;
   char *ptr;

   clear_buf(buf);
   clear_buf(ubuf);
   clear_buf(lbuf);
   
   count++;
   
   if (toupper_kwd) 
      {
      u = search_list(charmap_list, first_entry);
      l = search_list(charmap_list, second_entry);
      } 
   else 
      {
      l = search_list(charmap_list, first_entry);
      u = search_list(charmap_list, second_entry);
      }
			  
   /* for both upper and lower, if entry found 
    * in the charmap list, convert it to proper
    * format.  Otherwise, assume entry is in 
    * one of the numeric formats, or a single CHAR
    */
			   
   if (u)
      {
      /* convert string value to hex digits */
      ptr = map2hex(ubuf, u->mapping, u->ctype);
      
      if (u->ctype != 0)
         {
	 if ((cm = malloc(sizeof(struct symbol_node))) == NULL)
	     exit_malloc_error ();
	       
	 if ((cm->mapping = malloc(strlen(ptr))) == NULL)
	     exit_malloc_error ();
	 else
	     strcpy(cm->mapping, ptr);
	 
         if ((cm->symbol = malloc(5)) == NULL)
	     exit_malloc_error ();
	 else
	     sprintf(cm->symbol, "%d", -1);

         if (u->ctype == 1)
   	    insert_list(&(lctype1_list[current_kwd]), cm);
         else if (u->ctype == 2)
   	    insert_list(&(lctype2_list[current_kwd]), cm);
         else
   	    insert_list(&(lctype3_list[current_kwd]), cm);

	 alt_keywords = 1;
	 flag = 1;
	 }
      }
   else
      if (first_entry[0] != '\\')
         sprintf(ubuf, "0x%x", *first_entry);
      else
         ptr = map2hex(ubuf, first_entry, u->ctype);
   if (l)
      {
      /* convert string value to hex digits */
      ptr = map2hex(lbuf, l->mapping, l->ctype);
      
      if (l->ctype != 0)
         {
	 if ((cm = malloc(sizeof(struct symbol_node))) == NULL)
	     exit_malloc_error ();
	       
	 if ((cm->mapping = malloc(strlen(ptr))) == NULL)
	     exit_malloc_error();
	 else
	     strcpy(cm->mapping, ptr);

         if ((cm->symbol = malloc(5)) == NULL)
	     exit_malloc_error();
	 else
	     sprintf(cm->symbol, "%d", -1);

         if (l->ctype == 1)
   	    insert_list(&(lctype1_list[current_kwd]), cm);
         else if (l->ctype == 2)
   	    insert_list(&(lctype2_list[current_kwd]), cm);
         else
   	    insert_list(&(lctype3_list[current_kwd]), cm);

	 alt_keywords = 1;
	 flag = 1;
	 }
      }
   else
      if (second_entry[0] != '\\')
         sprintf(lbuf, "0x%x", *second_entry);
      else
         ptr = map2hex(lbuf, second_entry, l->ctype);
	 
   if (flag == 0)
      {
      if (count%3 == 1)
         sprintf(buf, "   \\\n                <%s  %s>", ubuf, lbuf); 
      else
         sprintf(buf, "   <%s  %s>", ubuf, lbuf);
      }
	 
   fputs((const char *)buf, att_file[LC_CTYPE]);
   return;
}


void print_lctype( symbol_ptr list_array[], char *header )
{
   symbol_ptr cur;
   
   /* print out alt. keywords list */
   clear_buf(buf);
   sprintf(buf, header);
   fputs((const char *)buf, att_file[LC_CTYPE]);
   for (i = 0; i < MAX_KEYWORDS; i++)
      {
      if (att_extensions)
         continue;
      count = 0;
      cur = list_array[i];
      if ( cur && cur->symbol != NULL)
         {
	 clear_buf(buf);
	 if ((i == KWD_TOUPPER) || (i == KWD_TOLOWER))
	    sprintf(buf, "\nul             ");
	 else
	    sprintf(buf, "\nis%-13s", keywords[i]);

	 fputs((const char *)buf, att_file[LC_CTYPE]);

	 do
	   {
	   clear_buf(buf);
	   
	   /* these "magic numbers" are the strings which
	    * correspond to the constants in map2hex,
	    * stored in output_type; or "-1" for ul_convert.
	    * "0" is CONTINUE (more to come)
	    * "1" is end of ELLIPSIS
	    * "2" is SINGLETON
	    */

	   if (strcmp(cur->symbol, "0") == 0)
	      {
	      if (count >= 4)
	         {
	         sprintf(buf, "  \\\n               %s", cur->mapping);
		 count = 0;
		 }
	      else
	         sprintf(buf, "   %s", cur->mapping);
	      }
	   else if (strcmp(cur->symbol, "1") == 0)
   	      {
	      sprintf(buf, " - %s", cur->mapping);
//	      if (count%3 == 1)
//	         sprintf(buf, "  \\\n            ");
	      }
	   else if (strcmp(cur->symbol,"-1") == 0)
	      {
	      if (count%3 == 1)
	         sprintf(buf, "   \\\n                <%s  %s>",
		   cur->mapping, (cur->next)->mapping);
	      else
	         sprintf(buf, "   <%s  %s>", cur->mapping, (cur->next)->mapping);
	      cur = cur->next;
	      }
	   else
	      sprintf(buf, "%s", cur->mapping);
	   count++;
	   cur = cur->next;
	   fputs((const char *)buf, att_file[LC_CTYPE]);
	   }
	 while (cur != NULL);
         }
      free_list(&(list_array[i]));
      }
      
   fputs("\n", att_file[LC_CTYPE]);
   return;
}

