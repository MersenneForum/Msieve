#pragma once

#define MAX(a, b) (((a) >= (b)) ? (a) : (b))
#define MIN(a, b) (((a) <= (b)) ? (a) : (b))

#define ROUND_UP(a, b) (~((b) - 1) & ((a) + (b) - 1))
#define ROUND_DOWN(a, b) (~(b - 1) & a)

#define DIV_UP(a, b) (((a) + (b) - 1) / (b))

#define WARP_SIZE 32
#define LOG_WARP_SIZE 5


#ifndef NUM_WARPS
#define NUM_WARPS (NUM_THREADS / WARP_SIZE)
#else
#define NUM_THREADS (NUM_WARPS * WARP_SIZE)
#endif

#ifndef NO_VIDEO_INSTRUCTIONS
#define USE_VIDEO_INSTRUCTIONS
#endif

#include <device_functions.h>
#include <vector_functions.h>
#include <sm_11_atomic_functions.h>		// atomicAdd()

#define DEVICE extern "C" __device__ __forceinline__
#define DEVICE2 __device__ __forceinline__

typedef unsigned int uint;
typedef unsigned short uint16;


// retrieve numBits bits from x starting at bit
DEVICE uint bfe(uint x, uint bit, uint numBits) {
	uint ret;
#if __CUDA_ARCH__ >= 200
	asm("bfe.u32 %0, %1, %2, %3;" : "=r"(ret) : "r"(x), "r"(bit), "r"(numBits));
#else
	uint mask = (1 << numBits) - 1;
	ret = (x >> bit) & mask;
#endif
	return ret;
}


// insert the first numBits of y into x starting at bit
DEVICE uint bfi(uint x, uint y, uint bit, uint numBits) {
	uint ret;
#if __CUDA_ARCH__ >= 200
	asm("bfi.b32 %0, %1, %2, %3, %4;" : 
		"=r"(ret) : "r"(y), "r"(x), "r"(bit), "r"(numBits));
#else
	uint mask = (1 << numBits) - 1;
	ret = (x & ~(mask << bit)) | ((y & mask) << bit);
#endif
	return ret;
}

// Same syntax as __byte_perm, but without nvcc's __byte_perm bug that masks all
// non-immediate index arguments by 0x7777.
DEVICE uint prmt(uint a, uint b, uint index) {
	uint ret;
#if __CUDA_ARCH__ >= 200
	asm("prmt.b32 %0, %1, %2, %3;" : "=r"(ret) : "r"(a), "r"(b), "r"(index));
#else
	ret = __byte_perm(a, b, index);
#endif
	return ret;
}

DEVICE uint shl_add(uint a, uint b, uint c) {
#if defined(USE_VIDEO_INSTRUCTIONS) && __CUDA_ARCH__ >= 200
	uint ret;
	asm("vshl.u32.u32.u32.clamp.add %0, %1, %2, %3;" :
		"=r"(ret) : "r"(a), "r"(b), "r"(c));
	return ret;
#else
	return (a<< b) + c;
#endif
}

// (a<< b) + c, where b is a constant. We hope to use the ISCADD instruction 
// rather than the vshl.add instruction.
DEVICE uint shl_add_c(uint a, uint b, uint c) {
	return (a<< b) + c;
}

DEVICE uint shr_add(uint a, uint b, uint c) {
#if defined(USE_VIDEO_INSTRUCTIONS) && __CUDA_ARCH__ >= 200
	uint ret;
	asm("vshr.u32.u32.u32.clamp.add %0, %1, %2, %3;" : 
		"=r"(ret) : "r"(a), "r"(b), "r"(c));
	return ret;
#else
	return (a>> b) + c;
#endif
}

DEVICE uint mul_add(uint a, uint b, uint c) {
#if defined(USE_VIDEO_INSTRUCTIONS) && __CUDA_ARCH__ >= 200
	uint ret;
	asm("vmad.u32.u32.u32 %0, %1, %2, %3;" : 
		"=r"(ret) : "r"(a), "r"(b), "r"(c));
	return ret;
#else
	return (a * b) + c;
#endif
}

DEVICE uint imad(uint a, uint b, uint c) {
#if defined(USE_VIDEO_INSTRUCTIONS) && __CUDA_ARCH__ >= 200
	uint ret;
	asm("mad.lo.u32 %0, %1, %2, %3;" : "=r"(ret) : "r"(a), "r"(b), "r"(c));
	return ret;
#else
	return a * b + c;
#endif
}


DEVICE uint GetByte(uint a, uint i) {
	return prmt(a, 0, 0x4440 + i);
}
DEVICE uint ExpandUint8Low(uint a) {
	return prmt(a, 0, 0x4140);
}
DEVICE uint ExpandUint8High(uint a) {
	return prmt(a, 0, 0x4342);
}
DEVICE uint2 Expand8Uint4To8Uint8(uint a) {
	// b.x = (0xf & a) | // 0 -> 0
	// ((0xf0 & a) << 4) | // 4 -> 8
	// ((0xf00 & a) << 8) | // 8 -> 16
	// ((0xf000 & a) << 12); // 12 -> 24
	// b.y = ((0xf0000 & a) >> 16) | // 16 -> 0
	// ((0xf00000 & a) >> 12) | // 20 -> 8
	// ((0xf000000 & a) >> 8) | // 24 -> 16
	// ((0xf0000000 & a) >> 4); // 28 -> 24
	uint2 b;
	uint a2 = a>> 4;
	b.x = 0x0f0f0f0f & prmt(a, a2, 0x5140);
	b.y = 0x0f0f0f0f & prmt(a, a2, 0x7362);
	return b;
}

DEVICE uint StridedThreadOrder(uint index) {
	return index + (index / WARP_SIZE);
}


DEVICE uint LoadKey(const uint* keys_global, uint index, uint numElements, 
	bool checkRange) {
	uint key = 0xffffffff;
	if(checkRange)
		if(index < numElements) key = keys_global[index];
	else
		key = keys_global[index];
	return key;
}

// volatile qualifier appropriate for shared memory.
DEVICE2 uint LoadShifted(const volatile uint* shared, uint shiftedIndex) {
	return *((volatile uint*)(((volatile char*)shared) + shiftedIndex));
}
DEVICE2 void StoreShifted(volatile uint* shared, uint shiftedIndex, uint val) {
	*((volatile uint*)(((volatile char*)shared) + shiftedIndex)) = val;
}


// Put a float into radix order.
DEVICE float UintToFloat(uint u) {
	int adjusted = (int)u;
	
	// Negative now has high bit set, positive has high bit clear.
	int flipped = adjusted - 0x80000000;
	
	// Fill the register with set bits if negative.	
	int bits = flipped>> 31;

	int x = flipped ^ (0x7fffffff & bits);

	float f = __int_as_float(x);
	return f;
}

// Put a radix order into back into a float.
DEVICE uint FloatToUint(float f) {
	int x = __float_as_int(f);
	int bits = x>> 31;

	int flipped = x ^ (0x7fffffff & bits);

	int adjusted = 0x80000000 + flipped;

	uint u = (uint)adjusted;
	return u;
}
