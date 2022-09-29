#ident	"cmd/failover/foconfig.c: $Revision: 1.5 $"

#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <invent.h>
#include <string.h>
#include <dslib.h>

#include <sys/scsi.h>
#include <sys/dkio.h>
#include <sys/failover.h>

#define MAX_NAME_LENGTH 2048
#define CONFIGURATION_FILE_NAME	"/etc/failover.conf"

int ps_scan ( char *string, char *argv[], int pointers);
vertex_hdl_t fo_get_lun_vhdl (char *canonical);
int fo_do_inquiry (char *canonical, char *inqdata, size_t length);
void fo_invalidate_candidates (int *candidate_table);

char *program_name=NULL;
int vflag=0;

void
usage(void)
{
	fprintf (stderr,"%s [-d] [-f configuration_file] [[-v][-v]...]\n",program_name);
	exit(1);
}

int
same_device(char *name1, char *name2)
{
	char	*tmpstr1;
	char	*tmpstr2;
	int	 rc;

	if (strncmp(name1, "target", 6) == 0)
		return !strcmp(name1, name2);

	/*
	 * Compare "node/<NUMBER>/" part of name.
	 */
	tmpstr1 = strstr(name1, "port");
	tmpstr2 = strstr(name2, "port");
	if (tmpstr1 == NULL || tmpstr2 == NULL)
		return 0;
	*tmpstr1 = *tmpstr2 = '\0';

	rc = strcmp(name1, name2);
	*tmpstr1 = *tmpstr2 = 'p';
	if (rc)
		return 0;

	/*
	 * Move past "/port" in name
	 */
	tmpstr1 = strchr(tmpstr1, '/');
	tmpstr2 = strchr(tmpstr2, '/');
	if (tmpstr1 == NULL || tmpstr2 == NULL)
		return 0;

	/*
	 * Move to "/lun" in name
	 */
	tmpstr1++; tmpstr2++;
	tmpstr1 = strchr(tmpstr1, '/');
	tmpstr2 = strchr(tmpstr2, '/');
	if (tmpstr1 == NULL || tmpstr2 == NULL)
		return 0;

	/*
	 * Compare rest of name.
	 */
	return !strcmp(tmpstr1, tmpstr2);
}

char *
get_input_line (char *buf, int buf_size, FILE *stream)
{
	char	*rv;
	char	*b=buf;
	int	space = buf_size;
	int	length;

	/* fgets returns either NULL or the first argument */
	while ((rv = fgets(b,space,stream)) == b) {

		length = strlen(b);
		if (*(b+length-2) != '\\') {
			break;
		}
		*(b+length-1) = ' ';	/* get rid of newline */
		*(b+length-2) = ' ';	/* get rid of backslash */
		space -= length;
		b += length;
	}

	return (rv ? buf : rv);
}

main (int argc, char *argv[])
{

	char *input_file_name = CONFIGURATION_FILE_NAME;

	FILE *input_file=NULL;
	char *input_vector[128];		/* array of 128 pointers to hold token pointers */
	int  input_elements=sizeof(input_vector)/sizeof(char *);
	int  input_tokens;
	char input_line[MAX_NAME_LENGTH];			/* input line buffer */
	char input_copy[MAX_NAME_LENGTH];			/* input line copy */
	char fo_canonical_name[MAX_FO_PATHS][MAX_NAME_LENGTH];
	char fo_inquiry_data[MAX_FO_PATHS][SCSI_INQUIRY_LEN+1];	/* leave room for NULL */
	size_t  fo_inquiry_data_size = SCSI_INQUIRY_LEN+1;
	size_t  fo_inquiry_vendor_size = SCSI_DEVICE_NAME_SIZE;
	int  fo_inquiry_data_valid[MAX_FO_PATHS];
	int  fo_canonical_name_length = MAX_NAME_LENGTH;
	char *fo_device_name[MAX_FO_PATHS];
	char *fo_instance_name;
	char *fo_candidate_name[MAX_FO_PATHS];
	int  fo_is_candidate[MAX_FO_PATHS];
	vertex_hdl_t  fo_lun_vhdl[MAX_FO_PATHS];
	struct user_fo_generic_info fgi;
	int volume_fd;
	int status;
	int i,j,k;
	int c;
	int dry_run=0;
	int disable_disabled=0;
	int keep_partitions=1;
	extern char *optarg;
	extern int optind;


	/*
	** Process command line options.
	**
	**	Sets:	c - character of current argument
	**		input_file_name - as specified for "-f"
	**		vflag - verbose output flag
	**		dry_run - from "-d"
	**		program_name - from argv[0]
	*/

	program_name = argv[0];

	while ((c = getopt(argc,argv,"Ddf:kv")) != -1) {
		switch (c) {
		case 'D':
			disable_disabled=1;
			break;
		case 'd':
			dry_run = 1;
			break;
		case 'f':		/* input file override */
			input_file_name = optarg;
			break;
		case 'k':
			keep_partitions = 1;
			break;
		case 'v':		/* verbose output flag */
			vflag++;
			break;
		default:
			usage();
		}
	}


	/*
	** Open the failover configuration file.
	**
	**	Input:	vflag - verbose output flag
	**		program_name - as invoked
	**		input_file_name - from command line or default
	**
	**	Exit:	input_file - FILE handle
	*/

	if (vflag > 1) printf ("%s: opening %s.\n",program_name,input_file_name);

	input_file = fopen (input_file_name, "r");
	if (!input_file) {
		fprintf (stderr,"%s: Unable to open failover configuration file %s.\n",
			 program_name,input_file_name);
		exit(2);
	}

	if (vflag > 1) printf ("%s: Successfully opened %s.\n",program_name,input_file_name);


	/*
	** Read a line of the input file and process it.  Repeat.
	**
	**	Input:	input_file - FILE handle
	**
	**	Sets:	input_line - as read from file
	*/

	
	if (vflag>1) printf ("%s: processing %s.\n",program_name,input_file_name);

	while (get_input_line(input_line,sizeof(input_line),input_file) == input_line) {

		strcpy (input_copy, input_line);
		for (i=0; i<sizeof(input_copy); i++) {
			if (input_copy[i] == '\n') {
				input_copy[i] = '\0';
				break;
			}
		}

		if (vflag) printf ("Processing: %s\n",input_copy);

		/*
		** Initialize array elements.
		**
		**	Sets:	i - index into each array
		**		fgi
		**		fo_instance_name
		**		fo_lun_vhdl[]
		**		fo_device_name[]
		**		fo_is_candidate[]
		**		fo_canonical_name[][]
		**		fo_candidate_name[]
		**		fo_inquiry_data_valid[]
		*/

		bzero (&fgi,sizeof(fgi));

		fo_instance_name = NULL;

		for (i=0; i<MAX_FO_PATHS; i++) {
			fo_lun_vhdl[i] = 0;
			fo_device_name[i] = NULL;
			fo_is_candidate[i] = 0;
			fo_canonical_name[i][0] = NULL;
			fo_candidate_name[i] = NULL;
			fo_inquiry_data_valid[i] = 0;
		}

		/*
		** Tokenize each line, extract elements.
		**
		**	Input:	input_line - line from file
		**		input_elements - elements in input_vector[] array
		**
		**	Sets:	input_tokens - number of tokens in each line
		**		input_vector - initialized by ps_scan()
		**		fo_device_name - pointer to each potential device
		**		fo_instance_name - first non-comment token
		**		i - index into input_vector
		**		j - index into fo_device_name[]
		**
		**	Exit:	j - number of potential devices
		**		fo_device_name[] - strings from input line
		**		fo_instance_name - name of fo instance
		*/
	
		input_tokens = ps_scan (input_line, input_vector, input_elements);

		for (i=0, j=0; i<input_tokens && j<MAX_FO_PATHS; i++) {	/* skips tokenless lines */
			char *vector = input_vector[i];
			if (*vector == '#') {
				if (i == 0 && (strcmp(vector,"#dryrun") == 0)) {
					dry_run = 1;
				}
				else if (i == 0 && (strcmp(vector,"#verbose") == 0)) {
					vflag++;
				}
				else if (i == 0 && !disable_disabled && (strcmp(vector,"#disable") == 0)) {
					printf ("%s: disable directive encountered.  Exiting.\n",program_name);
					exit(1);
				}
				break;	/* comment line */
			}
			if (!fo_instance_name) {
				if (*vector == 's' && *(vector+1) == 'c') {
					printf ("%s: skipping line \'%s\': group name should not begin with \'sc\'.\n",
						program_name, input_copy);
					break;
				}
				fo_instance_name = input_vector[i];
			}
			else {
				fo_device_name[j++] = input_vector[i];
			}
		}

		if (!j) continue;

		/*
		** Validate each potential instance of failover devices.
		**	Get the canonical name of device and the lun vertex handle.
		**
		**	Input:	j - number of device names
		**		fo_canonical_name_length - max length of canonical path
		**		fo_device_name - pointer to each potential device
		**
		**	Sets:	i - index into fo_lun_vhdl[], fo_canonical_name[][0], fo_device_name[]
		**		fo_lun_vhdl[] - lun vertex handle associated with fo_device_name
		**		fo_canonical_name[] - set by fo_get_lun_vhdl() - absolute path
		**		fo_is_candidate[] - as appropriate
		**
		**	Exit:	j - number of potential devices
		**		fo_is_candidate[] - potential candidates for failover
		*/

		for (i=0; i<j; i++) {

			int length;
			char *status;
			char path[MAX_NAME_LENGTH+10];

			strcpy (path, "/hw/scsi/");
			strcat (path, fo_device_name[i]);
			length = fo_canonical_name_length;	/* gets trashed by call! */

			status = filename_to_devname(path,
						     &fo_canonical_name[i][0], 
						     &length);
			
			if (!status) {
				fo_is_candidate[i] = 0;
				if (vflag==1) printf ("%s: %s does not exist in system.\n",
						program_name, fo_device_name[i]);
				else if (vflag>1) printf ("%s: %s: Unable to convert filename %s to canonical name.\n",
						program_name, strerror(errno), fo_device_name[i]);
				continue;
			}


			fo_lun_vhdl[i] = fo_get_lun_vhdl (&fo_canonical_name[i][0]);

			if (fo_lun_vhdl[i] == -1) {
				fo_is_candidate[i] = 0;
				if (vflag==1) printf ("%s: %s does not exist in system.\n",
						program_name, fo_device_name[i]);
				else if (vflag>1) printf ("%s: Unable to get lun vertex handle for %s.\n",
						program_name, &fo_canonical_name[i][0]);
			}
			else {
				fo_is_candidate[i] = 1;
				if (vflag>1) {
					printf ("%s: %s -> %s (lvh %d).\n",
						program_name,
						fo_device_name[i],
						&fo_canonical_name[i][0],
						fo_lun_vhdl[i]
						);
				}
			}
		}

		/*
		** Examine paths to acertain that they all point to the same target and lun.
		** Add those that match to the fo_candidate_name[] array.  Get inquiry data
		** for potential candidates, and if valid, compare.  Inquiry data not being
		** valid is sufficient to cause device to not be a candidate.
		** Valid inquiry that doesn't compare will cause no devices to be considered for failover.
		**
		**	Input:	fo_canonical_name[][] - canonical name
		**		fo_device_name[] - pointer to each potential device
		**		fo_is_candidate[] - device is valid so far
		**		j - number of potential devices
		**
		**	Sets:	i - index into fo_canonical_name[], fo_device_name[], fo_inquiry_data[][], 
		**			       fo_inquiry_data_valid[], fo_is_candidate[]
		**		fo_candidate_name[] - pointer to devices with matching target/lun
		**		fo_inquiry_data[][] - inquiry data returned from dksc ioctl
		**		fo_inquiry_data_valid[] - flag indicated inquiry data is valid
		**		fo_is_candidate[] - flag indicated device is candidate for failover
		*/

		{

		char *scsi_ctlr;
		char *target, *first_target=NULL;
		int first_target_index;
		int status;

		for (i=0; i<j; i++) {

			if (!fo_is_candidate[i]) continue;

			scsi_ctlr = strstr (&fo_canonical_name[i][0],"scsi_ctlr");

			if (scsi_ctlr == NULL) {	/* not found */

				fo_is_candidate[i] = 0;
				if (vflag) printf ("%s: %s -> %s has no scsi controller.\n",
					program_name, fo_device_name[i],&fo_canonical_name[i][0]);

			}
			else {
				target = strstr (scsi_ctlr,"target");
				if (target == NULL)
					target = strstr(scsi_ctlr, "node");
				if (target) {
					bzero (&fo_inquiry_data[i][0],fo_inquiry_data_size);
					if (!first_target) {
						/*
						** perform inquiry for first_target here
						*/
						fo_inquiry_data_valid[i] = 
							fo_do_inquiry (&fo_canonical_name[i][0],
								       &fo_inquiry_data[i][0],
								       fo_inquiry_data_size);
						if (!fo_inquiry_data_valid[i]) {
							if (vflag) {
								printf ("%s: %s: Could not get inquiry data.  Skipping instance.\n",
									program_name,fo_instance_name);
							}
							fo_invalidate_candidates(fo_is_candidate);
							break;
						}
						first_target = target;
						first_target_index = i;
						fo_candidate_name[i] = &fo_canonical_name[i][0];
					}
					else if (same_device(target, first_target)) {	/* match */
						/*
						** perform inquiry here
						*/
						fo_inquiry_data_valid[i] = 
							fo_do_inquiry (&fo_canonical_name[i][0],
								       &fo_inquiry_data[i][0],
								       fo_inquiry_data_size);
						if (!fo_inquiry_data_valid[i] ||
						    bcmp(&fo_inquiry_data[i][8],
							 &fo_inquiry_data[first_target_index][8],
							 fo_inquiry_vendor_size) != 0) {
							fo_invalidate_candidates(fo_is_candidate);
							if (vflag>1) {
								printf ("%s: different inquiry data for %s and %s, skipping.\n",
									program_name,
									fo_device_name[first_target_index],
									fo_device_name[i]);
								printf ("%s: %s: %s.\n",
									program_name,
									fo_device_name[first_target_index],
									&fo_inquiry_data[first_target_index][0]);
								printf ("%s: %s: %s.\n",
									program_name,
									fo_device_name[i],
									&fo_inquiry_data[i][0]);
							}
							break;
						}
						fo_candidate_name[i] = &fo_canonical_name[i][0];
					}
					else {
						if (vflag) printf ("%s: %s and %s refer to different devices.\n",
								program_name,
								fo_device_name[first_target_index],
								fo_device_name[i]);
						fo_invalidate_candidates(fo_is_candidate);
						break;
					}
				}
				else {
					if (vflag) printf ("%s: Unable to locate target in string %s.\n",
							program_name,scsi_ctlr);
					fo_invalidate_candidates(fo_is_candidate);
					break;
				}
			}
		}

		}

		for (i=0; i<j; i++) {
			if (vflag && fo_is_candidate[i])
				printf ("%s: candidate: %s -> %s\n",
					program_name,
					fo_device_name[i],
					fo_candidate_name[i]);
		}

		/*
		** Build the scsi_fo_generic_info table and call failover.
		**
		**	Input:	fo_canonical_name[][] - canonical name
		**		fo_is_candidate[] - device is valid so far
		**		fo_lun_vhdl[]
		**		fo_inquiry_data[][] - inquiry data returned from dksc ioctl
		**		fo_instance_name
		**		j - number of potential devices
		**
		**	Sets:	i - index into fo_canonical_name[], fo_inquiry_data[][], 
		**			       fo_is_candidate[], fo_lun_vhdl[]
		**		k - index into fgi_lun_vhdl[], fgi_inq_data[]
		**		fgi - fo_generic_info table initialized
		**		volume_fd - file descriptor of an associated volume/char
		*/

		for (volume_fd=-1, k=0, i=0; i<j; i++) {
			if (fo_is_candidate[i]) {
				fgi.fgi_lun_vhdl[k] = fo_lun_vhdl[i];
				bcopy (&fo_inquiry_data[i][0],&fgi.fgi_inq_data[k][0],SCSI_INQUIRY_LEN);
				k++;
				if (volume_fd < 0) volume_fd = fo_open_volume (&fo_canonical_name[i][0]);
			}
		}
		strncpy (fgi.fgi_instance_name,fo_instance_name,sizeof(fgi.fgi_instance_name)-1);
		fgi.fgi_keep_partitions = keep_partitions;

		if (!k) {
			printf ("%s: %s: No candidates.\n",program_name, fo_instance_name);
		}
		else if (volume_fd < 0) {	/* Couldn't open a dksc node for any member */
			printf ("%s: %s: Cannot establish failover instance, cannot open any volume device.\n",
					program_name,fo_instance_name);
		}
		else if (dry_run) {
			printf ("%s: dry run: fgi_instance_name %s.\n",program_name,fgi.fgi_instance_name);
			printf ("%s: dry run: fgi_lun_vhdl(s) ",program_name);
			for (i=0; i<j; i++) printf ("%d ",fgi.fgi_lun_vhdl[i]);
			printf ("\n");
			for (i=0; i<j; i++) {
				printf ("%s: dry run: inq string: %s.\n",program_name,&fgi.fgi_inq_data[i][8]);
			}
			close (volume_fd);
		}
		else {
			status = ioctl (volume_fd,DIOCFOUPDATE,&fgi);
			if (status < 0) {
				printf ("%s: Failover instance not established for instance %s.\n",
					program_name,fo_instance_name);
			}
			close (volume_fd);
		}
	}
}

void
fo_invalidate_candidates (int *candidate_table)
{

	int i;

	for (i=0; i<MAX_FO_PATHS; i++) *candidate_table++ = 0;
}

int
fo_open_volume (char *canonical)
{

	int volume_fd=-1;
	int found_lun=0;
	char *lun, *lun_digit, *lun_slash;
	char volume_name[MAX_NAME_LENGTH];

	strcpy (volume_name, canonical);
	
	lun = strstr (volume_name,"lun");
	if (lun) {
		lun_digit = strpbrk (lun,"0123456789");
		if (lun_digit) {
			found_lun = 1;
			lun_slash = strpbrk (lun_digit,"/");
			if (lun_slash) {
				*lun_slash = NULL;
			}
		}
	}


	if (found_lun) {

		strcat (volume_name, "/disk/volume/char");

		volume_fd = open (volume_name, O_RDONLY);
	}

	return volume_fd;
}

int
fo_send_inquiry_command (struct dsreq *dsp, uchar_t *data, long datalen, int vu, int neg)
{
	int fd;
	int status;
	fillg0cmd(dsp, (uchar_t *)CMDBUF(dsp), G0_INQU, 0, 0, 0, B1(datalen), B1(vu<<6));
	filldsreq(dsp, data, datalen, DSRQ_READ|DSRQ_SENSE|neg);
	dsp->ds_time = 1000 * 30; /* 90 seconds */
	return doscsireq (getfd(dsp), dsp);
}

int
fo_do_inquiry (char *canonical, char *inqdata, size_t length)
{

	int status = 0;
	int volume_fd;

	struct dsreq *dsp;

	uchar_t *inqbuf = (uchar_t *)malloc(length);

	bzero (inqbuf,length);

	dsp = dsopen (canonical, O_RDONLY);
	if (!dsp) {
		printf ("dsopen of %s failure.\n",canonical);
	}

	else {
		if ( fo_send_inquiry_command (dsp, inqbuf, length, 0, 0) ) {
			printf ("inquiry failure for %s.\n", canonical);
		}
		else {
			status = 1;	/* status ok */
			bcopy (inqbuf, inqdata, length);
		}
	}

	dsclose (dsp);

	return status;
}

vertex_hdl_t
fo_get_lun_vhdl (char *canonical)
{
	char lun_name[MAX_NAME_LENGTH];
	int i, j, found_lun=0;
	char *lun, *lun_digit, *lun_slash;
	vertex_hdl_t lvh = -1;
	struct stat stat_buffer;

	strcpy (lun_name, canonical);

	lun = strstr (lun_name,"lun");
	if (lun) {
		lun_digit = strpbrk (lun,"0123456789");
		if (lun_digit) {
			found_lun = 1;
			lun_slash = strpbrk (lun_digit,"/");
			if (lun_slash) {
				*lun_slash = NULL;
			}
		}
	}


	if (!found_lun) {
		if (vflag) printf ("%s: Unable to locate lun for specified device: %s.\n",
					program_name,canonical);
	}

	else {
		if (stat(lun_name,&stat_buffer) >= 0) {
			lvh = (vertex_hdl_t) stat_buffer.st_ino;
		}
	}

	return lvh;
}

/********************************************************************************
 *										*
 * ps_scan - Tokenize a null-terminated string.					*
 *										*
 *	string - null terminated string to tokenize				*
 *	argv - array of pointers to receive token addresses			*
 *	pointers - number of pointers in argv array				*
 *										*
 * RETURNS: number of tokens encountered.					*
 *										*
 * Note: input string is modified by inserting NULL string terminators at the	*
 *	 end of each token.							*
 *										*
 * Borrowed from Cray Research, Inc., Giga-Ring software!			*
 *										*
 ********************************************************************************/

int
ps_scan ( char *string, char *argv[], int pointers)
{
	int argc = 0;
	enum {SPACE, WORD} state = SPACE;
	int i;

	for (i=0; *string && argc<pointers; string++)
		switch (state) {

		case SPACE:
			if (!isspace (*string)) {
				argv[argc++] = string;
				state = WORD;
			}
			break;

		case WORD:
			if (isspace (*string)) {
				*string = '\0';
				state = SPACE;
			}
			break;

		}

	return (argc);
}
