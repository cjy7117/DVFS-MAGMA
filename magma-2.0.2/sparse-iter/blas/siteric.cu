/*
    -- MAGMA (version 2.0.2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date May 2016

       @generated from sparse-iter/blas/ziteric.cu normal z -> s, Mon May  2 23:30:48 2016

*/
#include "magmasparse_internal.h"

#define PRECISION_s


__global__ void 
magma_siteric_csr_kernel(    
    magma_int_t n, 
    magma_int_t nnz, 
    magma_index_t *Arowidx, 
    magma_index_t *Acolidx, 
    const float * __restrict__  A_val,
    magma_index_t *rowptr, 
    magma_index_t *colidx, 
    float *val )
{
    int i, j;
    int k = (blockDim.x * blockIdx.x + threadIdx.x); // % nnz;


    float zero = MAGMA_S_MAKE(0.0, 0.0);
    float s, sp;
    int il, iu, jl, ju;

    if ( k < nnz )
    {     
        i = Arowidx[k];
        j = Acolidx[k];

#if (__CUDA_ARCH__ >= 350) && (defined(PRECISION_d) || defined(PRECISION_s))
        s = __ldg( A_val+k );
#else
        s = A_val[k];
#endif

        il = rowptr[i];
        iu = rowptr[j];

        while (il < rowptr[i+1] && iu < rowptr[j+1])
        {
            sp = zero;
            jl = colidx[il];
            ju = colidx[iu];

            if (jl < ju)
                il++;
            else if (ju < jl)
                iu++;
            else
            {
                // we are going to modify this u entry
                sp = val[il] * val[iu];
                s -= sp;
                il++;
                iu++;
            }
        }
        // undo the last operation (it must be the last)
        s += sp;
        __syncthreads();

        // modify entry
        if (i == j)
            val[il-1] = MAGMA_S_MAKE( sqrt( fabs( MAGMA_S_REAL(s) )), 0.0 );
        else
            val[il-1] =  s / val[iu-1];
    }
}// kernel 










/**
    Purpose
    -------
    
    This routine iteratively computes an incomplete Cholesky factorization.
    The idea is according to Edmond Chow's presentation at SIAM 2014.
    This routine was used in the ISC 2015 paper:
    E. Chow et al.: 'Study of an Asynchronous Iterative Algorithm
                     for Computing Incomplete Factorizations on GPUs'
                     
    The input format of the initial guess matrix A is Magma_CSRCOO,
    A_CSR is CSR or CSRCOO format. 

    Arguments
    ---------

    @param[in]
    A           magma_s_matrix
                input matrix A - initial guess (lower triangular)

    @param[in,out]
    A_CSR       magma_s_matrix
                input/output matrix containing the IC approximation
                
    @param[in]
    queue       magma_queue_t
                Queue to execute in.
                
    @ingroup magmasparse_sgegpuk
    ********************************************************************/

extern "C" magma_int_t
magma_siteric_csr( 
    magma_s_matrix A,
    magma_s_matrix A_CSR,
    magma_queue_t queue )
{
    int blocksize1 = 128;
    int blocksize2 = 1;

    int dimgrid1 = magma_ceildiv( A.nnz, blocksize1 );
    int dimgrid2 = 1;
    int dimgrid3 = 1;

    // Runtime API
    // cudaFuncCachePreferShared: shared memory is 48 KB
    // cudaFuncCachePreferEqual: shared memory is 32 KB
    // cudaFuncCachePreferL1: shared memory is 16 KB
    // cudaFuncCachePreferNone: no preference
    //cudaFuncSetCacheConfig(cudaFuncCachePreferShared);

    cudaDeviceSetCacheConfig( cudaFuncCachePreferL1 );

    dim3 grid( dimgrid1, dimgrid2, dimgrid3 );
    dim3 block( blocksize1, blocksize2, 1 );
    magma_siteric_csr_kernel<<< grid, block, 0, queue->cuda_stream() >>>
            ( A.num_rows, A.nnz, 
              A.rowidx, A.col, A.val, 
              A_CSR.row, A_CSR.col,  A_CSR.val );

    return MAGMA_SUCCESS;
}
