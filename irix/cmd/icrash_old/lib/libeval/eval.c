#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libeval/RCS/eval.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#define _KERNEL  1
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <libelf.h>
#include <dwarf.h>
#include <libdwarf.h>
#include <syms.h>
#include <sym.h>

#include "icrash.h"
#include "extern.h"
#include "eval.h"
#include "variable.h"
#include "dwarflib.h"

/* Global variables
 */
static int logical_flag;

/* The base_type[] array is necessary because base types are not 
 * directly accessible from the Dwarf database. Three of the entries 
 * have to be modified at startup if the vmcore image is from a 
 * 32-bit system. They are the entries for "long", "signed long", 
 * and "unsigned long". On a 32-bit core dump, these types take up 
 * four bytes. On a 64-bit system, they take up eight bytes.
 */
base_t base_type[] = {
	/*---------------------------------------------------------*/
	/* NAME 		    	ACTUAL_NAME  BYTE_SIZE   ENCODING  */
	/*---------------------------------------------------------*/
	{ "char", 				(char *)0,           1,         6  },
	{ "signed char", 		"char",				 1,         6  },
	{ "unsigned char", 		(char *)0,           1,         8  },
	{ "short", 				(char *)0,           2,         5  },
	{ "signed short", 		"short",             2,         5  },
	{ "unsigned short", 	(char *)0,           2,         7  },
	{ "int", 				(char *)0,           4,         5  },
	{ "signed int", 		"int",               4,         5  },
	{ "signed", 		    "int",               4,         5  },
	{ "unsigned int", 		(char *)0,           4,         7  },
	{ "unsigned", 			"unsigned int",      4,         7  },
	{ "long", 				(char *)0,           8,         5  },
	{ "signed long", 		"long",              8,         5  },
	{ "unsigned long", 		(char *)0,           8,         7  },
	{ "long long", 			(char *)0,           8,         5  },
	{ "signed long long", 	"long long",         8,         5  },
	{ "unsigned long long",	(char *)0,           8,         7  },
	{ (char *)0,   			(char *)0,           0,         0  }
};

/*
 * get_base_type()
 */
base_t *
get_base_type(char *s)
{
	int i, count, blank_count = 0, len, gap = 0;
	char *ptr, *first, *last, *gapp = 0, *cp, typename[128] = "";

	strcat (typename, s);
	ptr = typename;
	len = strlen(typename);

	/* Strip off any leading "white space" (spaces and tabs)
	 */
	while ((*ptr == ' ') || (*ptr == '\t')) {
		blank_count++;
		ptr++;
	}
	len -= blank_count;
	bcopy (ptr, typename, len);
	typename[len] = 0;

	/* Compress any "white space" between words.
	 */
	ptr = typename;
	blank_count = 0;
	while (*ptr && (*ptr != '*')) {
		if ((*ptr == ' ') || (*ptr == '\t')) {
			cp = ptr + 1;
			blank_count = 0;
			while ((*cp == ' ') || (*cp == '\t')) {
				blank_count++;
				cp++;
			}
			if ((cp - ptr) > 1) {
				bcopy(cp, ptr + 1, len - (cp - typename));
				len -= blank_count;
			}
		}
		ptr++;
	}
	*ptr = 0;
	for (i = 0; base_type[i].name; i++) {
		if (!strcmp(typename, base_type[i].name)) {
			break;
		}
	}
	if (base_type[i].name) {
		return(&base_type[i]);
	}
	else {
		return((base_t*)NULL);
	}
}

/*
 * base_typestr()
 */
char *
base_typestr(type_t *tp, int flags)
{
	int i, len = 0, ptrcnt = 0;
	type_t *t;
	base_t *btp;
	char *tsp, *c;

	t = tp;
    while (t->flag == POINTER_FLAG) {
        ptrcnt++;
        t = t->t_next;
    }
    btp = (base_t *)t->t_ptr;

	if (btp->actual_name) {
		len = strlen(btp->actual_name) + ptrcnt + 3;
	}
	else {
		len = strlen(btp->name) + ptrcnt + 3;
	}
	tsp = (char *)ALLOC_BLOCK(len, flags);
	c = tsp;
	*c++ = '(';
	if (btp->actual_name) {
		strcat(c, btp->actual_name);
		c += strlen(btp->actual_name);
	}
	else {
		strcat(c, btp->name);
		c += strlen(btp->name);
	}
	for (i = 0; i < ptrcnt; i++) {
		*c++ = '*';
	}
	*c++ = ')';
	*c = 0;
	return(tsp);
}

/*
 * get_type() -- Convert a typecast string into a type. 
 * 
 *   Returns a pointer to a struct containing type information (dwarf,
 *   base, etc.) The type of struct returned is indicated by the 
 *   contents of type. If the typecast contains an asterisk, set
 *   ptr_type equal to one, otherwise set it equal to zero.
 */
type_t *
get_type(char *s, int flags)
{
	static int symtable_type = -1;
	int len, type = 0;
	char *cp, typename[128];
	base_t *bp;
	type_t *t, *head, *last;
	dw_type_t *dtp;
	dw_type_info_t *dti;

	head = last = (type_t *)NULL;

	if (symtable_type < 0) {
		symtable_type = DWARF_TYPE_FLAG;
	}

	if (!strncmp(s, "struct", 6)) {
		if (cp = strpbrk(s + 7, " \t*")) {
			len = cp - (s + 7);
		}
		else {
			len = strlen(s + 7);
		}
		bcopy (s + 7, typename, len);
		typename[len] = 0;
		if (symtable_type == DWARF_TYPE_FLAG) {
			if (!(dtp = dw_lkup(stp, typename, DW_TAG_structure_type))) {
				return ((type_t *)NULL);
			}
			type = DWARF_TYPE_FLAG;
		} 
	}
	else if (!strncmp(s, "union", 5)) {
		if (cp = strpbrk(s + 6, " \t*")) {
			len = cp - (s + 6);
		}
		else {
			len = strlen(s + 6);
		}
		bcopy (s + 6, typename, len);
		typename[len] = 0;
		if (symtable_type == DWARF_TYPE_FLAG) {
			if (!(dtp = dw_lkup(stp, typename, DW_TAG_union_type))) {
				return ((type_t *)NULL);
			}
			type = DWARF_TYPE_FLAG;
		} 
	}
	else if (bp = get_base_type(s)) {
		type = BASE_TYPE_FLAG;
	}
	else {
		/* Check to see if this is a typedef
		 */
		if (cp = strpbrk(s, " \t*)")) {
			len = cp - s;
		}
		else {
			len = strlen(s);
		}
		bcopy (s, typename, len);
		typename[len] = 0;
		if (symtable_type == DWARF_TYPE_FLAG) {
			if (!(dtp = dw_lkup(stp, typename, DW_TAG_typedef))) {
				return ((type_t *)NULL);
			}
			type = DWARF_TYPE_FLAG;
		} 
	}

	/* check to see if this cast is a pointer to a type (or a pointer
	 * to a pointer to a type, etc.)
	 */
	cp = s;
	while (cp = strpbrk(cp, "*")) {
		t = (type_t *)ALLOC_BLOCK(sizeof(type_t), flags);
		t->flag = POINTER_FLAG;
		if (last) {
			last->t_next = t;
			last = t;
		}
		else {
			head = last = t;
		}
		cp++;
	}

	/* Allocate a type block that will point to the type specific
	 * (BASE or DWARF) record
	 */
	t = (type_t *)ALLOC_BLOCK(sizeof(type_t), flags);
	t->flag = type;

	switch (t->flag) {

		case BASE_TYPE_FLAG :
			t->t_ptr = (void *)bp;
			break;

		case DWARF_TYPE_FLAG :
			dti = (dw_type_info_t *)ALLOC_BLOCK(sizeof(*dti), flags);
			if (!dw_type_info(dtp->dt_off, dti, flags)) {
				return((type_t*)NULL);
			}
			t->t_ptr = (void *)dti;
			break;

		default :
			free_type(head);
			return((type_t*)NULL);
	}

	if (last) {
		last->t_next = t;
	}
	else {
		head = t;
	}
	return(head);
}

/*
 * free_type()
 */
void
free_type(type_t *head) 
{
	type_t *t0, *t1;

	t0 = head;
	while(t0) {
		if (t0->flag == POINTER_FLAG) {
			t1 = t0->t_next;
			free_block((k_ptr_t)t0);
			t0 = t1;
		}
		else {
			if (is_temp_block(t0->t_ptr)) {
				if (t0->flag == DWARF_TYPE_FLAG) {
					dw_free_type_info((dw_type_info_t*)t0->t_ptr);
				}
				else {
					free_block((k_ptr_t)t0->t_ptr);
				}
			}
			free_block((k_ptr_t)t0);
			t0 = (type_t *)NULL;
		}
	}
}

/*
 * member_to_type() -- Converts a struct/union member to a type
 *
 *   With dwarf members, all DW_TAG_pointer dies are converted to
 *   type_t structs with POINTER_FLAG set. The original dw_type_info_t
 *   struct pointer is linked to the t_link field -- since it contains
 *   information necessary for the dowhatis() command.
 */
type_t *
member_to_type(void *x, int ntype, int flags)
{
	dw_die_t *dp;
	dw_type_info_t *t, *dti;
	type_t *tp, *head = (type_t *)NULL, *last = (type_t *)NULL;

	if (ntype == DWARF_TYPE_FLAG) {

		t = (dw_type_info_t *)x;

		/* Make sure this is a DW_TAG_member
		 */
		if (t->t_tag != DW_TAG_member) {
			return((type_t *)NULL);
		}

		dp = t->t_actual_type;
		while (dp && dp->d_tag == DW_TAG_pointer_type) {
			tp = (type_t *)ALLOC_BLOCK(sizeof(type_t), flags);
			tp->flag = POINTER_FLAG;
			if (last) {
				last->t_next = tp;
				last = tp;
			}
			else {
				head = last = tp;
			}
			dp = dp->d_next;
		}

		/* If We step past all the pointer dies and don't point at anything,
		 * this must be a void pointer. Setup a VOID type struct so that we
		 * can maintain a pointer to the type info in dw_type_info_t struct.
		 */
		if (!dp) {
			tp = (type_t *)ALLOC_BLOCK(sizeof(type_t), flags);
			tp->flag = VOID_FLAG;
			tp->t_ptr = t;
			if (last) {
				last->t_next = tp;
				last = tp;
			}
			else {
				head = last = tp;
			}
			return(head);
		}

		dti = (dw_type_info_t *)ALLOC_BLOCK(sizeof(*dti), flags);
		switch (dp->d_tag) {

			case DW_TAG_structure_type :

				/* Check to see if this die points to the struct 
				 * members. If not, we need to search for the actual
				 * struct definition.
				 */
				if (!dp->d_sibling_off) {
					dw_type_t *dtp;

					if (!(dtp = dw_lkup(stp, dp->d_name, 
										DW_TAG_structure_type))) {

						/* Information is not available for this
						 * struct. Make the actual type be dp. It's
						 * better to have something rather than 
						 * nothing.
						 */
						t->t_actual_type = t->t_type;
						tp = (type_t *)ALLOC_BLOCK(sizeof(type_t), flags);
						tp->flag = DWARF_TYPE_FLAG;
						tp->t_ptr = (void *)t;
						if (last) {
							last->t_next = tp;
						}
						else {
							head = tp;
						}
						return(head);
					}
					if (!dw_type_info(dtp->dt_off, dti, flags)) {
						return((type_t *)NULL);
					}
				}
				else {
					if (!dw_type_info(dp->d_off, dti, flags)) {
						return((type_t *)NULL);
					}
				}
				break;

			case DW_TAG_union_type :

				/* Check to see if this die points to the union 
				 * members. If not, we need to search for the actual
				 * union definition.
				 */
				if (!dp->d_sibling_off) {
					dw_type_t *dtp;

					if (!(dtp = dw_lkup(stp, 
							dp->d_name, DW_TAG_union_type))) {

						/* Information is not available for this
						 * union. Make the actual type be dp. It's
						 * better to have something rather than 
						 * nothing.
						 */
						t->t_actual_type = t->t_type;
						tp = (type_t *)ALLOC_BLOCK(sizeof(type_t), flags);
						tp->flag = DWARF_TYPE_FLAG;
						tp->t_ptr = (void *)t;
						if (last) {
							last->t_next = tp;
						}
						else {
							head = tp;
						}
						return(head);
					}
					if (!dw_type_info(dtp->dt_off, dti, flags)) {
						return((type_t *)NULL);
					}
				}
				else {
					if (!dw_type_info(dp->d_off, dti, flags)) {
						return((type_t *)NULL);
					}
				}
				break;

			default :
				if (!dw_type_info(dp->d_off, dti, flags)) {
					return((type_t *)NULL);
				}
				break;
		}

		/* Check to see if the real type is a DW_TAG_base_type. If it is,
		 * then check to see if t_bit_offset and/or t_bit_size are set in
		 * the original dw_type_info_s struct. If they are, copy the values
		 * over to dti.
		 */
		if (dti->t_tag == DW_TAG_base_type) {
			if (t->t_bit_offset) {
				dti->t_bit_offset = t->t_bit_offset;
			}
			if (t->t_bit_size) {
				dti->t_bit_size = t->t_bit_size;
			}
			dti->t_byte_size = t->t_byte_size;
		}

		/* Save the pointer to the info about the actual member (for
		 * use by dowhatis() )
		 */
		dti->t_link = t;

		tp = (type_t *)ALLOC_BLOCK(sizeof(type_t), flags);
		tp->flag = DWARF_TYPE_FLAG;
		tp->t_ptr = (void *)dti;
		if (last) {
			last->t_next = tp;
		}
		else {
			head = tp;
		}
		return(head);
	} 
	return((type_t *)NULL);
}

/*
 * is_keyword()
 */
int
is_keyword(char *c, int len)
{
	/* This is temporary. Really need to lookup string to see if
	 * it is a type or typedef.
	 */
	if (!strncmp(c, "struct", len)) {
		return(1);
	}
	else if (!strncmp(c, "union", len)) {
		return(1);
	}
	else if (!strncmp(c, "char", len)) {
		return(1);
	}
	else if (!strncmp(c, "short", len)) {
		return(1);
	}
	else if (!strncmp(c, "int", len)) {
		return(1);
	}
	else if (!strncmp(c, "long", len)) {
		return(1);
	}
	else if (!strncmp(c, "unsigned", len)) {
		return(1);
	}
	else if (!strncmp(c, "signed", len)) {
		return(1);
	}
	else {
		char *c1;

		c1 = (char*)alloc_block(len+1, B_TEMP);
		strncat(c1, c, len);
		c1[len] = 0;
		switch (symbol_table) {

			case DWARF_TYPE_FLAG:
				if (dw_lkup(stp, c1, DW_TAG_typedef)) {
					free_block((k_ptr_t)c1);
					return(1);
				}
				break;
		}
		free_block((k_ptr_t)c1);

	}
	return(0);
}

/* 
 * expand_variables()
 */
char *
expand_variables(char *exp, int flags)
{
	int len;
	char vname[64], *s, *e, *v, *newexp;
	variable_t *vp;

	newexp = (char *)ALLOC_BLOCK(256, flags);

	e = exp;
	v = strchr(e, '$');
	while (v) {
		vname[0] = 0;
		strncat(newexp, e, (v - e));
		if (s = strpbrk((v + 1), " .\t+-*/()[]|~!$&%^<>?:&=^\"\'")) {
			len = (uint)s - (uint)v + 1;
		}
		else {
			len = strlen(v) + 1;
		}

		strncpy(vname, v, len);
		vname[len -1] = 0;
		vp = find_variable(vtab, vname, V_TYPEDEF|V_STRING);
		if (!vp) {
			return((char *)NULL);
		}

		/* If this is a typedef, then make sure the typestr is between
		 * an open and close parenthesis. Otherwise, just include the 
		 * string.
		 */
		if (vp->v_flags & V_TYPEDEF) {
			strcat(newexp, "(");
			strcat(newexp, vp->v_typestr);
			strcat(newexp, ")");
		}
		else {
			strcat(newexp, vp->v_exp);
		}

		if (e = s) {
			v = strchr(e, '$');;
		}
		else {
			v = (char *)NULL;
		}
	}
	if (e) {
		strcat(newexp, e);
	}
	return(newexp);
}

/*
 * get_token_list()
 */
token_t *
get_token_list(char *str, int *error)
{
	int i, paren_count = 0;
	char *cp;
	token_t *tok, *tok_head = (token_t*)NULL, *tok_last = (token_t*)NULL;

	cp = str;
	*error = 0;

	while (*cp) {

		/* Skip past any "white space" (spaces and tabs).
		 */
		switch (*cp) {
			case ' ' :
			case '\t' :
			case '`' :
				cp++;
				continue;

			case '\'' :
				break;

			case '\"' :
				break;

			default :
				break;
		}

		/* Allocate space for the next token
		 */
		tok = (token_t *)alloc_block(sizeof(token_t), B_TEMP);
		tok->ptr = cp;

		switch(*cp) {

			/* Check for operators
			 */
			case '+' :
				if (*((char*)cp + 1) == '+') {

					/* We aren't doing asignment here, so the ++ operator
					 * is not considered valid.
					 */
					*error = E_BAD_OPERATOR;
					return (tok_last);
				}
				else if (!tok_last ||
				  (tok_last->operator && (tok_last->operator != CLOSE_PAREN))) {
					tok->operator = UNARY_PLUS;
				}
				else {
					tok->operator = ADD;
				}
				break;

			case '-' :
				if (*((char*)cp + 1) == '-') {

					/* We aren't doing asignment here, so the -- operator
					 * is not considered valid.
					 */
					*error = E_BAD_OPERATOR;
					return (tok_last);
				}
				else if (*((char*)cp + 1) == '>') {
					tok->operator = RIGHT_ARROW;
					cp++;
				}
				else if (!tok_last ||
				  (tok_last->operator && (tok_last->operator != CLOSE_PAREN))) {
					tok->operator = UNARY_MINUS;
				}
				else {
					tok->operator = SUBTRACT;
				}
				break;

			case '.' :
				/* XXX - need to check to see if this is a decimal point
				 * in the middle fo a floating point value.
				 */
				tok->operator = DOT;
				break;

			case '*' :
				/* XXX - need a better way to tell if this is an INDIRECTION.
				 * perhaps check the next token?
				 */
				if (!tok_last || 
				  (tok_last->operator && ((tok_last->operator != CLOSE_PAREN) &&
							(tok_last->operator != CAST)))) {
					tok->operator = INDIRECTION;
				}
				else {
					tok->operator = MULTIPLY;
				}
				break;

			case '/' :
				tok->operator = DIVIDE;
				break;

			case '%' :
				tok->operator = MODULUS;
				break;

			case '(' : {
				char *s;
				int len;

				/* Make sure the previous token is an operator
				 */
				if (tok_last && !tok_last->operator) {
					*error = E_SYNTAX_ERROR;
					return (tok_last);
				}

				if (tok_last && ((tok_last->operator == RIGHT_ARROW) || 
								 (tok_last->operator == DOT))) {
					*error = E_SYNTAX_ERROR;
					return (tok_last);
				}

				/* Check here to see if following tokens constitute a cast.
				 */

				/* Skip past any "white space" (spaces and tabs)
				 */
				while ((*(cp+1) == ' ') || (*(cp+1) == '\t')) {
					cp++;
				}
				if ((*(cp+1) == '(') || (*(cp+1) == '*') || (*(cp+1) == ')')){
					tok->operator = OPEN_PAREN;
					paren_count++;
					break;
				}
				
				if (s = strpbrk(cp+1, " *)")) {
					len = (uint)s - (uint)(cp+1);
					if (is_keyword((char*)((uint)(cp+1)), len)) {
						if (!(s = strpbrk((cp+1), ")"))) {
							*error = E_OPEN_PAREN;
							return (tok);
						}
						len = (uint)s - (uint)(cp+1);
						tok->string = (char *)alloc_block(len + 1, B_TEMP);
						bcopy((cp+1), tok->string, len);
						tok->string[len] = 0;
						tok->operator = CAST;
						cp = (char *)((uint)(cp+1) + len);
						break;
					}
				}
				tok->operator = OPEN_PAREN;
				paren_count++;
				break;
			}

			case ')' :
				if (tok_last && ((tok_last->operator == RIGHT_ARROW) || 
								 (tok_last->operator == DOT))) {
					*error = E_SYNTAX_ERROR;
					return (tok_last);
				}
				tok->operator = CLOSE_PAREN;
				paren_count--;
				break;

			case '&' :
				if (*((char*)cp + 1) == '&') {
					tok->operator = LOGICAL_AND;
					cp++;
				}
				else if (!tok_last || (tok_last && 
					(tok_last->operator && 
						tok_last->operator != CLOSE_PAREN))) {
					tok->operator = ADDRESS;
				}
				else {
					tok->operator = BITWISE_AND;
				}
				break;

			case '|' :
				if (*((char*)cp + 1) == '|') {
					tok->operator = LOGICAL_OR;
					cp++;
				}
				else {
					tok->operator = BITWISE_OR;
				}
				break;

			case '=' :
				if (*((char*)cp + 1) == '=') {
					tok->operator = EQUAL;
					cp++;
				}
				else {
					/* ASIGNMENT -- NOT IMPLEMENTED
					 */
					tok->operator = NOT_YET;
				}
				break;

			case '<' :
				if (*((char*)cp + 1) == '<') {
					tok->operator = LEFT_SHIFT;
					cp++;
				}
				else if (*((char*)cp + 1) == '=') {
					tok->operator = LESS_THAN_OR_EQUAL;
					cp++;
				}
				else {
					tok->operator = LESS_THAN;
				}
				break;

			case '>' :
				if (*((char*)(cp + 1)) == '>') {
					tok->operator = RIGHT_SHIFT;
					cp++;
				}
				else if (*((char*)cp + 1) == '=') {
					tok->operator = GREATER_THAN_OR_EQUAL;
					cp++;
				}
				else {
					tok->operator = GREATER_THAN;
				}
				break;

			case '!' :
				if (*((char*)cp + 1) == '=') {
					tok->operator = NOT_EQUAL;
					cp++;
				}
				else {
					tok->operator = LOGICAL_NEGATION;
				}
				break;

			case '$' : {
				int e, len;
				char *s, *vname;
				variable_t *vp;
				token_t *t;

				if (s = strpbrk((cp + 1), " .\t+-*/()[]|~!$&%^<>?:&=^\"\'")) {
					len = (uint)s - (uint)cp + 1;
				}
				else {
					len = strlen(cp) + 1;
				}
				vname = (char *)alloc_block(len, B_TEMP);
				memcpy(vname, cp, (len -1));
				vname[len - 1] = 0;
				vp = find_variable(vtab, vname, 0);
				if (!vp || (vp->v_flags & V_COMMAND)) {
					*error = E_BAD_EVAR;	
					free_block((k_ptr_t)vname);
					return(tok);
				}
				if (vp->v_flags & V_TYPEDEF) {
					free_block((k_ptr_t)vname);
					free_tokens(tok);
					t = get_token_list(vp->v_typestr, &e);
					if (!tok_head) {
						tok_head = tok_last = t;
					}
					else {
						tok_last->next = t;
						while (t->next) {
							t = t->next;
						}
						tok_last = t;
					}
					if (cp = s) {
						continue;
					}
					else {
						return(tok_head);
					}
				}
				else if (vp->v_flags & V_STRING) {
					if (tok_head) {
						*error = E_END_EXPECTED;
						return(tok);
					}

					/* Skip ahead to the first non-blank character
					 */
					while (s && (*s == ' ')) {
						s++;
					}
					if (s && (*s != 0)) {
						*error = E_END_EXPECTED;
						tok->ptr = s;
						return(tok);
					}

					tok->string = 
						(char *)alloc_block(strlen(vp->v_exp) + 1, B_TEMP);

					/* We have to copy the string so that it doesn't 
					 * get freed when the token is freed.
					 */
					strcpy(tok->string, vp->v_exp);
					tok->type = TEXT;
					return(tok);
				}
			}

			case '~' :
				tok->operator = ONES_COMPLEMENT;
				break;
			
			case '^' :
				tok->operator = BITWISE_EXCLUSIVE_OR;
				break;

			case '?' :
				tok->operator = CONDITIONAL;
				break;

			case ':' :
				tok->operator = CONDITIONAL_ELSE;
				break;

			case '[' :
				tok->operator = OPEN_SQUARE_BRACKET;;
				break;

			case ']' :
				tok->operator = CLOSE_SQUARE_BRACKET;;
				break;

			default: {

				char *s;
				int len;

				/* Check and see if this token is a STRING or CHARACTER
				 * type (begins with a single or double quote).
				 */
				if ((*cp == '\'') || (*cp == '\"')) {

					/* Make sure we don't already have any tokens.
					 */
					if (tok_head) {
						*error = E_END_EXPECTED;
						return(tok);
					}
					switch (*cp) {
						case '\'' :
							s = strpbrk((cp + 1), "\'");
							if (!s) {
								*error = E_BAD_STRING;
								return(tok);
							}
							len = (uint)s - (uint)cp;
							if (len == 2) {
								tok->type = CHARACTER;
							}
							else {
								tok->type = TEXT;
							}
							break;

						case '\"' :
							s = strpbrk((cp + 1), "\"");
							if (!s) {
								*error = E_BAD_STRING;
								return(tok);
							}
							len = (uint)s - (uint)cp;
							tok->type = TEXT;
							break;
					}
					if (strlen(cp) > (len + 1)) {

						/* Check to see if there is a colon or
						 * semi-colon directly following the 
						 * string. If there is, then the string
						 * is OK (the following characters are part
						 * of the next expression). Also, it's OK
						 * to have trailing blanks as long as that's
						 * all threre is.
						 */
						char *c;

						c = s + 1;
						while (*c) {
							if ((*c == ',') || (*c == ';')) {
								break;
							}
							else if (*c != ' ') {
								*error = E_END_EXPECTED;
								tok->ptr = c;
								return(tok);
							}
							c++;
						}
						/* Truncate the trailing blanks (they are not part
						 * of the string).
						 */
						if (c != (s + 1)) {
							*(s + 1) = 0;
						}
					}
					tok->string = (char *)alloc_block(len, B_TEMP);
					bcopy((cp + 1), tok->string, len - 1);
					tok->string[len - 1] = 0;
					return(tok);
				}

				if (s = strpbrk(cp, " .\t+-*/()[]|~!$&%^<>?:&=^\"\'")) {
					len = (uint)s - (uint)cp + 1;
				}
				else {
					len = strlen(cp) + 1;
				}

				tok->string = (char *)alloc_block(len, B_TEMP);
				bcopy(cp, tok->string, len - 1);
				tok->string[len - 1] = 0;

				cp = (char *)((uint)cp + len - 2);

				/* Check to see if this is the keyword "sizeof." If not,
				 * then check to see if the string is a member name.
				 */
				if (!strcmp(tok->string, "sizeof")) {
					tok->operator = SIZEOF;
					free_block((k_ptr_t)tok->string);
					tok->string = 0;
				}
				else if (tok_last && ((tok_last->operator == RIGHT_ARROW) ||
								 (tok_last->operator == DOT))) {
					tok->type = MEMBER;
				}
				else {
					tok->type = STRING;
				}
				break;
			}
		}
		if (!(tok->type)) {
			tok->type = OPERATOR;
		}
		if (!tok_head) {
			tok_head = tok_last = tok;
		}
		else {
			tok_last->next = tok;
			tok_last = tok;
		}
		cp++;
	}
	if (paren_count < 0) {
		*error = E_CLOSE_PAREN;
		return(tok);
	}
	else if (paren_count > 0) {
		*error = E_OPEN_PAREN;
		return(tok);
	}
	if (DEBUG(DC_EVAL, 1)) {
		token_t *t;

		fprintf(KL_ERRORFP, "   TOKEN  OPERATOR       PTR                      "
				"STRING      NEXT\n");
		fprintf(KL_ERRORFP, "--------------------------------------------------"
				"----------------\n");
		t = tok_head;
		while(t) {
			fprintf(KL_ERRORFP, "%8x  %8d  %8x  %26s  %8x\n", 
				t, t->operator, t->ptr, t->string, t->next);
			t = t->next;
		}
		fprintf(KL_ERRORFP, "--------------------------------------------------"
				"----------------\n");
	}
	return(tok_head);
}

/*
 * is_unary()
 */
int
is_unary(int op)
{
	switch (op) {
		case LOGICAL_NEGATION :
		case ADDRESS :
		case INDIRECTION :
		case UNARY_MINUS :
		case UNARY_PLUS :
		case ONES_COMPLEMENT :
		case CAST :
			return(1);

		default :
			return(0);
	}
}


/*
 * is_binary()
 */
int
is_binary(int op)
{
	switch (op) {

		case BITWISE_OR :
		case BITWISE_EXCLUSIVE_OR :
		case BITWISE_AND :
		case RIGHT_SHIFT :
		case LEFT_SHIFT :
		case ADD :
		case SUBTRACT :
		case MULTIPLY :
		case DIVIDE :
		case MODULUS :
		case LOGICAL_OR :
		case LOGICAL_AND :
		case EQUAL :
		case NOT_EQUAL :
		case LESS_THAN :
		case GREATER_THAN :
		case LESS_THAN_OR_EQUAL :
		case GREATER_THAN_OR_EQUAL :
		case RIGHT_ARROW :
		case DOT :
			return(1);

		default :
			return(0);
	}
}

/*
 * precedence()
 */
int
precedence(int a) 
{
	if ((a >= CONDITIONAL) && (a <= CONDITIONAL_ELSE)) {
		return(1);
	}
	else if (a == LOGICAL_OR) {
		return(2);
	}
	else if (a == LOGICAL_AND) {
		return(3);
	}
	else if (a == BITWISE_OR) {
		return(4);
	}
	else if (a == BITWISE_EXCLUSIVE_OR) {
		return(5);
	}
	else if (a == BITWISE_AND) {
		return(6);
	}
	else if ((a >= EQUAL) && (a <= NOT_EQUAL)) {
		return(7);
	}
	else if ((a >= LESS_THAN) && (a <= GREATER_THAN_OR_EQUAL)) {
		return(8);
	}
	else if ((a >= RIGHT_SHIFT) && (a <= LEFT_SHIFT)) {
		return(9);
	}
	else if ((a >= ADD) && (a <= SUBTRACT)) {
		return(10);
	}
	else if ((a >= MULTIPLY) && (a <= MODULUS)) {
		return(11);
	}
	else if ((a >= LOGICAL_NEGATION) && (a <= SIZEOF)) {
		return(12);
	}
	else if ((a >= RIGHT_ARROW) && (a <= DOT)) {
		return(13);
	}
	else {
		return(0);
	}
}

/*
 * make_node()
 */
node_t *
make_node(token_t *t, int flags)
{
	node_t *np;
	struct syment *sp;
	symdef_t *sdp;

	np = (node_t*)ALLOC_BLOCK(sizeof(*np), flags);

	if (t->type == OPERATOR) {

		/* Check to see if this token represents a typecast
		 */
		if (t->operator == CAST) {
			type_t *tp;

			if (!(np->type = get_type(t->string, flags))) {
				free_nodes(np);
				return((node_t*)NULL);
			}
			np->node_type = OPERATOR;
			np->operator = CAST;

			/* Determin whether or not this is a pointer to a type
			 * and the type class (base or dwarf).
			 */
			tp = np->type;
			if (tp->flag == POINTER_FLAG) {
				np->flags = POINTER_FLAG;
				tp = tp->t_next;
				while (tp->flag == POINTER_FLAG) {
					tp = tp->t_next;
				}
			}

			switch(tp->flag) {
				case BASE_TYPE_FLAG:
					np->flags |= BASE_TYPE_FLAG;
					break;

				case DWARF_TYPE_FLAG:
					np->flags |= DWARF_TYPE_FLAG;
					break;

				default:
					free_nodes(np);
					return((node_t*)NULL);
			}
		}
		else {
			np->node_type = OPERATOR;
			np->operator = t->operator;
		}
	}
	else if (t->type == MEMBER) {
		np->name = (char *)dup_block((k_ptr_t)t->string, B_TEMP);
		np->node_type = MEMBER;
	}
	else if (t->type == STRING) {
		if ((sp = get_sym(t->string, B_TEMP)) && !(flags & C_NOVARS)) {
			np->address = sp->n_value;
			np->flags |= ADDRESS_FLAG;
			np->name = t->string;
			t->string = (char*)NULL;
			np->node_type = VARIABLE;

			/* Need to make a call to sym_lkup() to see if there
			 * is type information available for this variable.
			 *
			 * XXX - The symdef_s struct actually overlays dw_type_s
			 * struct. In that struct is the die offset of the variable.
			 * If type information is available for this variable, then
			 * we need to follow this path (check the variable die, then
			 * the type, etc.) Not sure if there are any functions that
			 * will do this right now (get_type() takes a type name -- 
			 * something that we don't have yet).
			 */
			sdp = sym_lkup(stp, np->name);
			free_sym(sp);

			/* XXX - For now, addtach a type struct for type long (it will 
			 * be the size of a kernel pointer). That will at least let us
			 * do something and will prevent the scenario where we have a 
			 * type node with out a pointer to a type struct! This needs
			 * to be replaced with code to handle actual variable types.
			 */
			np->type = alloc_block(sizeof(type_t), flags);
			np->type->flag = POINTER_FLAG;
			np->type->t_ptr = get_type("long", flags);
			np->node_type = TYPE_DEF;
			np->flags |= POINTER_FLAG;
			kl_get_block(K, np->address, K_NBPW(K), 
					&np->value, "pointer value");
			if (!PTRSZ64(K)) {
				np->value >>= 32;
			}
		}
		else if (flags & (C_WHATIS|C_SIZEOF)) {

			switch (symbol_table) {

				case DWARF_TYPE_FLAG : {

					dw_type_t *dtp;

					/* Check and see if this is a struct name
					 */
					dtp = dw_lkup(stp, t->string, DW_TAG_structure_type);
					if (!dtp) { 
						/* Check to see if this is a union name
						 */
						dtp = dw_lkup(stp, t->string, DW_TAG_union_type);
					}
					if (!dtp) {
						/* Check to see if this is a typedef name
						 */
						dtp = dw_lkup(stp, t->string, DW_TAG_typedef);
					}
					if (dtp) {
						/* If struct, union or typedef
						 */
						dw_type_info_t *dti;

						dti = 
							(dw_type_info_t*)ALLOC_BLOCK(sizeof(*dti), flags);

						dw_type_info(dtp->dt_off, dti, flags);

						np->node_type = TYPE_DEF;
						np->flags = DWARF_TYPE_FLAG;
						np->type = (type_t*)alloc_block(sizeof(type_t), B_TEMP);
						np->type->flag = DWARF_TYPE_FLAG;
						np->type->t_ptr = dti;
					}
					else if (sp) {
						np->address = sp->n_value;
						np->flags |= ADDRESS_FLAG;
						np->name = t->string;
						t->string = (char*)NULL;
						np->node_type = VARIABLE;
						free_sym(sp);
					}
					else {
						if (GET_VALUE(t->string, &np->value)) {
							free_nodes(np);
							return((node_t*)NULL);
						}
						np->node_type = NUMBER;
					}
					return(np);
				}
			}
		}
		else {
			if (GET_VALUE(t->string, &np->value)) {
				free_nodes(np);
				return((node_t*)NULL);
			}
			np->node_type = NUMBER;
		}
	}
	else if ((t->type == TEXT) || (t->type == CHARACTER)) {
		np->node_type = t->type;
		np->name = t->string;
		t->string = (char*)NULL; /* So the block doesn't get freed twice */
	}
	else {
		/* ERROR! */
	}
	np->tok_ptr = t->ptr;
	return(np);
}

/*
 * get_node()
 */
node_t *
get_node(token_t **tpp, int flags)
{
	node_t *n;

	n = make_node(*tpp, flags);
	*tpp = (*tpp)->next;
	return(n);
}

/*
 * add_node()
 */
int
add_node(node_t *root, node_t *new_node)
{
	node_t *n = root;

	/* Find the most lower-right node
	 */
	while (n->right) {
		n = n->right;
	}

	/* If the node we found is a leaf node, return an error (we will 
	 * have to insert the node instead).
	 */
	if (n->node_type == NUMBER) {
		return(-1);
	}
	else {
		n->right = new_node;
	}
	return(0);
}

/*
 * add_rchild()
 */
int
add_rchild(node_t *root, node_t *new_node)
{
	if (add_node(root, new_node) == -1) {
		return(-1);
	}
	return(0);
}

/*
 * add_lchild()
 */
int
add_lchild(node_t *root, node_t *new_node)
{
	node_t *n = root;

	/* Find the most lower-left node and make sure it is not a leaf 
	 * (child) node.
	 */
	while (n->left) {
		n = n->left;
	}

	/* If the node we found is a leaf node, return an error.
	 */
	if (n->node_type == NUMBER) {
		return(-1);
	}
	else {
		n->left = new_node;
	}
	return(0);

}

/*
 * node_to_typestr() 
 *
 *  Return a type string based on contents of node. The string is
 *  allocated as B_TEMP or B_PERM depending on the contents of flags.
 *
 */
char *
node_to_typestr(node_t *np, int flags)
{
	char *tsp = (char *)NULL;
	type_t *t;

	/* Determine if this is a Dwarf type or base type. Determine this
	 * by walking pointer chain until we get to the type-specific struct. 
	 * However, pass a pointer to the entire chain on to the typestr 
	 * routine.
	 */
	if (np->flags & DWARF_TYPE_FLAG) {
		tsp = dw_typestr(np->type, flags);
	}
	else if (np->flags & BASE_TYPE_FLAG) {
		tsp = base_typestr(np->type, flags);
	}
	else {
		/* Assume the value is an int.
		 */
		tsp = (char *)ALLOC_BLOCK(6, flags);
		strcpy(tsp, "(int)");
	}
	return(tsp);
}

/* 
 * free_tokens()
 */
void
free_tokens(token_t *tp)
{
	token_t *t, *tnext;

	t = tp;
	while (t) {
		tnext = t->next;
		if (t->string) {
			free_block((k_ptr_t)t->string);
		}
		free_block((k_ptr_t)t);
		t = tnext;
	}
}

/*
 * free_nodes()
 */
void
free_nodes(node_t *np)
{
	type_t *tp, *tnext;
	node_t *q;

	/* If there is nothing to free, just return.
	 */
	if (!np) {
		return;
	}

	if (q = np->left) {
		free_nodes(q);
	}
	if (q = np->right) {
		free_nodes(q);
	}
	if (np->name) {
		free_block((k_ptr_t)np->name);
	}
	free_type(np->type);
	free_block((k_ptr_t)np);
}

/*
 * get_sizeof()
 */
node_t *
get_sizeof(token_t **tpp, int *error)
{
	int size;
	node_t *n0;

	/* The next token should be either a CAST or an OPEN_PAREN. If it's
	 * something else, then return with an error.
	 */
	if ((*tpp)->type == OPERATOR) {
		if ((*tpp)->operator == OPEN_PAREN) {
			n0 = do_eval(tpp, C_SIZEOF, error);
			if (*error) {
				return(n0);
			}
		}
		else if ((*tpp)->operator == CAST) {
			if (!(n0 = get_node(tpp, C_SIZEOF))) {
				*error = E_SYNTAX_ERROR;
				return((node_t*)NULL);
			}
		}
		else {
			*error = E_BAD_TYPE;
			return(n0);
		}

		if (!n0->type) {
			*error = E_NOTYPE;
			return(n0);
		}

		if (n0->type->flag & POINTER_TYPE) {
			n0->value = K_NBPW(K);
		}
		else if (n0->type->flag & DWARF_TYPE_FLAG) {
			dw_type_info_t *dp = n0->type->t_ptr;

			n0->value = dp->t_byte_size;
		}
		else if (n0->type->flag & BASE_TYPE_FLAG) {
			base_t *bp = n0->type->t_ptr;

			n0->value = bp->byte_size;
		}
		else {
			*error = E_BAD_TYPE;
			return(n0);
		}
		n0->node_type = NUMBER;
		n0->flags = 0;
		n0->operator = 0;
		n0->byte_size = 0;
		n0->address = 0;
		if (n0->type) {
			free_type(n0->type);
			n0->type = 0;
		}
	}
	return(n0);
}

/* 
 * do_eval() -- Reduces an equation to a single value. 
 * 
 *   Any parenthesis (and nested parenthesis) within the equation will
 *   be solved first via a recursive call to do_eval().
 */
node_t *
do_eval(token_t **tpp, int flags, int *error)
{
	node_t *root = (node_t*)NULL, *n0, *n1;

	/* Loop through the tokens until we run out of tokens or we hit
	 * a CLOSE_PAREN. If we hit an OPEN_PAREN, make a recursive call
	 * to do_eval().
	 */
	while (*tpp) {

		n0 = n1 = (node_t *)NULL;

		/* Check for an OPEN_PAREN token 
		 */
		if ((*tpp)->operator == OPEN_PAREN) {

			/* skip over the OPEN_PAREN token
			 */
			*tpp = (*tpp)->next;

			/* Get the value contained within the parenthesis. If there 
			 * was an error, just return.
			 */
			n0 = do_eval(tpp, flags, error);
			if (*error) {
				free_nodes(root);
				return(n0);
			}

			/* If CLOSE_PAREN was the last token, we are at the end of 
			 * the token list. In that case, just return n0 (which should 
			 * contain the result). Otherwise, just fall through and see 
			 * what the next operator is (so that we can determine the
			 * proper location for this operand in the parse tree).
			 *
			 * Or, if do_eval() was called from get_sizeof(), then we
			 * need to return also (even if there are aditional tokens.
			 */
			if (!*tpp || (flags & C_SIZEOF)) {
				if (root) {
					add_rchild(root, n0);
				}
				else {
					root = n0;
				}
				return(root);
			}

		}
		else if ((*tpp)->operator == SIZEOF) {
			
			/* skip over the SIZEOF token
			 */
			*tpp = (*tpp)->next;

			n0 = get_sizeof(tpp, error);
			if (*error) {
				free_nodes(root);
				return(n0);
			}
		}
		else if (((*tpp)->operator == CLOSE_PAREN) ||
						((*tpp)->operator == CLOSE_SQUARE_BRACKET)) {

			/* Reduce the resulting tree to a single value
			 */
			replace(root, error, flags);
			
			/* Advance the token pointer past the CLOSE_PAREN or 
			 * CLOSE_SQUARE_BRACKET and then return.
			 */
			*tpp = (*tpp)->next;
			return(root);
		}

		/* If we don't already have a token loaded into n0, get one
		 * now.
		 */
		if (!n0) {
			if (!(n0 = get_node(tpp, flags))) {
				*error = E_SYNTAX_ERROR;
				return((node_t*)NULL);
			}
		}

		if (n0->node_type == OPERATOR) {
			if (is_unary(n0->operator)) {
				int do_eval_call = 0;

				if (n0->operator == INDIRECTION) {

					/* Make a recursive call to do_eval() and then 
					 * take the resulting value and link it in as 
					 * the right child node (after making sure that 
					 * no error occurred).
					 */
					do_eval_call++;
					n1 = do_eval(tpp, flags, error);
					if (*error) {

						/* Free nodes, if there are any
						 */
						free_nodes(root);
						free_nodes(n0);
						return(n1);
					}
				}
				else if (n0->operator == CAST) {
					if (!*tpp) {
						*error = E_SYNTAX_ERROR;
					}
					else if (((*tpp)->type == TYPE_DEF) || 
								 ((*tpp)->type == OPERATOR) ||
								 ((*tpp)->type == VARIABLE)) {
						if ((*tpp)->operator == CLOSE_PAREN) {

							/* Step over the CLOSE_PAREN and return
							 */
							*tpp = (*tpp)->next;
							return(n0);
						}
						do_eval_call++;
						n1 = do_eval(tpp, flags, error);
					}
					else if (!(n1 = get_node(tpp, flags))) {
						*error = E_SYNTAX_ERROR;
					}
					if (*error) {
						free_nodes(root);
						return(n0);
					}
				}
				else if (n0->operator == ADDRESS) {
					if (!*tpp) {
						*error = E_SYNTAX_ERROR;
					}
					else if (((*tpp)->operator == OPEN_PAREN) ||
							((*tpp)->next && 
							(((*tpp)->next->operator == RIGHT_ARROW) || 
							((*tpp)->next->operator == DOT)))) {

						do_eval_call++;
						n1 = do_eval(tpp, flags, error);
					}
					else if (!(n1 = get_node(tpp, flags))) {
						*error = E_SYNTAX_ERROR;
					}

					if (*error) {
						free_nodes(root);
						return(n0);
					}
				}
				else if (*tpp && ((*tpp)->operator == OPEN_PAREN)) {

					/* skip over the OPEN_PAREN token
					 */
					*tpp = (*tpp)->next;

					/* Get the value contained within the parenthesis. If 
					 * there was an error, just return.
					 */
					n1 = do_eval(tpp, flags, error);
					if (*error) {
						free_nodes(root);
						free_nodes(n0);
						return(n1);
					}
				}
				else {
					if ((*tpp == (token_t*)NULL) || 
							!(n1 = get_node(tpp, flags))){
						*error = E_SYNTAX_ERROR;
						free_nodes(root);
						return(n0);
					}
				}
				n0->right = n1;

				/* Check for nested unary operators
				 */
				while (is_unary(n1->operator)) {
					node_t *n2 = (node_t *)NULL;

					/* If there aren't any more tokens, return an
					 * error (there are none to match up with the 
					 * unary operator).
					 */
					if (!(*tpp) || !(n2 = get_node(tpp, flags))) {
						*error = E_SYNTAX_ERROR;
						return(n1);
					}
					n1->right = n2;
					n1 = n2;
				}

				if (replace_unary(n0, error, flags) == -1) {
					if (n0->right) {
						free_block((k_ptr_t)n0->right);
						n0->right = (node_t *)NULL;
					}
					if (*error == 0) {
						*error = E_SYNTAX_ERROR;
					}
					return(n0);
				}

				if (do_eval_call) {
					if (!root) {
						root = n0;
					}
					else {
						add_rchild(root, n0);
					}
					if (!replace(root, error, flags)) {
						if (!(*error)) {
							*error = E_SYNTAX_ERROR;
						}
					}
					return(root);
				}
			}
			else {
				/* ERROR? */
			}
		}

		/* n0 should now contain a non-operator node. Check to see if 
		 * there is a next token. If there isn't, just add the last 
		 * rchild and return.
		 */
		if (!*tpp) {
			if (!root) {
				root = n0;
			}
			else {
				add_rchild(root, n0);
			}
			if (!replace(root, error, flags)) {
				if (!(*error)) {
					*error = E_SYNTAX_ERROR;
				}
			}
			return(root);
		}

		/* Make sure the next token is an operator.
		 */
		if (!(*tpp)->operator) {
			free_nodes(root);
			free_nodes(n0);
			n1 = get_node(tpp, flags);
			*error = E_SYNTAX_ERROR;
			return(n1);
		}
		else if (((*tpp)->operator == CLOSE_PAREN) ||
					((*tpp)->operator == CLOSE_SQUARE_BRACKET)) {

			if (root) {
				add_rchild(root, n0);
			}
			else {
				root = n0;
			}

			/* Reduce the resulting tree to a single value
			 */
			if (!replace(root, error, flags)) {
				if (!(*error)) {
					*error = E_SYNTAX_ERROR;
				}
				return(root);
			}
			
			/* Advance the token pointer past the CLOSE_PAREN and then
			 * return.
			 */
			*tpp = (*tpp)->next;
			return(root);
		}
		else if ((*tpp)->operator == OPEN_SQUARE_BRACKET) {

			/* skip over the OPEN_SQUARE_BRACKET token
			 */
			*tpp = (*tpp)->next;

			/* Get the value contained within the brackets. This 
			 * value must represent an array index.
			 */
			n1 = get_array_index(tpp, error);
			if (*error) {
				free_nodes(root);
				free_nodes(n0);
				return(n1);
			}

			/* Convert the array (or pointer type) to an element type
			 * using the index value obtained above. Make sure that
			 * n0 contains some sort of type definition first, however.
			 */
			if (n0->node_type != TYPE_DEF) {
				*error = E_BAD_TYPE;
				free_nodes(n1);
				return(n0);
			}
			array_to_element(n0, n1, error);
			if (*error) {
				free_nodes(n0);
				return(n1);
			}
			free_nodes(n1);

			/* If there aren't any more tokens, just
			 * return.
			 */
			if (!(*tpp)) {
				return(n0);
			}
		}
		else if (!is_binary((*tpp)->operator)) {
			free_nodes(root);
			n1 = get_node(tpp, flags);
			*error = E_SYNTAX_ERROR;
			return(n1);
		}

		/* Now get the operator node
		 */
		if (!(n1 = get_node(tpp, flags))) {
			*error = E_SYNTAX_ERROR;
			return((node_t*)NULL);
		}

		/* Check to see if this binary operator is RIGHT_ARROW or DOT.
		 * If it is, we need to reduce it to a single value node now.
		 */
		while ((n1->operator == RIGHT_ARROW) || (n1->operator == DOT)) {

			/* The next node must contain the name of the struct|union
			 * member.
			 */
			if (!*tpp || ((*tpp)->type != MEMBER)) {
				if (root) {
					free_nodes(root);
				}
				free_nodes(n0);
				*error = E_BAD_MEMBER;
				return(n1);
			}
			n1->left = n0;

			/* Now get the next node and link it as the right child.
			 */
			if (!(n0 = get_node(tpp, flags))) {
				*error = E_SYNTAX_ERROR;
				return((node_t*)NULL);
			}
			n1->right = n0;
			if (!(n0 = replace(n1, error, flags))) {
				if (!(*error)) {
					*error = E_SYNTAX_ERROR;
				}
				return(n1);
			};

			/* Check to see if there is a next token. If there is, check
			 * to see if it is the operator CLOSE_PAREN. If it is, then
			 * return (skipping over the CLOSE_PAREN first).
			 */
			if ((*tpp && (*tpp)->operator == CLOSE_PAREN) ||
					(*tpp && (*tpp)->operator == CLOSE_SQUARE_BRACKET)) {

				if (root) {
					add_rchild(root, n0);
				}
				else {
					root = n0;
				}

				/* Reduce the resulting tree to a single value
				 */
				replace(root, error, flags);
				
				/* Advance the token pointer past the CLOSE_PAREN and then
				 * return.
				 */
				*tpp = (*tpp)->next;
				return(root);
			}

			/* Check to see if the next node is an OPEN_SQUARE_BRACKET.
			 * If it is, then we have to reduce the contents of the square
			 * brackets to an index array.
			 */
			if (*tpp && (*tpp)->operator == OPEN_SQUARE_BRACKET) {

				/* Advance the token pointer and call do_eval() again.
				 */
				*tpp = (*tpp)->next;

				n1 = get_array_index(tpp, error);
				if (*error) {
					free_nodes(root);
					free_nodes(n0);
					return(n1);
				}

				/* Convert the array (or pointer type) to an element type
				 * using the index value obtained above. Make sure that
				 * n0 contains some sort of type definition first, however.
				 */
				if (n0->node_type != TYPE_DEF) {
					*error = E_BAD_TYPE;
					free_nodes(n1);
					return(n0);
				}
				array_to_element(n0, n1, error);
				if (*error) {
					free_nodes(n0);
					return(n1);
				}
				free_nodes(n1);
			}

			/* Now get the next operator node (if there is one).
			 */
			if (!*tpp || !(n1 = get_node(tpp, flags))) {
				if (root) {
					add_rchild(root, n0);
				}
				else {
					root = n0;
				}
				return(root);
			}
		}

		if (!root) {
			root = n1;
			n1->left = n0;
		}
		else if (precedence(root->operator) >= precedence(n1->operator)) {
			add_rchild(root, n0);
			n1->left = root;
			root = n1;
		}
		else {
			if (!root->right) {
				n1->left = n0;
				root->right = n1;
			}
			else {
				add_rchild(root, n0);
				n1->left = root->right;
				root->right = n1;
			}
		}
	} /* while(*tpp) */
	return(root);
}

/*
 * get_array_index()
 *
 */
node_t *
get_array_index(token_t **tpp, int *error)
{
	int size, shft;
	k_uint_t index;
	node_t *n0;
	dw_type_info_t *dp;

	n0 = do_eval(tpp, 0, error);
	if (*error) {
		return(n0);
	}

	if (n0->node_type != NUMBER) {
		if (n0->node_type == TYPE_DEF) {

			if (n0->type->flag != DWARF_TYPE_FLAG) {
				*error = E_BAD_INDEX;
			}
			else {
				dp = n0->type->t_ptr;
				if (dp->t_tag == DW_TAG_base_type) {
					size = dp->t_byte_size;
					kl_get_block(K, n0->address, size, &index, "index");
					if (shft = (8 - size)) {
						index = index >> (shft * 8);
					}
					n0->value = index;
					n0->address = 0;
					free_type(n0->type);
					n0->type = 0;
					n0->flags = 0;
				}
				else {
					*error = E_BAD_INDEX;
				}
			}
		}
		else {
			*error = E_BAD_INDEX;
		}
	}
	n0->node_type = INDEX;
	return(n0);
}

/*
 * replace_cast()
 */
int
replace_cast(node_t *n, int *error, int flags)
{
	int type;
	k_uint_t value;
	type_t *t;

	if (!n || !n->right) {
		*error = E_SYNTAX_ERROR;
		return(-1);
	}

	if (n->flags & POINTER_FLAG) {
		if (n->right->node_type == VADDR) {
			if (n->right->flags & ADDRESS_FLAG) {
				n->value = n->right->address;
			}
			else {
				*error = E_SYNTAX_ERROR;
				return(-1);
			}
		}
		else if (n->right->node_type == VARIABLE) {

			/* Replace a VARIABLE node with a TYPE_DEF node if possible.
			 * Return if there was an error.
			 */
			replace_variable(n->right, error, flags);
			if (*error) {
				return(-1);
			}

			/* Check and see if the attempt to determine this variable's
			 * type failed. We know that's the case when the NOTYPE_FLAG 
			 * is set. If the ADDRESS_FLAG is also set, we can continue 
			 * (using the address value in the cast). Otherwise, we have 
			 * to return an error.
			 */
			if (n->right->flags & NOTYPE_FLAG) {
				if (n->right->flags & ADDRESS_FLAG) {
					n->value = n->right->address;
				}
				else {
					*error = E_SYNTAX_ERROR;
					return(-1);
				}
			}
			else {
				n->value = n->right->value;
			}

		}
		else {
			n->value = n->right->value;
		}
		n->node_type = TYPE_DEF;
		n->operator = 0;
		free_nodes(n->right);
		n->right = (node_t *)NULL;
		return(0);
	}

	if (!(t = eval_type(n))) {
		*error = E_BAD_TYPE;
		return(-1);
	}

	/* If we don't have a pointer and we're a dwarf type AND
	 * we don't point to a base type, return error.
	 */
	if (!(n->flags & POINTER_FLAG)) {
		if (n->flags & DWARF_TYPE_FLAG) {

			dw_type_info_t *atp = (dw_type_info_t*)n->type->t_ptr;

			if (atp->t_actual_type->d_tag != DW_TAG_base_type) {
				*error = E_BAD_TYPE;
				return(-1);
			}
		}
		else if (!(n->flags & BASE_TYPE_FLAG)) {
			*error = E_BAD_TYPE;
			return(-1);
		}
	}

	if (t->flag & DWARF_TYPE_FLAG) {
		if (n->right->node_type == TYPE_DEF) {
			n->value = n->right->value;
		}
		else if (n->right->node_type == VARIABLE) {
			/* XXX not working...
			 */
			n->value = n->right->address;
		}
		else if (n->right->node_type == NUMBER) {
			n->value = n->right->value;
		}
		else {
			*error = E_BAD_TYPE;
			return(-1);
		}
	}
	else if (t->flag & BASE_TYPE_FLAG) {
		n->byte_size = ((base_t*)t->t_ptr)->byte_size;
		value = n->right->value;
		switch (((base_t*)t->t_ptr)->byte_size) {

			case 1 :
				if (((base_t*)(t->t_ptr))->encoding == 8) {
					n->value = (unsigned char)value;
					n->flags |= UNSIGNED_FLAG;
				}
				else {
					n->value = (char)value;
				}
				break;

			case 2 :
				if (((base_t*)(t->t_ptr))->encoding == 7) {
					n->value = (unsigned short)value;
					n->flags |= UNSIGNED_FLAG;
				}
				else {
					n->value = (short)value;
				}
				break;

			case 4 :
				if (((base_t*)(t->t_ptr))->encoding == 7) {
					n->value = (unsigned int)value;
					n->flags |= UNSIGNED_FLAG;
				}
				else {
					n->value = (int)value;
				}
				break;

			case 8 :
				if (((base_t*)(t->t_ptr))->encoding == 7) {
					n->value = (unsigned long long)value;
					n->flags |= UNSIGNED_FLAG;
				}
				else {
					n->value = (long long)value;
				}
				break;

			default :
				*error = E_BAD_TYPE;
				return(-1);
		}
	}
	n->node_type = TYPE_DEF;
	n->operator = 0;
	n->type = t;
	free_block((k_ptr_t)n->right);
	n->right = (node_t *)NULL;
	return(0);
}

/*
 * apply_unary()
 */
int
apply_unary(node_t *n, k_uint_t *value, int *error)
{
	if (!n || !n->right) {
		return(-1);
	}

	switch (n->operator) {

		case UNARY_MINUS :
			*value = (0 - n->right->value);
			break;

		case UNARY_PLUS :
			*value = (n->right->value);
			break;

		case ONES_COMPLEMENT :
			*value = ~(n->right->value);
			break;

		case LOGICAL_NEGATION :
			if (n->right->value) {
				*value = 0;
			}
			else {
				*value = 1;
			}
			logical_flag++;
			break;

		default :
			break;
	}
	return(0);
}

/*
 * replace_unary() -- 
 * 
 *   Convert a unary operator node that contains a pointer to a value
 *   with a node containing the numerical result. Free the node that
 *   originally contained the value.
 */
int
replace_unary(node_t *n, int *error, int flags)
{
	kaddr_t addr;
	k_uint_t value;

	if (!n->right) {
		*error = E_SYNTAX_ERROR;
		return(-1);
	}

	if (is_unary(n->right->operator)) {
		replace_unary(n->right, error, flags);
	}

	if (n->operator == CAST) {
		return(replace_cast(n, error, flags));
	}
	else if (n->operator == INDIRECTION) {
		type_t *tp;
		dw_die_t *d;
		dw_type_info_t *dtp;

		/* Replace a VARIABLE node with a TYPE_DEF node if possible.
		 * If it's not possible, return an error.
		 */
		if (n->right->node_type == VARIABLE) {
			if (replace_variable(n->right, error, flags)) {
				if (!(*error)) {
					*error = E_NOTYPE;
				}
				return(-1);
			}
		}

		/* Make sure we have a pointer
		 */
		if (!(n->right->flags & POINTER_FLAG)) {
			*error = E_BAD_POINTER;
			return(-1);
		}

		/* Get the pointer to the type_s struct
		 */
		if (!(tp = n->right->type)) {
			*error = E_BAD_TYPE;
			return(-1);
		}

		/* Make sure the right child is a TYPE_DEF. 
		 */
		if (n->right->node_type != TYPE_DEF) {
			*error = E_BAD_TYPE;
			return(-1);
		}

		/* Make sure we have a pointer to a base or dwarf type
		 * structure.
		 */
		if (n->right->flags & BASE_TYPE_FLAG) {
			n->flags = BASE_TYPE_FLAG;
		}
		else if (n->right->flags & DWARF_TYPE_FLAG) {
			n->flags = DWARF_TYPE_FLAG;
		}
		else {
			*error = E_BAD_TYPE;
			return(-1);
		}

		n->node_type = TYPE_DEF;
		n->operator = 0;

		/* The first type_t struct MUST be a pointer to a type (BASE or
		 * DWARF). If it points to a DWARF type, strip off the 
		 * pointer block and procede. If it points to a BASE type, we need 
		 * to check if it points to type char. If it does, we have to set
		 * the STRING_FLAG before stripping off the pointer block.
		 */
		if (tp->flag == POINTER_FLAG) {

			type_t *t;

			if (!(t = eval_type(n->right))) {
				*error = E_BAD_TYPE;
				return(-1);
			}

			/* Zero out the type field in the right child so it wont 
			 * accidently be freed when the right child is freed 
			 * (upon success).
			 */
			n->right->type = (type_t*)NULL;

			if (!strcmp(((base_t*)t->t_ptr)->name, "char")) {
				n->flags |= STRING_FLAG;
			}
			n->type = tp->t_next;

			/* Free the pointer block
			 */
			free_block((k_ptr_t)tp);

			/* Get the pointer address 
			 */
			addr = n->address = n->right->value;
			n->flags |= (INDIRECTION_FLAG|ADDRESS_FLAG);

			/* If this is a pointer to a pointer, just get the next
			 * pointer value and return.
			 */
			if (n->type->flag == POINTER_FLAG) {

				kl_get_block(K, addr, K_NBPW(K), &n->value, "pointer value");
				if (!PTRSZ64(K)) {
					n->value >>= 32;
				}

				/* Set the appropriate node flag values 
				 */
				n->flags |= POINTER_FLAG;
				free_nodes(n->right);
				n->left = n->right = (node_t *)NULL;
				return(0);
			}
			n->value = addr;

			/* Turn off the pointer flag
			 */
			n->flags &= (~POINTER_FLAG);
		}
		else {
			*error = E_SYNTAX_ERROR;
			return(-1);
		}

		/* Zero out the type field in the right child so it wont 
		 * accidently be freed when the right child is freed 
		 * (upon success).
		 */
		n->right->type = (type_t*)NULL;
		free_nodes(n->right);
		n->left = n->right = (node_t *)NULL;
		return(0);
	}
	else if (n->operator == ADDRESS) {
		type_t *t;

		if (n->right->node_type == TYPE_DEF) {
			t = n->right->type;
		}
		else if (n->right->node_type == VARIABLE) {
			int ret;

			ret = replace_variable(n->right, error, flags);
			if (*error) {
				return(-1);
			}

			/* If we can't find type information for this variable, we 
			 * should convert the node to a VADDR node (with no type
			 * reference). That way pointer math will still work.
			 */
			if (ret == 1) {
				n->node_type = VADDR;
				n->flags = ADDRESS_FLAG;
				n->operator = 0;
				n->address = n->right->address;
				n->name = n->right->name = 0;
				n->type = n->right->type = 0;
				n->value = 0;
				free_nodes(n->right);
				n->left = n->right = (node_t *)NULL;
				return(0);
			}
			t = n->right->type;
		}
		else {
			*error = E_BAD_TYPE;
			return(-1);
		}

		n->type = (type_t*)ALLOC_BLOCK(sizeof(type_t), flags);
		n->type->flag = POINTER_FLAG;
		n->type->t_next = t;
		n->node_type = TYPE_DEF;
		n->operator = 0;
		n->value = n->right->address;
		n->flags = POINTER_FLAG;

		if (!(t = eval_type(n))) {
			*error = E_BAD_TYPE;
			return(-1);
		}
		n->flags |= t->flag;
		n->right->type = 0;
		free_nodes(n->right);
		n->left = n->right = (node_t *)NULL;
		return(0);
	}
	else if (apply_unary(n, &value, error) == -1) {
		return(-1);
	}
	free_nodes(n->right);
	n->node_type = NUMBER;
	n->operator = 0;
	n->left = n->right = (node_t *)NULL;
	bcopy(&value, &n->value, sizeof(k_uint_t));
	return(0);
}

/*
 * apply()
 */
int
apply(node_t *np, k_uint_t *value, int *error, int flags)
{
	int do_signed = 0;

	/* Each operator node must have two operands
	 */
	if (!np->right || !np->left) {
		*error = E_SYNTAX_ERROR;
		return(-1);
	}

	if (np->right->node_type == OPERATOR) {
		replace(np->right, error, flags);
	}

	if (!(np->flags & UNSIGNED_FLAG)) {
		do_signed++;
	}

	if ((np->left->node_type == TYPE_DEF) ||
					(np->left->node_type == VARIABLE) ||
					(np->left->node_type == VADDR)) {

		int size;
		k_uint_t lvalue;

		/* If the left or right nodes are base types (either Dwarf or 
		 * BAES_TYPE), convert them to proper numeric values. Otherwise, 
		 * use the address/pointer value to perform pointer math.
		 */
		if (!type_to_number(np->left, error)) {
			if (np->left->flags & ADDRESS_FLAG) {
				size = K_NBPW(K);
				lvalue = np->left->address;
			}
			else {
				type_t *tp;

				/* Since we only allow pointer math, anything other than
				 * a pointer causes failure.
				 */
				tp = (type_t*)np->left->type;
				if (tp->flag != POINTER_FLAG) {
					*error = E_SYNTAX_ERROR;
					return(-1);
				}

				tp = tp->t_next;

				switch (tp->flag) {

					case POINTER_FLAG :
						size = K_NBPW(K);
						break;

					case DWARF_TYPE_FLAG :
						size = ((dw_type_info_t*)tp->t_ptr)->t_byte_size;
						break;

					default :
						*error = E_SYNTAX_ERROR;
						return(-1);
				}
				lvalue = np->left->value;
			}

			switch (np->operator) {
				case ADD :
					*value = lvalue + (np->right->value * size);
					break;

				case SUBTRACT :
					*value = lvalue - (np->right->value * size);
					break;

				default :
					*error = E_BAD_OPERATOR;
					return(-1);
			}
			return(0);
		}
	}

	if (do_signed) {
		switch (np->operator) {
			case ADD :
				*value = (k_int_t)np->left->value + (k_int_t)np->right->value;
				break;

			case SUBTRACT :
				*value = (k_int_t)np->left->value - (k_int_t)np->right->value;
				break;

			case MULTIPLY :
				*value = (k_int_t)np->left->value * (k_int_t)np->right->value;
				break;

			case DIVIDE :
				*value = (k_int_t)np->left->value / (k_int_t)np->right->value;
				break;

			case BITWISE_OR :
				*value = (k_int_t)np->left->value | (k_int_t)np->right->value;
				break;

			case BITWISE_AND :
				*value = (k_int_t)np->left->value & (k_int_t)np->right->value;
				break;

			case MODULUS :
				*value = (k_int_t)np->left->value % (k_int_t)np->right->value;
				break;

			case RIGHT_SHIFT :
				*value = 
					(k_int_t)np->left->value >> (k_int_t)np->right->value;
				break;

			case LEFT_SHIFT :
				*value = 
					(k_int_t)np->left->value << (k_int_t)np->right->value;
				break;

			case LOGICAL_OR :
				if ((k_int_t)np->left->value || (k_int_t)np->right->value) {
					*value = 1;
				}
				else {
					*value = 0;
				}
				logical_flag++;
				break;

			case LOGICAL_AND :
				if ((k_int_t)np->left->value && (k_int_t)np->right->value) {
					*value = 1;
				}
				else {
					*value = 0;
				}
				logical_flag++;
				break;

			case EQUAL :
				if ((k_int_t)np->left->value == (k_int_t)np->right->value) {
					*value = 1;
				}
				else {
					*value = 0;
				}
				logical_flag++;
				break;

			case NOT_EQUAL :
				if ((k_int_t)np->left->value != (k_int_t)np->right->value) {
					*value = 1;
				}
				else {
					*value = 0;
				}
				logical_flag++;
				break;

			case LESS_THAN :
				if ((k_int_t)np->left->value < (k_int_t)np->right->value) {
					*value = 1;
				}
				else {
					*value = 0;
				}
				logical_flag++;
				break;

			case GREATER_THAN :
				if ((k_int_t)np->left->value > (k_int_t)np->right->value) {
					*value = 1;
				}
				else {
					*value = 0;
				}
				logical_flag++;
				break;

			case LESS_THAN_OR_EQUAL :
				if ((k_int_t)np->left->value <= (k_int_t)np->right->value) {
					*value = 1;
				}
				else {
					*value = 0;
				}
				logical_flag++;
				break;

			case GREATER_THAN_OR_EQUAL :
				if ((k_int_t)np->left->value >= (k_int_t)np->right->value) {
					*value = 1;
				}
				else {
					*value = 0;
				}
				logical_flag++;
				break;

			default :
				break;
		}
	}
	else {
		switch (np->operator) {
			case ADD :
				*value = np->left->value + np->right->value;
				break;

			case SUBTRACT :
				*value = np->left->value - np->right->value;
				break;

			case MULTIPLY :
				*value = np->left->value * np->right->value;
				break;

			case DIVIDE :
				*value = np->left->value / np->right->value;
				break;

			case BITWISE_OR :
				*value = np->left->value | np->right->value;
				break;

			case BITWISE_AND :
				*value = np->left->value & np->right->value;
				break;

			case MODULUS :
				*value = np->left->value % np->right->value;
				break;

			case RIGHT_SHIFT :
				*value = np->left->value >> np->right->value;
				break;

			case LEFT_SHIFT :
				*value = np->left->value << np->right->value;
				break;

			case LOGICAL_OR :
				if (np->left->value || np->right->value) {
					*value = 1;
				}
				else {
					*value = 0;
				}
				logical_flag++;
				break;

			case LOGICAL_AND :
				if (np->left->value && np->right->value) {
					*value = 1;
				}
				else {
					*value = 0;
				}
				logical_flag++;
				break;

			case EQUAL :
				if (np->left->value == np->right->value) {
					*value = 1;
				}
				else {
					*value = 0;
				}
				logical_flag++;
				break;

			case NOT_EQUAL :
				if (np->left->value != np->right->value) {
					*value = 1;
				}
				else {
					*value = 0;
				}
				logical_flag++;
				break;

			case LESS_THAN :
				if (np->left->value < np->right->value) {
					*value = 1;
				}
				else {
					*value = 0;
				}
				logical_flag++;
				break;

			case GREATER_THAN :
				if (np->left->value > np->right->value) {
					*value = 1;
				}
				else {
					*value = 0;
				}
				logical_flag++;
				break;

			case LESS_THAN_OR_EQUAL :
				if (np->left->value <= np->right->value) {
					*value = 1;
				}
				else {
					*value = 0;
				}
				logical_flag++;
				break;

			case GREATER_THAN_OR_EQUAL :
				if (np->left->value >= np->right->value) {
					*value = 1;
				}
				else {
					*value = 0;
				}
				logical_flag++;
				break;

			default :
				break;
		}
	}
	if ((np->flags & UNSIGNED_FLAG) && (np->byte_size < sizeof(k_uint_t))) {
		k_uint_t mask;

		mask = ((k_uint_t)1 << (np->byte_size * 8)) - 1;
		*value &= mask;
	}
	return(0);
}

/*
 * replace() -- 
 *
 *   Replace the tree with a node containing the numerical result of
 *   the equation.
 */
node_t *
replace(node_t *np, int *error, int flags)
{
	k_uint_t value;
	node_t *q, *r;
				
	if (!np) {
		return((node_t *)NULL);
	}

	if (np->node_type == OPERATOR) {
		if (!(q = np->left)) {
			return((node_t *)NULL);
		}
		while (q) {
			if (!replace(q, error, flags)) {
				return((node_t *)NULL);
			}
			q = q->right;
		}

		/* If the left node is a VARIABLE, convert it to TYPE_DEF here.
		 */
		if (np->left->node_type == VARIABLE) {

			/* Replace the VARIABLE node with a TYPE_DEF node. If 
			 * there was an error, or if there wasn't any dwarf type 
			 * info available, return an error.
			 */
			if (replace_variable(np->left, error, flags)) {
				if (*error == 0) {
					*error = E_NOTYPE;
				}
				return((node_t *)NULL);
			}
		}

		if ((np->operator == RIGHT_ARROW) || (np->operator == DOT)) {

			int i;
			kaddr_t addr = 0;
			type_t *tp, *mp;

			/* The left node must point to a TYPE_DEF
			 */
			if (np->left->node_type != TYPE_DEF) {
				if (np->left->flags & NOTYPE_FLAG) {
					*error = E_NOTYPE;
				}
				else {
					*error = E_BAD_TYPE;
				}
				return((node_t *)NULL);
			}

			/* Get the type information. Check to see if we have a 
			 * pointer to a type. If we do, we need to strip off the 
			 * pointer and get the type info. 
			 */
			if (np->left->type->flag == POINTER_FLAG) {
				tp = np->left->type->t_next;
				free_block((k_ptr_t)np->left->type);
			}
			else {
				tp = np->left->type;
			}

			/* We need to zero out the left child's type pointer 
			 * to prevent the type structs from being prematurely 
			 * freed (upon success). We have to remember, however, to
			 * the free the type information before we return.
			 */
			np->left->type = (type_t*)NULL;

			/* tp should now point at a dwarf type_t struct. If it 
			 * points to anything else, return failure.
			 */
			if (tp->flag != DWARF_TYPE_FLAG) {
				*error = E_BAD_TYPE;
				free_type(tp);
				return((node_t *)NULL);
			}

			switch (tp->flag) {

				case DWARF_TYPE_FLAG: {

					dw_die_t *dp;
					dw_type_info_t *dti;

					/* Get the type die information entry. It should be 
					 * one of the following:
					 *
					 *   DW_TAG_structure_type 
					 *   DW_TAG_union_type 
					 *   DW_TAG_pointer_type 
					 *
					 * If it's a DW_TAG_pointer_type, it must reference a 
					 * DW_TAG_structure_type or DW_TAG_union_type. Otherwise 
					 * return failure.
					 */
					dti = (dw_type_info_t *)tp->t_ptr;
					/* 
					 * XXX: Hack for the curnmlist musical
					 * chairs. Do a lookup on the name again.
					 */
					if(dti && dti->t_name)
					  dw_lkup(stp,dti->t_name,dti->t_tag);
					dp = dti->t_actual_type;

					/* If dp points to a DW_TAG_pointer_type, get the next 
					 * die record (which should contain struct/union 
					 * information).
					 */
					if (dp->d_tag == DW_TAG_pointer_type) {
						dp = dp->d_next;
					}

					/* Make sure dp points to a DW_TAG_structure_type or 
					 * DW_TAG_union_type. If it doesn't, return failure.
					 */
					if ((dp->d_tag != DW_TAG_structure_type) &&
									 (dp->d_tag != DW_TAG_union_type)) {
						*error = E_BAD_TYPE;
						free_type(tp);
						return((node_t *)NULL);
					}

					/* Get type information for member. If member is a 
					 * pointer to a type, get the pointer address and load 
					 * it into value. Otherwise, load the struct/union 
					 * address plus loc offset into address.
					 */
					if (!(mp = dw_lkup_member(stp,
									tp->t_ptr, np->right->name, flags))) {
						*error = E_BAD_MEMBER;
						free_type(tp);
						return((node_t *)NULL);
					}

					/* Now free the struct type information
					 */
					free_type(tp);

					np->node_type = TYPE_DEF;
					np->operator = 0;
					addr = np->left->value + 
							((dw_type_info_t *)mp->t_ptr)->t_loc;
					np->address = addr;
					np->flags |= (DWARF_TYPE_FLAG|ADDRESS_FLAG);
					if (!(np->type = member_to_type(mp->t_ptr,
									DWARF_TYPE_FLAG, flags))) {
						free_type(mp);
						*error = E_BAD_MEMBER;
						return((node_t *)NULL);
					}

					/* We have to free up the memory block that mp points
					 * to. We can't call free_type(), since it will free
					 * up the type information that np->type points to. So, 
					 * we have to free the block manually...
					 */
					free_block((k_ptr_t)mp);
					
					if (np->type->flag == POINTER_FLAG) {
						np->flags |= POINTER_FLAG;
						kl_get_block(K, addr, K_NBPW(K), 
								&np->value, "pointer value");
						if (!PTRSZ64(K)) {
							np->value >>= 32;
						}

						
					}
					else {
						np->value = addr;
					}
					break;
				}
			}

			free_nodes(np->left);
			free_nodes(np->right);
			np->left = np->right = (node_t*)NULL;
			return(np);
		}
		else {
			if (np->left->byte_size && np->right->byte_size) {
				if (np->left->byte_size > np->right->byte_size) {

					/* Left byte_size is greater than right
					 */
					np->byte_size = np->left->byte_size;
					np->type = np->left->type;
					np->flags = np->left->flags;
					free_type(np->right->type);
				}
				else if (np->left->byte_size < np->right->byte_size) {

					/* Right byte_size is greater than left
					 */
					np->byte_size = np->right->byte_size;
					np->type = np->right->type;
					np->flags = np->right->flags;
					free_type(np->left->type);
				}
				else {

					/* Left and right byte_size is equal
					 */
					if (np->left->flags & UNSIGNED_FLAG) {
						np->byte_size = np->left->byte_size;
						np->type = np->left->type;
						np->flags = np->left->flags;
						free_type(np->right->type);
					}
					else if (np->right->flags & UNSIGNED_FLAG) {
						np->byte_size = np->right->byte_size;
						np->type = np->right->type;
						np->flags = np->right->flags;
						free_type(np->left->type);
					}
					else {
						np->byte_size = np->left->byte_size;
						np->type = np->left->type;
						np->flags = np->left->flags;
						free_type(np->right->type);
					}
				}
			}
			else if (np->left->byte_size) {
				np->byte_size = np->left->byte_size;
				np->type = np->left->type;
				np->flags = np->left->flags;
				free_type(np->right->type);
			}
			else if (np->right->byte_size) {
				np->byte_size = np->right->byte_size;
				np->type = np->right->type;
				np->flags = np->right->flags;
			}
			else {
				/* XXX - No byte sizes
				 */
			}

			if (apply(np, &value, error, flags)) {
				return((node_t *)NULL);
			}
		}
		np->right->type = np->left->type = (type_t*)NULL;

		/* Flesh out the rest of the node struct. 
		 */
		np->node_type = NUMBER;
		np->flags &= ~(DWARF_TYPE_FLAG|BASE_TYPE_FLAG);
		np->operator = 0;
		np->value = value;
		free_block((k_ptr_t)np->left);
		free_block((k_ptr_t)np->right);
		np->left = np->right = (node_t*)NULL;
	}
	return(np);
}

/*
 * replace_variable()
 */
int
replace_variable(node_t *n, int *error, int flags)
{
	dw_type_t *dtp;
	type_t *t;
	dw_type_info_t *dti;

	/* Check to see if this is a dwarf kernel
	 */
	if (!stp) {

		/* Lookup the variable type by name
		 */
		if (!(dtp = dw_lkup(stp, n->name, DW_TAG_variable))) {
			*error = E_BAD_VARIABLE;
			return(-1);
		}

		/* Allocate some space for the dw_type_info_s struct and then
		 * get the struct referenced by d_off.
		 */ 
		dti = (dw_type_info_t*)ALLOC_BLOCK(sizeof(*dti), flags);
		if (!dw_type_info(dtp->dt_off, dti, flags)) {
			dw_free_type_info(dti);
			*error = E_BAD_TYPE;
			return(-1);
		}

		if (!(n->type = dw_var_to_type(dti, flags))) {

			/* If we can't find type information for this variable, we
			 * set a flag (NOTYPE_FLAG) and return one (1). We don't,
			 * set *error, however. We may be able to get by with just 
			 * the address of the variable. 
			 */
			n->flags |= NOTYPE_FLAG;
			dw_free_type_info(dti);
			return(1);
		}

		n->node_type = TYPE_DEF;
		n->operator = 0;
		if (n->type->flag == POINTER_FLAG) {
			n->flags = POINTER_FLAG;
			kl_get_block(K, n->address, 8, &n->value, "pointer value (DWARF)");
		}
		else {
			n->value = n->address;
		}

		if (!(t = eval_type(n))) {
			*error = E_BAD_TYPE;
			return(-1);
		}
		n->flags |= t->flag;
	} 
	return(0);
}

/*
 * array_to_element()
 */
void
array_to_element(node_t *n0, node_t *n1, int *error)
{
	int size;
	type_t *t;
	base_t *btp;
	dw_type_info_t *dtp;
	dw_die_t *dp;

	/* Take the array index and determine the proper pointer 
	 * value for the type pointer or array data type.
	 */
	t = n0->type;
	if (n0->node_type == TYPE_DEF) {
		if (t->flag == POINTER_FLAG) {
			if (t->t_next->flag == DWARF_TYPE_FLAG) {
				dtp = (dw_type_info_t*)t->t_next->t_ptr;
				t = t->t_next;
				free_block((k_ptr_t)n0->type);
				n0->type = t;
				size = dtp->t_byte_size;
			}
			else if (t->t_next->flag == BASE_TYPE_FLAG) {
				btp = (base_t*)t->t_next->t_ptr;
				t = t->t_next;
				free_block((k_ptr_t)n0->type);
				n0->type = t;
				size = btp->byte_size;
			}
			else {
				*error = E_BAD_INDEX;
			}
		}
		else if (t->flag == DWARF_TYPE_FLAG) {
			dtp = (dw_type_info_t*)t->t_ptr;
			if (dtp->t_tag == DW_TAG_array_type) {
				dtp->t_tag = dtp->t_type->d_next->d_tag;
				dtp->t_off = dtp->t_type->d_next->d_off;
				dtp->t_name = dtp->t_type->d_next->d_name;
				dp = dtp->t_type;
				while (dp->d_next) {
					dp = dp->d_next;
				}
				size = dp->d_byte_size;
				dtp->t_type = dtp->t_type->d_next;
				dtp->t_actual_type = dp;
				dtp->t_byte_size = size;
			}
			else {
				*error = E_BAD_INDEX;
			}
		}
		else {
			*error = E_BAD_INDEX;
		}
		if (!(*error)) {
			n0->address = n0->value + (n1->value * size);
			n0->value = n0->address;
			n0->flags &= (~POINTER_FLAG);
			n0->flags |= (ADDRESS_FLAG|INDIRECTION_FLAG);
		}
	}
	else {
		*error = E_BAD_INDEX;
	}
}

/*
 * type_to_number() 
 *
 *   Convert a base type (Dwarf or BASE_TYPE) to a numeric value. Return 
 *   1 on successful conversion, 0 if nothing was done.
 */
int
type_to_number(node_t *np, int *error) 
{
	int byte_size, bit_offset, bit_size, encoding, j, k;
	k_uint_t value, value1;
	dw_type_info_t *dti;

    /* Make sure we point to a type 
     */
    if (!np->type) {
        return(0);
    }

    if (np->type->flag == POINTER_FLAG) {
        return(0);
    }

    dti = (dw_type_info_t*)np->type->t_ptr;

	if (np->flags & BASE_TYPE_FLAG) {

		/* Nothing needs to be done. It's already a base value
		 * (we still need to return a 1).
		 */
		return(1);
	}
	else if (np->flags & DWARF_TYPE_FLAG) {
		if (dti->t_tag == DW_TAG_base_type) {
			byte_size = dti->t_byte_size;
			bit_offset = dti->t_bit_offset;
			if (!(bit_size = dti->t_bit_size)) {
				bit_size = (byte_size * 8);
			}
			encoding = dti->t_encoding;
		}
		else {
			return(0);
		}
	}

	kl_get_block(K, np->address, byte_size, &value1, "value");
	value = kl_get_bit_value(&value1, byte_size, bit_size, bit_offset);

	switch (byte_size) {

		case 1 :
			if (encoding == 8) {
				np->value = (unsigned char)value;
				np->flags |= UNSIGNED_FLAG;
			}
			else {
				np->value = (char)value;
			}
			break;

		case 2 :
			if (encoding == 7) {
				np->value = (unsigned short)value;
				np->flags |= UNSIGNED_FLAG;
			}
			else {
				np->value = (short)value;
			}
			break;

		case 4 :
			if (encoding == 7) {
				np->value = (unsigned int)value;
				np->flags |= UNSIGNED_FLAG;
			}
			else {
				np->value = (int)value;
			}
			break;

		case 8 :
			if (encoding == 7) {
				np->value = (unsigned long long)value;
				np->flags |= UNSIGNED_FLAG;
			}
			else {
				np->value = (long long)value;
			}
			break;

		default :
			*error = E_BAD_TYPE;
			return(0);
	}
	np->node_type = NUMBER;
	return(1);
}

/*
 * eval_type()
 */
type_t *
eval_type(node_t *n) 
{
	type_t *t;

	t = n->type;
	while (t->flag == POINTER_FLAG) {
		t = t->t_next;

		/* If for some reason, there is no type pointer (this shouldn't 
		 * happen but...), we have to make sure that we don't try to
		 * reference a NULL pointer and get a SEGV. Return an error if 
		 * 't' is NULL.
		 */
		 if (!t) {
			return((type_t*)NULL);
		 }
	}
	if ((t->flag == BASE_TYPE_FLAG) || (t->flag == DWARF_TYPE_FLAG)) {
		return (t);
	}
	return((type_t*)NULL);
}

/*
 * eval()
 */
node_t *
eval(char **exp, int flags, int *error)
{
	int i;
	token_t *tok, *t;
	node_t *n;
	char *e, *s;

	*error = 0;
	logical_flag = 0;

	/* Make sure we actually have a command line to evaluate
	 */
	if (!(*exp)) {
		return ((node_t*)NULL);
	}

	/* Expand any variables that are in the expression string. Free
	 * the original string and change s so that it points to the new
	 * expression string (so that the new string will be freed up when 
	 * we are done).
	 */
	if (e = expand_variables(*exp, 0)) {
		free_block((k_ptr_t)*exp);
		*exp = e;
		s = *exp;
	}
	else {
		*error = E_BAD_VARIABLE;
		tok = (token_t *)alloc_block(sizeof(token_t), B_TEMP);
		tok->ptr = *exp;
	}

	if (!(*error)) {
		tok = get_token_list(s, error);
	}

	if (*error) {
		n = (node_t *)ALLOC_BLOCK(sizeof(node_t), flags);
		n->tok_ptr = tok->ptr;
	}
	else {
		t = tok;
		n = do_eval(&t, flags, error);
		if (!*error) {
			if (!(n = replace(n, error, flags))) {
				n = (node_t *)ALLOC_BLOCK(sizeof (node_t), flags);
				n->tok_ptr = s + strlen(s) -1;
				*error = E_SYNTAX_ERROR;
			}
			else {
				/* Check to see if the the result should be interpreted 
				 * as 'true' or 'false'
				 */
				if (logical_flag && ((n->value == 0) || (n->value == 1))) {
					n->flags |= BOOLIAN_FLAG;
				}
			}
		}
	}
	if (!(*error) && (n->node_type ==  VARIABLE)) {
		if (n->flags & NOTYPE_FLAG) {
			if (!(n->flags & ADDRESS_FLAG)) {
				*error = E_NOTYPE;
			}
		}
		else {
			if (replace_variable(n, error, flags) == 1) {
				*error = E_NOTYPE;
			}
		}
	}
	free_tokens(tok);
	return(n);
}

/*
 * print_number()
 *
 *   If we get here it means the type is BASE_TYPE. If there isn't
 *   a type_s struct linked in, then make type the default (int).
 *   Then, depending on the contents of flags, display the value 
 *   in HEX, OCTAL, or DECIMAL form. There is a special case for 
 *   type char (display as a character).
 */
void
print_number(node_t *np, FILE *ofp, int flags)
{
	base_t *bt;
	k_uint_t value;

	/* Check to make sure we have a base_type pointer. This is kind
	 * of a hack since we can get to this routine from a number of
	 * different ways and it's not always clear that a type will have
	 * been designated along the way. Check the upper word and see if
	 * any bits are set. If they are, treat the value as a "long long"
	 * type (64-bit) value. Otherwise, treat it as an "int" value.
	 */
	if (!np->type) {
		if (np->value & 0xffffffff00000000) {
			np->type = get_type("long long", 0);
		}
		else {
			np->type = get_type("int", 0);
		}
		np->byte_size = ((base_t *)np->type->t_ptr)->byte_size;
	}

	if (flags & C_HEX) {
		if (np->byte_size && (np->byte_size != sizeof(k_uint_t))) {
			np->value = np->value & 
				(((k_uint_t)1<<(k_uint_t)(np->byte_size*8))-1);
		}
		fprintf(ofp, "0x%llx", np->value);
	} 
	else if (flags & C_OCTAL) {
		fprintf(ofp, "0%llo", np->value);
	} 
	else {
		if (np->flags & UNSIGNED_FLAG) {
			fprintf(ofp, "%llu", np->value);
		}
		else {
			bt = (base_t *)np->type->t_ptr;
			value = np->value << ((8 - np->byte_size) * 8);
			print_base(&value, bt->byte_size, bt->encoding, flags, ofp);
		}
	}
}

/*
 * print_eval_results()
 */
int
print_eval_results(node_t *np, FILE *ofp, int flags)
{
    int size;
	kaddr_t addr;
	dw_type_info_t *t;
	dw_die_t *d;

	/* Print the results
	 */
	switch (np->node_type) {

		case NUMBER:
			print_number(np, ofp, flags);
			break;

		case TYPE_DEF: {

			type_t *tp;
			k_ptr_t ptr;

			/* If this is a pointer just print out the address.
			 */

			if (np->flags & POINTER_FLAG) {
				fprintf(ofp, "0x%llx", np->value);
				break;
			}

			if (np->flags & DWARF_TYPE_FLAG) {

				/* Get the type information
				 */
				t = (dw_type_info_t *)np->type->t_ptr;
			        /* 
				 * XXX: Hack for the curnmlist musical chairs.
				 * Do a lookup on that name again.
				 */
				if(t && t->t_name)
				  dw_lkup(stp,t->t_name,t->t_tag);

				if (t->t_type->d_tag == DW_TAG_typedef) {
					d = t->t_actual_type;
				}
				else {
					d = t->t_type;
				}
				size = t->t_byte_size;
				ptr = alloc_block(size, B_TEMP);
				if ((d->d_tag == DW_TAG_base_type) && 
							!(np->flags & ADDRESS_FLAG)) {
					switch (size) {
						case 1:
							*(unsigned char *)ptr = np->value;
							break;

						case 2:
							*(unsigned short *)ptr = np->value;
							break;

						case 4:
							*(unsigned int *)ptr = np->value;
							break;

						case 8:
							*(unsigned long long *)ptr = np->value;
							break;
					}
					dw_print_type(ptr, t, 0, flags|SUPPRESS_NAME, ofp);
					free_block(ptr);
					return(1);;
				}

				addr = np->value;
				kl_get_block(K, addr, size, ptr, "type");
				if (KL_ERROR) {
					kl_print_error(K);
					free_block(ptr);
					return(1);
				}

				/* Print out the actual type
				 */

				switch (d->d_tag) {
					case DW_TAG_structure_type:
					case DW_TAG_union_type:
						dw_print_type(ptr, t, 0, flags, ofp);
						break;

					default:
						dw_print_type(ptr, t, 0, 
							(flags|SUPPRESS_NAME|SUPPRESS_NL), ofp);
						break;
				}
				free_block(ptr);
			} 
			else if (np->flags & BASE_TYPE_FLAG) {
				base_t *b;

				if (!(flags & ADDRESS_FLAG)) {
					print_number(np, ofp, flags);
				}
				else {

					/* Get the type information
					 */
					b = (base_t *)np->type->t_ptr;
					size = b->byte_size;
					ptr = alloc_block(size, B_TEMP);
					addr = np->value;
					kl_get_block(K, addr, size, ptr, "type");
					if (KL_ERROR) {
						free_block(ptr);
						return(1);
					}
					print_base(ptr, size, b->encoding, flags, ofp);
				}
			}
			break;
		}

		case VADDR:
		case VARIABLE:
			/* If we get here, there was no type info available.
			 * The ADDRESS_FLAG should be set (otherwise we would have
			 * returned an error). So, print out the address.
			 */ 
			fprintf(ofp, "0x%llx", np->address);
			break;

		default:
			if (np->node_type == TEXT) {
				print_string(np->name, ofp);
				if (KL_ERROR) {
					kl_print_error(K);
					return(1);
				}
			}
			else if (np->node_type == CHARACTER) {
				fprintf(ofp, "\'%c\'", np->name[0]);
			}
			break;
	}
	return(0);
}

/*
 * print_eval_error()
 */
void
print_eval_error(char *cmdname, char *s, char *bad_ptr, int error, int flags)
{
	int i, cmd_len;

	if ((cmdname[0] == 'p') || !strcmp(cmdname, "eval")) {
		fprintf(KL_ERRORFP, "print");
		if ((cmdname[1] == 'x') || (cmdname[1] == 'o')) {
			fprintf(KL_ERRORFP, "%c %s\n", cmdname[1], s);
			cmd_len = 6;
		}
		else {
			fprintf(KL_ERRORFP, " %s\n", s);
			cmd_len = 5;
		}
	}
	else {
		if (flags & CMD_STRING) {
			fprintf(KL_ERRORFP, "%s%s\n", cmdname, s);
		}
		else {
			fprintf(KL_ERRORFP, "%s %s\n", cmdname, s);
		}
		cmd_len = strlen(cmdname);
	}

	if (!bad_ptr) {
		for (i = 0; i < (strlen(s) + cmd_len); i++) {
			fprintf(KL_ERRORFP, " ");
		}
	}
	else {
		for (i = 0; i < (bad_ptr - s + 1 + cmd_len); i++) {
			fprintf(KL_ERRORFP, " ");
		}
	}
	fprintf(KL_ERRORFP, "^ ");
	switch (error) {
		case E_OPEN_PAREN :
			fprintf(KL_ERRORFP, "Too many open parenthesis\n");
			break;

		case E_CLOSE_PAREN :
			fprintf(KL_ERRORFP, "Too many close parenthesis\n");
			break;

		case E_BAD_MEMBER :
			fprintf(KL_ERRORFP, "No such member\n");
			break;

		case E_BAD_OPERATOR :
			fprintf(KL_ERRORFP, "Bad operator\n");
			break;

		case E_BAD_TYPE :
			fprintf(KL_ERRORFP, "Bad type\n");
			break;

		case E_NOTYPE :
			fprintf(KL_ERRORFP, "Could not find type information\n");
			break;

		case E_BAD_POINTER :
			fprintf(KL_ERRORFP, "Bad pointer\n");
			break;

		case E_BAD_VARIABLE :
			fprintf(KL_ERRORFP, "Not a variable\n");
			break;

		case E_BAD_INDEX :
			fprintf(KL_ERRORFP, "Bad array index\n");
			break;

		case E_BAD_STRING :
			fprintf(KL_ERRORFP, "Non-termining string\n");
			break;

		case E_END_EXPECTED :
			fprintf(KL_ERRORFP, "Expected end of print statement\n");
			break;

		case E_BAD_EVAR :
			fprintf(KL_ERRORFP, "Bad eval variable\n");
			break;

		default :
			fprintf(KL_ERRORFP, "Syntax error\n");
			break;
	}
}




