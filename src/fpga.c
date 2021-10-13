#include "fpga.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#define __DEBUG__

#define ACCEL_BASE_ADDR (0xa1000000ULL)

typedef enum GEMM_CTRL_REG {
    M_SIZE = 0,
    N_SIZE,
    K_SIZE,
    A_BASE,
    A_STEP,
    B_BASE,
    B_STEP,
    C_BASE,
    C_STEP,
    DATA_RDY,
    RESET,
    DEBUG,
    MAGIC,
    GEMM_REG_NUM
} gemm_reg_t;

#define GEMM_TOT_BYTES  (0x20000000ULL)
#define MAP_SIZE        (0x1000000ULL)
#define MAP_MASK        (MAP_SIZE-1)
#define MAP_BLK_WORDS   (MAP_SIZE>>2)

/* global memmap for fpga accesses */
volatile int memfd = 0;
volatile uint32_t* base0 = NULL;
volatile uint32_t* base1 = NULL;
volatile bool init_success = false;

int fpga_init(void)
{
#ifdef __DEBUG__
    printf("fpga_init\n");
    fflush(stdout);
#endif

    //Open memory as a file
    memfd = open("/dev/mem", O_RDWR|O_SYNC);
    if(!memfd) {
        printf("Unable to open /dev/mem.  Ensure it exists (major=1, minor=1)\n");
        fflush(stdout);
        return -1;
    }

#ifdef __DEBUG__
    printf("fpga_init memfd'd\n");
    fflush(stdout);
#endif

    //Map the physical base address to local pointer (in virtual address space)
    base0 = (uint32_t *)mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, memfd, ACCEL_BASE_ADDR & ~MAP_MASK);	
    if(base0 == MAP_FAILED) {
        printf("mapping failed\n");
        fflush(stdout);
        return -2;
    }
    base1 = (uint32_t *)mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, memfd, (ACCEL_BASE_ADDR+MAP_SIZE) & ~MAP_MASK);	
    if(base1 == MAP_FAILED) {
        printf("mapping failed\n");
        fflush(stdout);
        return -2;
    }

#ifdef __DEBUG__
    printf("fpga_init mmap'd\n");
    fflush(stdout);
#endif
    
    if (base0[MAGIC] != 0xdeadbeefULL) {
        printf("accelerator memory corrupted\n");
        fflush(stdout);
        return -3;
    }

    init_success = true;
#ifdef __DEBUG__
    printf("fpga_init success\n");
    fflush(stdout);
#endif
    return 0;
}

bool fpga_ready(void)
{
    if (init_success) {
        return base0[DATA_RDY] & 0x02;
    } else {
        return false;
    }
}

void fpga_gemm(int m, int n, int k,
        fx_t *A, int lda,
        fx_t *B, int ldb,
        fx_t *C, int ldc)
{
    if (!init_success) return;
    base0[RESET] = 0x01;
    while (!fpga_ready());

    base0[M_SIZE] = m;
    base0[N_SIZE] = n;
    base0[K_SIZE] = k;
    base0[A_STEP] = lda;
    base0[B_STEP] = ldb;
    base0[C_STEP] = ldc;

#ifdef __DEBUG__
    printf("\nfpga_gemm: transferring arrays\n");
    fflush(stdout);
#endif

    int32_t abase = GEMM_REG_NUM;
    int32_t asize = m*k;
    int32_t bbase = abase+asize;
    int32_t bsize = k*n;
    int32_t cbase = bbase+bsize;
    base0[A_BASE] = abase;
    base0[B_BASE] = bbase;
    base0[C_BASE] = cbase;

    /* some dirty shit to memcpy accross mmap boundaries */
#ifdef __DEBUG__
    printf("\nfpga_gemm: transferring A\n");
    fflush(stdout);
#endif
    if (bbase < MAP_BLK_WORDS) {
        memcpy(base0+abase, A, asize*sizeof(fx_t));
    } else {
        int32_t sb1 = bbase - MAP_BLK_WORDS;
        int32_t sb0 = asize - (sb1 > 0 ? sb1 : 0);
#ifdef __DEBUG__
    printf("\nfpga_gemm: A is split, asize: %d, sb0: %d, sb1: %d\n", asize, sb0, sb1);
    fflush(stdout);
#endif
        memcpy(base0+abase, A, sb0*sizeof(fx_t));
        memcpy(base1, A+sb0, sb1*sizeof(fx_t));
    }

#ifdef __DEBUG__
    printf("\nfpga_gemm: transferring B\n");
    fflush(stdout);
#endif
    if (bbase < MAP_BLK_WORDS) {
        int32_t sb1 = cbase - MAP_BLK_WORDS;
        int32_t sb0 = bsize - (sb1 > 0 ? sb1 : 0);
#ifdef __DEBUG__
    printf("\nfpga_gemm: B potentially split, bbase: %d, bsize: %d, sb0: %d, sb1: %d\n", bbase, bsize, sb0, sb1);
    fflush(stdout);
#endif
        memcpy(base0+bbase, B, sb0*sizeof(fx_t));
#ifdef __DEBUG__
    printf("\nfpga_gemm: sb0 copied\n");
    fflush(stdout);
#endif
        if (sb1 > 0) memcpy(base1, B+sb0, sb1*sizeof(fx_t));
    } else {
        bbase -= MAP_BLK_WORDS;
        memcpy(base1+bbase, B, bsize*sizeof(fx_t));    
    }
    
#ifdef __DEBUG__
    printf("\ndone\n");
    fflush(stdout);
#endif

    base0[DATA_RDY] = 0x01;
}

void fpga_read(int m, int n, int k, fx_t* C)
{
    if (!init_success) return;
#ifdef __DEBUG__
    printf("\nfpga_read: wait for ready\n");
    fflush(stdout);
#endif
    while (!fpga_ready());

#ifdef __DEBUG__
    printf("\nfpga_read: transferring arrays\n");
    fflush(stdout);
#endif

    int32_t cbase = base0[C_BASE];
    int32_t csize = m*n;
    if (cbase < MAP_BLK_WORDS) {
        int32_t sb1 = cbase + csize - MAP_BLK_WORDS;
        int32_t sb0 = csize - (sb1 > 0 ? sb1 : 0);
#ifdef __DEBUG__
    printf("\nfpga_read: C potentially split, cbase: %d, csize: %d, sb0: %d, sb1: %d\n", cbase, csize, sb0, sb1);
    fflush(stdout);
#endif
        memcpy(C, base0+cbase, sb0*sizeof(fx_t));
        if (sb1 > 0) memcpy(C, base1, sb1*sizeof(fx_t));
    } else {
#ifdef __DEBUG__
    printf("\nfpga_read: C in base1\n");
    fflush(stdout);
#endif
        cbase -= MAP_BLK_WORDS;
        memcpy(C, base1+cbase, csize*sizeof(fx_t));
    }

#ifdef __DEBUG__
    printf("\nfpga_read: done\n");
    fflush(stdout);
#endif
}

void fpga_free(void)
{
    if (!init_success) return;
#ifdef __DEBUG__
    printf("\nfpga_free: free\n");
    fflush(stdout);
#endif
    init_success = false;
    munmap((void *) base0, MAP_SIZE);
    munmap((void *) base1, MAP_SIZE);
    close(memfd);
#ifdef __DEBUG__
    printf("\nfpga_free: done\n");
    fflush(stdout);
#endif
}


