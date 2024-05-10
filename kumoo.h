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
//    printf("Physical Memory Frame Bitmap:\n");
//    for (int i = 0; i < pfnum_bitmap_size; i++) {
//        for (int j = 0; j < 8; j++) {
//            if (i * 8 + j < pfnum) {
//                printf("%d", (pfnum_freelist[i] >> j) & 1);
//            }
//        }
//    }
//    printf("\n");
//
//    printf("Swap Space Frame Bitmap:\n");
//    for (int i = 0; i < sfnum_bitmap_size; i++) {
//        for (int j = 0; j < 8; j++) {
//            if (i * 8 + j < sfnum) {
//                printf("%d", (sfnum_freelist[i] >> j) & 1);
//            }
//        }
//    }
//    printf("\n");
}

//  and page directories & allocate and zero-fills page directories
/**
 *
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
