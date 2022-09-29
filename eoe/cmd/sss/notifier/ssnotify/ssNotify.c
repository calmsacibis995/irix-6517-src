/* Copyright 1998, Silicon Graphics, Inc.                                       */
/* ALL RIGHTS RESERVED                                                          */
/*                                                                              */
/* UNPUBLISHED -- Rights reserved under the copyright laws of the United        */
/* States.   Use of a copyright notice is precautionary only and does not       */
/* imply publication or disclosure.                                             */
/*                                                                              */
/* U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:                                    */
/* Use, duplication or disclosure by the Government is subject to restrictions  */
/* as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights */
/* in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or  */
/* in similar or successor clauses in the FAR, or the DOD or NASA FAR           */
/* Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,              */
/* 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.                        */
/*                                                                              */
/* THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY               */
/* INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,         */
/* DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY   */
/* PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON           */
/* GRAPHICS, INC.                                                               */
/*                                                                              */
/* ssNotify.c                                                                   */
/*                                                                              */
/* Don Littlefield II                                                           */
/* Silicon Graphics, Inc                                                        */

#include "ssNotify.h"
#include "logstuff.h"
#include <sys/param.h>
#include <X11/Xlib.h>

char fhostname[MAXHOSTNAMELEN];

int main( int argc, char **argv )
{
	int  que_failure(int n_type);
	int  test_file(char *);
	void set_defaults(void);
	int  check_args(void);
	int  check_files(void);
	void print_usage(void);

        int  do_gui(void);
        void do_exec(void);
        void do_email(void);
	int  do_page(void);
	void do_audio(void);
        void do_console(void);

	char *tempaddr;

        short errorflag = OFF;    /* Holds count of parameter errors   */

	extern int errno;
        extern char *optarg;
	char *eaddrs;
	char *pagerid;
	char *pagerinfo;
	char hostname[MAXHOSTNAMELEN];
	
        int c;
	int status, filestatus;

	if(argc == 1){ 
		print_usage();
		exit(EXIT_FAILURE);
	}

	strncat(fhostname, "[",1);
	gethostname(hostname, MAXHOSTNAMELEN);
	strncat(fhostname, hostname, strlen(hostname));
	strncat(fhostname, "]: ", 3);

        /*while( (c = getopt(argc, argv, "adD:c:C:E:f:F:g:i:Km:M:n:o:p:P:N:qQ:s:S:t:")) != EOF ) { */
	/*--------------------------------------------------------------------*/
	/* COMMENT-1:                                                         */
	/* Based on discussions with some of the team members, during which   */
	/* I posed the question :                                             */
	/*    Does it make sense for espnotify to exec a program ?            */
	/* The reasoning for posing this question is that espnotify is a      */
	/* notification program and a user expects some sort of notification  */
	/* that happens as a result of executing espnotify.  There are a lot  */
	/* of security concerns with executing a program and it doesn't make  */
	/* sense to execute another program from espnotify.  Execing a program*/
	/* ideally should be done via DSM rules.                              */
	/*                                                                    */
	/* It is for this reason, I've removed P:, M: and F: options from     */
	/* getopts string so that getopts complains when any of these options */
	/* are given on the command line.  However, the actual code that execs*/
	/* a program is left intact if in future we want to enable this       */
	/* feature                                                            */
	/*                                                                    */
	/* - Sri                                                              */
	/*--------------------------------------------------------------------*/
        while( (c = getopt(argc, argv, "adD:c:C:E:f:g:i:Km:n:o:p:N:qQ:s:S:t:A:")) != EOF ) {
		
                switch( c ) {
			case 'A':
				memset(acontent, 0, BUFFER);
				Aflag = ON;
				strncpy(acontent, optarg, strlen(optarg));
				break;
                        case 'd':
                                dflag = ON;
                                break;

			case 'K':
				Kflag = ON;
                                break;

			case 'n':	
				memset(priority, 0, 2);
				nflag = ON;
				strncpy(priority, optarg, 2);
				break;

                        case 'P':
				memset(exec_this, 0, BUFFER);
				Exec  = ON;
				Pflag = ON;
				strncat(exec_this, optarg, strlen(optarg));
                                break;

			case 'Q':
				memset(q_server, 0, BUFFER);
				Pager = ON;
				Qflag = ON;
				strncat(q_server, optarg, strlen(optarg));
				break;

			case 'N':
				memset(char_tries, 0, 2);
                                Nflag = ON;
                               	strncat(char_tries, optarg, 2);
                                break;

			case 'q':
				qflag = ON;
				break;

                        case 's':
				memset(subject,0, BUFFER);
				strncat(subject, fhostname, strlen(fhostname));
				Email = ON;
                                sflag = ON;
				strncat(subject, optarg, strlen(optarg));
                                break;

                        case 'm':
				memset(message,0, BUFFER);
				Email = ON;
				mflag = ON;
				strncat(message, optarg, strlen(optarg));
                                break;

                        case 'f':
				Email    = ON;
                                fflag    = ON;
				filename = optarg;
                                break;

                        case 'M':
				memset(Message,0, BUFFER);
				Exec  = ON;
				Mflag = ON;
                                strncat(Message, optarg, strlen(optarg));
                                break;

                        case 'F':
				Exec     = ON;
				Fflag    = ON;
				Filename = optarg;
                                break;

                        case 'c':
				memset(content, 0,BUFFER);
				Gui   = ON;
				cflag = ON;
				strncat(content, optarg, strlen(optarg));
                                break;

                        case 'C':
				memset(Content, 0,  BUFFER);
				strncat(Content, fhostname, strlen(fhostname));
                                Cflag = ON;
				Pager = ON;
				strncat(Content, optarg, strlen(optarg));
				orig_content = malloc(strlen(optarg)+1);
                                strcpy(orig_content, optarg);
                                break;

                        case 'o':
				Email = ON;
                                oflag = ON;
				options = optarg;
                                break;

                        case 'E':
                                eaddrs = optarg;
				Eflag = ON;
				strncat(eaddrlist, ((char*)strtok(eaddrs, ",")), BUFFER);

				while((tempaddr = (char*)strtok(NULL, ",")) != NULL) {	
                                        strncat(eaddrlist, ",", 1);
					strncat(eaddrlist, tempaddr, BUFFER);	
				}
		
				Email = ON;
                                break;

			case 'S':
				memset(Service, 0,  20);
				Sflag = ON;
				Pager = ON;
				strncat(Service, optarg, strlen(optarg));
				break;

			case 'p':
				pflag = ON;
				Pager = ON;
				pagerid = optarg;
                                strncat(pageridlist, ((char*)strtok(pagerid, ",")), BUFFER);
	
				while((tempaddr = (char*)strtok(NULL, ",")) != NULL) {
					strncat(pageridlist, ",", 1);
					strncat(pageridlist, tempaddr, BUFFER);
				}	
	
				break;
				
                        case 'D':
				memset(displayhost, 0,  BUFFER);
				Gui   = ON;
                                Dflag = ON;
                                strncat(displayhost, optarg, strlen(optarg));
                                break;

                        case 'g':
				memset(geometry, 0,  BUFFER);
				Gui   = ON;
                                gflag = ON;
                                strncat(geometry, optarg, strlen(optarg));
                                break;

                        case 'i':
				memset(iconimage, 0,  BUFFER);
				Gui   = ON;
                                iflag = ON;
				strncat(iconimage, optarg, strlen(optarg));
                                break;

                        case 'a':
                                Audio = ON;
                                break;

			case 't':
				memset(title, 0,  BUFFER);
				Gui   = ON;
				tflag = ON;
				strncat(title, fhostname, strlen(fhostname));
				strncat(title, optarg, strlen(optarg));
				break;

			case '?':
				errorflag = ON;
				break;
                }       
        }

	if(optind == 1) {
		print_usage();
		logstuff(0, "Invalid options specified.");
	}
	
	else {
		if(filestatus = check_files()) {
			if ((status = check_args()) == OK) {

				status = 0;

				set_defaults();
		
				if(Aflag) {
					do_console();
				}

				if(Email) {
					do_email();

					/* Undocumented = remove filename specified with -f after doing email */

					if(Kflag) { 
						unlink(filename);
					}
                                }
	
				if(Exec)  { do_exec(); }

				if(Pager) {
					if(do_page() == PGR_FAILURE) {
						if(!qflag) { que_failure(PGR_FAILURE); }
						status = status|PGR_FAILURE;
					}
				}

                                if(Gui)   {
                                        if(do_gui() == GUI_FAILURE) {
						if(!qflag) { que_failure(GUI_FAILURE); }
						status = status|GUI_FAILURE;
					}

                                        if(Audio) {
                                                do_audio();
                                        }
                                }
			}
			
			else {
				print_usage();
				logstuff(0, "No notifications attempted");
			}
		}
			
		else {
			status = NOTOK;
			logstuff(0, "No notifications attempted");
		}
	
	}
	free(orig_content);
	return(status);
}

/*------------------------------------------------------------------------------*/
/*                                                                              */
/*    Function: int do_gui(void)                                                */
/*     Purpose: Exec xconfirm sending -c parameter for content                  */
/*                                                                              */
/*------------------------------------------------------------------------------*/

int do_gui(void)
{
	int status = OK;

	Display *displayname;

	pid_t xconf_pid, audio_pid;
	
        displayname = XOpenDisplay(displayhost);

        if(!displayname) {
		fprintf(stderr, "\nXOpenDisplay failed for host: %s\n", displayhost);
		status = GUI_FAILURE;
		logstuff(0, "Graphical notification did not complete successfully");
        }

	else {
	
		if((xconf_pid = fork()) == -1) {
			logstuff(errno, "Error forking xconfirm for Graphical notification");
			status = GUI_FAILURE;
	                return status;
		}
	
		if(xconf_pid == 0) {
			
			close(STDOUT_FILENO);
		
			if(gflag) {
	
				if (dflag) {
					logstuff(0, "Attempting to exec sPopup with geometry string");
				}
			
				if((execl(xconfirmloc, "xconfirm", "-display",
                                          displayhost, "-header", title, "-t", content, "-icon",
                                          iconimage, "-geometry", geometry, "-b", "Close", NULL)) == -1) {
                                        logstuff(errno, "Unable to perform graphical notification");
                                        status = GUI_FAILURE;
                                }
			}
		
			else {
	
				if(dflag) {
					logstuff(0, "Attempting to exec xconfirm(1) without geometry string");
	                        }

	                        if((execl(xconfirmloc, "xconfirm", "-display",
                                          displayhost, "-header", title, "-t", content, "-icon",
                                          iconimage, "-b", "Close", NULL)) == -1) {
					logstuff(errno, "Unable to perform graphical notification");
					status = GUI_FAILURE;
				}
			}
	
			if(status == GUI_FAILURE) {
				logstuff(0, "Unable to perform Graphical notification");
				exit(EXIT_FAILURE);
		        }
		}
	}
	return status;
}

/*------------------------------------------------------------------------------*/
/*										*/
/*    Function: int do_exec(void)						*/
/*     Purpose: Exec specified program with given -M or -F option		*/
/*										*/
/*------------------------------------------------------------------------------*/

void do_exec(void)
{	
	pid_t child_pid;


	if((child_pid = fork()) == -1) {
		logstuff(errno, "Unable to exec program");
        }

	else if(child_pid == 0) {
	
		if(Mflag) {

			if(dflag) {
				logstuff(0, "Attempting exec with -m parameter");
			}			

			if((execl(exec_this, exec_this, "-m", Message, NULL)) == -1) {
				logstuff(errno, exec_this);
			}
		}
			
		else if(Fflag){

                        if(dflag) {
                                logstuff(0, "Attempting exec with -f parameter");
                        }
	
			if((execl(exec_this, exec_this, "-f",  Filename, NULL)) == -1) {
				logstuff(errno, exec_this);
			}
		}

		logstuff(0, "Program exec notification failed");
		exit(EXIT_FAILURE);
	}
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*   Function: int do_email( void )                                           */
/*                                                                            */
/*    Purpose: To send either file or message string data to specified users. */
/*                                                                            */
/*      Notes: If the -o option is specified along with the -f                */
/*             option, the corresponding Availmon actions will be taken.      */
/*                                                                            */
/*                      COMP - Compress file via 'compress'                   */
/*             If the -f is specified without the -o option, the file is      */
/*             is simply used as the email body.                              */
/*                                                                            */
/*----------------------------------------------------------------------------*/

void do_email(void)
{
	char commandline[MAXBUFSIZE];

	
	if(oflag && fflag) {

		/* Do compression on the mail */

		if(COMP) {
		        char cfile[BUFFER];

			strcat(cfile, filename);
			strcat(cfile, ".Z");

			snprintf(commandline, MAXBUFSIZE, "/usr/bsd/compress -f -c %s |"
				"/usr/bsd/uuencode %s | /usr/sbin/Mail -s \"%s\" %s",
				filename, cfile, subject, (char*)eaddrlist);
		}

		else if (ENCO) {
			
                        snprintf(commandline, MAXBUFSIZE, "/usr/bsd/uuencode %s %s | /usr/sbin/Mail -s \"%s\" %s",
                                filename, filename, subject, (char*)eaddrlist);
                }
	}

	else if((!oflag) && (fflag)) {

		if(dflag) {
			logstuff(0, "Standard e-mail.  Content is a file");
		}

		snprintf(commandline, MAXBUFSIZE, "/usr/sbin/Mail -s \"%s\" %s < %s\n",
				     subject, (char*)eaddrlist, filename);
	}
	
	else if((!oflag) && (mflag)) {

                if(dflag) {
                        logstuff(0, "Standard e-mail.  Content is a string");
                }

                snprintf(commandline, MAXBUFSIZE, "echo %s | /usr/sbin/Mail -s \"%s\" %s\n",
                                     message, subject, (char*)eaddrlist);
	}

	if((system(commandline)) == -1) {
		logstuff(errno, "Unable to perform e-mail notification");
	}
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*   Function: int do_audio( void )                                          */
/*                                                                           */
/*    Purpose: Driver which determines what audio file to play               */
/*                                                                           */
/*    Returns: An integer (OK or NOTOK) depending on if problems occured     */
/*                                                                           */
/*---------------------------------------------------------------------------*/

void do_audio(void)
{
        int status;
	pid_t audio_pid;

        if((audio_pid = fork()) == -1) {
		logstuff(errno, "Unable to perform audio notification");
        }

        if(audio_pid == 0) {

                if(dflag) {
			logstuff(0, "Attempting to exec ssplay");
                }

                if((execl(ssplayloc, "ssplay", soundfile, NULL)) == -1) {
                        logstuff(0, "Unable to perform audio notification");

			exit(EXIT_FAILURE);
                }
	}
}



/*---------------------------------------------------------------------------*/
/*                                                                           */
/*   Function: int do_page( void )                                           */
/*                                                                           */
/*    Purpose: Execs Qpage software with correct options for paging          */
/*             notification                                                  */
/*                                                                           */
/*---------------------------------------------------------------------------*/

int do_page(void)
{
	int check_qpage_server(char *server);

	int status = OK;

	pid_t childpid;


	if(Qflag) {
		status = check_qpage_server(q_server);
	}
	
	else {
		status = check_qpage_server(NULL);
	}

	if(status == PGR_FAILURE) {
		logstuff(0, "Unable to contact QuickPage Server");
		return(PGR_FAILURE);
	}
	
	if((childpid = fork()) == -1) {
 		logstuff(errno, "Unable to notify via QuickPage");
        }

        if(childpid == 0) {
	
                if(dflag) {
			logstuff(0, "Attempting to exec Qpage");
                }

                if(Sflag && Qflag) {
                        if((execl(qpageloc, "qpage", "-c", Service, "-p", pageridlist, "-s", q_server, Content, NULL)) == -1) {
                                logstuff(errno, "Pager notification did not complete successfully");
                                exit(EXIT_FAILURE);
			}
                }

		else if ((Sflag) && (!Qflag)) {
			if((execl(qpageloc, "qpage", "-c", Service, "-p" , pageridlist, Content, NULL)) == -1) {
                                logstuff(errno, "Pager notification did not complete successfully");
                                exit(EXIT_FAILURE);
                        }
                }

                else if ((!Sflag) && (Qflag)) {
                        if((execl(qpageloc, "qpage", "-p" , pageridlist, "-s", q_server, Content, NULL)) == -1) {
                                logstuff(errno, "Pager notification did not complete successfully");
                                exit(EXIT_FAILURE);
                        }
                }

                else if((!Sflag) && (!Qflag)) {
                        if((execl(qpageloc, "qpage", "-p" , pageridlist, Content, NULL)) == -1) {
                                logstuff(errno, "Pager notification did not complete successfully");
                                exit(EXIT_FAILURE);
                        }
                }
	}
        return(status);
}


int check_qpage_server( char *server ) {

	int sock;
	struct sockaddr_in addr;
	struct hostent *hp;

        addr.sin_family = AF_INET;
        addr.sin_port = htons(PGR_PORT);

	if(server == NULL) {
	        if ((hp = gethostbyname("localhost")) == NULL) {
	                herror("check_qpage_server - gethostbyname(\"localhost\")");
	                return(PGR_FAILURE);
	        }
	}

	else {
		if ((hp = gethostbyname(q_server)) == NULL) {
			herror(q_server);
			return(PGR_FAILURE);
	        } 
	}

	(void)memcpy((char *)&addr.sin_addr.s_addr, hp->h_addr_list[0], hp->h_length);

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		logstuff(errno, "socket() failure");
		return(PGR_FAILURE);
        }

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		logstuff(errno, "Could not connect() to SNPP server");
		close(sock);	
		return(PGR_FAILURE);
        }

 	else { close(sock); }

	return(OK);
}


/*---------------------------------------------------------------------------*/
/*                                                                           */
/*   Function: int test_file( char * )                                       */
/*                                                                           */
/*    Purpose: Checks whether the filename stated after the -f option        */
/*             is a valid filename to use.                                   */
/*                                                                           */
/*---------------------------------------------------------------------------*/

int test_file(char *filename)
{
	struct stat buf;
	int status = OK;
	
	if((stat(filename, &buf)) == -1) {
		logstuff(errno, filename);
		status = NOTOK;
	}

	return status;
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*   Function: void set_defaults( void )                                     */
/*                                                                           */
/*    Purpose: Set's default values for commandline options not given        */
/*                                                                           */
/*---------------------------------------------------------------------------*/

void set_defaults( void )
{
	if(!Nflag && !qflag) {
		strncat(char_tries, "5", 1);
	}
		

	if(nflag) {

		switch(atoi(priority)) {
			case 1:
		                if(dflag) {
					logstuff(0, "Setting defaults to priority 1");
		                }

				if((!iflag) && (Gui))   { strcat(iconimage, critical);   }

				if((!sflag) && (Email)) {
					strncat(subject, fhostname, strlen(fhostname));
					strcat(subject, critical_text);
				}

				if((!tflag) && (Gui)) {
					strncat(title, fhostname, strlen(fhostname));
					strcat(title, critical_text);
				}

				if(Audio)               { strcat(soundfile, "FatalError");    }
				break;
	
			case 2:

                                if(dflag) {
					logstuff(0, "Setting defaults to priority 2");
                                }

				if((!iflag) && (Gui))   { strcat(iconimage, error);    }

				if((!sflag) && (Email)) {
					strncat(subject, fhostname, strlen(fhostname));
					strcat(subject, error_text);
				}

				if((!tflag) && (Gui))   { strncat(title, fhostname, strlen(fhostname)); strcat(title, error_text);   }
				if(Audio)               { strcat(soundfile, "Error");  }
				break;
		
			case 3:

                                if(dflag) {
					logstuff(0, "Setting defaults to priority 3");
                                }

				if((!iflag) && (Gui))   { strcat(iconimage, warning);    }

				if((!sflag) && (Email)) {
					strncat(subject, fhostname, strlen(fhostname));
					strcat(subject, warning_text);
				}

				if((!tflag) && (Gui))   { strncat(title, fhostname, strlen(fhostname)); strcat(title, warning_text);   }
				if(Audio)               { strcat(soundfile, "Warning");  }
				break;
	
			case 4:

                                if(dflag) {
					logstuff(0, "Setting defaults to priority 4");
                                }

				if((!iflag) && (Gui))   { strcat(iconimage, info);    }
	
				if((!sflag) && (Email)) {
					strncat(subject, fhostname, strlen(fhostname));
					strcat(subject, info_text);
				}
	
				if((!tflag) && (Gui))   { strncat(title, fhostname, strlen(fhostname)); strcat(title, info_text);   }
				if(Audio)               { strcat(soundfile, "Info");  }
				break;
		}
	}
	
	else {
		if(dflag) {
			logstuff(0, "No priority given... Defaulting to priority 1");
		}

		if((!iflag) && (Gui))   { strcat(iconimage, info);  }

		if((!sflag) && (Email)) {
			strncat(subject, fhostname, strlen(fhostname));
			strcat(subject, info_text);
		}

		if((!tflag) && (Gui))   { strncat(title, fhostname, strlen(fhostname)); strcat(title, info_text);   }
		if(Audio)               { strcat(soundfile, "Info");      }
	}

	if((!Sflag) && (Pager)) {
		strcat(Service, default_service);
	}
}

/*------------------------------------------------------------------------------*/
/*                                                                              */
/*   Function: void print_usage( void )                                         */
/*                                                                              */
/*    Purpose: Output the relevant command options and usage information        */
/*                                                                              */
/*------------------------------------------------------------------------------*/

void print_usage(void)
{
	fprintf(stderr, "\nusage:	espnotify <options>\n");
	fprintf(stderr, "available options:\n\n");
        fprintf(stderr, "   Email Notification Options\n");
        fprintf(stderr, "      [-E emailaddress,emailaddress]  Address where notification is to be sent\n");
        fprintf(stderr, "      [-f filename]                   Text file for use as body of email\n");
        fprintf(stderr, "      [-m email_content_string]       String which will be body of email\n");
	fprintf(stderr, "      [-o processing_option]          Options are COMP or ENCO\n");
        fprintf(stderr, "      [-s email_subject]              Subject of email notification\n\n");
	fprintf(stderr, "      Note:                           -m if mutually exclusive with -f\n\n");

        fprintf(stderr, "   GUI Notification Options\n");
        fprintf(stderr, "      [-a ]                           Play audio.  Plays on host where exec'd\n");
        fprintf(stderr, "      [-c content_string]             Use content_string as content for\n");
	fprintf(stderr, "                                      notification\n");
        fprintf(stderr, "      [-D display_host]               Host to display notification on\n");
        fprintf(stderr, "      [-g geometry]                   Geometry string for notification\n");
        fprintf(stderr, "      [-i iconimage]                  Icon displayed in notification\n");
	fprintf(stderr, "                                      (info warning error critical)\n");
        fprintf(stderr, "      [-t title]                      Title for GUI notification dialog box\n\n");
	

#if 0                 
/*----------------------------------------------------------------------------*/
/* BEGIN #if 0  (See COMMENT-1 above)                                         */
/*----------------------------------------------------------------------------*/
        fprintf(stderr, "   Program Exec Options\n");	
        fprintf(stderr, "      [-P program]                    Program which to exec.\n");
	fprintf(stderr, "                                      Must take -M or -F for content\n");
        fprintf(stderr, "      [-F filename                    Text file which contains content\n");
        fprintf(stderr, "                                      for exec'd program\n");
	fprintf(stderr, "      [-M message_string]             String which contains content for\n");
	fprintf(stderr, "                                      exec'd program\n\n");
	fprintf(stderr, "      Note:                           -M if mutually exclusive with -F\n\n");
/*----------------------------------------------------------------------------*/
/* END   #if 0  (See COMMENT-1 above)                                         */
/*----------------------------------------------------------------------------*/
#endif


        fprintf(stderr, "   Pager Options\n");
        fprintf(stderr, "      [-p pagerid, pager, group]      Pager-id, Qpage pager, Qpage group\n");
	fprintf(stderr, "      [-S paging service]             Alternate paging service\n\n");
	fprintf(stderr, "      [-C content]                    Content for page.  Must be specified\n");
	fprintf(stderr, "      [-Q modem server]               Server to which modem is connected\n");
	fprintf(stderr, "                                      and thro' which page is delivered\n\n");

        fprintf(stderr, "   Miscellaneous Options\n");
        fprintf(stderr, "      [-A message_string]             Display message on console\n");
        fprintf(stderr, "      [-d]                            Display debug information\n");
	fprintf(stderr, "      [-n 1-7]                        Specify numeric priority for all\n");
	fprintf(stderr, "                                      notifications.\n\n");
}

/*---------------------------------------------------------------------------*/
/*   Function: void do_console( void )                                       */
/*                                                                           */
/*    Purpose: Prints out the message on either stderr or console            */
/*                                                                           */
/*---------------------------------------------------------------------------*/

void do_console(void)
{
    FILE  *fp = stdout;
    char  tmpbuffer[256];

    memset(tmpbuffer, 0, 256);
    if ( !isatty(fileno(fp)) ) {
	if ( (fp = fopen("/dev/console", "w")) == NULL ) {
	    fp = stdout;
	}
    }
    switch(priority[0]) {
	case '1':
	    strcpy(tmpbuffer, critical_text);
	    break;
	case '2':
	    strcpy(tmpbuffer, error_text);
	    break;
	case '3':
	    strcpy(tmpbuffer, warning_text);
	    break;
	case '4':
	default :
	    strcpy(tmpbuffer, info_text);
	    break;
    }

    fprintf(fp, "[ESP %s Notification]:%s\n", tmpbuffer, acontent);
    if ( fp != stdout) fclose(fp);
    return;
}


/*---------------------------------------------------------------------------*/
/*                                                                           */
/*   Function: int check_args( void )                                        */
/*                                                                           */
/*    Purpose: Checks for any problems with the argument given on the        */
/*             command line.                                                 */
/*                                                                           */
/*    Returns: An integer (OK or NOTOK) depending on if problems occured     */
/*                                                                           */
/*---------------------------------------------------------------------------*/

int check_args( void )
{
	int status = OK;

        if(nflag) {
                unsigned long p;

                p = LOG_PRI(strtoul(priority, NULL, 16));

		memset(priority, 0, 2);

		switch (p) {
		    case LOG_EMERG:
		    case LOG_ALERT:
		    case LOG_CRIT:
			priority[0] = '1';
			break;
		    case LOG_ERR:
			priority[0] = '2';
			break;
		    case LOG_WARNING:
			priority[0] = '3';
			break;
                    case LOG_NOTICE:
		    case LOG_INFO:
		    case LOG_DEBUG:
			priority[0] = '4';
			break;
                    default:
			priority[0] = '2';
			break;
		}

        }

	if(Cflag) {
		if(strlen(Content) == 0) {
			logstuff(0, "There must be text content specified in the -C option");
			status = NOTOK;
		}
	}

        if(((Cflag) && (!pflag)) || ((!Cflag) && (pflag))) {
		logstuff(0, "Paging requires both -C and -p to be specified");
                status = NOTOK;
        }
        
        if((Gui) &&(!cflag)) {
		logstuff(0, "GUI notification requires -c to be specified");
                status = NOTOK;
        }       

	if(Exec) {
		if((!Mflag) && (!Fflag)) {
			logstuff(0, "Program exec notification requires -M or -F to be specified");
       		        status = NOTOK; 
		}

                if((Mflag) && (Fflag)) {
			logstuff(0, "-M and -F cannot be specified together");
                        status = NOTOK;
                }
        }       
               
	if(Email) {
		if(!Eflag) {
			logstuff(0, "E-mail notification requires the -E argument to be specified");
			status = NOTOK;
		}
		
		if((mflag) && (fflag)) {
			logstuff(0, "-m and -f cannot be specified together");
			status = NOTOK;
		}
 
	        if(oflag && mflag) {
			logstuff(0, "-o and -m cannot be specified together");
			status = NOTOK;
	        }

		if((!mflag) && (!fflag)) {
			logstuff(0, "E-mail notification requires one of -m or -f to be specified");
       		        status = NOTOK;
		}

	        if(oflag) {
			if((strncmp(options, "COMP", 4)) == 0) {
				COMP = ON;
			}

			else if((strncmp(options, "ENCO", 4)) == 0) {
				ENCO = ON;
			}
			
			else {
				logstuff(0, "Incorrect processing option given.  Must be -o COMP or ENCO");
		                status = NOTOK;
			}
		}
        }

	if((Kflag) && (!fflag)) {
		logstuff(0, "-K requires that -f be specified");
	}
	return status;
}

int check_files(void)
{
        int status = OK;

        if(fflag) {
                if(!test_file(filename)) { status = NOTOK; }
        }

        if(Pflag) {
                if(!test_file(exec_this)) { status = NOTOK; }

                if(Fflag && status) {
                        if(!test_file(Filename)) { status = NOTOK; }
                }
        }

        return(status);
}	

int que_failure(int n_type)
{
    return(OK);
}

#if 0
int que_failure(int n_type)
{
        int que_fd;
	int status = OK;

	char *que_filename;
        char que_command[BUFFER];

	memset(que_command, 0, BUFFER);

        if((que_fd = open(tempnam(quedir, "ssQ"), O_WRONLY|O_CREAT|O_APPEND, 0644)) == -1) {
                logstuff(errno, "Could not open() que file");
        }

        else {
		write(que_fd, char_tries, strlen(char_tries));
		write(que_fd, "\n", 1);

                if(n_type == GUI_FAILURE) {
	
			write(que_fd, "GUI\n", 4);

			if(cflag) {
                                write(que_fd, content, strlen(content));
				write(que_fd, "\n", 1);
                        }

			else {
				write(que_fd, "N\n", 2);
			}

                        if(Dflag) {
				write(que_fd, displayhost, strlen(displayhost));
				write(que_fd, "\n", 1);
                        }

                        else {
                                write(que_fd, "N\n", 2);
                        }

                        if(gflag) {
                                write(que_fd, geometry, strlen(geometry));
				write(que_fd, "\n", 1);
                        }

                        else {
                                write(que_fd, "N\n", 2);
                        }

                        if(iflag) {
                                write(que_fd, iconimage, strlen(iconimage));
				write(que_fd, "\n", 1);
                        }

                        else {
                                write(que_fd, "N\n", 2);
                        }

                        if(tflag) {
                                write(que_fd, title, strlen(title));
				write(que_fd, "\n", 1);
                        }

                        else {
                                write(que_fd, "N\n", 2);
                        }

                }

                else if(n_type == PGR_FAILURE) {
	
			write(que_fd, "PGR\n", 4);

                        if(Cflag) {
                                write(que_fd, orig_content, strlen(orig_content));
                                write(que_fd, "\n", 1);
                        }

			else {
                                write(que_fd, "N\n", 2);
                        }

			if(Qflag) {
				write(que_fd, q_server, strlen(q_server));
                                write(que_fd, "\n", 1); 
                        }

                        else {
                                write(que_fd, "N\n", 2);
                        }


                        if(pflag) {
                                write(que_fd, pageridlist, strlen(pageridlist));
				write(que_fd, "\n", 1);
                        }

                        else {
                                write(que_fd, "N\n", 2);
                        }

                        if(Sflag) {
                                write(que_fd, Service, strlen(Service));
				write(que_fd, "\n", 1);
                        }

                        else {
                                write(que_fd, "N\n", 2);
                        }
                }

                if(dflag) {
                        write(que_fd, "Y\n", 2);
                }
		
		else {
			write(que_fd, "N\n", 2);
		}		

                if(nflag) {
                        write(que_fd, priority, strlen(priority));
                        write(que_fd, "\n", 1);
                }
	
		else {
			write(que_fd, "N\n", 2);
		}
        }

	close(que_fd);
        return status;
}
#endif
