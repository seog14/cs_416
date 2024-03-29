#include "my_vm.h"
#include <string.h>
#include <math.h>

pde_t ***page_directory;
void *physicalMemory;
char *virtualPageTracker;
char *physicalPageTracker;
struct tlb *tlbStore;
int offsetBits;
int totalBits;
int PTEBits;
int PDEBits;
int TLBIndexBits; 
int totalVirtualPages = (MAX_MEMSIZE / PGSIZE);
int totalPhysicalPages = (MEMSIZE / PGSIZE);
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned int TLBMiss;
unsigned int TLBTotal;


/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {
    
    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating
    physicalMemory = malloc(MEMSIZE);
    memset(physicalMemory, 0, MEMSIZE);
    page_directory = (pde_t ***)physicalMemory;
    
    /*
    printf("Address of page_directory first frame = %p\n", (void *)&page_directory[0][0]);
    printf("Address of page_directory first entry = %p\n", (void *)&page_directory[0]);
    printf("Address of page_directory second entry = %p\n", (void *)&page_directory[1]);
    printf("Address of page_directory second frame = %p\n", (void *)&page_directory[0][1]);*/

    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
    virtualPageTracker = malloc(totalVirtualPages/8);
    memset(virtualPageTracker, 0, totalVirtualPages/8);
    physicalPageTracker = malloc(totalPhysicalPages/8);
    memset(physicalPageTracker, 0, totalPhysicalPages/8);

    //global
    offsetBits = log(PGSIZE)/log(2);
    totalBits = log(MAX_MEMSIZE)/log(2);
    PTEBits = log(PGSIZE/sizeof(pte_t))/log(2);
    PDEBits = totalBits - offsetBits - PTEBits;
    TLBIndexBits = log(TLB_ENTRIES)/log(2);

    //Find pages needed for directory and allocate them in physical memory
    //In case multiple pages needed for the directory
    int num_pde = (1 << PDEBits);
    int num_pages_for_directory = ((num_pde * 4) + PGSIZE - 1) / PGSIZE;
    pde_t *** ptr = page_directory;
    int i; 
    for (i = 0; i < num_pde; i++){
        *ptr = NULL;
        ptr++; 
    }
    for(i = 0; i < num_pages_for_directory; i++){
        set_bit_at_index(physicalPageTracker, i);
    }
    set_bit_at_index(physicalPageTracker, 0);
    //First page table, maps to second physical page

    page_directory[0] = get_next_avail_p();

    // TLB Store
    tlbStore = malloc(sizeof(struct tlb) * (TLB_ENTRIES + 1));
    for (i = 0; i < TLB_ENTRIES; i++){
        tlbStore[i].VPN = -1;
    }
    TLBMiss = 0;
    TLBTotal = 0;
    set_bit_at_index(virtualPageTracker, 0);
}

/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int
add_TLB(void *va, void *pa)
{
    TLBTotal ++;
    TLBMiss ++;
    //(unsigned int)va >> (totalBits - TLBIndexBits);
    unsigned int VPN = (unsigned int)va >> offsetBits;
    unsigned int index = VPN & ((1 << TLBIndexBits) - 1);
    tlbStore[index].VPN = VPN;
    tlbStore[index].valid = 1;
    tlbStore[index].physicalAddress = (pte_t*)pa;
    return 0;
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t *
check_TLB(void *va) {

    /* Part 2: TLB lookup code here */
    
    //(unsigned int)va >> (totalBits - TLBIndexBits);
    unsigned int VPN = (unsigned int)va >> offsetBits;
    unsigned int index = VPN & ((1 << TLBIndexBits) - 1);
    if (tlbStore[index].valid == 1 && tlbStore[index].VPN == VPN){
        TLBTotal ++;
        return tlbStore[index].physicalAddress;
    }

   /*This function should return a pte_t pointer*/
}

void free_TLB(unsigned int VPN){
    unsigned int index = VPN & ((1 << TLBIndexBits) - 1);
    //VPN >> (totalBits - offsetBits - TLBIndexBits);
    tlbStore[index].valid = 0;
}

/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void
print_TLB_missrate()
{
    double miss_rate = (double)(TLBMiss) / (double)(TLBTotal);	

    /*Part 2 Code here to calculate and print the TLB miss rate*/
    
    fprintf(stderr, "TLB miss rate %lf with misses %d, total %d\n", miss_rate, TLBMiss, TLBTotal);
}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t *translate(pde_t *pgdir, void *va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */
    int offset = (unsigned int)va & ((1 << offsetBits) - 1);
    if ((int)check_TLB(va) != -1){
        //printf("Check TLB used\n");
        return check_TLB(va) + offset;
    }
    int PDEIndex = (unsigned int)va >> (offsetBits + PTEBits);
    int PTEIndex = ((unsigned int)va >> offsetBits) & ((1 << PTEBits) - 1);
    pde_t *** pageDirectoryEntry = page_directory + PDEIndex; 
    pte_t ** pageOfPageTable = *pageDirectoryEntry; 
    if (pageOfPageTable == NULL){
        exit(1);
        fprintf(stderr,"Accessing invalid entry in translate function\n");
        return (pte_t *)-1; // Not a valid page directory index
    }
    pte_t ** pageTableEntry = pageOfPageTable + PTEIndex; 
    int success = add_TLB(va, (void *) *pageTableEntry);
    //printf("TLB added if success is 0 %d\n", success);
    return *pageTableEntry + offset;
    /*
    if (page_directory[PDEIndex])[PTEIndex] == NULL {
        return NULL;
    }
    return (pte_t*)(page_directory[PDEIndex])[PTEIndex] + offset;*/
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int
page_map(pde_t *pgdir, void *va, void *pa)
{
    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */

    // map the virtual address to the physical address
    int PDEIndex = (unsigned int)va >> (offsetBits + PTEBits);
    int PTEIndex = ((unsigned int)va >> offsetBits) & ((1 << PTEBits) - 1);

    if (page_directory[PDEIndex] == NULL){
        // allocate a new page for the new page table and add it to page directory
        // get next available physical page
        page_directory[PDEIndex] = get_next_avail_p();
    }
    int offset = (unsigned int)va & ((1 << offsetBits) - 1);
    page_directory[PDEIndex][PTEIndex] = (pte_t *)(pa - offset);
    add_TLB(va, pa);
    /*
    int success = add_TLB(va, pa);
    printf("TLB added if success in page map is 0 %d\n", success);*/
    //printf("What was stored %lx\n", (long unsigned int)page_directory[PDEIndex][PTEIndex]);
    //printf("translate functon %lx\n", (long unsigned int)translate(NULL, va));
    return 0;
}


/*Function that gets the next available page frame number (not address)
*/
void *get_next_avail(int num_pages) {
    //Use virtual address bitmap to find the next free page
    int i;
    for (i = 0; i < totalVirtualPages; i ++){
        if (get_bit_at_index(virtualPageTracker, i) == 0){
            // check if we have contiguous space for the num_pages
            int j;
            int notGood = 0;
            for (j = 0; j < num_pages; j++){
                if (get_bit_at_index(virtualPageTracker, i + j) == 1){
                    notGood = 1;
                    break;
                }
            }
            if (!notGood){
                for (j = 0; j < num_pages; j++){
                    set_bit_at_index(virtualPageTracker, i + j);
                }
                return (void *)i;
            }
        }
    }
    // no pages left
    exit(1);
    fprintf(stderr,"No more virtual pages\n");
    return (void *)-1;
}

/*void *get_next_avail(int num_pages) {
    int next_virtual_avail_index; 
    int size = sizeof(virtualPageTracker)/sizeof(virtualPageTracker[0]); 
    int i; 
    int j; 
    for(i = 0; i < size; i++){
        for(j = 0; j < 8 - (num_pages - 1); j++){
            if(physicalPageTracker[i] & (1 << j) == 0){

            }
        }
    }
}*/


/* Function responsible for allocating pages
and used by the benchmark
*/
void *t_malloc(unsigned int num_bytes) {
    /* 
     * HINT: If the physical memory is not yet initialized, then allocate and initialize.
     */
    pthread_mutex_lock(&mutex); 
    if (physicalMemory == NULL){
        set_physical_mem();
    }

   /* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used.
    */
    int pagesNeeded = (num_bytes + PGSIZE - 1) / PGSIZE;
    int VPN = (int)get_next_avail(pagesNeeded);
    //printf("Next available page number = %d\n", VPN);
    int i;
    for (i = 0; i < pagesNeeded; i++){
        unsigned int physicalAddress = (unsigned int)get_next_avail_p();
        //printf("physical address %x\n", physicalAddress);
        //printf("Next available physical page number = %x\n", physicalAddress);
        int actualVirtualAddress = (VPN + i)*PGSIZE;
        memset((void *)physicalAddress, 0, PGSIZE);
        page_map(NULL, (void *)(actualVirtualAddress), (void *)physicalAddress);
    }
    pthread_mutex_unlock(&mutex);
    return (void *)(VPN << offsetBits);
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void t_free(void *va, int size) {
    /* Part 1: Free the page table entries starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the 
     * memory from "va" to va+size is valid.
     *
     * Part 2: Also, remove the translation from the TLB
     */
    pthread_mutex_lock(&mutex);

    int pagesNeeded = (size + PGSIZE - 1) / PGSIZE; 
    int i; 
    int vpn = (unsigned int) va >> offsetBits; 
    int offset = (unsigned int)va & ((1 << offsetBits) - 1);
    for (i = 0; i < pagesNeeded; i++){
        int actualVirtualAddress = ((unsigned int)(vpn + i) * PGSIZE) + offset; 
        pte_t *physicalAddress = translate(NULL, (void*) actualVirtualAddress);
        pte_t *physicalFrame = physicalAddress - offset;
        long unsigned int physicalPage = ((long unsigned int)physicalFrame - (long unsigned int)physicalMemory) / (PGSIZE); 
        int virtualPage = (unsigned int) actualVirtualAddress >> offsetBits; 
        if (get_bit_at_index(virtualPageTracker, virtualPage) == 0){
			fprintf(stderr,"Freeing already freed page\n");
			exit(1);
        }
        free_bit_at_index(virtualPageTracker, virtualPage);
        if (get_bit_at_index(physicalPageTracker, physicalPage) == 0){
            fprintf(stderr,"Freeing already freed page\n");
            exit(1);
        }
        free_bit_at_index(physicalPageTracker, physicalPage);
        memset(physicalAddress, 0, PGSIZE);
        free_TLB(virtualPage);
    }
    pthread_mutex_unlock(&mutex);

    // pte_t *physicalAddress = translate(NULL, va);
    // memset(physicalAddress, 0, size);
    // int PDEIndex = (unsigned int)va >> (offsetBits + PTEBits);
    // int PTEIndex = ((unsigned int)va >> offsetBits) & ((1 << PTEBits) - 1);
    // int virtualPage = (unsigned int)va >> (offsetBits);
    // page_directory[PDEIndex][PTEIndex] = NULL;
    // free_bit_at_index(virtualPageTracker, virtualPage);
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
 * The function returns 0 if the put is successfull and -1 otherwise.
*/
int put_value(void *va, void *val, int size) {
    pthread_mutex_lock(&mutex);
    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger 
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */
    int pagesNeeded = (size + PGSIZE - 1) / PGSIZE;
    int i;
    int sizeLeft = size;
    for (i = 0; i < pagesNeeded; i++){
        pte_t *physicalAddress = translate(NULL, va + i);
        memcpy((void *)physicalAddress, val, min(sizeLeft, PGSIZE));
        val += PGSIZE;
        sizeLeft -= PGSIZE;
    }
    //memcpy((void *)physicalAddress, val, size);
    pthread_mutex_unlock(&mutex);
    //printf("the number %ld is stored in physical address %lx\n", *physicalAddress, (unsigned long int)physicalAddress);
    return 0;

    /*return -1 if put_value failed and 0 if put is successfull*/

}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {
    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */
    pthread_mutex_lock(&mutex);
    int pagesNeeded = (size + PGSIZE - 1) / PGSIZE;
    int i;
    int sizeLeft = size;
    for (i = 0; i < pagesNeeded; i++){
        pte_t *physicalAddress = translate(NULL, va + i);
        memcpy((void *)val, physicalAddress, min(sizeLeft, PGSIZE));
        val += PGSIZE;
        sizeLeft -= PGSIZE;
    }
    //memcpy((void *)physicalAddress, val, size);
    pthread_mutex_unlock(&mutex);
    //printf("the number %ld is stored in physical address %lx\n", *physicalAddress, (unsigned long int)physicalAddress);
    return;
    //pte_t *physicalAddress = translate(NULL, va);
    //memcpy((void *)val, physicalAddress, size);
    //printf("the number %ld is in val", *val);

}


/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
     * matrix accessed. Similar to the code in test.c, you will use get_value() to
     * load each element and perform multiplication. Take a look at test.c! In addition to 
     * getting the values from two matrices, you will perform multiplication and 
     * store the result to the "answer array"
     */
    int x, y, val_size = sizeof(int);
    int i, j, k;
    for (i = 0; i < size; i++) {
        for(j = 0; j < size; j++) {
            unsigned int a, b, c = 0;
            for (k = 0; k < size; k++) {
                int address_a = (unsigned int)mat1 + ((i * size * sizeof(int))) + (k * sizeof(int));
                int address_b = (unsigned int)mat2 + ((k * size * sizeof(int))) + (j * sizeof(int));
                get_value( (void *)address_a, &a, sizeof(int));
                get_value( (void *)address_b, &b, sizeof(int));
                // printf("Values at the index: %d, %d, %d, %d, %d\n", 
                //     a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            // printf("This is the c: %d, address: %x!\n", c, address_c);
            put_value((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}

// Helper Functions
static void set_bit_at_index(char *bitmap, int index)
{
    //Implement your code here	
    int byteIndex = index / 8;
    int offset = index % 8;
    unsigned char mask = 1 << offset;
    bitmap[byteIndex] |= mask;
    //printf("bitmap index %d", bitmap[byteIndex]);
    return;
}

static void free_bit_at_index(char *bitmap, int index)
{
    //Implement your code here	
    int byteIndex = index / 8;
    int offset = index % 8;
    unsigned char mask = 1 << offset;
    bitmap[byteIndex] &= ~mask;
    //printf("bitmap index %d", bitmap[byteIndex]);
    return;
}


/* 
 * GETTING A BIT AT AN INDEX 
 */
static int get_bit_at_index(char *bitmap, int index)
{
    //Get to the location in the character bitmap array
    //Implement your code here
    int byteIndex = index / 8;
    int offset = index % 8;
    char correctChar = bitmap[byteIndex] >> offset;
    return correctChar % 2;
}

// Gets the address of the next available physical address
void *get_next_avail_p(){
    int i;
    for (i = 0; i < totalPhysicalPages; i ++){
        if (get_bit_at_index(physicalPageTracker, i) == 0){
            set_bit_at_index(physicalPageTracker, i);
            return (void *)(i * PGSIZE) + ((unsigned long int)physicalMemory);
        }
    }
    // no pages left in physical memory
    exit(1);
    fprintf(stderr,"No pages left in physical memory\n");
    return (void *)-1;
}

int min(int a, int b) {
  if (a < b) {
    return a;
  } else {
    return b;
  }
}

