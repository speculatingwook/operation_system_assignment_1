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


struct pcb{
	unsigned short pid;
	FILE *fd;
	unsigned short *pgdir;
	/* Add more fields as needed */
};
void ku_freelist_init(){
    int pfnum_bitmap_size = (pfnum + 7) / 8;
    int sfnum_bitmap_size = (sfnum + 7) / 8;

    pfnum_freelist = (char*)malloc(pfnum_bitmap_size);
    sfnum_freelist = (char*)malloc(sfnum_bitmap_size);

    memset(pfnum_freelist, 0, pfnum_bitmap_size);
    memset(sfnum_freelist, 0, sfnum_bitmap_size);

    // 비트맵 출력 (테스트용)
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

int ku_proc_init(int argc, char *argv[]){

}

int ku_scheduler(unsigned short arg1){

}
int ku_pgfault_handler(unsigned short arg1){

}
int ku_proc_exit(unsigned short arg1){

}