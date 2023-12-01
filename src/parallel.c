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
static pthread_mutex_t sum_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex for sum
static bool visited[MAX_NODES] = {false};  // Array to track visited nodes
static bool enqueuing[MAX_NODES] = {false};  
static bool stop = false;  // Variable to signal threadpool to stop

typedef struct {
    unsigned int node_index;
} GraphTaskArg;

static GraphTaskArg task_args[MAX_NODES] = {{0}};
static int all_nodes_visited_flag = 0;  // Flag to signal when all nodes are visited

static void process_node(unsigned int idx);

static void process_node_wrapper(void *arg) {
    GraphTaskArg *task_arg = (GraphTaskArg *)arg;
    process_node(task_arg->node_index);
}

static int all_nodes_visited() {
    for (unsigned int i = 0; i < graph->num_nodes; i++) {
        if (!visited[i]) {
            return 0;  // Return false if any node is not visited
        }
    }
    return 1;  // Return true if all nodes are visited
}

static os_task_t *create_graph_task(unsigned int node_index) {
    task_args[node_index].node_index = node_index;
    return create_task(process_node_wrapper, (void *)&task_args[node_index], NULL);
}

static void process_node(unsigned int idx) {
	pthread_mutex_lock(&sum_mutex);

	if (visited[idx] || stop) {
		pthread_mutex_unlock(&sum_mutex);
		return;
	}

	while (enqueuing[idx]) {
		pthread_mutex_unlock(&sum_mutex);
		usleep(100000);  // Sleep for a short duration to avoid busy waiting
		pthread_mutex_lock(&sum_mutex);
	}

	os_node_t *node = graph->nodes[idx];
	sum += node->info;  // Add node value to the overall sum
	visited[idx] = true;  // Mark the node as visited
	enqueuing[idx] = true;  // Mark the node as being enqueued

	for (unsigned int i = 0; i < node->num_neighbours; i++) {
		unsigned int neighbor_index = node->neighbours[i];

		if (!visited[neighbor_index] && !stop) {
			os_task_t *neighbor_task = create_graph_task(neighbor_index);
			enqueue_task(tp, neighbor_task);
		}
	}

	enqueuing[idx] = false;  // Mark the node as not being enqueued
	pthread_mutex_unlock(&sum_mutex);

	if (all_nodes_visited() && !all_nodes_visited_flag) {
		all_nodes_visited_flag = 1;  // Set the flag
	}
}

void stop_program() {
    stop = true;
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

    signal(SIGALRM, stop_program);
    alarm(2);  // Set an alarm for 2 seconds

    while (!all_nodes_visited_flag && !stop) {
        for(int i = 0; i < 10000000; i++);  // Sleep for a short duration to avoid busy waiting
	}
	wait_for_completion(tp);  // Wait for all threads to finish
    destroy_threadpool(tp);

    printf("%d", sum);

    return 0;
}