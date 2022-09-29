#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>		/* For varargs */
#include <stdlib.h>		/* For string conversion routines */
#include <string.h>
#include <bstring.h>
#include <ctype.h>		/* For character definitions */
#include <errno.h>
#include <fcntl.h>		/* for I/O functions */

#include "cfg_scanner.h"
#include "config.h"
#include "debug.h"

char get_tmpString[10000];

vt_struct_t agent_vt[] = {
  { "PollPeriod",           VT_INTEGER, OFFSET(agent_cfg_struct_t, ac_pollperiod) },
  { "FlashPeriod",          VT_INTEGER, OFFSET(agent_cfg_struct_t, ac_flashperiod) },
  { "PreRemovalCallout",    VT_STRING,  OFFSET(agent_cfg_struct_t, ac_preremovalco) },
  { "PostRemovalCallout",   VT_STRING,  OFFSET(agent_cfg_struct_t, ac_postremovalco) },
  { "PostInsertionCallout", VT_STRING,  OFFSET(agent_cfg_struct_t, ac_postinsertco) },
  { "StatusChangedCallout", VT_STRING,  OFFSET(agent_cfg_struct_t, ac_stat_change_co) },
  { "DebugLevel",           VT_INTEGER, OFFSET(agent_cfg_struct_t, ac_debug) },
  { "AllowRemoteRequests",  VT_INTEGER, OFFSET(agent_cfg_struct_t, ac_allow_remote) },
};

vt_t FindVTEinTable(char *s, vt_t vt)
{
  for (; vt->vt_name; ++vt) {
    if (strcmp(s, vt->vt_name) == 0)
      return(vt);
  }
  return(NULL);
}

int string_to_boolean(char *str)
{
  char *tmps, *s, *d;

  tmps = malloc(strlen(str));
  s = str;
  d = tmps;

  /* Convert to lower case */
  while (*s)
    *d++ = tolower(*s++);
  *d = '\0';

  if ((strcmp(tmps, "true") == 0) ||
      (strcmp(tmps, "on")   == 0))
    return(1);
  else
  if ((strcmp(tmps, "false") == 0) ||
      (strcmp(tmps, "off")   == 0))
    return(0);
  else
    return(-1);
}

void cfg_parseerror(const char *format, ...)
{
  va_list ap;
  char tmpString[1000];

  va_start(ap, format);
  sprintf(tmpString, "CONFIGURATION PARSE ERROR : %s", format);
  vfprintf(stderr, tmpString, ap);
  fflush(stderr);
  va_end(ap);
}

int cfg_read(char *filename, void *vars, vt_t vt)
{
  CFG_SC_FILE *scf;
  token_t tk;
  vt_t vte;
  int error = 0;

  if ((scf = cfg_scopen(filename, "r")) == NULL)
    return(-1);

  while (1) {
    tk = cfg_get_token(scf);
    if (tk->tk_type == TK_EOF)
      break;
    else
    if (tk->tk_type == TK_EOL)
      continue;
    else
    if (tk->tk_type == TK_STRING) {
      /* Find VTE */
      if ((vte = FindVTEinTable(tk->tk_value, vt)) == NULL) {
	cfg_parseerror("Unknown variable name '%s'\n", tk->tk_value);
	error = 1;
	goto done;
      }
      /* Read the colon */
      tk = cfg_get_token(scf);
      if (tk->tk_type != TK_COLON) {
	cfg_parseerror("':' expected after variable name '%s'\n", vte->vt_name);
	error = 1;
	goto done;
      }
      /* Read the values */
      switch(vte->vt_type) {
      case VT_INTEGER: 
	{
	  int int_val;
	  int neg_flag = 0, done_flag = 0;
	  
	  while (!done_flag) {
	    tk = cfg_get_token(scf);
	    switch (tk->tk_type) {
	    case TK_PLUS:
	      break;
	    case TK_MINUS:
	      neg_flag = 1;
	      break;
	    case TK_NUMBER:
	      int_val = (int)tk->tk_value * ((neg_flag) ? -1 : +1);
	      done_flag = 1;
	      break;
	    case TK_EOF:
	      cfg_parseerror("Unexpected EOF following variable name '%s'\n", vte->vt_name);
	      error = 1;
	      goto vt_integer_done;
	    case TK_EOL:
	      cfg_parseerror("Unexpected EOL following variable name '%s'\n", vte->vt_name);
	      error = 1;
	      goto vt_integer_done;
	    default:
	      cfg_parseerror("Unexpected token following variable name '%s'\n", vte->vt_name);
	      error = 1;
	      goto vt_integer_done;
	    }
	  }
	  while (1) {
	    tk = cfg_get_token(scf);
	    if ((tk->tk_type == TK_EOL) || (tk->tk_type == TK_EOF))
	      break;
	  }
	  *(int *)((__psunsigned_t)vars + vte->vt_offset) = int_val;
	}
      vt_integer_done:
	if (error)
	  goto done;
	break;
      case VT_STRING:
      case VT_BOOLEAN:
	{
	  char *string_val = NULL;
	  int int_val;
	  int done_flag = 0;
	  
	  while (!done_flag) {
	    tk = cfg_get_token(scf);
	    switch (tk->tk_type) {
	    case TK_STRING:
	      string_val = malloc(strlen(tk->tk_value) + 1);
	      strcpy(string_val, tk->tk_value);
	      done_flag = 1;
	      break;
	    case TK_EOF:
	      cfg_parseerror("Unexpected EOF following variable name '%s'\n", vte->vt_name);
	      error = 1;
	      goto vt_string_done;
	    case TK_EOL:
	      cfg_parseerror("Unexpected EOL following variable name '%s'\n", vte->vt_name);
	      error = 1;
	      goto vt_string_done;
	    default:
	      cfg_parseerror("Unexpected token following variable name '%s'\n", vte->vt_name);
	      error = 1;
	      goto vt_string_done;
	    }
	  }
	  while (1) {
	    tk = cfg_get_token(scf);
	    if ((tk->tk_type == TK_EOL) || (tk->tk_type == TK_EOF))
	      break;
	  }
	  if (vte->vt_type == VT_BOOLEAN) {
	    if ((int_val = string_to_boolean(string_val)) == -1) {
	      cfg_parseerror("Invalid boolean value '%s' following variable name '%s'\n", 
			     string_val, vte->vt_name);
	      error = 1;
	      goto vt_string_done;
	    }
	    *(int *)((__psunsigned_t)vars + vte->vt_offset) = int_val;
	  }
	  else {
	    *(char **)((__psunsigned_t)vars + vte->vt_offset) = string_val;
	  }
	}
      vt_string_done:
	if (error)
	  goto done;
	break;
      case VT_STRINGS:
	{
	  char **strings_val = NULL;
	  int size = 50, i = 0;
	  int done_flag = 0;

	  strings_val = malloc(size * sizeof(char *));
	  bzero(strings_val, size * sizeof(char *));
	  while (!done_flag) {
	    tk = cfg_get_token(scf);
	    switch (tk->tk_type) {
	    case TK_STRING:
	      strings_val[i] = malloc(strlen(tk->tk_value) + 1);
	      strcpy(strings_val[i], tk->tk_value);
	      if (++i == size) {
		size *= 2;
		strings_val = realloc(strings_val, size * sizeof(char *));
	      }
	      break;
	    case TK_COMMA:
	      break;
	    case TK_EOL:
	      done_flag = 1;
	      break;
	    case TK_EOF:
	      cfg_parseerror("Unexpected EOF following variable name '%s'\n", vte->vt_name);
	      error = 1;
	      goto vt_strings_done;
	    default:
	      cfg_parseerror("Unexpected token following variable name '%s'\n", vte->vt_name);
	      error = 1;
	      goto vt_strings_done;
	    }
	  }
	  strings_val[i] = NULL;
	  *(char ***)((__psunsigned_t)vars + vte->vt_offset) = strings_val;
	}
      vt_strings_done:
	if (error)
	  goto done;
	break;
      default:
	fatal("cfg_read() : Unexpected vt_type %d\n", vte->vt_type);
      }
    }
  }

 done:
  cfg_scclose(scf);
  return(error);
}
