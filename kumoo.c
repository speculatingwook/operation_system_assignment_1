#include <stdio.h>
#include <stdlib.h>
#include "kumoo.h"

#define SCHED	0
#define PGFAULT	1
#define EXIT	2
#define TSLICE	5

struct handlers{
       int (*sched)(unsigned short);
       int (*pgfault)(unsigned short);
       int (*exit)(unsigned short);
}kuos;

void ku_dump_pmem(void){
    for(int i = 0; i < (64 << 12); i++){
        printf("%x ", pmem[i]);
    }
    printf("\n");
}
void ku_dump_swap(void){
    for(int i = 0; i < (64 << 14); i++){
        printf("%x ", swaps[i]);
    }
    printf("\n");
}

void ku_reg_handler(int flag, int (*func)(unsigned short)){
	switch(flag){
		case SCHED:
			kuos.sched = func;
			break;
		case PGFAULT:
			kuos.pgfault = func;
			break;
		case EXIT:
			kuos.exit = func;
			break;
		default:
			exit(0);
	}
}

int ku_traverse(unsigned short va, int write){
	int pd_index, pt_index, pa;
	unsigned short *ptbr;
	unsigned short *pte, *pde;
	int PFN;

	pd_index = (va & 0xFFC0) >> 11;
	pde = pdbr + pd_index;

	if(!(*pde & 0x1)){
		return -1;
    }
    
	PFN = (*pde & 0xFFF0) >> 4;
	ptbr = (unsigned short*)(pmem + (PFN << 6));

	pt_index = (va & 0x07C0) >> 6;
	pte = ptbr + pt_index;

	if(!(*pte & 0x1)){
        return -1;
    }

	PFN = (*pte & 0xFFF0) >> 4;

	pa = (PFN << 6)+(va & 0x3F);

    if (write)
        *pte |= 0x2;

	return pa;
}


void ku_os_init(void){
    /* Initialize physical memory*/
    pfnum = 1 << 12;
    sfnum = 1 << 14;
    pmem = (char*)malloc(64 << 12);
    swaps = (char*)malloc(64 << 14);
    /* Init free list*/
    ku_freelist_init();
    /*Register handler*/
	ku_reg_handler(SCHED, ku_scheduler);
	ku_reg_handler(PGFAULT, ku_pgfault_handler);
	ku_reg_handler(EXIT, ku_proc_exit);
}

int op_read(unsigned short pid){
    unsigned short va;
    int addr, pa, ret = 0;
    char sorf = 'S';
    /* get Address from the line */
    if(fscanf(current->fd, "%d", &addr) == EOF){
        /* Invaild file format */
        return 1;
    }
    va = addr & 0xFFFF;
    pa = ku_traverse(va, 0);

    if (pa < 0){
        /* page fault!*/
        ret = kuos.pgfault(va);
    	if (ret > 0){
            /* No free page frames or SEGFAULT */
            sorf = 'E';
            ret = kuos.exit(current->pid);
            if (ret > 0){
                /* invalid PID */
                return 1;
            }
        }
        else {
            pa = ku_traverse(va, 0);
            sorf = 'F';
        }
    } 

    if (pa < 0){
        printf("%d: %d -> (%c)\n", pid, va, sorf);
        return -1;
    }
    else {
        printf("%d: %d -> %d (%c)\n", pid, va, pa, sorf);
        return 0;
    }
  
}

int op_write(unsigned short pid){
    unsigned short va;
    int addr, pa, ret = 0;
    char input ,sorf = 'S';
    /* get Address from the line */
    if(fscanf(current->fd, "%d %c", &addr, &input) == EOF){
        /* Invaild file format */
        return 1;
    }
    va = addr & 0xFFFF;
    pa = ku_traverse(va, 1);

    if (pa < 0){
        /* page fault!*/
        ret = kuos.pgfault(va);
    	if (ret > 0){
            /* No free page frames or SEGFAULT */
            sorf = 'E';
            ret = kuos.exit(current->pid);
            if (ret > 0){
                /* invalid PID */
                return 1;
            }
        }
        else {
            pa = ku_traverse(va, 1);
            sorf = 'F';
        }
    }

    if (pa < 0){
        printf("%d: %d -> (%c)\n", pid, va, sorf);
        return -1;
    }
    else {
        *(pmem + pa) = input;
        printf("%d: %d -> %d (%c)\n", pid, va, pa, sorf);
        return 0;
    }

}

int do_ops(char op){
    char sorf;
    int ret;
    unsigned short pid = current->pid;

    switch(op){
        case 'r':
            ret = op_read(pid);
        break;

        case 'w':
            ret = op_write(pid);
        break;

        case 'e':
            ret = kuos.exit(pid);
            if (ret > 0){
                /* invalid PID */
                return ret;
            }
            return -1;
        break;
    }

}

void ku_run_procs(void){
    char sorf;
	int addr, pa, i;
    char op;
    int ret;
    unsigned short pid = 0;

    ret = kuos.sched(10);
    /* No processes */
    if (ret > 0)
        return;
    
	do{
		if(!current)
			exit(0);

		for( i=0 ; i<TSLICE ; i++){
			    /* Get operation from the line */
			if(fscanf(current->fd, " %c", &op) == EOF){
				/* Invaild file format */
				return;
			}

            pid = current->pid;
			ret = do_ops(op);
		
            /* process terminated */
            if (ret < 0){
                break;
            }
            else if( ret > 0){
                /* something went wrong */
            }
		}

		ret = kuos.sched(pid);
        /* No processes */
        if (ret > 0)
            return;

	}while(1);
}

int main(int argc, char *argv[]){
	/* System initialization */
	ku_os_init();
	/* Per-process initialization */
	ku_proc_init(argc, argv);
	/* Process execution */
	ku_run_procs();

	return 0;
}
