// memory.c - Memory management
//

#ifndef TRACE
#ifdef MEMORY_TRACE
#define TRACE
#endif
#endif

#ifndef DEBUG
#ifdef MEMORY_DEBUG
#define DEBUG
#endif
#endif

#include "config.h"

#include "memory.h"
#include "console.h"
#include "halt.h"
#include "heap.h"
#include "csr.h"
#include "string.h"
#include "error.h"
#include "thread.h"
#include "process.h"

#include <stdint.h>

// EXPORTED VARIABLE DEFINITIONS
//

char memory_initialized = 0;
uintptr_t main_mtag;

// IMPORTED VARIABLE DECLARATIONS
//

// The following are provided by the linker (kernel.ld)

extern char _kimg_start[];
extern char _kimg_text_start[];
extern char _kimg_text_end[];
extern char _kimg_rodata_start[];
extern char _kimg_rodata_end[];
extern char _kimg_data_start[];
extern char _kimg_data_end[];
extern char _kimg_end[];

// INTERNAL TYPE DEFINITIONS
//

union linked_page {
    union linked_page * next;
    char padding[PAGE_SIZE];
};

struct pte {
    uint64_t flags:8;
    uint64_t rsw:2;
    uint64_t ppn:44;
    uint64_t reserved:7;
    uint64_t pbmt:2;
    uint64_t n:1;
};

// INTERNAL MACRO DEFINITIONS
//

#define VPN2(vma) (((vma) >> (9+9+12)) & 0x1FF)
#define VPN1(vma) (((vma) >> (9+12)) & 0x1FF)
#define VPN0(vma) (((vma) >> 12) & 0x1FF)
#define MIN(a,b) (((a)<(b))?(a):(b))

// INTERNAL FUNCTION DECLARATIONS
//

static inline int wellformed_vma(uintptr_t vma);
static inline int wellformed_vptr(const void * vp);
static inline int aligned_addr(uintptr_t vma, size_t blksz);
static inline int aligned_ptr(const void * p, size_t blksz);
static inline int aligned_size(size_t size, size_t blksz);

static inline uintptr_t active_space_mtag(void);
static inline struct pte * mtag_to_root(uintptr_t mtag);
static inline struct pte * active_space_root(void);

static inline void * pagenum_to_pageptr(uintptr_t n);
static inline uintptr_t pageptr_to_pagenum(const void * p);

static inline void * round_up_ptr(void * p, size_t blksz);
static inline uintptr_t round_up_addr(uintptr_t addr, size_t blksz);
static inline size_t round_up_size(size_t n, size_t blksz);
static inline void * round_down_ptr(void * p, size_t blksz);
static inline size_t round_down_size(size_t n, size_t blksz);
static inline uintptr_t round_down_addr(uintptr_t addr, size_t blksz);

static inline struct pte leaf_pte (
    const void * pptr, uint_fast8_t rwxug_flags);
static inline struct pte ptab_pte (
    const struct pte * ptab, uint_fast8_t g_flag);
static inline struct pte null_pte(void);

static inline void sfence_vma(void);

static int free_ptab(struct pte * ptab);
// INTERNAL GLOBAL VARIABLES
//

static union linked_page * free_list;

static struct pte main_pt2[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096)));
static struct pte main_pt1_0x80000[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096)));
static struct pte main_pt0_0x80000[PTE_CNT]
    __attribute__ ((section(".bss.pagetable"), aligned(4096)));

// EXPORTED VARIABLE DEFINITIONS
//

// EXPORTED FUNCTION DEFINITIONS
// 

/*
 * function: memory_init
 *
 * Input:
 * - None 
 *
 * Output:
 * - None
 *
 *
 * Description:
 * This function sets up the virtual-to-physical memory mapping for the system
 * during the early stages of the boot process. It configures the page tables
 * for different memory regions such as the MMIO region, kernel text, kernel data,
 * read-only data, and heap, as well as setting up mappings for free page pools.
 * Additionally, this function enables the paging mechanism and sets up the
 * memory region for the heap allocator.
 *
 *
 * Side Effects:
 * - Modifies the `main_pt1_0x80000` and `main_pt2` page tables, which affect
 *   the system's virtual-to-physical memory mappings.
 * - Potentially raises a panic if the kernel image is too large to fit within the
 *   allowed memory boundaries (2MB megapage limit).
 */

void memory_init(void) {
    const void * const text_start = _kimg_text_start;
    const void * const text_end = _kimg_text_end;
    const void * const rodata_start = _kimg_rodata_start;
    const void * const rodata_end = _kimg_rodata_end;
    const void * const data_start = _kimg_data_start;
    union linked_page * page;
    void * heap_start;
    void * heap_end;
    size_t page_cnt;
    uintptr_t pma;
    const void * pp;

    trace("%s()", __func__);

    assert (RAM_START == _kimg_start);

    kprintf("           RAM: [%p,%p): %zu MB\n",
        RAM_START, RAM_END, RAM_SIZE / 1024 / 1024);
    kprintf("  Kernel image: [%p,%p)\n", _kimg_start, _kimg_end);

    // Kernel must fit inside 2MB megapage (one level 1 PTE)
    
    if (MEGA_SIZE < _kimg_end - _kimg_start)
        panic("Kernel too large");

    // Initialize main page table with the following direct mapping:
    // 
    //         0 to RAM_START:           RW gigapages (MMIO region)
    // RAM_START to _kimg_end:           RX/R/RW pages based on kernel image
    // _kimg_end to RAM_START+MEGA_SIZE: RW pages (heap and free page pool)
    // RAM_START+MEGA_SIZE to RAM_END:   RW megapages (free page pool)
    //
    // RAM_START = 0x80000000
    // MEGA_SIZE = 2 MB
    // GIGA_SIZE = 1 GB
    
    // Identity mapping of two gigabytes (as two gigapage mappings)
    for (pma = 0; pma < RAM_START_PMA; pma += GIGA_SIZE)
        main_pt2[VPN2(pma)] = leaf_pte((void*)pma, PTE_R | PTE_W | PTE_G);
    
    // Third gigarange has a second-level page table
    main_pt2[VPN2(RAM_START_PMA)] = ptab_pte(main_pt1_0x80000, PTE_G);

    // First physical megarange of RAM is mapped as individual pages with
    // permissions based on kernel image region.

    main_pt1_0x80000[VPN1(RAM_START_PMA)] =
        ptab_pte(main_pt0_0x80000, PTE_G);
    kprintf("   Kernel text: [%p,%p)\n", text_start, text_end);
    kprintf(" Kernel rodata: [%p,%p)\n", rodata_start, rodata_end);
    kprintf("   Kernel data: [%p,%p)\n", data_start, _kimg_end);
    kprintf("Heap available: [%p,%p)\n", _kimg_end, RAM_START + MEGA_SIZE);
    for (pp = text_start; pp < text_end; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_X | PTE_G);
    }

    // Set up page table entries for the kernel read-only data section
    for (pp = rodata_start; pp < rodata_end; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_G);
    }

    // Set up page table entries for the kernel data section
    for (pp = data_start; pp < RAM_START + MEGA_SIZE; pp += PAGE_SIZE) {
        main_pt0_0x80000[VPN0((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_W | PTE_G);
    }

    // Remaining RAM mapped in 2MB megapages

    for (pp = RAM_START + MEGA_SIZE; pp < RAM_END; pp += MEGA_SIZE) {
        main_pt1_0x80000[VPN1((uintptr_t)pp)] =
            leaf_pte(pp, PTE_R | PTE_W | PTE_G);
    }

    // Enable paging. This part always makes me nervous.

    main_mtag =  // Sv39
        ((uintptr_t)RISCV_SATP_MODE_Sv39 << RISCV_SATP_MODE_shift) |
        pageptr_to_pagenum(main_pt2);
    
    csrw_satp(main_mtag);
    sfence_vma();

    // Give the memory between the end of the kernel image and the next page
    // boundary to the heap allocator, but make sure it is at least
    // HEAP_INIT_MIN bytes.

    heap_start = _kimg_end;
    heap_end = round_up_ptr(heap_start, PAGE_SIZE);
    if (heap_end - heap_start < HEAP_INIT_MIN) {
        heap_end += round_up_size (
            HEAP_INIT_MIN - (heap_end - heap_start), PAGE_SIZE);
    }

    if (RAM_END < heap_end)
        panic("Not enough memory");
    
    // Initialize heap memory manager

    heap_init(heap_start, heap_end);

    kprintf("Heap allocator: [%p,%p): %zu KB free\n",
        heap_start, heap_end, (heap_end - heap_start) / 1024);

    free_list = heap_end; // heap_end is page aligned
    page_cnt = (RAM_END - heap_end) / PAGE_SIZE;

    kprintf("Page allocator: [%p,%p): %lu pages free\n",
        free_list, RAM_END, page_cnt);

    // Put free pages on the free page list
    // TODO: FIXME implement this (must work with your implementation of
    // memory_alloc_page and memory_free_page).
    ((union linked_page *)free_list)->next = NULL;
    for(size_t i = 1; i < page_cnt; i++) {
        page = (union linked_page *)(heap_end + i * PAGE_SIZE);
        page->next = free_list;
        free_list = page;
    }
    
    // Allow supervisor to access user memory. We could be more precise by only
    // enabling it when we are accessing user memory, and disable it at other
    // times to catch bugs.

    csrs_sstatus(RISCV_SSTATUS_SUM);

    memory_initialized = 1;
}
uintptr_t memory_space_create(uint_fast16_t asid){
    // input:
    //  asid: the address space identifier
    //
    // output:
    //  Returns a memory space tag (type uintptr_t) that may be used to refer to the
    //
    // Creates a new memory space and makes it the currently active space. Returns a
    // memory space tag (type uintptr_t) that may be used to refer to the memory
    // space. The created memory space contains the same identity mapping of MMIO
    // address space and RAM as the main memory space. This function never fails; if
    // there are not enough physical memory pages to create the new memory space, it
    // panics.

    trace("%s(%u)", __func__, asid);
    struct pte * root = active_space_root();
    struct pte * new_root = memory_alloc_page();
    memset(new_root, 0, PAGE_SIZE);

    // Copy the main memory space root page table to the new memory space
    ////////////////////////////////////////
    for (size_t i = 0; i < PTE_CNT; i++) {
        new_root[i] = root[i];
    }
    // setup satp
    uintptr_t mtag = ((uintptr_t)RISCV_SATP_MODE_Sv39 << RISCV_SATP_MODE_shift) |
                    (uintptr_t)asid<<RISCV_SATP_ASID_shift | pageptr_to_pagenum(new_root);
    csrw_satp(mtag);
    sfence_vma();
    
    return mtag;
}
void memory_space_reclaim(void){
    // input: none
    //
    // output: none
    //
    // side effect: 
    //
    // Switch the active memory space to the main memory space and
    // reclaims the memory space that was active on entry. 
    // All physical pages mapped by the memory space that are not
    // part of the global mapping are reclaimed.

    trace("%s()", __func__);
    struct pte * old_root = active_space_root();
    struct pte * ptab;
    //uintptr_t  for vpn2, vpn1, vpn0;
    uintptr_t pma;
    size_t i, j, k;

    // Reclaim pages from the old memory space
    // make first level - ppn2
    for (i = 0; i < PTE_CNT; i++) {
        if (((old_root[i].flags & PTE_V) != 0 ) && ((PTE_G & old_root[i].flags) == 0)) {
            ptab = (struct pte *)pagenum_to_pageptr(old_root[i].ppn);
            // make second level - ppn1
            for (j = 0; j < PTE_CNT; j++) {
                if (((ptab[j].flags & PTE_V) != 0 ) && ((PTE_G & ptab[j].flags) == 0)) {
                    struct pte * ptab1 = (struct pte *)pagenum_to_pageptr(ptab[j].ppn);
                    // make third level - ppn0 -> pma
                    for (k = 0; k < PTE_CNT; k++) {
                        if (((ptab1[k].flags & PTE_V) != 0 ) && ((ptab1[k].flags & PTE_G) == 0)) {
                            pma = (uintptr_t)pagenum_to_pageptr(ptab1[k].ppn);
                            if (((uint64_t)pma >= (uint64_t)RAM_START) && ((uint64_t)pma < (uint64_t)RAM_END)) {
                                // free current page that pte is pointing to
                                memory_free_page((void *)pma);
                                ptab1[k] = null_pte();
                            }
                        }
                    }
                    if(free_ptab(ptab1) == 1)
                        ptab[j] = null_pte();
                }
            }
            if(free_ptab(ptab) == 1)
                old_root[i] = null_pte();
        }
    }
    // Switch to main memory space
    csrw_satp(main_mtag);
    sfence_vma();
}

void * memory_alloc_page(void) {
    // input: none
    //
    // output: 
    //  Returns the virtual address of the direct mapped page as a void*
    //
    // side effect: 
    // Allocate a physical page of memory using the free pages list
    // Panics if there are no free pages available

    trace("%s()", __func__);
    // Get the first free page
    union linked_page * page = free_list;
    if (page == NULL) {
        panic("Out of memory");
    }
    // Update the free list to the next page
    free_list = page->next;
    return (void *)page;
}

void memory_free_page(void * pp) {
    // input: 
    //      pp: a pointer to a physical page of memory
    //
    // output: none
    //
    // side effect: 
    // returns a physical memory page to the physical page allocator
    // the page must have been previously allocated by memory_alloc_page

    trace("%s(%p)", __func__, pp);
    union linked_page * page = (union linked_page *)pp;
    // Set the next pointer of the page to the current free list
    page->next = free_list;
    free_list = page;
}

void* 
memory_alloc_and_map_page(uintptr_t vma, uint_fast8_t rwxug_flags){
    // input: 
    //  - v_address: the virtual address of the page to map
    //  - rwxug_flags: an OR of the PTE flags
    //
    // output: 
    //  - return a pointer to the mapped virtual page, i.e., (void*)v_address
    //
    // side effect: 
    //  - panics if the request cannot be satisfied

    trace("%s(%p, %x)", __func__, vma, rwxug_flags);
    vma = round_down_addr(vma, PAGE_SIZE);
    struct pte * root = active_space_root();
    // create VPN for 3 levels
    uintptr_t vpn2 = VPN2(vma);
    uintptr_t vpn1 = VPN1(vma);
    uintptr_t vpn0 = VPN0(vma);
    struct pte * ptab2 = root;
    struct pte * ptab1;
    struct pte * ptab0;
    //assert(((uint64_t)vma < (uint64_t)RAM_END) && ((uint64_t)vma >= (uint64_t)RAM_START));
    // direct mapping if vma is in the kernel space, else allocate a new page
    void *pp;
    if((uint64_t)vma < (uint64_t)(RAM_START + MEGA_SIZE)){
        pp = (void *)vma;
    }else{
        pp = memory_alloc_page();
    }
    // create and set ptab 2
    if (!(ptab2[vpn2].flags & PTE_V)) {
        ptab1 = memory_alloc_page();
        memset(ptab1, 0, PAGE_SIZE);
        ptab2[vpn2].flags = PTE_V;
        ptab2[vpn2].ppn = pageptr_to_pagenum(ptab1);
    }
    ptab1 = (struct pte *)pagenum_to_pageptr(ptab2[vpn2].ppn);
    
    // create and set ptab 1
    if (!(ptab1->flags & PTE_V)) {
        ptab0 = memory_alloc_page();
        memset(ptab0, 0, PAGE_SIZE);
        ptab1[vpn1].flags = PTE_V;
        ptab1[vpn1].ppn = pageptr_to_pagenum(ptab0);
    }
    ptab0 = (struct pte *)pagenum_to_pageptr(ptab1[vpn1].ppn);
    ptab0[vpn0] = leaf_pte(pp, rwxug_flags);
    sfence_vma();
    return (void*)vma;
}

void * memory_alloc_and_map_range(uintptr_t vma, size_t size, uint_fast8_t rwxug_flags){
    // input: 
    //  v_address: the virtual address of the page to map
    //  size: the size of the range to map
    //  rwxug_flags: an OR of the PTE flags
    //
    // output: 
    //  return a pointer to the mapped virtual page, i.e., (void*)v_address
    //
    // side effect: 
    //  panics if the request cannot be satisfied

    trace("%s(%p, %zu, %x)", __func__, vma, size, rwxug_flags);
    // allocates and maps memory pages for a specified range of virtual memory addresses 
    // based on the provided size and permissions
    void * vp = (void *)vma;
    void * end = vp + size;
    while (vp < end) {
        vp = memory_alloc_and_map_page((uintptr_t)vp, rwxug_flags);
        vp += PAGE_SIZE;
    }
    return (void *)vma;
}

void memory_set_page_flags(const void *vp, uint8_t rwxug_flags){
    // input: 
    //  vp: a pointer to a virtual page
    //  rwxug_flags: an OR of the PTE flags
    //
    // output: none
    //
    // side effect: 
    //  changes the PTE flags for the page at the virtual address vp

    trace("%s(%p, %x)", __func__, vp, rwxug_flags);
    const void * rounded_vp = (void *)round_down_addr((uintptr_t)vp, PAGE_SIZE);
    struct pte * root = active_space_root();
    // calculate vpn for these 3 levles
    uintptr_t vpn2 = VPN2((uintptr_t)rounded_vp);
    uintptr_t vpn1 = VPN1((uintptr_t)rounded_vp);
    uintptr_t vpn0 = VPN0((uintptr_t)rounded_vp);
    // set pointers of pte for 3 levels
    struct pte * ptab2 = root;
    struct pte * ptab1 = (struct pte *)pagenum_to_pageptr(ptab2[vpn2].ppn);
    struct pte * ptab0 = (struct pte *)pagenum_to_pageptr(ptab1[vpn1].ppn);
    ptab0[vpn0].flags = rwxug_flags| PTE_A | PTE_D | PTE_V;
    sfence_vma();
}

void memory_set_range_flags(const void * vp, size_t size, uint_fast8_t rwxug_flags){
    // input: 
    //  vp: a pointer to the start of a range of virtual pages
    //  size: the size of the range
    //  rwxug_flags: an OR of the PTE flags
    //
    // output: none
    //
    // side effect: 
    //  changes the PTE flags for all pages in the mapped range

    trace("%s(%p, %zu, %x)", __func__, vp, size, rwxug_flags);
    // set specified flags for each page in the given memory range
    const void * end = vp + size;
    while (vp < end) {
        memory_set_page_flags(vp, rwxug_flags);
        vp += PAGE_SIZE;
    }
}

void memory_unmap_and_free_user(){
    // input: none
    //
    // output: none
    //
    // side effect: 
    //  unmaps and frees all pages with the U bit set in the PTE flags

    trace("%s()", __func__);
    struct pte * root = active_space_root();
    // uintptr_t vpn2, vpn1, vpn0;
    struct pte * ptab2, * ptab1;
    uintptr_t pma;
    size_t i, j, k;
    // set first level - ppn2
    for (i = 0; i < PTE_CNT; i++) {
        if (((root[i].flags & PTE_V) != 0 ) && ((PTE_G & root[i].flags) == 0)) {
            ptab2 = (struct pte *)pagenum_to_pageptr(root[i].ppn);
            // set second level - ppn1
            for (j = 0; j < PTE_CNT; j++) {
                if (((ptab2[j].flags & PTE_V) != 0 ) && ((ptab2[j].flags & PTE_G) == 0)) {
                    ptab1 = (struct pte *)pagenum_to_pageptr(ptab2[j].ppn);
                    // set third level - ppn0 -> pma
                    for (k = 0; k < PTE_CNT; k++) {
                        if (((ptab1[k].flags & PTE_V) != 0 ) && ((ptab1[k].flags & PTE_G) == 0) && ((ptab1[k].flags & PTE_U) != 0)) {
                            pma = (uintptr_t)pagenum_to_pageptr(ptab1[k].ppn);
                            if (((uint64_t)pma >= (uint64_t)RAM_START) && ((uint64_t)pma < (uint64_t)RAM_END)) {
                                memory_free_page((void *)pma);
                                ptab1[k] = null_pte();
                            }
                        }
                    }
                    if(free_ptab(ptab1) == 1)
                        ptab2[j] = null_pte();
                }
            }
            if(free_ptab(ptab2) == 1)
                root[i] = null_pte();
        }
    }
}

int memory_validate_vptr_len (const void * vp, size_t len, uint_fast8_t rwxug_flags){
    // input:
    //  vp: a pointer to a virtual address
    //  len: the length of the string to validate
    //  rwxug_flags: an OR of the PTE flags
    //
    // output:
    //  Ensure that the virtual pointer provided (vp) points to a mapped region of size len and
    //  has at least the specified flags.
    //  Returns 0 if and only if every virtual page containing the specified virtual 
    //  address range is mapped with the specified flags
    //
    // side effect: none

    trace("%s(%p, %zu, %x)", __func__, vp, len, rwxug_flags);
    const void * crt = vp;
    struct pte * root = active_space_root();
    while (crt < vp + len) {
        // set vpn for three levels
        uintptr_t vpn2 = VPN2((uintptr_t)crt);
        uintptr_t vpn1 = VPN1((uintptr_t)crt);
        uintptr_t vpn0 = VPN0((uintptr_t)crt);
        // set pointers of pte for 3 levels
        struct pte * ptab2 = root;
        struct pte * ptab1 = (struct pte *)pagenum_to_pageptr(ptab2[vpn2].ppn);
        struct pte * ptab0 = (struct pte *)pagenum_to_pageptr(ptab1[vpn1].ppn);
        if (!(ptab0[vpn0].flags & rwxug_flags)) {
            return -1;
        }
        crt += PAGE_SIZE;
    }
    return 0;
}

int memory_validate_vstr (const char * vs, uint_fast8_t ug_flags){
    // input: 
    //  vs: a virtual pointer to a null-terminated string
    //  ug_flags: an OR of the PTE flags
    //
    // output: 
    //  Ensure that the virtual pointer provided (vs) contains a NUL-terminated string.
    //  returns 1 if and only if the virtual pointer points to a mapped range containing a null-terminated string
    //
    // side effect: none

    trace("%S(%p, %x)", __func__, vs, ug_flags);
    const void * crt = vs;
    char * pp;

    // while loop used to ensure provided virtual pointers
    do{
        if(memory_validate_vptr_len(crt, 1, ug_flags) != 0)
            return 0;
        pp = (char *)crt;
        crt += 1;
    }while(strcmp(pp, "\0") != 0);
    return 0;
}

void memory_handle_page_fault(const void * vptr){
    // input:
    //  vptr: a pointer to the virtual address that caused the page fault
    //
    // output: none
    //
    // side effect:
    //  May choose to panic or to allocate a new page, 
    //  depending on if the address is within the user region.

    trace("%s(%p)", __func__, vptr);
    if (((uintptr_t)vptr >= USER_START_VMA) && ((uintptr_t)vptr < USER_END_VMA) 
        && wellformed_vptr(vptr)) {
        const void* vptr1 = round_down_ptr((void*)vptr, PAGE_SIZE);
        // set pointer of pte for 3 levels
        struct pte * ptab2 = active_space_root();
        struct pte * ptab1;
        struct pte * ptab0;
        // set vpn for 3 levels
        uintptr_t vpn2 = VPN2((uintptr_t)vptr1);
        uintptr_t vpn1 = VPN1((uintptr_t)vptr1);
        uintptr_t vpn0 = VPN0((uintptr_t)vptr1);
        void* pp = memory_alloc_page();
        // judge level 2 by using V bit
        if(!(ptab2[vpn2].flags & PTE_V)){
            // allocate memory for table
            ptab1 = memory_alloc_page();
            memset(ptab1, 0, PAGE_SIZE);
            ptab2[vpn2].flags = PTE_V;
            ptab2[vpn2].ppn = pageptr_to_pagenum(ptab1);
        }
        ptab1 = (struct pte *)pagenum_to_pageptr(ptab2[vpn2].ppn);

        // judge level 1 by using V bit
        if(!(ptab1[vpn1].flags & PTE_V)){
            // allocate memory for table
            ptab0 = memory_alloc_page();
            memset(ptab0, 0, PAGE_SIZE);
            ptab1[vpn1].flags = PTE_V;
            ptab1[vpn1].ppn = pageptr_to_pagenum(ptab0);
        }
        ptab0 = (struct pte *)pagenum_to_pageptr(ptab1[vpn1].ppn);
        ptab0[vpn0] = leaf_pte(pp, PTE_R | PTE_W | PTE_U | PTE_V);
        sfence_vma();

    } else {
        kprintf("Page fault at %p, process exit.\n", vptr);
        process_exit();
    }
}
// INTERNAL FUNCTION DEFINITIONS
//
static int free_ptab(struct pte * ptab) {
    // input: 
    //  ptab: a pointer to a page table
    //  cnt: the number of entries in the page table
    //
    // output: 
    //  1 on success, 0 on failure
    //
    // side effect: 
    //  frees all pages in the page table

    trace("%s(%p, %zu)", __func__, ptab);
    int k;
    // judge whether to succeed for free
    for(k = 0; k < PTE_CNT; k++) {
        if(ptab[k].flags & PTE_V) {
            return 0;
        }
    }
    memory_free_page((void *)ptab);
    return 1;
}
static inline int wellformed_vma(uintptr_t vma) {
    // Address bits 63:38 must be all 0 or all 1
    uintptr_t const bits = (intptr_t)vma >> 38;
    return (!bits || !(bits+1));
}

static inline int wellformed_vptr(const void * vp) {
    return wellformed_vma((uintptr_t)vp);
}

static inline int aligned_addr(uintptr_t vma, size_t blksz) {
    return ((vma % blksz) == 0);
}

static inline int aligned_ptr(const void * p, size_t blksz) {
    return (aligned_addr((uintptr_t)p, blksz));
}

static inline int aligned_size(size_t size, size_t blksz) {
    return ((size % blksz) == 0);
}

static inline uintptr_t active_space_mtag(void) {
    return csrr_satp();
}

static inline struct pte * mtag_to_root(uintptr_t mtag) {
    return (struct pte *)((mtag << 20) >> 8);
}


static inline struct pte * active_space_root(void) {
    return mtag_to_root(active_space_mtag());
}

static inline void * pagenum_to_pageptr(uintptr_t n) {
    return (void*)(n << PAGE_ORDER);
}

static inline uintptr_t pageptr_to_pagenum(const void * p) {
    return (uintptr_t)p >> PAGE_ORDER;
}

static inline void * round_up_ptr(void * p, size_t blksz) {
    return (void*)((uintptr_t)(p + blksz-1) / blksz * blksz);
}

static inline uintptr_t round_up_addr(uintptr_t addr, size_t blksz) {
    return ((addr + blksz-1) / blksz * blksz);
}

static inline size_t round_up_size(size_t n, size_t blksz) {
    return (n + blksz-1) / blksz * blksz;
}

static inline void * round_down_ptr(void * p, size_t blksz) {
    return (void*)((uintptr_t)p / blksz * blksz);
}

static inline size_t round_down_size(size_t n, size_t blksz) {
    return n / blksz * blksz;
}

static inline uintptr_t round_down_addr(uintptr_t addr, size_t blksz) {
    return (addr / blksz * blksz);
}

static inline struct pte leaf_pte (
    const void * pptr, uint_fast8_t rwxug_flags)
{
    return (struct pte) {
        .flags = rwxug_flags | PTE_A | PTE_D | PTE_V,
        .ppn = pageptr_to_pagenum(pptr)
    };
}

static inline struct pte ptab_pte (
    const struct pte * ptab, uint_fast8_t g_flag)
{
    return (struct pte) {
        .flags = g_flag | PTE_V,
        .ppn = pageptr_to_pagenum(ptab)
    };
}

static inline struct pte null_pte(void) {
    return (struct pte) { };
}

static inline void sfence_vma(void) {
    asm inline ("sfence.vma" ::: "memory");
}
