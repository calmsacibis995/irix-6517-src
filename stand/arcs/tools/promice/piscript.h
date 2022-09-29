/*	pscript.h - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - This file contains the syntax for all parsing operations 
	
	Format: break characters are 'bt,/:='
	literal command &
	$ - read a string
	# - read a decimal number
	* - read a hex number
	: - read unit id
	% - read embedded number
	@ - read a list of zero or more hex numbers
	& - read a list of zero or more decimal numbers
	last index - 56
*/

#ifdef ANSI
void pisyn(char *in, char *scripts[], void (*psynf[])(void));
#else
void pisyn();
#endif

#ifdef PDGLOB
char	*pcmfsyn[] = {

/* initialization file parameters */

	"write&:*(49",			/* control write_enable bits on AI */
	"word&#&(23",			/* specify ROM configuration - word size */
	"version&:#(48",		/* report LoadICE and PromICE ver #s */
	"verify&#(22",			/* don't verify down-loaded data */
	"ulock&(39",			/* check for locked units -sepcial feature */
	"timer&#(58",			/* tell PromICE not to use the internal timer */
	"test&#(20",			/* test PromICE memory */
	"stop&(19",				/* put PromICEs in LOAD mode */
	"status&(59",			/* display target status */
	"socket&:%(36",			/* set target socket size */
	"slow&(54",				/* PromICE xmit to simulate 9600 baud */
	"sleep&#(84",			/* sleep for a bit */
	"serial&:#(21",			/* return PromICE serial # */
	"search&**$(10",		/* find string in ROM */
	"save&$:@(45",			/* save ROM contents to file */
	"rom&:%(18",			/* set emulation ROM size */
	"resetfp&(72",			/* reset network host */
	"reset&#(44",			/* specify target reset period */
	"restart&(42",			/* restart host/PromICE link */
	"reqack&#(82",			/* use req/ack for read/write */
#ifdef MSDOS
	"output&%*(15",			/* specify serial port to use */
	"pponly&%*(17",			/* bidipp */
	"ppbus&%*(53",			/* ppbusmode */
	"parallel&%*(16", 		/* parallel port name */
#else
	"output&$(15",			/* specify serial device name */
	"pponly&$(17",			/* bidirectional pport */
	"ppbus&$(53",			/* pport is a bus */
	"parallel&$(16",		/* parallel device name */
#endif
	"ppmode&#(46",			/* set pport control parameters */
	"number&#(14",			/* prespecify number of units on linke (UNIX) */
	"noverify&(22",			/* don't verify down-loaded data */
	"notimer&#(58",			/* tell PromICE not to use the internal timer */
	"nomap&#(41",			/* do do that */
	"nocursor&#(56",		/* no spinning curser */
	"nochecksum&#(24",		/* don't check checksum in hex file data */
	"noautoreset&#(73",		/* don't control target reset per mode */
	"noautorecovery&(47",	/* don't intiate host/PromICE link recovery */
	"noautorecover&(47",	/* don't intiate host/PromICE link recovery */
	"noaddrerr&#(26",		/* ignore data that won't fit/fall in ROM */
	"move&:***(13",			/* move ROM data around */
	"modeout&$(77",			/* specify mode on disconnect */
	"modein&$(76",			/* specify PromICE mode on connect */
	"map&#(41",				/* do do that */
	"log&$#(70",			/* start a log file for PromICE traffic */
	"load&(12",				/* always load files (when dialog) */
	"light&#(57",			/* turn on diagnostics on RUN light */
	"k&:***+##(11",			/* compute and store ROM checksum */
	"intlen&#(67",			/* set target interrupt length */
	"image&$*:*&(35",		/* ditto */
	"hso&***(9",			/* program target interrupt polarity & activity */
	"holda&#(82",			/* use hold/holdack for read/write */
	"high&(8",				/* PromICE to xmit serial full rate */
	"help&$(34",			/* display built in help messages */
	"gxint&#(69",			/* set global-cmd-done int to target */
	"go&(33",				/* put all PromICEs to emulate mode */
	"gasrd&#(68",			/* set global-async-read in PromICE */
	"fn&#$(37",				/* specify Function key */
	"find&**#@(10",			/* find binary pattern in ROM */
	"fill&:@(7",			/* specify fill parameters */
	"file&$*:*&(32",		/* ditto */
	"ffill&:@(7",			/* force fill same as fill */
	"fastport&$(60",		/* network host name */
	"fast&&(51",			/* stretch STROBE on pp on fast hosts */
	"exit&##(50",			/* exit LoadICE */
	"edit&:*@(5",			/* edit ROM data */
	"dump&:@(4",			/* dump ROM data */
	"display&*(30",			/* set LoadICE output level */
	"dialog&(52",			/* enter dialog mode */
	"delay&#(25",			/* vary or disable timeout delay */
	"cursor&#(56",			/* no spinning curser */
	"cserr&##(81",			/* checksum error control */
	"config&$(29",			/* display system configuration */
	"compare&(3",			/* compare ROM data against files */
	"checksum&:***+##(11",	/* compute and store ROM checksum */
	"burst&#(74",			/* set AI for burst mode target access */
	"baud&#(2",				/* specify serial baud rate to use */
	"bank&#(71",			/* emulate multiple banks */
	"autoreset&#(73",		/* don't control target reset per mode */
	"autorecovery&(47",		/* don't intiate host/PromICE link recovery */
	"autorecover&(47",		/* don't intiate host/PromICE link recovery */
	"altfn&#$(83",			/* same for ALTF key */
	"altf&#$(83",			/* same for ALTF key */
	"aitreset&#(62",		/* enable - reset target on host int */
	"aitint&#(61",			/* enable - int target on host int */
	"aiswitch&#(27",		/* specify AIdataPBX port for PromICE */
	"aireset&#(63",			/* enable - reset AI on host int */
	"airci&#(66",			/* enable above for cmd mode AI ops */
	"ainorci&#(65",			/* disable per rcv char int to target */
	"aimode&#(78",			/* select different AI options */
	"ailoc&:*#*#(1",		/* AI transparant link setup */
	"aifast&#(64",			/* use fast AI transparant mode (no buf) */
	"aicontrol&#(75",		/* select different AI operational modes */
	"afn&#$(83",			/* same for ALTF key */
	"addrerr&##(80",		/* ignore data that won't fit/fall in ROM */
	"!&!(31",				/* escape command to system shell */
	0
	};

char	*pcmlsyn[] = {

/*	command line arguments: */
	
	"z&#(26",				/* ignore data out-of-bounds */
	"x&#(24",				/* ignore checksum in hex files */
	"W&:*(49",				/* enable/disable AI write enables */
	"w&#&(23",				/* set ROM words size */
	"ver&:#(48",			/* report LoadICE and PromICE ver #s */
	"v&(22",				/* don't verify down-loaded data */
	"U&*(47",				/* control autorecover of host link */
	"ts&#(20",				/* test PromICE memory */
	"t&:##(20",				/* test PromICE memory */
	"T&(58",				/* disable timeout */
	"stop&(19",				/* put PromICEs in LOAD mode */
	"st&(59",				/* report status */
	"sn&:#(21",				/* return PromICE serial # */
	"se&**$(10",			/* find string in ROM */
	"sa&$:@(45",			/* save ROM contents to file */
	"s&:%(36",				/* socket size on target */
	"S&(54",				/* emulate 9600 of PromICE xmit */
	"rfp&#(72",				/* reset network host */
	"ra&#(82",				/* use req/ack for read/write */
	"r&:%(18",				/* ROM size on target */
	"R&#(44",				/* set target reset period */
#ifdef MSDOS
	"o&%*(15",				/* serial device name */
	"q&%*(17",				/* bidirectional parallel port name */
	"pb&%*(53",				/* parallel port bus mode device name */
	"p&%*(16",				/* parallel port down-load device name */
#else
	"o&$(15",				/* serial device name */
	"q&$(17",				/* bidirectional parallel port name */
	"pb&$(53",				/* parallel port bus mode device name */
	"p&$(16",				/* parallel port down-load device name */
#endif
	"P&#(46",				/* set parallel port mode */
	"O&#(41",				/* control data map display */
	"noar&#(73",			/* disable auto target reset */
	"n&#(14",				/* number of PromICEs on link (UNIX) */
	"mo&$(77",				/* specify mode on disconnect */
	"mi&$(76",				/* specify PromICE mode on connect */
	"m&:***(13",			/* move ROM data around */
	"M&#(39",				/* check for locked units - special */
	"log&$#(70",			/* start a log file for PromICE traffic */
	"li&(57",				/* turn on RUN light diagnostic */
	"L&(79",				/* do not load before dialog */
	"l&(12",				/* do load before dialog */
	"k&:***+##(11",			/* ROM checksum parameters */
	"j&$*:*&(36",			/* formatted binary file */
	"il&#(67",				/* set target interrupt length */
	"I&***(9",				/* set auxilliary signal polarity */
	"i&$*:*&(35",			/* binary file */
	"H&&(51",				/* fast host - STROBE stetch */
	"ha&#(82",				/* use hold/holdack for read/write */
	"h&(8",					/* full speed serial response */
	"gr&(68",				/* set global-async-read in PromICE */
	"go&(33",				/* put all PromICEs to emulate mode */
	"gi&(69",				/* set global-cmd-done int to target */
	"fp&$(60",				/* network host name */
	"fn&#$(37",				/* specify Function key */
	"fi&**#@(10",			/* find */
	"fa&&(51",				/* fast host - STROBE stetch */
	"f&:@(7",				/* fill specifications */
	"F&(40",				/* don't fill ROMs */
	"e&:*@(5",				/* edit parameters */
	"dl&#(25",				/* set timeout delay */
	"D&*(30",				/* change LoadICE display level */
	"d&(52",				/* enter dialog mode */
	"cu&#(56",				/* enable or disable spinner */
	"c&(3",					/* compare instead of down-load */
	"C&$(29",				/* display system configuration */
	"ba&#(71",				/* emulate multiple banks */
	"B&#(74",				/* control burst mode for AI */
	"b&#(2",				/* specify serial baud rate */
	"ar&#(73",				/* control target reset per mode */
	"altfn&#$(83",			/* same for ALTF key */
	"altf&#$(83",			/* same for ALTF key */
	"aitr&#(62",			/* enable - reset target on host int */
	"aiti&#(61",			/* enable - int target on host int */
	"aire&#(63",			/* enable - reset AI on host int */
	"airi&#(66",			/* enable above for cmd mode AI ops */
	"ainri&#(65",			/* disable per rcv char int to target */
	"aim&#(78",				/* select different AI options */
	"aif&#(64",				/* use fast AI transparant mode (no buf) */
	"aic&#(75",				/* select different AI operational modes */
	"ai&#(1",				/* set AI transparant mode parameters */
	"afn&#$(83",			/* same for ALTF key */
	"a&:*#*#(1",			/* internal AI test command */
	"A&#(27",				/* AIdataPBX port */
	"@&$(47",				/* loadice.ini file alternate */
	"~$*:*&(32",			/* default is hex data file */
	0
	};

char	*pusrsyn[] = {

/*	interactive dialog mode commands: */
	
	"z&#(26",				/* control address out of range error */
	"X&##(50",				/* exit */
	"x&##(50",				/* ditto */
	"write&:*(49",			/* control write_enable bits on AI */
	"word&#&(23",			/* set ROM word size */
	"W&:*(49",				/* control write_enable bits on AI */
	"w&#&(23",				/* ditto */
	"version&:#(48",		/* report LoadICE and PromICE ver #s */
	"verify&#(22",			/* do or don't verify down-loaded data */
	"v&:#(48",				/* ditto */
	"timer&#(58",			/* tell PromICE not to use the internal timer */
	"test&:##(20",			/* test PromICE memory */
	"t&:##(20",				/* ditto */
	"T&#(58",				/* tell PromICE not to use the internal timer */
	"stop&(19",				/* put PromICEs in LOAD mode */
	"status&(59",			/* display target status */
	"st&(59",				/* ditto */
	"socket&:%(36",			/* set SOCKET size */
	"sn&:#(21",				/* return PromICE serial # */
	"slow&(54",				/* PromICE xmit to simulate 9600 baud */
	"sleep&#(84",			/* sleep */
	"sl&#(84",				/* sleep */
	"serial&:#(21",			/* return PromICE serial # */
	"search&**$(10",		/* find string in ROM */
	"save&$:@(45",			/* save ROM contents to file */
	"S&**$(10",				/* search */
	"s&$:@(45",				/* save */
	"rom&:%(18",			/* set ROM size */
	"restart&(42",			/* restart host/PromICE link */
	"reset&#(44",			/* reset target */
	"reqack&#(82",			/* use req/ack for read/write */
	"ra&#(82",				/* use req/ack for read/write */
	"R&:%(18",				/* set ROM size */
	"r&#(44",				/* reset target */
	"quit&##(50",			/* ditto */
	"move&:***(13",			/* more ROM data around */
	"mo&$(77",				/* specify mode on disconnect */
	"map&#(41",				/* turn data map on and off */
	"m&:***(13",			/* move */
	"log&#(70",				/* turn loging on or off */
	"load&$*:*&(12",		/* load files */
	"limage&$*:*&(38",		/* load this file and reinit list */
	"light&#(57",			/* turn on diagnostics on RUN light */
	"li&$*:*&(38",			/* load image, binary file */
	"l&$*:*&(12",			/* load hexfile */
	"k&:***+##(11",			/* compute and store checksum */
	"j&$*:*&(36",			/* formatted binary file */
	"intlen&#(67",			/* set target interrupt length */
	"image&$*:*&(35",		/* binary file specs */
	"il&#(67",				/* set target interrupt length */
	"I&***(9",				/* program target interrupt polarity & activity */
	"i&$*:*&(35",			/* binary file */
	"hso&***(9",			/* program target interrupt polarity & activity */
	"high&(8",				/* PromICE to xmit serial full rate */
	"help&$(34",			/* display built in help messages */
	"ha&#(82",				/* use hold/holdack for read/write */
	"h&$*:*&(32",			/* ditto */
	"gxint&(69",			/* set global-cmd-done int to target */
	"gr&(68",				/* set global-async-read in PromICE */
	"go&(33",				/* put all PromICEs to emulate mode */
	"gi&(69",				/* set global-cmd-done int to target */
	"gasrd&(68",			/* set global-async-read in PromICE */
	"fn&#$(37",				/* specify Function key */
	"find&**#@(10",			/* find binary pattern in ROM */
	"fill&:@(7",			/* fill ROMs */
	"file&$*:*&(32",		/* hex file specs */
	"fast&&(51",			/* stretch STROBE on pp on fast hosts */
	"f&:@(7",				/* fill */
	"F&**#@(10",			/* find */
	"exit&##(50",			/* exit LoadICE */
	"edit&:*@(5",			/* edit ROM data */
	"e&:*@(5",				/* ditto */
	"dump&:@(4",			/* dump ROM data */
	"dl&#(25",				/* set timeout delay */
	"display&*(30",			/* change LoadICE output level */
	"delay&#(25",			/* set timeout delay */
	"d&:@(4",				/* ditto */
	"D&*(30",				/* Ditto */
	"cursor&#(56",			/* enable or disable spinner */
	"cserr&##(81",			/* ignore checksum errors in hex data */
	"config&$(29",			/* ditto */
	"compare&(3",			/* compare ROM data against files */
	"checksum&:***+##(11",	/* do ROM checksum */
	"c&(3",					/* compare ROM data against files */
	"C&$(29",				/* display config */
	"burst&#(74",			/* set AI for burst mode target access */
	"bu&#(74",				/* set AI for burst mode target access */
	"bank&#(71",			/* emulate multiple banks */
	"ba&#(71",				/* emulate multiple banks */
	"auto&*(47",			/* control target reset */
	"ar&*(47",				/* control target reset */
	"altfn&#$(83",			/* same for ALTF key */
	"altf&#$(83",			/* same for ALTF key */
	"aitreset&#(62",		/* enable - reset target on host int */
	"aitr&#(62",			/* enable - reset target on host int */
	"aitint&#(61",			/* enable - int target on host int */
	"aiti&#(61",			/* enable - int target on host int */
	"aiswitch&#(27",		/* specify AIdataPBX port for PromICE */
	"aireset&#(63",			/* enable - reset AI on host int */
	"aire&#(63",			/* enable - reset AI on host int */
	"airci&#(66",			/* enable above for cmd mode AI ops */
	"airi&#(66",			/* enable above for cmd mode AI ops */
	"ainorci&#(65",			/* disable per rcv char int to target */
	"ainri&#(65",			/* disable per rcv char int to target */
	"aimode&#(78",			/* select different AI options */
	"aim&#(78",				/* select different AI options */
	"ailoc&:*#*#(1",		/* AI transparant link setup */
	"aifast&#(64",			/* use fast AI transparant mode (no buf) */
	"aif&#(64",				/* use fast AI transparant mode (no buf) */
	"aicontrol&#(75",		/* select different AI operational modes */
	"aic&#(75",				/* select different AI operational modes */
	"ai&:*#*#(28",			/* set AI tty link parameters */
	"afn&#$(83",			/* same for ALTF key */
	"addrerr&##(80",		/* ignore out-of-bound data */
	"?&$(34",				/* help */
	".j&$*:*&(36",			/* formatted binary same way */
	".image&$*:*&(35",		/* same for binary files */
	".i&$*:*&(35",			/* yep */
	".h&$*:*&(32",			/* same */
	".file&$*:*&(32",		/* hex file specs and start file list anew */
	"!&!(31",				/* escape command to system shell */
	0
	};
#else
extern char *pcmfsyn[];
extern char *pcmlsyn[];
extern char *pusrsyn[];
#endif
