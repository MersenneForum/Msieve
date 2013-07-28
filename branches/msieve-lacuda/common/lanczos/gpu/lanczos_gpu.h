/*--------------------------------------------------------------------
This source distribution is placed in the public domain by its author,
Jason Papadopoulos. You may use it for any purpose, free of charge,
without having to notify anyone. I disclaim any responsibility for any
errors.

Optionally, please be nice and tell me if you find this source to be
useful. Again optionally, if you add to the functionality present here
please consider making those additions public too, so that others may 
benefit from your work.	

$Id$
--------------------------------------------------------------------*/

#ifndef _COMMON_LANCZOS_GPU_LANCZOS_GPU_H_
#define _COMMON_LANCZOS_GPU_LANCZOS_GPU_H_

#include <cuda_xface.h>
#include "../lanczos.h"

#ifdef __cplusplus
extern "C" {
#endif

/* implementation-specific structure */

typedef struct {

	gpu_info_t *gpu_info;

	CUcontext gpu_context;
	CUmodule gpu_module;

	gpu_launch_t *launch;

	/* inner product data */

	CUdeviceptr inner_scratch;

	/* outer product data */

	CUdeviceptr outer_scratch;

	/* matrix data */

	CUtexref matmul_texref;

	CUdeviceptr *dense_blocks;

	CUdeviceptr block_num_entries;
	CUdeviceptr block_entries_start;
	CUdeviceptr block_entries;

} gpudata_t;


typedef struct {
	gpudata_t *gpudata;
	uint64 *host_vec;
	CUdeviceptr gpu_vec;
} gpuvec_t;

#define LANCZOS_GPU_DEBUG

/* ordinal list of GPU kernels */
enum {
	GPU_K_MASK = 0,
	GPU_K_XOR,
	GPU_K_INNER_PROD,
	GPU_K_OUTER_PROD,
	NUM_GPU_FUNCTIONS /* must be last */
};

void v_mul_64xN_Nx64_cpu(uint64 *x, uint64 *y,
		   uint64 *xy, uint32 n);

void v_mul_Nx64_64x64_acc_cpu(uint64 *v, uint64 *x,
			uint64 *y, uint32 n);

#ifdef __cplusplus
}
#endif

#endif /* !_COMMON_LANCZOS_GPU_LANCZOS_GPU_H_ */