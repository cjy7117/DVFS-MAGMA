/*
    -- MAGMA (version 1.7.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date September 2015

       @generated from zherk_batched.cpp normal z -> s, Fri Sep 11 18:29:22 2015

       @author Jakub Kurzak
       @author Stan Tomov
       @author Mark Gates
       @author Azzam Haidar

       [zcds]gemm_fermi.cu          defines the CPU driver.
       [zcds]gemm_fermi_kernels.h   defines the block sizes for each precision.
       gemm_stencil_defs.h          defines types and functions for precision-independent code.
       
       These files are included multiple times, once for each transpose version.
       herk_stencil.cuh             defines the GPU kernel (device function).
       herk_kernel_batched.cuh              defines the GPU kernel (global function).
       
       The batched version uses herk_kernel_batched.cuh instead of herk_kernel.cuh.
*/
#include "common_magma.h"
#include "commonblas_s.h"

#define PRECISION_s

///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////
/**
    Purpose
    -------
    SSYRK performs one of the symmetric rank k operations

    C := alpha*A*A**H + beta*C,

    or

    C := alpha*A**H*A + beta*C,

    where alpha and beta are real scalars, C is an n by n symmetric
    matrix and A is an n by k matrix in the first case and a k by n
    matrix in the second case.
    
    Parameters
    ----------

    @param[in]
    uplo    CHARACTER*1.
           On entry, uplo specifies whether the upper or lower
           triangular part of the array C is to be referenced as
           follows:

           uplo = 'U' or 'u' Only the upper triangular part of C
           is to be referenced.

           uplo = 'L' or 'l' Only the lower triangular part of C
           is to be referenced.
    
    @param[in]
    trans   CHARACTER*1.
            On entry, trans specifies the operation to be performed as
            follows:

            trans = 'N' or 'n' C := alpha*A*A**H + beta*C.

            trans = 'C' or 'c' C := alpha*A**H*A + beta*C.

    @param[in]
    n       INTEGER.
            On entry,  specifies the order of the matrix C. N must be
            at least zero.
    
    @param[in]
    k       INTEGER.
            On entry with trans = 'N' or 'n', k specifies the number
            of columns of the matrix A, and on entry with
            trans = 'C' or 'c', k specifies the number of rows of the
            matrix A. K must be at least zero.

    @param[in]
    alpha   REAL
            On entry, ALPHA specifies the scalar alpha.
    
    @param[in]
    dA_array      Array of pointers, dimension (batchCount).
             Each is a REAL A array of DIMENSION ( ldda, ka ), where ka is
             k  when  trans = MagmaNoTrans,  and is  n  otherwise.
             Before entry with  trans = MagmaNoTrans,  the leading  m by k
             part of the array A must contain the matrix A, otherwise
             the leading  k by m  part of the array A must contain  the
             matrix A.
    
    @param[in]
    ldda    INTEGER.
            On entry, ldda specifies the first dimension of each array A as declared
            in the calling (sub) program. When  trans = MagmaNoTrans then
            ldda must be at least  max( 1, n ), otherwise  ldda must be at
            least  max( 1, k ).
    
    @param[in]
    beta    REAL.
            On entry,  BETA  specifies the scalar  beta.  When  BETA  is
            supplied as zero then C need not be set on input.
    
    @param[in,out]
    dC_array      Array of pointers, dimension (batchCount).
             Each is a REAL array C of DIMENSION ( lddc, n ).
             Before entry with uplo = 'U' or 'u', the leading n by n
             upper triangular part of the array C must contain the upper
             triangular part of the symmetric matrix and the strictly
             lower triangular part of C is not referenced. On exit, the
             upper triangular part of the array C is overwritten by the
             upper triangular part of the updated matrix.
             Before entry with uplo = 'L' or 'l', the leading n by n
             lower triangular part of the array C must contain the lower
             triangular part of the symmetric matrix and the strictly
             upper triangular part of C is not referenced. On exit, the
             lower triangular part of the array C is overwritten by the
             lower triangular part of the updated matrix.
             Note that the imaginary parts of the diagonal elements need
             not be set, they are assumed to be zero, and on exit they
             are set to zero.

    @param[in]
    lddc    INTEGER.
            On entry, lddc specifies the first dimension of each array C as declared
            in  the  calling  (sub)  program.   lddc  must  be  at  least
            max( 1, m ).
    
    @param[in]
    batchCount  INTEGER
                The number of matrices to operate on.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_sblas3
    ********************************************************************/
extern "C" void
magma_ssyrk_batched(
    magma_uplo_t uplo, magma_trans_t trans, magma_int_t n, magma_int_t k,
    float alpha,
    float const * const * dA_array, magma_int_t ldda,
    float beta,
    float **dC_array, magma_int_t lddc, magma_int_t batchCount, magma_queue_t queue )
{
    magmablas_ssyrk_batched(
    uplo, trans, n, k,
    alpha, dA_array, ldda,
    beta, dC_array, lddc, 
    batchCount, queue );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
