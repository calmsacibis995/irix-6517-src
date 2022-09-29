#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>
#include <signal.h>
#include <errno.h>

static void	do_setup(void);
static void	do_basename_dirname(void);
static void	do_bgets(void);
static void	do_bufsplit(void);
static void	do_copylist(void);
static void	do_eaccess(void);
static void	do_gmatch(void);
static void	do_isencrypt(void);
static void	do_p2open_p2close(void);
static void	do_pathfind(void);
static void	do_strccpy(void);
static void	do_strfind_strrspn_strtrns(void);
static void	do_regex(void);
static void	do_rmdirp(void);

#define NUM_NAMES	8
#define NUM_TEXT	4
char name[NUM_NAMES][128];

#define BASEDIR "/usr/tmp/libgen.test/"

main(void)
{

	setbuf(stdout, NULL);

	do_setup();

	printf("--------------------- TESTING basename ---------------------\n\n");

	do_basename_dirname();

	printf("\n\n");
	printf("--------------------- TESTING bgets ---------------------\n\n");

	do_bgets();

	printf("\n\n");
	printf("--------------------- TESTING bufsplit ---------------------\n\n");

	do_bufsplit();

	printf("\n\n");
	printf("--------------------- TESTING copylist ---------------------\n\n");

	do_copylist();

	printf("\n\n");
	printf("--------------------- TESTING eaccess ---------------------\n\n");

	do_eaccess();

	printf("\n\n");
	printf("--------------------- TESTING gmatch ---------------------\n\n");

	do_gmatch();

	printf("\n\n");
	printf("--------------------- TESTING isencrypt ---------------------\n\n");

	do_isencrypt();

	printf("\n\n");
	printf("--------------------- TESTING p2open_p2close ---------------------\n\n");

	do_p2open_p2close();

	printf("\n\n");
	printf("--------------------- TESTING pathfind ---------------------\n\n");

	do_pathfind();

	printf("\n\n");
	printf("--------------------- TESTING strccpy ---------------------\n\n");

	do_strccpy();

	printf("\n\n");
	printf("--------------------- TESTING strfind_strrspn_strtrns ---------------------\n\n");

	do_strfind_strrspn_strtrns();

	printf("\n\n");
	printf("--------------------- TESTING regex ---------------------\n\n");

	do_regex();

	printf("\n\n");
	printf("--------------------- TESTING rmdirp ---------------------\n\n");

	do_rmdirp();
}

static void
do_setup(void)
{
	struct my_file {
		char *name;
		int mode;
	};

	struct my_file fake_file[NUM_TEXT] = {
		"people/igehy/cshrc",		0744,
		"people/igehy/scope",		0744,
		"people/redwood/bgets.c//",	0755,
		"people/guest/basename.c",	0744
	};

	char *real_file[NUM_NAMES-NUM_TEXT] = {
		"/etc/passwd",
		"/usr/bin/ls",
		"/",
		""
	};

	char *tmp = BASEDIR;
	char *str;
	char buf[1024];
	char cmd[1024];
	int i;
	FILE *fp[2];

	if (p2open("rm -r -f /usr/tmp/libgen.test", fp))
		fprintf(stderr, "Error--p2open failed\n");
	p2close(fp);
	
	for(i = 0; i < (NUM_TEXT); i++) {
		str = buf;
		(void) strcpy(name[i], tmp);
		(void) strcat(name[i], fake_file[i].name);
		str = dirname(strcpy(buf, name[i]));
		if (eaccess(str, F_OK) &&
		    (mkdirp(str, 0755) == -1))
			fprintf(stderr, "%s: %s.\n\t%s\n",
					"Error--cannot create directory",
					str,
					"Error in dirname() or mkdirp()");
		str = buf;
		str = strcat(basename(strcpy(buf, name[i])), "_test");
		sprintf(cmd, "/usr/bin/cp %s %s", str, name[i]);
		if (p2open(cmd, fp))
			fprintf(stderr, "Error--p2open failed\n");
		p2close(fp);
	}

	for(i = NUM_TEXT; i < NUM_NAMES; i++)
		(void) strcpy(name[i], real_file[i-(NUM_NAMES/2)]);
}

static void
do_basename_dirname(void)
{
	char dir_name[128];
	char base_name[128];
	int i;

	for (i = 0; i < NUM_NAMES; i++) {
		strcpy(dir_name, name[i]);
		strcpy(base_name, name[i]);
		printf("dirname/basename of %s =\n%s\t%s\n", name[i],
				dirname(dir_name), basename(base_name));
	}
}

static void
do_bgets(void)
{
	int i;
	char buffer[1024];
	FILE *fp;
	char *bgot;

	for (i = 0; i < NUM_TEXT; i++) {
		printf("----------------%s--------------------\n", name[i]);
		if (fp = fopen(name[i], "r")) {
			bgot = bgets(buffer, 1024, fp, ":;$a");
			if (bgot)
				printf("bgot-buffer: %ld\tstrlen: %ld\nbuffer: \n%s\n",
				bgot - buffer, strlen(buffer), buffer);
			else
				printf("No :;$a\n");
			fclose(fp);
		}
		else
			printf("Could not open file. Errno = %ld\n", errno);
		printf("------------------------------------------------\n\n");
	}
}

static void
do_bufsplit(void)
{
	int i, j, numsplit;
	FILE *fp;
	char buffer[256];
	char *field[10];

	for (i = 0; i < NUM_TEXT; i++) {
		printf("------------------------------------------------\n");
		if ((fp = fopen(name[i], "r")) &&
		   (fread(buffer, sizeof(char), 256, fp) == 256)) {
			buffer[255] = '\0';
			numsplit = (int) bufsplit(buffer, 10, field);
			printf("Number of fields: %ld\n", numsplit);
			for (j = 0; j < numsplit; j++)
				printf("%s\n", field[j]);
			fclose(fp);
		}
		printf("------------------------------------------------\n\n");
	}		
}

static void
do_copylist(void)
{
	int i, j;
	off_t size;
	char *buf;

	for (i = 0; i < (NUM_TEXT); i++) {
		buf = copylist(name[i], &size);
		printf("----------%s----------\n", name[i]);
		for (j = 0; j < size; j++)
			if(buf[j])
				putchar(buf[j]);
			else
				putchar('\n');
		printf("------------------------\n\n");
	}	
}

static void
do_eaccess(void)
{
	int i;

	for (i = 0; i < NUM_NAMES; i++)
		if (eaccess(name[i], X_OK))
			printf("%s has no euid execute permission\n", name[i]);
		else
			printf("%s has euid execute permission\n", name[i]);
}

static void
do_gmatch(void)
{
	int i;

	printf("------------------------\n");
	for (i = 0; i < NUM_NAMES; i++)
		if (gmatch(name[i], "*.[ch]"))
			printf("%s is a '*.[ch]'\n", name[i]);
		else
			printf("%s is not a '*.[ch]'\n", name[i]);
	printf("------------------------\n");
}

static void
do_isencrypt(void)
{
	int i;
	FILE *fp;
	char buf[256];

	printf("------------------------\n");
	for (i = 0; i < NUM_NAMES; i++)
		if ((fp = fopen(name[i], "r")) &&
		    (fread(buf, sizeof(char), 256, fp) == 256)) {
			if (isencrypt(buf, 256))
				printf("%s is encrypted\n", name[i]);
			else
				printf("%s is not a encrypted\n", name[i]);
			fclose(fp);
		}
	printf("------------------------\n");
}

static void
do_p2open_p2close(void)
{
          FILE *fp[2];
          pid_t pid;
          char buf[16];

          pid=p2open("/usr/bin/cat", fp);
          if ( pid != 0 ) {
               fprintf(stderr, "p2open failed (pid)\n");
               return;
          }
          write(fileno(fp[0]),"This is a test\n", 16);


          if(read(fileno(fp[1]), buf, 16) <=0)
               fprintf(stderr, "p2open failed (read)\n");
          else
               write(1, buf, 16);
          (void)p2close(fp);

}

static void
do_pathfind(void)
{
	int i;
	char base[128];

	for(i = 0; i < NUM_NAMES; i++) {
		strcpy(base, name[i]);
		printf("Pathfind: %s from %s\n",
				pathfind("/usr/bin", basename(base), "r"),
				basename(base));
	}
}

static void
do_strccpy(void)
{
	char foo[100] = "No\neternal\011reward\twill\007forgive\002us\001";
	char bar[100];
	char cho[100];
	char *end;

	printf("begin: %s\n\n", foo);

	end = streadd(bar, foo, NULL);

	printf("streadd: %s\tstrlen: %ld\tend-bar: %ld\n\n",
		bar, strlen(bar), end - bar);

	end = strcadd(cho, bar);

	printf("strcadd: %s\tstrlen: %ld\tend-cho: %ld\n\n",
		cho, strlen(cho), end - cho);

	if (strcmp(cho, foo) == 0)
		printf("begin = strcadd\n\n");
}

static void
do_strfind_strrspn_strtrns(void)
{
	int i, j, offset;
	char buffer[256];

	for(i = 0; i < NUM_NAMES; i++) {
		offset = strfind(name[i], "/b");
		if (offset == -1)
			printf("No \"/b\" in %s\n\n", name[i]);
		else {
			printf("Offset of \"/b\" is at %ld\n", offset);
			printf("%s\n", name[i]);
			for(j = 0; j < offset; j++)
				printf("-");
			printf("^\n");
		}
	}

	printf("\n---------Change .->, /->\\-------------\n\n");
	for(i = 0; i < NUM_NAMES; i++)
		printf("%s -->\n%s\n\n", name[i],
					 strtrns(name[i], "./", ",\\", buffer));
}

static void
do_regex(void)
{
	char *cmp, *begin, *end;
	char temp;
	int i;

	cmp = regcmp("/([^/])*", (char *) 0);
	for(i = 0; i < NUM_NAMES; i ++) {
		printf("%s\n", name[i]);
		begin = name[i];
		while (end = regex(cmp, begin)) {
			temp = *end;
			*end = '\0';
			printf("%s\n", begin);
			*end = temp;
			begin = end;
		}
		printf("\n");
	}

	cmp = regcmp("([A-Za-z][A-za-z0-9]{0,7})$0", (char *)0);
	end = regex(cmp, "012Testing345", begin);
	printf("end = %s\tret0 = %s\n\n", end, begin);
}



static void
do_rmdirp(void)
{
	int i;
	char buf[128];
	char buf2[128];
	char *ptr = buf;
	char *not_rem = buf2;

	chdir(BASEDIR);

	for(i = 0; i < NUM_TEXT; i++)
		remove(name[i]);

	for(i = 0; i < NUM_TEXT; i++) {
		*not_rem = '\0';
		strcpy(buf, name[i]);
		ptr = dirname(buf);
		ptr += strlen(BASEDIR);
		printf("On removal of %s, ", ptr);
		rmdirp(ptr, not_rem);
		if (*not_rem)
			printf("%s was not removed\n", not_rem);
		else
			printf("all was removed\n");
	}

	chdir("/");

	if (rmdir(BASEDIR))
		printf("Could not remove %s: errno = %ld\n", BASEDIR, errno);

	if (eaccess(BASEDIR, F_OK) == -1)
		printf("%s removed successfully\n", BASEDIR);
	else
		printf("%s was not removed\n", BASEDIR);
}

