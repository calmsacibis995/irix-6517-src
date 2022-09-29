/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.10 $
 */

/***********************************************************
	Copyright 1988, 1989 by Carnegie Mellon University

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <errno.h>

#include <sm_types.h>
#include <smt_asn1.h>
#include <smt_snmp.h>
#include <smt_snmp_impl.h>
#include <smt_snmp_api.h>
#include <smtd_parm.h>
#include <smt_snmp_clnt.h>
#include <smt_parse.h>
#include <sm_log.h>
#include <smtd.h>


struct synch_state snmp_synch_state;

struct snmp_pdu *
snmp_pdu_create(command)
	int command;
{
	struct snmp_pdu *pdu;

	pdu = (struct snmp_pdu *)Malloc(sizeof(struct snmp_pdu));
	pdu->command = command;
	pdu->errstat = SNMP_DEFAULT_ERRSTAT;
	pdu->errindex = SNMP_DEFAULT_ERRINDEX;
	pdu->address.sin_addr.s_addr = SNMP_DEFAULT_ADDRESS;
	pdu->enterprise = NULL;
	pdu->enterprise_length = 0;
	pdu->variables = NULL;
	return(pdu);
}

/*
 * Add a null variable with the requested name to the end of the list of
 * variables for this pdu.
 */
void
snmp_add_null_var(pdu, name, name_length)
    	struct snmp_pdu *pdu;
    	oid *name;
    	int name_length;
{
    	struct variable_list *vars;

    	if (pdu->variables == NULL) {
		vars = (struct variable_list *)Malloc(sizeof(*vars));
		pdu->variables = vars;
    	} else {
		vars = pdu->variables;
		while (vars->next_variable)
			vars = vars->next_variable;
		vars->next_variable =
			(struct variable_list *)Malloc(sizeof(*vars));
		vars = vars->next_variable;
    	}

    	vars->next_variable = NULL;
    	vars->name = (oid *)Malloc(name_length * sizeof(oid));
    	bcopy(name, vars->name, name_length * sizeof(oid));
    	vars->name_length = name_length;
    	vars->type = ASN_NULL;
    	vars->val.string = NULL;
    	vars->val_len = 0;
}

int
snmp_synch_input(op, session, reqid, pdu, magic)
    int op;
    struct snmp_session *session;
    int reqid;
    struct snmp_pdu *pdu;
    void *magic;
{
    struct variable_list *var, *newvar;
    struct synch_state *state = (struct synch_state *)magic;
    struct snmp_pdu *newpdu;

    if (reqid != state->reqid)
	return 0;
    state->waiting = 0;
    if (op == RECEIVED_MESSAGE && pdu->command == GET_RSP_MSG){
	/* clone the pdu */
	state->pdu = newpdu = (struct snmp_pdu *)Malloc(sizeof(struct snmp_pdu));
	bcopy(pdu, newpdu, sizeof(struct snmp_pdu));
	newpdu->variables = 0;
	var = pdu->variables;
	if (var != NULL){
	    newpdu->variables = newvar = (struct variable_list *)Malloc(sizeof(struct variable_list));
	    bcopy(var, newvar, sizeof(struct variable_list));
	    if (var->name != NULL){
		newvar->name = (oid *)Malloc(var->name_length * sizeof(oid));
		bcopy(var->name, newvar->name, var->name_length*sizeof(oid));
	    }
	    if (var->val.string != NULL){
		newvar->val.string = Malloc(var->val_len);
		bcopy(var->val.string, newvar->val.string, var->val_len);
	    }
	    newvar->next_variable = 0;
	    while(var->next_variable){
		newvar->next_variable = (struct variable_list *)Malloc(sizeof(struct variable_list));
		var = var->next_variable;
		newvar = newvar->next_variable;
		bcopy(var, newvar, sizeof(struct variable_list));
		if (var->name != NULL){
		    newvar->name = (oid *)Malloc(var->name_length * sizeof(oid));
		    bcopy(var->name, newvar->name, var->name_length * sizeof(oid));
		}
		if (var->val.string != NULL){
		    newvar->val.string = Malloc(var->val_len);
		    bcopy(var->val.string, newvar->val.string, var->val_len);
		}
		newvar->next_variable = 0;
	    }
	}
	state->status = STAT_SUCCESS;
    } else if (op == TIMED_OUT){
	state->status = STAT_TIMEOUT;
    }
    return 1;
}


/*
 * If there was an error in the input pdu, creates a clone of the pdu
 * that includes all the variables except the one marked by the errindex.
 * The command is set to the input command and the reqid, errstat, and
 * errindex are set to default values.
 * If the error status didn't indicate an error, the error index didn't
 * indicate a variable, the pdu wasn't a get response message, or there
 * would be no remaining variables, this function will return NULL.
 * If everything was successful, a pointer to the fixed cloned pdu will
 * be returned.
 */
struct snmp_pdu *
snmp_fix_pdu(pdu, command)
	struct snmp_pdu *pdu;
	int command;
{
	struct variable_list *var, *newvar;
	struct snmp_pdu *newpdu;
	int index, copied = 0;

	if (pdu->command != GET_RSP_MSG ||
	    pdu->errstat == SNMP_ERR_NOERROR ||
	    pdu->errindex <= 0)
		return NULL;

	/* clone the pdu */
	newpdu = (struct snmp_pdu *)Malloc(sizeof(struct snmp_pdu));
	bcopy(pdu, newpdu, sizeof(struct snmp_pdu));
	newpdu->variables = 0;
	newpdu->command = command;
	newpdu->reqid = SNMP_DEFAULT_REQID;
	newpdu->errstat = SNMP_DEFAULT_ERRSTAT;
	newpdu->errindex = SNMP_DEFAULT_ERRINDEX;
	var = pdu->variables;
	index = 1;
	if (pdu->errindex == index){	/* skip first variable */
		var = var->next_variable;
		index++;
	}
	if (var == NULL)
		goto fix_done;

	newvar = (struct variable_list *)Malloc(sizeof(struct variable_list));
	newpdu->variables = newvar;
	bcopy(var, newvar, sizeof(struct variable_list));
	if (var->name != NULL){
		newvar->name = (oid *)Malloc(var->name_length * sizeof(oid));
		bcopy(var->name, newvar->name, var->name_length*sizeof(oid));
	}
	if (var->val.string != NULL){
		newvar->val.string = Malloc(var->val_len);
		bcopy(var->val.string, newvar->val.string, var->val_len);
	}
	newvar->next_variable = 0;
	copied++;

	while(var->next_variable){
		var = var->next_variable;
		if (++index == pdu->errindex)
			continue;
		newvar->next_variable =(struct variable_list *)Malloc(sizeof(struct variable_list));
		newvar = newvar->next_variable;
		bcopy(var,newvar,sizeof(struct variable_list));
		if (var->name != NULL){
			newvar->name=(oid*)Malloc(var->name_length*sizeof(oid));
			bcopy(var->name, newvar->name,
			      var->name_length * sizeof(oid));
		}
		if (var->val.string != NULL){
			newvar->val.string = Malloc(var->val_len);
			bcopy(var->val.string,
			      newvar->val.string, var->val_len);
		}
		newvar->next_variable = 0;
		copied++;
	}

fix_done:
	if (index < pdu->errindex || copied == 0){
		snmp_free_pdu(newpdu);
		return NULL;
	}
	return(newpdu);
}


int
snmp_synch_response(
	struct snmp_session *ss,
	struct snmp_pdu *pdu,
	struct snmp_pdu **response)
{
	fd_set fdset;
	int numfds, count, block;
	struct timeval timeout, *tvp;
	struct synch_state *state = &snmp_synch_state;

	if ((state->reqid = snmp_send(ss, pdu)) == 0){
		*response = NULL;
		snmp_free_pdu(pdu);
		return(STAT_ERROR);
	}

	state->waiting = 1;
	state->pdu = NULL;

	while(state->waiting) {
		numfds = 0;
		FD_ZERO(&fdset);
		block = 1;
		tvp = &timeout;
		timerclear(tvp);
		snmp_select_info(&numfds, &fdset, tvp, &block);
		if (block == 1)
	    		tvp = NULL;	/* block without timeout */
		sm_log(LOG_DEBUG,0,"select(%d,%x,0,0,%d)\n",numfds,fdset,block);
		count = select(numfds, &fdset, 0, 0, tvp);
		if (count > 0){
	    		snmp_read(&fdset);
		} else switch(count) {
	    		case 0:
				snmp_timeout();
				break;
	    		case -1:
				if (errno == EINTR)
		    			continue;
		    		perror("select");
	    		/* FALLTHRU */
	    		default:
				return STAT_ERROR;
		}
	}
	*response = state->pdu;
	return(state->status);
}

void
snmp_synch_setup(struct snmp_session *session)
{
	session->callback = snmp_synch_input;
	session->callback_magic = (void *)&snmp_synch_state;
}

static char *error_string[6] = {
	"No Error",
	"Response message would have been too large",
	"Can't find variable name in this MIB",
	"The value given has the wrong type or length",
	"This variable is read only",
	"A general error occured"
};
char *
snmp_errstring(int errstat)
{
	if ((errstat <= SNMP_ERR_GENERR) && (errstat >= SNMP_ERR_NOERROR))
		return(error_string[errstat]);
	return("Unknown Error");
}
