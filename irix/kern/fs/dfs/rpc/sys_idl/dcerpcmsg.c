/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 *
 * HISTORY
 * $Log: dcerpcmsg.c,v $
 * Revision 65.2  1999/02/04 22:37:20  mek
 * Convert SAMS file to C for IRIX kernel integration.
 *
 * Revision 65.1  1997/10/20 19:16:20  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.6.2  1996/03/09  23:26:48  marty
 * 	Update OSF copyright year
 * 	[1996/03/09  22:42:21  marty]
 *
 * Revision 1.1.6.1  1995/12/08  00:23:35  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/20  22:19 UTC  jrr
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/11/17  17:10 UTC  dat  /main/dat_xidl2/1]
 * 
 * 	HP revision /main/HPDCE02/1  1995/06/05  19:06 UTC  wesfrost
 * 	Merge changes to mainlline
 * 
 * 	HP revision /main/wesfrost_man1/1  1995/06/05  12:28 UTC  wesfrost
 * 	Correct typos
 * 	[1995/12/08  00:01:15  root]
 * 
 * Revision 1.1.2.13  1994/08/25  15:54:54  hopkins
 * 	Merged with changes from 1.1.2.12
 * 	[1994/08/25  15:54:40  hopkins]
 * 
 * 	Final partial checkin of doc-cleanup work.
 * 	[1994/08/24  22:16:48  hopkins]
 * 
 * 	Checkin of partial doc-cleanup work (undocument
 * 	all the obsolete messages that are nonetheless
 * 	in the table to avoid breaking anyone).
 * 	[1994/08/24  20:21:07  hopkins]
 * 
 * 	Checkin of partial doc-cleanup work.
 * 	[1994/08/24  20:03:17  hopkins]
 * 
 * Revision 1.1.2.12  1994/08/23  20:19:12  tom
 * 	Add new status: rpc_s_fault_codeset_conv_error (OT 10410)
 * 	[1994/08/23  20:17:28  tom]
 * 
 * Revision 1.1.2.11  1994/08/18  22:09:33  hopkins
 * 	Serviceability:
 * 	  Add new subcomponent for libidl.
 * 	  Add subcomp description messsage
 * 	  and two new messages for libidl.
 * 	[1994/08/18  21:49:43  hopkins]
 * 
 * Revision 1.1.2.10  1994/08/12  21:30:44  tatsu_s
 * 	Print the association state and event names instead of numbers.
 * 	[1994/08/11  16:34:19  tatsu_s]
 * 
 * Revision 1.1.2.9  1994/08/05  19:49:51  tom
 * 	Bug 11358 - don't print status, translate to message.
 * 	[1994/08/05  19:06:57  tom]
 * 
 * Revision 1.1.2.8  1994/07/28  18:58:27  tom
 * 	Bug 11399 - Remove rpc_svc_ from sub-component names.
 * 	[1994/07/28  18:53:26  tom]
 * 
 * Revision 1.1.2.7  1994/06/21  21:54:21  hopkins
 * 	Added some new messages.
 * 	[1994/06/08  21:35:05  hopkins]
 * 
 * Revision 1.1.2.6  1994/06/10  20:55:05  devsrc
 * 	cr10871 - fix copyright
 * 	[1994/06/10  15:00:31  devsrc]
 * 
 * Revision 1.1.2.5  1994/05/23  19:01:40  hopkins
 * 	Make the rpc_s_mod entry "not incatalog",
 * 	so that a zero message ID isn't written
 * 	to the .msf file -- gencat doesn't like it.
 * 	[1994/05/23  18:19:00  hopkins]
 * 
 * Revision 1.1.2.4  1994/05/20  22:16:07  hopkins
 * 	Merged with changes from 1.1.2.3
 * 	[1994/05/20  22:15:53  hopkins]
 * 
 * 	Un-obsolete all the obsolete messages,
 * 	since it turns out lots of the _are_
 * 	referenced in code.
 * 	Add rpc_s_mod and set the starting message
 * 	id to zero ("value 0").  This change is
 * 	dependent on a change to sams that prevents
 * 	the zero message from getting written to the
 * 	message file -- gencat chokes on a zero message.
 * 	[1994/05/20  20:53:56  hopkins]
 * 
 * Revision 1.1.2.3  1994/05/20  22:03:38  sommerfeld
 * 	Add value 0/rpc_s_mod ..
 * 	[1994/05/20  21:39:42  sommerfeld]
 * 
 * Revision 1.1.2.2  1994/05/19  21:15:17  hopkins
 * 	More serviceability.
 * 	[1994/05/18  21:25:57  hopkins]
 * 
 * 	Merge with DCE1_1.
 * 
 * 	Serviceability work.
 * 	[1994/05/03  20:26:26  hopkins]
 * 
 * Revision 1.1.2.1  1994/04/08  00:47:56  hopkins
 * 	Serviceability: new file
 * 	[1994/04/08  00:47:11  hopkins]
 * 
 * $EndLog$
 */

/* Generated from ../../../../../src/rpc/sys_idl/rpc.sams on 1999-01-04-16:56:34.000 */
/* Do not edit! */
#include <dce/dce.h>
#include <dce/dce_msg.h>
dce_msg_table_t rpc_g_svc_msg_table[338] = {
    { 0x16c9a001, "Operation out of range" },
    { 0x16c9a002, "Cannot create socket" },
    { 0x16c9a003, "Cannot bind socket" },
    { 0x16c9a004, "not in call" },
    { 0x16c9a005, "no port" },
    { 0x16c9a006, "Wrong boot time" },
    { 0x16c9a007, "too many sockets" },
    { 0x16c9a008, "illegal register" },
    { 0x16c9a009, "cannot receive" },
    { 0x16c9a00a, "bad packet" },
    { 0x16c9a00b, "unbound handle" },
    { 0x16c9a00c, "address in use" },
    { 0x16c9a00d, "Input arguments too big" },
    { 0x16c9a00e, "string too long" },
    { 0x16c9a00f, "too many objects" },
    { 0x16c9a010, "Not authenticated" },
    { 0x16c9a011, "Unknown authentication service" },
    { 0x16c9a012, "No memory for RPC runtime" },
    { 0x16c9a013, "cannot nmalloc" },
    { 0x16c9a014, "Call faulted" },
    { 0x16c9a015, "call failed" },
    { 0x16c9a016, "Communications failure" },
    { 0x16c9a017, "RPC daemon communications failure" },
    { 0x16c9a018, "illegal family rebind" },
    { 0x16c9a019, "invalid handle" },
    { 0x16c9a01a, "RPC coding error" },
    { 0x16c9a01b, "Object not found" },
    { 0x16c9a01c, "Call thread not found" },
    { 0x16c9a01d, "Invalid binding" },
    { 0x16c9a01e, "Already registered" },
    { 0x16c9a01f, "Endpoint not found" },
    { 0x16c9a020, "Invalid RPC protocol sequence" },
    { 0x16c9a021, "Descriptor not registered" },
    { 0x16c9a022, "Already listening" },
    { 0x16c9a023, "No protocol sequences" },
    { 0x16c9a024, "No protocol sequences registered" },
    { 0x16c9a025, "No bindings" },
    { 0x16c9a026, "Max descriptors exceeded" },
    { 0x16c9a027, "No interfaces" },
    { 0x16c9a028, "Invalid timeout" },
    { 0x16c9a029, "Cannot inquire socket" },
    { 0x16c9a02a, "Invalid network address family ID" },
    { 0x16c9a02b, "Invalid network address" },
    { 0x16c9a02c, "Unknown interface" },
    { 0x16c9a02d, "Unsupported type" },
    { 0x16c9a02e, "Invalid call option" },
    { 0x16c9a02f, "No fault" },
    { 0x16c9a030, "Cancel timeout" },
    { 0x16c9a031, "Call canceled" },
    { 0x16c9a032, "Invalid call handle" },
    { 0x16c9a033, "cannot allocate association" },
    { 0x16c9a034, "Cannot connect" },
    { 0x16c9a035, "Connection aborted" },
    { 0x16c9a036, "Connection closed" },
    { 0x16c9a037, "Cannot accept" },
    { 0x16c9a038, "Association group not found" },
    { 0x16c9a039, "stub interface error" },
    { 0x16c9a03a, "Invalid object" },
    { 0x16c9a03b, "invalid type" },
    { 0x16c9a03c, "invalid interface operation number" },
    { 0x16c9a03d, "different server instance" },
    { 0x16c9a03e, "Protocol error" },
    { 0x16c9a03f, "cannot receive message" },
    { 0x16c9a040, "Invalid string binding" },
    { 0x16c9a041, "Connection request timed out" },
    { 0x16c9a042, "Connection request rejected" },
    { 0x16c9a043, "Network is unreachable" },
    { 0x16c9a044, "Not enough resources for connection at local or remote host" },
    { 0x16c9a045, "Network is down" },
    { 0x16c9a046, "Too many connections at remote host" },
    { 0x16c9a047, "Endpoint does not exist at remote host" },
    { 0x16c9a048, "Remote host is down" },
    { 0x16c9a049, "Remote host is unreachable" },
    { 0x16c9a04a, "Access control information invalid at remote host" },
    { 0x16c9a04b, "Connection aborted by local host" },
    { 0x16c9a04c, "Connection closed by remote host" },
    { 0x16c9a04d, "Remote host crashed" },
    { 0x16c9a04e, "Invalid endpoint format for remote host" },
    { 0x16c9a04f, "unknown status code" },
    { 0x16c9a050, "Unknown or unsupported manager type" },
    { 0x16c9a051, "association creation failed" },
    { 0x16c9a052, "Association group maximum exceeded" },
    { 0x16c9a053, "association group allocation failed" },
    { 0x16c9a054, "Invalid state machine state" },
    { 0x16c9a055, "association request rejected" },
    { 0x16c9a056, "Association shutdown" },
    { 0x16c9a057, "Transfer syntaxes unsupported" },
    { 0x16c9a058, "Context id not found" },
    { 0x16c9a059, "cannot listen on socket" },
    { 0x16c9a05a, "No addresses" },
    { 0x16c9a05b, "Cannot get peer name" },
    { 0x16c9a05c, "Cannot get interface ID" },
    { 0x16c9a05d, "Protocol sequence not supported" },
    { 0x16c9a05e, "Call orphaned" },
    { 0x16c9a05f, "Who are you failed" },
    { 0x16c9a060, "Unknown reject" },
    { 0x16c9a061, "Type already registered" },
    { 0x16c9a062, "stop listening disabled" },
    { 0x16c9a063, "Invalid argument" },
    { 0x16c9a064, "Not supported" },
    { 0x16c9a065, "Wrong kind of binding" },
    { 0x16c9a066, "Authentication authorization mismatch" },
    { 0x16c9a067, "Call queued" },
    { 0x16c9a068, "Cannot set nodelay" },
    { 0x16c9a069, "Not an RPC tower" },
    { 0x16c9a06a, "invalid RPC protocol ID" },
    { 0x16c9a06b, "invalid RPC tower floor" },
    { 0x16c9a06c, "Call timed out" },
    { 0x16c9a06d, "Management operation disallowed" },
    { 0x16c9a06e, "Manager not entered" },
    { 0x16c9a06f, "Calls too large for well known endpoint" },
    { 0x16c9a070, "Server too busy" },
    { 0x16c9a071, "Protocol version mismatch" },
    { 0x16c9a072, "RPC protocol version mismatch" },
    { 0x16c9a073, "No import cursor" },
    { 0x16c9a074, "Fault address error" },
    { 0x16c9a075, "Fault context mismatch" },
    { 0x16c9a076, "Fault floating point divide by zero" },
    { 0x16c9a077, "Fault floating point error" },
    { 0x16c9a078, "Fault floating point overflow" },
    { 0x16c9a079, "Fault floating point underflow" },
    { 0x16c9a07a, "Fault illegal instruction" },
    { 0x16c9a07b, "Fault integer divide by zero" },
    { 0x16c9a07c, "Fault integer overflow" },
    { 0x16c9a07d, "Fault invalid bound" },
    { 0x16c9a07e, "Fault invalid tag" },
    { 0x16c9a07f, "Fault pipe closed" },
    { 0x16c9a080, "Fault pipe communication error" },
    { 0x16c9a081, "Fault pipe discipline" },
    { 0x16c9a082, "Fault pipe empty" },
    { 0x16c9a083, "Fault pipe memory" },
    { 0x16c9a084, "Fault pipe order" },
    { 0x16c9a085, "Fault remote communication failure" },
    { 0x16c9a086, "Fault remote no memory" },
    { 0x16c9a087, "Fault unspecified" },
    { 0x16c9a088, "Bad UUID version" },
    { 0x16c9a089, "Socket failure" },
    { 0x16c9a08a, "get_configuration failure" },
    { 0x16c9a08b, "No IEEE 802 hardware address" },
    { 0x16c9a08c, "overrun" },
    { 0x16c9a08d, "Internal error" },
    { 0x16c9a08e, "UUID coding error" },
    { 0x16c9a08f, "Invalid string UUID" },
    { 0x16c9a090, "no memory" },
    { 0x16c9a091, "no more entries" },
    { 0x16c9a092, "unknown name service error" },
    { 0x16c9a093, "Name service unavailable" },
    { 0x16c9a094, "Incomplete name" },
    { 0x16c9a095, "group not found" },
    { 0x16c9a096, "Invalid name syntax" },
    { 0x16c9a097, "No more members" },
    { 0x16c9a098, "no more interfaces" },
    { 0x16c9a099, "invalid name service" },
    { 0x16c9a09a, "no name mapping" },
    { 0x16c9a09b, "profile not found" },
    { 0x16c9a09c, "Not found" },
    { 0x16c9a09d, "no updates" },
    { 0x16c9a09e, "update failed" },
    { 0x16c9a09f, "no match exported" },
    { 0x16c9a0a0, "Entry not found" },
    { 0x16c9a0a1, "Invalid inquiry context" },
    { 0x16c9a0a2, "Interface not found" },
    { 0x16c9a0a3, "Group member not found" },
    { 0x16c9a0a4, "Entry already exists" },
    { 0x16c9a0a5, "name service initialization failure" },
    { 0x16c9a0a6, "Unsupported name syntax" },
    { 0x16c9a0a7, "No more profile elements" },
    { 0x16c9a0a8, "No permission for name service operation" },
    { 0x16c9a0a9, "Invalid inquiry type" },
    { 0x16c9a0aa, "Profile element not found" },
    { 0x16c9a0ab, "Profile element replaced" },
    { 0x16c9a0ac, "import already done" },
    { 0x16c9a0ad, "database busy" },
    { 0x16c9a0ae, "invalid import context" },
    { 0x16c9a0af, "uuid set not found" },
    { 0x16c9a0b0, "uuid member not found" },
    { 0x16c9a0b1, "No interfaces exported" },
    { 0x16c9a0b2, "tower set not found" },
    { 0x16c9a0b3, "tower member not found" },
    { 0x16c9a0b4, "object uuid not found" },
    { 0x16c9a0b5, "No more bindings" },
    { 0x16c9a0b6, "Invalid priority" },
    { 0x16c9a0b7, "Not an RPC entry" },
    { 0x16c9a0b8, "invalid lookup context" },
    { 0x16c9a0b9, "Binding vector full" },
    { 0x16c9a0ba, "Cycle detected" },
    { 0x16c9a0bb, "Nothing to export" },
    { 0x16c9a0bc, "Nothing to unexport" },
    { 0x16c9a0bd, "Invalid interface version option" },
    { 0x16c9a0be, "no RPC data" },
    { 0x16c9a0bf, "Member picked" },
    { 0x16c9a0c0, "Not all objects unexported" },
    { 0x16c9a0c1, "No entry name" },
    { 0x16c9a0c2, "Priority group done" },
    { 0x16c9a0c3, "Partial results" },
    { 0x16c9a0c4, "Name service environment variable not defined" },
    { 0x16c9a0c5, "Unknown sockaddr" },
    { 0x16c9a0c6, "Unknown tower" },
    { 0x16c9a0c7, "Not implemented" },
    { 0x16c9a0c8, "Max calls (threads) too small" },
    { 0x16c9a0c9, "cthread create failed" },
    { 0x16c9a0ca, "cthread pool exists" },
    { 0x16c9a0cb, "No such cthread pool" },
    { 0x16c9a0cc, "cthread invoke disabled" },
    { 0x16c9a0cd, "Cannot perform endpoint map operation" },
    { 0x16c9a0ce, "No memory for endpoint map service" },
    { 0x16c9a0cf, "Invalid endpoint database" },
    { 0x16c9a0d0, "Cannot create endpoint database" },
    { 0x16c9a0d1, "Cannot access endpoint database" },
    { 0x16c9a0d2, "Endpoint database already open by another process" },
    { 0x16c9a0d3, "Invalid endpoint entry" },
    { 0x16c9a0d4, "Cannot update endpoint database" },
    { 0x16c9a0d5, "Invalid endpoint map or lookup context" },
    { 0x16c9a0d6, "Not registered in endpoint map" },
    { 0x16c9a0d7, "endpoint mapper unavailable" },
    { 0x16c9a0d8, "Name is underspecified" },
    { 0x16c9a0d9, "Invalid name service handle" },
    { 0x16c9a0da, "Unidentified communications error in client stub" },
    { 0x16c9a0db, "Could not open file containing ASCII/EBCDIC translation tables" },
    { 0x16c9a0dc, "File containing ASCII/EBCDIC translation tables is damaged" },
    { 0x16c9a0dd, "A client side context handle has been incorrectly modified" },
    { 0x16c9a0de, "Null value of [in] context handle or all [in,out] context handles" },
    { 0x16c9a0df, "Persistent errors on socket" },
    { 0x16c9a0e0, "Requested protection level is not supported" },
    { 0x16c9a0e1, "Received packet has an invalid checksum" },
    { 0x16c9a0e2, "Credentials invalid" },
    { 0x16c9a0e3, "Credentials too large for packet" },
    { 0x16c9a0e4, "Call ID in packet unknown" },
    { 0x16c9a0e5, "Key ID in packet unknown" },
    { 0x16c9a0e6, "Decrypt integrity check failed" },
    { 0x16c9a0e7, "Authentication ticket expired" },
    { 0x16c9a0e8, "Authentication ticket not yet valid" },
    { 0x16c9a0e9, "Authentication request is a replay" },
    { 0x16c9a0ea, "Authentication ticket is not for destination" },
    { 0x16c9a0eb, "Authentication ticket/authenticator do not match" },
    { 0x16c9a0ec, "Clock skew too great to authenticate" },
    { 0x16c9a0ed, "Incorrect network address in authentication request" },
    { 0x16c9a0ee, "Authentication protocol version mismatch" },
    { 0x16c9a0ef, "Invalid authentication message type" },
    { 0x16c9a0f0, "Authentication message stream modified" },
    { 0x16c9a0f1, "Authentication message out of order" },
    { 0x16c9a0f2, "Authentication key version not available" },
    { 0x16c9a0f3, "Authentication service key not available" },
    { 0x16c9a0f4, "Mutual authentication failed" },
    { 0x16c9a0f5, "Incorrect authentication message direction" },
    { 0x16c9a0f6, "Alternative authentication method required" },
    { 0x16c9a0f7, "Incorrect sequence number in authentication message" },
    { 0x16c9a0f8, "Inappropriate authentication checksum type" },
    { 0x16c9a0f9, "Authentication field too long for implementation" },
    { 0x16c9a0fa, "Received packet has an invalid CRC" },
    { 0x16c9a0fb, "Binding incomplete (no object ID and no endpoint)" },
    { 0x16c9a0fc, "Key function not allowed when default authentication service specified" },
    { 0x16c9a0fd, "Interface's stub/runtime version is unknown" },
    { 0x16c9a0fe, "Interface's version is unknown" },
    { 0x16c9a0ff, "RPC protocol not supported by this authentication protocol" },
    { 0x16c9a100, "Authentication challenge malformed" },
    { 0x16c9a101, "Protection level changed unexpectedly" },
    { 0x16c9a102, "No manager EPV available" },
    { 0x16c9a103, "Stub or runtime protocol error" },
    { 0x16c9a104, "RPC class version mismatch" },
    { 0x16c9a105, "Helper process not running" },
    { 0x16c9a106, "Short read from kernel helper" },
    { 0x16c9a107, "Helper process catatonic" },
    { 0x16c9a108, "Helper process aborted" },
    { 0x16c9a109, "Feature not supported in kernel" },
    { 0x16c9a10a, "Attempting to use credentials belonging to another user" },
    { 0x16c9a10b, "Helper process too busy" },
    { 0x16c9a10c, "DG protocol needs reauthentication." },
    { 0x16c9a10d, "Receiver cannot support authentication subtype" },
    { 0x16c9a10e, "Wrong type of pickle passed to unpickling routine" },
    { 0x16c9a10f, "Listener thread is not running" },
    { 0x16c9a110, "Buffer not usable by IDL Encoding Services" },
    { 0x16c9a111, "Action cannot be performed by IDL Encoding Services" },
    { 0x16c9a112, "Wrong version of IDL Encoding Services" },
    { 0x16c9a113, "User defined exception received" },
    { 0x16c9a114, "Conversion between codesets not possible" },
    { 0x16c9a115, "Transaction not started before operation" },
    { 0x16c9a116, "Transaction open failed at server" },
    { 0x16c9a117, "Credentials have been fragmented" },
    { 0x16c9a118, "I18N tags structure is not valid" },
    { 0x16c9a119, "Unsupported attribute type was given to NSI" },
    { 0x16c9a11a, "Invalid character input for conversion" },
    { 0x16c9a11b, "No room to place the converted characters" },
    { 0x16c9a11c, "iconv failed other than conversion operation" },
    { 0x16c9a11d, "No compatible code set found" },
    { 0x16c9a11e, "Character sets are not compatible" },
    { 0x16c9a11f, "Code set registry access succeeded." },
    { 0x16c9a120, "Value not found in the code set registry" },
    { 0x16c9a121, "No local code set name exists in the code set registry" },
    { 0x16c9a122, "Cannot open the code set registry file" },
    { 0x16c9a123, "Cannot read the code set registry file" },
    { 0x16c9a124, "Cannot allocate memory for code set info" },
    { 0x16c9a125, "Cleanup failed within an evaluation routine." },
    { 0x16c9a144, "Illegal state transition detected in CN server association state machine [cur_state: %s, cur_event: %s, assoc: %x]" },
    { 0x16c9a145, "Illegal state transition detected in CN client association state machine [cur_state: %s, cur_event: %s, assoc: %x]" },
    { 0x16c9a146, "Illegal state transition detected in CN server association group state machine [cur_state: %d, cur_event: %d, grp: %x]" },
    { 0x16c9a147, "Illegal state transition detected in CN client association group state machine [cur_state: %d, cur_event: %d, grp: %x]" },
    { 0x16c9a148, "Illegal state transition detected in CN server call state machine [cur_state: %d, cur_event: %d, call_rep: %x]" },
    { 0x16c9a149, "Illegal state transition detected in CN client call state machine [cur_state: %d, cur_event: %d, call_rep: %x]" },
    { 0x16c9a14a, "(%s) Illegal or unknown packet type: %x" },
    { 0x16c9a14b, "(receive_packet) assoc->%x %s: Protocol version mismatch - major->%x minor->%x" },
    { 0x16c9a14c, "(receive_packet) assoc->%x frag_length %d in header > fragbuf data size %d" },
    { 0x16c9a14d, "(%s) Unsupported stub/RTL IF version" },
    { 0x16c9a14e, "(%s) Unhandled call state: %s" },
    { 0x16c9a14f, "%s failed: %s" },
    { 0x16c9a150, "%s: call failed" },
    { 0x16c9a151, "%s failed, errno = %d" },
    { 0x16c9a152, "%s on server failed: %s" },
    { 0x16c9a153, "%s on client failed: %s" },
    { 0x16c9a154, "(%s) Error message will not fit in packet" },
    { 0x16c9a155, "(%s) Unexpected search attribute seen" },
    { 0x16c9a156, "(%s) Negotiated transfer syntax not found in presentation context element" },
    { 0x16c9a157, "(%s) Inconsistency in ACC_BYTCNT field" },
    { 0x16c9a158, "(%s) Pre-v2 interface spec" },
    { 0x16c9a159, "(%s) Unknown interface spec version" },
    { 0x16c9a15a, "(%s) Socket's maximum receive buffering is less than NCA Connection Protocol minimum requirement" },
    { 0x16c9a15b, "(%s) Unaligned RPC_CN_PKT_AUTH_TRL" },
    { 0x16c9a15c, "(%s) Unexpected exception was raised" },
    { 0x16c9a15d, "(%s) No stub data to send" },
    { 0x16c9a15e, "(%s) Event list full" },
    { 0x16c9a15f, "(%s) Unknown socket type" },
    { 0x16c9a160, "(%s) Call not implemented" },
    { 0x16c9a161, "(%s) Invalid call sequence number" },
    { 0x16c9a162, "(%s) Can't create UUID" },
    { 0x16c9a163, "(%s) Can't handle pre-v2 server stubs" },
    { 0x16c9a164, "(%s) DG packet free pool is corrupted" },
    { 0x16c9a165, "(%s) Attempt to free already-freed DG packet" },
    { 0x16c9a166, "(%s) Lookaside list is corrupted" },
    { 0x16c9a167, "(%s) Memory allocation failed" },
    { 0x16c9a168, "(%s) Memory reallocation failed" },
    { 0x16c9a169, "(%s) Can't open file %s." },
    { 0x16c9a16a, "(%s) Can't read address from file %s." },
    { 0x16c9a16c, "Out of memory while trying to run down contexts of client %x" },
    { 0x16c9a16d, "Exception in routine at %x, running down context %x of client %x" },
    { 0x16c9a16e, "Fault codeset conversion error" },
    { 0x16c9a16f, "codeset conversion error" },
    { 0x16c9a170, "Fault client stub not linked into application" },
    { 0x16c9a171, "Server stub not linked into application" }
};
