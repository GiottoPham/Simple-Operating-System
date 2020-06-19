
#include "mem.h"
#include "stdlib.h"
#include "string.h"
#include <pthread.h>
#include <stdio.h>

static BYTE _ram[RAM_SIZE];

static struct {
	uint32_t proc;	// ID of process currently uses this page
	int index;	// Index of the page in the list of pages allocated
			// to the process.
	int next;	// The next page in the list. -1 if it is the last
			// page.
} _mem_stat [NUM_PAGES];

static pthread_mutex_t mem_lock;

void init_mem(void) {
	memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES);
	memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);
	pthread_mutex_init(&mem_lock, NULL);
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr) {
	return addr & ~((~0U) << OFFSET_LEN);
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr) {
	return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr) {
	return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
}

/* Search for page table table from the a segment table */
static struct page_table_t * get_page_table(
		addr_t index, 	// Segment level index
		struct seg_table_t * seg_table) { // first level table
	
	/*
	 * TODO: Given the Segment index [index], you must go through each
	 * row of the segment table [seg_table] and check if the v_index
	 * field of the row is equal to the index
	 *
	 * */

	int i;
	for (i = 0; i < seg_table->size; i++) {
		// Enter your code here
		if (seg_table->table[i].v_index == index) {
			return seg_table->table[i].pages;
		}
	}
	return NULL;

}

/* Translate virtual address to physical address. If [virtual_addr] is valid,
 * return 1 and write its physical counterpart to [physical_addr].
 * Otherwise, return 0 */
static int translate(
		addr_t virtual_addr, 	// Given virtual address
		addr_t * physical_addr, // Physical address to be returned
		struct pcb_t * proc) {  // Process uses given virtual address

	/* Offset of the virtual address */
	addr_t offset = get_offset(virtual_addr);
	/* The first layer index */
	addr_t first_lv = get_first_lv(virtual_addr);
	/* The second layer index */
	addr_t second_lv = get_second_lv(virtual_addr);
	
	/* Search in the first level */
	struct page_table_t * page_table = NULL;
	page_table = get_page_table(first_lv, proc->seg_table);
	if (page_table == NULL) {
		return 0;
	}

	int i;
	for (i = 0; i < page_table->size; i++) {
		if (page_table->table[i].v_index == second_lv) {
			/* TODO: Concatenate the offset of the virtual addess
			 * to [p_index] field of page_table->table[i] to 
			 * produce the correct physical address and save it to
			 * [*physical_addr]  */
			addr_t physical_index = page_table->table[i].p_index; 
			* physical_addr = (physical_index << OFFSET_LEN) | offset;
			return 1;
		}
	}
	return 0;	
}

addr_t alloc_mem(uint32_t size, struct pcb_t * proc) {
	pthread_mutex_lock(&mem_lock);
	addr_t ret_mem = 0;
	/* TODO: Allocate [size] byte in the memory for the
	 * process [proc] and save the address of the first
	 * byte in the allocated memory region to [ret_mem].
	 * */

	uint32_t num_pages = ((size % PAGE_SIZE) == 0) ? size / PAGE_SIZE : size / PAGE_SIZE + 1; // Number of pages we will use
	int mem_avail = 0; // We could allocate new memory region or not?

	/* First we must check if the amount of free memory in
	 * virtual address space and physical address space is
	 * large enough to represent the amount of required 
	 * memory. If so, set 1 to [mem_avail].
	 * Hint: check [proc] bit in each page of _mem_stat
	 * to know whether this page has been used by a process.
	 * For virtual memory space, check bp (break pointer).
	 * */

	int i, v_check = 0, p_check = 0;

	// Check virtual memory space
	uint32_t new_proc_bp = proc->bp + num_pages * PAGE_SIZE;
	if (new_proc_bp < RAM_SIZE) v_check = 1;

	// Check physical memory space
	int cnt_free_pages = 0; 
	for (i = 0; i < NUM_PAGES; i++) {
		if (_mem_stat[i].proc == 0) {
			cnt_free_pages++;
			if (cnt_free_pages == num_pages) break;
		}
	}
	if (cnt_free_pages >= num_pages) p_check = 1;

	mem_avail = p_check && v_check;


	if (mem_avail) {
		/* We could allocate new memory region to the process */
		ret_mem = proc->bp;
		proc->bp += num_pages * PAGE_SIZE;
		/* Update status of physical pages which will be allocated
		 * to [proc] in _mem_stat. Tasks to do:
		 * 	- Update [proc], [index], and [next] field
		 * 	- Add entries to segment table page tables of [proc]
		 * 	  to ensure accesses to allocated memory slot is
		 * 	  valid. */

		int cnt = 0; 
		int last_page_idx = -1;
		int pg;
		for (pg = 0; pg < NUM_PAGES; pg++) {
			if (_mem_stat[pg].proc == 0) {
				_mem_stat[pg].proc = proc->pid; 
				_mem_stat[pg].index = cnt; 

				addr_t virtual_address = ret_mem + cnt * PAGE_SIZE; 
				addr_t virtual_segment = get_first_lv(virtual_address);
				// printf("+ pid = %d ::: v_segment = %d, v_page = %d\n", 
				// 	proc->pid, v_segment, get_second_lv(v_address));
				struct page_table_t * virtual_page_table = get_page_table(virtual_segment, proc->seg_table);
				if (virtual_page_table == NULL) {
					int index = proc->seg_table->size;
					proc->seg_table->table[index].v_index = virtual_segment;
					virtual_page_table
						= proc->seg_table->table[index].pages
						= (struct page_table_t*) malloc(sizeof(struct page_table_t));
					proc->seg_table->size++;
				}
				int idx = virtual_page_table->size++;
				virtual_page_table->table[idx].v_index = get_second_lv(virtual_address);
				virtual_page_table->table[idx].p_index = pg; 

				if (last_page_idx != -1) { 
					_mem_stat[last_page_idx].next = pg;
				}
				last_page_idx = pg; 
				
				cnt++;
				if (cnt == num_pages) {
					_mem_stat[pg].next = -1; 
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&mem_lock);
	return ret_mem;
}

int free_mem(addr_t address, struct pcb_t * proc) {
	/*TODO: Release memory region allocated by [proc]. The first byte of
	 * this region is indicated by [address]. Task to do:
	 * 	- Set flag [proc] of physical page use by the memory block
	 * 	  back to zero to indicate that it is free.
	 * 	- Remove unused entries in segment table and page tables of
	 * 	  the process [proc].
	 * 	- Remember to use lock to protect the memory from other
	 * 	  processes.  */

	pthread_mutex_lock(&mem_lock);
	
	addr_t v_address = address;	
	addr_t p_address = 0;		
	
	if (!translate(v_address, &p_address, proc)) return 1;
	
	addr_t physical_segment_page_index = p_address >> OFFSET_LEN; 
	int num_pages = 0; 
	int i;
	for (i = physical_segment_page_index; i != -1;) {
		num_pages++;
		_mem_stat[i].proc = 0; 
		i = _mem_stat[i].next;
	}
	
	for (i = 0; i < num_pages; i++) {
		addr_t v_addr = v_address + i * PAGE_SIZE;
		addr_t v_segment = get_first_lv(v_addr);
		addr_t v_page = get_second_lv(v_addr);
		// printf("- pid = %d ::: v_segment = %d, v_page = %d\n", proc->pid, v_segment, v_page);
		struct page_table_t * page_table = get_page_table(v_segment, proc->seg_table);
		if (page_table == NULL) continue;

		int j;
		for (j = 0; j < page_table->size; j++) {
			if (page_table->table[j].v_index == v_page) {
				page_table->size--;
				int last_page = page_table->size;
				page_table->table[j] = page_table->table[last_page];
				break;
			}
		}

		if (page_table->size == 0) {
			struct seg_table_t * seg_table = proc->seg_table;
			if (seg_table == NULL) continue;
			int i;
			for (i = 0; i < seg_table->size; i++) {
				if (seg_table->table[i].v_index == v_segment) {
					int idx = seg_table->size - 1;
					seg_table->table[i] = seg_table->table[idx];
					seg_table->table[idx].v_index = 0;
					free(seg_table->table[idx].pages);
					seg_table->size--;
				}
			}
		}
	}

	if (v_address + num_pages * PAGE_SIZE == proc->bp) {
		while (proc->bp >= PAGE_SIZE) {
			addr_t last_addr = proc->bp - PAGE_SIZE;
			addr_t last_segment = get_first_lv(last_addr);
			addr_t last_page = get_second_lv(last_addr);
			struct page_table_t * page_table = get_page_table(last_segment, proc->seg_table);
			if (page_table == NULL) break;
			while (last_page >= 0) {
				int i;
				for (i = 0; i < page_table->size; i++) {
					if (page_table->table[i].v_index == last_page) {
						proc->bp -= PAGE_SIZE;
						last_page--;
						break;
					}
				}
				if (i == page_table->size) break;
			}
			if (last_page >= 0) break;
		}
	}

	pthread_mutex_unlock(&mem_lock);
	return 0;
}

int read_mem(addr_t address, struct pcb_t * proc, BYTE * data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		*data = _ram[physical_addr];
		return 0;
	}else{
		return 1;
	}
}

int write_mem(addr_t address, struct pcb_t * proc, BYTE data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		_ram[physical_addr] = data;
		return 0;
	}else{
		return 1;
	}
}

void dump(void) {
	int i;
	for (i = 0; i < NUM_PAGES; i++) {
		if (_mem_stat[i].proc != 0) {
			printf("%03d: ", i);
			printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n",
				i << OFFSET_LEN,
				((i + 1) << OFFSET_LEN) - 1,
				_mem_stat[i].proc,
				_mem_stat[i].index,
				_mem_stat[i].next
			);
			int j;
			for (	j = i << OFFSET_LEN;
				j < ((i+1) << OFFSET_LEN) - 1;
				j++) {
				
				if (_ram[j] != 0) {
					printf("\t%05x: %02x\n", j, _ram[j]);
				}
					
			}
		}
	}
}
