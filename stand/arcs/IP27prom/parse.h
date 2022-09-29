#include <hwreg.h>

#include "rtc.h"

#define PARSE_OPER_MAX		64

#define PARSE_OPER_END		((void (*)()) (__psunsigned_t) 0)
#define PARSE_OPER_REPEAT	((void (*)()) (__psunsigned_t) 1)
#define PARSE_OPER_LOOP		((void (*)()) (__psunsigned_t) 2)
#define PARSE_OPER_WHILE	((void (*)()) (__psunsigned_t) 3)
#define PARSE_OPER_FOR		((void (*)()) (__psunsigned_t) 4)
#define PARSE_OPER_IF		((void (*)()) (__psunsigned_t) 5)
#define PARSE_OPER_TIME		((void (*)()) (__psunsigned_t) 6)

#define PARSE_ARG_MAX		16

typedef struct parse_oper_s {
    void		(*func)();
    __uint64_t		arg;
} parse_oper_t;

typedef struct parse_data_s {
    struct reg_struct  *regs;
    struct flag_struct *flags;
    hwreg_set_t	       *last_regset;
    hwreg_t	       *last_hwreg;
    parse_oper_t	oper[PARSE_OPER_MAX];
    int			oper_count;
    parse_oper_t       *oper_ptr;
    __uint64_t		arg_stack[PARSE_ARG_MAX];
    int			arg_ptr;
    int			prompt_col;
    rtc_time_t		next;
} parse_data_t;

void		parse(parse_data_t *pd, char *s, int prompt_col);
void		parse_push(parse_data_t *pd, __uint64_t value);
__uint64_t	parse_pop(parse_data_t *pd);
void		parse_push_string(parse_data_t *pd, char *s);
void		parse_pop_string(parse_data_t *pd, char *buf);
