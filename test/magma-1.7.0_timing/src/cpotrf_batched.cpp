/*
    -- MAGMA (version 1.7.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date September 2015
       
       @author Azzam Haidar
       @author Tingxing Dong

       @generated from zpotrf_batched.cpp normal z -> c, Fri Sep 11 18:29:32 2015
*/
#include "common_magma.h"
#include "batched_kernel_param.h"
#include "cublas_v2.h"
///////////////////////////////////////////////////////////////////////////////////////
/**
    Purpose
    -------
    CPOTRF computes the Cholesky factorization of a complex Hermitian
    positive definite matrix dA.

    The factorization has the form
        dA = U**H * U,   if UPLO = MagmaUpper, or
        dA = L  * L**H,  if UPLO = MagmaLower,
    where U is an upper triangular matrix and L is lower triangular.

    This is the block version of the algorithm, calling Level 3 BLAS.
    If the current stream is NULL, this version replaces it with a new
    stream to overlap computation with communication.

    Arguments
    ---------
    @param[in]
    uplo    magma_uplo_t
      -     = MagmaUpper:  Upper triangle of dA is stored;
      -     = MagmaLower:  Lower triangle of dA is stored.

    @param[in]
    n       INTEGER
            The order of the matrix dA.  N >= 0.

    @param[in,out]
    dA_array      Array of pointers, dimension (batchCount).
             Each is a COMPLEX array on the GPU, dimension (LDDA,N)
             On entry, each pointer is a Hermitian matrix dA.  
             If UPLO = MagmaUpper, the leading
             N-by-N upper triangular part of dA contains the upper
             triangular part of the matrix dA, and the strictly lower
             triangular part of dA is not referenced.  If UPLO = MagmaLower, the
             leading N-by-N lower triangular part of dA contains the lower
             triangular part of the matrix dA, and the strictly upper
             triangular part of dA is not referenced.
    \n
             On exit, if corresponding entry in info_array = 0, 
             each pointer is the factor U or L from the Cholesky
             factorization dA = U**H * U or dA = L * L**H.

    @param[in]
    ldda     INTEGER
            The leading dimension of each array dA.  LDDA >= max(1,N).
            To benefit from coalescent memory accesses LDDA must be
            divisible by 16.

    @param[out]
    info_array    Array of INTEGERs, dimension (batchCount), for corresponding matrices.
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value
      -     > 0:  if INFO = i, the leading minor of order i is not
                  positive definite, and the factorization could not be
                  completed.
    
    @param[in]
    batchCount  INTEGER
                The number of matrices to operate on.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_cposv_comp
    ********************************************************************/
extern "C" magma_int_t
magma_cpotrf_lg_batched(
    magma_uplo_t uplo, magma_int_t n,
    magmaFloatComplex **dA_array, magma_int_t ldda,
    magma_int_t *info_array,  magma_int_t batchCount, magma_queue_t queue)
{
    magma_int_t arginfo = 0;

#define A(i_, j_)  (A + (i_) + (j_)*ldda)   
    float d_alpha = -1.0;
    float d_beta  = 1.0;

    if ( n > 2048 ) {
        #ifndef MAGMA_NOWARNING
        printf("=========================================================================================\n");
        printf("   WARNING batched routines are designed for small sizes it might be better to use the\n   Native/Hybrid classical routines if you want performance\n");
        printf("=========================================================================================\n");
        #endif
    }


    magma_int_t j, k, ib, use_stream;
    magma_int_t nb, recnb;
    magma_get_cpotrf_batched_nbparam(n, &nb, &recnb);

    cublasHandle_t myhandle;
    cublasCreate_v2(&myhandle);
    cublasSetStream(myhandle, queue);



    magmaFloatComplex **dA_displ   = NULL;
    magmaFloatComplex **dW0_displ  = NULL;
    magmaFloatComplex **dW1_displ  = NULL;
    magmaFloatComplex **dW2_displ  = NULL;
    magmaFloatComplex **dW3_displ  = NULL;
    magmaFloatComplex **dW4_displ  = NULL;
    magmaFloatComplex **dinvA_array = NULL;
    magmaFloatComplex **dwork_array = NULL;

    magma_malloc((void**)&dA_displ,   batchCount * sizeof(*dA_displ));
    magma_malloc((void**)&dW0_displ,  batchCount * sizeof(*dW0_displ));
    magma_malloc((void**)&dW1_displ,  batchCount * sizeof(*dW1_displ));
    magma_malloc((void**)&dW2_displ,  batchCount * sizeof(*dW2_displ));
    magma_malloc((void**)&dW3_displ,  batchCount * sizeof(*dW3_displ));
    magma_malloc((void**)&dW4_displ,  batchCount * sizeof(*dW4_displ));
    magma_malloc((void**)&dinvA_array, batchCount * sizeof(*dinvA_array));
    magma_malloc((void**)&dwork_array,    batchCount * sizeof(*dwork_array));

    magma_int_t invA_msize = magma_roundup( n, TRI_NB )*TRI_NB;
    magma_int_t dwork_msize = n*nb;
    magmaFloatComplex* dinvA      = NULL;
    magmaFloatComplex* dwork      = NULL; // dinvA and dwork are workspace in ctrsm
    magmaFloatComplex **cpuAarray = NULL;
    magma_cmalloc( &dinvA, invA_msize * batchCount);
    magma_cmalloc( &dwork, dwork_msize * batchCount );
    magma_malloc_cpu((void**) &cpuAarray, batchCount*sizeof(magmaFloatComplex*));
   /* check allocation */
    if ( dA_displ  == NULL || dW0_displ == NULL || dW1_displ   == NULL || dW2_displ   == NULL || 
         dW3_displ == NULL || dW4_displ == NULL || dinvA_array == NULL || dwork_array == NULL || 
         dinvA     == NULL || dwork     == NULL || cpuAarray   == NULL ) {
        magma_free(dA_displ);
        magma_free(dW0_displ);
        magma_free(dW1_displ);
        magma_free(dW2_displ);
        magma_free(dW3_displ);
        magma_free(dW4_displ);
        magma_free(dinvA_array);
        magma_free(dwork_array);
        magma_free( dinvA );
        magma_free( dwork );
        magma_free_cpu(cpuAarray);
        magma_int_t info = MAGMA_ERR_DEVICE_ALLOC;
        magma_xerbla( __func__, -(info) );
        return info;
    }
    magmablas_claset_q(MagmaFull, invA_msize, batchCount, MAGMA_C_ZERO, MAGMA_C_ZERO, dinvA, invA_msize, queue);
    magmablas_claset_q(MagmaFull, dwork_msize, batchCount, MAGMA_C_ZERO, MAGMA_C_ZERO, dwork, dwork_msize, queue);
    cset_pointer(dwork_array, dwork, 1, 0, 0, dwork_msize, batchCount, queue);
    cset_pointer(dinvA_array, dinvA, TRI_NB, 0, 0, invA_msize, batchCount, queue);


    magma_int_t streamid;
    const magma_int_t nbstreams=10;
    magma_queue_t stream[nbstreams];
    for (k=0; k < nbstreams; k++) {
        magma_queue_create( &stream[k] );
    }
    magma_getvector( batchCount, sizeof(magmaFloatComplex*), dA_array, 1, cpuAarray, 1);

    if (uplo == MagmaUpper) {
        printf("Upper side is unavailable \n");
        goto fin;
    }
    else {
        for (j = 0; j < n; j += nb) {
            ib = min(nb, n-j);
#if 1
            //===============================================
            //  panel factorization
            //===============================================
            magma_cdisplace_pointers(dA_displ, dA_array, ldda, j, j, batchCount, queue);
            cset_pointer(dwork_array, dwork, 1, 0, 0, dwork_msize, batchCount, queue);
            cset_pointer(dinvA_array, dinvA, TRI_NB, 0, 0, invA_msize, batchCount, queue);


            if (recnb == nb)
            {
                arginfo = magma_cpotrf_panel_batched(
                                   uplo, n-j, ib,
                                   dA_displ, ldda,
                                   dwork_array, dwork_msize,
                                   dinvA_array, invA_msize,
                                   dW0_displ, dW1_displ, dW2_displ,
                                   dW3_displ, dW4_displ,
                                   info_array, j, batchCount, myhandle, queue);
            }
            else {
                //arginfo = magma_cpotrf_rectile_batched(
                arginfo = magma_cpotrf_recpanel_batched(
                                   uplo, n-j, ib, recnb,
                                   dA_displ, ldda,
                                   dwork_array, dwork_msize,
                                   dinvA_array, invA_msize,
                                   dW0_displ, dW1_displ, dW2_displ,
                                   dW3_displ, dW4_displ, 
                                   info_array, j, batchCount, myhandle, queue);
            }
            if (arginfo != 0 ) goto fin;
            //===============================================
            // end of panel
            //===============================================
#endif            
#if 1
            //real_Double_t gpu_time;
            //gpu_time = magma_sync_wtime(NULL);
            if ( (n-j-ib) > 0) {
                use_stream = magma_crecommend_cublas_gemm_stream(MagmaNoTrans, MagmaConjTrans, n-j-ib, n-j-ib, ib);
                if (use_stream)
                { 
                    //-------------------------------------------
                    //          USE STREAM  HERK
                    //-------------------------------------------
                    // since it use different stream I need to wait the panel.
                    // But since the code use the NULL stream everywhere, 
                    // so I don't need it, because the NULL stream do the sync by itself
                    //magma_queue_sync(NULL); 
                    /* you must know the matrix layout inorder to do it */  
                    magma_queue_sync(queue); 
                    for (k=0; k < batchCount; k++)
                    {
                        streamid = k%nbstreams;                                       
                        magmablasSetKernelStream(stream[streamid]);
                        // call herk, class cherk must call cpu pointer 
                        magma_cherk(MagmaLower, MagmaNoTrans, n-j-ib, ib, 
                            d_alpha, 
                            (const magmaFloatComplex*) cpuAarray[k] + j+ib+j*ldda, ldda, 
                            d_beta,
                            cpuAarray[k] + j+ib+(j+ib)*ldda, ldda);
                     }
                     // need to synchronise to be sure that panel do not start before
                     // finishing the update at least of the next panel
                     // BUT no need for it as soon as the other portion of the code 
                     // use the NULL stream which do the sync by itself 
                     //magma_device_sync(); 
                     if ( queue != NULL ) {
                         for (magma_int_t s=0; s < nbstreams; s++)
                             magma_queue_sync(stream[s]);
                     }
                     magmablasSetKernelStream(queue);
                }
                else
                {
                    //-------------------------------------------
                    //          USE BATCHED GEMM(which is a HERK in fact, since it only access the lower part)
                    //-------------------------------------------
                    magma_cdisplace_pointers(dA_displ, dA_array, ldda, j+ib, j, batchCount, queue);
                    magma_cdisplace_pointers(dW1_displ, dA_array, ldda, j+ib, j+ib, batchCount, queue);
                    magmablas_cherk_batched(uplo, MagmaNoTrans, n-j-ib, ib,
                                          d_alpha, dA_displ, ldda, 
                                          d_beta,  dW1_displ, ldda, 
                                          batchCount, queue);
                }
            } 
            //gpu_time = magma_sync_wtime(NULL) - gpu_time;
            //real_Double_t flops = (n-j-ib) * (n-j-ib) * ib / 1e9 * batchCount;
            //real_Double_t gpu_perf = flops / gpu_time;
            //printf("Rows= %d, Colum=%d, herk time = %7.2fms, Gflops= %7.2f\n", n-j-ib, ib, gpu_time*1000, gpu_perf);
#endif
        }
    }

fin:
    magmablasSetKernelStream(queue);
    magma_queue_sync(queue);
    for (k=0; k < nbstreams; k++) {
        magma_queue_destroy( stream[k] );
    }
    cublasDestroy_v2(myhandle);

    magma_free(dA_displ);
    magma_free(dW0_displ);
    magma_free(dW1_displ);
    magma_free(dW2_displ);
    magma_free(dW3_displ);
    magma_free(dW4_displ);
    magma_free(dinvA_array);
    magma_free(dwork_array);
    magma_free( dinvA );
    magma_free( dwork );
    magma_free_cpu(cpuAarray);

    return arginfo;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern "C" magma_int_t
magma_cpotrf_batched(
    magma_uplo_t uplo, magma_int_t n,
    magmaFloatComplex **dA_array, magma_int_t ldda,
    magma_int_t *info_array,  magma_int_t batchCount, magma_queue_t queue)
{
    cudaMemset(info_array, 0, batchCount*sizeof(magma_int_t));
    magma_int_t arginfo = 0;
    
    if ( uplo != MagmaUpper && uplo != MagmaLower) {
        arginfo = -1;
    } else if (n < 0) {
        arginfo = -2;
    } else if (ldda < max(1,n)) {
        arginfo = -4;
    }

    if (arginfo != 0) {
        magma_xerbla( __func__, -(arginfo) );
        return arginfo;
    }

    // Quick return if possible
    if (n == 0) {
        return arginfo;
    }
    

    magma_int_t crossover = magma_get_cpotrf_batched_crossover();

    if (n > crossover )
    {   
        // The memory allocation/deallocation inside this routine takes about 3-4ms 
        arginfo = magma_cpotrf_lg_batched(uplo, n, dA_array, ldda, info_array, batchCount, queue);
    }
    else
    {
        #if defined(VERSION20)
            arginfo = magma_cpotrf_lpout_batched(uplo, n, dA_array, ldda, 0, info_array, batchCount, queue);
        #elif defined(VERSION33)
            arginfo = magma_cpotrf_v33_batched(uplo, n, dA_array, ldda, info_array, batchCount, queue);
        #elif defined(VERSION31)
            arginfo = magma_cpotrf_lpin_batched(uplo, n, dA_array, ldda, 0, info_array, batchCount, queue);
        #else
            printf("ERROR NO VERSION CHOSEN\n");
        #endif
    }
    magma_queue_sync(queue);

    return arginfo;
}
