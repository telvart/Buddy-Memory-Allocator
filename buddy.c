/**
 * Buddy Allocator
 *
 * For the list library usage, see http://www.mcs.anl.gov/~kazutomo/list/
 */

/**************************************************************************
 * Conditional Compilation Options
 **************************************************************************/
#define USE_DEBUG 0

/**************************************************************************
 * Included Files
 **************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "buddy.h"
#include "list.h"

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define MIN_ORDER 12//4kb min size(page size)
#define MAX_ORDER 20//1mg total size

#define PAGE_SIZE (1<<MIN_ORDER) // 2^12 = 4k
/* page index to address */
#define PAGE_TO_ADDR(page_idx) (void *)(((page_idx)*PAGE_SIZE) + g_memory) // returns pointer to location in g_memory

/* address to page index */
#define ADDR_TO_PAGE(addr) ((unsigned long)(((void *)(addr)) - (void *)g_memory) / PAGE_SIZE) //return what page address belongs to

/* find buddy address */
#define BUDDY_ADDR(addr, o) (void *)((((unsigned long)(addr) - (unsigned long)g_memory) ^ (1<<(o))) + (unsigned long)g_memory) //returns a pointer to the memory's buddy.(args = address,order)




#if USE_DEBUG == 1
#  define PDEBUG(fmt, ...) \
	fprintf(stderr, "%s(), %s:%d: " fmt,			\
		__func__, __FILE__, __LINE__, ##__VA_ARGS__)
#  define IFDEBUG(x) x
#else
#  define PDEBUG(fmt, ...)
#  define IFDEBUG(x)
#endif


/**************************************************************************
 * Public Types
 **************************************************************************/
typedef struct {
	struct list_head list;

	int index;
	char *address;
	int order;

} page_t;

/**************************************************************************
 * Global Variables
 **************************************************************************/
/* free lists*/
struct list_head free_area[MAX_ORDER+1];
/* memory area */
char g_memory[1<<MAX_ORDER]; // 2^20 bytes of memory

/* page structures */
page_t g_pages[(1<<MAX_ORDER)/PAGE_SIZE];

/**************************************************************************
 * Public Function Prototypes
 **************************************************************************/

/**************************************************************************
 * Local Functions
 **************************************************************************/

/**
 * Initialize the buddy system
 */
void buddy_init()
{
	int i;
	int n_pages = (1<<MAX_ORDER) / PAGE_SIZE;

	//Initialize g_pages
	for (i = 0; i < n_pages; i++)
	{
		g_pages[i].index = i;
		g_pages[i].address = PAGE_TO_ADDR(i);
		g_pages[i].order = -1;
	}

	//initialize free_area
	for (i = MIN_ORDER; i <= MAX_ORDER; i++)
 	{
		INIT_LIST_HEAD(&free_area[i]);
	}

	g_pages[0].order = MAX_ORDER;

	// list the entire memory as free
	list_add(&g_pages[0].list, &free_area[MAX_ORDER]);
}

//determines what order of mem will need to be allocated
int determineOrder(int size)
{
	int order;
	for (order = MIN_ORDER; order <= MAX_ORDER; order++)
	{
		if ((1<<order) >= size)
		{
			return order;
	 	}
}
	return -1;
}


//recursive function to split memory into smaller blocks as needed
void splitMemory(page_t* page, int order, int orderNeeded)
{
	if(order == orderNeeded)
	{
		return;
	}
	page_t* buddy = &g_pages[ADDR_TO_PAGE(BUDDY_ADDR(page->address,order-1))];
	buddy->order = order-1;
	list_add(&(buddy->list),&free_area[order-1]);//adds buddy to free area
	splitMemory(page,order-1,orderNeeded);
}

/**
 * Allocate a memory block.
 *
 * On a memory request, the allocator returns the head of a free-list of the
 * matching size (i.e., smallest block that satisfies the request). If the
 * free-list of the matching block size is empty, then a larger block size will
 * be selected. The selected (large) block is then splitted into two smaller
 * blocks. Among the two blocks, left block will be used for allocation or be
 * further splitted while the right block will be added to the appropriate
 * free-list.
 *
 * @param size size in bytes
 * @return memory block address
 */
void *buddy_alloc(int size)
{
	int orderNeeded = determineOrder(size);

	if( orderNeeded == -1) //too big of a request
	{
		return NULL;
	}

	for (int i = orderNeeded; i<= MAX_ORDER; i++)
	{
		if(!list_empty(&free_area[i]))
		{

			page_t *page = list_entry(free_area[i].next,page_t,list);

			list_del_init(&(page->list));
			splitMemory(page, i, orderNeeded);
			page->order = orderNeeded;
			return ((void*)(page->address));
		}
	}
	return NULL; //there was not enough memory available
}

/**
 * Free an allocated memory block.
 *
 * Whenever a block is freed, the allocator checks its buddy. If the buddy is
 * free as well, then the two buddies are combined to form a bigger block. This
 * process continues until one of the buddies is not free.
 *
 * @param addr memory block address to be freed
 */
void buddy_free(void *addr)
{
	page_t * page = &g_pages[ADDR_TO_PAGE(addr)];
	int i;
	int currentOrder=page->order;

	if((currentOrder<MIN_ORDER) || (currentOrder>MAX_ORDER))
	{
		printf("ERROR: invalid order: currentOrder=%d",currentOrder);
		while(1);
	}

	for(i = currentOrder; i<MAX_ORDER; i++)
	{
		page_t* buddy = &g_pages[ADDR_TO_PAGE(BUDDY_ADDR(page->address,i))];
		char freed = 0;
		struct list_head *pos;

		list_for_each(pos,&free_area[i])
		{
			if(list_entry(pos,page_t,list)==buddy)
			{
				freed = 1;
			}
		}

		if (!freed)
		{
			break;
		}

		list_del_init(&buddy->list);

		if(buddy<page)
		{
			page = buddy;
		}
	}

	page->order = i;
	list_add(&page->list,&free_area[i]);

}

/**
 * Print the buddy system status---order oriented
 *
 * print number of free pages in each order.
 */
void buddy_dump()
{
	int o;
	for (o = MIN_ORDER; o <= MAX_ORDER; o++) {
		struct list_head *pos;
		int cnt = 0;
		list_for_each(pos, &free_area[o]) {
			cnt++;
		}
		printf("%d:%dK ", cnt, (1<<o)/1024);
	}
	printf("\n");
}
