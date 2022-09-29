#ifndef _CFG_SCANNER_H_
#define _CFG_SCANNER_H_

typedef enum {
  TK_UNKNOWN=0,
  TK_EOF,
  TK_EOL,
  TK_COLON,
  TK_COMMA,
  TK_MINUS,
  TK_PLUS,
  TK_STRING,
  TK_NUMBER
} token_type_t;

typedef struct {
  token_type_t tk_type;
  void        *tk_value;
} token_struct_t;
typedef token_struct_t *token_t;

typedef struct {
  FILE          *_f;
  int            _ungetc_valid;
  char           _ungetc_char;
  uint           _tk_curr_valid;
  token_struct_t _tk_curr;
  char           _tk_string[100];
} CFG_SC_FILE;

CFG_SC_FILE *cfg_scopen(char *fn, char *type);
void         cfg_scclose(CFG_SC_FILE *scf);
token_t      cfg_get_token(CFG_SC_FILE *scf);
void         cfg_unget_token(CFG_SC_FILE *scf, token_t tk);

#endif

