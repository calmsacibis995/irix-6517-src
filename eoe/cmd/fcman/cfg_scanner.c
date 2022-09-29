#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <fcntl.h>		/* for I/O functions */
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include "cfg_scanner.h"

#define isident(_c) \
  (isalpha(_c) || isdigit(_c) || (_c == '-') || (_c == '_') || (_c == '.'))
#define GETC(_scf) \
  (_cfg_char_interface(0, _scf))
#define UNGETC(_scf, _ch) \
  (_cfg_char_interface(1, _scf, _ch))

int _cfg_char_interface(int cmd, CFG_SC_FILE *scf, ...)
{
  va_list ap;
  char ch;

  switch (cmd) {
  case 0:			/* getc() */
    if (scf->_ungetc_valid) {
      scf->_ungetc_valid = 0;
      return(scf->_ungetc_char);
    }
    ch = getc(scf->_f);
    return(ch);
  case 1:			/* ungetc() */
    va_start(ap, scf);
    ch = va_arg(ap, int);
    va_end(ap);
    if (scf->_ungetc_valid)
      fatal("_cfg_char_available() : scf->_ungetc_valid = 1.\n");
    scf->_ungetc_valid = 1; 
    scf->_ungetc_char = ch;
    return(0);
  default:
    fatal("_cfg_char_interface() : invalid cmd.\n");
  }
  /* NOTREACHED */
}

void cfg_unget_token(CFG_SC_FILE *scf, token_t tk)
{
  if (tk != &scf->_tk_curr)
    fatal("cfg_unget_token() : tk != &scf->_tk_curr\n");
  if (scf->_tk_curr_valid != 0)
    fatal("cfg_unget_token() : scf->_tk_curr_valid != 0\n");
  scf->_tk_curr_valid = 1;
}

token_t cfg_get_token(CFG_SC_FILE *scf)
{
  char ch, *p;
  int repeat = 0;

  if (scf->_tk_curr_valid) {
    scf->_tk_curr_valid = 0;
    return(&scf->_tk_curr);
  }

  do {
    repeat = 0;
    ch = GETC(scf);
    /* ch = tolower(ch); */

    if (isalpha(ch) || (ch == '/')) {
      p = scf->_tk_string;
      do {
	*p++ = ch;
	ch = GETC(scf);
	/* ch = tolower(ch); */
      } while (isident(ch) || (ch == '/'));
      UNGETC(scf, ch);
      *p = '\0';
      scf->_tk_curr.tk_value = scf->_tk_string;
      scf->_tk_curr.tk_type = TK_STRING;
    }
    else
    if (isdigit(ch)) {
      int hex_flag = 0;
      p = scf->_tk_string;
      if (ch == '0') {
	*p++ = ch;
	ch = GETC(scf);
	/* ch = tolower(ch); */
	if (ch == 'x') {
	  hex_flag = 1;
	  ch = GETC(scf);
	}
      }
      while ((hex_flag && isxdigit(ch)) ||
	     isdigit(ch)) {
	*p++ = ch;
	ch = GETC(scf);
	/* ch = tolower(ch); */
      }
      UNGETC(scf, ch);
      *p = '\0';
      if (hex_flag)
	sscanf(scf->_tk_string, "%x", &(scf->_tk_curr.tk_value));
      else
	sscanf(scf->_tk_string, "%d", &(scf->_tk_curr.tk_value));
      scf->_tk_curr.tk_type = TK_NUMBER;
    }
    else
    if (ch == '"') {
      p = scf->_tk_string;
      ch = GETC(scf);
      while ((ch != '"') && (ch != '\n') && (ch != (char)EOF)) {
	*p++ = ch;
	if (ch == '\\')
	  *p++ = GETC(scf);
	ch = GETC(scf);
      }
      *p = '\0';
      if ((ch == '\n') && (ch == (char)EOF))
	UNGETC(scf, ch);
      scf->_tk_curr.tk_type = TK_STRING;
      scf->_tk_curr.tk_value = (void *)malloc(strlen(scf->_tk_string) + 1);
      strcpy(scf->_tk_curr.tk_value, scf->_tk_string);
    }
    else
      switch(ch) {
      case '#' :
	while (1) {
	  ch = GETC(scf);
	  if (ch == '\n') {
	    scf->_tk_curr.tk_type = TK_EOL;
	    break;
	  }
	  if (ch == (char)EOF) {
	    scf->_tk_curr.tk_type = TK_EOF;
	    break;
	  }
	}
	break;
      case '+' :
	scf->_tk_curr.tk_type = TK_PLUS;
	break;
      case '-' :
	scf->_tk_curr.tk_type = TK_MINUS;
	break;
      case ',' :
	scf->_tk_curr.tk_type = TK_COMMA;
	break;
      case ':' :
	scf->_tk_curr.tk_type = TK_COLON;
	break;
      case '\n':
	scf->_tk_curr.tk_type = TK_EOL;
	break;
      case (char)EOF :
	scf->_tk_curr.tk_type = TK_EOF;
	break;
      case '\\' :
	while ((ch = GETC(scf)) != '\n')
	  ;
	repeat = 1;
	break;
      case ' ' :
      case '\t' :
	repeat = 1;
	break;
      default:
	scf->_tk_curr.tk_type = TK_UNKNOWN;
      }
  } while(repeat);
  
  return (&scf->_tk_curr);
}


CFG_SC_FILE	*cfg_scopen(char *fn, char *type)
{
  CFG_SC_FILE *scf;
  FILE *f;

  if ((f = fopen(fn, type)) == NULL)
    return(NULL);

  scf = (CFG_SC_FILE *)malloc(sizeof(CFG_SC_FILE));
  bzero(scf, sizeof(CFG_SC_FILE));
  scf->_f = f;
  return(scf);
}

void cfg_scclose(CFG_SC_FILE *scf)
{
  fclose(scf->_f);
  bzero(scf, sizeof(CFG_SC_FILE));
  free(scf);
}


