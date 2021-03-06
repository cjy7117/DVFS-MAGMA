#//////////////////////////////////////////////////////////////////////////////
#   -- MAGMA (version 1.4.1) --
#      Univ. of Tennessee, Knoxville
#      Univ. of California, Berkeley
#      Univ. of Colorado, Denver
#      December 2013
#//////////////////////////////////////////////////////////////////////////////

DIR = control
MAGMA_DIR = ..
include ../Makefile.internal

ZSRC := \
	zpanel_to_q.cpp	\
	zprint.cpp	\

ifeq ($(PGIFORTRAN), 0)
ZSRC += \
	magma_f77.cpp		\
	magma_param.f90		\
	magma.f90
else
ZSRC += \
	magma_zf77pgi.cpp
endif

SRC := \
	auxiliary.cpp		\
	get_nb.cpp		\
	timer.cpp		\
	trace.cpp		\
	strlcpy.cpp		\
	bulge_auxiliary.cpp	\
	affinity.cpp		\
	connection_mgpu.cpp	\
	pthread_barrier.cpp	\
	xerbla.cpp		\
	magma_threadsetting.cpp	\
	magmawinthread.cpp	\

# Fortran wrappers are generated by 'make wrappers'
# They don't directly use precision generation;
# the headers that they are generated from use precision generation.
SRC += \
	magma_sf77.cpp		\
	magma_df77.cpp		\
	magma_cf77.cpp		\
	magma_zf77.cpp		\
	magma_sfortran.F90	\
	magma_dfortran.F90	\
	magma_cfortran.F90	\
	magma_zfortran.F90	\

-include Makefile.local
-include Makefile.src

ALLSRC := $(ZSRC) $(CSRC) $(DSRC) $(SSRC) $(SRC)

ALLOBJ := $(ALLSRC:.cpp=.o)
ALLOBJ := $(ALLOBJ:.f90=.o)
ALLOBJ := $(ALLOBJ:.F90=.o)

.PHONY: all clean cleanall

.DEFAULT_GOAL := all
all: $(LIBMAGMA)

$(LIBMAGMA): stamp
	$(ARCH) $(ARCHFLAGS) $@ $(ALLOBJ)
	$(RANLIB) $@
	cp *.mod ../include

# this forces LIBMAGMA to be archived
stamp: $(ALLOBJ)
	touch $@

clean:
	rm -f *.cu_o *.o *~ *.linkinfo *.mod ../include/*.mod sizeptr stamp

cleanall: clean
	rm -f $(LIBMAGMA)

wrappers: ../include/magma_s.i ../include/magma_d.i ../include/magma_c.i ../include/magma_z.i
	../tools/fortran_wrappers.pl ../include/magma_s.i
	../tools/fortran_wrappers.pl ../include/magma_d.i
	../tools/fortran_wrappers.pl ../include/magma_c.i
	../tools/fortran_wrappers.pl ../include/magma_z.i
	rm $^

# Extra dependencies for .mod files
magma.o: magma_zfortran.o magma_cfortran.o magma_dfortran.o magma_sfortran.o 
magma_zfortran.o: magma_param.o
magma_cfortran.o: magma_param.o
magma_dfortran.o: magma_param.o
magma_sfortran.o: magma_param.o
