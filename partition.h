#include <stdlib.h>
#include <ctype.h>


#define IOCTL_CMD_LBA48_ACCESS 0xcafebabe
#define IOCTL_SUBCMD_GET_INFO 0

#define LBA48_GET_INFO_MAGIC1_IDX 0
#define LBA48_GET_INFO_MAGIC1_VAL 0xcafebabe
#define LBA48_GET_INFO_MAGIC2_IDX 1
#define LBA48_GET_INFO_MAGIC2_VAL 0xbabeface
#define LBA48_GET_INFO_PATCHCODE_VERSION_IDX 2
#define LBA48_GET_INFO_LOWCODE_BASE_IDX  3
#define LBA48_GET_INFO_HIGHCODE_BASE_IDX 4
#define LBA48_GET_INFO_PATCHSEG_SIZE_IDX 5
#define LBA48_GET_INFO_PART_TABLE_OFS_IDX 6

#define XBOX_SWAPPART1_LBA_START		(0x00000400UL)
#define XBOX_SWAPPART_LBA_SIZE			(0x00177000UL)
#define XBOX_SWAPPART2_LBA_START		(XBOX_SWAPPART1_LBA_START + XBOX_SWAPPART_LBA_SIZE)
#define XBOX_SWAPPART3_LBA_START		(XBOX_SWAPPART2_LBA_START + XBOX_SWAPPART_LBA_SIZE)

#define XBOX_SYSPART_LBA_START			(XBOX_SWAPPART3_LBA_START + XBOX_SWAPPART_LBA_SIZE)
#define XBOX_SYSPART_LBA_SIZE			(0x000fa000UL)

#define XBOX_MUSICPART_LBA_START		(XBOX_SYSPART_LBA_START + XBOX_SYSPART_LBA_SIZE)
#define XBOX_MUSICPART_LBA_SIZE			(0x009896b0UL)

#define XBOX_PART6_LBA_START			(XBOX_MUSICPART_LBA_START + XBOX_MUSICPART_LBA_SIZE)


#define XBOX_STANDARD_MAX_LBA			(XBOX_MUSICPART_LBA_START + XBOX_MUSICPART_LBA_SIZE)

#define PE_PARTFLAGS_IN_USE	0x80000000

#define CLUSTER_SIZE 0x4000
#define SECTORS_PER_CLUSTER 32
#define SECTOR_SIZE 512

typedef int bool;
typedef u_int32_t ULONG;
typedef u_int32_t USHORT;

#define false 0
#define true 1

#define PARTITION_MODE_F_CAPPED 0
#define PARTITION_MODE_F_ALL 1
#define PARTITION_MODE_G_REST 2
#define PARTITION_MODE_FG_EVEN 3

#define LBASIZE_512GB   1000000000
#define LBASIZE_256GB    500000000
#define LBASIZE_MAX     4294967295

#define LBASIZE_320GB		  625000000
#define LBASIZE_ONE_TERA	 1953125000
#define LBASIZE_TWO_TERA	 3906250000
#define LBASIZE_OT_EXTENDED	 1937591928
#define LBASIZE_OFT_EXTENDED	 2914054428
#define LBASIZE_TT_EXTENDED	 3890616928

typedef struct
{
	unsigned char Name[16];
	u_int32_t Flags;
	u_int32_t LBAStart;
	u_int32_t LBASize;
	u_int32_t Reserved;
} XboxPartitionTableEntry;


typedef struct
{
	unsigned char	Magic[16];
	char	Res0[32];
	XboxPartitionTableEntry	TableEntries[14];
} XboxPartitionTable;

typedef struct _FATX_SUPERBLOCK
{
	char	Tag[4];
	unsigned int		VolumeID;
	unsigned int	ClusterSize;
	USHORT	FatCopies;
	int		Resvd;
	char	Unused[4078];
} FATX_SUPERBLOCK;


void part_add_size(XboxPartitionTable *PartTbl, int PartNumber, ULONG TotalSectors, ULONG addition);
void part_sub_size(XboxPartitionTable *PartTbl, int PartNumber, ULONG TotalSectors, ULONG subtraction);
void part_up(XboxPartitionTable *PartTbl, int PartNumber, ULONG TotalSectors, ULONG addition);
void part_down(XboxPartitionTable *PartTbl, int PartNumber, ULONG TotalSectors, ULONG subtraction);
void part_even(XboxPartitionTable *PartTbl, ULONG TotalSectors);
void part_change_size(XboxPartitionTable *PartTbl, int PartitionNum, ULONG TotalSectors, int TrigValue, int change_start, int Direction, bool);
void part_init_unused(XboxPartitionTable *PartTbl);
void part_disable(XboxPartitionTable *PartTbl, int PartNumber);
bool part_enable(XboxPartitionTable *PartTbl, ULONG TotalSectors, int PartNumber);
int part_get_last_available(XboxPartitionTable *PartTbl);
ULONG part_get_free_sectors(XboxPartitionTable *PartTbl, ULONG TotalSectors);
ULONG part_get_used_sectors(XboxPartitionTable *PartTbl);
void part_setup_standard_partitions(XboxPartitionTable *PartTbl, XboxPartitionTable *BackupPartTbl, ULONG TotalSectors, int Type,u_int64_t TB_TotalSectors);
ULONG part_get_ceil(XboxPartitionTable *PartTbl, int PartNumber, ULONG TotalSectors);
ULONG safeadd (ULONG a,ULONG b);
ULONG safesubstract(u_int64_t a,ULONG b);
void part_setup_desiredsize_partitions(XboxPartitionTable *PartTbl, XboxPartitionTable *BackupPartTbl, ULONG TotalSectors,int FdesiredPercent,u_int64_t tb_totalsectors);
