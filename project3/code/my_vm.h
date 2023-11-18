#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> 
#include <stdbool.h>

//Assume the address space is 32 bits, so the max memory size is 4GB
//Page size is 4KB

//Add any important includes here which you may need

#define PGSIZE 4096

// Maximum size of virtual memory
#define MAX_MEMSIZE 4ULL*1024*1024*1024

// Size of "physcial memory"
#define MEMSIZE 1024*1024*1024

// Represents a page table entry
typedef unsigned long pte_t;

// Represents a page directory entry
typedef unsigned long pde_t;

#define TLB_ENTRIES 512

//Structure to represents TLB
struct tlb {
    /*Assume your TLB is a direct mapped TLB with number of entries as TLB_ENTRIES
    * Think about the size of each TLB entry that performs virtual to physical
    * address translation.
    */
   unsigned int VPN;
   pte_t* physicalAddress;
   int mostRecentTLBUse;
};
//struct tlb tlbStore;


void set_physical_mem();
pte_t *translate(pde_t *pgdir, void *va);
int page_map(pde_t *pgdir, void *va, void* pa);
//bool check_in_tlb(void *va);
//void put_in_tlb(void *va, void *pa);
//int add_TLB(void *va, void *pa);
//pte_t *check_TLB(void *va)
void *t_malloc(unsigned int num_bytes);
void t_free(void *va, int size);
int put_value(void *va, void *val, int size);
void get_value(void *va, void *val, int size);
void mat_mult(void *mat1, void *mat2, int size, void *answer);
void print_TLB_missrate();
void free_TLB(unsigned int VPN);
static void set_bit_at_index(char *bitmap, int index);
static int get_bit_at_index(char *bitmap, int index);
static void free_bit_at_index(char *bitmap, int index);
void *get_next_avail(int num_pages);
void *get_next_avail_p();

#endif
