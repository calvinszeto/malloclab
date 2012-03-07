/*
 * mm.c
 * 
 * Dynamic memory allocator implemented using a segregated free list and
 * explicit linked lists. The beginning of the heap is reserved for a
 * segregated free list where each node points to an explicit free list
 * of blocks with maximum 2 ** (box number + 4). The first box is empty.
 * Box 15 holds anything larger than 262144 bytes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
	/* Team name */
	"ateam",
	/* First member's full name */
	"Calvin Szeto",
	/* First member's email address */
	"szeto.calvin@gmail.com",
	/* Second member's full name (leave blank if none) */
	"Matthew Granado",
	/* Second member's email address (leave blank if none) */
	"mattg@mail.utexas.edu"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4 //word size
#define DSIZE 8 //double word size
#define CHUNKSIZE (1<<12) //extend the heap 2**10 words at a time

#define MAX(x,y) ((x) > (y)? (x) : (y)) //max of x and y

//size is a multiple of 8 so last three bits are available for alloc status
#define PACK(size,alloc) ((size)|(alloc))

//dereferences p, must cast first since p is type void *
#define GET(p) (*(unsigned int *)(p))
#define PUT(p,val) (*(unsigned int *)(p)=(val))

//gets size or alloc status from a pointer
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

//given a block pointer, returns header or footer, could change if footer size changes
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

//given block pointer, gives next or previous block pointer
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

static char *free_listp;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	int i;
	//Push up break pointer by 20 words
	if((free_listp = mem_sbrk(20*WSIZE)) == (void *)-1)
		return -1;
	PUT(free_listp,0);//padding word
	//Initialize free list
	free_listp += WSIZE;
	for(i=0;i<16;i++)
		PUT(free_listp+(i*WSIZE),0);
	PUT(free_listp+(16*WSIZE),PACK(DSIZE,1));//header
	PUT(free_listp+(17*WSIZE),PACK(DSIZE,1));//footer
	PUT(free_listp+(18*WSIZE),PACK(0,1));//epilogue block

	char *bp;
	if ((bp=extend_heap(CHUNKSIZE/WSIZE)) == NULL)//expand the heap
		return -1;
	add_to_free(bp);
	return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
	size_t asize;//Allocate size
	size_t extendsize;
	char *bp;

	if(size==0)
		return NULL;

	if(size<=DSIZE)
		asize=2*DSIZE;
	else
		//Add overhead and round to nearest multiple of DSIZE
		asize=DSIZE*((size+(DSIZE)+(DSIZE-1))/DSIZE);

	if((bp=find_fit(asize))!=NULL) {
		place(bp,asize);
		return bp;
	}
	
	//Extend the heap if no free block is large enough
	extendsize=MAX(asize,CHUNKSIZE);
	if((bp=extend_heap(extendsize/WSIZE))==NULL)
		return NULL;
	place(bp,asize);
	return bp;
}

/*
 * mm_free - Freeing a block does something.
 */
void mm_free(void *ptr)
{
	size_t size = GET_SIZE(HDRP(ptr));
	//Basically change alloc bit to 0
	PUT(HDRP(ptr),PACK(size,0));
	PUT(FTRP(ptr),PACK(size,0));
	add_to_free(coalesce(ptr));
}

/*
 * find_box - takes a size and returns which box it belongs to
 */
int find_box(size_t size) {
	//Round size to next multiple of 8 and find the highest box b where
	//(2**b*8)+8<=aligned size
	int asize;
	int box = 0;
	if(size<16)
		return -1;
	else if(size==16)
		return 1;
	asize=(ALIGN(size)-8)/8;
	while((asize = asize >> 1))
		box += 1;
	return ((box > 14) ? 15 : box);
}

/*
 * add_to_free - Adds a block to the free list.
 */
void *add_to_free(void *bp)
{
	size_t size = GET_SIZE(HDRP(bp));
	int box = find_box(size);
	size_t nextbp;
	
	nextbp=GET(free_listp+(box*WSIZE));
	PUT(bp,nextbp);
	PUT(free_listp+(box*WSIZE),((size_t)bp));
	return bp;
}

/*
 * remove_from_free - Removes a block from the free list.
 */
void remove_from_free(void *bp)
{
	size_t next = GET(bp);
	char *pbp = free_listp+(find_box(GET_SIZE(HDRP(bp)))*WSIZE);
	//Assert that remove_from_free is only called on a pointer
	//which exists in the free list.
	while(GET(pbp)!=(size_t)(bp)) {
		pbp=(void *)GET(pbp);
		assert(GET(pbp)!=0);
	}
	PUT(pbp,next);
}

void *extend_heap(size_t words)
{
	char *bp;
	size_t size;
	//allocate some multiple of DSIZE 
	size=(words % 2) ? (words+1) * WSIZE : words * WSIZE;
	if((long)(bp=mem_sbrk(size)) == -1)
		return NULL;

	//Add free block to heap
	PUT(HDRP(bp),PACK(size,0));
	PUT(FTRP(bp),PACK(size,0));
	//Add epilogue block
	PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));
	bp=coalesce(bp);

	return bp;
}

void *coalesce(void *bp)
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	if (prev_alloc && next_alloc) {
		return bp;
	}

	else if (prev_alloc && !next_alloc) {
		remove_from_free(NEXT_BLKP(bp));
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size,0));
		PUT(FTRP(bp), PACK(size,0));
	}

	else if (!prev_alloc && next_alloc) {
		remove_from_free(PREV_BLKP(bp));
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size,0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
		bp=PREV_BLKP(bp);
	}

	else {
		remove_from_free(NEXT_BLKP(bp));
		remove_from_free(PREV_BLKP(bp));
		size += GET_SIZE(HDRP(PREV_BLKP(bp)))+
			GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
		bp=PREV_BLKP(bp);
	}
	return bp;
}

/*
 * place - Places the requested block at the beginning of the free block,
 *		splitting only if the size of the remainder would equal or exceed
 *		the minimum block size
 */
void place(void *bp, size_t asize)
{
	char *bpsplit=NULL;
	size_t size=asize;
	size_t extr_spc;
	//Minimum block size is 16
	//Note, free block is already removed from free list but alloc bit must be
	//reset to 1
	if((extr_spc = GET_SIZE(HDRP(bp))-asize)>=(2*DSIZE)) { 
		PUT(HDRP(bp),PACK(size,1));
		PUT(FTRP(bp),PACK(size,1));
		bpsplit=NEXT_BLKP(bp);//Next free block pointer
		PUT(HDRP(bpsplit),PACK(extr_spc,0));
		PUT(FTRP(bpsplit),PACK(extr_spc,0));
		add_to_free(bpsplit);
	}
	else {
		size=GET_SIZE(HDRP(bp));//Take the whole free block
		PUT(HDRP(bp),PACK(size,1));
		PUT(FTRP(bp),PACK(size,1));
	}
}

/*
 * find_fit - Performs a first-fit search of the corresponding box in the
 *		segregated free list
 */
void *find_fit(size_t size)
{
	char *bp;
	int box = find_box(size);
	//We search in the smallest matching box first, then move up
	while(box<=15) {
		bp=(char *)GET(free_listp+(box*WSIZE));
		while(bp!=0) {//Run through explicit list until none left
			if(GET_SIZE(HDRP(bp))>=size) {
				remove_from_free(bp);
				return bp;
			}
			else {
				bp=(char *)GET(bp);
			}
		}
		box++;
	}
	return NULL;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size)
{
	if(bp==NULL)
		return mm_malloc(size);
	if(size==0) {
		mm_free(bp);
		return NULL;
	}
	
	
	char *newbp=NULL;
	size_t copySize = GET_SIZE(HDRP(bp));
	//Try to "coalesce" with surrounding blocks before resorting to a
	//heap extension
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t asize = copySize;
	size_t msize = DSIZE*((size+(DSIZE)+(DSIZE-1))/DSIZE);
	int noSpace=0;

	if(size<copySize) {/*
		if((copySize-size)>=(2*DSIZE)) {
			//Split current block
			place(bp,size);
			newbp=bp;
		}
		else {*/
			noSpace=1;	
			copySize=size;
		//}
	}

	else if (prev_alloc && next_alloc) {
		noSpace=1;
	}

	else if (prev_alloc && !next_alloc) {
		asize += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		if(asize>=msize) {
			remove_from_free(NEXT_BLKP(bp));
			if((asize-msize)>=2*DSIZE) {
				PUT(HDRP(bp), PACK(msize,1));
				PUT(FTRP(bp), PACK(msize,1));
				char *bpsplit=NEXT_BLKP(bp);
				PUT(HDRP(bpsplit),PACK(asize-msize,0));
				PUT(FTRP(bpsplit),PACK(asize-msize,0));
				add_to_free(bpsplit);
			}
			else {
				PUT(HDRP(bp), PACK(asize,1));
				PUT(FTRP(bp), PACK(asize,1));
			}
			//No need to copy memory
			newbp=bp;
		}
		else
			noSpace=1;
	}

	else if (!prev_alloc && next_alloc) {
		asize += GET_SIZE(HDRP(PREV_BLKP(bp)));
		if(asize>=msize) {
			remove_from_free(PREV_BLKP(bp));
			if((asize-msize)>=2*DSIZE) {
				newbp=PREV_BLKP(bp);
				PUT(HDRP(newbp), PACK(msize,1));
				char *bpsplit=NEXT_BLKP(newbp);
				//Use memmove in case of overlapping data
				//Do this before adding any new footers or headers
				//that could overwrite data.
				memmove(newbp,bp,copySize);
				PUT(FTRP(newbp), PACK(msize,1));
				PUT(HDRP(bpsplit),PACK(asize-msize,0));
				PUT(FTRP(bpsplit),PACK(asize-msize,0));
				add_to_free(bpsplit);
			}
			else {
				PUT(FTRP(bp), PACK(asize,1));
				PUT(HDRP(PREV_BLKP(bp)), PACK(asize,1));
				newbp=PREV_BLKP(bp);
				memmove(newbp,bp,copySize);
			}
		}
		else
			noSpace=1;
	}

	else {
		asize += GET_SIZE(HDRP(PREV_BLKP(bp)))+
			GET_SIZE(HDRP(NEXT_BLKP(bp)));
		if(asize>=msize) {
			remove_from_free(NEXT_BLKP(bp));
			remove_from_free(PREV_BLKP(bp));
			if((asize-msize)>=2*DSIZE){
				newbp=PREV_BLKP(bp);
				PUT(HDRP(newbp), PACK(msize,1));
				char *bpsplit=NEXT_BLKP(newbp);
				//Use memmove in case of overlapping data
				//Do this before adding any new footers or headers
				//that could overwrite data.
				memmove(newbp,bp,copySize);
				PUT(FTRP(newbp), PACK(msize,1));
				PUT(HDRP(bpsplit),PACK(asize-msize,0));
				PUT(FTRP(bpsplit),PACK(asize-msize,0));
				add_to_free(bpsplit);
			}
			else {
				PUT(FTRP(bp), PACK(asize,1));
				PUT(HDRP(PREV_BLKP(bp)), PACK(asize,1));
				newbp=PREV_BLKP(bp);
				memmove(newbp,bp,copySize);
			}
		}
		else
			noSpace=1;
	}

	if(noSpace) {
		newbp=mm_malloc(size);
		//Copy over memory and free pointer
		memcpy(newbp,bp,copySize);
		mm_free(bp);
	}

	return newbp;
}

int mm_check(void)
{
	/*
	//Is every block in the free list marked as free?
	int i;
	for(i=1;i<16;i++) {
	}
	//Are there any contiguous free blocks that somehow escaped coalescing?
	int free;
	while(no epilogue){
	}
	//Is every free block actually in the free list?
	//Do the pointers in the free list point to valid free blocks?
	//Do any allocated blocks overlap?
	//Do the pointers in a heap block point to valid heap addresses?
	*/
	return 1;
}
