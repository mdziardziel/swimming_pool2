#include "mpi.h"
#include <iostream> 
#include <thread> 
#include <queue>
#include <mutex>
#include <cstdlib>

using namespace std; 

#define PROC_NUM 8 // liczba procesów
#define MAX_MSG_LEN 4 // maksymalna długość wiadomości
#define TAG 100

class Message{
    public:
    Message(){}
    Message(int msg[MAX_MSG_LEN], int ssender){
        type = msg[0];
        m1 = msg[1];
        m2 = msg[2];
        m3 = msg[3];
        sender = ssender;
    }

    Message(int ttype, int mm2, int mm3, int mm4, int ssender){
        m1 = mm2;
        m2 = mm3;
        m3 = mm4;
        type = ttype;
        sender = ssender;
    }
    
    int m1, m2, m3, type, sender;
};

queue <Message> message_buffer; // stos wiadomości
queue <Message> hold_messages; // stos wiadomości


mutex wait_for_message;
int state = 0;
int timer = -1;
int proc_id = -1;
int gender = -1;
int prev_state = -1;


void message_reader(){ // służy TYLKO do odbierania wiadomości i przekazywania do bufora
    while(1){
        int tmp_msg[MAX_MSG_LEN];
        int tag;

        MPI_Status status;
        MPI_Recv(tmp_msg, MAX_MSG_LEN, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        Message m = Message(tmp_msg, status.MPI_SOURCE);

        message_buffer.push(m);
        printf("odbiorca: %d; nadawca: %d; typ: %d %d %d %d\n", proc_id, m.sender, m.type, m.m1, m.m2, m.m3); 
        wait_for_message.unlock();
    }
}

void send_msg(int m0, int m1, int m2, int m3, int send_to){
    int send_msg[] = {m0, m1, m2, m3};
    MPI_Send(send_msg, MAX_MSG_LEN, MPI_INT, send_to, TAG, MPI_COMM_WORLD);
}

void send_to_all(int m0, int m1, int m2, int m3){
    for(int i = 0; i < PROC_NUM; i++) {
        if(i == proc_id) continue;
        send_msg(m0, m1, m2, m3, i);
    }
}

Message read_message(){
    Message m = message_buffer.front();
    message_buffer.pop();

    return m;
}

void sleep(int num){
    int sleep_time = rand()%num;
    this_thread::sleep_for(chrono::milliseconds(sleep_time));
}

bool is_my_priority_better(int sender_prev_state, int sender_timer, int sender_proc_id){
    if(sender_prev_state == 3 && prev_state != 3){
        return false;
    } else {
        if(sender_timer > timer){
            return true;
        } else if(sender_timer == timer && sender_proc_id > proc_id){
            return true;
        }
    }

    return false;
}

void change_state(int new_state){
    prev_state = state;
    state = new_state;
}

/**
msg.type - typ wiadomości
msgsender - nadawca wiadomości

typy wiadomości:
1 - pytanie stanu 1 o to czy nadawca może wejść do szatni (1, timer, prev_state, -1)
0 - idpowiedź na pytanie stanu 1, pozwolenie na wejście do szatni 
    (0, czy_jestem_aktualnie_w_szatni, numer_szatni, płeć)

**/

void handle_zero_state(){
    sleep(1000);
    change_state(1);
}

void handle_first_state(){
    send_to_all(1, timer,prev_state,-1);
    Message msg;
    int received_messages = 0;
    // change_state(2);
    while(1){
        // int msg[MAX_MSG_LEN + 1] = {-1};
        if(message_buffer.empty()) wait_for_message.lock();
        msg = read_message();

        switch(msg.type){
            case 1:
                if(is_my_priority_better(msg.m2, msg.m1, msg.sender)){
                    hold_messages.push(msg);
                } else {
                    send_msg(0, 0, -1, gender,msg.sender);
                }
                break;
            case 0:
                received_messages++;
                if(received_messages == PROC_NUM - 1){
                    change_state(2);
                }
                break;
        }

    }
}

void handle_second_state(){
    while(1){
        sleep(10000);
    }
}

void handle_third_state(){

}

int main(int argc, char **argv)
{
    srand( time( NULL ) );
    gender = rand()%2;


	MPI_Init(&argc, &argv);
	MPI_Comm_rank( MPI_COMM_WORLD, &proc_id);
    printf("START %d\n", proc_id);

    timer = proc_id;

    thread msg_th(message_reader); 

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // send_to_all(1,2,3,4,proc_id);

    while(1){
        switch (state) {
            case 0: //sekcja lokalna
                printf("%d -> sekcja lokalna\n", proc_id);
                handle_zero_state();
                printf("%d <- sekcja lokaln\n", proc_id);
                break;
            case 1: // P1
                printf("%d -> poczekalnia\n", proc_id);
                handle_first_state();
                printf("%d <- poczekalnia\n", proc_id);
                break;
            case 2: // P2
                printf("%d -> szatnia\n", proc_id);
                handle_second_state();
                printf("%d <- szatnia\n", proc_id);
                break;
            case 3: // szatnia
                handle_third_state();
                break;
            default:
                break;
            }
    }
    

    msg_th.join();
	MPI_Finalize();
}