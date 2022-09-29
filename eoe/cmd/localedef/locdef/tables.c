//____________________________________________________________________________
//
// Copyright (C) 1995 Silicon Graphics, Inc
//
// All Rights Reserved Worldwide.
//
//____________________________________________________________________________
//
// FILE NAME :          tables.c
//
// DESCRIPTION :        Preprocesses local file to create tables used
//                      during file parsing
//
//____________________________________________________________________________
//

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory.h>
#include <malloc.h>
#include <string.h>
#include <values.h>
#include <alloca.h>
#include <errno.h>

#include "localedef.h"
#include "tables.h"

/* Value of the highest encoding of a character
   Following values will be used for charsymbols */
static enc_t	enc_charsym_max = ENC_ILLEGAL;
static enc_t	enc_collsym_max = ENC_ILLEGAL;

/* Keywords */
#define CHARKW_CODE_SET_NAME	"<code_set_name>"
#define CHARKW_MB_CUR_MAX	"<mb_cur_max>"
#define CHARKW_MB_CUR_MIN	"<mb_cur_min>"

charmap_info_t charmap_info =
{
    "ISO8859-1",	/* code_set_name */
    1,			/* mb_cur_max */
    1			/* mb_cur_min */
};

/* Symbol tables implemented as linked lists */
symbol_ptr charmap_list = NULL;   /* head of list of valid CHARSYMBOLS  */
symbol_ptr collsym_list = NULL;   /* head of list of valid COLLSYMBOLS  */
symbol_ptr collelt_list = NULL;   /* head of list of valid COLLELEMENTS */

/*
 * These global flags are set here and used in loc.y
 * to understand the structure of the chrtbl based on 
 * the types of values in the charmap.  Note that they
 * won't necessarily be used...by default, ctype_flag 
 * is on, and all other off.
 */

int ctype_width = 1;
int ctype1 = 0;
int ctype2 = 0;
int ctype3 = 0;

// MATCHLINE - find position in buffer that matches given string,
// starting at the given position.  Search should not proceed 
// beyond backstop, the last position in the buffer.  
// Assume that strings start at
// beginning of line (minus whitespace) also that any whitespace in
// string matches any number of whitespace characters in buffer.
// Just uses the most brain-dead algorithm possible (strcmp).

char *matchline(char *source, char *line, FILE *file)
{
   char *ch = line;

   do
      {
 
      /* eat any white space */
      while (*ch == SPACE || *ch == TAB)
        ch++;

      /* compare source string with string on line */
      if (strncmp(source, ch, strlen(source)) == 0)
         {
         /* return position if strings match */
         return(ch);
         }
      /* otherwise try next line */
      ch = fgets(line, BUFSIZE, file);
      }
   while (ch != NULL); 
   return(NOT_FOUND);
}

// MATCH_KEYWORD:
//  matches the given keyword anywhere in a buffer pointed to by line

char *match_keyword(char *line, char *keyword)
{
   char *ch, *dup = strdup(line);

   ch = strtok(dup, " \t");
   do
      {
      /* compare keyword with string on line */
      if (strncmp(keyword, ch, strlen(keyword)) == 0)
         {
         /* return position if strings match */
         free(dup);
         return(ch);
         }
      }
   while ((ch = strtok(NULL, " \t\n")) != NULL);

   free(dup);
   return(NOT_FOUND);
}

char * category_name [ LC_ALL ] = {
    "LC_CTYPE",
    "LC_NUMERIC",
    "LC_TIME",
    "LC_COLLATE",
    "LC_MONETARY",
    "LC_MESSAGES"
};

int
match_category ( const char * from )
{
    int i;
    char * ch, * line = strdup ( from );

    ch = strtok( line, " \t");
    do
    {
	for ( i = 0; i < LC_ALL; i++ )

	    if (strncmp ( ch, category_name[i], strlen(category_name[i]) ) == 0)
	    {
		free ( line );
		return i;
	    }
    }
    while ((ch = strtok(NULL, " \t\n")) != NULL);

   free( line );
   return -1;
}


// get_valid_symbol: 
//  Verifies that a symbol of the form <{valid-char]*>
//  exists on the line.  If so, it is returned to the caller.  If not,
//  an error message is printed and the line is skipped.

char *get_valid_symbol(char *line)
{
   int  count;
   char *symbol, *symbol1, *symbol2;

   symbol1 = strchr(line, (int) L_ANGLE);
   symbol2 = strchr(line, (int) R_ANGLE);

   /* if line is blank, skip it quietly */
   if ((symbol1 == NULL) && (symbol2 == NULL))
      return(symbol1);
     
   /* search for escaped characters */
   symbol = symbol1;
   while (symbol < symbol2)
     {
     if (*symbol == BACK_SLASH)
	{
	/* if we have "\\", skip it */
	if (*(symbol+1) == BACK_SLASH)
	   symbol += 2;
	/* if we have "\>", find a new ">" */
	else if (*(symbol+1) == R_ANGLE)
	   {
	   symbol2 = strchr(symbol2+1, (int) R_ANGLE);
	   symbol += 2;
	   }
	else
	   symbol++;
	}
     else
	symbol++;
     }
   
   if (symbol1 == NULL || symbol2 == NULL)
      {
      fprintf(stderr, GETMSG(MSGID_VALIDCM), line);
      symbol = NULL;
      }
   else
      {
      count = symbol2 - symbol1 + 2;
      if ((symbol = malloc(count)) == NULL)
	  exit_malloc_error ( );
      else
         {
	 memcpy(symbol, symbol1, count-1);
	 *(symbol+count-1) = NULL_CHAR;
	 }
      }

   return(symbol);
}

/* PARSE_MAPPING
 *  Sets flags based on the format of the encoding string
 *  In particular, determines whether special Japanses
 *  of Taiwanese formats exist.
 */

int parse_mapping(char *encoding)
{
   int  i, ctype = 0, num1, num_bytes = 1;
   char *first_byte, *cur;
   char *dup = strdup(encoding);
   
   /*
    * There are three possibilities for the elements
    * of the encoding string:
    *  \d<decimal digits>
    *  \x<hexadecimal digits>
    *  \<octal digits>
    */
   
   first_byte = strtok(dup, "\\ \t\n");

   while (strtok(NULL, "\\\n ") != NULL)
     {
     num_bytes++;
     }

   if (num_bytes > 1)
     {
     /* parse out first_byte using strtol */

     if (*first_byte == 'd')
       {
       /* decimal: convert digits to hex */
       num1 = (int) strtol(first_byte+1, &cur, 10);
       }
     else if (*first_byte == 'x')
       {
       /* hexadecimal */
       num1 = (int) strtol(first_byte+1, &cur, 16);
       }
     else
       {
        /* octal byte value: drop escape chars */
       num1 = (int) strtol(first_byte, &cur, 8);
       }
	
   YDBG_PRINTF(("Parse Mapper: first byte [%s][%x] byte length [%d] \n",
		first_byte, num1, num_bytes));
   
     /* now the analysis */
     if (num1 == LC2_HEADER)
        {
	ctype  = 2;
	ctype2 = num_bytes - 1;

	/* skip the first header byte */
	for (cur = encoding+1; *cur != '\\'; cur++)
	   ;
	   
	for (i = 0; *cur != NULL_CHAR; cur++, i++)
	  *(encoding+i)  = *cur;
	*(encoding+i) = NULL_CHAR;
	}
     else if (num1 == LC3_HEADER)
	{
	ctype  = 3;
	ctype3 = num_bytes - 1;

	/* skip the first header byte */
	for (cur = encoding+1; *cur != '\\'; cur++)
	  ;
	   
	for (i = 0; *cur != NULL_CHAR; cur++, i++)
	  *(encoding+i)  = *cur;
	*(encoding+i) = NULL_CHAR;
	}
     else
	{
	ctype = 1;
        ctype1 = ((num_bytes > ctype1) ? num_bytes : ctype1);
	}
     }

   ctype_width = ((num_bytes > ctype_width) ? num_bytes : ctype_width);
   
   return(ctype);
}

void close_coll_file(FILE *coll_file)
{
   fclose(coll_file);
}

// get_mapping:
//  Verifies that a CHARSYMBOL mapping in octal or hex format
//  exists as the next field on a line.  If so, it is returned
//  to the caller.  If not, an error message is printed and the 
//  line is skipped.

char *get_mapping(char *line)
{
   int  count;
   char *symbol, *symbol1;

   strtok(line, " \t\n");
   symbol = symbol1 = strtok(NULL, " \t\n");

   if (symbol == NULL)
      {
      fprintf(stderr, GETMSG(MSGID_VALIDCM), line);
      return(NULL);
      }

   /* increment second pointer until whitespace found */
   while ((*symbol != NULL_CHAR) &&
	  (*symbol != NL)        &&
	  (*symbol != TAB) &&
	  (*symbol != SPACE))
     symbol++;
 
   /* (*symbol) pointing at whitespace, back up one */
   symbol--;
   count = symbol - symbol1 + 2;
   if ((symbol = malloc(count)) == NULL)
       exit_malloc_error ();
   else
      {
      memcpy(symbol, symbol1, count-1);
      *(symbol+count-1) = NULL_CHAR;
      }

   return(symbol);
}

int
process_charmap_keyword ( char * symbol, char * value )
{
    if ( !strcmp ( symbol, CHARKW_CODE_SET_NAME ) )
    {
	
	char * end;

	if ( (value = strchr ( value, '"' )) == NULL )
	    return 0;
	value++;
	if ( (end = strchr ( value, '"' )) == NULL )
	    return 0;
	*end = '\0';
	charmap_info.code_set_name = strdup ( value );

    }
    else if ( !strcmp ( symbol, CHARKW_MB_CUR_MAX ) )
    {
	if ( sscanf ( value, "%d\n", & charmap_info.mb_cur_max ) != 1 )
	    return 0;
    }
    else if ( !strcmp ( symbol, CHARKW_MB_CUR_MIN ) )
    {
	if ( sscanf ( value, "%d\n", & charmap_info.mb_cur_min ) != 1 )
	    return 0;
    }
    else
	return 0;
    return 1;
}



/* PROCESS_CHARMAP -
 *  get each line of the charmap file and store the symbol
 *  along with its associated value string in the charmap list.
 */
   
void process_charmap ( FILE *cm_file )
{
   int i;
   int lineno = 0;
   char line [ BUFSIZE ];
   struct symbol_node *cm;
   char * symbol, * value;

   enc_charsym_max = 0;
   
   while ( fgets(line, BUFSIZE, cm_file) &&
	   strncmp ( line, "CHARMAP", sizeof ( "CHARMAP" ) -1 ) )
   {
       lineno ++;
       if ( ( symbol = get_valid_symbol ( line ) ) &&
	    ( value = get_mapping ( line ) ) )

	   if ( ! process_charmap_keyword ( symbol, value ) )
	   {
	       fprintf ( stderr, GETMSG ( MSGID_CHARMAP_SYNTAX ), lineno );
	       exit ( 2 );
	   }
   }

   while (strncmp(line, "END CHARMAP", sizeof("END CHARMAP")-1) != 0)
     {
     if ((cm = malloc(sizeof(struct symbol_node))) == NULL)
	 exit_malloc_error ();

     if (((cm->symbol = get_valid_symbol(line)) != NULL) &&
	 ((cm->mapping = get_mapping(line)) != NULL))
	{
	cm->ctype = parse_mapping(cm->mapping);
	cm->encoding = encs_from_num ( NULL , cm->mapping );
	if ( ! cm->encoding )
	{
	    fprintf ( stderr, GETMSG ( MSGID_CHARMAP_SYNTAX ), lineno );
	    continue;
	}

	/* encs_from_num guarantees one and only one encoding */
	if ( cm->encoding->enc[0] > enc_charsym_max )
	    enc_charsym_max = cm->encoding->enc[0];

	insert_list( & charmap_list, cm );
	}
     if ( fgets(line, BUFSIZE, cm_file) == NULL )
	 continue;
     lineno ++;
     }
   
   enc_collsym_max = enc_charsym_max;

   return;
}

/* INPUT: FILE *fp, assumed to be opened and at BOF */
/* OUTPUT: new escape/comment character, or NULL_CHAR if none found */
char get_special_char(FILE *fp, char *keyword)
{
   char ch = NULL_CHAR, *line, buffer[BUFSIZE];

   /* load the first line into buffer */
   if ((line = fgets(buffer, BUFSIZE, fp)) == NULL)
      {
#ifdef YACC_DEBUG
      fprintf(stderr, GETMSG(MSGID_EOF), "get-escape-char");
#endif	 
      return(ch);
      }
   
   /* attempt to match the line containing the keyword */
   if ((line = matchline(keyword, line, fp)) != NOT_FOUND)
      {
      /* format should be <keyword> <char> */
      line = strtok(line, " \t"); /* points to <keyword> */
      line = strtok(NULL, " \t"); /* points to <char>    */
	 
      /* line should be pointing to char */
      ch = *line;

      YDBG_PRINTF(("DEBUG: In get-special-char, new character for %s is %c\n", 
		  keyword, ch));
      }

   /* rewind the file */
   rewind(fp);

   return(ch);
}

void redefine_comment(char new, char *line)
{

   /* Processing depends on the # comment character,
    * so change all occurances of new (in the comment
    * position) to '#'.  Since comment lines must start
    * with new as the first non-whitespace on a line, only
    * those occurances need be altered
    */
   
   /* eat any initial white space */
   while (*line == SPACE || *line == TAB)
     line++;

   /* compare source string with string on line */
   if (new == *line)
      /* we got a match, replace character in buffer */
      *line = COMMENT_CH;
   
   return;     
}

void insert_char(char *base, char *cur, char ch)
{
   char *end;
   int base_len, total;
   
   base_len = strlen(base);
   total = base_len + 1;
   
   /* length must not exceed BUFSIZE */
   if (total > BUFSIZE)
     {
     fprintf(stderr, GETMSG(MSGID_BUFLEN));
     return;
     }
   
   /* 'allocate' enough room for the inserted text */
   end = base + base_len + 1;
   
   /* copy everything down a spot */
   do
     {
     *end = *(end-1);
     end--;
     }
   while (end != cur);
   
   return;   
}

void replace_embedded(char *base, char *pos, char type)
{
   char *end, *txt;
   int base_len, txt_len, total;
   
   if (type == R_ANGLE)
     txt = RAB_ASCII;
   else 
     txt = LAB_ASCII;
   

  YDBG_PRINTF(("Processing embedded brkts in %s", base));
   
   base_len = strlen(base);
   txt_len  = strlen(txt);
   total = base_len + txt_len;
   
   /* length must not exceed BUFSIZE */
   if (total > BUFSIZE)
     {
     fprintf(stderr, GETMSG(MSGID_BUFLEN));
     return;
     }
   
   /* 'allocate' enough room for the inserted text */
   end = base + base_len + txt_len - 1;
   
   /* copy everything down a spot */
   do
     {
     *end = *(end-txt_len+1);
     end--;
     }
   while (end != pos);

   memcpy(pos+1, txt, txt_len);
    
   return;   
}

void embedded_brkts_driver(char *base)
{
   char *pos = base;
   
   while (*pos != NULL_CHAR)
     {
     if (*pos == BACK_SLASH)
	{
	if (*(pos+1) == BACK_SLASH)
	  pos++;
	else if (*(pos+1) == R_ANGLE)
 	  replace_embedded(base, pos, R_ANGLE);
	else if (*(pos+1) == L_ANGLE)
 	  replace_embedded(base, pos, L_ANGLE);
	}
     pos++;
     }
   
   return;
}

void redefine_escape(char new, char *line)
{
   char *begin = line;
   
   /* Case where an escape sequence occurs and must be
    * replaced;  if we see:
    * 
    * 1. <new><new> - substitute first one only
    * 2. <new><anything> - substitute
    * 3. <new>\ - substitute
    * 4. <anything else>\ - add a \
    */
   
   while (*line != NULL_CHAR)
     {
     if (*line == new) 
	{
	*line = BACK_SLASH;
	if ((*(line+1) == new) ||
	    (*(line+1) == BACK_SLASH)) /* skip it */
	    line++;
//	else if (*(line+1) == R_ANGLE)
//	    replace_embedded(begin, line, RAB_ASCII);
	}
     else if (*line == BACK_SLASH)
	{
	insert_char(begin, line, BACK_SLASH);
	line++;
	}

     line++;
     }

   return;
}

/* process_ellipsis - inserts implicit charsyms into charmap file
 * 
 * Input: line - line to parse
 *        fp - output file pointer
 */

void process_ellipsis(char *line, FILE *fp)
{
   int i, start, end, count, num1, num2;
   char *encoding, *base, *number, *cur, *temp;
   
   number = base = strdup(line);

   while (strncmp(number, ELL_KEYWORD, strlen(ELL_KEYWORD)) != 0)
     {
     /* if no ellipsis on line, put line to file and return */
     if (*number == NULL_CHAR)
	{
	fputs(line, fp);
        return;
	}
     /* otherwise, keep looking */
     number++;
     }

   number = base;
   
   YDBG_PRINTF(("Processing ellipsis in charmap line %s", base));
   
   /* get first number to use in symbol names */
   /* NB: we will skip leading zeroes and consider them to be */
   /* part of the base */
   
   while ((*number > '9') || (*number < '1'))
     {
     /* if end of line reached, charmap line is not in proper format */
     if (*number == NL)
        return;        
     number++;
     }

   cur = number;
   start = atoi(number);
   
   /* skip to end of symbol */

   number = strchr(base, (int) R_ANGLE);
   *cur = NULL_CHAR;
   
   /* get second number */
   while ((*number > '9') || (*number < '1'))
     {
     /* if end of line reached, charmap line is not in proper format */
     if (*number == NL)
        return;        
     number++;
     }
   
   end = atoi(number);
   count = end - start + 1;
   
   *number = NULL_CHAR;
   
   YDBG_PRINTF(("CM Ellipsis: base [%s] count [%d] start [%d]",
		base, count, start));
   
   if ((encoding = get_mapping(line)) != NULL)
     {
	/*
	 * Three possibilities for the encodeing strings
	 *  \d<decimal digits>
	 *  \x<hexadecimal digits>
	 *  \<octal digits>
	 */
    
    /* if two byte encoding, strlen will be more than 5 */
    cur = encoding;
	
    if (strlen(encoding) > 5)
       {
       if (cur[0] == BACK_SLASH && cur[1] == 'd')
          {
          num1 = (int) strtol(encoding+2, &temp, 10);
	  num2 = (int) strtol(temp+2, NULL, 10);
	  }
       else if (cur[0] == BACK_SLASH && cur[1] == 'x')
          {
	  num1 = (int) strtol(encoding+2, &temp, 16);
          num2 = (int) strtol(temp+2, NULL, 16);
	  }
       else
          {
          num1 = (int) strtol(encoding+1, &temp, 8);
          num2 = (int) strtol(temp+1, NULL, 8);
	  }
       }
    else
       {
       num1 = 0;
       if (cur[0] == BACK_SLASH && cur[1] == 'd')
          num2 = (int) strtol(encoding+2, NULL, 10);
       else if (cur[0] == BACK_SLASH && cur[1] == 'x')
          num2 = (int) strtol(encoding+2, NULL, 16);
       else
          num2 = (int) strtol(encoding+1, NULL, 8);
       }

     /* write sequence to temporary file */
	
     for (i = 0; i < count; i++)
	{
	if (num1 == 0)
          {
	  sprintf(line, "%s%d>   \\x%x\n", base, start+i, num2+i);
	  if (num2+i == 0xFF) num1++; 
	  }
        else
          sprintf(line, "%s%d>   \\x%x\\x%x\n", base, start+i, num1, num2+i);
	fputs(line, fp);
	}
     }
   
   return;
}


/* process_copy - copies locale sections from referenced locale def
 * 
 * Input: line - command line to parse
 *        cur_section - pointer to LC section name currently being processed
 *        fp - output file pointer
 */

void process_copy(char *line, char *buffer, int category, FILE *fp)
{
   FILE *input;
   char new_comment, new_escape, *fname, *dup = strdup(line);

   YDBG_PRINTF(("Processing copy > %s", line));

   strtok(dup, " \t");
   if ((fname = strtok(NULL, " \"\t")) == NULL)
     {
     fprintf(stderr, GETMSG(MSGID_NOCOPY));
     fputs(line, fp);
     free(dup);
     return;
     }
   
   /* strip out trailing quotes */
   if ((dup = strchr(fname, '"')) != NULL)
     *dup = NULL_CHAR;

   if ( ! ( input = process_symlink_copy ( category, fname ) ) )
     {
     fprintf(stderr, GETMSG(MSGID_NOCOPY));
     fputs(line, fp);
     free(dup);
     return;
     }
   
   /* check for redefinition of comment and escape characters in original */
   new_comment = get_special_char(input, LOC_NEW_COMMENT);
   new_escape  = get_special_char(input, LOC_NEW_ESCAPE);
   
   if ((line = fgets(buffer, BUFSIZE, input)) == NULL)
     {
     fprintf(stderr, GETMSG(MSGID_EOF), fname);
     fputs(line, fp);
     return;
     }
     
   if ((line = matchline( category_name[category], line, input)) == NOT_FOUND)
     {
     fprintf(stderr, GETMSG(MSGID_NOCOPY));
     fputs(line, fp);
     return;
     }
   
   while (((line = fgets(buffer, BUFSIZE, input)) != NULL) &&
	  (match_keyword(line, "END") == NOT_FOUND))
     {
     if (new_comment != NULL_CHAR)
	redefine_comment(new_comment, line);
	
     if (new_escape != NULL_CHAR)
	redefine_escape(new_escape, line);

     embedded_brkts_driver(line);

     fputs(buffer, fp);
     }
   
   free(dup);
   
   return;
}

/* lpp - locale def file preprocessor
 * 
 *   The goal of this function is to handle boundary cases well, including
 *     input from stdin
 *     alternate syntax for escape characters
 *     alternate syntax for comment characters
 * 
 * INPUT: 
 *   **fp: address of input file pointer
 *   type: LOC for locale def, CM for charmap
 * 
 * OUTPUT:
 *   return value is 0 for success, non-zero otherwise
 *   returns pointer to new (tmp) file in fp
 */

int lpp(FILE **fp, int type)
{
   FILE *temp_fp;
   char new_comment, new_escape, *line, buffer[BUFSIZE], oldbuf[BUFSIZE];
   int current_cat = -1;
   int cat = -1;

   if (type == LOC && *fp == NULL)
     {
     /* stdin processing goes here */
     *fp = stdin;	
     }

   /* open a temporary file */
   if ((temp_fp = tmpfile()) == NULL)
     {
	fprintf(stderr, GETMSG(MSGID_TMPFIL));
     return(1);
     }

   /* check for redefinition of comment and escape characters in original */
   new_comment = get_special_char(*fp, ((type == LOC) ? LOC_NEW_COMMENT : CM_NEW_COMMENT));
   new_escape  = get_special_char(*fp, ((type == LOC) ? LOC_NEW_ESCAPE : CM_NEW_ESCAPE));
   
   /* copy original to temporary file line by line,
    * making necessary changes to comments and 
    * escape sequences as we go;  expanding brackets embedded in
    * CHARSYMBOLS, doing copies if found and expanding ellipses
    * in the charmap file.
    */
   
   while ((line = fgets(buffer, BUFSIZE, *fp)) != NULL)
     {
     if (new_comment != NULL_CHAR)
	redefine_comment(new_comment, line);
	
     if (new_escape != NULL_CHAR)
	redefine_escape(new_escape, line);
	
     embedded_brkts_driver(line);

     if ( (cat = match_category ( line )) != -1 )
	 current_cat = cat;

     if (type == CM)
	process_ellipsis(line, temp_fp);

     else if ( match_keyword(line, COPY_KEYWORD) != NOT_FOUND &&
	       current_cat != -1 )
	process_copy(line, buffer, current_cat, temp_fp);
     else
        {
	fputs(buffer, temp_fp);
	strcpy(oldbuf, buffer);
	}
     }

   /* close up the original and rename */
   fclose(*fp);
   rewind(temp_fp);
   *fp = temp_fp;

   return(0);
}

void
add_collsym ( char * symbol )
{
    /* Check for existance in charsymbols and collsymbols.
       Note: no collelement has yet been defined */
    symbol_ptr node;

    if ( search_list ( charmap_list, symbol ) ||
	 search_list ( collsym_list, symbol ) )
    {
	fprintf ( stderr, GETMSG ( MSGID_DUPSYM ), symbol );
	return;
    }

    if ( ( node = malloc(sizeof(struct symbol_node))) == NULL)
	exit_malloc_error ();

    node->symbol = symbol;
    node->mapping = NULL;
    node->encoding = encs_append ( NULL , enc_collsym_max ++ );

    insert_list ( & collsym_list, node );
}

void
add_collelt ( char * element, encs_t * encoding )
{
    /* Check for existance in charsymbols, collsymbols and collelements */
    symbol_ptr node;

    if ( search_list ( charmap_list, element ) ||
	 search_list ( collsym_list, element ) ||
	 search_list ( collelt_list, element ) )
    {
	fprintf ( stderr, GETMSG ( MSGID_DUPSYM ), element );
	return;
    }

    if ( ( node = malloc(sizeof(struct symbol_node))) == NULL)
	exit_malloc_error ();

    node->symbol = strdup ( element );
    node->mapping = NULL;
    node->encoding = encoding;

    insert_list ( & collelt_list, node );
}




#define ENCS_INIT_SIZE	2

encs_t *
encs_new ( encs_t * this )
{
    if ( !this )
	this = malloc ( sizeof ( encs_t ) );

    this->enc = malloc ( sizeof ( enc_t ) * ENCS_INIT_SIZE );
    this->len = 0;
    this->size = ENCS_INIT_SIZE;
    this->is_string = 0;

    return this;
}

void
encs_delete ( encs_t * this )
{
    if ( this )
    {
	if ( this->enc )
	    free ( this->enc );
	free ( this );
    }
}

encs_t *
encs_append ( encs_t * this, const enc_t val )
{
    if ( ! this )
	this = encs_new ( NULL );

    if ( this->len >= this->size )
    {
	this->size *= 2;
	this->enc = realloc ( this->enc, this->size * sizeof(enc_t) );
    }
    this->enc[this->len++] = val;

    return this;
}

encs_t *
encs_append_encs ( encs_t * this, const encs_t * toadd )
{
    int i;
    for ( i = 0; i < toadd->len; i ++ )
	this = encs_append ( this, toadd->enc[i] );

    return this;
}

encs_t *
encs_set_string ( encs_t * this, const int yesno )
{
    this->is_string = yesno;
    return this;
}

encs_t *
encs_from_num ( encs_t * this, const char * string )
{
    /* This will create a string of one encoding */
    enc_t val = convert_num_to_enc ( string, NULL );

    if ( val == ENC_ILLEGAL )
	return 0;
    return encs_append ( this, val );
}

encs_t *
encs_from_charsym ( encs_t * this, const char * charsym )
{
    /* This will create a string of one encoding */
    enc_t val = convert_charsym_to_enc ( charsym, NULL );

    if ( val == ENC_ILLEGAL )
	return 0;
    return encs_append ( this, val );
}

encs_t *
encs_from_collsym ( encs_t * enc, const char * collsym )
{
    /* This will create a string of one encoding */
    enc_t val = convert_collsym_to_enc ( collsym );

    if ( val == ENC_ILLEGAL )
	return 0;
    return encs_append ( enc, val );
}

encs_t *
encs_from_collelt ( encs_t * enc, const char * elt )
{
    symbol_ptr n;

    if ( n = search_list ( collelt_list, (char *) elt ) )
	return encs_append_encs ( enc, n->encoding );
    return 0;
}

encs_t *
encs_from_string ( encs_t * this, const char * from )
{
    enc_t enc;
    int n;

    while ( *from )
    {
	enc = convert_string_to_enc ( from, & n );
	if ( enc == ENC_ILLEGAL )
	    break;
	from += n;
	this = encs_append ( this, enc );
    }
    return this;
}

int
encs_compare ( const encs_t * a, const encs_t * b )
{
    if ( a == b )
	return 1;

    if ( a->len != b->len )
	return 0;

    /* memcmp is like strcmp */
    return ! ( memcmp ( a->enc, b->enc, a->len * sizeof ( a->enc[0] ) ) );
}

enc_t
convert_char_to_enc ( const char c )
{
    return (enc_t) c;
}
	

enc_t
convert_num_to_enc ( const char * from, int * nb_char_read )
{
    char escape_string [ 2 ];
    char * string = strcpy ( alloca ( strlen (from) + 1 ), from );
    char * s;
    char * end = 0;
    enc_t  val = 0;
    short  byte = 0;

    escape_string[0] = escape_char;
    escape_string[1] = '\0';

    s = strtok ( string, escape_string );

    while ( s )
    {
	switch ( *s ) {
	case 'd':
	    s++;
	    byte = strtol ( s, & end, 10 );
	    break;

	case 'x':
	    /* Convert from hexadecimal */
	    s++;
	    byte = strtol ( s, & end, 16 );
	    break;

	default:
	    /* Convert from octal */
	    byte = strtol ( s, & end, 8 );
	}
	if ( ( *end != '\0' && ! isspace ( *end ) )
	     || ( byte < 0 || byte > 255 ) )

	    /* Error */
	    return ENC_ILLEGAL;

	val = ( val << 8 ) + byte;

	s = strtok ( NULL, escape_string );
    }
    return val;
}

enc_t
convert_charsym_to_enc ( const char * from, int * nb_char_read )
{
    symbol_ptr n;
    char * string;
    const char * c = from;

    if ( nb_char_read )
	*nb_char_read = 0;

    if ( *c++ != L_ANGLE )
	return ENC_ILLEGAL;

    while ( ( *c != R_ANGLE ) && ( *(c-1) != escape_char ) && ( *c != '\0' ) )
	c++;

    if ( *c == '\0' )
	return ENC_ILLEGAL;

    c++;     /* Include the last angle bracket */

    string = strncpy ( alloca ( c-from+1 ), from, c-from );
    string [ c - from ] = '\0';

    if ( n = search_list ( charmap_list, (char *)string ) )
    {
	if ( nb_char_read )
	    *nb_char_read = c - from;
	return ( n->encoding->enc[0] );
    }
    return ENC_ILLEGAL;
}

enc_t
convert_collsym_to_enc ( const char * collsym )
{
    symbol_ptr n;

    if ( n = search_list ( collsym_list, (char *)collsym ) )
	return ( n->encoding->enc[0] );
    return ENC_ILLEGAL;
}

enc_t
convert_string_to_enc ( const char * from, int * nb_char_read )
{
    enc_t enc;

    if ( *from == L_ANGLE )
	/* Character symbol */
	enc = convert_charsym_to_enc ( from, nb_char_read );

    else if ( *from == escape_char )
	/* Can be a whole bunch of things */
	/* Numeric value */
	enc = convert_num_to_enc ( from, nb_char_read );
    else
    {
	/* Character */
	enc = convert_char_to_enc ( *from );
	if ( nb_char_read )
	    *nb_char_read = 1;
    }

    return enc;
}

char *
convert_encs_to_string ( const encs_t * encs,
				const char * format,
				char * buf )
{
    static char ibuf [ 256 ];
    char * c = ibuf;
    int i;

    for ( i = 0; i < encs->len ; i ++ )
    {
	if ( encs->enc [ i ] <= enc_charsym_max )
	    c += sprintf ( c, format, encs->enc[i] );
	else
	    fprintf ( stderr, "Internal error: printing a collsymbol %d\n",
		      encs->enc [ i ] );
    }

    if ( !buf )
	return strdup ( ibuf );
    else
	return strcpy ( buf, ibuf );
}


void
init_node ( symbol_ptr node )
{
    node->symbol	= NULL;
    node->mapping	= NULL;
    node->encoding	= NULL;
    node->next		= NULL;
    node->last		= NULL;
}

void
insert_list ( symbol_ptr * head_ptr , symbol_ptr node )
{
    if ( head_ptr == NULL )
	/* We have been given no list to insert into */
	return;

    if ( *head_ptr == NULL )
	*head_ptr = node;
    else
	(*head_ptr)->last->next = node;

    (*head_ptr)->last = node;
    node->next = NULL;

}

symbol_ptr
search_list ( symbol_ptr HEAD, char *key )
{
   symbol_ptr cur;
   
   for (cur = HEAD; cur != NULL && 
	            strcmp(cur->symbol, key) != 0; cur = cur->next)
     ;
   
   return(cur);
}

void dump_list(symbol_ptr HEAD, char *name)
{
   symbol_ptr cur;
   
   printf("Contents of %s list:\n\n", name);
   
   for (cur = HEAD; cur != NULL; cur = cur->next)
     {
     printf("   symbol [%s]\n", cur->symbol);
     if (cur->mapping != NULL)
	  printf("   value  [%s]\n", cur->mapping);
     }
}

void
free_list ( symbol_ptr * list )
{
    symbol_ptr temp, cur;
   
    if ( list == NULL )
	return;

    for ( cur = *list; cur != NULL; )
    {
	temp = cur->next;
	free ( cur );
	cur = temp;
    }
    *list = NULL;
}
