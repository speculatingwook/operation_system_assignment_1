#define ADDR_SIZE 16
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG	0
#define TEST	1

const short PDE_MASK = 0b1111100000000000;
const short PTE_MASK = 0b0000011111000000;
const short SWAP_MASK = 0b1111111111111100;

struct pcb *current;
unsigned short *pdbr;
char *pmem, *swaps;
int pfnum = 4;
int sfnum;
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

void print_binary_short(unsigned short value, const char* label) {
    printf("%s: ", label);
    for (int i = sizeof(unsigned short) * 8 - 1; i >= 0; i--) {
        printf("%d", (value >> i) & 1);
    }
    printf("\n");
}

void print_binary_int(unsigned int value, const char* label) {
    printf("%s: ", label);
    for (int i = sizeof(unsigned int) * 8 - 1; i >= 0; i--) {
        printf("%d", (value >> i) & 1);
    }
    printf("\n");
}

void print_binary_pointer(void* pointer, const char* label) {
    printf("%s: ", label);
    for (int i = sizeof(void*) * 8 - 1; i >= 0; i--) {
        printf("%lu", ((unsigned long)pointer >> i) & 1);
    }
    printf("\n");
}

void print_page_frame_contents(char *pmem, int page_frame_number) {
    printf("ku_pgfault_handler| Contents of the filled page frame %d:\n", page_frame_number);
    for (int i = 0; i < 4096; i += 16) {
        printf("%04X: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02X ", pmem[(page_frame_number * 4096) + i + j]);
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
    pfnum = 4;
    int pfnum_bitmap_size = (pfnum + 7) / 8;
    int sfnum_bitmap_size = (sfnum + 7) / 8;

    pfnum_freelist = (char*)malloc(pfnum_bitmap_size);
    sfnum_freelist = (char*)malloc(sfnum_bitmap_size);

    memset(pfnum_freelist, 0, pfnum_bitmap_size);
    memset(sfnum_freelist, 0, sfnum_bitmap_size);
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
    unsigned short *pde = pdbr + ((virtual_address & PDE_MASK) >> 10);
//    print_binary_short(pde, "get_pte| pde");
    // pde의 present bit가 0
    if ((*pde & 1) == 0) {
        printf("Page directory entry is not valid\n");
        return NULL;
    }

    unsigned short* page_table = (unsigned short *)(*pde & 0xFFF0);
    unsigned short* pte = page_table + ((virtual_address & PTE_MASK) >> 6);
//    print_binary_short(pte, "get_pte| pte");
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
    unsigned short pte = get_pte(evicted_page_frame_number);
    pte = (pte & 0xFFFE) | (1 << 15); // Set the swapped-out bit
    pte = (pte & 0x1FFF) | (swap_frame_number << 13); // Set the swap frame number

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
    } else {
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
        print_binary_short(arg1, "ku_scheduler| No process found with PID");
        return 1; // Error
    }

    // Update the current process
    current = next_process;

    // Update the page directory base register (pdbr)
    pdbr = current->pgdir;
//    print_binary_pointer(*pdbr, "ku_scheduler| PDBR");

    // Print the selected process information
//    print_process_info(current);

    return 0; // Success
}

int find_available_page_frame() {
    int page_frame_number = -1;
    int pfnum_bitmap_size = (pfnum + 7) / 8;

    for (int i = 0; i < pfnum_bitmap_size; i++) {
        if (pfnum_freelist[i] != (char)0xFF) {
            for (int j = 0; j < 8; j++) {
                if ((pfnum_freelist[i] & (1 << j)) == 0) {
                    page_frame_number = i * 8 + j;
                    break;
                }
            }
            if (page_frame_number != -1) {
                break;
            }
        }
    }

    return page_frame_number;
}
void mark_page_frame_used(int page_frame_number) {
    pfnum_freelist[page_frame_number / 8] |= (1 << (page_frame_number % 8)); // Mark the page frame as used
}

void mark_swap_frame_free(int swap_frame_number) {
    sfnum_freelist[swap_frame_number / 8] &= ~(1 << (swap_frame_number % 8)); // Mark the swap frame as free
}

// 이 함수는 주어진 page_frame_number와 swap_frame_number에 따라
// 각각 페이지 프레임과 스왑 프레임을 업데이트합니다.
void update_free_lists(int page_frame_number, int swap_frame_number) {
    mark_page_frame_used(page_frame_number);

    if (swap_frame_number != -1) {
        mark_swap_frame_free(swap_frame_number);
    }
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
int ku_pgfault_handler(unsigned short arg1) {
    unsigned short virtual_address = arg1;
//    print_binary_short(virtual_address, "ku_pgfault_handler| virtual address");
    int page_frame_number = -1;
    int swap_frame_number = -1;
    int i;

    printf("ku_pgfault_handler| Handling page fault for virtual address: 0x%04X\n", virtual_address);
    page_frame_number = find_available_page_frame();

    if (page_frame_number != -1) {
//        print_binary_int(page_frame_number, "ku_pgfault_handler| Found available page frame");
    } else {
        printf("ku_pgfault_handler| No available page frame found. Performing page eviction.\n");
    }

    // If no available page frame, perform page eviction (FIFO) and swap-out
    if (page_frame_number == -1) {
//        printf("ku_pgfault_handler| No available page frame found. Performing page eviction.\n");
        page_frame_number = perform_page_replacement_fifo();
        if (page_frame_number == -1) {
//            printf("ku_pgfault_handler| Page fault handling failed. No available page frame and swap frame.\n");
            return 1;
        }
//        printf("ku_pgfault_handler| Page eviction performed. Selected page frame: %d\n", page_frame_number);
    }
    unsigned short* pte = get_pte(virtual_address);

//    // perform swap-in if the touched page is swapped-out
//    if ((*pte & 1) == 0) { // present bit 0
//        // The page is not present in physical memory
//        if ((*pte & 0b10) != 0) { // dirty bit 1
//            // The page is swapped-out
//            swap_frame_number = (*pte & SWAP_MASK) >> 2;
//            print_binary_int(swap_frame_number, "ku_pgfault_handler| Performing swap-in. Swap frame number");
//
//            // Copy the contents of the swap frame to the page frame
//            memcpy(pmem + (page_frame_number * 4096), swaps + (swap_frame_number * 4096), 4096);
////            printf("ku_pgfault_handler| Page swapped-in from swap frame to page frame.\n");
//        } else {
//            // The page is not mapped
////            printf("ku_pgfault_handler| Page is not mapped. Allocating new page frame.\n");
//
//            // Fill the new page frame with zeros
//            memset(pmem + (page_frame_number * 4096), 0, 4096);
////            printf("ku_pgfault_handler| New page frame filled with zeros.\n");
//        }
//
////        print_binary_short(pte, "ku_pgfault_handler| PTE before update");
//        *pte = (page_frame_number << 1) | 1;
////        print_binary_short(pte, "ku_pgfault_handler| PTE after update");
////        printf("ku_pgfault_handler| Page mapped to page frame %d.\n", page_frame_number);
//    }

    // 페이지 테이블 새로 할당하는 경우
    if (pte == NULL) {
//        printf("ku_pgfault_handler| Page table not present for virtual address: 0x%04X\n", virtual_address);
        memset(pmem + (page_frame_number * 4096), 0, 4096);

        // Page table is not present, allocate a new one
        unsigned short* page_table = pmem + (page_frame_number * 4096);
        memset(page_table, 0, ADDR_SIZE * sizeof(unsigned short));
//        printf("ku_pgfault_handler| Allocated new page table at address: %p\n", (void *)page_table);

        // Update the page directory entry
        unsigned short* pde = pdbr + ((virtual_address & PDE_MASK) >> 10);
        *pde = (*pde) & (page_frame_number << 4);
        *pde = (*pde) & 1;
        mark_page_frame_used(page_frame_number);
//        printf("ku_pgfault_handler| Updated PDE at address: %d, value: 0x%04X\n", pde, pde);

        // find new available page frame
        page_frame_number = find_available_page_frame();

        // Update the page table entry
        unsigned short* pte = page_table + ((virtual_address & PTE_MASK) >> 6);
        *pte = (*pte) & (page_frame_number << 4);
        *pte = (*pte) & 1;
        mark_page_frame_used(page_frame_number);
//        printf("ku_pgfault_handler| Updated PTE at address: %d, value: 0x%04X\n", pte, pte);

//        print_page_frame_contents(pmem, page_frame_number);
        update_free_lists(page_frame_number, swap_frame_number);
    } else {
        printf("ku_pgfault_handler| PTE found for virtual address: 0x%04X, value: %d\n", virtual_address, pte);
    }

    // update free lists
    update_free_lists(page_frame_number, swap_frame_number);
    return 0; // Success
}

/**
 * removes PCB
 * Reaps page frames and swap frames mapped(i.e, updates free lists)
 * @param arg1 process ID
 * @return value 0: success
 *               1: error(invalid PID)
 */
int ku_proc_exit(unsigned short arg1) {
    pcb* current_pcb = pcb_head->next;
    pcb* prev_pcb = pcb_head;

    // Find the PCB with the given PID
    while (current_pcb != NULL) {
        if (current_pcb->pid == arg1) {
            break;
        }
        prev_pcb = current_pcb;
        current_pcb = current_pcb->next;
    }

    // If PCB is not found, return error
    if (current_pcb == NULL) {
        printf("ku_proc_exit| Invalid PID: %d\n", arg1);
        return 1;
    }

    // Remove the PCB from the linked list
    prev_pcb->next = current_pcb->next;

    // Reap page frames and swap frames mapped by the process
    unsigned short* pgdir = current_pcb->pgdir;
    for (int i = 0; i < ADDR_SIZE; i++) {
        unsigned short pde = pgdir[i];
        if (pde & 1) {
            // Page directory entry is valid
            unsigned short* page_table = (unsigned short*)(unsigned long)(pde & 0xFFFFF000);
            for (int j = 0; j < ADDR_SIZE; j++) {
                unsigned short pte = page_table[j];
                if (pte & 1) {
                    // Page table entry is valid
                    int page_frame_number = pte >> 1;
                    // Mark the page frame as free in the free list
                    pfnum_freelist[page_frame_number / 8] &= ~(1 << (page_frame_number % 8));
                    printf("ku_proc_exit| Page frame %d marked as free\n", page_frame_number);
                } else if (pte != 0) {
                    // Page is swapped-out
                    int swap_frame_number = pte >> 1;
                    // Mark the swap frame as free in the free list
                    sfnum_freelist[swap_frame_number / 8] &= ~(1 << (swap_frame_number % 8));
                    printf("ku_proc_exit| Swap frame %d marked as free\n", swap_frame_number);
                }
            }
            // Free the page table
            free(page_table);
        }
    }

    // Free the page directory
    free(pgdir);

    // Free the PCB
    fclose(current_pcb->fd);
    free(current_pcb);

    printf("ku_proc_exit| Process %d exited successfully\n", arg1);
    return 0;
}
