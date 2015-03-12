/*
 * jacobi.c
 *
 *  Created on: Feb 26, 2015
 *      Author: nzhang-dev
 */

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <omp.h>

#include "zjacobi.c"
#include "kernel.c"


#define NDIM 3

#define stdEncode(x, y, z) ((x) * array_size * array_size + (y) * array_size + (z))

int main(int argc, char* argv[])
{
	printf("Allocating problem\n");
	uint64_t array_size = 9;
	uint32_t iterations = 1;
	uint64_t num_threads_tmp = omp_get_max_threads();
//	num_threads_tmp = 2;
	uint64_t num_threads = 1;
	uint64_t parallel_bits = 0;

	if (argc > 1)
	{
		array_size = atoi((const char*) argv[1]);
	}
	if (argc > 2)
	{
		iterations = atoi((const char*) argv[2]);
	}
	if (argc > 3)
	{
		num_threads_tmp = atoi((const char*) argv[3]);
	}

	while (num_threads < num_threads_tmp)
	{
		num_threads <<= 1;
		parallel_bits++;
	}
	if (num_threads > num_threads_tmp)
	{
		num_threads >>= 1;
		parallel_bits--;
	}
	int bits = array_size;
	array_size = 1 << array_size;
	uint64_t actual_size = 1;
	for(int i = 0; i < NDIM; i++)
	{
		actual_size *= array_size;
	}
	printf("Malloccing\n");
	uint32_t *data = (uint32_t*) malloc(sizeof(uint32_t) * actual_size);
	uint32_t *out = (uint32_t*) malloc(sizeof(uint32_t) * actual_size);
	if(data == NULL)
	{
		printf("Allocation failed");
		return 1;
	}
	printf("Malloc succeeded\n");
	int64_t neighborhood_deltas[NEIGHBORS][NDIM] = {{-1, -1, -1}, {-1, -1, 0}, {-1, -1, 1}, {-1, 0, -1}, {-1, 0, 0}, {-1, 0, 1}, {-1, 1, -1}, {-1, 1, 0}, {-1, 1, 1}, {0, -1, -1}, {0, -1, 0}, {0, -1, 1}, {0, 0, -1}, {0, 0, 0}, {0, 0, 1}, {0, 1, -1}, {0, 1, 0}, {0, 1, 1}, {1, -1, -1}, {1, -1, 0}, {1, -1, 1}, {1, 0, -1}, {1, 0, 0}, {1, 0, 1}, {1, 1, -1}, {1, 1, 0}, {1, 1, 1}};
	printf("allocating delta matrix\n");
	uint64_t neighborhood_encoded_deltas[NEIGHBORS];

	printf("Reindexing\n");
	for (uint64_t i = 0; i < NEIGHBORS; i++)
	{
		neighborhood_encoded_deltas[i] = encode(neighborhood_deltas[i]);
	}
	for (int i = 0; i < NEIGHBORS; i++)
	{
		printf("%d\t", neighborhood_encoded_deltas[i]);
	}
	printf("\n");
	printf("Finished Reindexing\n");
	for(uint32_t z = 0; z < array_size; z++)
	{
		uint64_t index[] = {0, 0, z};
		uint64_t addr = encode(index);
		data[addr] = 729;
	}
	printf("Allocation succeeded\n");

	printf("Found %d threads, %d parallel bits\n", num_threads, parallel_bits);
	double start = omp_get_wtime();
	//printf("Start time: %f", start/CLOCKS_PER_SEC);

	for (int iter = 0; iter < iterations; iter++)
	{
		printf("Starting iteration %d\n", iter);
		//private(partition) private(neighborhood) shared(data) shared(out) shared(neighborhood_encoded_deltas)
		#pragma omp parallel for num_threads(num_threads)
		for (uint64_t partition = 0; partition < num_threads; partition++)
		{
			uint32_t neighborhood[NEIGHBORS];
			uint64_t partition_mask = partition << ((bits*NDIM) - parallel_bits);
			for (uint64_t i = 0; i < (actual_size >> parallel_bits); i++)
			{

				uint64_t index = i | partition_mask;
				//printf("i: %u\tpartition:%u\tindex:%d\n", i, partition, index);
				//#pragma omp critical
				{
				for (uint64_t neighborhood_index = 0; neighborhood_index < NEIGHBORS; neighborhood_index++)
				{
					uint64_t delta = neighborhood_encoded_deltas[neighborhood_index];
//					printf("Index: %u\t", index);
					uint64_t ind = add(index, delta);
//					printf("ind: %u\t", ind);
					clamp(&ind);
//					printf("code: %u\n", ind);
					neighborhood[neighborhood_index] = data[ind];
				}
				}
				out[index] = kernel(neighborhood);

			}

		}
	}
	double end = omp_get_wtime();

	//reusing Data as output, reordered
	for (uint32_t x = 0; x < array_size; x++)
	{
		for(uint32_t y = 0; y < array_size; y++)
		{
			for(uint32_t z = 0; z < array_size; z++)
			{
				uint64_t code[] = {x, y, z};
				uint64_t encoded = encode(code);
				data[stdEncode(x, y, z)] = out[encoded];
			}
		}
	}

	float elapsed = end - start;
	printf("Total time: %f\n", elapsed);
	dump(data, array_size * 2);
	free(data);
	free(out);
	return 0;
}
