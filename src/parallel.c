// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS		4
#define MAX_NODES 		1000
static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;
static pthread_mutex_t sum_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t visited_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool visited[MAX_NODES] = {false};
static bool enqueuing[MAX_NODES] = {false};
static bool stop = false;

typedef struct {
	unsigned int node_index;
} GraphTaskArg;

static GraphTaskArg task_args[MAX_NODES] = {{0}};

static void process_node(unsigned int idx) {
	pthread_mutex_lock(&visited_mutex);

	if (visited[idx] || stop) {
		pthread_mutex_unlock(&visited_mutex);
		return;
	}

	os_node_t *node = graph->nodes[idx];

	pthread_mutex_lock(&sum_mutex);
	sum += node->info;
	pthread_mutex_unlock(&sum_mutex);
	visited[idx] = true;
	pthread_mutex_unlock(&visited_mutex);
	

	for (unsigned int iterator = 0; iterator < node->num_neighbours; iterator++) {
		unsigned int neighbor_index = node->neighbours[iterator];
		process_node(node->neighbours[iterator]);
	}

}

int main(int argc, char *argv[]) {
	FILE *input_file;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

	tp = create_threadpool(NUM_THREADS);
	process_node(0);

	pthread_mutex_init(&visited_mutex, NULL);

	os_task_t *neighbor_task = create_task(process_node, 0, NULL);
	enqueue_task(tp, neighbor_task);

	wait_for_completion(tp);
	destroy_threadpool(tp);

	printf("%d", sum);

	return 0;
}
