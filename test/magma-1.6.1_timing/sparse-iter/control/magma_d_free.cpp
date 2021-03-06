/*
    -- MAGMA (version 1.6.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2015

       @generated from magma_z_free.cpp normal z -> d, Fri Jan 30 19:00:32 2015
       @author Hartwig Anzt
*/

#include <fstream>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <iostream>
#include <ostream>
#include <assert.h>
#include <stdio.h>
#include "magmasparse_d.h"
#include "magma.h"
#include "mmio.h"



using namespace std;








/**
    Purpose
    -------

    Free the memory of a magma_d_vector.


    Arguments
    ---------

    @param[i,out]
    x           magma_d_vector*
                vector to free    
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_daux
    ********************************************************************/

extern "C" magma_int_t
magma_d_vfree(
    magma_d_vector *x,
    magma_queue_t queue )
{
    if ( x->memory_location == Magma_CPU ) {
        magma_free_cpu( x->val );
        x->num_rows = 0;
        x->nnz = 0;
        x->val = NULL;
        return MAGMA_SUCCESS;     
    }
    else if ( x->memory_location == Magma_DEV ) {
        if ( magma_free( x->dval ) != MAGMA_SUCCESS ) {
            printf("Memory Free Error.\n");  
            return MAGMA_ERR_INVALID_PTR;
        }
        
        x->num_rows = 0;
        x->nnz = 0;
        x->dval = NULL;
        return MAGMA_SUCCESS;     
    }
    else {
        printf("Memory Free Error.\n");  
        return MAGMA_ERR_INVALID_PTR;
    }
}


/**
    Purpose
    -------

    Free the memory of a magma_d_sparse_matrix.


    Arguments
    ---------

    @param[in,out]
    A           magma_d_sparse_matrix*
                matrix to free    
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_daux
    ********************************************************************/

extern "C" magma_int_t
magma_d_mfree(
    magma_d_sparse_matrix *A,
    magma_queue_t queue )
{
    if ( A->memory_location == Magma_CPU ) {
       if ( A->storage_type == Magma_ELL || A->storage_type == Magma_ELLPACKT ){
            magma_free_cpu( A->val );
            magma_free_cpu( A->col );
            A->num_rows = 0;
            A->num_cols = 0;
            A->nnz = 0;                      
        } 
        if (A->storage_type == Magma_ELLD ) {
            magma_free_cpu( A->val );
            magma_free_cpu( A->col );
            A->num_rows = 0;
            A->num_cols = 0;
            A->nnz = 0;                       
        } 
        if ( A->storage_type == Magma_ELLRT ) {
            magma_free_cpu( A->val );
            magma_free_cpu( A->row );
            magma_free_cpu( A->col );
            A->num_rows = 0;
            A->num_cols = 0;
            A->nnz = 0;                        
        } 
        if ( A->storage_type == Magma_SELLP ) {
            magma_free_cpu( A->val );
            magma_free_cpu( A->row );
            magma_free_cpu( A->col );
            A->num_rows = 0;
            A->num_cols = 0;
            A->nnz = 0;                      
        } 
        if ( A->storage_type == Magma_CSR || A->storage_type == Magma_CSC 
                                        || A->storage_type == Magma_CSRD
                                        || A->storage_type == Magma_CSRL
                                        || A->storage_type == Magma_CSRU ) {
            magma_free_cpu( A->val );
            magma_free_cpu( A->col );
            magma_free_cpu( A->row );
            A->num_rows = 0;
            A->num_cols = 0;
            A->nnz = 0;                      
        } 
        if (  A->storage_type == Magma_CSRCOO ) {
            magma_free_cpu( A->val );
            magma_free_cpu( A->col );
            magma_free_cpu( A->row );
            magma_free_cpu( A->rowidx );
            A->num_rows = 0;
            A->num_cols = 0;
            A->nnz = 0;                     
        } 
        if ( A->storage_type == Magma_BCSR ) {
            magma_free_cpu( A->val );
            magma_free_cpu( A->col );
            magma_free_cpu( A->row );
            magma_free_cpu( A->blockinfo );
            A->num_rows = 0;
            A->num_cols = 0;
            A->nnz = 0; 
            A->blockinfo = 0;                    
        } 
        if ( A->storage_type == Magma_DENSE ) {
            magma_free_cpu( A->val );
            A->num_rows = 0;
            A->num_cols = 0;
            A->nnz = 0;                      
        } 
        A->val = NULL;
        A->col = NULL;
        A->row = NULL;
        A->rowidx = NULL;
        A->blockinfo = NULL;
        A->diag = NULL;
        A->dval = NULL;
        A->dcol = NULL;
        A->drow = NULL;
        A->drowidx = NULL;
        A->ddiag = NULL;
        return MAGMA_SUCCESS; 
    }

    if ( A->memory_location == Magma_DEV ) {
       if ( A->storage_type == Magma_ELL || A->storage_type == Magma_ELLPACKT ){
            if ( magma_free( A->dval ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }
            if ( magma_free( A->dcol ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }

            A->num_rows = 0;
            A->num_cols = 0;
            A->nnz = 0;                    
        } 
        if ( A->storage_type == Magma_ELLD ) {
            if ( magma_free( A->dval ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }
            if ( magma_free( A->dcol ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }

            A->num_rows = 0;
            A->num_cols = 0;
            A->nnz = 0;                   
        } 
        if ( A->storage_type == Magma_ELLRT ) {
            if ( magma_free( A->dval ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }
            if ( magma_free( A->drow ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }
            if ( magma_free( A->dcol ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }

            A->num_rows = 0;
            A->num_cols = 0;
            A->nnz = 0;                         
        } 
        if ( A->storage_type == Magma_SELLP ) {
            if ( magma_free( A->dval ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }
            if ( magma_free( A->drow ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }
            if ( magma_free( A->dcol ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }
            A->num_rows = 0;
            A->num_cols = 0;
            A->nnz = 0;                  
        } 
        if ( A->storage_type == Magma_CSR || A->storage_type == Magma_CSC 
                                        || A->storage_type == Magma_CSRD
                                        || A->storage_type == Magma_CSRL
                                        || A->storage_type == Magma_CSRU ) {
            if ( magma_free( A->dval ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }
            if ( magma_free( A->drow ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }
            if ( magma_free( A->dcol ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }

            A->num_rows = 0;
            A->num_cols = 0;
            A->nnz = 0;                    
        } 
        if (  A->storage_type == Magma_CSRCOO ) {
            if ( magma_free( A->dval ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }
            if ( magma_free( A->drow ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }
            if ( magma_free( A->dcol ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }
            if ( magma_free( A->drowidx ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }

            A->num_rows = 0;
            A->num_cols = 0;
            A->nnz = 0;                      
        } 
        if ( A->storage_type == Magma_BCSR ) {
            if ( magma_free( A->dval ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }
            if ( magma_free( A->drow ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }
            if ( magma_free( A->dcol ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }
            free( A->blockinfo );
            A->num_rows = 0;
            A->num_cols = 0;
            A->nnz = 0;                  
        } 
        if ( A->storage_type == Magma_DENSE ) {
            if ( magma_free( A->dval ) != MAGMA_SUCCESS ) {
                printf("Memory Free Error.\n");  
                return MAGMA_ERR_INVALID_PTR;
                
            }

            A->num_rows = 0;
            A->num_cols = 0;
            A->nnz = 0;        
                
        }   
        A->val = NULL;
        A->col = NULL;
        A->row = NULL;
        A->rowidx = NULL;
        A->blockinfo = NULL;
        A->diag = NULL;
        A->dval = NULL;
        A->dcol = NULL;
        A->drow = NULL;
        A->drowidx = NULL;
        A->ddiag = NULL;
        return MAGMA_SUCCESS; 
    }

    else {
        printf("Memory Free Error.\n");  
        return MAGMA_ERR_INVALID_PTR;
    }
    return MAGMA_SUCCESS;
}



   


