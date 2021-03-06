/*
    -- MAGMA (version 1.6.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2013
       
   @author Azzam Haidar
   @author Adrien Remy

   @generated from zgetrf_panel_nopiv_batched.cpp normal z -> c, Sat Nov 15 19:54:10 2014
*/
#include "common_magma.h"
////////////////////////////////////////////////////////////////////////////////////////
extern "C" magma_int_t
magma_cgetrf_panel_nopiv_batched_q(
    magma_int_t m, magma_int_t nb,    
    magmaFloatComplex** dA_array,    magma_int_t ldda,
    magmaFloatComplex** dX_array,    magma_int_t dX_length,
    magmaFloatComplex** dinvA_array, magma_int_t dinvA_length,
    magmaFloatComplex** dW0_displ, magmaFloatComplex** dW1_displ,  
    magmaFloatComplex** dW2_displ, magmaFloatComplex** dW3_displ,
    magmaFloatComplex** dW4_displ,     
    magma_int_t *info_array, magma_int_t gbstep,  
    magma_int_t batchCount, magma_queue_t stream, cublasHandle_t myhandle)
{
     magma_int_t arginfo = 0;
    //===============================================
    //  panel factorization
    //===============================================
    if(m < nb){
        printf("magma_cgetrf_panel_nopiv_batched_q m < nb %d < %d \n",(int) m, (int) nb);
        return -101;
    }

#if 0
    arginfo = magma_cgetf2_nopiv_batched(
                       m, nb,
                       dA_array, ldda,
                       dW1_displ, dW2_displ, dW3_displ,
                       info_array, gbstep, batchCount, myhandle);
    if (arginfo != 0) return arginfo;
#else
    arginfo = magma_cgetf2_nopiv_batched(
                       nb, nb,
                       dA_array, ldda,
                       dW1_displ, dW2_displ, dW3_displ,
                       info_array, gbstep, batchCount, myhandle);
    if (arginfo != 0) return arginfo;
    if((m-nb) > 0){
        magma_cdisplace_pointers(dW0_displ, dA_array, ldda, nb, 0, batchCount);
        magmablas_ctrsm_work_batched(MagmaRight, MagmaUpper, MagmaNoTrans, MagmaNonUnit,
                              1, m-nb, nb, 
                              MAGMA_C_ONE,
                              dA_array,    ldda, 
                              dW0_displ,   ldda, 
                              dX_array,    m-nb, 
                              dinvA_array, dinvA_length, 
                              dW1_displ,   dW2_displ, 
                              dW3_displ,   dW4_displ,
                              1, batchCount);
    }
#endif
    return 0;
}


////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////
extern "C" magma_int_t
magma_cgetrf_recpanel_nopiv_batched_q(
    magma_int_t m, magma_int_t n, magma_int_t min_recpnb,    
    magmaFloatComplex** dA_array,    magma_int_t ldda,
    magmaFloatComplex** dX_array,    magma_int_t dX_length,
    magmaFloatComplex** dinvA_array, magma_int_t dinvA_length,
    magmaFloatComplex** dW1_displ, magmaFloatComplex** dW2_displ,  
    magmaFloatComplex** dW3_displ, magmaFloatComplex** dW4_displ,
    magmaFloatComplex** dW5_displ, 
    magma_int_t *info_array, magma_int_t gbstep,
    magma_int_t batchCount, magma_queue_t stream, cublasHandle_t myhandle)
{
    // Quick return if possible
    if (m == 0 || n == 0) {
        return 0;
    }
    magma_int_t arginfo = 0;


    magmaFloatComplex **dA_displ  = NULL;
    magma_malloc((void**)&dA_displ,   batchCount * sizeof(*dA_displ));
    
    magma_int_t panel_nb = n;
    if(panel_nb <= min_recpnb){
        // if(DEBUG>0)printf("calling bottom panel recursive with m=%d nb=%d\n",m,n);
        //  panel factorization
        //magma_cdisplace_pointers(dA_displ, dA_array, ldda, 0, 0, batchCount);
        arginfo = magma_cgetrf_panel_nopiv_batched_q(
                           m, panel_nb, 
                           dA_array, ldda,
                           dX_array, dX_length,
                           dinvA_array, dinvA_length,
                           dW1_displ, dW2_displ,
                           dW3_displ, dW4_displ, dW5_displ,
                           info_array, gbstep, batchCount, stream, myhandle);
        if (arginfo != 0) return arginfo;
    }
    else{
        // split A over two [A A2]
        // panel on A1, update on A2 then panel on A1    
        magma_int_t n1 = n/2;
        magma_int_t n2 = n-n1;
        magma_int_t m1 = m;
        magma_int_t m2 = m-n1;
        magma_int_t p1 = 0;
        magma_int_t p2 = n1;
        // panel on A1
        //printf("calling recursive panel on A1 with m=%d nb=%d min_recpnb %d\n",m1,n1,min_recpnb);
        magma_cdisplace_pointers(dA_displ, dA_array, ldda, p1, p1, batchCount); 
        arginfo = magma_cgetrf_recpanel_nopiv_batched_q(
                           m1, n1, min_recpnb,
                           dA_displ, ldda,
                           dX_array, dX_length,
                           dinvA_array, dinvA_length,
                           dW1_displ, dW2_displ,
                           dW3_displ, dW4_displ, dW5_displ,
                           info_array, gbstep, batchCount, stream, myhandle);
        if (arginfo != 0) return arginfo;

        // update A2
        //printf("calling update A2 with             m=%d n=%d k=%d\n",m2,n2,n1);
        
        magma_cdisplace_pointers(dW5_displ, dA_array, ldda, p1, p2, batchCount); 
        magmablas_ctrsm_work_batched(MagmaLeft, MagmaLower, MagmaNoTrans, MagmaUnit, 1,
                              n1, n2,
                              MAGMA_C_ONE,
                              dA_displ,    ldda, // dA
                              dW5_displ,   ldda, // dB
                              dX_array,  n1, // dX
                              dinvA_array, dinvA_length,
                              dW1_displ,   dW2_displ, 
                              dW3_displ,   dW4_displ,
                              1, batchCount);

        magma_cdisplace_pointers(dW1_displ, dA_array, ldda, p2, 0, batchCount); 
        magma_cdisplace_pointers(dA_displ, dA_array, ldda, p2, p2, batchCount); 
        magmablas_cgemm_batched( MagmaNoTrans, MagmaNoTrans, m2, n2, n1, 
                              MAGMA_C_NEG_ONE, dW1_displ, ldda, 
                              dW5_displ, ldda, 
                              MAGMA_C_ONE,  dA_displ, ldda, 
                              batchCount);
        // panel on A2
        //printf("calling recursive panel on A2 with m=%d nb=%d min_recpnb %d\n",m2,n2,min_recpnb);
        arginfo = magma_cgetrf_recpanel_nopiv_batched_q(
                           m2, n2, min_recpnb,
                           dA_displ, ldda,
                           dX_array, dX_length,
                           dinvA_array, dinvA_length,
                           dW1_displ, dW2_displ,
                           dW3_displ, dW4_displ, dW5_displ,
                           info_array, gbstep+p2, batchCount, stream, myhandle);
        if (arginfo != 0) return arginfo;
    }

    magma_free(dA_displ);
    return 0;
}


////////////////////////////////////////////////////////////////////////////////////////

