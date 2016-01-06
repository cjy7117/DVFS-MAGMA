/*
    -- MAGMA (version 1.6.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2014

       @generated from zgerbt_func_batched.cu normal z -> c, Sat Nov 15 19:53:59 2014

       @author Adrien Remy
       @author Azzam Haidar
*/
#include "common_magma.h"
#include "cgerbt.h"


#define block_height  32
#define block_width  4
#define block_length 256
#define NB 64
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
    Purpose
    -------
    CPRBT_MVT compute B = UTB to randomize B
    
    Arguments
    ---------
    @param[in]
    n       INTEGER
            The number of values of db.  n >= 0.

    @param[in]
    du     COMPLEX array, dimension (n,2)
            The 2*n vector representing the random butterfly matrix V
    
    @param[in,out]
    db     COMPLEX array, dimension (n)
            The n vector db computed by CGESV_NOPIV_GPU
            On exit db = du*db
    
    @param[in]
    queue   magma_queue_t
            Queue to execute in.
    ********************************************************************/
extern "C" void
magmablas_cprbt_mtv_batched_q(
    magma_int_t n, 
    magmaFloatComplex *du, magmaFloatComplex **db_array,
    magma_queue_t queue, magma_int_t batchCount)
{
    /*

     */
    magma_int_t threads = block_length;
    dim3 grid ( n/(4*block_length) + ((n%(4*block_length))!=0), batchCount);

    magmablas_capply_transpose_vector_kernel_batched<<< grid, threads, 0, queue >>>(n/2, du, n, db_array, 0);
    magmablas_capply_transpose_vector_kernel_batched<<< grid, threads, 0, queue >>>(n/2, du, n+n/2, db_array, n/2);

    threads = block_length;
    grid = n/(2*block_length) + ((n%(2*block_length))!=0), batchCount;
    magmablas_capply_transpose_vector_kernel_batched<<< grid, threads, 0, queue >>>(n, du, 0, db_array, 0);
}


/**
    @see magmablas_cprbt_mtv_q
    ********************************************************************/
extern "C" void
magmablas_cprbt_mtv_batched(
    magma_int_t n, 
    magmaFloatComplex *du, magmaFloatComplex **db_array, magma_int_t batchCount)
{
    magmablas_cprbt_mtv_batched_q(n, du, db_array, magma_stream, batchCount);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
    Purpose
    -------
    CPRBT_MV compute B = VB to obtain the non randomized solution
    
    Arguments
    ---------
    @param[in]
    n       INTEGER
            The number of values of db.  n >= 0.
    
    @param[in,out]
    db      COMPLEX array, dimension (n)
            The n vector db computed by CGESV_NOPIV_GPU
            On exit db = dv*db
    
    @param[in]
    dv      COMPLEX array, dimension (n,2)
            The 2*n vector representing the random butterfly matrix V
    
    @param[in]
    queue   magma_queue_t
            Queue to execute in.
    ********************************************************************/
extern "C" void
magmablas_cprbt_mv_batched_q(
    magma_int_t n, 
    magmaFloatComplex *dv, magmaFloatComplex **db_array,
    magma_queue_t queue, magma_int_t batchCount)
{

    magma_int_t threads = block_length;
    dim3 grid ( n/(2*block_length) + ((n%(2*block_length))!=0), batchCount);

    magmablas_capply_vector_kernel_batched<<< grid, threads, 0, queue >>>(n, dv, 0, db_array, 0);


    threads = block_length;
    grid = n/(4*block_length) + ((n%(4*block_length))!=0), batchCount;

    magmablas_capply_vector_kernel_batched<<< grid, threads, 0, queue >>>(n/2, dv, n, db_array, 0);
    magmablas_capply_vector_kernel_batched<<< grid, threads, 0, queue >>>(n/2, dv, n+n/2, db_array, n/2);


}



/**
    @see magmablas_cprbt_mtv_q
    ********************************************************************/
extern "C" void
magmablas_cprbt_mv_batched(
    magma_int_t n, 
    magmaFloatComplex *dv, magmaFloatComplex **db_array, magma_int_t batchCount)
{
    magmablas_cprbt_mv_batched_q(n, dv, db_array, magma_stream, batchCount);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
    Purpose
    -------
    CPRBT randomize a square general matrix using partial randomized transformation
    
    Arguments
    ---------
    @param[in]
    n       INTEGER
            The number of columns and rows of the matrix dA.  n >= 0.
    
    @param[in,out]
    dA      COMPLEX array, dimension (n,ldda)
            The n-by-n matrix dA
            On exit dA = duT*dA*d_V
    
    @param[in]
    ldda    INTEGER
            The leading dimension of the array dA.  LDA >= max(1,n).
    
    @param[in]
    du      COMPLEX array, dimension (n,2)
            The 2*n vector representing the random butterfly matrix U
    
    @param[in]
    dv      COMPLEX array, dimension (n,2)
            The 2*n vector representing the random butterfly matrix V
    
    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    ********************************************************************/
extern "C" void 
magmablas_cprbt_batched_q(
    magma_int_t n, 
    magmaFloatComplex **dA_array, magma_int_t ldda, 
    magmaFloatComplex *du, magmaFloatComplex *dv,
    magma_queue_t queue, magma_int_t batchCount)
{
    du += ldda;
    dv += ldda;

    dim3 threads(block_height, block_width);
    dim3 grid(n/(4*block_height) + ((n%(4*block_height))!=0), 
            n/(4*block_width)  + ((n%(4*block_width))!=0),
            batchCount);

    magmablas_celementary_multiplication_kernel_batched<<< grid, threads, 0, queue >>>(n/2, dA_array,            0, ldda, du,   0, dv,   0);
    magmablas_celementary_multiplication_kernel_batched<<< grid, threads, 0, queue >>>(n/2, dA_array,     ldda*n/2, ldda, du,   0, dv, n/2);
    magmablas_celementary_multiplication_kernel_batched<<< grid, threads, 0, queue >>>(n/2, dA_array,          n/2, ldda, du, n/2, dv,   0);
    magmablas_celementary_multiplication_kernel_batched<<< grid, threads, 0, queue >>>(n/2, dA_array, ldda*n/2+n/2, ldda, du, n/2, dv, n/2);

    dim3 threads2(block_height, block_width);
    dim3 grid2(n/(2*block_height) + ((n%(2*block_height))!=0), 
            n/(2*block_width)  + ((n%(2*block_width))!=0),
            batchCount);
    magmablas_celementary_multiplication_kernel_batched<<< grid2, threads2, 0, queue >>>(n, dA_array, 0, ldda, du, -ldda, dv, -ldda);
}


/**
    @see magmablas_cprbt_q
    ********************************************************************/
extern "C" void 
magmablas_cprbt_batched(
    magma_int_t n, 
    magmaFloatComplex **dA_array, magma_int_t ldda, 
    magmaFloatComplex *du, magmaFloatComplex *dv,
    magma_int_t batchCount)
{
    magmablas_cprbt_batched_q(n, dA_array, ldda, du, dv, magma_stream, batchCount);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

