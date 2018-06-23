#ifndef _Q4SCLIENT_H_
#define _Q4SCLIENT_H_

#include <time.h> // to use clock
#include <stdio.h> // to use printf
#include <stdlib.h> // to use NULL
#include <string.h>  // to use strcpy, strcat, etc
#include <errno.h> // to describe errors
#include <pthread.h> // to create and manage threads
#include <stdbool.h> // to use boolean values
#include <math.h> // to use math related functions

// To create and manage connection
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

// To create and execute state machine
#include "fsm.h"
#include "fsm.c"

// To detect pressed keys and read them
#include "kbhit.h"
#include "kbhit.c"

// To use md5 algorithm
#include <openssl/md5.h>


#define CLK_FSM 0 // state machine update period

// FLAGS of the system
#define FLAG_CONNECT              0x01
#define FLAG_BEGIN 			          0x02
#define FLAG_RECEIVE_OK       		0x04
#define FLAG_GO_TO_0        			0x08
#define FLAG_GO_TO_1        			0x10
#define FLAG_TEMP_PING_0        	0x20
#define FLAG_RECEIVE_PING         0x40
#define FLAG_FINISH_PING     			0x80
#define FLAG_BWIDTH_BURST_SENT    0x100
#define FLAG_RECEIVE_BWIDTH    		0x200
#define FLAG_MEASURE_BWIDTH   		0x400
#define FLAG_FINISH_BWIDTH    	  0x800
#define FLAG_GO_TO_2   			      0x1000
#define FLAG_TEMP_PING_2      		0x2000
#define FLAG_CANCEL               0x4000
#define FLAG_RECEIVE_CANCEL       0x8000

// Q4S open ports
#define HOST_PORT_TCP 56001
#define HOST_PORT_UDP 56000
#define CLIENT_PORT_TCP 55001
#define CLIENT_PORT_UDP 55000

#define MAXDATASIZE 6000 // maximum size of a Q4S message in bytes
#define MESSAGE_BWIDTH_SIZE 1000 // size of Q4S BWIDTH messages in bytes
#define MAXNUMSAMPLES 300 // size of arrays of samples

// Type defined for Q4S messages
typedef struct {
  char start_line[200];
	char header[500];
  char body[5000];
} type_q4s_message;

// Type defined for latency measure
typedef struct {
  struct timespec tm;
  int seq_number;
} type_latency_tm;

// State machine type
typedef enum {
  WAIT_CONNECT,
	WAIT_START,
	HANDSHAKE,
	STAGE_0,
	PING_MEASURE_0,
  WAIT_NEXT,
	STAGE_1,
  BWIDTH_MEASURE,
  STAGE_2,
  PING_MEASURE_2,
  TERMINATION,
  END
} type_state_machine;

// Type defined for a q4s client session
typedef struct {
	int session_id;
  int expiration_time;
  int stage;
  int ping_clk_negotiation_client; // in ms
  int ping_clk_negotiation_server; // in ms
  int ping_clk_continuity; // in ms
  int bwidth_clk; // in ms
  int bwidth_messages_per_ms; // BWIDTH messages sent each 1 ms
  int ms_per_bwidth_message[11]; // intervals of ms for sending BWIDTH messages
  int window_size_latency_jitter;
  int window_size_packetloss;
  int seq_num_client;
  int seq_num_server;
  int seq_num_confirmed;
  int qos_level[2]; // {upstream, downstream}
  int alert_pause;
  bool alert_pause_activated;
  int latency_th;
  int jitter_th[2];  // {upstream, downstream}
  int bw_th[2];  // {upstream, downstream}
  float packetloss_th[2];  // {upstream, downstream}
  int latency_measure_server;
  int latency_measure_client;
  int jitter_measure_server; // upstream
  int jitter_measure_client; // downstream
  int bw_measure_server; // upstream
  int bw_measure_client; // downstream
  float packetloss_measure_server; // upstream
  float packetloss_measure_client; // downstream
  int latency_samples[MAXNUMSAMPLES];
  int latency_window[MAXNUMSAMPLES];
  int elapsed_time_samples[MAXNUMSAMPLES];
  int elapsed_time_window[MAXNUMSAMPLES];
  int packetloss_samples[MAXNUMSAMPLES];
  int packetloss_window[MAXNUMSAMPLES];
  char server_timestamp[40];
	type_q4s_message message_to_send;
	char prepared_message[MAXDATASIZE];
	type_q4s_message message_received;
} type_q4s_session;

// Prototype of check functions
int check_connect (fsm_t* this);
int check_begin (fsm_t* this);
int check_receive_ok (fsm_t* this);
int check_go_to_0 (fsm_t* this);
int check_temp_ping_0 (fsm_t* this);
int check_receive_ping (fsm_t* this);
int check_finish_ping (fsm_t* this);
int check_go_to_1 (fsm_t* this);
int check_bwidth_burst_sent (fsm_t* this);
int check_receive_bwidth (fsm_t* this);
int check_measure_bwidth (fsm_t* this);
int check_finish_bwidth (fsm_t* this);
int check_go_to_2 (fsm_t* this);
int check_temp_ping_2 (fsm_t* this);
int check_cancel (fsm_t* this);
int check_receive_cancel (fsm_t* this);

// Prototypes of action functions
void Setup (fsm_t* fsm);
void Begin (fsm_t* fsm);
void Store (fsm_t* fsm);
void Ready0 (fsm_t* fsm);
void Ping_Init (fsm_t* fsm);
void Ping (fsm_t* fsm);
void Update (fsm_t* fsm);
void Respond_ok (fsm_t* fsm);
void Decide (fsm_t* fsm);
void Ready1 (fsm_t* fsm);
void Bwidth_Decide (fsm_t* fsm);
void Ready2 (fsm_t* fsm);
void Cancel (fsm_t* fsm);
void Exit (fsm_t* fsm);

// Prototype of Q4S message related functions
void create_begin (type_q4s_message *q4s_message);
void create_ping (type_q4s_message *q4s_message);
void create_200 (type_q4s_message *q4s_message);
void create_bwidth (type_q4s_message *q4s_message);
void create_ready0 (type_q4s_message *q4s_message);
void create_ready1 (type_q4s_message *q4s_message);
void create_ready2 (type_q4s_message *q4s_message);
void create_cancel (type_q4s_message *q4s_message);
void create_request (type_q4s_message *q4s_message, char method[10],
  char header[500], char body[5000]);
void create_response (type_q4s_message *q4s_message, char status_code[10],
	char reason_phrase[10], char header[500], char body[5000]);
void prepare_message (type_q4s_message *q4s_message, char prepared_message[MAXDATASIZE]);
bool store_message (char received_message[MAXDATASIZE], type_q4s_message *q4s_message);

// Prototype of Q4S parameter storage function
void store_parameters(type_q4s_session *q4s_session, type_q4s_message *q4s_message);

// Prototype of functions for storage and update of measures
void update_latency(type_q4s_session *q4s_session, int latency_measured);
void update_jitter(type_q4s_session *q4s_session, int elapsed_time);
void update_packetloss(type_q4s_session *q4s_session, int lost_packets);


// Prototype of connection functions
int connect_to_server();
void send_message_TCP (char prepared_message[MAXDATASIZE]);
void send_message_UDP (char prepared_message[MAXDATASIZE]);
void *thread_receives_TCP();
void *thread_receives_UDP();


// Prototipe of function exploring the keyboard
void *thread_explores_keyboard();

// Prototype of timer functions
void *ping_timeout_0();
void *ping_timeout_2();
void *bwidth_delivery();
void *bwidth_reception_timeout();
void *alert_pause_timeout();
void *end_measure_timeout();

// Prototype of initialization function
int system_setup();

// Other protoypes of functions
void delay (int milliseconds);
void udelay (int microseconds);
const char * current_time();
int ms_elapsed(struct timespec tm1, struct timespec tm2);
int us_elapsed(struct timespec tm1, struct timespec tm2);
void sort_array(int samples[MAXNUMSAMPLES], int length);
int min (int a, int b);
int max (int a, int b);
static void MD5mod(const char* str, char hash[33]);

#endif /* _Q4SCLIENT_H_ */
