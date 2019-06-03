#include "mpi.h"
#include <iostream> 
#include <thread> 
#include <queue>
#include <mutex>
#include <cstdlib>
#include <condition_variable>

using namespace std; 

#define PROC_NUM 8 // liczba procesów
#define MAX_MSG_LEN 4 // maksymalna długość wiadomości
#define TAG 100
#define ROOMS_NUM 3

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
queue <int> hold_messages; // stos wiadomości


condition_variable wait_for_message;
mutex wait_for_message_mutex;

int state = 0;
int timer = -1;
int proc_id = -1;
int gender = -1;
int prev_state = -1;
int room = -1;
int room_capacity = 2;

int room_men[PROC_NUM] = {-1};
int room_women[PROC_NUM] = {-1};
int room_boxes[PROC_NUM] = {-1};

bool waiting_for_room = false;

int get_zero_message[PROC_NUM] = {-1};
int received_permition[PROC_NUM] = {-1};

bool was_in_pool = false;

void message_reader(){ // służy TYLKO do odbierania wiadomości i przekazywania do bufora
    while(1){
        int tmp_msg[MAX_MSG_LEN];
        int tag;

        MPI_Status status;
        MPI_Recv(tmp_msg, MAX_MSG_LEN, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        message_buffer.push(Message(tmp_msg, status.MPI_SOURCE));
        wait_for_message.notify_one();
    }
}

void send_msg(int m0, int m1, int m2, int m3, int send_to){
    int send_msg[] = {m0, m1, m2, m3};
    MPI_Send(send_msg, MAX_MSG_LEN, MPI_INT, send_to, TAG, MPI_COMM_WORLD);
}

void send_msg(Message m){
    int send_msg[] = {m.type, m.m1, m.m2, m.m3};
    MPI_Send(send_msg, MAX_MSG_LEN, MPI_INT, m.sender, TAG, MPI_COMM_WORLD);
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
    }else if(sender_prev_state != 3 && prev_state == 3){
        return true;
    }
    else {
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
    printf("%d: STAN %d -> %d, timer: %d, szatnia: %d\n", proc_id, prev_state, state, timer, room);
}

void resend_hold_messages(){
    while(!hold_messages.empty()){
        send_msg(Message(0, 1, room, gender, hold_messages.front()));
        hold_messages.pop();
    }
}

int get_available_room(){
    
    int room_boxes1[ROOMS_NUM] = {0};
    int room_women1[ROOMS_NUM] = {0};
    int room_men1[ROOMS_NUM] = {0};


    for(int i = 0; i < ROOMS_NUM; i++){
        room_boxes1[i] = 0;
        room_men1[i] = 0;
        room_women1[i] = 0;

    }
    
    for(int i = 0; i < PROC_NUM; i++){
        if(room_boxes[i] != -1) room_boxes1[room_boxes[i]]++;
        if(room_men[i] != -1) room_men1[room_men[i]]++;
        if(room_women[i] != -1) room_women1[room_women[i]]++;
    }
    
    // ------ gdy mam już szafkę w szatni
    if(room != -1){
        if(gender == 1 && room_women1[room] > 0 ) return -1;
        if(gender == 0 && room_men1[room] > 0 ) return -1;
        return room;
    }
    // -----

    for(int i  = 0; i < ROOMS_NUM; i++){
        if(room_boxes1[i] >= room_capacity) {
            continue;
        }

        if(gender == 1 && room_women1[i] > 0 ) continue;
        if(gender == 0 && room_men1[i] > 0 ) continue;

        return i;
    }

    return -1;
}

void sleep_and_resend(int am_i_in_room, int num){
    int slp = rand()%num;
    int sleep_inteval = 10;
    for(int i = 0; i < slp; i=i+sleep_inteval){
        Message msg;
        if(!message_buffer.empty()){
            msg = read_message();
    
            switch(msg.type){
                case 1: // opowiedź na pytanie o wjeście do szatni
                    send_msg(0, am_i_in_room, room, gender, msg.sender);
                    break;
                case 21: // odpowiedź na pytanie o timer
                    send_msg(25, timer, -1, -1, msg.sender);
                    break;
            }
        }

        this_thread::sleep_for(chrono::milliseconds(sleep_inteval));
    }
}

void handle_rooms(int s_in_room, int s_room_nr, int s_gender, int sender){
    room_boxes[sender] = s_room_nr;

    if(gender == 1 && s_in_room == 1) {
        room_men[sender] = s_room_nr;  
    } else if(gender == 1 && s_in_room != 1) {
        room_men[sender] = -1; 
    }

    if(gender == 0 && s_in_room == 1){
        room_women[sender] = s_room_nr; 
    }else if(gender == 0 && s_in_room != 1){
        room_women[sender] = -1;     
    }     
    
}

void clean_rooms_info(){
    for(int i = 0; i < PROC_NUM; i++){
        room_boxes[i] = -1;
        room_men[i] = -1;
        room_women[i] = -1;
    }
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
    sleep_and_resend(0, 1000);
    change_state(1);
}

void handle_first_state(){
    send_to_all(1, timer,prev_state,-1);
    Message msg;
    int received_messages = 0;
    int additional_messages = 0;
    while(1){
        if(message_buffer.empty()) {
            unique_lock<mutex> lk(wait_for_message_mutex);
            wait_for_message.wait(lk);
        }   
        msg = read_message();

        switch(msg.type){
            case 1:
                if(is_my_priority_better(msg.m2, msg.m1, msg.sender)){
                    hold_messages.push(msg.sender);
                } else {
                    additional_messages++;
                    send_msg(1, timer, prev_state, -1 ,msg.sender);
                    send_msg(0, 0, -1, gender,msg.sender);
                }
                break;
            case 0:
                received_messages++;
                handle_rooms(msg.m1, msg.m2, msg.m3, msg.sender);
                if(received_messages == PROC_NUM + additional_messages - 1){
                    room = get_available_room();
                    if(room == -1){
                        break;
                    }

                    received_messages = 0;
                    additional_messages = 0;
                    change_state(2);
                    return;
                }
                break;
            case 20:
                handle_rooms(-1, msg.m1, msg.m2, msg.sender);

                if(received_messages == PROC_NUM + additional_messages - 1){
                    room = get_available_room();
                    if(room == -1){
                        break;
                    }

                    received_messages = 0;
                    additional_messages = 0;
                    waiting_for_room = false;
                    change_state(2);
                    return;
                }
                break;
            case 21: // odpowiedź na pytanie o timer
                send_msg(25, timer, -1, -1, msg.sender);
                break;
            case 22:
                handle_rooms(1, msg.m1, msg.m2, msg.sender);
                break;
        }

    }
}

void handle_second_state(){
    send_to_all(22, room, gender, -1);
    resend_hold_messages();
    sleep_and_resend(1, 1000);

    Message msg;
    int received_messages = 0;
    int max_timer = 0;
    bool do_while = true;
    send_to_all(21, -1, -1, -1);

    while(do_while){
        if(message_buffer.empty()) {
            unique_lock<mutex> lk(wait_for_message_mutex);
            wait_for_message.wait(lk);
        }   
        msg = read_message();

            switch(msg.type){
                case 1: // opowiedź na pytanie o wjeście do szatni
                    send_msg(0, 1, room, gender, msg.sender);
                    break;
                case 21: // odpowiedź na pytanie o timer
                    send_msg(25, timer, -1, -1, msg.sender);
                    break;
                case 25:
                    received_messages++;
                    if(msg.m1 > max_timer) max_timer = msg.m1;
                    if(received_messages == PROC_NUM - 1){
                        timer = max_timer + 1;
                        do_while = false;
                        break;
                    }
            }
    }

    clean_rooms_info();
    if(was_in_pool){
        was_in_pool = false;
        change_state(0);
        room = -1;
        send_to_all(20, room, gender, -1);
    }else{
        change_state(3);
        send_to_all(20,room,gender, -1);
    }
}

void handle_third_state(){
    sleep_and_resend(0,1000);
    was_in_pool = true;
    change_state(1);
}

int main(int argc, char **argv)
{
    srand( time( NULL ) );
    clean_rooms_info();


	MPI_Init(&argc, &argv);
	MPI_Comm_rank( MPI_COMM_WORLD, &proc_id);

    timer = proc_id;
    gender = proc_id%2;


    thread msg_th(message_reader); 

    sleep_and_resend(0, 1000);

    while(1){
        switch (state) {
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
            default:
                break;
            }
    }
    

    msg_th.join();
	MPI_Finalize();
}
 