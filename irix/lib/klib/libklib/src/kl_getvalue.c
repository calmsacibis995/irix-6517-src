#ident "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>

#define ARGLEN 40                      /* max length of argument             */

/*
 * Definitions for the do_math() routine.
 */
#define M_ADD      '+'
#define M_SUBTRACT '-'
#define M_MULTIPLY '*'
#define M_DIVIDE   '/'

/*
 * do_math() -- Calculate some math values based on a string argument
 *              passed into the function.  For example, if you use:
 * 
 *              0xffffc000*2+6/5-3*19-8
 * 
 *              And you will get the value 0xffff7fc0 back.  I could
 *              probably optimize this a bit more, but right now, it
 *              works, which is good enough for me.
 */
kaddr_t
do_math(char *str)
{
	int i = 0, j;
	char *buf, *loc;
	kaddr_t value1, value2;
	struct syment *sp;

	buf = (char *)kl_alloc_block((strlen(str) + 1), K_TEMP);
	sprintf(buf, "%s", str);
	for (i = strlen(str); i >= 0; i--) {
		if ((str[i] == M_ADD) || (str[i] == M_SUBTRACT)) {
			buf[i] = '\0';
			value1 = do_math(buf);
			value2 = do_math(&str[i+1]);
			kl_free_block((k_ptr_t)buf);
			if (str[i] == M_SUBTRACT) {
				return value1 - value2;
			} 
			else {
				return value1 + value2;
			}
		}
	}

	for (i = strlen(str); i >= 0; i--) {
		if ((str[i] == M_MULTIPLY) || (str[i] == M_DIVIDE)) {
			buf[i] = '\0';
			value1 = do_math(buf);
			value2 = do_math(&str[i+1]);
			kl_free_block((k_ptr_t)buf);
			if (str[i] == M_MULTIPLY) {
				return value1 * value2;
			} 
			else {
				if (value2 == 0) {
					/* handle divide by zero */
					if (DEBUG(KLDC_GLOBAL, 1)) {
						fprintf(KL_ERRORFP, "Divide by zero error!\n");
					}
					return 0;
				} 
				else {
					return value1 / value2;
				}
			}
		}
	}

	/*
	 * Otherwise, just process the value, and return it.
	 */
	sp = kl_get_sym(buf, K_TEMP);
	if (KL_ERROR) {
		kl_reset_error();
		value2 = strtoul(buf, &loc, 10);
		if (((!value2) && (buf[0] != '0')) || (*loc) ||
			(!strncmp(buf, "0x", 2)) || (!strncmp(buf, "0X", 2))) {
			value1 = GET_HEX_VALUE(buf);
		} 
		else {
			value1 = GET_DEC_VALUE(buf);
		}
	}
	else {
		kl_free_sym(sp);
		value1 = sp->n_value;
	}
	kl_free_block((k_ptr_t)buf);
	return value1;
}

/* 
 * kl_get_value() -- Translate numeric input strings 
 * 
 *   A generic routine for translating an input string (param) in a
 *   number of dfferent ways. If the input string is an equation
 *   (contains the characters '+', '-', '/', and '*'), then perform
 *   the math evaluation and return one of the following modes (if
 *   mode is passed):
 *
 *   0 -- if the resulting value is <= elements, if elements (number
 *        of elements in a table) is passed.  
 *
 *   1 -- if the first character in param is a pund sign ('#').
 *
 *   3 -- the numeric result of an equation.
 *
 *   If the input string is NOT an equation, mode (if passed) will be
 *   set in one of the following ways (depending on the contents of
 *   param and elements).
 *
 *   o When the first character of param is a pound sign ('#'), mode
 *     is set equal to one and the trailing numeric value (assumed to
 *     be decimal) is returned.
 *
 *   o When the first two characters in param are "0x" or "0X," or
 *     when when param contains one of the characers "abcdef," or when
 *     the length of the input value is eight characters. mode is set
 *     equal to two and the numeric value contained in param is
 *     translated as hexadecimal and returned.
 *
 *   o The value contained in param is translated as decimal and mode
 *     is set equal to zero. The resulting value is then tested to see
 *     if it exceeds elements (if passed). If it does, then value is
 *     translated as hexadecimal and mode is set equal to two.
 *
 *   Note that mode is only set when a pointer is passed in the mode
 *   paramater.
 *
 *   Also note that when elements is set equal to zero, any non-hex
 *   (as determined above) value not starting with a pound sign will
 *   be translated as hexadecimal (mode will be set equal to two) --
 *   IF the length of the string of characters is less than 16
 *   (kaddr_t).
 *
 */
int
kl_get_value(char *param, int *mode, int elements, kaddr_t *value)
{
	char *loc, *c;

	if (DEBUG(KLDC_FUNCTRACE, 4)) {
		fprintf(KL_ERRORFP, "kl_get_value: param=\"%s\", mode=0x%x, "
			"elements=%d, value=0x%x\n", param, mode, elements, value);
	}

	kl_reset_error();

	/* Check to see if we are going to need to do any math 
	 */
	if (strpbrk(param, "+-/*")) {
		if (!strncmp(param, "#", 1)) {
			*value = do_math(&param[1]);
			if (mode) {
				*mode = 1;
			}
		}
		else {
			*value = do_math(param);
			if (mode) {
				if (elements && (*value <= elements)) {
					*mode = 0;
				}
				else {
					*mode = 3;
				}
			}
		}
	}
	else {
		if (!strncmp(param, "#", 1)) {
			*value = strtoull(&param[1], &loc, 10);
			if (*loc) {
				KL_SET_ERROR_CVAL(KLE_INVALID_VALUE, param);
				return (1);
			}
			if (mode) {
				*mode = 1;
			}
		} 
		else if (!strncmp(param, "0x", 2) || 
				 !strncmp(param, "0X", 2) || strpbrk(param, "abcdef")) {
			*value = strtoull(param, &loc, 16);
			if (*loc) {
				KL_SET_ERROR_CVAL(KLE_INVALID_VALUE, param);
				return (1);
			}
			if (mode) {
				*mode = 2; /* HEX VALUE */
			}
		} 
		else if (elements || (strlen(param) < 16) || (strlen(param) > 16)) {
			*value = strtoull(param, &loc, 10);
			if (*loc) {
				KL_SET_ERROR_CVAL(KLE_INVALID_VALUE, param);
				return (1);
			}
			if (elements && (*value >= elements)) {
				*value = GET_HEX_VALUE(param);
				if (mode) {
					*mode = 2; /* HEX VALUE */
				}
			}
			else if (mode) {
				*mode = 0;
			} 
		}
		else {
			*value = strtoull(param, &loc, 16);
			if (*loc) {
				KL_SET_ERROR_CVAL(KLE_INVALID_VALUE, param);
				return (1);
			}
			if (mode) {
				*mode = 2; /* ASSUME HEX VALUE */
			}
		}
	}
	return (0);
}

