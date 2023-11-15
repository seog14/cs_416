#include "my_vm.h"

pde_t **page_directory;
void *physicalMemory;
char *virtualPageTracker;
char *physicalPageTracker;
struct tlb *tlbStore[TLB_ENTRIES];
int offsetBits;
int totalBits;
int PTEBits;
int PDEBits;
int totalVirtualPages = (MAX_MEMSIZE / PGSIZE);
int totalPhysicalPages = (MEMSIZE / PGSIZE);

/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating
    physicalMemory = malloc(MEMSIZE);
    page_directory = &physicalMemory;

    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
    virtualPageTracker = malloc(totalVirtualPages/8);
    memset(virtualPageTracker, 0, totalVirtualPages/8);
    physicalPageTracker = malloc(totalPhysicalPages/8);
    memset(physicalPageTracker, 0, totalPhysicalPages/8);

    //First physical page is used for page directory
    set_bit_at_index(physicalPageTracker, 0);

    // TLB Store
    //tlbStore = malloc(sizeof(struct tlb) * TLB_ENTRIES);

    //global
    offsetBits = (int)log(PGSIZE)/log(2);
    totalBits = (int)log(MAX_MEMSIZE)/log(2);
    PTEBits = (int)log(PGSIZE/sizeof(pte_t))/log(2);
    PDEBits = totalBits - offsetBits - PTEBits;
}


/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int
add_TLB(void *va, void *pa)
{

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */

    return -1;
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t *
check_TLB(void *va) {

    /* Part 2: TLB lookup code here */



   /*This function should return a pte_t pointer*/
}


/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void
print_TLB_missrate()
{
    double miss_rate = 0;	

    /*Part 2 Code here to calculate and print the TLB miss rate*/




    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t *translate(pde_t **pgdir, void *va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */

    /*if (check_TLB(va) != NULL){
        return check_TLB(va) + offset;
    }*/
    int PDEIndex = (unsigned int)va >> (offsetBits + PTEBits);
    int PTEIndex = ((unsigned int)va >> offsetBits) & ((1 << PTEBits) - 1);
    int offset = (unsigned int)va & ((1 << offsetBits) - 1);
    //add_TLB(va, page_directory[PDEIndex][PTEIndex]);
    //If translation not successful, then return NULL
    return (pgdir[PDEIndex])[PTEIndex] + offset;
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int
page_map(pde_t **pgdir, void *va, void *pa)
{
    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */

    // check if physical page is allocated already
    int physicalPage = (unsigned int)pa >> (offsetBits);
    if (get_bit_at_index(physicalPageTracker, physicalPage) == 1){
        return -1;
    }
    // check if virtual page is allocated already
    int virtualPage = (unsigned int)va >> (offsetBits);
    if (get_bit_at_index(virtualPageTracker, virtualPage) == 1){
        return -1;
    }

    // map the virtual address to the physical address
    int PDEIndex = (unsigned int)va >> (offsetBits + PTEBits);
    int PTEIndex = ((unsigned int)va >> offsetBits) & ((1 << PTEBits) - 1);
    if (pgdir[PDEIndex] == NULL){
        // allocate a new page for the new page table and add it to page directory
        // virtual page
        get_next_avail(1);
        // get next available physical page
        pde_t pageAddress = (pde_t)((uintptr_t)get_next_avail_p() << offsetBits);
        pgdir[PDEIndex] = pageAddress;
    }
    int offset = (unsigned int)va & ((1 << offsetBits) - 1);
    pgdir[PDEIndex][PTEIndex] = (pte_t *)((uintptr_t)pa - (uintptr_t)(void *)offset);
    return 0;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
 
    //Use virtual address bitmap to find the next free page
    int i;
    for (i = 0; i < totalVirtualPages; i ++){
        if (get_bit_at_index(virtualPageTracker, i) == 0){
            // check if we have contiguous space for the num_pages
            // may not be necessary?
            /*for (j = 0; j < num_pages; j++){
                if (get_bit_at_index(virtualPageTracker, i + j) == 1){
                    break;
                }
            }*/
            set_bit_at_index(virtualPageTracker, i);
            return i;
        }
    }
    // no pages left
    return -1;
}


/* Function responsible for allocating pages
and used by the benchmark
*/
void *t_malloc(unsigned int num_bytes) {

    /* 
     * HINT: If the physical memory is not yet initialized, then allocate and initialize.
     */
    if (physicalMemory == NULL){
        set_physical_mem();
    }

   /* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used.
    */
   int totalNeededPages = num_bytes/PGSIZE + 1;
   int VPN = get_next_avail(totalNeededPages);
    return NULL;
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
    
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
 * The function returns 0 if the put is successfull and -1 otherwise.
*/
int put_value(void *va, void *val, int size) {

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger 
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */


    /*return -1 if put_value failed and 0 if put is successfull*/

}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */


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


/* 
 * Function 3: GETTING A BIT AT AN INDEX 
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

void *get_next_avail_p(){
    int i;
    for (i = 0; i < totalPhysicalPages; i ++){
        if (get_bit_at_index(physicalPageTracker, i) == 0){
            set_bit_at_index(physicalPageTracker, i);
            return i;
        }
    }
    // no pages left in physical memory
    return -1;
}

