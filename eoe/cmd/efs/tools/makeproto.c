#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <ftw.h>
#include <getopt.h>

#define INDENTS 2	/* indent 2 spaces per directory level */

int empty_dir = 0, level = 0;

int nxtentry(char *name, struct stat *st, int type);
void putentry(char *name, struct stat *st);
char spaces[INDENTS*PATH_MAX+1];
int dolinks = 0;
unsigned long sectors;	/* size of all files examined;
	useful for constructing CD images, etc.  May overreport
	because it doesn't handle links.  The reported size
	should be bumped when creating the filesystem, because
	there is no allowance for inodes, etc, and the directory
	size heuristic is pretty simple minded. */

main(int cnt, char **args)
{
	char *dir;
	int i;
	int opt;

	memset(spaces, ' ', sizeof(spaces)-1);

	while ((opt = getopt(cnt, args, "l")) != EOF) {
		switch(opt) {
		case 'l':
			dolinks++;
			break;
		default:
			fprintf(stderr, "Usage: %s [-l] [topdir]\n", *args);
			return 1;
		}
	}

	if (cnt - optind == 0)
		dir = ".";
	else if(cnt - optind == 1)
		dir = args[optind];
	else {
		fprintf(stderr, "Usage: %s [-l] [topdir]\n", *args);
		return 1;
	}

	if(chdir(dir)) {
		fprintf(stderr, "couldn't chdir to ");
		perror(dir);
		return 2;
	}
	printf("nobootfile\n1 1\n");	/* boot file and size/inodes ignored */

	if(ftw(".", nxtentry, 20)) {
		fprintf(stderr, "Error occurred during tree walk\n");
		return 2;
	}

	/*
	 * Need to get a '$' in there for each level we
	 * pop back up at the end
	 */
	if (empty_dir)
		printf("%.*s$\n", level*INDENTS, spaces);
	for (i = level - 1; i >= 1; i--)
		printf("%.*s$\n", i*INDENTS, spaces);

	printf("$\n$\n");	/* the $ that terminates the proto file */

	fprintf(stderr, "%lu %d byte blocks\n", sectors, NBPSCTR);
	return 0;
}


/* determine level by counting '/'s in name.  Used because we could
 * go up more than one level between nxtentry calls, depending on
 * directory layout.  returns +1, because we always have the first
 * (root) directory.
*/
nslash(char *name)
{
	int slashes = 0;
	while(*name)
		if(*name++ == '/')
			slashes++;
	return slashes+1;
}

/* note that we can't have a full pathname, or one with leading .'s, so
 * we strip until / or a null.
*/
nxtentry(char *name, struct stat *st, int type)
{
	static int curlen;
	static char curdir[PATH_MAX+1];
	int nlevel;
	int i;
	struct stat stbuf;

	/*
	 * Make it work for symbolic links
	 */
	if (dolinks && (lstat(name, &stbuf) == 0)) {
		st = &stbuf;
	}

	while(*name == '/' || *name == '.') {
		name++;
		if(name[-1] == '/')
			break;	/* so files like ./.abc do not get the 2nd .
				* stripped */
	}
	if(!*name) {
		putentry(name,st);	/* first entry will have no name */
		level++;
		return 0;
	}

	if(!strrchr(name, '/'))	/* if null, we are at the top level */
		nlevel = 1;
	else
		nlevel = nslash(name);

	/*
	 * A hack to catch the case of a directory being empty.  We set
	 * empty_dir everytime we see a directory.  If we get here
	 * and empty_dir is set and we're at the same or higher
	 * level, we know that the last entry was an empty directory,
	 * so we print the '$'.
	 *
	 *      rogerc     09/11/91
	 */
	if (empty_dir && nlevel <= level)
		printf("%.*s$\n", level*INDENTS, spaces );
	empty_dir = 0;

	switch(type) {
	case FTW_F:
		if(nlevel < level) {
			/* went up a level */
			for (i = level - 1; i >= nlevel; i--)
				printf("%.*s$\n", i*INDENTS, spaces);
		}
		sectors += (NBPSCTR-1 + st->st_size)/NBPSCTR;
		break;
	case FTW_D:
		empty_dir = 1;
		if(nlevel < level) {
			/* went up a level */
			for (i = level - 1; i >= nlevel; i--)
				printf("%.*s$\n", i*INDENTS, spaces);
			strncpy(curdir, name, curlen);
		}
		/* else same level or deeper */
		strcpy(curdir, name);
		curlen = strlen(curdir);
		sectors += 2;	/* a reasonable average... */
		break;
	case FTW_DNR:
		fprintf(stderr, "Can't read directory %s; skipped\n", name);
		return 0;
	case FTW_NS:
		fprintf(stderr, "Can't stat %s; skipped\n", name);
		return 0;
	}
	level = nlevel;
	printf("%.*s", level*INDENTS, spaces);
	putentry(name,st);	/* first entry will have no name */
	return 0;
}

void
putentry(char *name, struct stat *st)
{
	char *path;
	unsigned mode = st->st_mode;
	char linkedto[1000];

	path = strrchr(name, '/');
	if(path)
		path++;
	else
		path = name;
	printf("%-16s  ", path);	/* most names are short, gives
		* a 'nicer' output for human beings */

	if(S_ISREG(mode))
		putchar('-');
	else if(S_ISDIR(mode))
		putchar('d');
	else if(S_ISCHR(mode))
		putchar('c');
	else if(S_ISBLK(mode))
		putchar('b');
	else if (S_ISLNK(mode))
		putchar('l');
	else {
		fprintf (stderr, "Bad file type 0%o for %s\n", mode, name);
		return;
	}
	putchar(mode & S_ISUID ? 'u' : '-');
	putchar(mode & S_ISGID ? 'g' : '-');
	printf ("%03o %u %u", mode & 0777, st->st_uid, st->st_gid);

	if(S_ISCHR(mode) || S_ISBLK(mode))
		printf(" %d %d", major(st->st_rdev), minor(st->st_rdev));
	else if(S_ISREG(mode))
		printf (" %s", name);
	else if (S_ISLNK(mode)) {
		int n;
		if ((n = readlink(name, linkedto, sizeof(linkedto))) > 0) {
			linkedto[n] = '\0';
			printf(" %s", linkedto);
		}
	}
	putchar('\n');
}
