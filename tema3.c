#include "functions.h"


int main (int argc, char *argv[])
{
    int procs, rank;
    int N, bonus;
    int* v;
    int* v1;
    int standard_work, rest_work, standard_work1;
    int* to_work;
    int total_workers;
    int dest[3];


    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);


    if (rank == 0) {
        N = atoi(argv[1]);
        bonus = atoi(argv[2]);
    }

    // Se afla topologia
    int** clusters = malloc (NUM_CLUSTERS * sizeof(int *));
    int* no_workers = malloc (NUM_CLUSTERS * sizeof(int));

    // rank-ul coordonator este initial propriul rank
    int coord_rank = rank;
    char name_file[MAX_LEN] = DEFAULT_NAME_FILE;

    // fiecare worker isi afla cluster-ul
    divide_into_clusters(rank, name_file, clusters, no_workers, &coord_rank);
    
    // coordonatorii afla daca suntem la bonus sau nu
    // prin intermediul lui 2
    send_bonus_flag(rank, &bonus);

    // coordonatorii trimit catre workeri daca suntem la bonus
    // sau nu si acestia retin
    find_out_bonus(rank, &bonus, no_workers, clusters, coord_rank);

    // incepe diferentierea fata de bonus
    if (bonus == 0) {

        initialize_coord(dest);
        if (rank < NUM_CLUSTERS) {
            // trimit la ceilalti coordonatori vectorul cu cluster-ul meu
            send_cluster_to_neighbours(clusters[rank], no_workers[rank], rank);

            // primesc numarul de workeri de la restul coordonatorilor
            // si workerii lor
            register_workers(rank, no_workers, clusters);
        }

        // Ca si coordonator trimit in interiorul cluster-ului
        // topologia, ca si worker o primesc si memorez
        find_topology(rank, no_workers, clusters, coord_rank);


        fflush(stdin);
        // printez topologia
        print_topo(rank, clusters, no_workers);
        fflush(stdin);
    

        // doar procesul 0 cunoaste vectorul si N-ul
        if (rank == 0) {
            v = initialize_vector(N);

            total_workers = calculate_total_workers(no_workers);
            
            // cate elemente ar reveni unui worker
            // daca s-ar imparti egal
            divide_work(N, total_workers, &standard_work, &rest_work);
        
            send_standard_work_to_coord(standard_work, v, no_workers, rank, dest);
        }

        // daca sunt proces coordonator primesc de ce bucati trebuie sa se ocupe
        // workerii mei
        if (rank < NUM_CLUSTERS && rank != 0) {
           v = receive_standard_to_work(&standard_work, no_workers[rank], rank, 0);
        }

        if (rank < NUM_CLUSTERS) {
            // trimit catre workeri, cat au de muncit si bucata corespunzatoare
            send_standard_work_to_workers(standard_work, v, no_workers, clusters, rank);
        }

        // primesc bucatile de care trebuie sa ma ocup ca si worker
        else {

            v = receive_standard_to_work(&standard_work, 1, rank, coord_rank);

            // fac inmultirea
            do_work(v, standard_work);

            // trimit bucata muncita catre coordonatorul cluster-ului
            printf("M(%d,%d)\n", rank, coord_rank);
            MPI_Send(v, standard_work, MPI_INT, coord_rank, 0, MPI_COMM_WORLD);
        }
        

        if (rank < NUM_CLUSTERS) {
            // primesc bucatile de la muncitori
            recv_work_from_workers(standard_work, no_workers, v, rank, clusters);
        }

        if (rank == 1 || rank == 2) {
            // trimit bucata mare catre procesul 0
            printf("M(%d,%d)\n", rank, 0);
            MPI_Send(v, standard_work * no_workers[rank], MPI_INT, 0, 0, MPI_COMM_WORLD);
        }

        // primesc toate bucatile
        int stop = -2;
        int* rest_cluster;

        if (rank == 0) {
            
            recv_work_from_coord(standard_work, no_workers, v, rank, clusters, dest);

            if (rest_work == 0) {
                print_vect(N, v);

                // Trimit la toata lumea ca s-a incheiat cu succes prelucrarea
                stop = -1;
            }

            else {
                // trimit ca nu s-a incheiat
                stop = 0;
            }

            // trimit catre coordonatori daca s-a incheiat sau nu
            send_stop_flag_to_coord(rank, stop);
        }

        // coordonatorii 1 si 2 afla daca s-a terminat sau nu
        if (rank == 1 || rank == 2) {
            MPI_Recv(&stop, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
        }

        // trimit daca s-a incheiat sau nu la toata lumea
        // ca si coordonator
        if (rank < 3) {
            send_stop_flag_to_workers(rank, no_workers, clusters, stop);
        }
        else {
            MPI_Recv(&stop, 1, MPI_INT, coord_rank, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
        }
        
        // daca nu s-a terminat
        if (stop == 0) {

            int* rest_to_work;
            int num_rest_workers;

            if (rank == 0) {
                rest_cluster = calculate_rest_work(&rest_work, no_workers);

                // cat mai are de prelucrat fiecare
                // trimit cate coordonatori
                num_rest_workers = rest_cluster[0];
                rest_to_work = malloc (num_rest_workers * sizeof(int));

                int deplas = 0;
                for (int i = 0; i < rest_cluster[0]; i++) {
                    rest_to_work[i] = v[standard_work * total_workers + deplas];
                    deplas++;
                }

                send_rest_work(rank, v, standard_work, total_workers, rest_cluster, deplas, dest);
            }     


            if (rank == 1 || rank == 2) {
                // primesc cat am de lucrat si ce, de la 0
                rest_to_work = recv_rest_work(&num_rest_workers, 0);
            }

            process_rest_work(rank, coord_rank, num_rest_workers, clusters, rest_to_work, no_workers);

            if (rank < 3) {
                // primesc de la subordonati
                for (int i = 0; i < num_rest_workers; i++) {
                    MPI_Recv(&rest_to_work[i], 1, MPI_INT, clusters[rank][i], MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
                }
            }

            if (rank == 1 || rank == 2) {
                // trimit la procesul 0
                printf("M(%d,%d)\n", rank, 0);
                MPI_Send(rest_to_work, num_rest_workers, MPI_INT, 0, 0, MPI_COMM_WORLD);
            }

            if (rank == 0) {

                for (int i = 0; i < rest_cluster[0]; i++) {
                    v[standard_work * total_workers + i] = rest_to_work[i];
                }

                int deplas = rest_cluster[0];
                // primeste resturile
                recv_rest_to_work(v, standard_work, total_workers, deplas, rest_cluster, dest);

                fflush(stdin);
                print_vect(N, v);
                fflush(stdin);

                free(rest_cluster);
            }

            if (rank < NUM_CLUSTERS) {
                free(rest_to_work);
            }
        }
    }

    // implementarea de la bonus
    else if (bonus == 1) {
        
         int dest[1];
         int rank_dest[3];
         initialize_coord_bonus(rank_dest);

        // trimit cluster-ul meu catre 0 si 1
        if (rank == 2) {
            send_cluster_to_neighbours(clusters[rank], no_workers[rank], rank);
        }

        if (rank == 0) {
            // primesc cluster-ul de la 2
            clusters[2] = receive_cluster(&no_workers[2], 2, 2);

            // trimit cluster-ul meu catre 2
            dest[0] = 2;
            send_cluster(0, clusters[0], no_workers[0], dest, 1);
        }

        if (rank == 2) {
            // 2 primeste de la 0
            clusters[0] = receive_cluster(&no_workers[0], 0, 0);

            // trimit informatia de la 0 la 1
            dest[0] = 1;
            send_cluster(2, clusters[0], no_workers[0], dest, 1);
        }

        if (rank == 1) {
            // primesc informatia de la 2
            clusters[2] = receive_cluster(&no_workers[2], 2, 2);

            // primesc informatia de la 0, prin 2
            clusters[0] = receive_cluster(&no_workers[0], 0, 2);

            // trimit informatia mea catre 2
            dest[0] = 2;
            send_cluster(1, clusters[1], no_workers[1], dest, 1);
        }

        if (rank == 2) {
            // primesc informatia de la 1, o memorez si o trimit catre 0
            clusters[1] = receive_cluster(&no_workers[1], 0, 1);

            // trimit informatia mea catre 0
            dest[0] = 0;
            send_cluster(2, clusters[1], no_workers[1], dest, 1);
        }

        if (rank == 0) {
            // primesc si memorez informatia de la 1, prin 2
            clusters[1] = receive_cluster(&no_workers[1], 0, 2);
        }

        // trimit toata topologia in interiorul cluster-ului 
        // pe care il coordonez
        find_topology(rank, no_workers, clusters, coord_rank);

        fflush(stdin);
        // printez topologia
        print_topo(rank, clusters, no_workers);
        fflush(stdin);

        // etapa de calcule
        // doar procesul 0 cunoaste vectorul si N-ul
        if (rank == 0) {

            v = initialize_vector(N);

            total_workers = calculate_total_workers(no_workers);
            
            // cate elemente ar reveni unui worker
            // daca s-ar imparti egal
            divide_work(N, total_workers, &standard_work, &rest_work);
            
            // trimit standard-ul pt 1 si 2 catre coordonatorul 2
            send_standard_work_to_coord(standard_work, v, no_workers, rank, rank_dest);
        }

        if (rank == 2) {
            // primesc info despre 1 de la 0
            v1 = receive_standard_to_work(&standard_work1, no_workers[1], rank, 0);

            // primesc info despre mine
            v = receive_standard_to_work(&standard_work, no_workers[rank], rank, 0);

            // trimit info despre 1
            // cat trebuie sa munceasca fiecare worker
            printf("M(%d,%d)\n", rank, 1);
            MPI_Send(&standard_work1, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);

            // bucata din vector corespunzatoare pentru tot cluster-ul
            printf("M(%d,%d)\n", rank, 1);
            MPI_Send(v1, standard_work1 * no_workers[1], MPI_INT, 1, 0, MPI_COMM_WORLD);
        }

        if (rank == 1) {
            // primesc info despre mine de la 0, prin 2
            v = receive_standard_to_work(&standard_work, no_workers[rank], rank, 2);
        }

        if (rank < NUM_CLUSTERS) {
            // trimit catre workeri, cat au de muncit si bucata corespunzatoare
            send_standard_work_to_workers(standard_work, v, no_workers, clusters, rank);
        }

        // primesc bucatile de care trebuie sa ma ocup ca si worker
        else {
            v = receive_standard_to_work(&standard_work, 1, rank, coord_rank);

            // fac inmultirea
            do_work(v, standard_work);

            // trimit bucata muncita catre coordonatorul cluster-ului
            printf("M(%d,%d)\n", rank, coord_rank);
            MPI_Send(v, standard_work, MPI_INT, coord_rank, 0, MPI_COMM_WORLD);
        }

        if (rank < NUM_CLUSTERS) {
            // primesc bucatile de la muncitori
            recv_work_from_workers(standard_work, no_workers, v, rank, clusters);
        }

        if (rank == 1) {
            // trimit bucata mare catre procesul 2, care o va trimite lui 0
            printf("M(%d,%d)\n", rank, 2);
            MPI_Send(v, standard_work * no_workers[rank], MPI_INT, 2, 0, MPI_COMM_WORLD);
        }

        if (rank == 2) {
            // primesc bucata de la 1 si o trimit lui 0
            MPI_Recv(v1, standard_work1 * no_workers[1], MPI_INT, 1, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
            printf("M(%d,%d)\n", rank, 0);
            MPI_Send(v1, standard_work1 * no_workers[1], MPI_INT, 0, 0, MPI_COMM_WORLD);

            // trimit bucata mare catre procesul 0
            printf("M(%d,%d)\n", rank, 0);
            MPI_Send(v, standard_work * no_workers[rank], MPI_INT, 0, 0, MPI_COMM_WORLD);
        }

         // primesc toate bucatile
        int rest_work_copy, stop = -2;
        int* rest_cluster;

        if (rank == 0) {
            int last_index = standard_work * no_workers[0];
            rest_work_copy = rest_work;

            // primesc de la 2 ambele bucati
            recv_work_from_coord(standard_work, no_workers, v, rank, clusters, rank_dest);

            if (rest_work == 0) {
                print_vect(N, v);

                // Trimit la toata lumea ca s-a incheiat cu succes prelucrarea
                stop = -1;
            }

            else {
                // trimit ca nu s-a incheiat
                stop = 0;
            }

            // trimit catre 2 daca s-a terminat sau nu
            printf("M(%d,%d)\n", rank, 2);
            MPI_Send(&stop, 1, MPI_INT, 2, 0, MPI_COMM_WORLD);
        }

        if (rank == 2) {
            // primesc daca s-a terminat sau nu de la 0
            MPI_Recv(&stop, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);

            // trimit catre 1 stop-ul
            printf("M(%d,%d)\n", rank, 1);
            MPI_Send(&stop, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
        }
        
        if (rank == 1) {
            MPI_Recv(&stop, 1, MPI_INT, 2, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
        }

        // trimit daca s-a incheiat sau nu la toata lumea
        // ca si coordonator
        if (rank < 3) {
            send_stop_flag_to_workers(rank, no_workers, clusters, stop);
        }
        else {
            MPI_Recv(&stop, 1, MPI_INT, coord_rank, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
        }

         // daca nu s-a terminat
        if (stop == 0) {

            int* rest_to_work;
            int num_rest_workers;
            int* rest_to_work1;
            int num_rest_workers1;

            if (rank == 0) {
                // procesul 0 calculeaza de cati workeri e nevoie
                // la fiecare cluster
                rest_cluster = calculate_rest_work(&rest_work, no_workers);

                // cat mai are de prelucrat fiecare
                // trimit cate coordonatori
                num_rest_workers = rest_cluster[0];
                rest_to_work = malloc (num_rest_workers * sizeof(int));

                int deplas = 0;
                for (int i = 0; i < rest_cluster[0]; i++) {
                    rest_to_work[i] = v[standard_work * total_workers + deplas];
                    deplas++;
                }

                // ii trimit lui 2
                send_rest_work(rank, v, standard_work, total_workers, rest_cluster, deplas, rank_dest);
            }

            if (rank == 2) {
                // primesc cat am de lucrat si ce, de la 0
                rest_to_work1 = recv_rest_work(&num_rest_workers1, 0);

                // primesc cat am de lucrat si ce, de la 0
                rest_to_work = recv_rest_work(&num_rest_workers, 0);

                // trimit lui 1 ce are de lucrat
                // cat mai am
                printf("M(%d,%d)\n", rank, 1);
                MPI_Send(&num_rest_workers1, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);

                // vectorul in sine
                printf("M(%d,%d)\n", rank, 1);
                MPI_Send(rest_to_work1, num_rest_workers1, MPI_INT, 1, 0, MPI_COMM_WORLD);

            }
  
            if (rank == 1) {
                // primesc cat am de lucrat si ce, de la 0, prin 2
                rest_to_work = recv_rest_work(&num_rest_workers, 2);
            }

            process_rest_work(rank, coord_rank, num_rest_workers, clusters, rest_to_work, no_workers);

            if (rank < 3) {
                // primesc de la subordonati
                for (int i = 0; i < num_rest_workers; i++) {
                    MPI_Recv(&rest_to_work[i], 1, MPI_INT, clusters[rank][i], MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
                }
            }

            if (rank == 1) {
                // trimit la procesul 2
                printf("M(%d,%d)\n", rank, 2);
                MPI_Send(&num_rest_workers, 1, MPI_INT, 2, 0, MPI_COMM_WORLD);

                printf("M(%d,%d)\n", rank, 2);
                MPI_Send(rest_to_work, num_rest_workers, MPI_INT, 2, 0, MPI_COMM_WORLD);
            }

            if (rank == 2) {
                // primeste de la 1
                MPI_Recv(&num_rest_workers1, 1, MPI_INT, 1, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);
                MPI_Recv(rest_to_work1, num_rest_workers1, MPI_INT, 1, MPI_ANY_TAG, MPI_COMM_WORLD, NULL);

                // trimit la procesul 0 info de la 1
                printf("M(%d,%d)\n", rank, 0);
                MPI_Send(rest_to_work1, num_rest_workers1, MPI_INT, 0, 0, MPI_COMM_WORLD);

                // trimit la procesul 0 info sa
                printf("M(%d,%d)\n", rank, 0);
                MPI_Send(rest_to_work, num_rest_workers, MPI_INT, 0, 0, MPI_COMM_WORLD);

            }

            if (rank == 0) {
                 for (int i = 0; i < rest_cluster[0]; i++) {
                    v[standard_work * total_workers + i] = rest_to_work[i];
                }

                int deplas = rest_cluster[0];
                // primeste resturile de la 2
                recv_rest_to_work(v, standard_work, total_workers, deplas, rest_cluster, rank_dest);

                fflush(stdin);
                print_vect(N, v);
                fflush(stdin);

                free(rest_cluster);
            }

            if (rank < NUM_CLUSTERS) {
                free(rest_to_work);
            }


            if (rank == 2) {
                free(rest_to_work1);
                free(v1);
            }

        }

    }

    for (int i = 0; i < NUM_CLUSTERS; i++) {
        free(clusters[i]);
    }
    free(clusters);
    free(no_workers);

  
    free(v);

    MPI_Finalize();

}
