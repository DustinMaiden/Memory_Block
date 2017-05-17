////////////////////////////////////////////////////////////
//  Main File : 	mem.c
//  Semester : 		CS 354 Fall 2016
//   
//  Author : 		Dustin Maiden
//  Email :		dmaiden@wisc.edu
//  CS Login : 		maiden
/////////////////////////////////////////////////////////////
//  Disclosures : I took CS 536 - Intro to Programming
//                Languages and Compilers with Fischer 
//                in Spring 2015 and CS 564 - Database 
//                Managements Systems in Fall 2014.
//                A few concepts are somewhat familiar.
//
//                Beyond that, my biggest struggle was casting 
//		  which were answered in Piazza
/////////////////////////////////////////////////////////////
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "mem.h"

/* this structure serves as the header for each block */
typedef struct block_hd{
	/* The blocks are maintained as a linked list */
	/* The blocks are ordered in the increasing order of addresses */
	struct block_hd* next;

	/* The size_status field is the size of the payload+padding and is always a multiple of 4 */
	/* ie, last two bits are always zero - can be used to store other information*/
	/* LSB = 0 => free block */
	/* LSB = 1 => allocated/busy block */

	/* So for a free block, the value stored in size_status will be the same as the block size*/
	/* And for an allocated block, the value stored in size_status will be one more than the block size*/

	/* The value stored here does not include the space required to store the header */

	/* Example: */
	/* For a block with a payload of 24 bytes (ie, 24 bytes data + an additional 8 bytes for header) */
	/* If the block is allocated, size_status should be set to 25, not 24!, not 23! not 32! not 33!, not 31! */
	/* If the block is free, size_status should be set to 24, not 25!, not 23! not 32! not 33!, not 31! */
	int size_status;

}block_header;

/* Global variable - This will always point to the first block */
/* ie, the block with the lowest address */
block_header* list_head = NULL;


/* Function used to Initialize the memory allocator */
/* Not intended to be called more than once by a program */
/* Argument - sizeOfRegion: Specifies the size of the chunk which needs to be allocated */
/* Returns 0 on success and -1 on failure */
int Mem_Init(int sizeOfRegion)
{
	int pagesize;
	int padsize;
	int fd;
	int alloc_size;
	void* space_ptr;
	static int allocated_once = 0;

	if(0 != allocated_once)
	{
		fprintf(stderr,"Error:mem.c: Mem_Init has allocated space during a previous call\n");
		return -1;
	}
	if(sizeOfRegion <= 0)
	{
		fprintf(stderr,"Error:mem.c: Requested block size is not positive\n");
		return -1;
	}

	/* Get the pagesize */
	pagesize = getpagesize();

	/* Calculate padsize as the padding required to round up sizeOfRegio to a multiple of pagesize */
	padsize = sizeOfRegion % pagesize;
	padsize = (pagesize - padsize) % pagesize;

	alloc_size = sizeOfRegion + padsize;

	/* Using mmap to allocate memory */
	fd = open("/dev/zero", O_RDWR);
	if(-1 == fd)
	{
		fprintf(stderr,"Error:mem.c: Cannot open /dev/zero\n");
		return -1;
	}
	space_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (MAP_FAILED == space_ptr)
	{
		fprintf(stderr,"Error:mem.c: mmap cannot allocate space\n");
		allocated_once = 0;
		return -1;
	}

	allocated_once = 1;

	/* To begin with, there is only one big, free block */
	list_head = (block_header*)space_ptr;
	list_head->next = NULL;
	/* Remember that the 'size' stored in block size excludes the space for the header */
	list_head->size_status = alloc_size - (int)sizeof(block_header);

	return 0;
}


/* Function for allocating 'size' bytes. */
/* Returns address of allocated block on success */
/* Returns NULL on failure */
/* Here is what this function should accomplish */
/* - Check for sanity of size - Return NULL when appropriate */
/* - Round up size to a multiple of 4 */
/* - Traverse the list of blocks and allocate the best free block which can accommodate the requested size */
/* -- Also, when allocating a block - split it into two blocks when possible */
/* Tips: Be careful with pointer arithmetic */
void* Mem_Alloc(int size)
{
  //error handling for bad requests
  if(size == 0){
	return NULL;
  }
  if(size < 0){
	return NULL;
  }

  //Check size and round it up to a multiple of 4
  int size_request = 0;  	
  if(size % 4 == 0){
	size_request = size;
  }
  else{
	//rounding up by multiple of 4
	size_request = size + 4 - (size % 4);
  }
  
  //block_headers to be used
  block_header* this_block; // current block 
  this_block = list_head;   // setting to global head
  
  //best fit block_header
  block_header* best_block; // best block
  best_block = NULL;        // setting to NULL
  int leftover = 99999999; // leftover space to calculate best block
 
  //Search for the best fit block in the entre free list
  while(this_block != NULL){
    
    //if free, the last bit will be a 0  	
    if((this_block->size_status % 2) == 0){
       	
	//if the block is big enough to handle the request
        //this will calculate how much space is leftover
	if(this_block->size_status >= size_request){
		
	   //calculate leftover space to identify best fit
	   if(leftover > (this_block->size_status - size_request)){
		leftover = this_block->size_status - size_request;
		best_block = this_block;//this is best fit for now
	   }	
	}
    }
    this_block = this_block->next; //iterate
  }

  //if block was found, means there is no enough space
  if(best_block == NULL){
	//printf("fail");
	return NULL;
  }

  //splitting blocks - if enough space is available  
  if(best_block->size_status > (size_request + (int)sizeof(block_header))){
 	
  //the next block        
  block_header* next_block = best_block->next; 
  int cur_size = best_block->size_status; 
         
  //best block points to the new block 
  best_block->next = (block_header*) ((char*)best_block + size_request + (int)sizeof(block_header)); 
  
  //insert blcok
  best_block->next->next = next_block;
          
  //calculate size
  best_block->next->size_status = cur_size - size_request - (int)sizeof(block_header); 
  best_block->size_status = size_request +  1; //allocated
  
  return (void*)((char*)best_block + (int)sizeof(block_header));
  }

  //if not enough space to split, then return the block + header
  else{ 
  best_block->size_status += 1; // allocated
  return ((void*)((char*)best_block + (int)sizeof(block_header))); 
  }

  return NULL; //shouldn't hit this, but the program needs it to compile
}


/* Function for freeing up a previously allocated block */
/* Argument - ptr: Address of the block to be freed up */
/* Returns 0 on success */
/* Returns -1 on failure */
/* Here is what this function should accomplish */
/* - Return -1 if ptr is NULL */
/* - Return -1 if ptr is not pointing to the first byte of a busy block */
/* - Mark the block as free */
/* - Coalesce if one or both of the immediate neighbours are free */
int Mem_Free(void *ptr)
{
 
  block_header* this_block; //block_header for ptr
  block_header* last_block; //block_header for previous
 
 //initailize block_headers
  this_block = list_head; 
  last_block = list_head;
  
  //error checking
  if(ptr == NULL)
  {
    //printf("empty");
    return -1;
  }
  
  // Check if the pointer is pointing to the start of the payload of an allocated block
  while(this_block != NULL)  
  {
    //the request must be in the block list
    if( (void*)((char*)this_block + (int)sizeof(block_header)) == ptr) 
    {
    //once found, break out
    //printf("found");  
    break;
    } 
    
    last_block = this_block;
    this_block = this_block->next;
  } 
 
  //if already free
  if((this_block->size_status % 2) == 0) 
  {
    //printf("allocated");
    return -1;
  }
  
  //free it
  this_block->size_status -= 1; //de-allocate 
  
  // Check the blocks to the left and right to see if the block can be coalesced
  // with either or both of them

  //next coalescing
  //if there is next block available AND it's free
  if((this_block->next !=NULL) && (this_block->next->size_status % 2) == 0){

      //combine and next
      this_block->size_status = this_block->size_status + this_block->next->size_status + (int)sizeof(block_header);
        this_block->next = this_block->next->next;  
   }

  //previous coalesing
  if((last_block != this_block) && (last_block->size_status % 2) == 0){ 

      // comgine and next 
      last_block->size_status = last_block->size_status + this_block->size_status + (int)sizeof(block_header);
      last_block->next = this_block->next;
  }
   
  return 0;  
}

/* Function to be used for debug */
/* Prints out a list of all the blocks along with the following information for each block */
/* No.      : Serial number of the block */
/* Status   : free/busy */
/* Begin    : Address of the first useful byte in the block */
/* End      : Address of the last byte in the block */
/* Size     : Size of the block (excluding the header) */
/* t_Size   : Size of the block (including the header) */
/* t_Begin  : Address of the first byte in the block (this is where the header starts) */
void Mem_Dump()
{
	int counter;
	block_header* current = NULL;
	char* t_Begin = NULL;
	char* Begin = NULL;
	int Size;
	int t_Size;
	char* End = NULL;
	int free_size;
	int busy_size;
	int total_size;
	char status[5];

	free_size = 0;
	busy_size = 0;
	total_size = 0;
	current = list_head;
	counter = 1;
	fprintf(stdout,"************************************Block list***********************************\n");
	fprintf(stdout,"No.\tStatus\tBegin\t\tEnd\t\tSize\tt_Size\tt_Begin\n");
	fprintf(stdout,"---------------------------------------------------------------------------------\n");
	while(NULL != current)
	{
		t_Begin = (char*)current;
		Begin = t_Begin + (int)sizeof(block_header);
		Size = current->size_status;
		strcpy(status,"Free");
		if(Size & 1) /*LSB = 1 => busy block*/
		{
			strcpy(status,"Busy");
			Size = Size - 1; /*Minus one for ignoring status in busy block*/
			t_Size = Size + (int)sizeof(block_header);
			busy_size = busy_size + t_Size;
		}
		else
		{
			t_Size = Size + (int)sizeof(block_header);
			free_size = free_size + t_Size;
		}
		End = Begin + Size;
		fprintf(stdout,"%d\t%s\t0x%08lx\t0x%08lx\t%d\t%d\t0x%08lx\n",counter,status,(unsigned long int)Begin,(unsigned long int)End,Size,t_Size,(unsigned long int)t_Begin);
		total_size = total_size + t_Size;
		current = current->next;
		counter = counter + 1;
	}
	fprintf(stdout,"---------------------------------------------------------------------------------\n");
	fprintf(stdout,"*********************************************************************************\n");

	fprintf(stdout,"Total busy size = %d\n",busy_size);
	fprintf(stdout,"Total free size = %d\n",free_size);
	fprintf(stdout,"Total size = %d\n",busy_size+free_size);
	fprintf(stdout,"*********************************************************************************\n");
	fflush(stdout);
	return;
}
