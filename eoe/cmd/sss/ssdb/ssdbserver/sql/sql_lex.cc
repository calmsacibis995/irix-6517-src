/* Copyright (C) 1979-1998 TcX AB & Monty Program KB & Detron HB

   This software is distributed with NO WARRANTY OF ANY KIND.  No author or
   distributor accepts any responsibility for the consequences of using it, or
   for whether it serves any particular purpose or works at all, unless he or
   she says so in writing.  Refer to the Free Public License (the "License")
   for full details.

   Every copy of this file must include a copy of the License, normally in a
   plain ASCII text file named PUBLIC.	The License grants you the right to
   copy, modify and redistribute this file, but only under certain conditions
   described in the License.  Among other things, the License requires that
   the copyright notice and this notice be preserved on all copies. */

/* A lexical scanner on a temporary buffer with a yacc interface */

#include "mysql_priv.h"
#include <m_ctype.h>
#include <hash.h>

/* Macros to look like lex */

#define yyGet()		*(lex->ptr++)
#define yyGetLast()	lex->ptr[-1]
#define yyPeek()	lex->ptr[0]
#define yyUnget()	lex->ptr--
#define yySkip()	lex->ptr++

pthread_key(LEX*,THR_LEX);


#define TOCK_NAME_LENGTH 20

#include "lex_hash.h"

static uchar state_map[256];


void lex_init(void)
{
  uint i;
  DBUG_ENTER("lex_init");
  for (i=0 ; i < array_elements(symbols) ; i++)
    symbols[i].length=(uchar) strlen(symbols[i].name);
  for (i=0 ; i < array_elements(sql_functions) ; i++)
    sql_functions[i].length=(uchar) strlen(sql_functions[i].name);

  VOID(pthread_key_create(&THR_LEX,NULL));

  /* Fill state_map with states to get a faster parser */
  for (i=0; i < 256 ; i++)
  {
    if (isalpha(i))
      state_map[i]=(uchar) STATE_IDENT;
    else if (isdigit(i))
      state_map[i]=(uchar) STATE_NUMBER_IDENT;
#if defined(USE_MB) && defined(USE_MB_IDENT)
    else if (ismbhead(i))
      state_map[i]=(uchar) STATE_IDENT;
#endif
    else
      state_map[i]=(uchar) STATE_CHAR;
  }
  state_map['_']=state_map['$']=(uchar) STATE_IDENT;
  state_map['\'']=state_map['"']=(uchar) STATE_STRING;
  state_map['-']=state_map['+']=(uchar) STATE_SIGNED_NUMBER;
  state_map['.']=(uchar) STATE_REAL_OR_POINT;
  state_map['<']=state_map['>']=state_map['=']=state_map['!']=
    (uchar) STATE_CMP_OP;
  state_map['&']=state_map['|']=(uchar) STATE_BOOL;
  state_map['#']=(uchar) STATE_COMMENT;
  state_map[';']=(uchar) STATE_COLON;
  state_map[0]=(uchar) STATE_EOL;
  state_map['\\']= (uchar) STATE_ESCAPE;
  state_map['/']= (uchar) STATE_LONG_COMMENT;
  state_map['*']= (uchar) STATE_END_LONG_COMMENT;
  state_map['@']= (uchar) STATE_USER_END;
  DBUG_VOID_RETURN;
}


void lex_free(void)
{					// Call this when daemon ends
  DBUG_ENTER("lex_free");
  DBUG_VOID_RETURN;
}


LEX *lex_start(THD *thd, uchar *buf,uint length)
{
  LEX *lex= &thd->lex;
  lex->next_state=STATE_START;
  lex->end_of_query=(lex->ptr=buf)+length;
  lex->yylineno = 1;
  lex->create_refs=lex->in_comment=0;
  lex->length=0;
  lex->in_sum_expr=0;
  lex->expr_list.empty();
  lex->convert_set=(lex->thd=thd)->convert_set;
  lex->yacc_yyss=lex->yacc_yyvs=0;
  lex->ignore_space=test(thd->client_capabilities & CLIENT_IGNORE_SPACE);
  return lex;
}

void lex_end(LEX *lex)
{
  lex->expr_list.delete_elements();	// If error when parsing sql-varargs
  x_free(lex->yacc_yyss);
  x_free(lex->yacc_yyvs);
}


static int find_keyword(LEX *lex,uint len, bool function)
{
  uchar *tok=lex->tok_start;

  if (len <= TOCK_NAME_LENGTH)
  {
    SYMBOL *symbol = get_hash_symbol((const char *)tok,len,function);

    if (symbol)
    {
      lex->yylval->lex_str.str= (char*) tok;
      lex->yylval->lex_str.length=len;
      return symbol->tok;
    }
  }
#ifdef HAVE_DLOPEN
  udf_func *udf;
  if (function && (udf=find_udf((char*) tok, len)))
  {
    switch (udf->returns) {
    case STRING_RESULT:
      lex->yylval->udf=udf;
      return UDF_CHAR_FUNC;
    case REAL_RESULT:
      lex->yylval->udf=udf;
      return UDF_FLOAT_FUNC;
    case INT_RESULT:
      lex->yylval->udf=udf;
      return UDF_INT_FUNC;
    }
  }
#endif
  return 0;
}


/* make a copy of token before ptr and set yytoklen */

static inline LEX_STRING get_token(LEX *lex)
{
  LEX_STRING tmp;
  yyUnget();			// ptr points now after last token char
  tmp.length=lex->yytoklen=(uint) (lex->ptr - lex->tok_start);
  tmp.str=(char*) sql_strmake((char*) lex->tok_start,lex->yytoklen);
  return tmp;
}

/* Return an unescaped text literal without quotes */
/* Should maybe expand '\n' ? */
/* Fix sometimes to do only one scan of the string */

static char *get_text(LEX *lex)
{
  reg1 uchar c,sep;
  uint found_escape=0;

  sep= yyGetLast();			// String should end with this
  //lex->tok_start=lex->ptr-1;		// Remember '
  while (lex->ptr != lex->end_of_query)
  {
    c = yyGet();
#ifdef USE_BIG5CODE
    if (lex->ptr != lex->end_of_query && isbig5head(c))
    {
      reg1 uchar d = yyGet();
      if (lex->ptr != lex->end_of_query && isbig5tail(d))
	continue;
      else
	yyUnget();
    }
#endif
#ifdef USE_MB
    int l;
    if ((l = ismbchar(lex->ptr-1, lex->end_of_query))) {
	lex->ptr += l-1;
	continue;
    }
#endif
    if (c == '\\')
    {					// Escaped character
      found_escape=1;
      if (lex->ptr == lex->end_of_query)
	return 0;
      yySkip();
    }
    else if (c == sep)
    {
      if (c == yyGet())			// Check if two separators in a row
      {
	found_escape=1;			// dupplicate. Remember for delete
	continue;
      }
      else
	yyUnget();

      /* Found end. Unescape and return string */
      uchar *str,*end,*start;

      str=lex->tok_start+1;
      end=lex->ptr-1;
      start=(uchar*) sql_alloc((uint) (end-str)+1);
      if (!found_escape)
      {
	lex->yytoklen=(uint) (end-str);
	memcpy(start,str,lex->yytoklen);
	start[lex->yytoklen]=0;
      }
      else
      {
	uchar *to;
	for (to=start ; str != end ; str++)
	{
#ifdef USE_BIG5CODE
	  if (str[1] && isbig5code(*str,*(str+1)))
	  {
	    *to++= *str;
	    *to++= *++str;
	    continue;
	  }
#endif
#ifdef USE_MB
	  int l;
	  if ((l = ismbchar(str, end))) {
	      while (l--)
		  *to++ = *str++;
	      str--;
	      continue;
	  }
#endif
	  if (*str == '\\' && str+1 != end)
	  {
	    switch(*++str) {
	    case 'n':
	      *to++='\n';
	      break;
	    case 't':
	      *to++= '\t';
	      break;
	    case 'r':
	      *to++ = '\r';
	      break;
	    case 'b':
	      *to++ = '\b';
	      break;
	    case '0':
	      *to++= 0;			// Ascii null
	      break;
	    case 'Z':			// ^Z must be escaped on Win32
	      *to++='\032';
	      break;
	    case '_':
	    case '%':
	      *to++= '\\';		// remember prefix for wildcard
	      /* Fall through */
	    default:
	      *to++ = *str;
	      break;
	    }
	  }
	  else if (*str == sep)
	    *to++= *str++;		// Two ' or "
	  else
	    *to++ = *str;

	}
	*to=0;
	lex->yytoklen=(uint) (to-start);
      }
      if (lex->convert_set)
	lex->convert_set->convert((char*) start,lex->yytoklen);
      return (char*) start;
    }
  }
  return 0;					// unexpected end of query
}


/*
** Calc type of integer; long integer, longlong integer or real.
** Returns smallest type that match the string.
** When using unsigned long long values the result is converted to a real
** because else they will be unexpected sign changes because all calculation
** is done with longlong or double.
*/

static char *long_str="2147483647";
static const uint long_len=10;
static char *signed_long_str="-2147483648";
static char *longlong_str="9223372036854775807";
static const uint longlong_len=19;
static char *signed_longlong_str="-9223372036854775808";
static const uint signed_longlong_len=19;


inline static uint int_token(const char *str,uint length)
{
  if (length < long_len)			// quick normal case
    return NUM;
  bool neg=0;

  if (*str == '+')				// Remove sign and pre-zeros
  {
    str++; length--;
  }
  else if (*str == '-')
  {
    str++; length--;
    neg=1;
  }
  while (*str == '0' && length)
  {
    str++; length --;
  }
  if (length < long_len)
    return NUM;

  uint smaller,bigger;
  char *cmp;
  if (neg)
  {
    if (length == long_len)
    {
      cmp= signed_long_str+1;
      smaller=NUM;				// If <= signed_long_str
      bigger=LONG_NUM;				// If >= signed_long_str
    }
    else if (length < signed_longlong_len)
      return LONG_NUM;
    else if (length > signed_longlong_len)
      return REAL_NUM;
    else
    {
      cmp=signed_longlong_str+1;
      smaller=LONG_NUM;				// If <= signed_longlong_str
      bigger=REAL_NUM;
    }
  }
  else
  {
    if (length == long_len)
    {
      cmp= long_str;
      smaller=NUM;
      bigger=LONG_NUM;
    }
    else if (length < longlong_len)
      return LONG_NUM;
    else if (length > longlong_len)
      return REAL_NUM;
    else
    {
      cmp=longlong_str;
      smaller=LONG_NUM;
      bigger=REAL_NUM;
    }
  }
  while (*cmp && *cmp++ == *str++) ;
  return ((uchar) str[-1] <= (uchar) cmp[-1]) ? smaller : bigger;
}


// yylex remember the following states from the following yylex()
// STATE_EOQ ; found end of query
// STATE_OPERATOR_OR_IDENT ; last state was an ident, text or number
// 			     (which can't be followed by a signed number)

int yylex(void *arg)
{
  reg1	uchar	c=0;
  int	tokval;
  uint length;
  enum lex_states state,prev_state;
  LEX	*lex=current_lex;
  YYSTYPE *yylval=(YYSTYPE*) arg;

  lex->yylval=yylval;			// The global state
  lex->tok_start=lex->tok_end=lex->ptr;
  prev_state=state=lex->next_state; lex->next_state=STATE_OPERATOR_OR_IDENT;
  for (;;)
  {
    switch(state) {
    case STATE_OPERATOR_OR_IDENT:	// Next is operator or keyword
    case STATE_START:			// Start of token
      // Skipp startspace
      for (c=yyGet() ; (c && !isgraph(c)) ; c= yyGet())
      {
	if (c == '\n')
	  lex->yylineno++;
      }
      lex->tok_start=lex->ptr-1;	// Start of real token
      state= (enum lex_states) state_map[c];
      break;
    case STATE_ESCAPE:
      if (yyGet() == 'N')
      {					// Allow \N as shortcut for NULL
	yylval->lex_str.str="\\N";
	yylval->lex_str.length=2;
	return NULL_SYM;
      }
    case STATE_CHAR:			// Unknown or single char token
      yylval->lex_str.str=(char*) (lex->ptr=lex->tok_start);// Set to first char
      yylval->lex_str.length=1;
      c=yyGet();
      if (c != ')')
	lex->next_state= STATE_START;	// Allow signed numbers
      if (c == ',')
	lex->tok_start=lex->ptr;	// Let tok_start point at next item
      return((int) c);

    case STATE_IDENT:			// Incomplete keyword or ident
#if defined(USE_MB) && defined(USE_MB_IDENT)
      if (ismbhead(yyGetLast())) {
	  int l = ismbchar(lex->ptr-1, lex->end_of_query);
	  if (l == 0) {
	      state = STATE_CHAR;
	      continue;
	  }
	  lex->ptr += l - 1;
      }
      while (state_map[c=yyGet()] == STATE_IDENT ||
	     state_map[c] == STATE_NUMBER_IDENT)
      {
	if (ismbhead(c))
	{
	  int l;
	  if ((l = ismbchar(lex->ptr-1, lex->end_of_query)) == 0)
	    break;
	  lex->ptr += l-1;
	}
      }
#else
      while (state_map[c=yyGet()] == STATE_IDENT ||
	     state_map[c] == STATE_NUMBER_IDENT) ;
#endif
      length= (uint) (lex->ptr - lex->tok_start)-1;
      if (lex->ignore_space)
      {
	for ( ; (c && !isgraph(c)) ; c= yyGet());
      }
      if (c == '.' && (state_map[yyPeek()] == STATE_IDENT ||
		       state_map[yyPeek()] == STATE_NUMBER_IDENT))
	lex->next_state=STATE_IDENT_SEP;
      else
      {					// '(' must follow directly if function
	yyUnget();
	if ((tokval = find_keyword(lex,length,c == '(')))
	{
	  lex->next_state= STATE_START;	// Allow signed numbers
	  return(tokval);		// Was keyword
	}
	yySkip();			// next state does a unget
      }
      state = STATE_FOUND_IDENT;	// Found compleat ident
      break;

    case STATE_IDENT_SEP:		// Found ident and now '.'
      lex->next_state=STATE_IDENT_START;// Next is an ident (not a keyword)
      yylval->lex_str.str=(char*) lex->ptr;
      yylval->lex_str.length=1;
      c=yyGet();			// should be '.'
      return((int) c);

    case STATE_NUMBER_IDENT:		// number or ident which starts with num
      while (isdigit((c = yyGet()))) ;
      if (state_map[c] != STATE_IDENT)
      {					// Can't be identifier
	state=STATE_INT_OR_REAL;
	break;
      }
      if (c == 'e' || c == 'E')
      {
	if ((c=(yyGet())) == '+' || c == '-')
	{				// Allow 1E+10
	  if (isdigit(yyPeek()))	// Number must have digit after sign
	  {
	    yySkip();
	    while (isdigit(yyGet())) ;
	    yylval->lex_str=get_token(lex);
	    return(REAL_NUM);
	  }
	}
	yyUnget(); /* purecov: inspected */
      }
      else if (c == 'x' && (lex->ptr - lex->tok_start) == 2 &&
	  lex->tok_start[0] == '0' )
      {						// Varbinary
	while (isxdigit((c = yyGet()))) ;
	if ((lex->ptr - lex->tok_start) >= 4)
	{
	  yylval->lex_str=get_token(lex);
	  yylval->lex_str.str+=2;		// Skipp 0x
	  yylval->lex_str.length-=2;
	  lex->yytoklen-=2;
	  return (HEX_NUM);
	}
	yyUnget();
      }
      // fall through
    case STATE_IDENT_START:		// Incomplete ident
#if defined(USE_MB) && defined(USE_MB_IDENT)
      if (ismbhead(yyGetLast()))
      {
	int l = ismbchar(lex->ptr-1, lex->end_of_query);
	if (l == 0)
	{
	  state = STATE_CHAR;
	  continue;
	}
	lex->ptr += l - 1;
      }
      while (state_map[c=yyGet()] == STATE_IDENT ||
	     state_map[c] == STATE_NUMBER_IDENT)
      {
	if (ismbhead(c))
	{
	  int l;
	  if ((l = ismbchar(lex->ptr-1, lex->end_of_query)) == 0)
	    break;
	  lex->ptr += l-1;
	}
      }
#else
      while (state_map[c = yyGet()] == STATE_IDENT ||
	     state_map[c] == STATE_NUMBER_IDENT) ;
#endif
      if (c == '.' && (state_map[yyPeek()] == STATE_IDENT ||
		       state_map[yyPeek()] == STATE_NUMBER_IDENT))
	lex->next_state=STATE_IDENT_SEP;// Next is '.'
      // fall through

    case STATE_FOUND_IDENT:		// Complete ident
      yylval->lex_str=get_token(lex);
      return(IDENT);

    case STATE_SIGNED_NUMBER:		// Incomplete signed number
      if (prev_state == STATE_OPERATOR_OR_IDENT)
      {
	state= STATE_CHAR;		// Must be operator
	break;
      }
      if (!isdigit(c=yyGet()))
      {
	if (c != '.')
	{
	  state = STATE_CHAR;		// Return sign as single char
	  break;
	}
	yyUnget();			// Fix for next loop
      }
      while (isdigit(c=yyGet())) ;	// Incomplete real or int number
      if ((c == 'e' || c == 'E') && (yyPeek() == '+' || yyPeek() == '-'))
      {					// Real number
	yyUnget();
	c= '.';				// Fool next test
      }
      // fall through
    case STATE_INT_OR_REAL:		// Compleat int or incompleat real
      if (c != '.')
      {					// Found complete integer number.
	yylval->lex_str=get_token(lex);
	return int_token(yylval->lex_str.str,yylval->lex_str.length);
      }
      // fall through
    case STATE_REAL:			// Incomplete real number
      while (isdigit(c = yyGet())) ;

      if (c == 'e' || c == 'E')
      {
	c = yyGet();
	if (c != '-' && c != '+')
	{				// No exp sig found
	  state= STATE_CHAR;
	  break;
	}
	if (!isdigit(yyGet()))
	{				// No digit after sign
	  state= STATE_CHAR;
	  break;
	}
	while (isdigit(yyGet())) ;
      }
      yylval->lex_str=get_token(lex);
      return(REAL_NUM);

    case STATE_CMP_OP:			// Incomplete comparison operator
      if (state_map[yyPeek()] == STATE_CMP_OP)
	yySkip();
      if ((tokval = find_keyword(lex,(uint) (lex->ptr - lex->tok_start),0)))
      {
	lex->next_state= STATE_START;	// Allow signed numbers
	return(tokval);
      }
      state = STATE_CHAR;		// Something fishy found
      break;

    case STATE_BOOL:
      if (c != yyPeek())
      {
	state=STATE_CHAR;
	break;
      }
      yySkip();
      tokval = find_keyword(lex,2,0);	// Is a bool operator
      lex->next_state= STATE_START;	// Allow signed numbers
      return(tokval);

    case STATE_STRING:			// Incomplete text string
      if (!(yylval->lex_str.str = get_text(lex)))
      {
	state= STATE_CHAR;		// Read char by char
	break;
      }
      yylval->lex_str.length=lex->yytoklen;
      return(TEXT_STRING);

    case STATE_COMMENT:			//  Comment
      while ((c = yyGet()) != '\n' && c) ;
      yyUnget();			// Safety against eof
      state = STATE_START;		// Try again
      break;
    case STATE_LONG_COMMENT:		/* Long C comment? */
      if (yyPeek() != '*')
      {
	state=STATE_CHAR;		// Probable division
	break;
      }
      yySkip();				// Skip '*'
      if (yyPeek() == '!')		// MySQL command in comment
      {
	yySkip();
	state=STATE_START;
	lex->in_comment=1;
	break;
      }
      while (lex->ptr != lex->end_of_query &&
	     ((c=yyGet()) != '*' || yyPeek() != '/'))
      {
	if (c == '\n')
	  lex->yylineno++;
      }
      if (lex->ptr != lex->end_of_query)
	yySkip();			// remove last '/'
      state = STATE_START;		// Try again
      break;
    case STATE_END_LONG_COMMENT:
      if (lex->in_comment && yyPeek() == '/')
      {
	yySkip();
	lex->in_comment=0;
	state=STATE_START;
      }
      else
	state=STATE_CHAR;		// Return '*'
      break;
    case STATE_COLON:			// optional line terminator
      if (yyPeek())
      {
	state=STATE_CHAR;		// Return ';'
	break;
      }
      /* fall true */
    case STATE_EOL:
      lex->next_state=STATE_END;	// Mark for next loop
      return(END_OF_INPUT);
    case STATE_END:
      lex->next_state=STATE_END;
      return(0);			// We found end of input last time

      // Actually real shouldn't start
      // with . but allow them anyhow
    case STATE_REAL_OR_POINT:
      if (isdigit(yyPeek()))
	state = STATE_REAL;		// Real
      else
      {
	state = STATE_CHAR;		// return '.'
	lex->next_state=STATE_IDENT_START;// Next is an ident (not a keyword)
      }
      break;
    case STATE_USER_END:		// end '@' of user@hostname
      if (state_map[yyPeek()] != STATE_STRING)	// If user@'hostname'
	lex->next_state=STATE_HOSTNAME;	// Mark for next loop     
      yylval->lex_str.str=(char*) lex->ptr;
      yylval->lex_str.length=1;
      return((int) '@');
    case STATE_HOSTNAME:		// end '@' of user@hostname
      for (c=yyGet() ; isgraph(c) && c != ',' ; c= yyGet()) ;
      yylval->lex_str=get_token(lex);
      return(LEX_HOSTNAME);
    }
  }
}
