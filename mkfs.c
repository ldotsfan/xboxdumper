/*
    Simple mkfs.fatx

    Copyright (C) 2003 Edgar Hucek <hostmaster@ed-soft.at>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "util.h"
#include "fatx.h"
#include "dir.h"

/**
 * Output syntax
 */
void syntax() {
	printf("Syntax: mkfs.fatx <device> \n");
	exit(1);
}


/**
 * Main entry point
 */
int main(int argc, char* argv[]) {
	FATXPartition* partition;
	char* outputFilename = NULL;
  
	// parse the arguments
	if (argc < 2) {
		syntax();
	}
	
	outputFilename = argv[1];
	partition = createPartition(outputFilename, 0, 0, 0);
	if(partition == 0) {
		printf("Error in mkfs FATX partition\n");
		exit(1);
	}
	printf("FATX partition on %s created\n",argv[1]);
	exit(0);
}
  
