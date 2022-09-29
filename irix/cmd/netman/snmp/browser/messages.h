#ifndef __messages_h
#define __messages_h

/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	error and warning messages for browser
 *
 *	$Revision: 1.11 $
 *	$Date: 1992/10/19 23:57:24 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#define STARTING_MSG		0
#define NODE_NOT_FOUND_MSG	1
#define PARENT_NOT_FOUND_MSG	2
#define NO_REMOTE_MSG		3
#define NO_RESPONSE_MSG		4
#define NO_MIB_MSG		5
#define NO_SUCH_NAME_MSG	6
#define BAD_VALUE_MSG		7
#define READ_ONLY_OBJECT_MSG	8
#define NO_VARS_MSG		9
#define GET_FIRST_MSG		10
#define INTERNAL_TOO_LONG_MSG	11
#define INTERNAL_GENERIC_MSG	12
#define ENCODE_MSG		13
#define DECODE_MSG		14
#define RECEIVE_MSG		15
#define TIMEOUT_MSG		16
#define MALLOC_MSG		17
#define TAB_NOT_VAR_MSG         18
#define GROUP_NOT_VAR_MSG       19
#define VAR_IN_TAB_MSG          20
#define NAME_NOT_IN_MIB_VIEW    21
#define ENTRY_NOT_VAR_MSG       22
#define NOFILE_MSG		23

static char *Message[] = {

    "Starting Browser", 
    "Couldn't find node", 
    "Couldn't find parent node", 
    "Send Error: Unable to communicate with the remote node", 
    "Time expired. Agent did not respond. Check whether the agent is running on the node; check whether the community name that you entered in the main window corresponds to what the agent expects and also whether your host is authorized for the service you requested on the remote host", 
    "MIB tree was not created", 
    "No such name. The given variable is not found in the MIB view", 
    "Bad value", 
    "Trying to write to read-only object", 
    "SNMP protocol error", 
    "Nothing to set in this table yet. Do a Get first", 
    "Internal Error: String is too long, even up to first blank", 
    "SNMP Generic error", 
    "Encode Error: Internal Error", 
    "Decode Error: Internal Error", 
    "Receive Error", 
    "Timeout Error", 
    "Malloc Error: Internal Error", 
    "Variable is a Table",
    "Variable is a Group",
    "Variable is in a Table. It requires an instance",
    "Variable name not in the MIB tree",
    "Variable is a list of table attributes. Specify the id of the attribute and its instance",
    "No file name; use Save As",
};



#endif /* __messages_h_ */
