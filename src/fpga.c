#include "fpga.h"
#include "xgemm_hw_block.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>

//#define __DEBUG__
//#define __DEBUG2__

#define ACCEL_BASE_ADDR (0xa0000000ULL) // to change
#define A_BASE_ADDR (0xb0000000ULL) // to change
#define B_BASE_ADDR (0xb0040000ULL) // to change
#define C_BASE_ADDR (0xb0080000ULL) // to change

typedef enum GEMM_CTRL_REG {
	ap_start = 0,
	ap_done = 1,
	ap_idle = 2,
	ap_ready = 3,
	auto_restart = 7
} gemm_reg_t;

//#define GEMM_TOT_BYTES  (0x20000000ULL)
//#define MAP_SIZE        (0x1000000ULL)
#define ACCELMAP_SIZE        (0x1000ULL)
#define ACCELMAP_MASK        (ACCELMAP_SIZE-1)
#define ACCELMAP_BLK_WORDS   (ACCELMAP_SIZE>>2)
#define AMAP_SIZE        (0x40000ULL)
#define AMAP_MASK        (AMAP_SIZE-1)
#define AMAP_BLK_WORDS   (AMAP_SIZE>>2)
#define BMAP_SIZE        (0x40000ULL)
#define BMAP_MASK        (BMAP_SIZE-1)
#define BMAP_BLK_WORDS   (BMAP_SIZE>>2)
#define CMAP_SIZE        (0x40000ULL)
#define CMAP_MASK        (CMAP_SIZE-1)
#define CMAP_BLK_WORDS   (CMAP_SIZE>>2)

/* global memmap for fpga accesses */
volatile int memfd = 0;
volatile uint32_t* accelptr = NULL;
volatile uint32_t* aptr = NULL;
volatile uint32_t* bptr = NULL;
volatile uint32_t* cptr = NULL;
volatile bool init_success = false;

/* interrupt stuff */
#define READ_CMD  (0x0 << 31)
#define WRITE_CMD (0x1 << 31)
volatile int det_int = 0;

// signal handler for receiving events from hardware driver
void sighandler(int signo)
{
  if(signo==SIGIO)
    {
      det_int++;
#ifdef __DEBUG__
      printf("\nInterrupt detected\n");
#endif
    }
  
  return;
}

int man_memcpy(uint32_t *dst, uint32_t *src, uint32_t num_bytes)
{
    int i;
    for (i = 0; i < num_bytes/4; ++i) {
        dst[i] = src[i];
        if (src[i] != dst[i]) {
            printf("Mismatch at offset %d\n!", i);
            return -1;
        }
    }
    return 0;
}

int fpga_init(void)
{
#ifdef __DEBUG__
    printf("fpga_init\n");
    fflush(stdout);
#endif

    //Open memory as a file
    memfd = open("/dev/mem", O_RDWR|O_SYNC);
    if(memfd < 0) {
        printf("Unable to open /dev/mem.  Ensure it exists (major=1, minor=1)\n");
        fflush(stdout);
        return -1;
    }

#ifdef __DEBUG__
    printf("fpga_init memfd'd: %d\n", memfd);
    fflush(stdout);
#endif

    //Map the physical base address to local pointer (in virtual address space)
    //accelptr = (uint32_t *)mmap(NULL, ACCELMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, memfd, (ACCEL_BASE_ADDR) & ~ACCELMAP_MASK);	
    accelptr = (uint32_t *)mmap(NULL, ACCELMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, memfd, (ACCEL_BASE_ADDR));	
    if(accelptr == MAP_FAILED) {
        perror("accel mapping failed: ");
        fflush(stdout);
        return -2;
    }
    aptr = (uint32_t *)mmap(NULL, AMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, memfd, (A_BASE_ADDR));	
    if(aptr == MAP_FAILED) {
        perror("a mapping failed: ");
        fflush(stdout);
        return -2;
    }
    bptr = (uint32_t *)mmap(NULL, BMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, memfd, (B_BASE_ADDR));	
    if(bptr == MAP_FAILED) {
        perror("b mapping failed: ");
        fflush(stdout);
        return -2;
    }
    cptr = (uint32_t *)mmap(NULL, CMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, memfd, (C_BASE_ADDR) & ~CMAP_MASK);	
    if(cptr == MAP_FAILED) {
        perror("c mapping failed: ");
        fflush(stdout);
        return -2;
    }

#ifdef __DEBUG__
    printf("fpga_init mmap'd\n");
    fflush(stdout);
#endif
    
//    if (aptr[MAGIC] != 0xdeadbeefULL) {
//        printf("accelerator memory corrupted\n");
//        fflush(stdout);
//        return -3;
//    }

    // Setup interrupt
    unsigned long volatile gie, iie;
    struct sigaction action;
    int fd;
    // install signal handler
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGIO);

    action.sa_handler = sighandler;
    action.sa_flags=0;

    sigaction(SIGIO, &action, NULL);

    // open hardware device (driver)
    fd=open("/dev/fpga", O_RDWR);
    if(fd < 0)
    {

        printf("Unable to open /dev/fpga.  Ensure it exists!\n");
        return -1;
    }
    fcntl(fd, F_SETOWN, getpid());
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)|O_ASYNC);

    // enable FPGA interrupts (global and IP)
    ioctl(fd, READ_CMD + 0x1, &gie);
    gie = gie | 0x00000001;
    ioctl(fd, WRITE_CMD + 0x1, &gie);

    iie = 0x1;
    ioctl(fd, WRITE_CMD + 0x2, &iie);

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
        return accelptr[XGEMM_AXILITES_ADDR_AP_CTRL>>2] & 0x2 ||
                accelptr[XGEMM_AXILITES_ADDR_AP_CTRL>>2] & 0x4;
    } else {
        return false;
    }
}

bool fpga_done(void)
{
    if (init_success) {
        return det_int;
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
    while (!fpga_ready());

    int i;

#ifdef __DEBUG__
    printf("\nfpga_gemm: transferring arrays\n");
    fflush(stdout);
#endif

    int32_t asize = k;
    int32_t bsize = k;
    accelptr[XGEMM_AXILITES_ADDR_K_DATA>>2] = k;

#ifdef __DEBUG__
    printf("\nfpga_gemm: transferring A\n");
    fflush(stdout);
#endif
    man_memcpy(aptr, A, asize*sizeof(fx_t));

#ifdef __DEBUG__
    printf("\nfpga_gemm: transferring B\n");
    fflush(stdout);
#endif
    man_memcpy(bptr, B, bsize*sizeof(fx_t));
    
#ifdef __DEBUG__
    printf("\ndone, starting gemm\n");
    fflush(stdout);
#endif

    accelptr[XGEMM_AXILITES_ADDR_AP_CTRL>>2] |= 0x1;
}

int num_gemm = 0;
void fpga_gemm_block(int m, int n, int k, int s, fx_t *A, fx_t *B, fx_t *C)
{
    if (!init_success) return;
    while (!fpga_ready());

    int i;

#ifdef __DEBUG2__
    printf("\nfpga_gemm %d: transferring arrays\n", num_gemm++);
    fflush(stdout);
#endif

    int32_t asize = s*k;
    int32_t bsize = k*s;
    int32_t csize = s*s;
    accelptr[XGEMM_AXILITES_ADDR_M_DATA>>2] = m;
    accelptr[XGEMM_AXILITES_ADDR_N_DATA>>2] = n;
    accelptr[XGEMM_AXILITES_ADDR_K_DATA>>2] = k;
    accelptr[XGEMM_AXILITES_ADDR_S_DATA>>2] = s;

#ifdef __DEBUG__
    printf("\nfpga_gemm: transferring A\n");
    fflush(stdout);
#endif
    man_memcpy(aptr, A, asize*sizeof(fx_t));

#ifdef __DEBUG__
    printf("\nfpga_gemm: transferring B\n");
    fflush(stdout);
#endif
    man_memcpy(bptr, B, bsize*sizeof(fx_t));
    
#ifdef __DEBUG__
    printf("\nfpga_gemm: transferring C\n");
    fflush(stdout);
#endif
    man_memcpy(cptr, C, csize*sizeof(fx_t));

#ifdef __DEBUG__
    printf("\ndone, starting gemm\n");
    fflush(stdout);
#endif

    accelptr[XGEMM_AXILITES_ADDR_AP_CTRL>>2] |= 0x1;
}

void fpga_read(int m, int n, int k, fx_t* C)
{
    if (!init_success) return;
#ifdef __DEBUG__
    printf("\nfpga_read: wait for ready\n");
    fflush(stdout);
#endif
    while (!fpga_done());
    det_int = 0;

    int32_t csize = 1;
#ifdef __DEBUG__
    printf("\nfpga_read: C\n");
    fflush(stdout);
#endif
    memcpy(C, cptr, csize*sizeof(fx_t));
#ifdef __DEBUG__
    printf("\nfpga_read: done\n");
    fflush(stdout);
#endif
}

void fpga_read_block(int s, fx_t* C)
{
    if (!init_success) return;
#ifdef __DEBUG__
    printf("\nfpga_read: wait for ready\n");
    fflush(stdout);
#endif
    while (!fpga_done());
    det_int = 0;

    int32_t csize = s*s;
#ifdef __DEBUG__
    printf("\nfpga_read: C\n");
    fflush(stdout);
#endif
    man_memcpy(C, cptr, csize*sizeof(fx_t));
#ifdef __DEBUG__
    printf("\nfpga_read: done\n");
    fflush(stdout);
#endif
}

void fpga_gemm_ablock(int k, int s, fx_t *A)
{
    if (!init_success) return;
    while (!fpga_ready());
    int32_t asize = s*k;
    memcpy(aptr, A, asize*sizeof(fx_t));
}

void fpga_gemm_bblock(int k, int s, fx_t *B)
{
    if (!init_success) return;
    while (!fpga_ready());
    int32_t bsize = s*k;
    memcpy(bptr, B, bsize*sizeof(fx_t));
}

void fpga_gemm_cblock(int s, fx_t *C)
{
    if (!init_success) return;
    while (!fpga_ready());
    int32_t csize = s*s;
    memcpy(cptr, C, csize*sizeof(fx_t));
}

void fpga_gemm_start(int m, int n, int k, int s)
{
    if (!init_success) return;
    while (!fpga_ready());
    accelptr[XGEMM_AXILITES_ADDR_M_DATA>>2] = m;
    accelptr[XGEMM_AXILITES_ADDR_N_DATA>>2] = n;
    accelptr[XGEMM_AXILITES_ADDR_K_DATA>>2] = k;
    accelptr[XGEMM_AXILITES_ADDR_S_DATA>>2] = s;
    accelptr[XGEMM_AXILITES_ADDR_AP_CTRL>>2] |= 0x1;
}

void fpga_free(void)
{
    if (!init_success) return;
#ifdef __DEBUG__
    printf("\nfpga_free: free\n");
    fflush(stdout);
#endif
    init_success = false;
    munmap((void *) accelptr, ACCELMAP_SIZE);
    munmap((void *) aptr, AMAP_SIZE);
    munmap((void *) bptr, BMAP_SIZE);
    munmap((void *) cptr, CMAP_SIZE);
    close(memfd);
#ifdef __DEBUG__
    printf("\nfpga_free: done\n");
    fflush(stdout);
#endif
}


