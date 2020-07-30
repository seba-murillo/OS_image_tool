/*
 * MBR.c
 *
 *  Created on: Apr 8, 2020
 *      Author: seba
 */

/*
 * codigo adaptado de:
 * 		https://stackoverflow.com/questions/12234051/printing-partition-table-c-program
 *
 * con info de:
 * 		https://en.wikipedia.org/wiki/Master_boot_record
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <MBR.h>

#define TAB "    "

/// convert a hexadecimal string into a decimal int
/// @param string the string containig the hexadecimal number(s)
/// @param length the digit length of the number
/// @returns the decimal int value of the number
long hex2dec(void* string, int length){
	char tmp[10];
	char hex[10];
	sprintf(hex, " ");
	for(int i = length - 1;i >= 0;i--){
		sprintf(tmp, "%02x", ((char*) string)[i] & 0xFF);
		strcat(hex, tmp);
	}
	return strtol(hex, NULL, 16);
}

/// prints the MBR information of the specified partition
/// @param disk the disk to read the MBR information from
void print_partition(const char* disk){
	if(strncmp(disk, "/dev/", 5) != 0){
		printf("> no partition table available for files\n");
		return;
	}
	struct partition* partition;
	int FD = open(disk, O_RDONLY | O_SYNC);

	if(FD < 0){
		fprintf(stderr, "ERROR: opening %s (%s)\n", disk, strerror(errno));
		exit(EXIT_FAILURE);
	}
	char buffer[MBR_SIZE];
	ssize_t read_bytes = read(FD, buffer, MBR_SIZE);
	if(read_bytes < 0){
		fprintf(stderr, "ERROR: reading %s (%s)\n", disk, strerror(errno));
		exit(EXIT_FAILURE);
	}
	if(close(FD) < 0){
		fprintf(stderr, "ERROR: closing %s (%s)\n", disk, strerror(errno));
		exit(EXIT_FAILURE);
	}
	// dump partition -> comparar output del dump con 'hexdump -n 512 -C /dev/sda'
	//printf("[MBR]: hex dump of %s (%ld bytes)\n\n", disk, read_bytes);
	//print_hex(buffer, MBR_SIZE); // dumps hex
	// print info
	printf("> partition table for %s:\n", disk);
	partition = (struct partition*) (buffer + BOOTSTRAP_CODE_SIZE);
	printf(TAB "%-15s%-10s%-20s%-20s%-10s\n", "name", "type", "size (bytes)", "first sector", "bootable");
	char boot[5];
	char name[20];
	for(int i = 0;i < 4;i++){
		long size = hex2dec(partition->sector_number, 4) * SECTOR_SIZE;
		partition = (struct partition*) (buffer + BOOTSTRAP_CODE_SIZE + (i * PARTITION_SIZE));
		if(size == 0) continue; // partition doesn't exist
		if(partition->boot_flag == 0x80) sprintf(boot, "yes");
		else sprintf(boot, "no");
		sprintf(name, "%s%d", disk, i + 1);
		printf(TAB "%-15s%-10lu%-20lu%-20lu%-10s\n", name, hex2dec(&partition->type, 1), size, hex2dec(partition->sector_start, 4), boot);
	}
	printf("\n");
}
/*
 * PARTITION TYPES
 0  Empty           24  NEC DOS         81  Minix / old Lin bf  Solaris
 1  FAT12           27  Hidden NTFS Win 82  Linux swap / So c1  DRDOS/sec (FAT-
 2  XENIX root      39  Plan 9          83  Linux           c4  DRDOS/sec (FAT-
 3  XENIX usr       3c  PartitionMagic  84  OS/2 hidden C:  c6  DRDOS/sec (FAT-
 4  FAT16 <32M      40  Venix 80286     85  Linux extended  c7  Syrinx
 5  Extended        41  PPC PReP Boot   86  NTFS volume set da  Non-FS data
 6  FAT16           42  SFS             87  NTFS volume set db  CP/M / CTOS / .
 7  HPFS/NTFS/exFAT 4d  QNX4.x          88  Linux plaintext de  Dell Utility
 8  AIX             4e  QNX4.x 2nd part 8e  Linux LVM       df  BootIt
 9  AIX bootable    4f  QNX4.x 3rd part 93  Amoeba          e1  DOS access
 a  OS/2 Boot Manag 50  OnTrack DM      94  Amoeba BBT      e3  DOS R/O
 b  W95 FAT32       51  OnTrack DM6 Aux 9f  BSD/OS          e4  SpeedStor
 c  W95 FAT32 (LBA) 52  CP/M            a0  IBM Thinkpad hi eb  BeOS fs
 e  W95 FAT16 (LBA) 53  OnTrack DM6 Aux a5  FreeBSD         ee  GPT
 f  W95 Ext'd (LBA) 54  OnTrackDM6      a6  OpenBSD         ef  EFI (FAT-12/16/
10  OPUS            55  EZ-Drive        a7  NeXTSTEP        f0  Linux/PA-RISC b
11  Hidden FAT12    56  Golden Bow      a8  Darwin UFS      f1  SpeedStor
12  Compaq diagnost 5c  Priam Edisk     a9  NetBSD          f4  SpeedStor
14  Hidden FAT16 <3 61  SpeedStor       ab  Darwin boot     f2  DOS secondary
16  Hidden FAT16    63  GNU HURD or Sys af  HFS / HFS+      fb  VMware VMFS
17  Hidden HPFS/NTF 64  Novell Netware  b7  BSDI fs         fc  VMware VMKCORE
18  AST SmartSleep  65  Novell Netware  b8  BSDI swap       fd  Linux raid auto
1b  Hidden W95 FAT3 70  DiskSecure Mult bb  Boot Wizard hid fe  LANstep
1c  Hidden W95 FAT3 75  PC/IX           be  Solaris boot    ff  BBT
1e  Hidden W95 FAT1 80  Old Minix

*/
