/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

       @author Mark Gates
       @generated s Wed Nov 14 22:52:29 2012
*/
#include "common_magma.h"

// -------------------------
// Put 0s in the upper triangular part of a panel and 1s on the diagonal.
// Stores previous values in work array, to be restored later with sq_to_panel.
extern "C"
void spanel_to_q(char uplo, magma_int_t ib, float *A, magma_int_t lda, float *work)
{
    int i, j, k = 0;
    float *col;
    float c_zero = MAGMA_S_ZERO;
    float c_one  = MAGMA_S_ONE;
    
    if (uplo == 'U' || uplo == 'u'){
        for(i = 0; i < ib; ++i){
            col = A + i*lda;
            for(j = 0; j < i; ++j){
                work[k] = col[j];
                col [j] = c_zero;
                ++k;
            }
            
            work[k] = col[i];
            col [j] = c_one;
            ++k;
        }
    }
    else {
        for(i=0; i<ib; ++i){
            col = A + i*lda;
            work[k] = col[i];
            col [i] = c_one;
            ++k;
            for(j=i+1; j<ib; ++j){
                work[k] = col[j];
                col [j] = c_zero;
                ++k;
            }
        }
    }
}


// -------------------------
// Restores a panel, after call to spanel_to_q.
extern "C"
void sq_to_panel(char uplo, magma_int_t ib, float *A, magma_int_t lda, float *work)
{
    int i, j, k = 0;
    float *col;
    
    if (uplo == 'U' || uplo == 'u'){
        for(i = 0; i < ib; ++i){
            col = A + i*lda;
            for(j = 0; j <= i; ++j){
                col[j] = work[k];
                ++k;
            }
        }
    }
    else {
        for(i = 0; i < ib; ++i){
            col = A + i*lda;
            for(j = i; j < ib; ++j){
                col[j] = work[k];
                ++k;
            }
        }
    }
}
