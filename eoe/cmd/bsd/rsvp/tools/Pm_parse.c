

/*******************************************************************

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies. and that any
  documentation, advertising materials, and other materials related to
  such distribution and use acknowledge that the software was developed
  in part by the University of Southern California, Information
  Sciences Institute.  The name of the University may not be used to
  endorse or promote products derived from this software without
  specific prior written permission.

  THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
  the suitability of this software for any purpose.  THIS SOFTWARE IS
  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

  Other copyrights might apply to parts of this software and are so
  noted when applicable.

********************************************************************/

#include "config.h"
#include "rsvp_types.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <netdb.h>
#ifdef STANDARD_C_LIBRARY
#  include <stdlib.h>
#  include <stddef.h>           /* for offsetof */
#else
#  include <malloc.h>
#  include <memory.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "Pm_parse.h"

#ifndef sgi /* this conflicts with stdio.h - mwang */
int printf(), fprintf();
#endif
int tolower(int);
int strcasecmp();
u_int32_t host_atoi32(char *cp);

/*
 *	Value list: output of parse machine
 */
/* definitions should be in .c files - mwang */
int		Pm_debug;
char *	        Pm_cp;		/* Scan pointer */
u_int32_t	Pm_val[20];
int		Pm_vi;		/* Value index: next value entry is
				 *	Pm_val[Pm_vi]
				 */
#define val0	Pm_val[0]
#define val1	Pm_val[1]
#define val2	Pm_val[2]
#define val3	Pm_val[3]

char 	*Pm_Class_names[] = {"End", "Show", "Op", "NotOp",
		"Is", "IsNot", "File", "NotFile",
		"Set", "Action", "Label", "TRV"};

char	*Pm_Op_names[] = {"none", "Integer", "cString", "Char", "Flag",
			"Host", "EOL", "WhSp"};

char	*Result_name[] = {"ERR", "NO", "OK"};

#define ERR_printf(x)		fprintf(stderr, (x))
#define ERR_printf2(x, y)	fprintf(stderr, (x), (y))
#define ERR_printf3(x, y, z)	fprintf(stderr, (x), (y), (z))

void	Pm_print(int, Pm_inst *), Pm_list(Pm_inst *);

#define SkipWhSp	{while (isspace(*Pm_cp)) Pm_cp++ ;}


/*	Parse Machine.  Given address of program array, start recognition
 *		sequence whose index is in transfer vector at offset PC_base.
 *		Returns OK, NO, or ERR.  May recurse.
 */
int
Parse_machine(Pm_inst *Pm_program, int PC_base)
	{
	int		 Pm_PC;
	Pm_inst		*ip, *tip;
	int		 testfor, result;
	u_int32_t	 val, host;
	int		 save_vi, first_vi;
	char		*tcp, *fcp, *save_cp;
	char		 tbuf[256];
	int		 i, L, found;
	FILE		*infp;

	/*	PC_base actually points to Transfer Vector entry in
	 *	bottom of memory.  Check that it is OK, then branch
	 *	to actual start of recognition sequence.
	 */
	tip = Pm_program + PC_base;
	if (PC_base <= 0 || tip->Pmi_class != PmC_TRV) {
		printf("Pm: Bad locn %d\n", PC_base);
		return(ERR);
	}
	Pm_PC = PC_base = tip->Pmi_next;
	first_vi = Pm_vi;
	while (1) {
		save_cp = Pm_cp;
		save_vi = Pm_vi;

		/*	Fetch:
		 */
		ip = Pm_program + Pm_PC++;
		if (Pm_debug) {
			Pm_print(Pm_PC-1, ip);
			printf("SCAN: %s\n", Pm_cp);
		}
		/*	Execute:
		 */
		testfor = OK;
		result = NO;
		switch (ip->Pmi_class) {
		
		case PmC_END:
			return(OK);

		case PmC_SHOW:
			if (ip->Pmi_parm)
				ERR_printf2("%s\n", ip->Pmi_parm);
			result = OK;
			break;

		case PmC_NOT_SUB:
			testfor = NO;
		case PmC_SUB:
			/*	Execute subroutine, and go to next if
			 *	does/does not succeed. Call parse machine
			 *	recursively.
			 */			
			result = Parse_machine(Pm_program, ip->Pmi_op);

			/*
			 *	If recognition failed, restore scan pointer
			 *	and value list pointer.
			 */
			if (result != OK) {
				Pm_cp = save_cp;
				Pm_vi = save_vi;
			}
			if (result == testfor && ip->Pmi_parm)
				ERR_printf2("%s\n", ip->Pmi_parm);
			break;

		case PmC_ACTION:
			/*	Execute semantic subroutine, and branch.
			 */
			if (Pm_debug) {
				printf("\nAction %d (%d) vi=%d\n", ip->Pmi_op,
						(int)ip->Pmi_parm, first_vi);
				printf("%d Vals: ", Pm_vi);
				for (i=0; i < Pm_vi; i++)
				    printf("%8.8X  ", (u_int32_t) Pm_val[i]);
				printf("\n\n");
			}
			result = Pm_Action(ip->Pmi_op,
					first_vi, (int)ip->Pmi_parm);
			break;

		case PmC_SET:
			/*	Create literal value and append to values list.
			 */
			result = OK;
			val = (int)ip->Pmi_parm;
			Append_Val(val);
			break;

		case PmC_NOT_FILE:
			testfor = NO;
		case PmC_FILE:
			/*	Look up <keyword> in specified file, and
			 *	if found, take the rest of the line of file
			 *	as input and recursively parse using specified
			 *	labelled rule.  Keyword is alphanumeric and dots.
			 */
			SkipWhSp;	/* Skip White Space */
			fcp = Pm_cp;
			while (isalnum(*Pm_cp) || *Pm_cp == '.')
				Pm_cp++;
			if (Pm_cp == fcp)
				break;

			/* Would be nicer to build in-memory copy of file...
			 */
			infp = fopen(ip->Pmi_parm, "r");
			if (!infp)
				break;
			found = 0;
			while (fgets(tbuf, sizeof(tbuf), infp)) {
				char *cp1 = fcp, *cp2 = tbuf;

				while(tolower(*cp1++) == tolower(*cp2++))
					if (cp1 == Pm_cp)
						break;
				if (cp1 != Pm_cp || !isspace(*cp2))
					continue;
				/* Found match */
				fcp = cp2;
				found = 1;
				break;
			}
			if (!found)
				break;
			save_cp = Pm_cp;
			Pm_cp = fcp;
			result = Parse_machine(Pm_program, ip->Pmi_op);
			Pm_cp = save_cp;
			if (result != OK)
				Pm_vi = save_vi;
			break;

		case PmC_NOT_OP:
			testfor = NO;
		case PmC_OP:
			/*	Execute built-in parse operation, and set
			 *	result to OK/NO/ERR.
			 */
		    switch (ip->Pmi_op) {

		    case PmO_cString:
				/*	Scan for case-independent match
				 *	to specified string.
				 */
			SkipWhSp;	/* Skip White Space */
			strncpy(tbuf, Pm_cp, L = strlen(ip->Pmi_parm));
			tbuf[L] = '\0';
			if (!strcasecmp(tbuf, ip->Pmi_parm)) {
				Pm_cp += L;
				result = OK;
			}
			break;

		    case PmO_Integer:
				/*	Scan for integer, and if one is
				 *	found, append to value list.  Suffixes
				 *	K (1000) and M (10**6) are allowed.
				 *	Parm field is string to be displayed if
				 *	result is as requested.  
				 */
			SkipWhSp;	/* Skip White Space */
			if (isdigit(*Pm_cp)) {
				val = 0;
				while (isdigit(*Pm_cp)) {
					val = 10*val + *Pm_cp++ - '0';
				}
				if (tolower(*Pm_cp) == 'k') {
					val *= 1000;
					Pm_cp++;
				}
				else if (tolower(*Pm_cp) == 'm') {
					val *= 1000000;
					Pm_cp++;
				}
				Append_Val(val);
				result = OK;
			}
			if (result == testfor && ip->Pmi_parm)
				ERR_printf2("%s\n", ip->Pmi_parm);
			break;

		    case PmO_Flag:
				/* 	Scan for any single char in given
				 *	string, and if one is found, append
				 *	its index to the value list.
				 */
		    case PmO_Char:
				/* 	Scan for any single char in given 
				 *	string but value list unchanged.
				 */
			SkipWhSp;	/* Skip White Space */
			for (tcp = ip->Pmi_parm; *tcp; tcp++) {
				if (*tcp == *Pm_cp) {
					result = OK;
					Pm_cp++;
					break;
				}
			}
			if (result == OK && ip->Pmi_op == PmO_Flag)
				Append_Val(tcp - ip->Pmi_parm); 
			break;

		    case PmO_Host:
				/*	Scan for host name or dotted decimal,
				 *	and if one is found, convert it.
				 */
			SkipWhSp;	/* Skip White Space */
			if (!isalnum(*Pm_cp))
				goto Nohost;
			fcp = Pm_cp;
			if (isdigit(*Pm_cp)) {
				/* Assume leading digit => dot dec */
				int K = 0;

				while (isdigit(*Pm_cp) || *Pm_cp == '.') {
					if (*Pm_cp == '.')
						K++;
					Pm_cp++;
				}
				if (K < 2)  /* Integer, not dotted decimal */
					goto Nohost;
				if (K == 2 || K > 3) {
					ERR_printf("Bad dotted decimal\n");
					result = ERR;
					break;
				}
			}
			else {
				while (isalnum(*Pm_cp) || *Pm_cp == '.' ||
							*Pm_cp == '-' )
					Pm_cp++;
			}
			strncpy(tbuf, fcp, L = Pm_cp - fcp);
		        tbuf[L] = '\0';
			host = host_atoi32(tbuf);
			if (host == (u_int32_t) -1) {
				ERR_printf2("Unknown host %s\n", tbuf);
				result = ERR;
				break;
			}
			result = OK;
			Append_Val(host);
Nohost:
			if (result == testfor && ip->Pmi_parm)
				ERR_printf2("%s\n", ip->Pmi_parm);
			break;

		    case PmO_EOL:
			if (*Pm_cp == '\0')
				result = OK;
			if (result == testfor && ip->Pmi_parm)
				ERR_printf2("%s\n", ip->Pmi_parm);
			break;

		    case PmO_WhSp:
			if (isspace(*Pm_cp)) {
				while (isspace(*Pm_cp)) Pm_cp++;
				result = OK;
			}
			if (result == testfor && ip->Pmi_parm)
				ERR_printf2("%s\n", ip->Pmi_parm);
			break;

		    default:
			ERR_printf3("?? op %d for class %d\n",
						ip->Pmi_op, ip->Pmi_class);
			result = ERR;
			break;
		    }
			/*
			 *	If recognition failed, restore scan pointer.
			 */
		    if (result != OK)
			Pm_cp = save_cp;
		    break;

		default:
			ERR_printf2("Unknown class %d\n", ip->Pmi_class);
			return(ERR);
		}
		if (Pm_debug)
			printf("RESULT: %s\n\n", Result_name[result+Pm_Ret+3]);

		if (result == ERR)
			return ERR;
		else if (result == testfor) {
			if (ip->Pmi_next <= (OK))
				return(ip->Pmi_next);
			Pm_PC += ip->Pmi_next - 1;
		}
	}
}


u_int32_t
host_atoi32(char *cp)
        {
        struct hostent *hp;

        if (isdigit(*cp))
                return inet_addr(cp);
        else if ((hp = gethostbyname(cp)))
                return *(u_int32_t *) hp->h_addr;
        else    return((u_int32_t) -1);
}

int
Pm_Init(Pm_inst *Pm_mem)
	{
	Pm_inst *ip, *ipd;
	int	i, N, L;

	N = L = 0;
	for (ip = Pm_mem; ; ip++, N++) {
		if (ip->Pmi_class == PmC_LABEL) {
			if (ip->Pmi_next == 0)
				break;
			L = (L > ip->Pmi_next)? L:ip->Pmi_next;
		}
	}
	L++;
	ipd = ip + L;
	while (ip >= Pm_mem) {
		*ipd-- = *ip--;
	}
	ipd = Pm_mem;
	for (i=0; i < L; i++, ipd++) {
		memset((char *)ipd, 0, sizeof(Pm_inst));
		ipd->Pmi_class = PmC_TRV;
	}
	for (ip = Pm_mem + L, N = L; ; ip++, N++) {
		if (ip->Pmi_class == PmC_LABEL) {
			if (ip->Pmi_next == 0)
				break;
			(Pm_mem+ip->Pmi_next)->Pmi_next = N + 1;
		}
	}
	return(1);
}

void
Pm_print(int Pm_PC, Pm_inst *ip)
	{
	char Next[8];

	if (ip->Pmi_next <= OK)
		sprintf(Next, "%s", Result_name[ip->Pmi_next+Pm_Ret+3]);
	else
		sprintf(Next, "%d", ip->Pmi_next);
		
	if (ip->Pmi_class == PmC_OP || ip->Pmi_class == PmC_NOT_OP)
		printf("Pm: %4d:  %8s %8s %6s",
			Pm_PC, Pm_Class_names[ip->Pmi_class],
			Pm_Op_names[ip->Pmi_op], Next);
	else if (ip->Pmi_class == PmC_ACTION || ip->Pmi_class == PmC_SET) {
		printf("Pm: %4d:  %8s %d %6s %d\n",
			Pm_PC, Pm_Class_names[ip->Pmi_class],
			ip->Pmi_op, Next, (int) ip->Pmi_parm);
		return;
	}
	else
		printf("Pm: %4d:  %8s %d %6s",
			Pm_PC, Pm_Class_names[ip->Pmi_class],
			ip->Pmi_op, Next);
	printf("  %s\n", (ip->Pmi_parm)? ip->Pmi_parm : "(null)");
}

void
Pm_list(Pm_inst *Pm_mem)
	{
	Pm_inst *ip;
	int	 pc = 0;

	for (ip = Pm_mem; ip->Pmi_class != PmC_END; ip++, pc++)
		Pm_print(pc, ip);
}

	
