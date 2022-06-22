#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>

#define COORD_0 0
#define COORD_1 1
#define COORD_2 2
#define NUM_CLUSTERS 3
#define DEFAULT_NAME_FILE "cluster0.txt"
#define MAX_LEN 13

int* read_from_file(char* name_file, int* no_workers);

void send_coord_rank(int rank, int* cluster, int no_workers);

void send_cluster(int rank, int* cluster, int no_workers, int* destination, int size_dest);

void send_cluster_to_neighbours(int* cluster, int no_workers, int sender);

int* receive_cluster(int* no_workers, int rank_cluster_mem, int rank_source);

void divide_into_clusters(int rank, char name_file[MAX_LEN], int **clusters, int *no_workers, int *coord_rank);

void send_bonus_flag(int rank, int* bonus);

void find_out_bonus(int rank, int *bonus, int* no_workers, int** clusters, int coord_rank);

void register_workers(int rank, int* no_workers, int** clusters);

void find_topology(int rank, int* no_workers, int** clusters, int coord_rank);

void print_topo(int rank, int** clusters, int *no_workers);

int* initialize_vector(int N);

int calculate_total_workers(int *no_workers);

void divide_work(int N, int total_workers, int *standard_work, int *rest_work);

void send_standard_work_to_coord(int standard_work, int* v, int* no_workers, int rank, int rank_dest[3]);

int* receive_standard_to_work(int* standard_work, int no_workers, int rank, int coord_rank);

void send_standard_work_to_workers(int standard_work, int* v, int* no_workers, int** clusters, int rank);

void do_work(int* v, int standard_work);

void recv_work_from_workers(int standard_work, int* no_workers, int* v, int rank, int** clusters);

void recv_work_from_coord(int standard_work, int* no_workers, int* v, int rank, int** clusters, int dest[3]);

void print_vect(int N, int* v);

void send_stop_flag_to_coord(int rank, int stop);

void send_stop_flag_to_workers(int rank, int* no_workers, int** clusters, int stop);

int* calculate_rest_work(int* rest_work, int* no_workers);

void send_rest_work(int rank, int* v, int standard_work, int total_workers, int* rest_cluster, int deplas, int dest[3]);

int* recv_rest_work(int* num_rest_workers, int rank_source);

void send_recv_work_to_workers(int num_rest_workers, int rank, int** clusters, int* no_workers, int* rest_to_work);

void process_rest_work(int rank, int coord_rank, int num_rest_workers,
                       int** clusters, int* rest_to_work, int* no_workers);

void recv_rest_to_work(int* v, int standard_work, int total_workers, int deplas, int* rest_cluster, int dest[3]);

void initialize_coord(int rank_coord[3]);

void initialize_coord_bonus(int rank_coord[3]);