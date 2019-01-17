
// Author:           Jun Yu Ma


#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "mem.h"

/*
 * This structure serves as the header for each allocated and free block
 * It also serves as the footer for each free block
 */
typedef struct blk_hdr {                         
        int size_status;
} blk_hdr;

/* This will always point to the first block
 * (the block with the lowest address) */
blk_hdr *first_blk = NULL;

/* 
 * Function for allocating 'size' bytes
 * Returns address of allocated block on success 
 * Returns NULL on failure 
 */                    
void* Alloc_Mem(int size) {	
    if (size < 1){
	    return NULL;
    }
    void *ReturnAddress; 
    int Total;
    int BestFitSize = 0;
    blk_hdr *CurrentHeader = NULL;
    blk_hdr *BestAddress = NULL;
    Total = size + sizeof(blk_hdr);
    int Remain = Total % 8;
    if (Remain == 0){
    }
    else{
	    Total = Total + 8 - Remain;
    }
   // Current Header is first block
    CurrentHeader = first_blk;
    //start best-fit search
    while (CurrentHeader->size_status/8 != 0){
	    if ((CurrentHeader->size_status & 1) == 0){
		    if (CurrentHeader->size_status >= Total && (BestFitSize == 0 ||
					   CurrentHeader->size_status < BestFitSize)){
			    //the best block of memory so fars address is BestAddress
			    //and the size of BestAddress is BestFitSize
			    BestAddress = CurrentHeader;
			    BestFitSize = CurrentHeader->size_status;
			    //if the size is perfect we can break the loop
			    if (Total == CurrentHeader->size_status){
				   break;
			    } 
			    if (Total == 0){
				    break;
			    }
		    }
	    }
	   CurrentHeader = CurrentHeader + CurrentHeader->size_status/sizeof(blk_hdr);
    }
    //if no suitable address can be found return NULL
    if (BestAddress == NULL){
	   return NULL;
    }
    if (BestAddress != NULL){
	    if (BestFitSize == Total){
		    //if the size is perfect then there is no need to split
		   BestAddress->size_status = (BestAddress->size_status|3);
		   CurrentHeader = BestAddress + BestAddress->size_status/sizeof(blk_hdr);
		   if (CurrentHeader->size_status != 1){
	          	 CurrentHeader->size_status = CurrentHeader->size_status|2;
		   }	   
	    }
	    if (BestFitSize > Total){
		    //otherwise, split the memory
		    CurrentHeader = BestAddress + (Total/sizeof(blk_hdr));
		    CurrentHeader->size_status = (BestAddress->size_status - Total);
		    //create new header for new block after Total
		    CurrentHeader->size_status = (CurrentHeader->size_status|2);
		    CurrentHeader = CurrentHeader + CurrentHeader->size_status/sizeof(blk_hdr) - 1;
		    //create a footer
		    CurrentHeader->size_status = (BestAddress->size_status - Total) - sizeof(blk_hdr);
		    BestAddress->size_status = Total;
		    BestAddress->size_status = (BestAddress->size_status|3);
		    ReturnAddress = BestAddress + sizeof(blk_hdr)/4;
		    Dump_Mem();
	    }
    }
    return ReturnAddress;
}

/* 
 * Function for freeing up a previously allocated block 
 * Argument - ptr: Address of the block to be freed up 
 * Returns 0 on success 
 * Returns -1 on failure 
 */                    
int Free_Mem(void *ptr) {                       
    //if pointer is NULL return -1
    if (ptr == NULL){
	    return -1;
    }
    printf("First Check\n");
    //if pointer is not 8 byte aligned return -1
    if (((int)ptr)%8 != 0){
 	    return -1;  
    }
    printf("Second Check\n");
    blk_hdr *PrevB;
    blk_hdr *NextB;
    blk_hdr *Point;
    Point = ptr - sizeof(blk_hdr);
    NextB = Point + (Point->size_status/4);
    //if the block is already free return -1
    int x = 0;
    int previous = Point->size_status & 2;
    if ((Point->size_status & 1) == 0){
	    printf("third check\n");
	    return -1;
    }
    Point->size_status = Point->size_status - (Point->size_status%2);
    blk_hdr *Backup = Point;
    if ((NextB->size_status&1) == 0){
	    Point->size_status = Point->size_status + NextB->size_status - (NextB->size_status%8);
    }
    //coalesce prev block
    //if the SLSB is 0
    if (previous != 2){
	    //Previous block is free
	    NextB = Point - 1;
	    NextB -= NextB->size_status/4 -1;
	    NextB->size_status += Point->size_status - (Point->size_status%8);
	    Point = PrevB;
    }
    //coleasce next block
    x = Point->size_status;
    Point->size_status -= Point->size_status%2;
    NextB = Point + x/sizeof(blk_hdr) - 1;
    NextB->size_status = x;
    NextB = NextB + 1;
    if (NextB->size_status != 1 && (NextB->size_status&2) != 0){
        NextB->size_status -= 2;
    }
    return 0;
}

/*
 * Function used to initialize the memory allocator
 * Returns 0 on success and -1 on failure 
 */                    
int Init_Mem(int sizeOfRegion)
{                         
    int pagesize;
    int padsize;
    int fd;
    int alloc_size;
    void* space_ptr;
    blk_hdr* end_mark;
    static int allocated_once = 0;
  
    if (0 != allocated_once) {
        fprintf(stderr, 
        "Error:mem.c: Init_Mem has allocated space during a previous call\n");
        return -1;
    }
    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion 
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    alloc_size = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    space_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, 
                    fd, 0);
    if (MAP_FAILED == space_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }
  
     allocated_once = 1;

    // for double word alignement and end mark
    alloc_size -= 8;

    // To begin with there is only one big free block
    // initialize heap so that first block meets 
    // double word alignement requirement
    first_blk = (blk_hdr*) space_ptr + 1;
    end_mark = (blk_hdr*)((void*)first_blk + alloc_size);
  
    // Setting up the header
    first_blk->size_status = alloc_size;

    // Marking the previous block as busy
    first_blk->size_status += 2;

    // Setting up the end mark and marking it as busy
    end_mark->size_status = 1;

    // Setting up the footer
    blk_hdr *footer = (blk_hdr*) ((char*)first_blk + alloc_size - 4);
    footer->size_status = alloc_size;
  
    return 0;
}

/* 
 * Prints out a list of all the blocks
 */                     
void Dump_Mem() {                        
    int counter;
    char status[5];
    char p_status[5];
    char *t_begin = NULL;
    char *t_end = NULL;
    int t_size;

    blk_hdr *current = first_blk;
    counter = 1;

    int busy_size = 0;
    int free_size = 0;
    int is_busy = -1;

    fprintf(stdout, "************************************Block list***\
                    ********************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout, "-------------------------------------------------\
                    --------------------------------\n");
  
    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;
    
        if (t_size & 1) {
            // LSB = 1 => busy block
            strcpy(status, "Busy");
            is_busy = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "Free");
            is_busy = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "Busy");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "Free");
        }

        if (is_busy) 
            busy_size += t_size;
        else 
            free_size += t_size;

        t_end = t_begin + t_size - 1;
    
        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%d\n", counter, status, 
        p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);
    
        current = (blk_hdr*)((char*)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout, "---------------------------------------------------\
                    ------------------------------\n");
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fprintf(stdout, "Total busy size = %d\n", busy_size);
    fprintf(stdout, "Total free size = %d\n", free_size);
    fprintf(stdout, "Total size = %d\n", busy_size + free_size);
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fflush(stdout);

    return;
}
