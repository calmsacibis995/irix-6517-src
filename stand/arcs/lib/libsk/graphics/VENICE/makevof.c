#include "stdio.h"
#include "mc.out.h"
#include "sys/types.h"
#include "sys/sema.h"
#include "sys/gfx.h"
#include "sys/rrm.h"
#include <sys/venice.h>
#include <voftypedefs.h>
#include <vofdefs.h>
#include <dg2_eeprom.h>

main(int argc, char *argv[])
{
    char c, *vofpath;
    mcout_t vofhdr;
    unsigned int voftop[DG2_PARAMETER_CNT];
    vof_data_t vofbody;
    int i, j, k, off, len;
    FILE *fp;
    int xpmax, ypmax;

    struct venice_dg2_eeprom eeprom;

    if (argc != 2) {
	fprintf(stderr, "Usage: makevof vof\n");
	exit(1);
    }
    vofpath = argv[1];

    if (!(fp = fopen(vofpath, "r"))) {
	perror(vofpath);
	exit(1);
    }

    if (fread(&vofhdr, 1, sizeof(vofhdr), fp) != sizeof(vofhdr)) {
	fprintf(stderr, "makevof: unexpected EOF in %s\n", vofpath);
	exit(1);
    }

    if (vofhdr.f_magic != DG2_VOF_MAGIC) {
	fprintf(stderr, "VOF is not for DG2 (0x%x)\n", vofhdr.f_magic);
	exit(1);
    }

    /* get to the code section */
    for (i = (vofhdr.f_codeoff - sizeof(vofhdr)); i--; ) {
	if (fread(&c, 1, 1, fp) != 1) {
	    fprintf(stderr, "makevof: unexpected EOF in %s\n", vofpath);
	    exit(1);
	}
    }

    /* read the parameter block */
    if (fread(&off, 1, sizeof(off), fp) != sizeof(off)) {
	fprintf(stderr, "makevof: unexpected EOF in %s\n", vofpath);
	exit(1);
    }

    if (fread(&len, 1, sizeof(len), fp) != sizeof(len)) {
	fprintf(stderr, "makevof: unexpected EOF in %s\n", vofpath);
	exit(1);
    }

    if (len != (DG2_PARAMETER_CNT * sizeof(int))) {
	fprintf(stderr, "makevof: internal error in %s\n", vofpath);
	exit(1);
    }

    for (i = off; i--; ) {
	if (fread(&c, 1, 1, fp) != 1) {
	    fprintf(stderr, "makevof: unexpected EOF in %s\n", vofpath);
	    exit(1);
	}
    }

    if (fread(voftop, 1, sizeof(voftop), fp) != sizeof(voftop)) {
	fprintf(stderr, "makevof: unexpected EOF in %s\n", vofpath);
	exit(1);
    }

    /* read the guts */
    if (fread(&off, 1, sizeof(off), fp) != sizeof(off)) {
	fprintf(stderr, "makevof: unexpected EOF in %s\n", vofpath);
	exit(1);
    }

    if (fread(&len, 1, sizeof(len), fp) != sizeof(len)) {
	fprintf(stderr, "makevof: unexpected EOF in %s\n", vofpath);
	exit(1);
    }

    if (len != sizeof(vofbody)) {
	fprintf(stderr, "makevof: internal error in %s\n", vofpath);
	exit(1);
    }

    for (i = off; i--; ) {
	if (fread(&c, 1, 1, fp) != 1) {
	    fprintf(stderr, "makevof: unexpected EOF in %s\n", vofpath);
	    exit(1);
	}
    }

    if (fread(&vofbody, 1, sizeof(vofbody), fp) != sizeof(vofbody)) {
	fprintf(stderr, "makevof: unexpected EOF in %s\n", vofpath);
	exit(1);
    }

    xpmax = vofbody.vof_file_info.vof_width;
    ypmax = vofbody.vof_file_info.vof_height;

    bzero(&eeprom, sizeof(struct venice_dg2_eeprom));

    eeprom.display_surface_width  = xpmax;
    eeprom.display_surface_height = ypmax;
    eeprom.going_to_vs2 = 0;
    eeprom.pan_x = 0;
    eeprom.pan_y = 0;
    for (i = 0; i < DG2_PARAMETER_CNT; i++)
	eeprom.normal_vof_top[i] = voftop[i];
    eeprom.normal_vof_body = vofbody;

    /*
     * write VOF to the DG2 eeprom (from fp)
     */

    /*
    if (!venice_set_dg2_eeprom(&eeprom)) {
	fprintf(stderr, "Can't write DG2 eeprom\n");
	exit(1);
    }
    */
    printf("#include \"sys/types.h\"\n");
    printf("#include \"sys/sema.h\"\n");
    printf("#include \"sys/gfx.h\"\n");
    printf("#include \"sys/rrm.h\"\n");
    printf("#include \"sys/venice.h\"\n");
    printf("#include \"vofdefs.h\"\n");
    printf("#include \"voftypedefs.h\"\n");
    printf("#include \"dg2_eeprom.h\"\n");

    printf("venice_dg2_eeprom_t backup_vof = {\n");
    printf("\t0,\t/* prom_id */\n");
    printf("\t0,\t/* prom_length */\n");
    printf("\t0,\t/* prom_checksum */\n");
    printf("\t0,\t/* prom_revision */\n");

    printf("\t%4d,\t/* display_surface_width */\n", eeprom.display_surface_width);
    printf("\t%4d,\t/* display_surface_height */\n", eeprom.display_surface_height);
    printf("\t%d,\t/* going_to_vs2 */\n", eeprom.going_to_vs2);

    printf("\t%d,\t/* pan_x */\n", eeprom.pan_x);
    printf("\t%d,\t/* pan_y */\n", eeprom.pan_y);

    printf("\t{\n");
    printf("\t\t%d,\t/* digipots[0] */\n", eeprom.digipots[0]);
    printf("\t\t%d,\t/* digipots[1] */\n", eeprom.digipots[1]);
    printf("\t\t%d,\t/* digipots[2] */\n", eeprom.digipots[2]);
    printf("\t\t%d,\t/* digipots[3] */\n", eeprom.digipots[3]);
    printf("\t\t%d,\t/* digipots[4] */\n", eeprom.digipots[4]);
    printf("\t},\n");

    printf("\t{\n");
    for(i = 0; i < DG2_PARAMETER_CNT; i++) {
	printf("\t\t%d,\t/* normal_vof_top[%d] */\n", eeprom.normal_vof_top[i],i);
    }
    printf("\t},\n");

    /* now the messy part, vof body */

    printf("\t{\n");
    printf("\t\t{\n");
    for(i = 0; i < VOF_STATE_TABLE_SIZE; i++) {
	printf("\t\t%d,\t/* normal_vof_body.state_duration_table[%d] */\n", eeprom.normal_vof_body.state_duration_table[i],i);
	
    }
    printf("\t\t},\n");

    printf("\t\t{\n");
    for(i = 0; i < VOF_STATE_TABLE_SIZE; i++) {
	printf("\t\t%d,\t/* normal_vof_body.line_type_table[%d] */\n", eeprom.normal_vof_body.line_type_table[i],i);
    }
    printf("\t\t},\n");

    printf("\t\t{\n");
    for(i = 0; i < VOF_EDGE_DEFINITION_SIZE; i++) {
	printf("\t\t%d,\t/* normal_vof_body.line_length_table[%d] */\n", eeprom.normal_vof_body.line_length_table[i],i);
    }
    printf("\t\t},\n");

    printf("\t\t{\n");
    for(i = 0; i <  VOF_EDGE_DEFINITION_SIZE; i++) {
	printf("\t\t\t{\n");
	for(j = 0; j < VOF_NUM_EDGES; j++) {
	    printf("\t\t{{");
	    for (k = 0; k < VOF_EDGE_HCOUNTS_SIZE; k++) {
	        printf("%d,",eeprom.normal_vof_body.signal_edge_table[i][j].edge[k]);
	    }
	    printf("},},\t/* normal_vof_body.signal_edge_table[%d][%d] */\n", i, j);
	}
	printf("\t\t\t},\n");
    }
    printf("\t\t},\n");

    printf("\t\t{\n");
    for(i = 0; i < VOF_DISPLAY_SCREEN_TABLE_SIZE; i++) {
	printf("\t\t%d,\t/* normal_vof_body.display_screen_table[%d] */\n", eeprom.normal_vof_body.display_screen_table[i],i);
    }
    printf("\t\t},\n");

    printf("\t\t{\n");
    printf("\t\t%d,\t/* normal_vof_body.vof_file_info.vof_width */\n", eeprom.normal_vof_body.vof_file_info.vof_width);
    printf("\t\t%d,\t/* normal_vof_body.vof_file_info.vof_height */\n", eeprom.normal_vof_body.vof_file_info.vof_height);
    printf("\t\t%d,\t/* normal_vof_body.vof_file_info.cursor_fudge_x */\n", eeprom.normal_vof_body.vof_file_info.cursor_fudge_x);
    printf("\t\t%d,\t/* normal_vof_body.vof_file_info.cursor_fudge_y */\n", eeprom.normal_vof_body.vof_file_info.cursor_fudge_y);
    printf("\t\t%d,\t/* normal_vof_body.vof_file_info.flags */\n", eeprom.normal_vof_body.vof_file_info.flags);
    printf("\t\t%d,\t/* normal_vof_body.vof_file_info.unused */\n", eeprom.normal_vof_body.vof_file_info.unused);
    printf("\t\t%d,\t/* normal_vof_body.vof_file_info.fields_per_frame */\n", eeprom.normal_vof_body.vof_file_info.fields_per_frame);
    printf("\t\t%d,\t/* normal_vof_body.vof_file_info.field_with_uppermost_line */\n", eeprom.normal_vof_body.vof_file_info.field_with_uppermost_line);
    printf("\t\t%d,\t/* normal_vof_body.vof_file_info.hwalk_length */\n", eeprom.normal_vof_body.vof_file_info.hwalk_length);
    printf("\t\t%d,\t/* normal_vof_body.vof_file_info.vof_framerate */\n", eeprom.normal_vof_body.vof_file_info.vof_framerate);
    printf("\t\t%d,\t/* normal_vof_body.vof_file_info.monitor_type */\n", eeprom.normal_vof_body.vof_file_info.monitor_type);
    printf("\t\t%d,\t/* normal_vof_body.vof_file_info.vof_total_width */\n", eeprom.normal_vof_body.vof_file_info.vof_total_width);
    printf("\t\t%d,\t/* normal_vof_body.vof_file_info.vof_total_height */\n", eeprom.normal_vof_body.vof_file_info.vof_total_height);
    printf("\t\t%d,\t/* normal_vof_body.vof_file_info.encoder_x_offset */\n", eeprom.normal_vof_body.vof_file_info.encoder_x_offset);
    printf("\t\t%d,\t/* normal_vof_body.vof_file_info.encoder_y_offset */\n", eeprom.normal_vof_body.vof_file_info.encoder_y_offset);
    /*
     * Dump out pixel density table.
     */
    printf("\t\t{\n");
    for(i = 0; i <  2; i++) {
	printf("\t\t\t{\n");
	for(j = 0; j < 3; j++) {
	    printf("\t\t\t%d,",
		eeprom.normal_vof_body.vof_file_info.pix_density[i][j]);
	    printf("\t/* normal_vof_body.vof_file_info.pix_density[%d][%d] */\n", i, j);
	}
	printf("\t\t\t},\n");
    }
    printf("\t\t},\n");
    /*
     * Dump out lines in field table.
     */
    printf("\t\t{\n");
    for(i = 0; i < VENICE_DG2_MAX_FIELDS; i++) {
	printf("\t\t\t%d,",eeprom.normal_vof_body.vof_file_info.lines_in_field[i]);
	printf("\t/* normal_vof_body.vof_file_info.lines_in_field[%d] */\n", i);
    }
    printf("\t\t},\n");

    printf("\t\t},\t/* end of vof_file_info_t initializations */\n");

    printf("\t},\t/* end of vof_body_t initializations */\n");
	

    printf("\t\t/* end of aggregate initialization */\n");
    /*
     * The remaining portion of struct venice_dg2_eeprom will be padded with
     * zeroes by the compiler; thus, the fields
     *
     *	int tovs2_vof_top[DG2_PARAMETER_CNT],
     *	vof_data_t tovs2_vof_body,
     *	unsigned long vs2_base_address,
     *	unsigned long vs2_vme_bus_number
     *
     * will all be zeroed when venice_dg2_eeprom_t backup_vof is compiled
     * as vof.c in the prom build.
     */
    printf("};\n");

}
