#define LEX_STRING_MAX		32

#include "parse_y.tab.h"

typedef struct lex_data_s {
    char       *ptr;
    char       *buf;
    char       *last;
    int		pushback;
    char	word_too_long;
} lex_data_t;

int	parse_yylex(parse_YYSTYPE *yylval, void *lex_data);
int	parse_yylex_init(lex_data_t *ld, char *s);
