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

// Definitions for FATX on-disk structures

#include <stdio.h>

#ifndef FATX_H
#define FATX_H

#define XBOX_SECTOR_STORE       (0x0055F400UL)
#define XBOX_SECTOR_SYSTEM      (0x00465400UL)
#define XBOX_SECTOR_CONFIG      (0x00000000UL)
#define XBOX_SECTOR_CACHE1      (0x00000400UL)
#define XBOX_SECTOR_CACHE2      (0x00177400UL)
#define XBOX_SECTOR_CACHE3      (0x002EE400UL)
#define XBOX_SECTOR_EXTEND      (0x00EE8AB0UL)
#define XBOX_SECTORS_CONFIG     (XBOX_SECTOR_CACHE1 - XBOX_SECTOR_CONFIG)

#define XBOX_SECTORS_STORE      (XBOX_SECTOR_EXTEND - XBOX_SECTOR_STORE)
#define XBOX_SECTORS_SYSTEM     (XBOX_SECTOR_STORE  - XBOX_SECTOR_SYSTEM)
#define XBOX_SECTORS_CACHE1     (XBOX_SECTOR_CACHE2 - XBOX_SECTOR_CACHE1)
#define XBOX_SECTORS_CACHE2     (XBOX_SECTOR_CACHE3 - XBOX_SECTOR_CACHE2)
#define XBOX_SECTORS_CACHE3     (XBOX_SECTOR_SYSTEM - XBOX_SECTOR_CACHE3)
#define XBOX_SECTORS_CONFIG     (XBOX_SECTOR_CACHE1 - XBOX_SECTOR_CONFIG)

// Size of FATX partition header
#define FATX_PARTITION_HEADERSIZE 0x1000

// FATX partition magic
#define FATX_PARTITION_MAGIC 0x58544146

// FATX chain table block size
#define FATX_CHAINTABLE_BLOCKSIZE 4096

// ID of the root FAT cluster
#define FATX_ROOT_FAT_CLUSTER 1

// Size of FATX directory entries
#define FATX_DIRECTORYENTRY_SIZE 0x40

// File attribute: read only
#define FATX_FILEATTR_READONLY 0x01

// File attribute: hidden
#define FATX_FILEATTR_HIDDEN 0x02

// File attribute: system 
#define FATX_FILEATTR_SYSTEM 0x04

// File attribute: archive
#define FATX_FILEATTR_ARCHIVE 0x20

// Directory entry flag indicating entry is a sub-directory
#define FATX_FILEATTR_DIRECTORY 0x10

// max filename size
#define FATX_FILENAME_MAX 42

// This structure describes a FATX partition
typedef struct {
  // The source file
  FILE *sourceFd;

  // The starting byte of the partition
  u_int64_t partitionStart;

  // The size of the partition in bytes
  u_int64_t partitionSize;

  // The cluster size of the partition
  u_int32_t clusterSize;

  // Number of clusters in the partition
  u_int32_t clusterCount;

  // Size of entries in the cluster chain map
  u_int32_t chainMapEntrySize;

  // Size of the chaintable
  u_int64_t chainTableSize;
  
  // The cluster chain map table (which may be in words OR dwords)
  union {
    u_int16_t* words;
    u_int32_t* dwords;
  } clusterChainMap;
  
  // Address of cluster 1
  u_int64_t cluster1Address;
  
} FATXPartition;

typedef struct {
	u_int8_t filenameSize;
	u_int8_t attributes;
	char filename[FATX_FILENAME_MAX];
	u_int32_t firstCluster;
	u_int32_t fileSize;
	u_int16_t modTime;	
	u_int16_t modDate;
	u_int16_t createTime;	
	u_int16_t createDate;
	u_int16_t laccessTime;	
	u_int16_t laccessDate;
/*	
0 1 Size of filename (max. 42)
1 1 Attribute as on FAT
2 42 Filename in ASCII, padded with 0xff (not zero-terminated)
44 4 First cluster
48 4 File size in bytes
52 2 Modification time
54 2 Modification date
56 2 Creation time
58 2 Creation date
60 2 Last access time
62 2 Last access date
*/
} FATXDirEntry;

/**
 * Open a FATX partition
 *
 * @param sourceFd File to read from
 * @param partitionOffset Offset into above file that partition starts at
 * @param partitionSize Size of partition in bytes
 */
FATXPartition* openPartition(FILE *sourceFd,
                             u_int64_t partitionOffset,
                             u_int64_t partitionSize);


/**
 * Close a FATX partition
 */
void closePartition(FATXPartition* partition);

/**
 * Dump entire directory tree to supplied stream
 *
 * @param partition The FATX partition
 * @param outputStream Stream to output data to
 */
void dumpTree(FATXPartition* partition, int outputStream);

void dumpFile(FATXPartition* partition, char* filename, FILE *outputStream);

FATXPartition* createPartition(char *szFileName, u_int64_t partitionOffset, 
		u_int64_t partitionSize,int nCreate);

void DumpSector(char *szFileName, long lSector);
unsigned long getDiskSize(char *szDrive);

int writeBRFR(char *szDrive,int p_mode);
int listPartitions(char *szDrive);
int prepareFG(char *szDrive,int p_mode);
#endif
