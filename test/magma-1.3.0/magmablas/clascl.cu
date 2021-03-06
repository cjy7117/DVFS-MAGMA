/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

       @generated c Wed Nov 14 22:53:48 2012

*/
#include "common_magma.h"

#define clascl_bs 64


__global__ void
l_clascl (int m, int n, float mul, cuFloatComplex* A, int lda){
    int ind =  blockIdx.x * clascl_bs + threadIdx.x ;

    int break_d = (ind < n)? ind: n-1;

    A += ind;
    if (ind < m)
       for(int j=0; j<=break_d; j++ )
           A[j*lda] *= mul;
}

__global__ void
u_clascl (int m, int n, float mul, cuFloatComplex* A, int lda){
    int ind =  blockIdx.x * clascl_bs + threadIdx.x ;

    A += ind;
    if (ind < m)
      for(int j=n-1; j>= ind; j--)
         A[j*lda] *= mul;
}


extern "C" void
magmablas_clascl(char type, magma_int_t kl, magma_int_t ku, 
                 float cfrom, float cto,
                 magma_int_t m, magma_int_t n, 
                 cuFloatComplex *A, magma_int_t lda, magma_int_t *info )
{
    int blocks;
    if (m % clascl_bs==0)
        blocks = m/ clascl_bs;
    else
        blocks = m/ clascl_bs + 1;

    dim3 grid(blocks, 1, 1);
    dim3 threads(clascl_bs, 1, 1);

    /* To do : implment the accuracy procedure */
    float mul = cto / cfrom;

    if (type == 'L' || type =='l')  
       l_clascl <<< grid, threads, 0, magma_stream >>> (m, n, mul, A, lda);
    else if (type == 'U' || type =='u')
       u_clascl <<< grid, threads, 0, magma_stream >>> (m, n, mul, A, lda);  
    else {
       printf("Only type L and U are available in clascl. Exit.\n");
       exit(1);
    }
}


