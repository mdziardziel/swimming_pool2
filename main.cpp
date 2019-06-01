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


condition_variable wait_for_message;
mutex wait_for_message_mutex;

int state = 0;
int timer = -1;
int proc_id = -1;
int gender = -1;
int prev_state = -1;
int room = -1;
int room_capacity = 2;

int room_men[3] = {0};
int room_women[3] = {0};
int room_boxes[3] = {0};

bool waiting_for_room = false;

int get_zero_message[PROC_NUM] = {-1};

void message_reader(){ // służy TYLKO do odbierania wiadomości i przekazywania do bufora
    while(1){
        int tmp_msg[MAX_MSG_LEN];
        int tag;

        MPI_Status status;
        MPI_Recv(tmp_msg, MAX_MSG_LEN, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        Message m = Message(tmp_msg, status.MPI_SOURCE);

        message_buffer.push(m);
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

void resend_hold_messages(){
    // printf("lalalala\n");
    // printf("hold mess %d\n", hold_messages.size());
    Message msg;
    while(!hold_messages.empty()){
        // printf("popoppo\n");
        msg = hold_messages.front();
        send_msg(Message(0, 1, room, gender, msg.sender));
        hold_messages.pop();
    }
}

int get_available_room(){
    for(int i  = 0; i < 3; i++){
        printf("%d: SZTATNIA: %d, szafek zajętych: %d, kobiet: %d, mężczyzn %d\n", timer, room_boxes[i], room_women[i], room_men[i]);
        if(room_boxes[i] == room_capacity) {
            continue;
        } else if(room_boxes[i] > room_capacity){
            printf("WIĘCEJ ZAJĘTYCH SZAFEK NIŻ DOSTĘPNYCH!!111!1!!\n");
        }

        if(gender == 1 && room_women[i] > 0 ) continue;
        if(gender == 0 && room_men[i] > 0 ) continue;

        if(room_men[i] > 0 && room_women[i] > 0) printf("KOBIETA I MĘŻCZYZNA W JEDNEJ SZATNI!!11!!\n");

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
                    send_msg(20, timer, -1, -1, -1);
                    break;
            }
        }

        this_thread::sleep_for(chrono::milliseconds(sleep_inteval));
    }
}

void handle_rooms(int s_in_room, int s_room_nr, int s_gender){
    if(s_room_nr == -1) return;
    room_boxes[s_room_nr]++;

    if(s_gender == 1 && s_in_room == 1){
        room_men[s_room_nr]++;
    } else if(s_gender == 0 && s_in_room == 1){
        room_women[s_room_nr]++;
    }
}

void clean_rooms_info(){
    for(int i = 0; i < 3; i++){
        room_boxes[i] = 0;
        room_men[i] = 0;
        room_women[i] = 0;
    }
    for(int i = 0; i < PROC_NUM; i++){
        get_zero_message[i] = -1;
    }
}

void handle_rooms_2(int s_in_room, int s_room_nr, int s_gender){
    if(s_in_room == -1){
        room_boxes[s_room_nr]--;
    }

    if(s_gender == 1){
        room_men[s_room_nr]--;
    } else if(s_gender == 0){
        room_women[s_room_nr]--;
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
    while(1){
        // int msg[MAX_MSG_LEN + 1] = {-1};
        if(message_buffer.empty()) {
            // printf("%d locl\n", proc_id);
            unique_lock<mutex> lk(wait_for_message_mutex);
            wait_for_message.wait(lk);
        }   
        // printf("msg %d\n", message_buffer.size());   
        msg = read_message();
        // printf("odbiorca: %d; nadawca: %d; typ: %d %d %d %d\n", proc_id, msg.sender, msg.type, msg.m1, msg.m2, msg.m3); 

        switch(msg.type){
            case 1:
                if(is_my_priority_better(msg.m2, msg.m1, msg.sender)){
                    hold_messages.push(msg);
                    // printf("%d kolejkuje %d\n", proc_id, msg.sender);
                } else {
                    // printf("%d odsyła %d\n", proc_id, msg.sender);
                    send_msg(0, 0, -1, gender,msg.sender);
                }
                break;
            case 0:
                // printf("odbiorca: %d; nadawca: %d; typ: %d %d %d %d\n", proc_id, msg.sender, msg.type, msg.m1, msg.m2, msg.m3); 
                received_messages++;
                get_zero_message[msg.sender] = 1;
                handle_rooms(msg.m1, msg.m2, msg.m3);
                    // printf("odbiorca: %d; nadawca: %d; typ: %d %d %d %d\n", proc_id, msg.sender, msg.type, msg.m1, msg.m2, msg.m3); 
                // printf("xd %d\n", received_messages);
                // printf("xx\n");
                if(received_messages == PROC_NUM - 1){
                    // printf("xd %d\n", received_messages);
                    room = get_available_room();
                    if(room == -1){
                        // waiting_for_room = true;
                        break;
                    }

                    received_messages = 0;
                    // waiting_for_room = false;
                    change_state(2);
                    return;
                }
                break;
            case 20:
                if(get_zero_message[msg.sender] != 1) break;
                // if(!waiting_for_room) break;
                // odjąć szatnie
                handle_rooms_2(msg.m1, msg.m2, msg.m3);

                if(received_messages == PROC_NUM - 1){
                    // printf("xd %d\n", received_messages);
                    room = get_available_room();
                    if(room == -1){
                        break;
                    }

                    received_messages = 0;
                    waiting_for_room = false;
                    change_state(2);
                    return;
                }
                break;
            case 22:
                if(get_zero_message[msg.sender] != 1) break;

                handle_rooms(1, msg.m1, msg.m2);
                break;
        }

    }
}

void handle_second_state(){
    send_to_all(22, room, gender, -1);
    resend_hold_messages();
    sleep_and_resend(1, 1000);
    send_to_all(20, -1, room, gender);
    room = -1;
    clean_rooms_info();
    change_state(0);
}

void handle_third_state(){

}

int main(int argc, char **argv)
{
    srand( time( NULL ) );


	MPI_Init(&argc, &argv);
	MPI_Comm_rank( MPI_COMM_WORLD, &proc_id);
    // printf("START %d\n", proc_id);

    timer = proc_id;
    gender = proc_id%2;


    thread msg_th(message_reader); 

    sleep_and_resend(0, 1000);
    // send_to_all(1,2,3,4,proc_id);

    while(1){
        switch (state) {
            case 0: //sekcja lokalna
                // printf("%d -> sekcja lokalna\n", proc_id);
                handle_zero_state();
                // printf("%d <- sekcja lokaln\n", proc_id);
                break;
            case 1: // P1
                // printf("%d -> poczekalnia\n", proc_id);
                handle_first_state();
                // printf("%d <- poczekalnia\n", proc_id);
                break;
            case 2: // P2
                printf("%d -> szatnia, room : %d\n", proc_id, room);
                handle_second_state();
                printf("%d <- szatnia, room : %d\n", proc_id, room);
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