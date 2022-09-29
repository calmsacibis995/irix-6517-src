/*
 * Copyright 1994, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <invent.h>
#include <stdio.h>
#include <stream.h>
#include <string.h>
#include <libgen.h>

#include <iMachTypes.h>

const int END = -2;

// Include all release-specific tables

#include "machtable"


char* itoa(int number)
{
    static char buff[20];

    sprintf(buff,"%d",number);

    return buff;
}


int
create_mach_map(ostream& mach_file)
{
    char  line[512];
    char  class_[20],type_[20],state_[20],statemask_[20],attribute_[20];
    int   i=0;

    while (map[i].class_ != END) {

	strcpy(class_,itoa(map[i].class_));
	strcpy(type_,itoa(map[i].type_));
	strcpy(state_,itoa(map[i].state_));
	strcpy(statemask_,itoa(map[i].statemask_));
	strcpy(attribute_,itoa(map[i].attribute_));

	if (map[i].val_[0] == '\0')
	    strcpy(map[i].val_,"NULL");

	sprintf(line,"%-5.5s %-5.5s %-5.5s %-8.8s %-5.5s %s",
		class_,type_,state_,statemask_,attribute_,map[i].val_);

	mach_file << line << endl;

	i++;
    }


    return 0;

}



int
create_subgr_map(ofstream& mach_file)
{
    char  line[512];
    int   i=0;


    while (subgrmap[i].cpu_[0] != '\0') {

	if (subgrmap[i].cpu_[0] == '\0')
	    strcpy(subgrmap[i].cpu_,"NULL");

	if (subgrmap[i].gfx_[0] == '\0')
	    strcpy(subgrmap[i].gfx_,"NULL");

	if (subgrmap[i].gfxboard_[0] == '\0')
	    strcpy(subgrmap[i].gfxboard_,"NULL");

	if (subgrmap[i].subgr_[0] == '\0')
	    strcpy(subgrmap[i].subgr_,"NULL");

	if (subgrmap[i].machine_[0] == '\0')
	    strcpy(subgrmap[i].machine_,"NULL");


	sprintf(line,"%-10.10s  %-15.15s  %-15.15s  %-15.15s  %s",
		subgrmap[i].cpu_,subgrmap[i].gfx_,subgrmap[i].gfxboard_,
		subgrmap[i].subgr_, subgrmap[i].machine_);

	mach_file << line << endl;

	i++;
    }

    return 0;
}




int
create_arch_map(ofstream& mach_file)
{
    char  line[512];
    int   i=0;


    while (archmap[i].cpu_[0] != '\0') {

	if (archmap[i].arch_[0] == '\0')
	    strcpy(archmap[i].arch_,"NULL");

	if (archmap[i].mode_[0] == '\0')
	    strcpy(archmap[i].mode_,"NULL");

	sprintf(line,"%-10.10s  %-20.20s  %s",
		archmap[i].cpu_,archmap[i].arch_,archmap[i].mode_);

	mach_file << line << endl;

	i++;
    }

    return 0;
}






main(int argc,char** argv)
{
    if (argc != 2) {
	cerr << "usage: mkmachfile <machfile>" << endl;
	return -1;
    }

    char* outfile     = argv[1];

    ofstream mach_file;

    //
    // Create the external machfile.
    //

    mach_file.open(outfile);

    if (!mach_file.good()) {
	cerr << "Unable to open " << outfile << endl;
	return -1;
    }

    if (create_mach_map(mach_file) < 0) {
	mach_file.close();
	return -1;
    }

    mach_file << "\n\nSUBGRMAP\n" << endl;
    
    if (create_subgr_map(mach_file) < 0) {
	mach_file.close();
	return -1;
    }

    mach_file << "\n\nARCHMAP\n" << endl;

    if (create_arch_map(mach_file) < 0) {
	mach_file.close();
	return -1;
    }

    mach_file.close();

    return 0;
}
