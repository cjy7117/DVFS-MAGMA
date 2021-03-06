/*
    -- MAGMA (version 1.6.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2014
       
       @author Azzam Haidar
       @author Tingxing Dong

       @generated from zpotrf_batched.cpp normal z -> s, Sat Nov 15 19:54:10 2014
*/
#include "common_magma.h"
#include "batched_kernel_param.h"
///////////////////////////////////////////////////////////////////////////////////////
/**
    Purpose
    -------
    SPOTRF computes the Cholesky factorization of a real symmetric
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
    dA      REAL array on the GPU, dimension (LDDA,N)
            On entry, the symmetric matrix dA.  If UPLO = MagmaUpper, the leading
            N-by-N upper triangular part of dA contains the upper
            triangular part of the matrix dA, and the strictly lower
            triangular part of dA is not referenced.  If UPLO = MagmaLower, the
            leading N-by-N lower triangular part of dA contains the lower
            triangular part of the matrix dA, and the strictly upper
            triangular part of dA is not referenced.
    \n
            On exit, if INFO = 0, the factor U or L from the Cholesky
            factorization dA = U**H * U or dA = L * L**H.

    @param[in]
    ldda     INTEGER
            The leading dimension of the array dA.  LDDA >= max(1,N).
            To benefit from coalescent memory accesses LDDA must be
            divisible by 16.

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value
      -     > 0:  if INFO = i, the leading minor of order i is not
                  positive definite, and the factorization could not be
                  completed.

    @ingroup magma_sposv_comp
    ********************************************************************/
extern "C" magma_int_t
magma_spotrf_batched(
    magma_uplo_t uplo, magma_int_t n,
    float **dA_array, magma_int_t ldda,
    magma_int_t *info_array,  magma_int_t batchCount)
{
#define A(i_, j_)  (A + (i_) + (j_)*ldda)   
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

    if( n > 2048 ){
        printf("=========================================================================================\n");
        printf("   WARNING batched routines are designed for small sizes it might be better to use the\n   Native/Hybrid classical routines if you want performance\n");
        printf("=========================================================================================\n");
    }


    magma_int_t j, k, ib;
    magma_int_t nb = POTRF_NB;
    magma_int_t gemm_crossover = 127;//nb > 32 ? 127 : 160;

#if defined(USE_CUOPT)    
    cublasHandle_t myhandle;
    cublasCreate_v2(&myhandle);
#else
    cublasHandle_t myhandle=NULL;
#endif

    float **dA_displ   = NULL;
    float **dW0_displ  = NULL;
    float **dW1_displ  = NULL;
    float **dW2_displ  = NULL;
    float **dW3_displ  = NULL;
    float **dW4_displ  = NULL;
    float **dinvA_array = NULL;
    float **dx_array    = NULL;

    magma_malloc((void**)&dA_displ,   batchCount * sizeof(*dA_displ));
    magma_malloc((void**)&dW0_displ,  batchCount * sizeof(*dW0_displ));
    magma_malloc((void**)&dW1_displ,  batchCount * sizeof(*dW1_displ));
    magma_malloc((void**)&dW2_displ,  batchCount * sizeof(*dW2_displ));
    magma_malloc((void**)&dW3_displ,  batchCount * sizeof(*dW3_displ));
    magma_malloc((void**)&dW4_displ,  batchCount * sizeof(*dW4_displ));
    magma_malloc((void**)&dinvA_array, batchCount * sizeof(*dinvA_array));
    magma_malloc((void**)&dx_array,    batchCount * sizeof(*dx_array));

    float* dinvA;
    float* dx;// dinvA and x are workspace in strsm
    magma_int_t invA_msize = ((n+TRI_NB-1)/TRI_NB)*TRI_NB*TRI_NB;
    magma_int_t x_msize = n*nb;
    magma_smalloc( &dinvA, invA_msize * batchCount);
    magma_smalloc( &dx,    x_msize * batchCount );
    sset_pointer(dx_array, dx, 1, 0, 0, x_msize, batchCount);
    sset_pointer(dinvA_array, dinvA, TRI_NB, 0, 0, invA_msize, batchCount);
    cudaMemset( dinvA, 0, batchCount * ((n+TRI_NB-1)/TRI_NB)*TRI_NB*TRI_NB * sizeof(float) );

    float **cpuAarray = NULL;
    magma_malloc_cpu((void**) &cpuAarray, batchCount*sizeof(float*));
    magma_getvector( batchCount, sizeof(float*), dA_array, 1, cpuAarray, 1);


    float d_alpha = -1.0;
    float d_beta  = 1.0;

    magma_queue_t cstream;
    magmablasGetKernelStream(&cstream);
    magma_int_t streamid;
    const magma_int_t nbstreams=32;
    magma_queue_t stream[nbstreams];
    for(k=0; k<nbstreams; k++){
        magma_queue_create( &stream[k] );
    }

    magmablasSetKernelStream(NULL);

    if (uplo == MagmaUpper) {
        printf("Upper side is unavailable \n");
        goto fin;
    }
    else {
        for(j = 0; j < n; j+=nb) {
            ib = min(nb, n-j);
#if 1
            //===============================================
            //  panel factorization
            //===============================================
            magma_sdisplace_pointers(dA_displ, dA_array, ldda, j, j, batchCount);
            sset_pointer(dx_array, dx, 1, 0, 0, x_msize, batchCount);
            sset_pointer(dinvA_array, dinvA, TRI_NB, 0, 0, invA_msize, batchCount);


            #if 0
            arginfo = magma_spotrf_panel_batched(
                               uplo, n-j, ib,
                               dA_displ, ldda,
                               dx_array, x_msize,
                               dinvA_array, invA_msize,
                               dW0_displ, dW1_displ, dW2_displ,
                               dW3_displ, dW4_displ,
                               info_array, j, batchCount, myhandle);
            #else
            //arginfo = magma_spotrf_rectile_batched(
            arginfo = magma_spotrf_recpanel_batched(
                               uplo, n-j, ib, 32,
                               dA_displ, ldda,
                               dx_array, x_msize,
                               dinvA_array, invA_msize,
                               dW0_displ, dW1_displ, dW2_displ,
                               dW3_displ, dW4_displ, 
                               info_array, j, batchCount, myhandle);
            #endif
            if(arginfo != 0 ) goto fin;
            //===============================================
            // end of panel
            //===============================================
#endif            
#if 1
            //real_Double_t gpu_time;
            //gpu_time = magma_sync_wtime(NULL);
            if( (n-j-ib) > 0){
                if( (n-j-ib) > gemm_crossover)   
                { 
                    //-------------------------------------------
                    //          USE STREAM  HERK
                    //-------------------------------------------
                    // since it use different stream I need to wait the panel.
                    // But since the code use the NULL stream everywhere, 
                    // so I don't need it, because the NULL stream do the sync by itself
                    //magma_queue_sync(NULL); 
                    /* you must know the matrix layout inorder to do it */  
                    for(k=0; k<batchCount; k++)
                    {
                        streamid = k%nbstreams;                                       
                        magmablasSetKernelStream(stream[streamid]);
                        // call herk, class ssyrk must call cpu pointer 
                        magma_ssyrk(MagmaLower, MagmaNoTrans, n-j-ib, ib, 
                            d_alpha, 
                            (const float*) cpuAarray[k] + j+ib+j*ldda, ldda, 
                            d_beta,
                            cpuAarray[k] + j+ib+(j+ib)*ldda, ldda);

                     }
                     // need to synchronise to be sure that panel do not start before
                     // finishing the update at least of the next panel
                     // BUT no need for it as soon as the other portion of the code 
                     // use the NULL stream which do the sync by itself 
                     //magma_device_sync(); 
                     magmablasSetKernelStream(NULL);
                }
                else
                {
                    //-------------------------------------------
                    //          USE BATCHED GEMM(which is a HERK in fact, since it only access the lower part)
                    //-------------------------------------------
                    magma_sdisplace_pointers(dA_displ, dA_array, ldda, j+ib, j, batchCount);
                    magma_sdisplace_pointers(dW1_displ, dA_array, ldda, j+ib, j+ib, batchCount);
                    magmablas_ssyrk_batched(uplo, MagmaNoTrans, n-j-ib, ib,
                                          d_alpha, dA_displ, ldda, 
                                          d_beta,  dW1_displ, ldda, 
                                          batchCount);
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
    magma_queue_sync(NULL);
    for(k=0; k<nbstreams; k++){
        magma_queue_destroy( stream[k] );
    }
    magmablasSetKernelStream(cstream);


#if defined(USE_CUOPT)    
    cublasDestroy_v2(myhandle);
#endif


    magma_free(dA_displ);
    magma_free(dW0_displ);
    magma_free(dW1_displ);
    magma_free(dW2_displ);
    magma_free(dW3_displ);
    magma_free(dW4_displ);

    magma_free(dinvA_array);
    magma_free(dx_array);
    magma_free(dinvA);
    magma_free(dx);
    magma_free_cpu(cpuAarray);

    return arginfo;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


