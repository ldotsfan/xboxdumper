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
    along with this program; if not, fwrite to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

// Functions for processing FATX partitions

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>
#include <linux/fs.h>
#include <errno.h>
#include "fatx.h"
#include "util.h"
#include "partition.h"

#define DEBUG

/**
 * Checks if the current entry is the last entry in a directory
 *
 * @param entry Raw entry data
 * @return 1 If it is the last entry, 0 if not
 */
int checkForLastDirectoryEntry(unsigned char* entry);

/**
 * Gets the next cluster in the cluster chain
 *
 * @param partition FATX partition
 * @param clusterId Cluster to find the netx cluster for
 * @return ID of the next cluster in the chain, or -1 if there is no next cluster
 */
u_int32_t getNextClusterInChain(FATXPartition* partition, int clusterId);

/**
 * Load data for a cluster
 *
 * @param partition FATX partition
 * @param clusterId ID of the cluster to load
 * @param clusterData Where to store the data (must be at least the cluster size)
 */
void loadCluster(FATXPartition* partition, unsigned long clusterId, unsigned char* clusterData);

/**
 * fwrite data to a cluster
 *
 * @param partition FATX partition
 * @param clusterId ID of the cluster to fwrite
 * @param clusterData
 */
void fwriteCluster(FATXPartition* partition, int clusterId, unsigned char* clusterData);

/**
 * Recursively dump the directory tree
 *
 * @param partition FATX Partition
 * @param outputStream Stream to output to
 * @param clusterId ID of cluster to start at
 * @param nesting Nesting level of directory tree
 */
void _dumpTree(FATXPartition* partition, 
               int outputStream,
               int clusterId, int nesting);



/**
 * Recursively down a directory tree to find and extract a specific file
 *
 * @param partition FATX Partition
 * @param filename Filename to extract
 * @param outputStream Stream to output to
 * @param clusterId ID of cluster to start at
 */
void _recurseToFile(FATXPartition* partition, 
                    char* filename,
                    FILE *outputStream,
                    int clusterId);



/** 
 * Dump a file to supplied outputStream
 *
 * @param partition FATX partition
 * @param outputStream Stream to output to
 * @param clusterId Starting cluster ID of file
 * @param fileSize Size of file
 */
void _dumpFile(FATXPartition* partition, FILE *outputStream, 
               int clusterId, u_int32_t fileSize);


void fwriteChainMap(FATXPartition *partition) {
	if((fseeko(partition->sourceFd, partition->partitionStart + FATX_PARTITION_HEADERSIZE, SEEK_SET)) == -1) {
		printf("fwriteChainMap : Error in setting position %lld\n",partition->partitionStart + FATX_PARTITION_HEADERSIZE);
	}
	fwrite(partition->clusterChainMap.words, 1, 2*sizeof(u_int64_t), partition->sourceFd);
	// append zero until the size of chainTableSize
	char buffer[] = {0x00};
	int i;
	for (i=0;i<partition->chainTableSize-sizeof(partition->clusterChainMap);i++)
		fwrite(buffer, 1, 1,partition->sourceFd); 	

}
	       
void DumpSector(char *szFileName, long lSector) {
	FILE *fp;
	unsigned char buffer[16*1024];
	u_int64_t lFileSize;
	u_int64_t cluster;
	FATXPartition *partition;	
	int i,n;

	fp = fopen(szFileName,"r");
	
	if(fp == NULL) {
		printf("DumpSector : error in opening file %s\n",szFileName);
		exit(0);
	}

	
	fseeko(fp, 0L, SEEK_END);
	lFileSize = ftello(fp);
	fseeko(fp, 0L, SEEK_SET);
	
        printf("DumpCluster : Filename %s Filesize %lld\n", szFileName, lFileSize);

	partition = openPartition(fp,0,lFileSize);

	cluster = lSector;

	loadCluster(partition, cluster, buffer);

	for(i = 0 ; i < 512/16; i++) {
		for(n=0;n < 16;n++) {
			printf("%02x ",buffer[(16*i) + n]);
		}
		printf("    ");
		for(n=0;n < 16;n++) {
			{
				char c;
				c = buffer[(16*i) + n];
				if((c<32) || (c>126))  c='.';
				printf("%c",c);
			}
		}
		printf("\n");
		
	}
	
	fclose(fp);
	
}
unsigned long getDiskSize(char *szDrive) {
	unsigned char args[4+512] = { WIN_IDENTIFY, 0, 0, 1, };
	unsigned short *drive_info=&args[4];

	int rc;
	int dev = open(szDrive,O_RDONLY);
	if (dev <= 0) {
		printf("Unable to open device %s\n",szDrive);
		return 0;
	}
	
	rc = ioctl(dev,HDIO_DRIVE_CMD,args);
	if(rc != 0) {
		printf("HDIO_DRIVE_CMD(identify) failed - is drive frozen?\n");
		return 0;
	}
     	close(dev);
     
	unsigned long sectorsTotal = *((unsigned int*)&(drive_info[60]));
	if( drive_info[86] & 1ul<<10 )  
        	sectorsTotal = *((unsigned int*)&(drive_info[100]));
	return sectorsTotal;
}
	       
int writeBRFR(char *szDrive,int partition_mode) {

	FILE *fp;
	int fd,i;
 	int error = 0;
	FATXPartition* partition = NULL;
	u_int64_t start,size,totalsize,totalsectors,tb_totalsectors=0;
	char szBuffer[512];
	XboxPartitionTable *BackupPartTbl;
	XboxPartitionTable *PartTbl;
	XboxPartitionTable DefaultPartTbl =
{
	{ '*', '*', '*', '*', 'P', 'A', 'R', 'T', 'I', 'N', 'F', 'O', '*', '*', '*', '*' },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{
		{ { 'X', 'B', 'O', 'X', ' ', 'S', 'H', 'E', 'L', 'L', ' ', ' ', ' ', ' ', ' ', ' '}, PE_PARTFLAGS_IN_USE, XBOX_SECTOR_STORE, XBOX_SECTORS_STORE, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'D', 'A', 'T', 'A', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, PE_PARTFLAGS_IN_USE, XBOX_SECTOR_SYSTEM, XBOX_SECTORS_SYSTEM, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'G', 'A', 'M', 'E', ' ', 'S', 'W', 'A', 'P', ' ', '1'}, PE_PARTFLAGS_IN_USE, XBOX_SECTOR_CACHE1, XBOX_SECTORS_CACHE1, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'G', 'A', 'M', 'E', ' ', 'S', 'W', 'A', 'P', ' ', '2'}, PE_PARTFLAGS_IN_USE, XBOX_SECTOR_CACHE2, XBOX_SECTORS_CACHE2, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'G', 'A', 'M', 'E', ' ', 'S', 'W', 'A', 'P', ' ', '3'}, PE_PARTFLAGS_IN_USE, XBOX_SECTOR_CACHE3, XBOX_SECTORS_CACHE3, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'F', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, XBOX_PART6_LBA_START, 0, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'G', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, XBOX_PART6_LBA_START, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
	}
};
	
	memset(szBuffer,0,256);
	//fseeko (fp,0L,SEEK_END);
	//totalsize = ftello(fp);
	//totalsectors = totalsize/512ULL;
	totalsectors = getDiskSize(szDrive);

	printf("Total Sectors  -> %lld\n",totalsectors);

	fp = fopen (szDrive, "r+b");
	if(!fp) {
		printf("Error opening %s\n",szDrive);
		return 0;
	}
	fseeko(fp,0x600,SEEK_SET);
	fread(szBuffer,4,1,fp);
	
	
	printf("freaded  -> %c%c%c%c\n",szBuffer[0],szBuffer[1],szBuffer[2],szBuffer[3]);

	memset(szBuffer,0,512);
			
	fseeko(fp,0L,SEEK_SET);
	for(i = XBOX_SECTOR_CONFIG;i < XBOX_SECTORS_CONFIG;i++)
		fwrite(szBuffer,512,1,fp);

	fseeko(fp,0x600,SEEK_SET);
	sprintf(szBuffer,"BRFR");
	fwrite(szBuffer,4,1,fp);

	memset(szBuffer,0,256);

	fseeko(fp,0x600,SEEK_SET);
	fread(szBuffer,4,1,fp);
	
	printf("Written -> %c%c%c%c\n",szBuffer[0],szBuffer[1],szBuffer[2],szBuffer[3]);
	PartTbl = (XboxPartitionTable*) malloc(sizeof(XboxPartitionTable));
	BackupPartTbl = (XboxPartitionTable*) malloc(sizeof(XboxPartitionTable));

	memcpy (BackupPartTbl,&DefaultPartTbl,sizeof(XboxPartitionTable));

	part_setup_standard_partitions (PartTbl,BackupPartTbl,totalsectors,partition_mode,tb_totalsectors);

	fseeko(fp,0L,SEEK_SET);
	fwrite(PartTbl, sizeof(XboxPartitionTable), 1, fp);
	
	fclose(fp);

	sprintf(szBuffer,"part50");
	start = XBOX_SECTOR_STORE * 512ULL;
	size = XBOX_SECTORS_STORE * 512ULL;
	printf("Creating -> %s start %lld size %lld\n",szBuffer, start ,size);
	
	partition = createPartition(szDrive, start,size, 0);
	if(partition == NULL) {
		printf("Error creating -> %s\n",szBuffer);
		return 0;
	}
	closePartition(partition);

	sprintf(szBuffer,"part51");
	start = XBOX_SECTOR_SYSTEM * 512ULL;
	size = XBOX_SECTORS_SYSTEM * 512ULL;
	printf("Creating -> %s start %lld size %lld\n",szBuffer, start ,size);
	
	partition = createPartition(szDrive, start,size, 0);
	if(partition == NULL) {
		printf("Error creating -> %s\n",szBuffer);
		return 0;
	}
	closePartition(partition);

	sprintf(szBuffer,"part52");
	start = XBOX_SECTOR_CACHE1 * 512ULL;
	size = XBOX_SECTORS_CACHE1 * 512ULL;
	printf("Creating -> %s start %lld size %lld\n",szBuffer, start ,size);
	
	partition = createPartition(szDrive, start,size, 0);
	if(partition == NULL) {
		printf("Error creating -> %s\n",szBuffer);
		return 0;
	}
	closePartition(partition);

	sprintf(szBuffer,"part53");
	start = XBOX_SECTOR_CACHE2 * 512ULL;
	size = XBOX_SECTORS_CACHE2 * 512ULL;
	printf("Creating -> %s start %lld size %lld\n",szBuffer, start ,size);
	
	partition = createPartition(szDrive, start,size, 0);
	if(partition == NULL) {
		printf("Error creating -> %s\n",szBuffer);
		return 0;
	}
	closePartition(partition);

	sprintf(szBuffer,"part54");
	start = XBOX_SECTOR_CACHE3 * 512ULL;
	size = XBOX_SECTORS_CACHE3 * 512ULL; 
	printf("Creating -> %s start %lld size %lld\n",szBuffer, start ,size);
	
	partition = createPartition(szDrive, start,size, 0);
	if(partition == NULL) {
		printf("Error creating -> %s\n",szBuffer);
		return 0;
	}
	closePartition(partition);
	
	for (i=5;i<14;i++)
	{
		if (PartTbl->TableEntries[i].Flags & PE_PARTFLAGS_IN_USE)
		{
			printf("Creating -> start %llu size %llu\n", (PartTbl->TableEntries[i].LBAStart)*512ULL,(PartTbl->TableEntries[i].LBASize)*512ULL);
			partition = createPartition(szDrive, (PartTbl->TableEntries[i].LBAStart)*512ULL,(PartTbl->TableEntries[i].LBASize)*512ULL,0);
			closePartition (partition);
		}
	}
        printf("Calling ioctl() to re-fread partition table.\n");
	for (i=0;i<14;i++)
	{
		if (PartTbl->TableEntries[i].Flags & PE_PARTFLAGS_IN_USE)
		{
			printf("partition %d start %llu size %llu\n", i,(PartTbl->TableEntries[i].LBAStart)*512ULL,(PartTbl->TableEntries[i].LBASize)*512ULL);
		}
	}
        sync();
	sleep(2);
	
	fd = open(szDrive,O_RDONLY);
        if ((i = ioctl(fd, BLKRRPART)) != 0) {
		error = errno;
	} else {
                sync();
                sleep(2);
		if ((i = ioctl(fd, BLKRRPART)) != 0)
			error = errno;
	}
	
	close(fd);

	return 1;
}
int prepareFG(char *szDrive,int partition_mode) {

	FILE *fp;
	int fd,i;
 	int error = 0;
	FATXPartition* partition = NULL;
	u_int64_t start,size,totalsize,totalsectors,tb_totalsectors=0;
	char szBuffer[512];
	XboxPartitionTable *BackupPartTbl;
	XboxPartitionTable *PartTbl;
	XboxPartitionTable DefaultPartTbl =
{
	{ '*', '*', '*', '*', 'P', 'A', 'R', 'T', 'I', 'N', 'F', 'O', '*', '*', '*', '*' },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{
		{ { 'X', 'B', 'O', 'X', ' ', 'S', 'H', 'E', 'L', 'L', ' ', ' ', ' ', ' ', ' ', ' '}, PE_PARTFLAGS_IN_USE, XBOX_SECTOR_STORE, XBOX_SECTORS_STORE, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'D', 'A', 'T', 'A', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, PE_PARTFLAGS_IN_USE, XBOX_SECTOR_SYSTEM, XBOX_SECTORS_SYSTEM, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'G', 'A', 'M', 'E', ' ', 'S', 'W', 'A', 'P', ' ', '1'}, PE_PARTFLAGS_IN_USE, XBOX_SECTOR_CACHE1, XBOX_SECTORS_CACHE1, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'G', 'A', 'M', 'E', ' ', 'S', 'W', 'A', 'P', ' ', '2'}, PE_PARTFLAGS_IN_USE, XBOX_SECTOR_CACHE2, XBOX_SECTORS_CACHE2, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'G', 'A', 'M', 'E', ' ', 'S', 'W', 'A', 'P', ' ', '3'}, PE_PARTFLAGS_IN_USE, XBOX_SECTOR_CACHE3, XBOX_SECTORS_CACHE3, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'F', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, XBOX_PART6_LBA_START, 0, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'G', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, XBOX_PART6_LBA_START, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
	}
};

	
	totalsectors = getDiskSize(szDrive);
	
	printf("Total Sectors  -> %lld\n",totalsectors);
	fp = fopen (szDrive, "r+b");
	if(!fp) {
		printf("Error opening %s\n",szDrive);
		return 0;
	}
	
	memset(szBuffer,0,256);
	memset(szBuffer,0,512);
			
	fseeko(fp,0L,SEEK_SET);
	for(i = XBOX_SECTOR_CONFIG;i < XBOX_SECTORS_CONFIG;i++)
		fwrite(szBuffer,512,1,fp);
	
	fseeko(fp,0x600,SEEK_SET);
	sprintf(szBuffer,"BRFR");
	fwrite(szBuffer,4,1,fp);

	PartTbl = (XboxPartitionTable*) malloc(sizeof(XboxPartitionTable));
	BackupPartTbl = (XboxPartitionTable*) malloc(sizeof(XboxPartitionTable));

	memcpy (BackupPartTbl,&DefaultPartTbl,sizeof(XboxPartitionTable));

	part_setup_standard_partitions (PartTbl,BackupPartTbl,totalsectors,partition_mode,tb_totalsectors);

	fseeko(fp,0L,SEEK_SET);
	fwrite(PartTbl, sizeof(XboxPartitionTable), 1, fp);
	
	fclose(fp);

	for (i=5;i<14;i++)
	{
		if (PartTbl->TableEntries[i].Flags & PE_PARTFLAGS_IN_USE)
		{
			printf("Creating -> start %llu size %llu\n", (PartTbl->TableEntries[i].LBAStart)*512ULL,(PartTbl->TableEntries[i].LBASize)*512ULL);
			partition = createPartition(szDrive, (PartTbl->TableEntries[i].LBAStart)*512ULL,(PartTbl->TableEntries[i].LBASize)*512ULL,0);
			closePartition (partition);
		}
	}
        printf("Calling ioctl() to re-fread partition table.\n");
	for (i=0;i<14;i++)
	{
		if (PartTbl->TableEntries[i].Flags & PE_PARTFLAGS_IN_USE)
		{
			printf("partition %d start %llu size %llu\n", i,(PartTbl->TableEntries[i].LBAStart)*512ULL,(PartTbl->TableEntries[i].LBASize)*512ULL);
		}
	}
        sync();
	sleep(2);
	
	/*fd = open(szDrive,O_RDONLY);
        if ((i = ioctl(fd, BLKRRPART)) != 0) {
		error = errno;
	} else {
                sync();
                sleep(2);
		if ((i = ioctl(fd, BLKRRPART)) != 0)
			error = errno;
	}
	
	close(fd);
*/	
	return 1;
}
int customFG(char *szDrive,int f_percent) {

	FILE *fp;
	int fd,i;
 	int error = 0;
	FATXPartition* partition = NULL;
	u_int64_t start,size,totalsize,totalsectors,tb_totalsectors=0;
	char szBuffer[512];
	XboxPartitionTable *BackupPartTbl;
	XboxPartitionTable *PartTbl;
	XboxPartitionTable DefaultPartTbl =
{
	{ '*', '*', '*', '*', 'P', 'A', 'R', 'T', 'I', 'N', 'F', 'O', '*', '*', '*', '*' },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{
		{ { 'X', 'B', 'O', 'X', ' ', 'S', 'H', 'E', 'L', 'L', ' ', ' ', ' ', ' ', ' ', ' '}, PE_PARTFLAGS_IN_USE, XBOX_SECTOR_STORE, XBOX_SECTORS_STORE, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'D', 'A', 'T', 'A', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, PE_PARTFLAGS_IN_USE, XBOX_SECTOR_SYSTEM, XBOX_SECTORS_SYSTEM, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'G', 'A', 'M', 'E', ' ', 'S', 'W', 'A', 'P', ' ', '1'}, PE_PARTFLAGS_IN_USE, XBOX_SECTOR_CACHE1, XBOX_SECTORS_CACHE1, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'G', 'A', 'M', 'E', ' ', 'S', 'W', 'A', 'P', ' ', '2'}, PE_PARTFLAGS_IN_USE, XBOX_SECTOR_CACHE2, XBOX_SECTORS_CACHE2, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'G', 'A', 'M', 'E', ' ', 'S', 'W', 'A', 'P', ' ', '3'}, PE_PARTFLAGS_IN_USE, XBOX_SECTOR_CACHE3, XBOX_SECTORS_CACHE3, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'F', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, XBOX_PART6_LBA_START, 0, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'G', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, XBOX_PART6_LBA_START, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, 0, 0, 0, 0 },
	}
};

	
	totalsectors = getDiskSize(szDrive);
	
	printf("Total Sectors  -> %lld\n",totalsectors);
	fp = fopen (szDrive, "r+b");
	if(!fp) {
		printf("Error opening %s\n",szDrive);
		return 0;
	}
	
	memset(szBuffer,0,512);
			
	fseeko(fp,0L,SEEK_SET);
	for(i = XBOX_SECTOR_CONFIG;i < XBOX_SECTORS_CONFIG;i++)
		fwrite(szBuffer,512,1,fp);
	
	fseeko(fp,0x600,SEEK_SET);
	sprintf(szBuffer,"BRFR");
	fwrite(szBuffer,4,1,fp);

	PartTbl = (XboxPartitionTable*) malloc(sizeof(XboxPartitionTable));
	BackupPartTbl = (XboxPartitionTable*) malloc(sizeof(XboxPartitionTable));

	memcpy (BackupPartTbl,&DefaultPartTbl,sizeof(XboxPartitionTable));

	part_setup_desiredsize_partitions (PartTbl,BackupPartTbl,totalsectors,f_percent,tb_totalsectors);

	fseeko(fp,0L,SEEK_SET);
	fwrite(PartTbl, sizeof(XboxPartitionTable), 1, fp);
	
	fclose(fp);

	for (i=5;i<14;i++)
	{
		if (PartTbl->TableEntries[i].Flags & PE_PARTFLAGS_IN_USE)
		{
			printf("Creating -> start %llu size %llu\n", PartTbl->TableEntries[i].LBAStart*512ULL,PartTbl->TableEntries[i].LBASize*512ULL);
			partition = createPartition(szDrive, PartTbl->TableEntries[i].LBAStart*512ULL,PartTbl->TableEntries[i].LBASize*512ULL,0);
			closePartition (partition);
		}
	}
        printf("Calling ioctl() to re-fread partition table.\n");
	for (i=0;i<14;i++)
	{
		if (PartTbl->TableEntries[i].Flags & PE_PARTFLAGS_IN_USE)
		{
			printf("partition %d start %llu size %llu\n", i,PartTbl->TableEntries[i].LBAStart*512ULL,PartTbl->TableEntries[i].LBASize*512ULL);
		}
	}
        sync();
	sleep(2);
	
	/*fd = open(szDrive,O_RDONLY);
        if ((i = ioctl(fd, BLKRRPART)) != 0) {
		error = errno;
	} else {
                sync();
                sleep(2);
		if ((i = ioctl(fd, BLKRRPART)) != 0)
			error = errno;
	}
	
	close(fd);
*/	
	return 1;
}


int listPartitions(char *szDrive) {

	FILE *fp;
	int fd,i,result;
 	int error = 0;
	FATXPartition* partition = NULL;
	u_int64_t start,size,totalsize,totalsectors;
	char szBuffer[512];
	XboxPartitionTable *PartTbl;
	FATX_SUPERBLOCK fatx_sb;
	int cl_size=0;
	
	
	totalsectors = getDiskSize(szDrive);
	
	printf("Total Sectors  -> %lld\n",totalsectors);
	fp = fopen (szDrive, "r");
	
        if(!fp) {
		printf("Error opening %s\n",szDrive);
		return 0;
	}
	PartTbl = (XboxPartitionTable*) malloc(sizeof(XboxPartitionTable));

	fseeko(fp,0L,SEEK_SET);
	result = fread(PartTbl, sizeof(XboxPartitionTable), 1, fp);
	
	if (result != 1){
		printf("Error freading %s\n",szDrive);
		return 0;
	}

	if (strcmp(PartTbl->Magic,"****PARTINFO****")){
		printf("Error freading partition table with strict magic compare in %s\n",szDrive);
			if (strstr(PartTbl->Magic,"PARTINFO")==NULL){
				printf("Error freading partition table with relaxed magic compare in %s\n",szDrive);
				return 0;
			}
			else
			{
				printf("Partition table:\n");
				for (i=0;i<14;i++)
				{
					if (PartTbl->TableEntries[i].Flags & PE_PARTFLAGS_IN_USE)
					{
						printf("partition %d\tstart %lu\tsize\t%010luMB\t", i,PartTbl->TableEntries[i].LBAStart,PartTbl->TableEntries[i].LBASize*512ULL/1048576);
						fseeko(fp,PartTbl->TableEntries[i].LBAStart*512ULL,SEEK_SET);
						result = fread(&fatx_sb, sizeof(FATX_SUPERBLOCK), 1, fp);
						if ((fatx_sb.ClusterSize>0) && (fatx_sb.ClusterSize % 16 == 0))
						{
							cl_size=fatx_sb.ClusterSize/2;
							if ((PartTbl->TableEntries[i].LBASize>=LBASIZE_512GB && cl_size<64)||
									(PartTbl->TableEntries[i].LBASize>=LBASIZE_256GB && cl_size<32))
								printf("Cluster size %02dK-ERR\n",cl_size);
							else
								printf("Cluster size %02dK\n",cl_size);
						}
						else
 						{
							printf("Unknown cluster size\n");
						}
 					}
				}
			}
	}
	else
	{
		printf("Partition table:\n");
		for (i=0;i<14;i++)
		{
			if (PartTbl->TableEntries[i].Flags & PE_PARTFLAGS_IN_USE)
			{
				printf("partition %d\tstart %lu\tsize\t%010luMB\t", i,PartTbl->TableEntries[i].LBAStart,PartTbl->TableEntries[i].LBASize*512ULL/1048576);
				fseeko(fp,PartTbl->TableEntries[i].LBAStart*512ULL,SEEK_SET);
				result = fread(&fatx_sb, sizeof(FATX_SUPERBLOCK), 1, fp);
				if ((fatx_sb.ClusterSize>0) && (fatx_sb.ClusterSize % 16 == 0))
				{
					cl_size=fatx_sb.ClusterSize/2;
					if ((PartTbl->TableEntries[i].LBASize>=LBASIZE_512GB && cl_size<64)||
								(PartTbl->TableEntries[i].LBASize>=LBASIZE_256GB && cl_size<32))
						printf("Cluster size %02dK-ERR\n",cl_size);
					else
						printf("Cluster size %02dK\n",cl_size);
				}
				else
 				{
					printf("Unknown cluster size\n");
				}
 
				
			}
		}

	}
	fclose(fp);

	
	return 1;
}

FATXPartition* createPartition(char *szFileName, u_int64_t partitionOffset, 
		u_int64_t partitionSize, int nCreate) {

	FATXPartition* partition;
	unsigned char partitionInfo[FATX_PARTITION_HEADERSIZE];
	unsigned char *clusterData;
	char szBuffer[1024];
	long lRet;
	u_int64_t i;
	u_int32_t eocMarker;
	u_int32_t rootFatMarker;

	memset(szBuffer,0,1024);
	
	partition = (FATXPartition*) malloc(sizeof(FATXPartition));
	
	if (partition == NULL) {
		printf("createPartition -> Out of memory\n");
		return NULL;
	}

	memset(partitionInfo,0,FATX_PARTITION_HEADERSIZE);
	*(u_int64_t *)partitionInfo = FATX_PARTITION_MAGIC;
	*(u_int64_t *)&partitionInfo[0x0004] = (u_int64_t) time(NULL); // Volume id
	if(!nCreate) {
		*(u_int64_t *)&partitionInfo[0x0008] = 0x00000020; // Clustersize in 512 bytes
	} else {
		*(u_int64_t *)&partitionInfo[0x0008] = 0x00000004; // Clustersize in 512 bytes
	}
	*(u_int16_t *)&partitionInfo[0x000C] = 0x0001; //Number of active FATs (always 1) (?)
	*(u_int32_t *)&partitionInfo[0x000E] = 0x00000000; //Unknown (always set to 0)
	memset(&partitionInfo[0x0012],0xff,0xfee); //Unknown (usually set to 0xff, or 0). Probably padding.

	memset(partition,0,sizeof(FATXPartition));

	if(!nCreate) {
		partition->sourceFd = fopen(szFileName,"r+");
	} else {
		partition->sourceFd = fopen(szFileName,"w+");
		partition->partitionSize = partitionSize * 1024 *1024;
	}

	if(partition->sourceFd == NULL) {
		printf("createPartition -> Error creating File %s\n",szFileName);
		return NULL;
	}
	
	if(!nCreate && (partitionSize == 0)) {
		fseeko(partition->sourceFd,0L,SEEK_END);
		partition->partitionSize = ftello(partition->sourceFd);
		fseeko(partition->sourceFd,0L,SEEK_SET);
	} else if(!nCreate && (partitionSize != 0)) {
		partition->partitionSize = partitionSize;
	}

	partition->partitionStart = partitionOffset;
	if (!nCreate){
	if ((partition->partitionSize >= (512000000000ULL))){ //more than 512GB
                *(u_int64_t *)&partitionInfo[0x0008] = 0x00000080; // Clustersize in 512 bytes
                    partition->clusterSize = 0x10000; // 64k clustersize
        }
        else if ((partition->partitionSize >= (256000000000ULL))){
                    // between 256GB and 512GB
                *(u_int64_t *)&partitionInfo[0x0008] = 0x00000040; // Clustersize in 512 bytes
                    partition->clusterSize = 0x8000; // 32k clustersize
        }
        else
              partition->clusterSize = 0x4000; // 16k clustersize
	}
	partition->clusterCount = partition->partitionSize / partition->clusterSize;
	partition->chainMapEntrySize = (partition->clusterCount >= 0xfff4) ? 4 : 2;
	
	if(nCreate) {
		memset(szBuffer,0x00,1024);
		for(i = 0; i < (partitionSize * 1024);i++) {
			lRet = fwrite(szBuffer, 1024, 1, partition->sourceFd);
		}
	}
	
	fseeko(partition->sourceFd,partition->partitionStart,SEEK_SET);
	
	*(u_int16_t *)&partitionInfo[0x000C] = 0x0001; //Number of active FATs (always 1) (?)

	// fwrite the header
	fwrite(partitionInfo, FATX_PARTITION_HEADERSIZE, 1, partition->sourceFd);
	
	partition->chainTableSize = partition->clusterCount * partition->chainMapEntrySize;
	if (partition->chainTableSize % FATX_CHAINTABLE_BLOCKSIZE) {
	// round up to nearest FATX_CHAINTABLE_BLOCKSIZE bytes
		partition->chainTableSize = ((partition->chainTableSize / FATX_CHAINTABLE_BLOCKSIZE) + 1) 
						* FATX_CHAINTABLE_BLOCKSIZE;
		
	}
	// Create empty chain map table
	partition->clusterChainMap.words = (u_int16_t*) malloc(2*sizeof(u_int64_t)); // (u_int16_t*) malloc(partition->chainTableSize);
	if (partition->clusterChainMap.words == NULL) {
		error("Out of memory here");
	}
	
	
	memset(partition->clusterChainMap.words,0x0000,2*sizeof(u_int64_t));
	//memset(partition->clusterChainMap.words,0x00,partition->chainTableSize);
		
	if (partition->chainMapEntrySize == 2) {
		rootFatMarker = 0xfff8;
		eocMarker = 0xffff;
		partition->clusterChainMap.words[0] = rootFatMarker;
		partition->clusterChainMap.words[1] = eocMarker;
	} else {
		rootFatMarker = 0xfffffff8;
		eocMarker = 0xffffffff;
		partition->clusterChainMap.dwords[0] = rootFatMarker;
		partition->clusterChainMap.dwords[1] = eocMarker;
	}
	
	// writing the new chaintable
	fwriteChainMap(partition);
	
	// Address of the first cluster
	partition->cluster1Address = partitionOffset + FATX_PARTITION_HEADERSIZE + partition->chainTableSize;
	fseeko(partition->sourceFd, partition->cluster1Address, SEEK_SET);
	
	// clearing the first cluster
	clusterData = (unsigned char *)malloc(partition->clusterSize);
	
	memset(clusterData,0xFF,partition->clusterSize);
	
	fwriteCluster(partition, 1, clusterData);
	// first direcory or file
	
	printf("createPartition : Filename : %s\n",szFileName);
	printf("createPartition : clusters       %ld\n",(unsigned long)partition->clusterCount);
	printf("createPartition : start          %llu\n",partition->partitionStart);
	printf("createPartition : size           %llu\n",partition->partitionSize);
	printf("createPartition : chainMapSize   %ld\n",(unsigned long)partition->chainMapEntrySize);
        printf("createPartition : clusterSize %lu\n",partition->clusterSize);
			
	return partition;
}

/**
 * Open a FATX partition
 *
 * @param sourceFd Source file
 * @param partitionOffset Offset into above file that partition starts at
 * @param partitionSize Size of partition in bytes
 */
FATXPartition* openPartition(FILE *sourceFd, 
                             u_int64_t partitionOffset,
                             u_int64_t partitionSize) {
  unsigned char partitionInfo[FATX_PARTITION_HEADERSIZE];
  size_t freadSize;
  FATXPartition* partition;
  u_int32_t chainTableSize = 0;

  // load the partition header
  fseeko(sourceFd, partitionOffset, SEEK_SET);
  freadSize = fread(partitionInfo, 1, FATX_PARTITION_HEADERSIZE, sourceFd);
#ifdef DEBUG
  printf("openPartition : %c%c%c%c FATX_PARTITION_HEADERSIZE %d freadSize %d \n",partitionInfo[0],partitionInfo[1],partitionInfo[2],partitionInfo[3],FATX_PARTITION_HEADERSIZE,freadSize);
#endif
  if (freadSize != FATX_PARTITION_HEADERSIZE) {
    error("Out of data while freading partition header");
  }

  // check the magic
  if (*((u_int32_t*) &partitionInfo) != FATX_PARTITION_MAGIC) {
    error("No FATX partition found at requested offset");
  }

  // make up new structure
  partition = (FATXPartition*) malloc(sizeof(FATXPartition));
  if (partition == NULL) {
    error("Out of memory");
  }

  // setup the easy bits
  partition->sourceFd = sourceFd;
  partition->partitionStart = partitionOffset;
  partition->partitionSize = partitionSize;
  partition->clusterSize = 0x4000;
  partition->clusterCount = partition->partitionSize / partition->clusterSize;
  partition->chainMapEntrySize = (partition->clusterCount >= 0xfff4) ? 4 : 2;
  
  // Now, work out the size of the cluster chain map table
  chainTableSize = partition->clusterCount * partition->chainMapEntrySize;
  if (chainTableSize % (unsigned long long)FATX_CHAINTABLE_BLOCKSIZE) {
    // round up to nearest FATX_CHAINTABLE_BLOCKSIZE bytes
    chainTableSize = ((chainTableSize / FATX_CHAINTABLE_BLOCKSIZE) + 1) * FATX_CHAINTABLE_BLOCKSIZE;
  }

  // Load the cluster chain map table
  partition->clusterChainMap.words = (u_int16_t*) malloc(chainTableSize);
  if (partition->clusterChainMap.words == NULL) {
    error("Out of memory");
  }
  fseeko(sourceFd, partitionOffset + FATX_PARTITION_HEADERSIZE, SEEK_SET);
  freadSize = fread(partition->clusterChainMap.words, 1, chainTableSize, sourceFd);
  if (freadSize != chainTableSize) {
    error("Out of data while freading cluster chain map table");
  }
  
  // Work out the address of cluster 1
  partition->cluster1Address = 
    partitionOffset + FATX_PARTITION_HEADERSIZE + chainTableSize;

  printf("openPartition : clusters	%d\n",partition->clusterCount);
  printf("openPartition : size		%lld\n",partition->partitionSize);
  printf("openPartition : chainMapSize	%d\n",partition->chainMapEntrySize);
  printf("openPartition : chainTableSize %d\n",chainTableSize);
		  
  // All done
  return partition;
}


/**
 * Close a FATX partition
 */
void closePartition(FATXPartition* partition) {
  free(partition->clusterChainMap.words);
  free(partition);
  partition = NULL;
}


/**
 * Dump a file to the supplied stream. If file is a directory, 
 * the directory listing will be dumped
 *
 * @param partition The FATX partition
 * @param outputStream Stream to output data to
 * @param filename Filename of file to dump
 */
void dumpFile(FATXPartition* partition, char* filename, FILE *outputStream) {
  int i = 0;
  
  // convert any '\' to '/' characters
  while(filename[i] != 0) {
    if (filename[i] == '\\') {
      filename[i] = '/';
    }
    i++;
  }

  // skip over any leading / characters
  i=0;
  while((filename[i] != 0) && (filename[i] == '/')) {
    i++;
  }
  
  // OK, start off the recursion at the root FAT
  _recurseToFile(partition, filename + i, outputStream, FATX_ROOT_FAT_CLUSTER);
}

/**
 * Dump entire directory tree to supplied stream
 *
 * @param partition The FATX partition
 * @param outputStream Stream to output data to
 */
void dumpTree(FATXPartition* partition, int outputStream) {
  // OK, start off the recursion at the root FAT
  _dumpTree(partition, outputStream, FATX_ROOT_FAT_CLUSTER, 0);
}


/**
 * Recursively dump the directory tree
 *
 * @param partition FATX Partition
 * @param outputStream Stream to output to
 * @param clusterId ID of cluster to start at
 * @param nesting Nesting level of directory tree
 */
void _dumpTree(FATXPartition* partition, 
              int outputStream,
              int clusterId, int nesting) {
  unsigned char clusterData[0x4000];
  int i;
  int j;
  int endOfDirectory;
  FATXDirEntry *dirEntry;
  char fwriteBuf[512];
  char flagsStr[5];

  
  // OK, output all the directory entries
  endOfDirectory = 0;
  while(clusterId != -1) {
    // load cluster data
    loadCluster(partition, clusterId, clusterData);

    // loop through it, outputing entries
    for(i=0; i< partition->clusterSize / sizeof(FATXDirEntry); i++) {
      // work out the currentEntry
      dirEntry = (FATXDirEntry *)&clusterData[i * sizeof(FATXDirEntry)];
      
      // first of all, check that it isn't an end of directory marker
      if(dirEntry->filenameSize == 0xFF) {
        endOfDirectory = 1;
        break;
      }

      // check if file is deleted
      if (dirEntry->filenameSize == 0xE5) {
        continue;
      }

      // extract the filename
      dirEntry->filename[dirEntry->filenameSize] = 0;

      // wipe fileSize
      if (dirEntry->attributes & FATX_FILEATTR_DIRECTORY) {
        dirEntry->fileSize = 0;
      }
      
      // zap flagsStr
      strcpy(flagsStr, "    ");

      // work out other flags
      if (dirEntry->attributes & FATX_FILEATTR_READONLY) {
        flagsStr[0] = 'R';
      }
      if (dirEntry->attributes & FATX_FILEATTR_HIDDEN) {
        flagsStr[1] = 'H';
      }
      if (dirEntry->attributes & FATX_FILEATTR_SYSTEM) {
        flagsStr[2] = 'S';
      }
      if (dirEntry->attributes & FATX_FILEATTR_ARCHIVE) {
        flagsStr[3] = 'A';
      }

      // Output it
      for(j=0; j< nesting; j++) {
        fwriteBuf[j] = ' ';
      }
      sprintf(fwriteBuf+nesting, "/%s  [%s] (SZ:%ld CL:%x)\n", 
              dirEntry->filename, flagsStr, (unsigned long)dirEntry->fileSize, dirEntry->firstCluster);
      write(outputStream, fwriteBuf, strlen(fwriteBuf));

      // If it is a sub-directory, recurse
      if (dirEntry->attributes & FATX_FILEATTR_DIRECTORY) {
        _dumpTree(partition, outputStream, dirEntry->firstCluster, nesting+1);
      }
    }

    // have we hit the end of the directory yet?
    if (endOfDirectory) {
      break;
    }
    
    // Find next cluster
    clusterId = getNextClusterInChain(partition, clusterId);
  }
}


/**
 * Recursively down a directory tree to find and extract a specific file
 *
 * @param partition FATX Partition
 * @param filename Filename to extract
 * @param outputStream Stream to output to
 * @param clusterId ID of cluster to start at
 */
void _recurseToFile(FATXPartition* partition, 
                    char* filename,
                    FILE *outputStream,
                    int clusterId) {
  unsigned char clusterData[0x4000];
  int i;
  int j;
  int endOfDirectory;
  char seekFilename[50];
  char* slashPos;
  int lookForDirectory = 0;
  int lookForFile = 0;
  FATXDirEntry *dirEntry;


  // work out the filename we're looking for
  slashPos = strchr(filename, '/');
  if (slashPos == NULL) {
    // looking for file
    lookForFile = 1;

    // check seek filename size
    if (strlen(filename) > FATX_FILENAME_MAX) {
      error("Bad filename supplied (one leafname is too big)");
    }
    
    // copy the filename to look for
    strcpy(seekFilename, filename);
  } else {
    // looking for directory
    lookForDirectory = 1;
    
    // check seek filename size
    if ((slashPos - filename) > FATX_FILENAME_MAX) {
      error("Bad filename supplied (one leafname is too big)");
    }

    // copy the filename to look for
    strncpy(seekFilename, filename, slashPos - filename);
    seekFilename[slashPos - filename] = 0;
  }

#ifdef DEBUG
  printf("_recurseToFile : Filename : %s\n",filename);
#endif

  // lowercase it
  for(i=0; i< strlen(seekFilename); i++) {
    seekFilename[i] = tolower(seekFilename[i]);
  }

  // OK, search through directory entries
  endOfDirectory = 0;
  while(clusterId != -1) {
    // load cluster data
    loadCluster(partition, clusterId, clusterData);

    // loop through it, outputing entries
    for(i=0; i< partition->clusterSize / FATX_DIRECTORYENTRY_SIZE; i++) {
      // work out the currentEntry
      dirEntry = (FATXDirEntry *)&clusterData[i * sizeof(FATXDirEntry)];

      // first of all, check that it isn't an end of directory marker
      if(dirEntry->filenameSize == 0xFF) {
        endOfDirectory = 1;
        break;
      }

      // check if file is deleted
      if (dirEntry->filenameSize == 0xE5) {
        continue;
      }

      dirEntry->filename[dirEntry->filenameSize] = 0;
      // lowercase foundfilename
      for(j=0; j< strlen(dirEntry->filename); j++) {
        dirEntry->filename[j] = tolower(dirEntry->filename[j]);
      }

      // is it what we're looking for...
      if (!strcmp(dirEntry->filename, seekFilename)) {
        // if we're looking for a directory and found a directory
        if (lookForDirectory) {
          if (dirEntry->attributes & FATX_FILEATTR_DIRECTORY) {
            _recurseToFile(partition, slashPos+1, outputStream, dirEntry->firstCluster);
            return;
          } else {
            error("File not found");
          }
        }

        // if we're looking for a file and found a file
        if (lookForFile) {
          if (!(dirEntry->attributes & FATX_FILEATTR_DIRECTORY)) {
#ifdef DEBUG
            printf("_recurseToFile : Cluster : %ld\n",(unsigned long)dirEntry->firstCluster);
#endif
            _dumpFile(partition, outputStream, dirEntry->firstCluster, dirEntry->fileSize);
            return;
          } else {
            error("File not found");
          }
        }
      }
    }

    // have we hit the end of the directory yet?
    if (endOfDirectory) {
      break;
    }
    
    // Find next cluster
    clusterId = getNextClusterInChain(partition, clusterId);
  }

  // not found it!
  error("File not found");
}



/** 
 * Dump a file to supplied outputStream
 *
 * @param partition FATX partition
 * @param outputStream Stream to output to
 * @param clusterId Starting cluster ID of file
 * @param fileSize Size of file
 */
void _dumpFile(FATXPartition* partition, FILE *outputStream, 
               int clusterId, u_int32_t fileSize) {
  unsigned char clusterData[partition->clusterSize];
  int writtenSize;

  // loop, outputting clusters
  while(clusterId != -1) {
    // Load the cluster data
    loadCluster(partition, clusterId, clusterData);
    
    // Now, output it
    writtenSize = 
      fwrite(clusterData, 1, (fileSize <= partition->clusterSize) ? fileSize : partition->clusterSize, outputStream);
    fileSize -=writtenSize;

    // Find next cluster
    clusterId = getNextClusterInChain(partition, clusterId);
  }

  // check we actually found enough data
  if (fileSize != 0) {
    error("Hit end of cluster chain before file size was zero");
  }
}



/**
 * Checks if the current entry is the last entry in a directory
 *
 * @param entry Raw entry data
 * @return 1 If it is the last entry, 0 if not
 */
int checkForLastDirectoryEntry(unsigned char* entry) {
  // if the filename length byte is 0 or 0xff,
  // this is the last entry
  if ((entry[0] == 0xff) || (entry[0] == 0)) {
    return 1;
  }

  // wasn't last entry
  return 0;
}


/**
 * Gets the next cluster in the cluster chain
 *
 * @param partition FATX partition
 * @param clusterId Cluster to find the netx cluster for
 * @return ID of the next cluster in the chain, or -1 if there is no next cluster
 */
u_int32_t getNextClusterInChain(FATXPartition* partition, int clusterId) {
  int nextClusterId = 0;
  u_int32_t eocMarker = 0;
  u_int32_t rootFatMarker = 0;
  u_int32_t maxCluster = 0;

  // check
  if (clusterId < 1) {
    error("Attempt to access invalid cluster: %i", clusterId);
  }

  // get the next ID
  if (partition->chainMapEntrySize == 2) {
    nextClusterId = partition->clusterChainMap.words[clusterId];
    eocMarker = 0xffff;
    rootFatMarker = 0xfff8;
    maxCluster = 0xfff4;
  } else if (partition->chainMapEntrySize == 4) {
    nextClusterId = partition->clusterChainMap.dwords[clusterId];
    eocMarker = 0xffffffff;
    rootFatMarker = 0xfffffff8;
    maxCluster = 0xfffffff4;
  } else {
    error("Unknown cluster chain map entry size: %i", partition->chainMapEntrySize);
  }
  
  // is it the end of chain?
  if ((nextClusterId == eocMarker) || (nextClusterId == rootFatMarker)) {
    return -1;
  }

  // is it something else unknown?
  if (nextClusterId == 0) {
    error("Cluster chain problem: Next cluster after %i is unallocated!", clusterId);
  }
  if (nextClusterId > maxCluster) {
    error("Cluster chain problem: Next cluster after %i has invalid value: %i", clusterId, nextClusterId);
  }
  
  // OK!
  return nextClusterId;
}


/**
 * Load data for a cluster
 *
 * @param partition FATX partition
 * @param clusterId ID of the cluster to load
 * @param clusterData Where to store the data (must be at least the cluster size)
 */
void loadCluster(FATXPartition* partition, unsigned long clusterId, unsigned char* clusterData) {
  u_int64_t clusterAddress;
  u_int64_t freadSize;
  int err;
  
  // work out the address of the cluster
  clusterAddress = partition->cluster1Address + ((unsigned long long)(clusterId - 1) * partition->clusterSize);
  
  printf("loadCluster : cluster1Address %lld clusterAddress %lld cluster %ld\n",
		  partition->cluster1Address, clusterAddress, clusterId);
  
  // Now, load it
  err = fseeko(partition->sourceFd, clusterAddress, SEEK_SET);
  
  if(err == -1) {
	  printf("loadCluster : Error in seeking to position %lld\n", clusterAddress);
  }
  
  freadSize = fread(clusterData, 1, partition->clusterSize, partition->sourceFd);
  if (freadSize != partition->clusterSize) {
    error("Out of data while freading cluster %i", clusterId);
  }
}

/**
 * fwrite data to a cluster
 *
 * @param partition FATX partition
 * @param clusterId ID of the cluster to fwrite
 * @param clusterData
 */
void fwriteCluster(FATXPartition* partition, int clusterId, unsigned char* clusterData) {
  u_int64_t clusterAddress;
  u_int64_t fwriteSize;
  
  // work out the address of the cluster
  clusterAddress = partition->cluster1Address + ((clusterId - 1) * partition->clusterSize);
  
  // Now, load it
  if((fseeko(partition->sourceFd, clusterAddress, SEEK_SET)) == -1) {
    printf("fwriteCluster : Error in setting position %lld\n",clusterAddress);
  }
  fwriteSize = fwrite(clusterData, 1, partition->clusterSize, partition->sourceFd);
  if (fwriteSize != partition->clusterSize) {
    error("Out of data while writing cluster %i", clusterId);
  }
}

