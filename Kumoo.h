#define ADDR_SIZE 16

struct pcb *current;
unsigned short *pdbr;
char *pmem, *swaps;
int pfnum, sfnum;

void ku_dump_pmem(void);
void ku_dump_swap(void);


struct pcb{
	unsigned short pid;
	FILE *fd;
	unsigned short *pgdir;
	/* Add more fields as needed */
};
void ku_freelist_init(){

}
int ku_proc_init(int argc, char *argv[]){

}
int ku_scheduler(unsigned short arg1){

}
int ku_pgfault_handler(unsigned short arg1){

}
int ku_proc_exit(unsigned short arg1){

}