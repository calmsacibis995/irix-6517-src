#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <pwd.h>
#include <grp.h>
#include <proj.h>

#ifndef major
#define major(x) (int)((unsigned)(x)>>8)
#endif
#ifndef minor
#define minor(x) (int)((x)&0377)
#endif

#define IPLMAX 3				/* itemsperline MAX */
#define preipl()  if (!qflag) if (itemsperline==0) printf("\n\t"); else printf("; "); else if (needqseparator) printf ("\n")
#define postipl() if (!qflag) itemsperline = (itemsperline++ >= IPLMAX) ? 0 : itemsperline; else needqseparator++
#define put64(str,v) {	preipl();			\
			qflag ? printf ("%llu", v) : printf("%s %llu", #str, v); \
			postipl();			\
		   }
#define put(str,v) {	preipl();			\
			qflag ? printf ("%u", v) : printf("%s %u", #str, v); \
			postipl();			\
		   }
int itemsperline;
int needqseparator = 0;	    /* if not 0 and if qflag, emit newline first */

int fflag = 0;
int fd;
int iflag = 0;
int dflag = 0;
int rflag = 0;
int lflag = 0;
int sflag = 0;
int pflag = 0;
int jflag = 0;
int tflag = 0;
int kflag = 0;
int uflag = 0;
int gflag = 0;
int cflag = 0;
int aflag = 0;
int mflag = 0;
int hflag = 0;
int qflag = 0;
int Lflag = 0;
int all = 1;
int justonetime = 0;

int nfailed = 0;

/*
 * Like ctime - only null-out final '\n' character.
 */

 char *
nl_ctime (tp)
 time_t *tp;
{
	char *ctime();			/* The real ctime() */
	char *t = ctime (tp);		/* result of ctime */
	char *u = t + strlen(t);	/* find end of string */
	*--u = 0;			/* backup and kill last char */
	return (t);
}

main(argc,argv)
 int argc;
 char *argv[];
{
	struct stat64 buf;
	char *nl_ctime();
	char *malloc();
	int c;
	extern int optind;
	extern char *optarg;
	char t[4];
	register char *file;


	if (argc == 1) help();
	while ((c = getopt (argc, argv, "f:?idrlspjkugcamtqL")) != EOF) {
		switch (c) {
			case 'f':
				fflag++;
				file = malloc(64);
				strcpy(file, "file descriptor ");
				strcat(file, optarg);
				fd = atoi(optarg);
				break;
			case 'i': iflag++; all = 0; break;
			case 'd': dflag++; all = 0; break;
			case 'r': rflag++; all = 0; break;
			case 'l': lflag++; all = 0; break;
			case 's': sflag++; all = 0; break;
			case 'p': pflag++; all = 0; break;
			case 'j': jflag++; all = 0; break;
			case 'k': kflag++; all = 0; break;
			case 'u': uflag++; all = 0; break;
			case 'g': gflag++; all = 0; break;
			case 'c': cflag++; all = 0; break;
			case 'a': aflag++; all = 0; break;
			case 'm': mflag++; all = 0; break;
			case 't': tflag=cflag=mflag=aflag=1; all = 0; break;
			case 'q': qflag++; break;
			case 'L': Lflag++; break;
			default:
			case '?':
			case 'h': hflag = 1; help(); break;
		}
	}

	if (cflag + aflag + mflag == 1) justonetime++;

	if (fflag) {
		if (fstat64 (fd, &buf) == 0) {
			if (fd == 1)
				*stdout = *stderr;
			goto showstat;
		} else {
			goto failstat;
		}
	}

	for ( ; !fflag && optind < argc; optind++) {
		file = argv[optind];
		if ( (Lflag ? lstat64 (file, &buf) : stat64 (file, &buf) ) == 0) {
showstat:
			if (!qflag) {
				if (justonetime) {
					int len = 20 - strlen(file);
					if (len < 2) len = 2;
					printf("%s",file);
					printf("%-*s",len,":");
				} else
					printf ("%s:", file);
			}

			/*
			 * For the following fields, leave the cursor
			 * at the end of each field as the default,
			 * with itemsperline the number of fields
			 * printed so far on the line.
			 * If itemsperline == 0, then cursor is at end
			 * of line, and next char out should be newline.
			 */

			itemsperline = 0;
			if (justonetime || qflag) itemsperline = 1;
			if (iflag || all)
				put64(inode,buf.st_ino);
			if (dflag || all)
				put(dev, buf.st_dev);
			if (rflag || all) {
				if (qflag) {
					printf("%u",buf.st_rdev);
					needqseparator++;
				} else {
					/* st_rdev is defined only for char special,
					 * block special, and lofs files */
					if ((S_IFMT & buf.st_mode) == S_IFCHR ||
					    (S_IFMT & buf.st_mode) == S_IFBLK ||
					    strcmp(buf.st_fstype, "lofs") == 0) {
						int maj, min;
						maj = major(buf.st_rdev);
						min = minor(buf.st_rdev);
						preipl();
						printf("rdev %d,%d", maj, min);
						postipl();
					}
				}
			}
			if (lflag || all)
				put(links,buf.st_nlink);
			if (sflag || all)
				put64(size, buf.st_size);
			if (kflag || all) {
				preipl();
				switch (S_IFMT & buf.st_mode) {
				case S_IFDIR: fputs("directory", stdout); break;
				case S_IFCHR: fputs("char_spec", stdout); break;
				case S_IFBLK: fputs("block_spec", stdout); break;
				case S_IFREG: fputs("regular", stdout); break;
				case S_IFIFO: fputs("fifo", stdout); break;
				case S_IFLNK: fputs("symlink", stdout); break;
				case S_IFSOCK: fputs("socket", stdout); break;
				default: fputs("unknown type", stdout); break;
				}
				postipl();
			}
			if (pflag || all) {
				register int m;

				preipl();
				if (qflag) {
					printf("%#lo",buf.st_mode);
				} else {
					fputs("mode is ", stdout);
					strcpy(t,"---");
					m = buf.st_mode & (S_ISUID|S_ISGID|S_ISVTX);
					if (m & S_ISUID) t[0] = 'u';
					if (m & S_ISGID) t[1] = 'g';
					if (m & S_ISVTX) t[2] = 's';
					if (m) fputs(t, stdout);

					strcpy(t,"---");
					m = buf.st_mode & 0700;
					if (m & S_IREAD) t[0] = 'r';
					if (m & S_IWRITE) t[1] = 'w';
					if (m & S_IEXEC) t[2] = 'x';
					fputs(t, stdout);

					strcpy(t,"---");
					m = (buf.st_mode & 070) << 3;
					if (m & S_IREAD) t[0] = 'r';
					if (m & S_IWRITE) t[1] = 'w';
					if (m & S_IEXEC) t[2] = 'x';
					fputs(t, stdout);

					strcpy(t,"---");
					m = (buf.st_mode & 07) << 6;
					if (m & S_IREAD) t[0] = 'r';
					if (m & S_IWRITE) t[1] = 'w';
					if (m & S_IEXEC) t[2] = 'x';
					fputs(t, stdout);
				}
				postipl();
			}
			if (uflag || all) {
				if ((gflag || all) && itemsperline == IPLMAX)
					itemsperline = 0;
				put(uid, buf.st_uid);
				if (!qflag) putuidname (buf.st_uid);
			}
			if (gflag || all) {
				put(gid, buf.st_gid);
				if (!qflag) putgidname (buf.st_gid);
			}

			if (jflag || all) {
				if (all)
					itemsperline = 0;
				put(projid, buf.st_projid);
				if (!qflag) putprojname (buf.st_projid);
			}
						
			/*
			 * Switch back to leaving cursor at start of line.
			 * Play around a little bit to make sure that if both
			 * the project id and the fstype are going to be
			 * printed, then they appear on the same line..
			 */
			itemsperline = 0;
			if (justonetime) 
				itemsperline = 1;
			else if (!qflag && !all) 
				putchar ('\n'); 

			if (all) {
				if (!qflag)
					printf ("\tst_fstype: %s\n",
						buf.st_fstype);
				else {
					preipl();
					printf ("%s", buf.st_fstype);
					postipl();
				}
			}
			
			if (!qflag) {
				if (cflag || all)
					printf("\tchange time - %s <%ld>\n", nl_ctime(&buf.st_ctime), buf.st_ctime);
				if (aflag || all)
					printf("\taccess time - %s <%ld>\n", nl_ctime(&buf.st_atime), buf.st_atime);
				if (mflag || all)
					printf("\tmodify time - %s <%ld>\n", nl_ctime(&buf.st_mtime), buf.st_mtime);
			} else {
				if (cflag || all)
					put ("", buf.st_ctime);
				if (aflag || all)
					put ("", buf.st_atime);
				if (mflag || all)
					put ("", buf.st_mtime);
			}


		} else {
failstat:
			if (!qflag) {
				extern int errno;
				fprintf (stderr, "can't stat: `%s' (errno %d)\n", file, errno);
				perror(file);
			}
			nfailed++;
		}
	}
	if (qflag && needqseparator)
		putchar ('\n');
	exit (nfailed > 255 ? 255 : nfailed);
}

putuidname (unsigned uid)
{
	register struct passwd *pp;
	if ((pp = getpwuid((int)uid)) != 0)
		printf (" (%s)", pp->pw_name);
}

putgidname (unsigned gid)
{
	register struct group *gp;
	if ((gp = getgrgid((int)gid)) != 0)
		printf (" (%s)", gp->gr_name);
}

putprojname (long projid)
{
	char prjname[MAXPROJNAMELEN];
	if (projname((prid_t) projid, prjname, MAXPROJNAMELEN) != 0)
		printf(" (%s)", prjname);
}

help() {
	fputs("Usage: stat [-idrlspjkugcamtqL] [-f fdesc] files ...\n",stderr);
	fputs("\tinode, dev, rdev, links, size, perm, kind, user, grp\n",stderr);
	fputs("\tctime, atime, mtime, t = all times, q = quiet, L = lstat, j = project\n",stderr);
	fputs("\tdefault is everything\n",stderr);
	fputs("\t-f option takes file descriptor instead of filenames\n",stderr);
	exit (1);
}
