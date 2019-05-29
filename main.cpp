#include "mpi.h"
#include <iostream> 
#include <thread> 
#include <queue>
#include <mutex>

using namespace std; 

#define PROC_NUM 8 // liczba procesów
#define MAX_MSG_LEN 4 // maksymalna długość wiadomości
#define CHANNEL_CAPACITY 3 // pojemność kanału

queue <int*> message_buffer; // stos wiadomości

mutex wait_for_message;


void message_reader(){ // służy TYLKO do odbierania wiadomości i przekazywania do bufora
    int * tmp_msg = new int[MAX_MSG_LEN];
    int tag;

    MPI_Status status;
    MPI_Recv(tmp_msg, MAX_MSG_LEN, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    message_buffer.push(tmp_msg);
    wait_for_message.unlock();
}

void send_msg(int m0, int m1, int m2, int m3, int send_to){
    int send_msg[] = {m0, m1, m2, m3};
    MPI_Send(send_msg, MAX_MSG_LEN, MPI_INT, send_to, MPI_ANY_TAG, MPI_COMM_WORLD);
}

void send_to_all(int m0, int m1, int m2, int m3, int rank){
    for(int i = 0; i < PROC_NUM; i++) {
        if(i == rank) continue;
        send_msg(i, m0, m1, m2, m3);
    }
}

void read_message(int * msg){
    int * tmp_msg = message_buffer.front();
    message_buffer.pop();

    for(int i = 0; i < PROC_NUM; i++) msg[i] = tmp_msg[i];

    delete[] tmp_msg;
}


void handle_zero_state(){
    while(1){
        int tmp_msg[PROC_NUM] = {-1};
        if(message_buffer.empty()) wait_for_message.lock();
        read_message(tmp_msg);
    }
}

void handle_first_state(){

}

void handle_second_state(){

}

void handle_third_state(){

}

void handle_fourth_state(){

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

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    send_to_all(1,2,3,4,rank);
    while(1){
        switch (get_state()) {
            case 0: //sekcja lokalna
                handle_zero_state();
                break;
            case 1: // P1
                handle_first_state();
                break;
            case 2: // P2
                handle_second_state();
                break;
            case 3: // szatnia
                handle_third_state();
                break;
            case 4: // basen
                handle_fourth_state();
                break;
            default:
                break;
            }
    }

    msg_th.join();
	MPI_Finalize();
}