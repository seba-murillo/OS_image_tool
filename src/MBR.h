/*
 * MBR.h
 *
 *  Created on: Apr 8, 2020
 *      Author: seba
 */

#ifndef MBR_H_
#define MBR_H_

/*
 * codigo adaptado de:
 * 		https://stackoverflow.com/questions/12234051/printing-partition-table-c-program
 *
 * con info de:
 * 		https://en.wikipedia.org/wiki/Master_boot_record
 * 		https://en.wikipedia.org/wiki/Master_boot_record#PTE
 */

/*
pos		desc				size (B)
0		bootstrap code		446
446		part entry 1		16
462		part entry 2		16
478		part entry 3		16
494		part entry 4		16
510		boot signatue		2
*/

#define MBR_SIZE 512
#define BOOTSTRAP_CODE_SIZE 446
#define PARTITION_SIZE 16
#define SECTOR_SIZE 512 // ASUMO que es 512 -> podria hacer un fork() + exec()
// https://stackoverflow.com/questions/40068904/portable-way-to-determine-sector-size-in-linux

struct partition{
	unsigned char boot_flag;
	unsigned char CHS_begin[3];
	unsigned char type;
	unsigned char CHS_end[3];
	unsigned char sector_start[4];
	unsigned char sector_number[4];
};

void print_partition(const char* disk);
long hex2dec(void* string, int length);

#endif /* MBR_H_ */

