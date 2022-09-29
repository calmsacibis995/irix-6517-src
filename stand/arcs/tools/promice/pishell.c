/*	PiShell.c - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "piconfig.h"
#include "piscript.h"
#include "pistruct.h"
#include "pierror.h"
#include "pihelp.h"

#ifdef MSDOS
char *pidefs1 = "output=com1";
#endif

#ifdef UNIX
char *pidefs1 = "output=/dev/ttyb";
#endif

#ifdef MAC
char *pidefs1 = "output=modem";
#endif

char *pidefs2 = "baud=19200";
char *utimes = "DDD:HH:MM:SS";

#ifdef ANSI
extern void (*psynf[])(void);
extern char *getenv(char *var);
extern void piemu(void);
extern void pilod(void);
extern void pcinit(void);
extern void pcexit(void);
extern void pcxinit(void);
static void pikeys(unsigned char c);
static void utime(short code);
void pishell(void)

#else
extern void (*psynf[])();
extern char *getenv();
extern void piemu();
extern void pilod();
extern void pcinit();
extern void pcexit();
extern void pcxinit();
static void pikeys();
static void utime();
void pishell()
#endif
	{
	short i;
	short skip;
	FILE *cf;
	unsigned char c;
	char *cstr,**hp;
	extern int errno;
	char ifile[PIC_FN];

	utime(0);	/* init timer */

	/* if user did just 'loadice ?' or 'loadice help' */

	if (pxcline[0] == '?' || !strcmp(pxcline,"help"))
		{
		hp = piclhlp;
		while(*hp)
			{
			printf("\n %s",*hp);
			if (**hp == '-')
				(void)fgets(pxuline,PIC_CL,stdin);
			hp++;
			}
		return;
		}

#ifndef MACAPPL
	pisyn(pidefs1,pcmfsyn,psynf);
	pisyn(pidefs2,pcmfsyn,psynf);
#endif

	/* if 'ini' file is on the command line as '@inifile' */

	if (pxcline[0] == '@')
		{
		cstr = ifile;
		for (i=0; ((i<(short)strlen(pxcline)) && (i<(PIC_FN-1))); i++)
			{
			pxcline[i] = ' ';
			if (pxcline[i+1] == ' ')
				break;
			else
				ifile[i] = pxcline[i+1];
			}
		ifile[i] = '\0';
		pxcline[i+1] = ' ';
		}
	else		/* look for environment variable or use default */
		{
		cstr = getenv("LOADICE");
		if (cstr == NULL)
			cstr = "loadice.ini";
		}

#ifdef MACAPPL
		cstr = "\0";
#endif

	if (pxcline[0] != '.') /* if command line start with '.' then don't do it */
		{
		if (pxdisp&PXHI)
				printf("\n Opening command file '%s'",cstr);
		if ((cf = fopen(cstr,"r")) != NULL)
			{
			while (fgets(pxuline,PIC_CL,cf) != NULL)
				{
				if (pxuline[0] == '*')	/* ignore comment lines */
					continue;
				if (pxuline[0] == '.')	/* execute command NOW */
					{
					pxuline[0] = ' ';
					piflags |= PiII;
					}
				else
					piflags &= ~PiII;
				pisyn(pxuline,pcmfsyn,psynf);	/* call parser */
				if (pxerror)
					{
					pierror();
					(void)fclose(cf);
					return;
					}
				pi_estr1 = (char *)0;
				}
			(void)fclose(cf);
			}
		else
			{
			if (pxdisp&PXHI)
				printf("\n File not found '%s' - proceeding",cstr);
			errno = 0;
			}
		}
	else
		pxcline[0] = ' '; /* get rid of the '.' */

	if (pxcline[0] != '\0')
		{
		if (pxdisp&PXHI)
			printf("\n Executing command line '%s'",pxcline);
		piflags |= PiCM;
		pisyn(pxcline,pcmlsyn,psynf);
		piflags &= ~PiCM;
		}

	if (pxerror)
		pierror();
	else	/* possibly dialog mode */
		{
		piflags |= PiiX;

		/* if wants to load, has files and no dialog mode requested */

		if (!(pxflags&POXX) && (pxflags&POLO || (pxnfile && !(pxflags&POIX))))
			{
			pisyn("l",pusrsyn,psynf);
			if (pxerror)
				{
				pierror();
				piemu();
				pcexit();
				return;
				}
			}

		/* if dialog mode requested or no files given (force dialog) */

		if ((pxflags&POIX) || !pxnfile)
			{
#ifdef MACAPPL
			pim_cmd();
#endif
			if (pxdisp&PXHI)
					printf("\n Entering dialog mode");
			pxflags &= ~POXX;
			pcinit();
			if (!pxerror)
				{
				for(;;)
					{
					if (pxerror)
						pierror();
					else
						piflags &= ~PiER;
					if (!(piflags&PiUP))
						{
						if (pxflags&POAR)
							{
							if (pxdisp&PXVH)
								printf(" Auto-recvery...");
							pisyn("restart",pusrsyn,psynf);
							if (pxerror)
								continue;
							}
						else
							if (pxdisp&PXVH)
								printf("\nCan't talk to the units - type 'restart'");
						}
					if (pxdisp&PXVH)
						{
						utime(1);
						printf("\n\n%s",utimes);
							printf("LoadICE: ");
						}
					piflags &= ~PiCR;
#ifdef MSDOS	/* we will do console I/O so we can handle special keys */
					skip = 0;
					i = 0;
					do
						{
						while (!(c = kbhit()));
						c = (unsigned char)getch();
						if (c == '\b')		/* process back-space */
							{
							if (i)
								{
								pxuline[i--] = '\0';
								putch('\b');
								putch(' ');
								putch('\b');
								}
							continue;
							}

						/* if any special key struck then we catch it here */

						if ((c == 0) || (c == 0x0E0) || (c == 0x1b))
							{
							skip = 1;
							if (c != 0x1b)
								c = (unsigned char)getch();
							switch (c)
								{
								case 0x1b:			/* ESC key - switch mode */
									if (piflags&PiMU)
										{
										pilod();
										if (!pxerror)
											printf("\n Now in Load Mode!");
										}
									else
										{
										piemu();
										if (!pxerror)
											printf("\n Now Emulating!");
										}
									break;
								default:	/* FUNCTION keys */
									if (c<0x45 && c>0x3a)
										c -= 0x3b;
									else
										if (c<0x72 && c>0x67)
											c -= 0x5c;
										else
											if (c<0x08d && c>0x084)
												{
												c -= 0x085;
												if (c<2)
													c += 10;
												else
													c += 16;
												}
									pikeys(c);	/* process the key */
								}
							}
						else		/* normal key - make line */
							{
							putch(c);
							if ((c != '\r') && (c != '\n'))
								pxuline[i++] = c;
							}
						} while ((c != '\r') && (!skip));
					if (pxerror)
						pierror();
					if (skip)
						continue;
					if (!i)			/* if just CR typed, do last command */
						{
						pxuline[0] = pxfirst;
						piflags |= PiCR;
						}
					else
						pxuline[i] = '\0'; /* else make sure line is mt */
					if (strlen(pxuline))
#else
					pibusy = 0;
					if (gets(pxuline))
#endif
						{
#ifdef MAC
						if (cstr=strrchr(pxuline,':'))
							strcpy(pxuline, cstr+2);
#endif
						pibusy = 1;
						if (!strlen(pxuline))
							{
							pxuline[0] = pxfirst;
							piflags |= PiCR;
							}
						pxfirst = pxuline[0];
						if (pxuline[0] == '.')	/* '.' means immediate */
							piflags |= PiII;
						else
							piflags &= ~PiII;
						utime(0);				/* reset timer */
						pisyn(pxuline,pusrsyn,psynf);	/* do command */
						if (pxerror)
							pierror();
						if (piflags&(PiHO|PiRQ|PiMO|PiAB))
							pcxinit();
						}
					if (pxflags&POXX)
						break;
					pi_estr1 = (char *)0;
					}
				}
			else
				pierror();
#ifdef MACAPPL
			pim_ucmd();
#endif
			}
		pcexit();	/* shut down link to PROMICEs */
		}
	}

/* `pikeys` - process function keys */

#ifdef MSDOS
static void pikeys(unsigned char c)
	{
	short i;

	if (c>23)	/* if not function keys then list keys */
		for (i=0; i<24; i++)
			{
			if (i<12)
				printf("\n    F%d\t= ",i+1);
			else
				printf("\n ALTF%d\t= ",i-11);
			if (pxkeys[i])
				printf("%s",pxkeys[i]);
			else
				printf("...Key NOT Assigned!");
			}
	else
		{
		if (pxkeys[c] == NULL)	/* assigned */
			pxerror = PGE_KEY;
		else
			{
			printf("%s",pxkeys[c]);	/* echo command */
			pisyn(pxkeys[c],pusrsyn,psynf);	/* do command */
			}
		}
	}

#else
static void pikeys(c)
unsigned char c;
	{
	}
#endif

/* `utime` - time stamp the prompt */

#ifdef ANSI
static void utime(short code)
#else
static void utime(code)
short code;
#endif
	{
	static time_t t;
	time_t tt;
	short day,hour,minute;

	if (!code)
		t = time(NULL);
	else
		{
		day = hour = minute = 0;
		tt = time(NULL) - t;
		if (tt >= 86400)
			{
			day = tt/86400;
			tt %= 86400;
			}
		if (tt >= 3600)
			{
			hour = tt/3600;
			tt %= 3600;
			}
		if (tt >= 60)
			{
			minute = tt/60;
			tt %= 60;
			}
		if (day)
			{
			sprintf(utimes,"%03d|%02d:%02d:%02d:",day,hour,minute,tt);
			return;
			}
		if (hour)
			{
			sprintf(utimes,"%02d:%02d:%02d:",hour,minute,tt);
			return;
			}
		if (minute)
			{
			sprintf(utimes,"%02d:%02d:",minute,tt);
			return;
			}
		sprintf(utimes,"%02d:",tt);
		}
	}
