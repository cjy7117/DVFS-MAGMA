/*
    -- MAGMA (version 1.6.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2015

       @generated from magma_zvpass_gpu.cpp normal z -> c, Fri Jan 30 19:00:32 2015
       @author Hartwig Anzt
*/

//  in this file, many routines are taken from 
//  the IO functions provided by MatrixMarket

#include <fstream>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <iostream>
#include <ostream>
#include <assert.h>
#include <stdio.h>

#include "magmasparse_c.h"
#include "magma.h"
#include "mmio.h"


using namespace std;




/**
    Purpose
    -------

    Passes a vector to MAGMA (located on DEV).

    Arguments
    ---------

    @param[in]
    m           magma_int_t 
                number of rows

    @param[in]
    n           magma_int_t 
                number of columns

    @param[in]
    val         magmaFloatComplex_ptr 
                array containing vector entries

    @param[out]
    v           magma_c_vector*
                magma vector
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_caux
    ********************************************************************/

extern "C"
magma_int_t
magma_cvset_dev(
    magma_int_t m, magma_int_t n, 
    magmaFloatComplex_ptr val,
    magma_c_vector *v,
    magma_queue_t queue )
{
    v->num_rows = m;
    v->num_cols = n;
    v->nnz = m*n;
    v->memory_location = Magma_DEV;
    v->dval = val;
    v->major = MagmaColMajor;

    return MAGMA_SUCCESS;
}


/**
    Purpose
    -------

    Passes a MAGMA vector back (located on DEV).

    Arguments
    ---------

    @param[in]
    v           magma_c_vector
                magma vector

    @param[out]
    m           magma_int_t 
                number of rows

    @param[out]
    n           magma_int_t 
                number of columns

    @param[out]
    val         magmaFloatComplex_ptr 
                array containing vector entries

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_caux
    ********************************************************************/

extern "C"
magma_int_t
magma_cvget_dev(
    magma_c_vector v,
    magma_int_t *m, magma_int_t *n, 
    magmaFloatComplex_ptr *val,
    magma_queue_t queue )
{
    if ( v.memory_location == Magma_DEV ) {

        *m = v.num_rows;
        *n = v.num_cols;
        *val = v.dval;
    } else {
        magma_c_vector v_DEV;
        magma_c_vtransfer( v, &v_DEV, v.memory_location, Magma_DEV, queue ); 
        magma_cvget_dev( v_DEV, m, n, val, queue );
        magma_c_vfree( &v_DEV, queue );
    }
    return MAGMA_SUCCESS;
}


