#define ADDR_SIZE 16
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct pcb *current;
unsigned short *pdbr;
char *pmem, *swaps;
int pfnum, sfnum;
char *pfnum_freelist, *sfnum_freelist;

void ku_dump_pmem(void);
void ku_dump_swap(void);


typedef struct pcb{
	unsigned short pid;
	FILE *fd;
	unsigned short *pgdir;
	/* Add more fields as needed */
    struct pcb* next;
} pcb;

typedef struct page_frame {
    int page_frame_number;
    struct page_frame *next;
} PageFrame;

PageFrame *queue_front = NULL;
PageFrame *queue_rear = NULL;

pcb* pcb_head = NULL;

/**
 *
 * @return Whether the pcb_head and next pcb is Null
 */
int is_empty() {
    if (pcb_head == NULL || pcb_head->next == NULL)
        return 1;
    return 0;
}

/* -------------------------------------------------------------------------------------- */
/**
 * test codes with printf
 */

// for linked list test
void print_pcb_list() {
    pcb* current = pcb_head->next;
    int count = 0;

    printf("PCB List:\n");
    while (current != NULL) {
        printf("PCB #%d\n", count);
        printf("  PID: %d\n", current->pid);
        printf("  File Descriptor: %p\n", (void*)current->fd);
        printf("  Page Directory: %p\n", (void*)current->pgdir);
        printf("\n");

        current = current->next;
        count++;
    }
}

void print_process_info(pcb* process) {
    printf("\n");
    printf("Selected process:\n");
    printf("  PID: %d\n", process->pid);
    printf("  File Descriptor: %p\n", (void*)process->fd);
    printf("  Page Directory: %p\n", (void*)process->pgdir);
}

void print_bitmap(char *bitmap, int size, const char *bitmap_name) {
    int bitmap_size = (size + 7) / 8;
    int group_size = 8;

    printf("%s Bitmap:\n", bitmap_name);
    for (int i = 0; i < bitmap_size; i++) {
        printf("Byte %2d: ", i);
        for (int j = 0; j < 8; j++) {
            if (i * 8 + j < size) {
                printf("%d", (bitmap[i] >> j) & 1);
            } else {
                printf("_");
            }
        }
        printf("\n");
    }
    printf("\n");
}
/* -------------------------------------------------------------------------------------- */

/**
 * - free list initialization with bitmap
 */
void ku_freelist_init(){
    int pfnum_bitmap_size = (pfnum + 7) / 8;
    int sfnum_bitmap_size = (sfnum + 7) / 8;

    pfnum_freelist = (char*)malloc(pfnum_bitmap_size);
    sfnum_freelist = (char*)malloc(sfnum_bitmap_size);

    memset(pfnum_freelist, 0, pfnum_bitmap_size);
    memset(sfnum_freelist, 0, sfnum_bitmap_size);

    // print bitmap for test
//    print_bitmap(pfnum_freelist, pfnum, "Physical Memory Frame");
//    print_bitmap(sfnum_freelist, sfnum, "Swap Space Frame");
}

// Enqueue a page frame number into the queue
void enqueue_page_frame(int page_frame_number) {
    PageFrame *new_page_frame = (PageFrame *)malloc(sizeof(PageFrame));
    new_page_frame->page_frame_number = page_frame_number;
    new_page_frame->next = NULL;

    if (queue_rear == NULL) {
        queue_front = new_page_frame;
        queue_rear = new_page_frame;
    } else {
        queue_rear->next = new_page_frame;
        queue_rear = new_page_frame;
    }
}

// Dequeue the oldest page frame number from the queue
int dequeue_page_frame() {
    if (queue_front == NULL) {
        return -1;
    }

    PageFrame *dequeued_page_frame = queue_front;
    int page_frame_number = dequeued_page_frame->page_frame_number;

    queue_front = queue_front->next;
    if (queue_front == NULL) {
        queue_rear = NULL;
    }

    free(dequeued_page_frame);
    return page_frame_number;
}

int find_free_swap_frame() {
    int sfnum_bitmap_size = (sfnum + 7) / 8;
    int swap_frame_number = -1;

    for (int i = 0; i < sfnum_bitmap_size; i++) {
        if (sfnum_freelist[i] != (char)0xFF) {
            for (int j = 0; j < 8; j++) {
                if ((sfnum_freelist[i] & (1 << j)) == 0) {
                    swap_frame_number = i * 8 + j;
                    break;
                }
            }
            if (swap_frame_number != -1) {
                break;
            }
        }
    }

    return swap_frame_number;
}

/**
 *
 * @param virtual_address
 * @return pte
 */
unsigned short* get_pte(unsigned short virtual_address) {
    unsigned short *pde = pdbr + ((virtual_address >> 10) & 0x3F);
    printf("get_pte| pde: %p\n", pde);
    if ((*pde & 1) == 0) {
        // Page directory entry is not valid
        return NULL;
    }

    unsigned short *page_table = (unsigned short *)(unsigned long)(*pde & 0xFFFFF000);
    unsigned short *pte = page_table + ((virtual_address >> 4) & 0x3FF);
    return pte;
}

// Perform page replacement using the FIFO algorithm
int perform_page_replacement_fifo() {
    int evicted_page_frame_number = dequeue_page_frame();
    // No page frames in the queue, indicate an error
    if (evicted_page_frame_number == -1) {
        return -1;
    }

    // Perform swap-out of the evicted page frame
    int swap_frame_number = find_free_swap_frame();
    // No available swap frame, indicate an error
    if (swap_frame_number == -1) {
        return -1;
    }

    // Copy the contents of the evicted page frame to the swap frame
    memcpy(swaps + (swap_frame_number * 4096), pmem + (evicted_page_frame_number * 4096), 4096);

    // Update the PTE of the evicted page
    unsigned short *pte = get_pte(evicted_page_frame_number);
    *pte = (*pte & 0xFFFE) | (1 << 15); // Set the swapped-out bit
    *pte = (*pte & 0x1FFF) | (swap_frame_number << 13); // Set the swap frame number

    // Update the free lists
    pfnum_freelist[evicted_page_frame_number / 8] &= ~(1 << (evicted_page_frame_number % 8)); // Mark the page frame as free
    sfnum_freelist[swap_frame_number / 8] |= (1 << (swap_frame_number % 8)); // Mark the swap frame as used

    return evicted_page_frame_number;
}


void init_page_directory(pcb* new_pcb) {
    unsigned short* page_directory = (unsigned short*)malloc(ADDR_SIZE * sizeof(unsigned short));
    memset(page_directory, 0, ADDR_SIZE * sizeof(unsigned short));
    new_pcb->pgdir = page_directory;
}

/**
 *
 * @param new_pid new process id
 * @param file_descriptor process file file_descriptor
 * @return
 */
int insert_pcb_at_head(unsigned short new_pid, const char *file_descriptor){
//    printf("from insert_pcb_at_head | pid: %d, FileDescriptor: %s\n", new_pid, file_directory);
    pcb* new_pcb = (pcb*)malloc(sizeof(pcb));
    if (new_pcb == NULL) {
        return 0;
    }

    // set pcb data
    new_pcb -> pid = new_pid;
    new_pcb -> fd = fopen(file_descriptor, "r");

    // page directory init
    init_page_directory(new_pcb);

    if (is_empty()) {
        pcb_head->next = new_pcb;
    } else{
        new_pcb->next = pcb_head -> next;
        pcb_head->next = new_pcb;
    }
    return 1;
}


//  and page directories & allocate and zero-fills page directories
/**
 *  Initializes PCBs and page directories for n processes
 *  - implemented by Linked list
 *  - Allocates page Directory
 *      - Zero-filling
 *          : Will be mapped one-by-one by the page fault handler(i.e, on-demand paging)
 *      - Page Directory is not swapped-out
 * @param argc
 * @param argv
 * @return
 */
int ku_proc_init(int argc, char *argv[]){
    FILE *input_file;
    char line[100];
    char *token;
    const char *delimiter = " ";
    int pid;
    char fd[100];
    pcb_head = (pcb*)malloc(sizeof(pcb));
    pcb_head->next = NULL;

    input_file = fopen("input.txt", "r");
    if(input_file == NULL){
        printf("file read error");
        return 0;
    }

    while(fgets(line, sizeof(line), input_file)){
        // 개행 문자 제거
        line[strcspn(line, "\n")] = '\0';

        token = strtok(line, delimiter);
        if (token != NULL) {
            pid = atoi(token);
            token = strtok(NULL, delimiter);
            if (token != NULL) {
                strcpy(fd, token);
            }
        }
        // initialize PCBs & page directories
        // and initialized with linked list
        insert_pcb_at_head(pid, fd);
//        printf("pid: %d, FileDirectory: %s from pcb_init()\n", pid, fd);
    }
    fclose(input_file);
    print_pcb_list();
    return 1;

    // check page directories are not swapped out
}

/**
 * - Selects the next process in a round-robin manner(starts from PID 0)
 * - Updates current and pdbr(page directory base register)
 * @param arg1 current process ID(10 for the first call)
 * @return value 0: success
 *               1: error(no response)
 */
int ku_scheduler(unsigned short arg1){
    // Find the next process in a round-robin manner
    pcb* next_process = NULL;

    if (arg1 == 10) {
        // If arg1 is 10, select the first process in the list
        next_process = pcb_head->next;
    } else {
        // Otherwise, find the next process after the current one
        next_process = current->next;
        if (next_process == NULL) {
            // If the end of the list is reached, wrap around to the beginning
            next_process = pcb_head->next;
        }
    }

    if (next_process == NULL) {
        // No process found with the given PID
        current = NULL;
        printf("No process found with PID: %d\n", arg1);
        return 1; // Error
    }

    // Update the current process
    current = next_process;

    // Update the page directory base register (pdbr)
    pdbr = current->pgdir;
    printf("ku_scheduler| PDBR: %p\n", pdbr);

    // Print the selected process information
    print_process_info(current);

    return 0; // Success
}

/**
 * - searches an available page frame(sequential search from PFN 0)
 * - Performs page eviction(FIFO) and swap-out if there is no available page frame
 * - Fills the first-loaded page with 0s
 * - Performs swap-in if the touched page is swapped-out
 * - updates free lists
 * - updates PED or PTE
 *
 * @param arg1 virtual address that generates a page fault
 * @return value 0: success
 *               1: error(segmentation fault or no space)
 */
int ku_pgfault_handler(unsigned short arg1){
    unsigned short virtual_address = arg1;
    printf("ku_pgfault_handler| virtual address: %d\n", virtual_address);
    int page_frame_number = -1;
    int swap_frame_number = -1;
    int i;

    printf("ku_pgfault_handler| Handling page fault for virtual address: 0x%04X\n", virtual_address);

    // Search for an available page frame (sequential search from PFN 0)
    int pfnum_bitmap_size = (pfnum + 7) / 8;
    for (i = 0; i < pfnum_bitmap_size; i++) {
//        print_bitmap(pfnum_freelist, pfnum, "Physical Memory Frame");
        printf("\n");
        if (pfnum_freelist[i] != (char)0xFF) {
            for (int j = 0; j < 8; j++) {
                if ((pfnum_freelist[i] & (1 << j)) == 0) {
                    page_frame_number = i * 8 + j;
                    break;
                }
            }
            printf("ku_pgfault_handler| page frame number: %d\n", page_frame_number);
            if (page_frame_number != -1) {
                break;
            }
        }
    }

    if (page_frame_number != -1) {
        printf("ku_pgfault_handler| Found available page frame: %d\n", page_frame_number);
    } else {
        printf("ku_pgfault_handler| No available page frame found. Performing page eviction.\n");
    }

    // If no available page frame, perform page eviction (FIFO) and swap-out
    if (page_frame_number == -1) {
        page_frame_number = perform_page_replacement_fifo();
        if (page_frame_number == -1) {
            printf("ku_pgfault_handler| Page fault handling failed. No available page frame and swap frame.\n");
            return 1;
        }
        printf("ku_pgfault_handler| Page eviction performed. Selected page frame: %d\n", page_frame_number);
    }

    // Fill the first-loaded page with 0s
    unsigned short *pte = get_pte(virtual_address);
    if (pte == NULL) {
        printf("ku_pgfault_handler| Page table not present for virtual address: 0x%04X\n", virtual_address);

        // Page table is not present, allocate a new one
        unsigned short *page_table = (unsigned short *)malloc(ADDR_SIZE * sizeof(unsigned short));
        memset(page_table, 0, ADDR_SIZE * sizeof(unsigned short));
        printf("ku_pgfault_handler| Allocated new page table at address: %p\n", (void *)page_table);

        // Update the page directory entry
        unsigned short *pde = pdbr + ((virtual_address >> 10) & 0x3F);
        *pde = ((unsigned long)page_table & 0xFFFFF000) | 1;
        printf("ku_pgfault_handler| Updated PDE at address: %p, value: 0x%04X\n", (void *)pde, *pde);

        // Get the PTE again after updating the page table
        pte = get_pte(virtual_address);
        printf("ku_pgfault_handler| PTE obtained after updating page table: %p\n", (void *)pte);
    } else {
        printf("ku_pgfault_handler| PTE found for virtual address: 0x%04X, value: 0x%04X\n", virtual_address, *pte);
    }
    printf((*pte & 1) == 0);

    if ((*pte & 1) == 0) {
        memset(pmem + (page_frame_number * 4096), 0, 4096);
        printf("ku_pgfault_handler| First-loaded page filled with zeros.\n");
    }

    // perform swap-in if the touched page is swapped-out
    if ((*pte & (1 << 15)) != 0) {
        // The page is swapped-out
        swap_frame_number = (*pte & 0xE000) >> 13;
        printf("ku_pgfault_handler| Performing swap-in. Swap frame number: %d\n", swap_frame_number);

        // Copy the contents of the swap frame to the page frame
        memcpy(pmem + (page_frame_number * 4096), swaps + (swap_frame_number * 4096), 4096);
        printf("ku_pgfault_handler| Page swapped-in from swap frame to page frame.\n");

        // Update the PTE of the swapped-in page
        *pte = (*pte & 0x1FFF) | (page_frame_number << 1);
        *pte &= 0x7FFF;
    }

    // update free lists
    pfnum_freelist[page_frame_number / 8] |= (1 << (page_frame_number % 8)); // Mark the page frame as used
    printf("ku_pgfault_handler| Page frame %d marked as used in the free list.\n", page_frame_number);
    if (swap_frame_number != -1) {
        sfnum_freelist[swap_frame_number / 8] &= ~(1 << (swap_frame_number % 8)); // Mark the swap frame as free
        printf("ku_pgfault_handler| Swap frame %d marked as free in the free list.\n", swap_frame_number);
    }

    // Update PED or PTE
    unsigned short *pde = pdbr + ((virtual_address >> 10) & 0x3F);
    if ((*pde & 1) == 0) {
        // Page directory entry is not valid, allocate a new page table
        unsigned short *page_table = (unsigned short *)malloc(ADDR_SIZE * sizeof(unsigned short));
        memset(page_table, 0, 4096);

        // Update the page directory entry
        *pde = ((unsigned long)page_table & 0xFFFFF000) | 1;
        printf("ku_pgfault_handler| New page table allocated and PDE updated.\n");
    }

    pte = (unsigned short *)((unsigned long)(*pde & 0xFFFFF000) + ((virtual_address >> 4) & 0x3FF));
    // Set the page frame number and valid bit
    *pte = (page_frame_number << 1) | 1;
    printf("ku_pgfault_handler| PTE updated. Virtual address 0x%04X mapped to page frame %d.\n", virtual_address, page_frame_number);
    return 0; // Success
}

/**
 * removes PCB
 * Reaps page frames and swap frames mapped(i.e, updates free lists)
 * @param arg1 process ID
 * @return value 0: success
 *               1: error(invalid PID)
 */
int ku_proc_exit(unsigned short arg1){

}
