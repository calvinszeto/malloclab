/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
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

static char *heap_listp;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	//Push up break pointer by four words
	if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
		return -1;
	PUT(heap_listp,0);//padding word
	PUT(heap_listp+(1*WSIZE),PACK(DSIZE,1));//header
	PUT(heap_listp+(2*WSIZE),PACK(DSIZE,1));//footer
	PUT(heap_listp+(3*WSIZE),PACK(0,1));//epilogue block

	if (extend_heap(CHUNKSIZE/WSIZE) == NULL)//expand the heap
		return -1;
	return 0;
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

	return coalesce(bp);
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
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size,0));
		PUT(FTRP(bp), PACK(size,0));
	}

	else if (!prev_alloc && next_alloc) {
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size,0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
		bp=PREV_BLKP(bp);
	}

	else {
		size += GET_SIZE(HDRP(PREV_BLKP(bp)))+
			GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
		bp=PREV_BLKP(bp);
	}
	return bp;
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
		asize=DSIZE*((size+(DSIZE)+(DSIZE+1))/DSIZE);

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
 * place - Places the requested block at the beginning of the free block,
 *		splitting only if the size of the remainder would equal or exceed
 *		the minimum block size
 */
void place(void *bp, size_t asize)
{
	char *bpsplit=NULL;
	size_t size=asize;
	size_t extr_spc;
	//Minimum block size is 8 for now
	if((extr_spc = GET_SIZE(HDRP(bp))-asize)>=DSIZE) {
		bpsplit=((char *)bp + asize);//Next free block pointer
		PUT(HDRP(bpsplit),PACK(extr_spc,0));
		PUT(FTRP(bpsplit),PACK(extr_spc,0));
	}
	else
		size=GET_SIZE(HDRP(bp));//Take the whole free block
	PUT(HDRP(bp),PACK(size,1));
	PUT(FTRP(bp),PACK(size,1));
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
	coalesce(ptr);
}

/*
 * find_fit - Performs a first-fit search of the implicit free list
 */
void *find_fit(size_t size)
{
	char *hdr = heap_listp;	
	char *bp;
	while(GET_SIZE(hdr)!=0) {//Run through implicit list until epilogue block
		bp=hdr+WSIZE;	
		if(!GET_ALLOC(hdr)&&GET_SIZE(hdr)>=size)
			return bp;
		else
			hdr=HDRP(NEXT_BLKP(bp));
	}
	return NULL;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
	void *oldptr = ptr;
	void *newptr;
	size_t copySize;

	newptr = mm_malloc(size);
	if (newptr == NULL)
		return NULL;
	copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
	if (size < copySize)
		copySize = size;
	memcpy(newptr, oldptr, copySize);
	mm_free(oldptr);
	return newptr;
}














