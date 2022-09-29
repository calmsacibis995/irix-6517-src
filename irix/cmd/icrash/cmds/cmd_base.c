#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_base.c,v 1.7 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>
#include <klib/klib.h>
#include "icrash.h"

#define ARGLEN      40                               /* maxlen of argument  */

/* 
 * prerrmes() -- print error message 
 */
void
prerrmes(FILE *ofp, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(ofp, fmt, ap);
	va_end(ap);
	fflush(ofp);
}

/* 
 * hextol() -- string to hexadecimal long conversion 
 */
long
hextol(FILE *ofp, char *s)
{
	int i,j;
	  
	for (j = 0; s[j] != '\0'; j++) {
		if ((s[j] < '0' || s[j] > '9') && (s[j] < 'a' || s[j] > 'f')
			&& (s[j] < 'A' || s[j] > 'F')) {
				break;
		}
	}

	if (s[j] != '\0' || sscanf(s, "%x", &i) != 1) {
		prerrmes(ofp, "%c is not a digit or letter a - f\n", s[j]);
		return(-1);
	}
	return(i);
}

/* 
 * stol() -- string to decimal long conversion 
 */
long
stol(FILE *ofp, char *s)
{
	int i, j;
	  
	for (j = 0; s[j] != '\0'; j++) {
		if((s[j] < '0' || s[j] > '9')) {
			break;
		}
	}
	if (s[j] != '\0' || sscanf(s, "%d", &i) != 1) {
		prerrmes(ofp, "%c is not a digit 0 - 9\n", s[j]);
		return(-1);
	}
	return(i);
}

/* 
 * octal() -- string to octal long conversion 
 */
long
octal(FILE *ofp, char *s)
{
	int i, j;
	  
	for (j = 0; s[j] != '\0'; j++) {
		if ((s[j] < '0' || s[j] > '7')) {
			break;
		}
	}
	if (s[j] != '\0' || sscanf(s, "%o", &i) != 1) {
		prerrmes(ofp, "%c is not a digit 0 - 7\n", s[j]);
		return(-1);
	}
	return(i);
}

/* 
 * btol() -- string to binary long conversion 
 */
long
btol(FILE *ofp, char *s)
{
	int i, j;
	  
	i = 0;
	for (j = 0; s[j] != '\0'; j++) {
		switch(s[j]) {
			case '0':
				i = i << 1;
				break;
			case '1':
				i = (i << 1) + 1;
				break;
			default :
				prerrmes(ofp, "%c is not a 0 or 1\n", s[j]);
				return(-1);
		}
	}
	return(i);
}

/* 
 * strcon() -- string to number conversion 
 */
long
strcon(FILE *ofp, char *string, char format)
{
	char *s;

	s = string;
	if (*s == '0') {
		if (strlen(s) == 1) {
			return(0); 
		}
		switch(*++s) {
			case 'X' :
			case 'x' :
				format = 'h';
				s++;
				break;

			case 'B' :
			case 'b' :
				format = 'b';
				s++;
				break;

			case 'D' :
			case 'd' :
				format = 'd';
				s++;
				break;

			default :
				format = 'o';
		}
	}

	if (!format) {
		format = 'd';
	}

	switch(format) {
		case 'h' :
			return(hextol(ofp, s));
		case 'd' :
			return(stol(ofp, s));
		case 'o' :
			return(octal(ofp, s));
		case 'b' :
			return(btol(ofp, s));
		default  :
			return(-1);
	}
}

/* 
 * _eval() -- simple arithmetic expression evaluation ( +  - & | * /) 
 *
 *   This function is used by the base command.
 */
long
_eval(FILE *ofp, char *string)
{
	int j,i;
	char rand1[ARGLEN];
	char rand2[ARGLEN];
	char *op;
	long addr1,addr2;

	if (string[strlen(string)-1] != ')') {
		prerrmes(ofp, "(%s is not a well-formed expression\n",string);
		return(-1);
	}

	if (!(op = strpbrk(string,"+-&|*/"))) {
		prerrmes(ofp, "(%s is not an expression\n",string);
		return(-1);
	}

	for (j=0,i=0; string[j] != *op; j++,i++) {
		if(string[j] == ' ') {
			--i;
		} else {
			rand1[i] = string[j];
		}
	}

	rand1[i] = '\0';
	j++;
	for (i = 0; string[j] != ')'; j++,i++) {
		if (string[j] == ' ') {
			--i;
		} else {
			rand2[i] = string[j];
		}
	}

	rand2[i] = '\0';
	if (!strlen(rand2) || strpbrk(rand2,"+-&|*/")) {
		prerrmes(ofp, "(%s is not a well-formed expression\n",string);
		return(-1);
	}

	if ((addr1 = strcon(ofp, rand1, NULL)) == -1) {
		return(-1);
	}

	if ((addr2 = strcon(ofp, rand2, NULL)) == -1) {
		return(-1);
	}

	switch(*op) {
		case '+' :
			return(addr1 + addr2);
		case '-' :
			return(addr1 - addr2);
		case '&' :
			return(addr1 & addr2);
		case '|' :
			return(addr1 | addr2);
		case '*' :
			return(addr1 * addr2);
		case '/' :
			if (addr2 == 0) {
				prerrmes(ofp, "cannot divide by 0\n");
				return(-1);
			}
			return(addr1 / addr2);
	}
	return(-1);
}

/* 
 * base_print() -- print results of function base 
 */
void
base_print(char *string, FILE *ofp)
{
	int i, error;
	long num;
	node_t *np;

	if (*string == '(') {
		num = _eval(ofp, ++string);
	} else {
		num = strcon(ofp, string, NULL);
	}

	if (num == -1) {
		return;
	}

	fprintf(ofp, "hex: %x\n", num);
	fprintf(ofp, "decimal: %d\n", num);
	fprintf(ofp, "octal: %o\n", num);
	fprintf(ofp, "binary: ");
	for (i = 0; num >= 0 && i < 32; i++, num <<= 1) ;
	for (; i < 32; i++, num <<= 1) {
		num < 0 ? fprintf(ofp, "1") : fprintf(ofp, "0");
	}
	fprintf(ofp, "\n");
	return;
}

/*
 * base_cmd() -- Run the 'base' command.
 */
int
base_cmd(command_t cmd)
{
	int i;

	for (i = 0; i < cmd.nargs; i++) {
		base_print(cmd.args[i], cmd.ofp);
	}
	return(0);
}

#define _BASE_USAGE "[-w outfile] numeric_values[s]"

/*
 * base_usage() -- Print the usage string for the 'base' command.
 */
void
base_usage(command_t cmd)
{
	CMD_USAGE(cmd, _BASE_USAGE);
}

/*
 * base_help() -- Print the help information for the 'base' command.
 */
void
base_help(command_t cmd)
{
	CMD_HELP(cmd, _BASE_USAGE,
		"Display a number in binary, octal, decimal, and hexadecimal. "
		"A number in a radix other then decimal should be preceded by a "
		"prefix that indicates its radix as follows:\n\n"
		"              0x      hexidecimal\n"
		"              0       octal\n"
		"              0b      binary");
}

/*
 * base_parse() -- Parse the command line arguments.
 */
int
base_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE);
}
