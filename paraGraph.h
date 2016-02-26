#ifndef __PARAGRAPH_H__
#define __PARAGRAPH_H__

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include "vertex_set.h"
#include "graph.h"
#include <set>
#include "mic.h"
#include "graph_internal.h"
#include <omp.h>
/*
 * edgeMap --
 *
 * Students will implement this function.
 *
 * The input argument f is a class with the following methods defined:
 *   bool update(Vertex src, Vertex dst)
 *   bool cond(Vertex v)
 *
 * See apps/bfs.cpp for an example of such a class definition.
 *
 * When the argument removeDuplicates is false, the implementation of
 * edgeMap need not remove duplicate vertices from the VertexSet it
 * creates when iterating over edges.  This is a performance
 * optimization when the application knows (and can tell ParaGraph)
 * that f.update() guarantees that duplicate vertices cannot appear in
 * the output vertex set.
 *
 * Further notes: the implementation of edgeMap is templated on the
 * type of this object, which allows for higher performance code
 * generation as these methods will be inlined.
 */
template <class F>
static VertexSet *edgeMapBottomUp(Graph g, VertexSet *u, F &f,
                                  bool removeDuplicates = true)
{
	//printf("bottom up\n");
	bool* newDenseVertices = new bool[u->numNodes]();
	int sum = 0;
	bool *ptrDenseVertices = u->denseVertices;
	int chunkSize = std::max(1, u->numNodes / (omp_get_num_threads() * 64));

	#pragma omp parallel for default(none) shared(g, f, newDenseVertices, ptrDenseVertices, chunkSize) schedule(dynamic,256)
	for (int i = 0; i < g->num_nodes; i++) {
		if (f.cond(i) && !newDenseVertices[i])
		{
			const Vertex* start = incoming_begin(g, i);
			const Vertex* end = incoming_end(g, i);
			for (const Vertex* v = start; v != end && f.cond(i); v++) {
				if (ptrDenseVertices[*v] == true  && f.updateNoWorries(*v, i)) {
					newDenseVertices[i] = true;
				}
				
			}
		}
	}
	int uNumNodes = u->numNodes;
	int sum_degrees = 0;
	#pragma omp parallel for reduction(+:sum,sum_degrees) default(none) shared(uNumNodes, newDenseVertices,g) schedule(dynamic,256)
	for (int i = 0; i < uNumNodes; ++i)
	{
		if (newDenseVertices[i])
		{
			sum += newDenseVertices[i];
			sum_degrees += outgoing_size(g, i);
		}

	}
	return newVertexSet(DENSE, sum ,  u->numNodes, newDenseVertices, sum_degrees);
	// }
}

template <class F>
static VertexSet *edgeMap(Graph g, VertexSet * u, F & f,
                          bool removeDuplicates = true)
{
	int threshold = u->numNodes / 20;
	int sum_degrees;
	sum_degrees = u->sum_degrees;
	if (sum_degrees > threshold||u->size>u->numNodes/100)
	{
		updateDense(u, true);
		return edgeMapBottomUp(g, u, f);
	}
	//printf("Converting to Sparse\n");
	updateSparse(u, true);
	sum_degrees = 0;
	Vertex *ptrVertices = u->vertices;
	int uSize = u->size;
	//printf("topdown\n");

	// if (removeDuplicates)
	{	bool* newDenseVertices = new bool[u->numNodes]();
		int uNumNodes = u->numNodes;
		int sum_degrees = 0;
		int sum = 0;
		#pragma omp parallel default(none)  shared(uSize, ptrVertices, newDenseVertices, f, g, uNumNodes)  reduction(+:sum,sum_degrees)
		{
			#pragma omp for
			for (int i = 0; i < uSize; ++i)
			{
				Vertex v = ptrVertices[i];
				const Vertex* start = outgoing_begin(g, v);
				const Vertex* end = outgoing_end(g, v);
				for (const Vertex* neigh = start; neigh != end; neigh++) {
					if (f.cond(*neigh) && f.update(v, *neigh))
					{
						newDenseVertices[*neigh] = true;
					}
				}
			}
			

			#pragma omp for schedule(dynamic, 256)
			for (int i = 0; i < uNumNodes; ++i)
			{
				if (newDenseVertices[i])
				{
					sum += newDenseVertices[i];
					sum_degrees += outgoing_size(g, i);
				}

			}
		}
		return newVertexSet(DENSE, sum ,  u->numNodes, newDenseVertices, sum_degrees);
	}
	// int* finalNeighbours = new int[sum_degrees];
	// prefix_sum(offsets, degrees, u->size);
	// offsets[0] = 0;
	// #pragma omp parallel for default(none) shared(g, uSize, ptrVertices, f, offsets, finalNeighbours)
	// for (int i = 0; i < uSize; ++i)
	// {
	// 	Vertex v = ptrVertices[i];
	// 	int offset = offsets[i];
	// 	const Vertex* start = outgoing_begin(g, v);
	// 	const Vertex* end = outgoing_end(g, v);
	// 	int j = 0;
	// 	for (const Vertex* neigh = start; neigh != end; neigh++, j++) {
	// 		if (f.cond(*neigh) && f.update(v, *neigh))
	// 		{
	// 			finalNeighbours[offset + j] = *neigh;
	// 		}
	// 		else {
	// 			finalNeighbours[offset + j] = -1;
	// 		}
	// 	}
	// }

	// if (removeDuplicates) {
	// 	remDuplicates(finalNeighbours, sum_degrees, u->numNodes);
	// }
	// Vertex* newSparseVertices = new Vertex[sum_degrees];

	// static bool* tempBoolArray = new bool[threshold]();
	// #pragma omp parallel for default(none) shared(sum_degrees, finalNeighbours, tempBoolArray)
	// for (int i = 0; i < sum_degrees; ++i)
	// {
	// 	if (finalNeighbours[i] >= 0)
	// 	{
	// 		tempBoolArray[i] = true;
	// 	} else {
	// 		tempBoolArray[i] = false;
	// 	}
	// }
	// int new_sum = packIndices(newSparseVertices,  finalNeighbours, tempBoolArray, sum_degrees);
	// VertexSet *trueResult = newVertexSet(SPARSE, sum_degrees, u->numNodes, newSparseVertices, new_sum);
	// delete[] offsets;
	// delete[] finalNeighbours;
	// return trueResult;
}



/*
 * vertexMap --
 *
 * Students will implement this function.
 *
 * The input argument f is a class with the following methods defined:
 *   bool operator()(Vertex v)
 *
 * See apps/kBFS.cpp for an example implementation.
 *
 * Note that you'll call the function on a vertex as follows:
 *    Vertex v;
 *    bool result = f(v)
 *
 * If returnSet is false, then the implementation of vertexMap should
 * return NULL (it need not build and create a vertex set)
 */
template <class F>
static VertexSet *vertexMap(VertexSet * u, F & f, bool returnSet = true)
{

	if (returnSet) {
		updateDense(u, true);
		bool* newDenseVertices = new bool[u->numNodes]();
		int sum = 0;
		int uNumNodes = u->numNodes;
		bool* ptrDenseVertices = u->denseVertices;
		#pragma omp parallel for default (none) shared(f,uNumNodes, ptrDenseVertices, newDenseVertices) reduction(+:sum) schedule(dynamic,256)
		for (int i = 0; i < uNumNodes; ++i)
		{
			if (ptrDenseVertices[i])
			{
				newDenseVertices[i] = f(i);
				sum += newDenseVertices[i];
			}
		}

		return newVertexSet(DENSE, sum, u->numNodes, newDenseVertices);
	}
	else {
		if (u->type == SPARSE)
		{
			int uSize = u->size;
			Vertex *ptrVertices = u->vertices;
			# pragma omp parallel for default(none) shared(uSize, f, ptrVertices)
			for (int i = 0; i < uSize; i++)
				f(ptrVertices[i]);
		}
		else {
			int uNumNodes = u->numNodes;
			bool *ptrDenseVertices = u->denseVertices;
			# pragma omp parallel for default(none) shared(uNumNodes, f, ptrDenseVertices) schedule(dynamic,256)
			for (int i = 0; i < uNumNodes; ++i)
			{
				ptrDenseVertices[i]&&f(i);

			}
		}

	}
	return NULL;
}

#endif /* __PARAGRAPH_H__ */
