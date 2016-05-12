#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include "include/fs.h"
//Block 0 = unused
//Block 1 = super
//Block 2 = start of Inodes

#define ROOTINO 1 //root of Inode
#define BSIZE 512 //block size
int DEBUG = 0;//used for whether or not to debug
/*
struct superblock{
	uint size;	//number of blocks in fs
	uint nblocks;
	uint ninodes;
};

#define NDIRECT (12)


struct dinode{
	short type;
	short major;	//device #
	short minor;
	short nlink;
	uint size;
	uint addrs[NDIRECT+1];	//Datablock addresses
};
*/


int
main(int argc, char *argv[])
{

	
	int fd = open(argv[1], O_RDONLY);
	if(fd <= -1){
		fprintf(stderr, "image not found.\n"); //test
		return 1;
	}

	int rc;
	struct stat sbuf;
	rc = fstat(fd, &sbuf);
	assert(rc == 0);

	void *img_ptr = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	assert(img_ptr != MAP_FAILED);

	struct superblock *sb;
	sb = (struct superblock *)(img_ptr + BSIZE);
	if(DEBUG)
		printf("%d %d %d\n", sb->size, sb->nblocks, sb->ninodes);//TODO debug purposes only


	//int range = (int)((sb->size) * BSIZE);//should be the max value of valid addresses
	//printf("%d\n", range);

	//inodes
	int i;
	struct dinode *dip = (struct dinode*)(img_ptr + (2*BSIZE));



	int bitmap[sb->nblocks];
	int inode_used[sb->nblocks];
	int block_addr, num;

	for(i = 0; i <= BBLOCK(sb->nblocks, sb->ninodes); i++)
		inode_used[i] = 1; 

	for(i = 0; i< sb->ninodes; i++){
		short type = dip->type;
		//CHECK:valid inodes
		//stat.h => type defs > T_DIR (directory) = 1
		//			T_FILE (file) = 2
		//			T_DEV (special device) = 3
		//note: 0 => unallocated
		if(type == 0 || type == 1 || type == 2 || type == 3){
			if(DEBUG)
				printf("%d type: %d\n", i, type);//TODO debug purposes only
			if(type != 0){
				
				inode_used[IBLOCK(i)] = 1;
				int dev = (dip->size)/BSIZE;
				int mod = (dip->size)%BSIZE;
				if(DEBUG)
					printf("%d.%d\n", dev, mod);//can use this to get the total number of blocks in use
				if(DEBUG)
					printf("number of links = %d\nsize = %d\naddresses = ", dip->nlink, dip->size);
				
				int j;
				for(j = 0; j < (NDIRECT+1); j++){
					block_addr = dip->addrs[j];
					if(block_addr != 0){
						if(inode_used[block_addr] == 1){
							fprintf(stderr, "ERROR: address used more than once.\n"); //test
							return 1;
						}
						inode_used[block_addr] = 1;
					}
					if(DEBUG)
						printf("%d  ", dip->addrs[j]);
				}
				if(DEBUG)
					printf("\n");
				if(dev > NDIRECT){
					if(DEBUG)
						printf("EXTRA BLOCK\n");					
					
					int *start_addr = (int*)(img_ptr + (block_addr*BSIZE));
					for(j = 0; j < BSIZE/4; j++){
						num = *(start_addr + j);
						if(num != 0){
							if(inode_used[num] == 1){
								fprintf(stderr, "ERROR: address used more than once.\n");
								return 1;
							}
							inode_used[num] = 1;
							if(DEBUG)
								printf("%d ", num);
						}
					}

					
				}
				if(DEBUG)
					printf("\n\n");
			}
			dip++;
		}else{
			fprintf(stderr,"ERROR: bad inode.\n");
			return 1;
		}

		//CHECK:root directory
		if(i == ROOTINO && type != 1){
			fprintf(stderr,"ERROR: root directory does not exist.\n");
			return 1;
		}
		
	}

	
	
	int bitmap_block, j, n_block;
	uint *dbmp_addr;
	bitmap_block = BBLOCK(0, sb->ninodes);
	//printf("\n\nbitmap block: %d\n\n", bitmap_block);
	dbmp_addr = (uint*)(img_ptr + (bitmap_block*BSIZE));
	
	uint word, bit, mask, word_mask;
	/*
	for(i = 0; i < (sb->nblocks)/32; i++){
		
		word = *(dbmp_addr+i);
		
		word = word & (0x0000000F);

		printf("%d - %d data bitmap: %x\n", (i*32), ((i+1)*32)-1, word);
		
		//if(bit == 1){

		//}
			
		
	}
	*/
	

	//dis the way I do the bitmap thing! You can replace it if you want, dylan.
	for(i = 0; i < ((sb->nblocks)/32+1); i++){
		word = *(dbmp_addr+i);
		if(i <31){
			for(j = 0; j < 32; j++){
				n_block = (i*32)+(j);
				if(DEBUG)
					printf("%d ",n_block);
				mask = (0x1 << j);
				if(DEBUG)
					printf("mask: %x ",mask);
				if(DEBUG)
					printf("word: %x ",word);
				word_mask = word & mask;
				if(DEBUG)
					printf("masked word: %x ",word_mask);
				bit = word_mask >> j;
				if(DEBUG) 
					printf("data bitmap: %x\n", bit);
		
				if(bit == 1){
					bitmap[n_block] = 1;

				}
			
			}
		}
		else {
			for(j = 0; j < (sb->nblocks % 32); j++){
				n_block = (i*32)+(j);
				if(DEBUG)
					printf("%d ",n_block);
				mask = (0x1 << j);
				if(DEBUG)
					printf("mask: %x ",mask);
				word = *(dbmp_addr+i);
				if(DEBUG)
					printf("word: %x ",word);
				word = word & mask;
				if(DEBUG)
					printf("masked word: %x ",word);
				bit = word >> j;
				if(DEBUG)
					printf("data bitmap: %x\n", bit);
		
				if(bit == 1){
					bitmap[n_block] = 1;

				}
			
			}

		}
	}
	/*
	for(i = 0; i < (sb->nblocks); i++){
		
		if((bitmap[i] == 0) & (inode_used[i] == 1)){
			fprintf(stderr,"address used by inode but marked free in bitmap.\n");
			return 1;
		}
		if((bitmap[i] == 1) & (inode_used[i] == 0)){
			fprintf(stderr,"bitmap marks block in use but it is not in use.\n");
			return 1;
		}
			
		
	}
	*/	

	//bitmap

	//other
	if(DEBUG)
		printf("0\n");
	return 0;
}
