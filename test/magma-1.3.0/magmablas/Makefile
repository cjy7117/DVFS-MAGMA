#//////////////////////////////////////////////////////////////////////////////
#   -- MAGMA (version 1.3.0) --
#      Univ. of Tennessee, Knoxville
#      Univ. of California, Berkeley
#      Univ. of Colorado, Denver
#      November 2012
#//////////////////////////////////////////////////////////////////////////////

MAGMA_DIR = ..
include ../Makefile.internal

INC += -I$(MAGMA_DIR)/control

ZHDR  = commonblas_z.h

# mostly alphabetic order
ZSRC := \
	zauxiliary.cu		\
	zcaxpycp.cu		\
	zclaswp.cu		\
	zgeadd.cu		\
	zhemv_fermi.cu		\
	zlacpy.cu		\
	zlag2c.cu		\
	clag2z.cu		\
	zlange.cu		\
	zlanhe.cu		\
	zlarf.cu		\
	zlarfg-v2.cu		\
	zlascl.cu		\
	zlaswp.cu		\
	zlat2c.cu		\
	zswap.cu		\
	zswapblk.cu		\
	zswapdblk.cu		\
	zsymmetrize.cu		\
	zsymmetrize_tiles.cu	\
	zsymv_fermi.cu		\
	csymv_tesla.cu		\
	ssyr2k.cu		\
	ztranspose.cu		\
	ztranspose-v2.cu	\
	zinplace_transpose.cu	\
	zgetmatrix_transpose.cu	\
	zsetmatrix_transpose.cu	\
	dtrsm.cu		\
	strsm.cu		\
	dznrm2.cu		\
	dtrsm_work.cu		\
	strsm_work.cu		\

# multi-GPU
ZSRC += \
	zbcyclic.cu			\
	zgetmatrix_transpose_mgpu.cu	\
	zsetmatrix_transpose_mgpu.cu	\
	zhemv_fermi_mgpu_32.cu		\
	zhemv_fermi_mgpu.cu		\

# not yet released
#	zhemm_1gpu.cpp			\
#	zhemm_1gpu_old.cpp		\
#	zhemm_mgpu.cpp			\
#	zhemm_mgpu_com.cpp		\
#	zhemm_mgpu_spec.cpp		\
#	zher2k_mgpu.cpp			\
#	zher2k_mgpu_spec.cpp		\

# Card-specific versions that need precisions generated must be above in ZSRC,
# so they are all put into Makefile.src in the release. The code is #ifdef'd
# for the correct card.

-include Makefile.local
-include Makefile.src

FERMISRC = \
	sgemv_fermi.cu	\
	dgemv_fermi.cu	\
	cgemv_fermi.cu	\
	zgemv_fermi.cu	\
	zgemvt_fermi.cu	\
	\
	sgemm_fermi.cu	\
	sgemm_fermi80.cu\
	sgemm_fermi64.cu\
	dgemm_fermi.cu	\
	zgemm_fermi.cu	\

TESLASRC = \
	sgemv_tesla.cu	\
	dgemv_tesla.cu	\
	cgemv_tesla.cu	\
	zgemv_tesla.cu	\
	gemv32_tesla.cu	\
	zsymv_tesla.cu	\
	zhemv_tesla.cu	\
	chemv_tesla.cu	\
	\
	sgemm_tesla.cu                            \
	sgemm_tesla_a_0.cu                        \
	sgemm_tesla_ab_0.cu                       \
	sgemm_tesla_N_N_64_16_16_16_4.cu          \
	sgemm_tesla_N_N_64_16_16_16_4_special.cu  \
	sgemm_tesla_N_T_64_16_4_16_4.cu           \
	sgemm_tesla_T_N_32_32_8_8_8.cu            \
	sgemm_tesla_T_T_64_16_16_16_4.cu          \
	\
	dgemm_tesla.cu                            \
	dgemm_tesla_a_0.cu                        \
	dgemm_tesla_ab_0.cu                       \
	dgemm_tesla_N_N_64_16_16_16_4.cu          \
	dgemm_tesla_N_N_64_16_16_16_4_special.cu  \
	dgemm_tesla_N_T_64_16_4_16_4.cu           \
	dgemm_tesla_T_N_32_32_8_8_8.cu            \
	dgemm_tesla_T_T_64_16_16_16_4.cu          \
	dgemm_tesla_T_T_64_16_16_16_4_v2.cu       \

ALLHDR := $(ZHDR) $(CHDR) $(DHDR) $(SHDR) \
	commonblas.h

ALLSRC := $(ZSRC) $(CSRC) $(DSRC) $(SSRC) \
	dgemv_MLU.cu	\
	stream.cu	\

ifeq (${GPU_TARGET}, Tesla)
    ALLSRC += $(TESLASRC)
else ifeq (${GPU_TARGET}, Fermi)
    ALLSRC += $(FERMISRC)
else ifeq (${GPU_TARGET}, Kepler)
    ALLSRC += $(FERMISRC)
endif

ALLOBJ := $(ALLSRC:.cu=.cu_o) 
ALLOBJ := $(ALLOBJ:.cpp=.o)

all: $(LIBMAGMABLAS) 

$(LIBMAGMABLAS): $(ALLHDR) $(ALLOBJ)
	$(ARCH) $(ARCHFLAGS) $@ $(ALLOBJ)
	$(RANLIB) $@

clean:
	rm -f *.cu_o *.o *~ *.linkinfo

cleanall: clean
	rm -f *.a
	rm -f $(LIBMAGMABLAS)
