#include "mpi.h"
#include <iostream> 
#include <thread> 
#include <stack> 

using namespace std; 

#define PROC_NUM 8 // liczba procesów
#define MAX_MSG_LEN 4 // maksymalna długość wiadomości
#define CHANNEL_CAPACITY 3 // pojemność kanału

stack <int[MAX_MSG_LEN]> message_buffer;

void message_reader(){
    int tmp_msg[MAX_MSG_LEN] = {-1};

    MPI_Status status;
    MPI_Recv(tmp_msg, MAX_MSG_LEN, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
}

int get_state(){
    return 0;
}

int main(int argc, char **argv)
{
    int rank;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank( MPI_COMM_WORLD, &rank );


    thread msg_th(message_reader); 
    while(1){
        switch (get_state()) {
            case 0: //sekcja lokalna
                break;
            case 1: // P1
                break;
            case 2: // P2
                break;
            case 3: // szatnia
                break;
            case 4: // basen
                break;
            default:
                break;
            }
    }

    msg_th.join();
	MPI_Finalize();
}