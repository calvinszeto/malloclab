#include <stdio.h>

extern int mm_init (void);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);
int mm_check(void);
int find_box(size_t size);
void *extend_heap(size_t words);
void *coalesce(void *bp);
void *add_to_free(void *bp);
void remove_from_free(void *bp);
void *find_fit(size_t size);
void *run_list(int box,size_t size);
void place(void *bp, size_t asize);
int in_free_list(void *bp);


/* 
 * Students work in teams of one or two.  Teams enter their team name, 
 * personal names and login IDs in a struct of this
 * type in their bits.c file.
 */
typedef struct {
    char *teamname; /* ID1+ID2 or ID1 */
    char *name1;    /* full name of first member */
    char *id1;      /* login ID of first member */
    char *name2;    /* full name of second member (if any) */
    char *id2;      /* login ID of second member */
} team_t;

extern team_t team;

