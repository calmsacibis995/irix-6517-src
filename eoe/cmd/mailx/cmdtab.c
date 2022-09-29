/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

#ifdef notdef
static char sccsid[] = "@(#)cmdtab.c	5.5 (Berkeley) 2/18/88";
#endif /* notdef */

#include "def.h"

/*
 * Mail -- a mail program
 *
 * Define all of the command names and bindings.
 */

extern int type(), preserve(), delete_msg(), undelete_msg(), next();
extern int shell(), schdir(), save_msg(), help();
extern int headers(), pdot(), respond(), editor();
extern int edstop(), rexit(), pcmdlist(), sendmail(), from(), copycmd();
extern int messize(), psalloc(), deltype(), unset(), set(), source();
extern int pversion(), group(), top(), null(), stouch(), visual();
extern int swrite(), dosh(), file(), echo(), Respond(), scroll(), ifcmd();
extern int elsecmd(), endifcmd(), mboxit(), clobber(), alternates();
extern int local(), folders(), igfield(), Type(), retfield(), more(), More();
extern int unread();	/* , Header(); */

extern int esctocmd(), tintr(), grabhdrs(), addirt(), addkey(), addt();
extern int setsubj(), addcc(), setcomments(), addbcc(), putdead(), getfile();
extern int addref(), addrtnrcpt(), addreplyto(), putfile(), fgetmessage();
extern int helpme(), printmessage(), pipemessage(), veditmsgbody();
extern int eeditmsgbody(), setencrypt(), getdead(), mgetmessage();
extern int graballhdrs(), veditmsg(), eeditmsg(), prtdeleted();

/* XPG4 related */
extern int Copycmd(), followup(), Followup(), Reply(), reply(), Save_msg();
extern int pipecmd(), Mgetmessage(), Fgetmessage();

/* XPG4 related -- tilde (~) commands */
extern int insert_val(), insert_sign(), insert_Sign();

struct cmd cmdtab[] = {
	"next",		next,		NDMLIST,	0,	MMNDEL,
	"alias",	group,		T|M|ALIASLIST,	0,	1000,
	"print",	type,		MSGLIST,	0,	MMNDEL,
	"type",		type,		MSGLIST,	0,	MMNDEL,
	"Type",		Type,		MSGLIST,	0,	MMNDEL,
	"Print",	Type,		MSGLIST,	0,	MMNDEL,
	"visual",	visual,		T|I|MSGLIST,	0,	MMNORM,
	"top",		top,		MSGLIST,	0,	MMNDEL,
	"touch",	stouch,		T|W|MSGLIST,	0,	MMNDEL,
	"preserve",	preserve,	T|W|MSGLIST,	0,	MMNDEL,
	"delete",	delete_msg,	T|W|P|MSGLIST,	0,	MMNDEL,
	"dp",		deltype,	W|MSGLIST,	0,	MMNDEL,
	"dt",		deltype,	W|MSGLIST,	0,	MMNDEL,
	"undelete",	undelete_msg,	T|P|MSGLIST,	MDELETED,MMNDEL,
	"unset",	unset,		T|M|RAWLIST,	1,	1000,
	"mail",		sendmail,	T|R|M|I|STRLIST,0,	0,
	"mbox",		mboxit,		T|W|MSGLIST,	0,	0,
	"more",		more,		MSGLIST,	0,	MMNDEL,
	"page",		more,		MSGLIST,	0,	MMNDEL,
	"More",		More,		MSGLIST,	0,	MMNDEL,
	"Page",		More,		MSGLIST,	0,	MMNDEL,
	"unread",	unread,		T|MSGLIST,	0,	MMNDEL,
	"Unread",	unread,		T|MSGLIST,	0,	MMNDEL,
	"new",		unread,		T|MSGLIST,	0,	MMNDEL,
	"New",		unread,		T|MSGLIST,	0,	MMNDEL,
	"!",		shell,		T|I|STRLIST,	0,	0,
	"copy",		copycmd,	T|M|STRLIST,	0,	0,
	"Copy",		Copycmd,	T|MSGLIST,	0,	0,
	"chdir",	schdir,		T|M|STRLIST,	0,	0,
	"cd",		schdir,		T|M|STRLIST,	0,	0,
	"save",		save_msg,	T|STRLIST,	0,	0,
	"Save",		Save_msg,	T|MSGLIST,	0,	0,
	"source",	source,		T|M|STRLIST,	0,	0,
	"set",		set,		T|M|RAWLIST,	0,	1000,
	"shell",	dosh,		T|I|NOLIST,	0,	0,
	"version",	pversion,	T|M|NOLIST,	0,	0,
	"group",	group,		T|M|RAWLIST,	0,	1000,
	"write",	swrite,		T|STRLIST,	0,	0,
	"from",		from,		T|MSGLIST,	0,	MMNORM,
	"file",		file,		T|M|RAWLIST,	0,	1,
	"fo",		followup,	T|R|I|MSGLIST,	0,	MMNDEL,
	"F",		Followup,	T|R|I|MSGLIST,	0,	MMNDEL,
	"folder",	file,		T|M|RAWLIST,	0,	1,
	"folders",	folders,	T|M|RAWLIST,	0,	1,
	"followup",	followup,	T|R|I|MSGLIST,	0,	MMNDEL,
	"Followup",	Followup,	T|R|I|MSGLIST,	0,	MMNDEL,
	"?",		help,		T|M|NOLIST,	0,	0,
	"z",		scroll,		T|M|STRLIST,	0,	0,
	"headers",	headers,	T|MSGLIST,	0,	MMNDEL,
	"hd",		prtdeleted,	T|M|NOLIST,	0,	0,
	"help",		help,		T|M|NOLIST,	0,	0,
	"=",		pdot,		T|NOLIST,	0,	0,
	/*
	 * Force user to use a "replyall" (ra for short) to reply to
	 * the CC list.  This avoids unecessary replies to "all".
	 */
	"Reply",	Reply,		T|R|I|MSGLIST,	0,	MMNDEL,
	"Respond",	Reply,		T|R|I|MSGLIST,	0,	MMNDEL,
	"reply",	reply,		T|R|I|MSGLIST,	0,	MMNDEL,
	"respond",	reply,		T|R|I|MSGLIST,	0,	MMNDEL,
	"ra",		respond,	T|R|I|MSGLIST,	0,	MMNDEL,
	"RA",		respond,	T|R|I|MSGLIST,	0,	MMNDEL,
	"replyall",	respond,	T|R|I|MSGLIST,	0,	MMNDEL,
	"edit",		editor,		T|I|MSGLIST,	0,	MMNORM,
	"ec",		echo,		T|M|RAWLIST,	0,	1000,
	"echo",		echo,		T|M|RAWLIST,	0,	1000,
	"quit",		edstop,		NOLIST, 	0,	0,
	"list",		pcmdlist,	T|M|NOLIST,	0,	0,
	"local",	local,		T|M|RAWLIST,	0,	1000,
	"xit",		rexit,		M|NOLIST,	0,	0,
	"exit",		rexit,		M|NOLIST,	0,	0,
	"size",		messize,	T|MSGLIST,	0,	MMNDEL,
	"hold",		preserve,	T|W|MSGLIST,	0,	MMNDEL,
	"if",		ifcmd,		T|F|M|RAWLIST,	1,	1,
	"else",		elsecmd,	T|F|M|RAWLIST,	0,	0,
	"endif",	endifcmd,	T|F|M|RAWLIST,	0,	0,
	"alternates",	alternates,	T|M|RAWLIST,	0,	1000,
	"ignore",	igfield,	T|M|RAWLIST,	0,	1000,
	"discard",	igfield,	T|M|RAWLIST,	0,	1000,
	"retain",	retfield,	T|M|RAWLIST,	0,	1000,
/*	"Header",	Header,		T|STRLIST,	0,	1000,	*/
	"#",		null,		T|M|NOLIST,	0,	0,
	"clobber",	clobber,	T|M|RAWLIST,	0,	1,
	"pipe",		pipecmd,	T|STRLIST,	0,	0,
	"|",		pipecmd,	T|STRLIST,	0,	0,
	0,		0,		0,		0,	0
};

/*
 *  To maintain compatibility for BSD Mail, the below 
 *  replaces 'cmdtab[]' entries which have conflicts with
 *  the BSD Mail entries.  The default 'cmdtab[]' entries 
 *  are for the 'mailx' version.  So, if there are any new conflicts
 *  found for compatibility between Mail and mailx, an entry should be
 *  placed in the bsdcompat_entries[] table below.  There is logic in the
 *  'main()' routine which will replace entries in the cmdtab[] with entries
 *  from the bsdcompat_entries[] table.
 */
struct cmd bsdcompat_entries[] = {
	"fo",		file,		T|M|RAWLIST,	0,	1,
	0,		0,		0,		0,	0
};

struct tcmd tcmdtab[] = {
	"!",		shell,		TSTR,
	":",		esctocmd,	TSTR,
	"-",		esctocmd,	TSTR,
	"q",		tintr,		TNOARG,
	"Q",		tintr,		TNOARG,
	"h",		grabhdrs,	THDR,
	"H",		graballhdrs,	THDR,
	"irt",		addirt,		THDRSTR,
	"k",		addkey,		THDRSTR,
	"t",		addt,		THDRSTR,
	"s",		setsubj,	THDRSTR,
	"c",		addcc,		THDRSTR,
	"cm",		setcomments,	THDRSTR,
	"b",		addbcc,		THDRSTR,
	"d",		getdead,	TBUF,
	"r",		getfile,	TBUFSTR,
	"rf",		addref,		THDRSTR,
	"rr",		addrtnrcpt,	THDRSTR,
	"rt",		addreplyto,	THDRSTR,
	"w",		putfile,	TBUFSTR,
	"m",		mgetmessage,	TBUFSTR,
	"f",		fgetmessage,	TBUFSTR,
	"?",		helpme,		TNOARG,
	"p",		printmessage,	TBUFHDR,
	"^",		pipemessage,	TBUFSTR,
	"|",		pipemessage,	TBUFSTR,
	"v",		veditmsgbody,	TBUFHDR,
	"vh",		veditmsg,	TBUFHDR,
	"V",		veditmsg,	TBUFHDR,
	"e",		eeditmsgbody,	TBUFHDR,
	"eh",		eeditmsg,	TBUFHDR,
	"E",		eeditmsg,	TBUFHDR,
	"en",		setencrypt,	THDRSTR,
	"i",		insert_val,	TBUFSTR,
	"a",		insert_sign,	TBUF,
	"A",		insert_Sign,	TBUF,
	"M",		Mgetmessage,	TBUFSTR,
	"F",		Fgetmessage,	TBUFSTR,
	"_",		esctocmd,	TSTR,
	"<",		getfile,	TBUFSTR,
	"x",		tintr,		TNOARG,
	0,		0,		0
};

/*
 *  XPG4: The below commands are invalid within a mailx start-up file.
 */
struct bcmd badcmdtab[] = {
	"!",
	"edit",
	"hold",
	"mail",
	"preserve",
	"reply",
	"Reply",
	"shell",
	"visual",
	"Copy",
	"followup",
	"Followup",
	0
};
