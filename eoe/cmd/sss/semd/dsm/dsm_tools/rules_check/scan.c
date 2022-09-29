#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <y.tab.h>
#include <parse.h>

extern parse_s_ptr yylval; 
extern parse_s_ptr parse_error;
extern FILE *stdofp,*stdifp,*stderrfp;

int characters_readin=0;

#define GETC()         getacharacter()

#define UNGETC(c)       \
{                       \
  ungetc(c,stdifp);     \
  characters_readin--;  \
}

struct keyword {
  const char *str;
  int tok;
};

static char buffer[NUMCHARS],*Pchar;

/* 
 * The strings in this structure should be in alphabetical order.
 */
static struct keyword keys[] = {
  { "ENV",     ENV_TOKEN,         },
  { "RETRY",   RETRY_TOKEN,       },
  { "TIME",    TIME_TOKEN,        },
  { "UID",     USER_TOKEN,        },
};

#define NUMKEYS (sizeof(keys)/sizeof(struct keyword))

static int getacharacter()
{
  int c=getc(stdifp);
  characters_readin++;
  return c;
}

/* 
 * Returns true for everything that does not signify the end of the current 
 * token.
 */
static int isendoftoken(int c)
{
   return (isspace(c) || c == ')' || c == ';' || c == EOF);
}

/* 
 * Returns true if the buffer is a floating point number.
 */
static int isfloat(char *buffer)
{
  int decimal_point=0;

  if(!buffer || !*buffer)
    return 0;

  while(*buffer && (isdigit(*buffer) || *buffer == '.'))
    {
      if(*buffer == '.')
	decimal_point++;
      buffer++;
    }
  if(*buffer || decimal_point > 1)
    return 0;

  return 1;
}

/* 
 * Returns true if the buffer is an integer.
 */
static int isint(char *buffer)
{
  if(!buffer || !*buffer)
    return 0;

  while(*buffer && (isdigit(*buffer)))
    {
      buffer++;
    }
  if(*buffer)
    return 0;

  return 1;
}

/* 
 * Returns true if the buffer is a hexadecimal number.
 */
static int ishex(char *buffer)
{
  if(!buffer || !*buffer)
    return 0;

  if(!*buffer || (*(buffer++) != '0'))
    return 0;
  if(!*buffer || ((*buffer != 'x') && (*buffer != 'X')))
    return 0;
  buffer++;

  while(*buffer && (isxdigit(*buffer)))
    {
      buffer++;
    }
  if(*buffer)
    return 0;

  return 1;
}

/* 
 * Returns true if the buffer is an octal number.
 */
static int isoctal(char *buffer)
{
  if(!buffer)
    return 0;

  if(!*buffer || *(buffer++)!='0')
    return 0;

  while(*buffer && (isdigit(*buffer) && *buffer < '9'))
    {
      buffer++;
    }
  if(*buffer)
    return 0;

  return 1;
}

/* 
 * Function called by bsearch.
 */
int compare_keywords(const void *k1, const void *k2)
{
  return strcasecmp(((struct keyword *)k1)->str,
		    ((struct keyword *)k2)->str);
}

/* 
 * Is the argument a keyword ?.
 */
static int iskeyword(char *str)
{
  struct keyword k,*Pk;

  k.str=str;
  Pk=bsearch(&k,keys,NUMKEYS,sizeof(struct keyword), compare_keywords);
  if(Pk)
    return Pk->tok;
  else
    return -1;
}

static int scan_error(char *buffer)
{
  int len;

  yylval=parse_error;

  if(!buffer)
    return ERROR_TOKEN;

  len=strlen(buffer);

  while(len)
    UNGETC(buffer[--len]);

  return ERROR_TOKEN;
}
  
/*
 * Main function of the scanner.
 */
int yylex()
{
  int c;
  static char *symbuf;
  static int length = 0;
  int i;
	
  if(!length)
    {
      /* 
       * initially allocate a buffer long enough for a 40 character
       * variable name.
       */
      length = NAME_LENGTH, symbuf = (char *)malloc(length+1);
    }
	
  while ((c=GETC()) == ' ' || c == '\t' || c=='\\' || c == '\n')
    {
      if (c=='\\') 
	{	       /* ignore '\\' followed immediately by '\n'. */
	  c=GETC();
	  if(c!='\n')
	    {
	      UNGETC(c);
	      c='\\';
	      break;
	    }
	}
      if (c == '\t') 
	c = ' ';
		    
    }
			
  /*
   * process numbers. 
   */
  yylval=NULL;
  if (isdigit(c) || c == '.')
    {
      __uint64_t number;
      float f;
      static char buffer[1024];
      int i=0;
		
      UNGETC(c);
      /* 
       * Get everything till the token ends.
       */
      while(!isendoftoken(c=GETC()))
	buffer[i++]=c;
      buffer[i]=0;
      UNGETC(c);

      if(isoctal(buffer))
	 {
	   if(sscanf(buffer,"%llo", &number) != 1)
	     {
	       return scan_error(buffer);
	     }
	 }
      else if(isint(buffer))
	{
	  if(sscanf(buffer,"%lld", &number) != 1)
	    {
	      return scan_error(buffer);
	    }
	}
      else if (ishex(buffer))
	{
	  if(sscanf(buffer,"%llx", &number) != 1)
	    {
	      return scan_error(buffer);
	    }
	}
      else if (isfloat(buffer))
	{
	  if(sscanf(buffer,"%f", &f) != 1)
	    {
              return scan_error(buffer);
	    }
	      
	  yylval       = (parse_s_ptr)calloc(sizeof(parse_s),1);
	  yylval->FLT  = f;
	  yylval->type = FLOAT;
	  
	  return FLOAT;
	}
      else
	{
	  return scan_error(buffer);
	}
      yylval       = (parse_s_ptr)calloc(sizeof(parse_s),1);
      yylval->VAL  = number;
      yylval->type = INT;
		
      return INT;
    }

  /*
   * Look for commands and KEYWORDS...
   * Anything else, try to pass it off as a symbol.
   */
  if(isalpha(c) || c == '\\' || c == '.' || c == '/' || c == '%')
    {
#if (PARSE_DEBUG > 1)
      fprintf(stderrfp,"SYM[0]=%c\n",c);
#endif
      /* 
       * Reuse the symbuf buffer..
       */
      i=0;
      do
	{
	  /*
	   * If buffer is full make it bigger.
	   */
	  if(i== length)
	    {
	      length *= 2;
	      symbuf = (char *)realloc(symbuf,length+1);
	    }
	  symbuf[i++] = c;
	  c = GETC();
	} while (!isendoftoken(c) && c != '(');

      if(c==EOF)
	goto eof;

      UNGETC(c);
      symbuf[i] = '\0';
		
      if((i=iskeyword(symbuf))>0)
	{
	  return i;
	}

      yylval       = (parse_s_ptr)calloc(sizeof(parse_s),1);
      yylval->PTR  = strdup(symbuf);
      yylval->type = SYMBOL;
      
      return SYMBOL;
    }

  /* 
   * Process strings.
   */
  if(c == '"')
    {
      /* 
       * Reuse the symbuf buffer..
       */
      i=0;
      symbuf[i++]='"';
      while ((c=GETC()) && c != EOF && c != '"')
	{
	  /*
	   * If buffer is full make it bigger.
	   */
	  if(i== length)
	    {
	      length *= 2;
	      symbuf = (char *)realloc(symbuf,length+1);
	    }
	  symbuf[i++] = c;
	  if(c=='\\')
	    {
	      c=GETC();
	      if(c!=EOF)
		symbuf[i++] = c;
	      else
		break;
	    }
	} 
      symbuf[i++]='"';
      symbuf[i] = '\0';

      if(c==EOF)
	goto eof;

      yylval       = (parse_s_ptr)calloc(sizeof(parse_s),1);
      yylval->PTR  = strdup(symbuf);
      yylval->type = STRING;
		
      return STRING;
    }

  if(c == '\'')
    {
      /* 
       * Reuse the symbuf buffer..
       */
      i=0;
      symbuf[i++]='"';
      while ((c=GETC()) && c != EOF && c != '\'')
	{
	  /*
	   * If buffer is full make it bigger.
	   */
	  if(i== length)
	    {
	      length *= 2;
	      symbuf = (char *)realloc(symbuf,length+1);
	    }
	  symbuf[i++] = c;
	  if(c=='\\')
	    {
	      c=GETC();
	      if(c!=EOF)
		symbuf[i++] = c;
	      else
		break;
	    }
	} 
		
      symbuf[i++]='"';
      symbuf[i] = '\0';

      if(c==EOF)
	goto eof;

      yylval       = (parse_s_ptr)calloc(sizeof(parse_s),1);
      yylval->PTR  = strdup(symbuf);
      yylval->type = STRING;
		
      return STRING;
    }


eof:	
  if(c == EOF)
    {
      /*
       * Exit out of the if/while/for loops..
       */
      exit(-1);
    }
	
  return c;
}


