#pragma once

#include "cucpp.h"

// Benchmark parameters for MGPU sort. We expose many options to test all aspects
// of the library.
struct MgpuTerms {
	int numBits;		// Number of bits in the sort.
	int bitPass;		// Target number of bits per pass (MGPU only).
						// If bitPass is zero, use the sortArray API.
	int numThreads;		// Number of threads in the sort block (MGPU only).
	int iterations;	
	bool reset;			// Update from the input array each iteration.
	bool earlyExit;
	bool useTransList;
	int valueCount;		// Set to -1 for index.
	int count;			// Number of elements.

	// Input, read-only, copied with cuMemcpyDtoD into the ping-pong storage.
	CuDeviceMem* randomKeys;
	CuDeviceMem* randomVals[6];

	// Output, copied with cuMemcpyDtoD from the ping-pong storage.
	CuDeviceMem* sortedKeys;
	CuDeviceMem* sortedVals[6];
};

sortStatus_t MgpuBenchmark(MgpuTerms& terms, sortEngine_t engine, double* elapsed);


