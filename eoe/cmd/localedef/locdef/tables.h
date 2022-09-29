//____________________________________________________________________________
//
// Copyright (C) 1995 Silicon Graphics, Inc
//
// All Rights Reserved Worldwide.
//
//____________________________________________________________________________
//
// FILE NAME :          tables.h
//
// DESCRIPTION :        Declares functions and constants used to 
//                      preprocess locale file
//                      
//
//____________________________________________________________________________

#ifndef _TABLES_H_
#define _TABLES_H_

#include <stdio.h>
#include <malloc.h>

/* global defines used by everybody */

#define BUFSIZE    256	/* assume lines no longer than 255 chars */
#define NOT_FOUND  NULL

#define TAB        '\t'
#define SPACE      ' '
#define L_ANGLE    '<'
#define R_ANGLE    '>'
#define COMMENT_CH '#'
#define SLASH      '/'
#define BACK_SLASH '\\'
#define NL         '\n'
#define NULL_CHAR  '\0'

#define RAB_ASCII  "x3e"  /* ascii code for '>' */
#define LAB_ASCII  "x3c"  /* ascii code for '<' */

#define LC_BYTE     0
#define LC_CTYPE1   1
#define LC_CTYPE2   2
#define LC_CTYPE3   3

#define LC2_HEADER  0x8e  /* header code for LC_CTYPE2 */
#define LC3_HEADER  0x8f  /* header code for LC_CTYPE3 */

#define TRUE       1
#define FALSE      0

/* parser file defines */

#define CONTINUE   0
#define ELLIPSOID  1
#define SINGLETON  2

#define MAX_KEYWORDS 17

#define KWD_UPPER      0
#define KWD_LOWER      1
#define KWD_ALPHA      2
#define KWD_DIGIT      3
#define KWD_PUNCT      4
#define KWD_XDIGIT     5
#define KWD_SPACE      6
#define KWD_PRINT      7
#define KWD_GRAPH      8
#define KWD_BLANK      9
#define KWD_CNTRL     10
#define KWD_TOUPPER   11
#define KWD_TOLOWER   12
#define KWD_IDEOGRAM  13
#define KWD_PHONOGRAM 14
#define KWD_ENGLISH   15
#define KWD_SPECIAL   16

#define DEFAULT_CM_FILENAME "/usr/lib/locale/charmap/POSIX"

/* names of optional charmap declarations */

#define CM_NEW_ESCAPE  "<escape_char>"
#define CM_NEW_COMMENT "<comment_char>"

/* names of locale def declarations */

#define LOC_NEW_ESCAPE  "escape_char"
#define LOC_NEW_COMMENT "comment_char"

#define COPY_KEYWORD    "copy"
#define ELL_KEYWORD     "..."

#define CM         0
#define LOC        1


/* Error Condition Codes
*/

#define SRC_FILE_ERR  1
#define COL_FILE_ERR  2
#define CM_FILE_ERR   3

// Data Structure Declarations
//

/* Encoding data type
 * ******************
 * Used to store the encoded value of a character
 * Currently defined as long (4 bytes) which should
 * be enough for now
 */

typedef long enc_t;

#define ENC_ILLEGAL	(-1)
#define ENC_IGNORE	(-2)

/* A string of encoded values */
typedef struct {
    enc_t	* enc;
    int		len;
    int		size;
    int		is_string;
} encs_t;

/* Constructors */
encs_t * encs_new ( encs_t * this );
void	 encs_delete ( encs_t * this );
encs_t * encs_append ( encs_t * this, const enc_t val );
encs_t * encs_append_encs ( encs_t * this, const encs_t * toadd );
encs_t * encs_set_string ( encs_t * this, const int yesno );

encs_t * encs_from_num ( encs_t * enc , const char * string );
encs_t * encs_from_charsym ( encs_t * enc, const char * charsym );
encs_t * encs_from_collsym ( encs_t * enc, const char * collsym );
encs_t * encs_from_collelt ( encs_t * enc, const char * collelt );
encs_t * encs_from_string ( encs_t * enc, const char * string );

/* Comparison */
int	 encs_compare ( const encs_t * a, const encs_t * b );

/* Conversion functions */
enc_t convert_char_to_enc ( const char c );
enc_t convert_num_to_enc ( const char * from, int * nb_char_read );
enc_t convert_charsym_to_enc ( const char * from, int * nb_char_read );
enc_t convert_collsym_to_enc ( const char * from );
enc_t convert_string_to_enc ( const char * from, int *nb_char_read );

char * convert_encs_to_string ( const encs_t * encs,
				const char * format,
				char * buf );

/* Symbols
 * *******
 */
struct symbol_node
{
    char	* symbol;
    char	* mapping;
    encs_t	* encoding;
    int		ctype;
    struct symbol_node *next, *last;
};

typedef struct symbol_node *symbol_ptr;

/* Symbol tables implemented as linked lists */
extern symbol_ptr charmap_list;
extern symbol_ptr collsym_list;
extern symbol_ptr collelt_list;

void add_collsym ( char * symbol );
void add_collelt ( char * element, encs_t * encoding );

struct charlist
{
   char *symbol;
   struct charlist *next;
};

typedef struct 
{
    char * code_set_name;
    int mb_cur_max;
    int mb_cur_min;
} charmap_info_t;

extern charmap_info_t charmap_info;

/* List handling functions */
void		init_node ( symbol_ptr node );
void		insert_list ( symbol_ptr * list, symbol_ptr node );
symbol_ptr	search_list ( symbol_ptr list, char * symbol );
void		dump_list ( symbol_ptr list , char * list_name );
void		free_list ( symbol_ptr * list );

/* symbol file processing functions */
char *matchline(char *, char *, FILE *); 
char *get_valid_symbol(char *);
void process_charmap ( FILE * );
int lpp(FILE **, int);

char *map2hex(char *, char *, int);
char *convert_charsyms(char *list);

#endif /* _TABLES_H_ */
