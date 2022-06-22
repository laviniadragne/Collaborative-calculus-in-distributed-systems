#include "functions.h"

// Citeste din fisierul de cluster
// si retine workerii subordonati
int* read_from_file(char* name_file, int* no_workers) {

    FILE *fptr;
    int* cluster;
    if ((fptr = fopen(name_file, "r")) == NULL) {
        fprintf(stderr, "Error! opening %s\n", name_file);
        exit(1);
    }

    // citesc numarul de procese worker pentru cluster
    fscanf(fptr, "%d", no_workers);
    cluster = (int *) malloc ((*no_workers) * sizeof(int));

    // procesele worker pentru cluster-ul
    for (int i = 0; i < (*no_workers); i++) {
        fscanf(fptr, "%d", &cluster[i]);
    }

    fclose(fptr);

    return cluster; 
}


// Trimit rank-ul meu (coordonatorul) catre workerii din cluster
// care imi sunt subordonati
void send_coord_rank(int rank, int* cluster, int no_workers) {
    for (int i = 0; i < no_workers; i++) {
            // logare in terminal a mesajului
            printf("M(%d,%d)\n", rank, cluster[i]);
            MPI_Send(&rank, 1, MPI_INT, cluster[i], 0, MPI_COMM_WORLD);
    }
}


// Trimit la fiecare element din destination dimensiunea si
// continutul cluster-ului cluster
void send_cluster(int rank, int* cluster, int no_workers, int* destination, int size_dest) {
    // trimit la toti workerii subordonati numarul de workeri din cluster-ul
    // pe care il coordonez
    for (int i = 0; i < size_dest; i++) {
        // logare in terminal a mesajului
        printf("M(%d,%d)\n", rank, destination[i]);
        MPI_Send(&no_workers, 1, MPI_INT, destination[i], 0, MPI_COMM_WORLD);
    }

    // trimit la toti workerii subordonati workerii efectivi din cluster
    for (int i = 0; i < size_dest; i++) {
        // logare in terminal a mesajului
        printf("M(%d,%d)\n", rank, destination[i]);
        MPI_Send(cluster, no_workers, MPI_INT, destination[i], 0, MPI_COMM_WORLD);
    }
}


// Trimite cluster-ul catre vecinii coordonatori
void send_cluster_to_neighbours(int* cluster, int no_workers, int sender) {
    
    int dest[1];

    for (int i = 0; i < NUM_CLUSTERS; i++) {
        if (i != sender) {
            dest[0] = i;
            // trimit la vecinii coordonatori nr de workers si workerii
            send_cluster(sender, cluster, no_workers, dest, 1);
        }
    }
  
}

// Memoreaza si returneaza un cluster primit
int* receive_cluster(int* no_workers, int rank_cluster_mem, int rank_source) {
    MPI_Recv(no_workers, 1, MPI_INT, rank_source, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
    int* cluster = malloc ((*no_workers) * sizeof(int));
    MPI_Recv(cluster, (*no_workers), MPI_INT, rank_source, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);

    return cluster;
}


// Procesele coordonator citesc din fisierele de input
// trimit catre workeri rank-ul cluster-ului din care fac parte
// si workerii retii
void divide_into_clusters(int rank, char name_file[MAX_LEN], int **clusters, int *no_workers, int *coord_rank) {
    // daca sunt proces coordonator
    if (rank < NUM_CLUSTERS) {
        // creez numele nou pentru fisierul de input
        name_file[7] = rank + 48;
        clusters[rank] = read_from_file(name_file, &no_workers[rank]);

        // trimit rank-ul meu (coordonatorul) catre workerii din cluster
        // care imi sunt subordonati
        send_coord_rank(rank, clusters[rank], no_workers[rank]);
    }

    // toata lumea afla in ce cluster e distribuit
    else {
        MPI_Recv(coord_rank, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
    }
}

// Toti coordonatorii afla de la 0 daca suntem
// la bonus sau nu
void send_bonus_flag(int rank, int* bonus) {
    // trimit daca sunt la bonus sau nu coordonatorilor
    // prin coordonatorul 2
    if (rank == 0) {
        printf("M(%d,%d)\n", rank, 2);
        MPI_Send(bonus, 1, MPI_INT, 2, 0, MPI_COMM_WORLD);
    }

    // 2 ii trimite lui 1
    if (rank == 2) {
        MPI_Recv(bonus, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
        // ii trimit lui 1
        printf("M(%d,%d)\n", rank, 1);
        MPI_Send(bonus, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
    }

    if (rank == 1) {
        // primeste de la 2
        MPI_Recv(bonus, 1, MPI_INT, 2, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
    }
}

// Subalternii afla daca sunt la bonus sau nu
void find_out_bonus(int rank, int *bonus, int* no_workers, int** clusters, int coord_rank) {
     // trimit la subalterni daca sunt la bonus sau nu
    if (rank < NUM_CLUSTERS) {
        for (int i = 0; i < no_workers[rank]; i++) {
            printf("M(%d,%d)\n", rank, clusters[rank][i]);
            MPI_Send(bonus, 1, MPI_INT, clusters[rank][i], 0, MPI_COMM_WORLD);
        }
    }
    // subalternii afla daca sunt la 0 sau nu
    else {
        MPI_Recv(bonus, 1, MPI_INT, coord_rank, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
    }
}

// Coordonatorii afla de workerii
// din celelalte clustere
void register_workers(int rank, int* no_workers, int** clusters) {
    // primesc numarul de workeri de la restul coordonatorilor
    // si workerii lor
    for (int j = 0; j < NUM_CLUSTERS; j++) {
        if (rank != j) {
            MPI_Recv(&no_workers[j], 1, MPI_INT, j, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
            clusters[j] = malloc (no_workers[j] * sizeof(int));
            MPI_Recv(clusters[j], no_workers[j], MPI_INT, j, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
        }
    }
}

// Ca si coordonator trimit in interiorul cluster-ului
// topologia, ca si worker o primesc si memorez
void find_topology(int rank, int* no_workers, int** clusters, int coord_rank) {
    // trimit toata topologia in interiorul cluster-ului 
    // pe care il coordonez
    if (rank < NUM_CLUSTERS) {
        for (int j = 0; j < NUM_CLUSTERS; j++) {
            send_cluster(rank, clusters[j], no_workers[j], clusters[rank], no_workers[rank]);
        }
    }

    // ca si simplu worker primesc topologia 
    else  {
        for (int j = 0; j < NUM_CLUSTERS; j++) {
            MPI_Recv(&no_workers[j], 1, MPI_INT, coord_rank, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
            clusters[j] = malloc (no_workers[j] * sizeof(int));
            MPI_Recv(clusters[j], no_workers[j], MPI_INT, coord_rank, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
        }
    }
}


// Printeaza topologia
void print_topo(int rank, int** clusters, int *no_workers) {

    int j;
    fflush(stdin);
    printf("%d -> ", rank);
    for (j = 0; j < NUM_CLUSTERS; j++) {
        printf("%d:", j);
        int i;
        for (i = 0; i < no_workers[j] - 1; i++) {
            printf("%d,", clusters[j][i]);
        }
        printf("%d ", clusters[j][i]);
    }
    printf("\n");
    fflush(stdin);
}

int* initialize_vector(int N) {
    int* v = malloc (N * sizeof(int));
    for (int i = 0; i < N; i++) {
        v[i] = i;
    }
    return v;
}

// Calculeaza 0 numarul total de workeri
// din sistem
int calculate_total_workers(int *no_workers) {
    int total_workers = 0;
    for (int i = 0; i < NUM_CLUSTERS; i++) {
        total_workers += no_workers[i];
    }
    return total_workers;
}

void divide_work(int N, int total_workers, int *standard_work, int *rest_work) {
    // cate elemente ar reveni unui worker
    // daca s-ar imparti egal
    (*standard_work) = N / total_workers;
    (*rest_work) = N % total_workers;
}

// Trimit bucata de munca standard catre coordonatori
void send_standard_work_to_coord(int standard_work, int* v, int* no_workers, int rank, int rank_dest[3]) {
    // trimit standard-ul catre coordonatorii 1 si 2
    int last_index = standard_work * no_workers[0];
    for (int i = 1; i < NUM_CLUSTERS; i++) {
        // cat trebuie sa munceasca fiecare worker
        printf("M(%d,%d)\n", rank, rank_dest[i]);
        MPI_Send(&standard_work, 1, MPI_INT, rank_dest[i], 0, MPI_COMM_WORLD);
        

        // bucata din vector corespunzatoare pentru tot cluster-ul
        printf("M(%d,%d)\n", rank, rank_dest[i]);
        MPI_Send(v + last_index, standard_work * no_workers[i], MPI_INT, rank_dest[i], 0, MPI_COMM_WORLD);
        last_index += standard_work * no_workers[i];
    }
}


// Retine in vectorul v ce are de muncit ca si worker
int* receive_standard_to_work(int* standard_work, int no_workers, int rank, int coord_rank) {
    MPI_Recv(standard_work, 1, MPI_INT, coord_rank, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
    int* v = malloc (((*standard_work + 1) * no_workers) * sizeof(int));
    MPI_Recv(v, *standard_work * no_workers, MPI_INT, coord_rank, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);

    return v;
}


// Fiecare coordonator trimite bucata catre workerii lui
void send_standard_work_to_workers(int standard_work, int* v, int* no_workers, int** clusters, int rank) {
    // trimit catre workeri, cat au de muncit si bucata corespunzatoare
    for (int i = 0; i < no_workers[rank]; i++) {
        printf("M(%d,%d)\n", rank, clusters[rank][i]);
        MPI_Send(&standard_work, 1, MPI_INT, clusters[rank][i], 0, MPI_COMM_WORLD);
        

        printf("M(%d,%d)\n", rank, clusters[rank][i]);
        MPI_Send(v + i * standard_work, standard_work, MPI_INT, clusters[rank][i], 0, MPI_COMM_WORLD);    
    }
}

// Operatia propriu-zisa facuta de workeri
void do_work(int* v, int standard_work) {
    for (int j = 0; j < standard_work; j++) {
        v[j] *= 2;
    }
}

// Primesc bucatile de la muncitori
void recv_work_from_workers(int standard_work, int* no_workers, int* v, int rank, int** clusters) {
    for (int i = 0; i < no_workers[rank]; i++) {
        MPI_Recv(v + i * standard_work, standard_work, MPI_INT, clusters[rank][i], MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
    }
}

// Primesc bucatile de la 1 si 2
void recv_work_from_coord(int standard_work, int* no_workers, int* v, int rank, int** clusters, int dest[3]) {
    int last_index = standard_work * no_workers[0];

    for (int i = 1; i < NUM_CLUSTERS; i++) {
        MPI_Recv(v + last_index, standard_work * no_workers[i], MPI_INT, dest[i], MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
        last_index += standard_work * no_workers[i];
    }
}

// Printeaza vectorul final
void print_vect(int N, int* v) {
    printf("Rezultat: ");
    for (int i = 0; i < N; i++) {
        printf("%d ", v[i]);
    }
    printf("\n");
}

void send_stop_flag_to_coord(int rank, int stop) {
    for (int i = 1; i < NUM_CLUSTERS; i++) {
        printf("M(%d,%d)\n", rank, i);
        MPI_Send(&stop, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
    }
}

void send_stop_flag_to_workers(int rank, int* no_workers, int** clusters, int stop) {
    for (int i = 0; i < no_workers[rank]; i++) {
        printf("M(%d,%d)\n", rank, clusters[rank][i]);
        MPI_Send(&stop, 1, MPI_INT, clusters[rank][i], 0, MPI_COMM_WORLD);
    }
}

int* calculate_rest_work(int* rest_work, int* no_workers) {
    // procesul 0 calculeaza de cati workeri e nevoie
    // la fiecare cluster
    int* rest_cluster = calloc (NUM_CLUSTERS, sizeof(int));
    for (int i = 0; i < NUM_CLUSTERS; i++) {
        if (*rest_work > no_workers[i]) {
            rest_cluster[i] = no_workers[i];
        }
        else {
            if (*rest_work > 0) {
                rest_cluster[i] = *rest_work;
            }
        }

        *rest_work -= rest_cluster[i];
    }

    return rest_cluster;
}

void send_rest_work(int rank, int* v, int standard_work, int total_workers, int* rest_cluster, int deplas, int dest[3]) {
    for (int i = 1; i < NUM_CLUSTERS; i++) {
        // cat mai am
        printf("M(%d,%d)\n", rank, dest[i]);
        MPI_Send(&rest_cluster[i], 1, MPI_INT, dest[i], 0, MPI_COMM_WORLD);

        // vectorul in sine
        printf("M(%d,%d)\n", rank, dest[i]);
        MPI_Send(v + standard_work * total_workers + deplas, rest_cluster[i], MPI_INT, dest[i], 0, MPI_COMM_WORLD);
        deplas += rest_cluster[i];
    }
}

int* recv_rest_work(int* num_rest_workers, int rank_source) {
    // primesc cat am de lucrat si ce, de la 0
    MPI_Recv(num_rest_workers, 1, MPI_INT, rank_source, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
    int* rest_to_work = malloc (*num_rest_workers * sizeof(int));
    MPI_Recv(rest_to_work, *num_rest_workers, MPI_INT, rank_source, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);

    return rest_to_work;
}

void send_recv_work_to_workers(int num_rest_workers, int rank, int** clusters, int* no_workers, int* rest_to_work) {
    int i;
    // trimit la subordonati
     for (i = 0; i < num_rest_workers; i++) {
        printf("M(%d,%d)\n", rank, clusters[rank][i]);
        MPI_Send(&rest_to_work[i], 1, MPI_INT, clusters[rank][i], 0, MPI_COMM_WORLD);
    }

    // la restul trimit -1
    for (i = num_rest_workers; i < no_workers[rank]; i++) {
        int elem = -1;
        printf("M(%d,%d)\n", rank, clusters[rank][i]);
        MPI_Send(&elem, 1, MPI_INT, clusters[rank][i], 0, MPI_COMM_WORLD);
    }
}

// Trimite workerilor cat mai au de lucrat
// respectiv primeste de la workeri 
void process_rest_work(int rank, int coord_rank, int num_rest_workers,
                       int** clusters, int* rest_to_work, int* no_workers) {
    if (rank < 3) {
        send_recv_work_to_workers(num_rest_workers, rank, clusters, no_workers, rest_to_work);
    }

    // subordonatii fac restul de treaba
    else {
        int elem = -1;
        MPI_Recv(&elem, 1, MPI_INT, coord_rank, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
        if (elem != -1) {
            elem *= 2;
            printf("M(%d,%d)\n", rank, coord_rank);
            MPI_Send(&elem, 1, MPI_INT, coord_rank, 0, MPI_COMM_WORLD);
        }
    }
}

void recv_rest_to_work(int* v, int standard_work, int total_workers, int deplas, int* rest_cluster, int dest[3]) {
    for (int i = 1; i < NUM_CLUSTERS; i++) {
        MPI_Recv(v + standard_work * total_workers + deplas, rest_cluster[i], MPI_INT, dest[i], MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
        deplas += rest_cluster[i];
    }
}

void initialize_coord(int rank_coord[3]) {
    for (int i = 0; i < NUM_CLUSTERS; i++) {
        rank_coord[i] = i;
    }
}


void initialize_coord_bonus(int rank_coord[3]) {
    for (int i = 0; i < NUM_CLUSTERS; i++) {
        rank_coord[i] = 2;
    }
}