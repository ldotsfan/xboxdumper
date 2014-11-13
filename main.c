/*
    Xboxdumper - FATX library and utilities.

    Copyright (C) 2005 Andrew de Quincey <adq_dvb@lidskialf.net>

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
  printf("Syntax: xboxdumper <list|dump <FATX filename> <output filename>> <XBOX image file>\n");
  printf("Syntax: xboxdumper <create <XBOX image file> <partitionsize in MB>\n");
  printf("Syntax: xboxdumper <mkfs   <XBOX image file>\n");
  printf("Syntax: xboxdumper <cluster <XBOX image file> <sector number>\n");
  printf("Syntax: xboxdumper <listpartitions <XBOX image file>\n");
  printf("Syntax: xboxdumper <prepare <XBOX hdd dev> <partition type>\n");
  printf("Syntax: xboxdumper <preparefg <XBOX hdd dev> <partition type>\n");
  printf("Syntax: where partition type is value of 0, 1, 2 or 3\n");
  printf("Syntax: partition type 0: F partition is capped at 137Gb or less\n");
  printf("Syntax: partition type 1: F partition occupies all space\n");
  printf("Syntax: partition type 2: G partition occupies space after 137Gb\n");
  printf("Syntax: partition type 3: F and G partitions splits space evenly\n");
  printf("Syntax: xboxdumper <customf <XBOX hdd dev> <f partition size in GB>\n");
  exit(1);
}


/**
 * Main entry point
 */
int main(int argc, char* argv[]) {
  FILE *sourceFd;
  FATXPartition* partition;
  char* sourceFilename = NULL;
  char* extractFilename = NULL;
  char* outputFilename = NULL;
  FILE *outputFd = NULL;
  int listFiles = 0;
  int extractFile = 0;
  u_int64_t lFileSize;
  u_int64_t lNewPartSize = 0;
  
  // parse the arguments
  if (argc < 3) {
    syntax();
  }
  if (!strcmp(argv[1], "list")) {
    // extract details
    listFiles = 1;
    sourceFilename = argv[2];
  } else if (!strcmp(argv[1], "dump")) {
    // ensure we still have enough args
    if (argc < 5) {
      syntax();
    }

    // extract config
    extractFile = 1;
    extractFilename = argv[2];
    outputFilename = argv[3];
    sourceFilename = argv[4];
  } else if (!strcmp(argv[1], "create")) {
  	if(argc < 4) {
		syntax();
	}
	outputFilename = argv[2];
	lNewPartSize = atol(argv[3]);
	partition = createPartition(outputFilename, 0, lNewPartSize, 1);
	if(partition == 0) {
		printf("Error in creating FATX partition\n");
		exit(1);
	}
	exit(0);
  } else if (!strcmp(argv[1], "mkfs")) {
  	if(argc < 3) {
		syntax();
	}
	outputFilename = argv[2];
	partition = createPartition(outputFilename, 0, 0, 0);
	if(partition == 0) {
		printf("Error in mkfs FATX partition\n");
		exit(1);
	}
	exit(0);
  } 
  else if (!strcmp(argv[1], "listpartitions")) {
  	if(argc < 3) {
		syntax();
	}
	outputFilename = argv[2];
	partition = listPartitions(outputFilename);
	if(partition == 0) {
		printf("Error in listing partitions\n");
		exit(1);
	}
	exit(0);
  }else if (!strcmp(argv[1], "cluster")){
  	if(argc < 4) {
		syntax();
	}
	DumpSector(argv[2],atol(argv[3]));
	exit(0);
  } else if (!strcmp(argv[1], "preparefg")){
  	if(argc < 4) {
		syntax();
	}
	prepareFG(argv[2],atoi(argv[3]));
	exit(0);
  } else if (!strcmp(argv[1], "customfg")){
  	if(argc < 4) {
		syntax();
	}
	customFG(argv[2],atoi(argv[3]));
	exit(0);
  }
  else if (!strcmp(argv[1], "prepare")){
  	if(argc < 4) {
		syntax();
	}
	writeBRFR(argv[2],atoi(argv[3]));
	exit(0);
  }
  else {
    syntax();
  }

  // open output file
  if (extractFile) {
    outputFd = fopen(outputFilename, "w");
    if (outputFd == 0) {
      error("Unable to open output file %s", outputFilename);
    }
  }
  
  // open the file
  if ((sourceFd = fopen(sourceFilename, "r")) == 0) {
    error("Unable to open source file %s", sourceFilename);
  }

  fseeko(sourceFd, 0L, SEEK_END);
  lFileSize = ftello(sourceFd);
  fseeko(sourceFd, 0L, SEEK_SET);
  printf("Filename : %s , Filesize %lld\n",sourceFilename,(unsigned long long)lFileSize);
		  
  // open the partition
  partition = openPartition(sourceFd,0,lFileSize);
  
  // dump the directory tree
  if (listFiles) {
    dumpTree(partition, fileno(stdout));
  }
  if (extractFile) {
    dumpFile(partition, extractFilename, outputFd);
  }
  
  // close output file
  if (extractFile) {
    fclose(outputFd);
  }
  
  // close the partition
  closePartition(partition);
  
  // close the file
  fclose(sourceFd);
  return 1;
}
  
