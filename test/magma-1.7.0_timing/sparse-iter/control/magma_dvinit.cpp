/*
    -- MAGMA (version 1.7.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date September 2015

       @generated from magma_zvinit.cpp normal z -> d, Fri Sep 11 18:29:46 2015
       @author Hartwig Anzt
*/
#include "common_magmasparse.h"


/**
    Purpose
    -------

    Allocates memory for magma_d_matrix and initializes it
    with the passed value.


    Arguments
    ---------

    @param[out]
    x           magma_d_matrix*
                vector to initialize

    @param[in]
    mem_loc     magma_location_t
                memory for vector

    @param[in]
    num_rows    magma_int_t
                desired length of vector
                
    @param[in]
    num_cols    magma_int_t
                desired width of vector-block (columns of dense matrix)

    @param[in]
    values      double
                entries in vector

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_daux
    ********************************************************************/

extern "C" magma_int_t
magma_dvinit(
    magma_d_matrix *x,
    magma_location_t mem_loc,
    magma_int_t num_rows,
    magma_int_t num_cols,
    double values,
    magma_queue_t queue )
{
    magma_int_t info = 0;
    
    // set queue for old dense routines
    magma_queue_t orig_queue = NULL;
    magmablasGetKernelStream( &orig_queue );

    x->val = NULL;
    x->diag = NULL;
    x->row = NULL;
    x->rowidx = NULL;
    x->col = NULL;
    x->blockinfo = NULL;
    x->dval = NULL;
    x->ddiag = NULL;
    x->drow = NULL;
    x->drowidx = NULL;
    x->dcol = NULL;
    x->storage_type = Magma_DENSE;
    x->memory_location = mem_loc;
    x->sym = Magma_GENERAL;
    x->diagorder_type = Magma_VALUE;
    x->fill_mode = MagmaFull;
    x->num_rows = num_rows;
    x->num_cols = num_cols;
    x->nnz = num_rows*num_cols;
    x->max_nnz_row = num_cols;
    x->diameter = 0;
    x->blocksize = 1;
    x->numblocks = 1;
    x->alignment = 1;
    x->major = MagmaColMajor;
    x->ld = num_rows;
    if ( mem_loc == Magma_CPU ) {
        CHECK( magma_dmalloc_cpu( &x->val, x->nnz ));
        for( magma_int_t i=0; i<x->nnz; i++) {
             x->val[i] = values;
        }
    }
    else if ( mem_loc == Magma_DEV ) {
        CHECK( magma_dmalloc( &x->val, x->nnz ));
        magmablas_dlaset(MagmaFull, x->num_rows, x->num_cols, values, values, x->val, x->num_rows);
    }
    
cleanup:
    magmablasSetKernelStream( orig_queue );
    return info; 
}
