#include "q4s_client.h"

//-----------------------------------------------
// VARIABLES
//-----------------------------------------------

// FOR Q4S SESSION MANAGING
// Q4S session
static type_q4s_session q4s_session;
// Variable to store the flags
long int flags = 0;
// Variable to store state machine
fsm_t* q4s_fsm;

// FOR CONNECTION MANAGING
// Structs with info for the connection
struct sockaddr_in client_TCP, client_UDP, server_TCP, server_UDP;
// Variable with struct length
socklen_t slen;
// Struct with host info
struct hostent *host;
// Variable for socket assignment
int socket_TCP, socket_UDP;
// Variable for TCP socket buffer
char buffer_TCP[MAXDATASIZE];
// Variable for UDP socket buffer
char buffer_UDP[MAXDATASIZE];

// FOR THREAD MANAGING
// Thread to check reception of TCP data
pthread_t receive_TCP_thread;
// Variable used to cancel a thread respecting the mutex
bool cancel_TCP_thread;
// Thread to check reception of UDP data
pthread_t receive_UDP_thread;
// Variable used to cancel a thread respecting the mutex
bool cancel_UDP_thread;
// Thread to check pressed keys on the keyboard
pthread_t keyboard_thread;
// Variable used to cancel a thread respecting the mutex
bool cancel_keyboard_thread;
// Thread acting as timer for ping delivery in Stage 0
pthread_t timer_ping_0;
// Variable used to cancel a thread respecting the mutex
bool cancel_timer_ping_0;
// Thread acting as timer for preventive wait when finishing measures
pthread_t timer_end_measure;
// Variable used to cancel a thread respecting the mutex
bool cancel_timer_end_measure;
// Thread acting as timer for ping delivery in Stage 2
pthread_t timer_ping_2;
// Variable used to cancel a thread respecting the mutex
bool cancel_timer_ping_2;
// Thread acting as timer for bwidth delivery
pthread_t timer_delivery_bwidth;
// Variable used to cancel a thread respecting the mutex
bool cancel_timer_delivery_bwidth;
// Thread acting as timer for bwidth reception
pthread_t timer_reception_bwidth;
// Variable used to cancel a thread respecting the mutex
bool cancel_timer_reception_bwidth;
// Thread acting as timer for alert pause
pthread_t timer_alert;
// Variable used to cancel a thread respecting the mutex
bool cancel_timer_alert;
// Variable for mutual exclusion with flags
pthread_mutex_t mutex_flags;
// Variable for mutual exclusion with q4s session
pthread_mutex_t mutex_session;
// Variable for mutual exclusion with TCP buffer
pthread_mutex_t mutex_buffer_TCP;
// Variable for mutual exclusion with UDP buffer
pthread_mutex_t mutex_buffer_UDP;
// Variable for mutual exclusion with message printing
pthread_mutex_t mutex_print;
// Variable for mutual exclusion with timers for latency measure
pthread_mutex_t mutex_tm_latency;
// Variable for mutual exclusion with timers for jitter measure
pthread_mutex_t mutex_tm_jitter;


// FOR TIME
// Variables to store time when a Q4S PING is sent and its sequence number
type_latency_tm tm_latency_start1;
type_latency_tm tm_latency_start2;
type_latency_tm tm_latency_start3;
type_latency_tm tm_latency_start4;
// Variable to store time when a Q4S 200 OK is received
type_latency_tm tm_latency_end;
// Variable to store the time when a Q4S PING is received
struct timespec tm1_jitter;
// Variable to store the time when next Q4S PING is received
struct timespec tm2_jitter;
// Variables to measure time elapsed in bwidth delivery
struct timespec tm1_adjust;
struct timespec tm2_adjust;

// FOR SECURITY
// Variable used as context when using MD5 algorithm
MD5_CTX  md5_ctx;

// AUXILIARY VARIABLES
// Indicates the position of the last sample in the array
int pos_latency;
// Indicates the position of the last sample in the array
int pos_elapsed_time;
// Indicates the position of the last sample in the array
int pos_packetloss;
// Variable that stores the number of successful samples needed to pass to next stage
int num_samples_succeed;
// Variable that indicates if end_measure_timeout is activated
bool end_measure_timeout_activated = false;
// Variable that indicates if bwidth_reception_timeout is activated
bool bwidth_reception_timeout_activated = false;
// Variable used to obviate packet losses in jitter measure
int num_ping;
// Variable that indicates the number of new packet losses occurred
int num_packet_lost;
// Variable that stores the number of Q4S PING/BWIDTH sent since last alert
int num_packet_since_alert;
// Variable that shows number of Q4S BWIDTHs received in a period
int num_bwidth_received;
// Variable that stores number of failure messages received
int num_failures;

bool finished;


//-----------------------------------------------
// AUXILIARY FUNCTIONS
//-----------------------------------------------

// GENERAL FUNCTIONS

// Waits for x milliseconds
void delay (int milliseconds) {
	long pause;
	clock_t now, then;
	pause = milliseconds*(CLOCKS_PER_SEC/1000);
	now = then = clock();
	while((now-then) < pause) {
		now = clock();
	}
}

// Waits for x microseconds
void udelay (int microseconds) {
	long pause;
	clock_t now, then;
	pause = microseconds*(CLOCKS_PER_SEC/1000000);
	now = then = clock();
	while((now-then) < pause) {
		now = clock();
	}
}

// Returns interval between two moments of time in ms
int ms_elapsed(struct timespec tm1, struct timespec tm2) {
  unsigned long long t = 1000 * (tm2.tv_sec - tm1.tv_sec)
    + (tm2.tv_nsec - tm1.tv_nsec) / 1000000;
	return t;
}

// Returns interval between two moments of time in us
int us_elapsed(struct timespec tm1, struct timespec tm2) {
  unsigned long long t = 1000000 * (tm2.tv_sec - tm1.tv_sec)
    + (tm2.tv_nsec - tm1.tv_nsec) / 1000;
	return t;
}

// Returns current time formatted to be included in Q4S header
const char * current_time() {
  time_t currentTime;
  time(&currentTime);
  char * now = ctime(&currentTime);
  char whitespace = ' ';
	char comma = ',';
  char prepared_time[30] = {now[0], now[1], now[2], comma, whitespace,
		now[8], now[9], whitespace, now[4], now[5], now[6], whitespace, now[20],
		now[21], now[22], now[23], whitespace, now[11], now[12], now[13],
		now[14], now[15], now[16], now[17],
    now[18], whitespace, 'G', 'M', 'T'};
  memset(now, '\0', sizeof(prepared_time));
	strcpy(now, prepared_time);
  return now;
}

// Sorts int array from minimum (not 0) to maximum values
void sort_array(int samples[MAXNUMSAMPLES], int length) {
	if (length <= 1) {
		return;
	}
	int temp;
	int i, j;
	bool swapped = false;
	// Loop through all numbers
	for(i = 0; i < length-1; i++) {
		if(samples[i+1] == 0) {
			break;
		}
		swapped = false;
		// Loop through numbers falling keyboard_ahead
		for(j = 0; j < length-1-i; j++) {
			if(samples[j] > samples[j+1]) {
				temp = samples[j];
				samples[j] = samples[j+1];
				samples[j+1] = temp;
				swapped = true;
			}
		}
		if (!swapped) {
			break;
		}
	}
}

// Returns the minimum value, given 2 integers
int min (int a, int b) {
	return a < b ? a:b;
}

// Returns the maximum value, given 2 integers
int max (int a, int b) {
	return a > b ? a:b;
}

// SECURITY FUNCTIONS

// Creates the MD5 hash of a char array
static void MD5mod(const char* str, char hash[33]) {
    char digest[16];
    int length = strlen(str);

    MD5_Init(&md5_ctx);
    MD5_Update(&md5_ctx, str, length);
    MD5_Final(digest, &md5_ctx);

    for (int i = 0; i < 16; ++i) {
      sprintf(&hash[i*2], "%02x", (unsigned int)digest[i]);
    }
}

// INITIALITATION FUNCTIONS

// System initialitation
// Creates a thread to explore keyboard and configures mutex
int system_setup (void) {
	slen = sizeof(server_UDP);

	pthread_mutex_init(&mutex_flags, NULL);
	pthread_mutex_init(&mutex_session, NULL);
	pthread_mutex_init(&mutex_buffer_TCP, NULL);
	pthread_mutex_init(&mutex_buffer_UDP, NULL);
	pthread_mutex_init(&mutex_print, NULL);
	pthread_mutex_init(&mutex_tm_latency, NULL);
	pthread_mutex_init(&mutex_tm_jitter, NULL);

	cancel_TCP_thread = false;
	cancel_UDP_thread = false;
	cancel_timer_ping_0 = false;
	cancel_timer_ping_2 = false;
	cancel_timer_alert = false;
	cancel_timer_end_measure = false;
	cancel_timer_delivery_bwidth = false;
	cancel_timer_reception_bwidth = false;
	cancel_keyboard_thread = false;

	// Throws a thread to explore PC keyboard
	pthread_create(&keyboard_thread, NULL, (void*)thread_explores_keyboard, NULL);
	return 1;
}

// State machine initialitation
// Puts every flag to 0
void fsm_setup(fsm_t* juego_fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	flags = 0;
	pthread_mutex_unlock(&mutex_flags);
}


// Q4S MESSAGE RELATED FUNCTIONS

// Creation of Q4S BEGIN message
// Creates a default message unless client wants to suggest Q4S quality thresholds
void create_begin (type_q4s_message *q4s_message) {
  char input[100]; // to store user inputs
	memset(input, '\0', sizeof(input));

	char header[500]; // it will be filled with header fields
  memset(header, '\0', sizeof(header));

	char body[5000]; // it will be empty or filled with SDP parameters
	memset(body, '\0', sizeof(body)); // body is empty by default

	char h1[100];
	memset(h1, '\0', sizeof(h1));

	char h2[100];
	memset(h2, '\0', sizeof(h2));

	char h3[100];
	memset(h3, '\0', sizeof(h3));

	char h4[100];
	memset(h4, '\0', sizeof(h4));

  pthread_mutex_lock(&mutex_print);
	printf("\nDo you want to specify desired quality thresholds? (yes/no): ");
	pthread_mutex_unlock(&mutex_print);

  scanf("%s", input); // variable input stores user's answer

  // If user wants to suggest Q4S quality thresholds
	if (strstr(input, "yes")) {
		char v[100];
		memset(v, '\0', sizeof(v));
		char o[100];
		memset(o, '\0', sizeof(o));
		char s[100];
		memset(s, '\0', sizeof(s));
		char i[100];
		memset(i, '\0', sizeof(i));
		char t[100];
		memset(t, '\0', sizeof(t));

		// Default SDP parameters
		strcpy(v, "v=0\n");
		strcpy(o, "o=\n");
		strcpy(s, "s=Q4S\n");
		strcpy(i, "i=Q4S desired parameters\n");
		strcpy(t, "t=0 0\n");

		char a1[100];
		memset(a1, '\0', sizeof(a1));
		char a2[100];
		memset(a2, '\0', sizeof(a2));
		char a3[100];
		memset(a3, '\0', sizeof(a3));
		char a4[100];
		memset(a4, '\0', sizeof(a4));
		char a5[100];
		memset(a5, '\0', sizeof(a5));

    // Includes user's suggestion about QoS levels (upstream and downstream)
		strcpy(a1, "a=qos-level:");
		pthread_mutex_lock(&mutex_print);
		printf("\nEnter upstream QoS level (from 0 to 9): ");
		pthread_mutex_unlock(&mutex_print);
	  scanf("%s", input);
		strcat(a1, input);
		strcat(a1, "/");
	  memset(input, '\0', strlen(input));
		pthread_mutex_lock(&mutex_print);
		printf("Enter downstream QoS level (from 0 to 9): ");
		pthread_mutex_unlock(&mutex_print);
	  scanf("%s", input);
		strcat(a1, input);
		strcat(a1, "\n");
		memset(input, '\0', strlen(input));

    // Includes user's suggestion about latency threshold
		strcpy(a2, "a=latency:");
		pthread_mutex_lock(&mutex_print);
		printf("Enter latency threshold (in ms): ");
		pthread_mutex_unlock(&mutex_print);
	  scanf("%s", input);
		strcat(a2, input);
		strcat(a2, "\n");
	  memset(input, '\0', strlen(input));

    // Includes user's suggestion about jitter threshold (upstream and downstream)
		strcpy(a3, "a=jitter:");
		pthread_mutex_lock(&mutex_print);
		printf("Enter upstream jitter threshold (in ms): ");
		pthread_mutex_unlock(&mutex_print);
	  scanf("%s", input);
		strcat(a3, input);
		strcat(a3, "/");
	  memset(input, '\0', strlen(input));
		pthread_mutex_lock(&mutex_print);
		printf("Enter downstream jitter threshold (in ms): ");
		pthread_mutex_unlock(&mutex_print);
	  scanf("%s", input);
		strcat(a3, input);
		strcat(a3, "\n");
	  memset(input, '\0', strlen(input));

    // Includes user's suggestion about bandwidth threshold (upstream and downstream)
		strcpy(a4, "a=bandwidth:");
		pthread_mutex_lock(&mutex_print);
		printf("Enter upstream bandwidth threshold (in kbps): ");
		pthread_mutex_unlock(&mutex_print);
	  scanf("%s", input);
		strcat(a4, input);
		strcat(a4, "/");
	  memset(input, '\0', strlen(input));
		pthread_mutex_lock(&mutex_print);
		printf("Enter downstream bandwidth threshold (in kbps): ");
		pthread_mutex_unlock(&mutex_print);
	  scanf("%s", input);
		strcat(a4, input);
		strcat(a4, "\n");
	  memset(input, '\0', strlen(input));

    // Includes user's suggestion about packetloss threshold (upstream and downstream)
		strcpy(a5, "a=packetloss:");
		pthread_mutex_lock(&mutex_print);
		printf("Enter upstream packetloss threshold (percentage from 0.00 to 1.00): ");
    pthread_mutex_unlock(&mutex_print);
		scanf("%s", input);
		strcat(a5, input);
		strcat(a5, "/");
	  memset(input, '\0', strlen(input));
		pthread_mutex_lock(&mutex_print);
		printf("Enter downstream packetloss threshold (percentage from 0.00 to 1.00): ");
    pthread_mutex_unlock(&mutex_print);
		scanf("%s", input);
		strcat(a5, input);
		strcat(a5, "\n");
	  memset(input, '\0', strlen(input));

    // Prepares body with SDP parameters
		strcpy(body, v);
		strcat(body, o);
		strcat(body, s);
		strcat(body, i);
		strcat(body, t);
		strcat(body, a1);
		strcat(body, a2);
		strcat(body, a3);
		strcat(body, a4);
		strcat(body, a5);

		// Creates a MD5 hash for the sdp and includes it in the header
		char hash[33];
		memset(hash, '\0', sizeof(hash));
	  MD5mod(body, hash);
		strcpy(h4, "Signature: ");
		strcat(h4, hash);
		strcat(h4, "\n");
	}

  // Prepares some header fields
	strcpy(h1, "Content-Type: application/sdp\n");
	strcpy(h2, "User-Agent: q4s-ua-experimental-1.0\n");

  // Includes body length in "Content Length" header field
	strcpy(h3, "Content Length: ");
	int body_length = strlen(body);
	char s_body_length[10];
	sprintf(s_body_length, "%d", body_length);
  strcat(h3, s_body_length);
	strcat(h3, "\n");

  // Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);
	strcat(header, h3);
	if (strlen(h4) > 0) {
		strcat(header, h4);
	}

  // Delegates in a request creation function
  create_request (q4s_message,"BEGIN", header, body);
}

// Creation of Q4S READY 0 message
void create_ready0 (type_q4s_message *q4s_message) {
  char body[5000]; // it will be empty or filled with SDP parameters
	memset(body, '\0', sizeof(body)); // body is empty by default

	char header[500]; // it will be filled with header fields
  memset(header, '\0', sizeof(header));

	char h1[100];
	memset(h1, '\0', sizeof(h1));

	char h2[100];
	memset(h2, '\0', sizeof(h2));

	char h3[100];
	memset(h3, '\0', sizeof(h3));

	char h4[100];
	memset(h4, '\0', sizeof(h4));

	char h5[100];
	memset(h5, '\0', sizeof(h5));

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");

  // Prepares some header fields
	strcpy(h2, "Stage: 0\n");
	strcpy(h3, "Content-Type: application/sdp\n");
	strcpy(h4, "User-Agent: q4s-ua-experimental-1.0\n");

  // Includes body length in "Content Length" header field
	strcpy(h5, "Content Length: ");
	int body_length = strlen(body);
	char s_body_length[10];
	sprintf(s_body_length, "%d", body_length);
  strcat(h5, s_body_length);
	strcat(h5, "\n");

  // Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);
	strcat(header, h3);
	strcat(header, h4);
	strcat(header, h5);

  // Delegates in a request creation function
  create_request (q4s_message,"READY", header, body);
}

// Creation of Q4S READY 1 message
void create_ready1 (type_q4s_message *q4s_message) {
	char body[5000]; // it will be empty or filled with SDP parameters
	memset(body, '\0', sizeof(body)); // body is empty by default

	char header[500]; // it will be filled with header fields
	memset(header, '\0', sizeof(header));

	char h1[100];
	memset(h1, '\0', sizeof(h1));

	char h2[100];
	memset(h2, '\0', sizeof(h2));

	char h3[100];
	memset(h3, '\0', sizeof(h3));

	char h4[100];
	memset(h4, '\0', sizeof(h4));

	char h5[100];
	memset(h5, '\0', sizeof(h5));

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");

	// Prepares some header fields
	strcpy(h2, "Stage: 1\n");
	strcpy(h3, "Content-Type: application/sdp\n");
	strcpy(h4, "User-Agent: q4s-ua-experimental-1.0\n");

	// Includes body length in "Content Length" header field
	strcpy(h5, "Content Length: ");
	int body_length = strlen(body);
	char s_body_length[10];
	sprintf(s_body_length, "%d", body_length);
	strcat(h5, s_body_length);
	strcat(h5, "\n");

	// Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);
	strcat(header, h3);
	strcat(header, h4);
	strcat(header, h5);

	// Delegates in a request creation function
	create_request (q4s_message,"READY", header, body);
}

// Creation of Q4S READY 2 message
void create_ready2 (type_q4s_message *q4s_message) {
	char body[5000]; // it will be empty or filled with SDP parameters
	memset(body, '\0', sizeof(body)); // body is empty by default

	char header[500]; // it will be filled with header fields
  memset(header, '\0', sizeof(header));

	char h1[100];
	memset(h1, '\0', sizeof(h1));

	char h2[100];
	memset(h2, '\0', sizeof(h2));

	char h3[100];
	memset(h3, '\0', sizeof(h3));

	char h4[100];
	memset(h4, '\0', sizeof(h4));

	char h5[100];
	memset(h5, '\0', sizeof(h5));

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");

  // Prepares some header fields
	strcpy(h2, "Stage: 2\n");
	strcpy(h3, "Content-Type: application/sdp\n");
	strcpy(h4, "User-Agent: q4s-ua-experimental-1.0\n");

  // Includes body length in "Content Length" header field
	strcpy(h5, "Content Length: ");
	int body_length = strlen(body);
	char s_body_length[10];
	sprintf(s_body_length, "%d", body_length);
  strcat(h5, s_body_length);
	strcat(h5, "\n");

  // Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);
	strcat(header, h3);
	strcat(header, h4);
	strcat(header, h5);

  // Delegates in a request creation function
  create_request (q4s_message,"READY", header, body);
}


// Creation of Q4S PING message
void create_ping (type_q4s_message *q4s_message) {
  char body[5000]; // it will be empty or filled with SDP parameters
	memset(body, '\0', sizeof(body)); // body is empty by default

	char header[500]; // it will be filled with header fields
  memset(header, '\0', sizeof(header));

	char h1[100];
	memset(h1, '\0', sizeof(h1));

	char h2[100];
	memset(h2, '\0', sizeof(h2));

	char h3[100];
	memset(h3, '\0', sizeof(h3));

	char h4[100];
	memset(h4, '\0', sizeof(h4));

	char h5[100];
	memset(h5, '\0', sizeof(h5));

	char h6[100];
	memset(h6, '\0', sizeof(h6));

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");
	// Includes sequence number of Q4S PING to send
	strcpy(h2, "Sequence-Number: ");
	char s_seq_num[10];
	sprintf(s_seq_num, "%d", (&q4s_session)->seq_num_client);
	strcat(h2, s_seq_num);
	strcat(h2, "\n");
	// Includes user agent
	strcpy(h3, "User-Agent: q4s-ua-experimental-1.0\n");
	// Includes timestamp
	strcpy(h4, "Timestamp: ");
	const char * now = current_time();
	strcat(h4, now);
	strcat(h4, "\n");
	// Includes measures
	strcpy(h5, "Measurements: ");
	strcat(h5, "l=");
	if((&q4s_session)->latency_measure_client && (&q4s_session)->latency_th) {
		char s_latency[6];
		sprintf(s_latency, "%d", (&q4s_session)->latency_measure_client);
    strcat(h5, s_latency);
	} else {
		strcat(h5, " ");
	}
	strcat(h5, ", j=");
	if((&q4s_session)->jitter_measure_client >= 0 && (&q4s_session)->jitter_th[1]) {
		char s_jitter[6];
		sprintf(s_jitter, "%d", (&q4s_session)->jitter_measure_client);
    strcat(h5, s_jitter);
	} else {
		strcat(h5, " ");
	}
	strcat(h5, ", pl=");
	if((&q4s_session)->packetloss_measure_client >= 0 && (&q4s_session)->packetloss_th[1]) {
		char s_packetloss[6];
		sprintf(s_packetloss, "%.2f", (&q4s_session)->packetloss_measure_client);
    strcat(h5, s_packetloss);
	} else {
		strcat(h5, " ");
	}
	strcat(h5, ", bw=");
	if((&q4s_session)->bw_measure_client >= 0 && (&q4s_session)->bw_th[1]) {
		char s_bw[6];
		sprintf(s_bw, "%d", (&q4s_session)->bw_measure_client);
    strcat(h5, s_bw);
	} else {
		strcat(h5, " ");
	}
	strcat(h5, "\n");
  // Includes body length in "Content Length" header field
	strcpy(h6, "Content Length: ");
	int body_length = strlen(body);
	char s_body_length[10];
	sprintf(s_body_length, "%d", body_length);
  strcat(h6, s_body_length);
	strcat(h6, "\n");

  // Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);
	strcat(header, h3);
	strcat(header, h4);
	strcat(header, h5);
	strcat(header, h6);

  // Delegates in a request creation function
  create_request (q4s_message,"PING", header, body);
}

// Creation of Q4S 200 OK message
// In respond to Q4S PING messages
void create_200 (type_q4s_message *q4s_message) {
	char header[500];  // to store header of the message
  memset(header, '\0', sizeof(header));

	char body[5000];  // to store body of the message
	memset(body, '\0', sizeof(body));

	char h1[100];
	memset(h1, '\0', sizeof(h1));

	char h2[100];
	memset(h2, '\0', sizeof(h2));

	char h3[100];
	memset(h3, '\0', sizeof(h3));

	char h4[100];
	memset(h4, '\0', sizeof(h4));

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");
	// Includes sequence number of Q4S PING received
	strcpy(h2, "Sequence-Number: ");
	char s_seq_num[10];
	sprintf(s_seq_num, "%d", (&q4s_session)->seq_num_server);
	strcat(h2, s_seq_num);
	strcat(h2, "\n");
	// Includes body length in "Content Length" header field
	strcpy(h3, "Content Length: ");
	int body_length = strlen(body);
	char s_body_length[10];
	sprintf(s_body_length, "%d", body_length);
	strcat(h3, s_body_length);
	strcat(h3, "\n");
	// Includes timestamp of server
	strcpy(h4, "Timestamp: ");
	strcat(h4, (&q4s_session)->server_timestamp);
	strcat(h4, "\n");

  // Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);
	strcat(header, h3);
	strcat(header, h4);

  // Delegates in a response creation function
  create_response (q4s_message, "200", "OK", header, body);
}

// Creation of Q4S BWIDTH message
void create_bwidth (type_q4s_message *q4s_message) {
	char body[5000]; // it will be empty or filled with SDP parameters
	memset(body, '\0', sizeof(body));

  strcpy(body, "a");
	for (int i = 0; i < 998; i++) {
		strcat(body, "a");
	}
	strcat(body, "\n");

	char header[500]; // it will be filled with header fields
	memset(header, '\0', sizeof(header));

	char h1[100];
	memset(h1, '\0', sizeof(h1));

	char h2[100];
	memset(h2, '\0', sizeof(h2));

	char h3[100];
	memset(h3, '\0', sizeof(h3));

	char h4[100];
	memset(h4, '\0', sizeof(h4));

	char h5[100];
	memset(h5, '\0', sizeof(h5));

	char h6[100];
	memset(h6, '\0', sizeof(h6));

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");
	// Includes sequence number of Q4S PING to send
	strcpy(h2, "Sequence-Number: ");
	char s_seq_num[10];
	sprintf(s_seq_num, "%d", (&q4s_session)->seq_num_client);
	strcat(h2, s_seq_num);
	strcat(h2, "\n");
	// Includes user agent
	strcpy(h3, "User-Agent: q4s-ua-experimental-1.0\n");
	// Includes content type
	strcpy(h4, "Content-Type: text\n");
	// Includes measures
	strcpy(h5, "Measurements: ");
	strcat(h5, "l=");
	if((&q4s_session)->latency_measure_client && (&q4s_session)->latency_th) {
		char s_latency[6];
		sprintf(s_latency, "%d", (&q4s_session)->latency_measure_client);
    strcat(h5, s_latency);
	} else {
		strcat(h5, " ");
	}
	strcat(h5, ", j=");
	if((&q4s_session)->jitter_measure_client >= 0 && (&q4s_session)->jitter_th[1]) {
		char s_jitter[6];
		sprintf(s_jitter, "%d", (&q4s_session)->jitter_measure_client);
    strcat(h5, s_jitter);
	} else {
		strcat(h5, " ");
	}
	strcat(h5, ", pl=");
	if((&q4s_session)->packetloss_measure_client >= 0 && (&q4s_session)->packetloss_th[1]) {
		char s_packetloss[6];
		sprintf(s_packetloss, "%.2f", (&q4s_session)->packetloss_measure_client);
    strcat(h5, s_packetloss);
	} else {
		strcat(h5, " ");
	}
	strcat(h5, ", bw=");
	if((&q4s_session)->bw_measure_client >= 0 && (&q4s_session)->bw_th[1]) {
		char s_bw[6];
		sprintf(s_bw, "%d", (&q4s_session)->bw_measure_client);
    strcat(h5, s_bw);
	} else {
		strcat(h5, " ");
	}
	strcat(h5, "\n");

  // Includes body length in "Content Length" header field
	strcpy(h6, "Content Length: ");
	int body_length = strlen(body);
	char s_body_length[10];
	sprintf(s_body_length, "%d", body_length);
  strcat(h6, s_body_length);
	strcat(h6, "\n");

  // Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);
	strcat(header, h3);
	strcat(header, h4);
	strcat(header, h5);
	strcat(header, h6);

  // Delegates in a request creation function
  create_request (q4s_message,"BWIDTH", header, body);
}

// Creation of Q4S CANCEL message
// Creates a default CANCEL message
void create_cancel (type_q4s_message *q4s_message) {
	char body[5000]; // it will be empty or filled with SDP parameters
	memset(body, '\0', sizeof(body));

	char header[500]; // it will be filled with header fields
	memset(header, '\0', sizeof(header));

	char h1[100];
	memset(h1, '\0', sizeof(h1));

	char h2[100];
	memset(h2, '\0', sizeof(h2));

	char h3[100];
	memset(h3, '\0', sizeof(h3));

	char h4[100];
	memset(h4, '\0', sizeof(h4));

	// Creates default header fields
	strcpy(h1, "User-Agent: q4s-ua-experimental-1.0\n");

	// Includes session ID
	strcpy(h2, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h2, s_session_id);
	strcat(h2, "\n");

	strcpy(h3, "Content-Type: application/sdp\n");

	// Includes body length in "Content Length" header field
	strcpy(h4, "Content Length: ");
	int body_length = strlen(body);
	char s_body_length[10];
	sprintf(s_body_length, "%d", body_length);
  strcat(h4, s_body_length);
	strcat(h4, "\n");

  // Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);
	strcat(header, h3);
	strcat(header, h4);

	// Delegates in a request creation function
  create_request (q4s_message,"CANCEL", header, body);
}

// Creation of Q4S requests
// Receives parameters to create the request line (start line)
// Receives prepared header and body from more specific functions
// Stores start line, header and body in a q4s message received as parameter
void create_request (type_q4s_message *q4s_message, char method[10],
	char header[500], char body[5000]) {
    memset((q4s_message)->start_line, '\0', sizeof((q4s_message)->start_line));
		memset((q4s_message)->header, '\0', sizeof((q4s_message)->header));
		memset((q4s_message)->body, '\0', sizeof((q4s_message)->body));
		// Establish Request URI
		char req_uri[100] = " q4s://www.example.com";
		// Prepares and stores start line
    strcpy((q4s_message)->start_line, method);
    strcat((q4s_message)->start_line, req_uri);
    strcat((q4s_message)->start_line, " Q4S/1.0");
		// Stores header
		strcpy((q4s_message)->header, header);
		// Stores body
		strcpy((q4s_message)->body, body);
}

// Fine tuning of Q4S messages before being sent
// Converts from type_q4s_mesage to char[MAXDATASIZE]
void prepare_message (type_q4s_message *q4s_message, char prepared_message[MAXDATASIZE]) {
  memset(prepared_message, '\0', MAXDATASIZE);
	// Pays special attention to Q4S message format
	strcpy(prepared_message, (q4s_message)->start_line);
  strcat(prepared_message, "\n");
  strcat(prepared_message, (q4s_message)->header);
  strcat(prepared_message, "\n");
  strcat(prepared_message, (q4s_message)->body);
}

// Creation of Q4S responses
// Receives parameters to create the status line (start line)
// Receives prepared header and body from more specific functions
// Stores start line, header and body in a q4s message received as parameter
void create_response (type_q4s_message *q4s_message, char status_code[10],
	char reason_phrase[10], char header[500], char body[5000]) {
		memset((q4s_message)->start_line, '\0', sizeof((q4s_message)->start_line));
		memset((q4s_message)->header, '\0', sizeof((q4s_message)->header));
		memset((q4s_message)->body, '\0', sizeof((q4s_message)->body));
		// Prepares and stores start line
    strcpy((q4s_message)->start_line, "Q4S/1.0 ");
    strcat((q4s_message)->start_line, status_code);
    strcat((q4s_message)->start_line, " ");
    strcat((q4s_message)->start_line, reason_phrase);
		// Stores header
		strcpy((q4s_message)->header, header);
		// Stores body
		strcpy((q4s_message)->body, body);
}

// Validation and storage of a Q4S message just received
// Converts from char[MAXDATASIZE] to type_q4s_message
bool store_message (char received_message[MAXDATASIZE], type_q4s_message *q4s_message) {
	if (strlen(received_message) > 5700) {
		pthread_mutex_lock(&mutex_print);
		printf("\nMessage received is too long\n");
		pthread_mutex_unlock(&mutex_print);
		return false;
	}

  // Auxiliary variables
	char *fragment1;
  char *fragment2;

  char start_line[MAXDATASIZE]; // to store start line
  char header[MAXDATASIZE];  // to store header
  char body[MAXDATASIZE];  // to store body

  memset(start_line, '\0', strlen(start_line));
  memset(header, '\0', strlen(header));
	memset(body, '\0', strlen(body));

  // Copies header + body
  fragment1 = strstr(received_message, "\n");
  // Obtains start line
  strncpy(start_line, received_message, strlen(received_message)-strlen(fragment1));
  // Quits initial "\n"
  fragment1 = fragment1 + 1;

  // Copies body (if present)
	fragment2 = strstr(received_message, "\n\n");
	if (strcmp(fragment2, "\n\n") != 0 ) {
		// Obtains header
	  strncpy(header, fragment1, strlen(fragment1)-(strlen(fragment2)-1));
	  // Quits initial "\n\n"
	  fragment2 = fragment2 + 2;
	  // Obtains body
	  strncpy(body, fragment2, strlen(fragment2));
	} else {
		// Obtains header
	  strncpy(header, fragment1, strlen(fragment1));
		// Body is empty
		memset(body, '\0', strlen(body));
	}

	// Creates a copy of start line to manipulate it
	char copy_start_line[MAXDATASIZE];
	memset(copy_start_line, '\0', strlen(copy_start_line));
  strcpy(copy_start_line, start_line);

	// Creates a copy of header to manipulate it
	char copy_header[MAXDATASIZE];
	memset(copy_header, '\0', strlen(copy_header));
  strcpy(copy_header, header);

	// Creates a copy of body to manipulate it
	char copy_body[MAXDATASIZE];
	memset(copy_body, '\0', strlen(copy_body));
	strcpy(copy_body, body);


	if (strlen(copy_start_line) > 200) {
		pthread_mutex_lock(&mutex_print);
		printf("\nURI received is too long\n");
		pthread_mutex_unlock(&mutex_print);
		return false;
	}

	if (strstr(copy_start_line, "\\") != NULL) {
		strcpy(copy_start_line, start_line);
		pthread_mutex_lock(&mutex_print);
		printf("\nInvalid URI\n");
		pthread_mutex_unlock(&mutex_print);
		return false;
	}
	strcpy(copy_start_line, start_line);

	if (strstr(copy_start_line, "\a") != NULL) {
		strcpy(copy_start_line, start_line);
		pthread_mutex_lock(&mutex_print);
		printf("\nInvalid URI\n");
		pthread_mutex_unlock(&mutex_print);
		return false;
	}
	strcpy(copy_start_line, start_line);

	if (strstr(copy_start_line, "\b") != NULL) {
		strcpy(copy_start_line, start_line);
		pthread_mutex_lock(&mutex_print);
		printf("\nInvalid URI\n");
		pthread_mutex_unlock(&mutex_print);
		return false;
	}
	strcpy(copy_start_line, start_line);

	if (strstr(copy_start_line, "\f") != NULL) {
		strcpy(copy_start_line, start_line);
		pthread_mutex_lock(&mutex_print);
		printf("\nInvalid URI\n");
		pthread_mutex_unlock(&mutex_print);
		return false;
	}
	strcpy(copy_start_line, start_line);

	if (strstr(copy_start_line, "\r") != NULL) {
		strcpy(copy_start_line, start_line);
		pthread_mutex_lock(&mutex_print);
		printf("\nInvalid URI\n");
		pthread_mutex_unlock(&mutex_print);
		return false;
	}
	strcpy(copy_start_line, start_line);

	if (strstr(copy_start_line, "\t") != NULL) {
		strcpy(copy_start_line, start_line);
		pthread_mutex_lock(&mutex_print);
		printf("\nInvalid URI\n");
		pthread_mutex_unlock(&mutex_print);
		return false;
	}
	strcpy(copy_start_line, start_line);

	if (strstr(copy_start_line, "\v") != NULL) {
		strcpy(copy_start_line, start_line);
		pthread_mutex_lock(&mutex_print);
		printf("\nInvalid URI\n");
		pthread_mutex_unlock(&mutex_print);
		return false;
	}
	strcpy(copy_start_line, start_line);

	if (strstr(copy_start_line, "<") != NULL) {
		pthread_mutex_lock(&mutex_print);
		printf("\nInvalid URI\n");
		pthread_mutex_unlock(&mutex_print);
		return false;
	}
	strcpy(copy_start_line, start_line);

	if (strstr(copy_start_line, ">") != NULL) {
		strcpy(copy_start_line, start_line);
		pthread_mutex_lock(&mutex_print);
		printf("\nInvalid URI\n");
		pthread_mutex_unlock(&mutex_print);
		return false;
	}
	strcpy(copy_start_line, start_line);

	// Auxiliary variable
	char *fragment;
	bool response;

	fragment = strtok(copy_start_line, " ");

	if (strcmp(fragment, "PING") == 0 || strcmp(fragment, "BWIDTH") == 0
		|| strcmp(fragment, "CANCEL") == 0) {
			response = false;
	} else if (strcmp(fragment, "Q4S/1.0") == 0) {
		response = true;
	} else {
		strcpy(copy_start_line, start_line);
		pthread_mutex_lock(&mutex_print);
		printf("\nUnknown method of received message\n");
		pthread_mutex_unlock(&mutex_print);
		return false;
	}

	if (!response) {
		fragment = strtok(NULL, " ");
		if (strstr(fragment, "q4s://") != NULL) {
			fragment = strtok(NULL, "/");
			if (strstr(fragment, "Q4S") != NULL) {
				fragment = strtok(NULL, "\n");
				if (strcmp(fragment, "1.0") == 0) {
					strcpy(copy_start_line, start_line);
				} else {
					strcpy(copy_start_line, start_line);
					pthread_mutex_lock(&mutex_print);
					printf("\nServer is using a different version of QoS\n");
					pthread_mutex_unlock(&mutex_print);
					return false;
				}
			} else {
				strcpy(copy_start_line, start_line);
				pthread_mutex_lock(&mutex_print);
				printf("\nInvalid format of start line\n");
				pthread_mutex_unlock(&mutex_print);
				return false;
			}
		} else {
			strcpy(copy_start_line, start_line);
			pthread_mutex_lock(&mutex_print);
			printf("\nInvalid format of start line\n");
			pthread_mutex_unlock(&mutex_print);
			return false;
		}
	}

	// If there is a Signature parameter in the header
	if (fragment = strstr(copy_header, "Signature")){
		fragment = fragment + 11; // moves to the beginning of the value
		char *signature;
		signature = strtok(fragment, "\n"); // stores value

		// Creates a MD5 hash for the sdp
		char hash[33];
	  MD5mod(copy_body, hash);
		if (strcmp(signature, hash) != 0) {
			pthread_mutex_lock(&mutex_print);
			printf("\nMD5 hash of the sdp doesn't coincide\n");
			printf("\nSignature: %s\n", signature);
			printf("\nHash: %s\n", hash);
			printf("\nIntegrity of message has been violated\n");
			pthread_mutex_unlock(&mutex_print);
			return false;
		}
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_body, body);  // restores copy of header
	}
	strcpy(copy_header, header); // restores copy of header

	if (strlen(copy_body) > 0) {
		if (strstr(copy_start_line, "BWIDTH") == NULL) {
			// Checks if the body is in sdp format
			if (fragment = strstr(copy_body, "a=")) {
				memset(fragment, '\0', strlen(fragment));
				strcpy(copy_body, body); // restores copy of header
			} else {
				strcpy(copy_body, body); // restores copy of header
				pthread_mutex_lock(&mutex_print);
				printf("\nUnrecognized format of body\n");
				pthread_mutex_unlock(&mutex_print);
				return false;
			}
		}

		strcpy(copy_start_line, start_line);  // restores copy of start line

		// If there is a QoS level parameter in the body
		if (fragment = strstr(copy_body, "a=qos-level:")) {
	    fragment = fragment + 12;  // moves to the beginning of the value
			char *qos_level_up;
			qos_level_up = strtok(fragment, "/");  // stores string value
			if (atoi(qos_level_up) > 9 || atoi(qos_level_up) < 0) {
				pthread_mutex_lock(&mutex_print);
				printf("\nInvalid upstream QoS level\n");
				pthread_mutex_unlock(&mutex_print);
				return false;
			}
			char *qos_level_down;
			qos_level_down = strtok(NULL, "\n");  // stores string value
			if (atoi(qos_level_down) > 9 || atoi(qos_level_down) < 0) {
				pthread_mutex_lock(&mutex_print);
				printf("\nInvalid downstream QoS level\n");
				pthread_mutex_unlock(&mutex_print);
				return false;
			}
			memset(fragment, '\0', strlen(fragment));
			strcpy(copy_body, body);  // restores copy of body
		}
	}

	if ((&q4s_session)->session_id >= 0) {
		// If there is a Session ID parameter in the header
		if (fragment = strstr(copy_header, "Session-Id")) {
			fragment = fragment + 12;  // moves to the beginning of the value
			char *string_id;
			string_id = strtok(fragment, "\n");  // stores string value
			if ((&q4s_session)->session_id != atoi(string_id)) {
				pthread_mutex_lock(&mutex_print);
				printf("\nSession ID of the message doesn't coincide\n");
				printf("\nID received: %d\n", atoi(string_id));
				printf("\nActual ID: %d\n", (&q4s_session)->session_id);
				pthread_mutex_unlock(&mutex_print);
				memset(fragment, '\0', strlen(fragment));
				strcpy(copy_header, header);  // restore copy of header
				return false;
			} else {
				memset(fragment, '\0', strlen(fragment));
				strcpy(copy_header, header);  // restore copy of header
			}
		} else if (fragment = strstr(copy_body, "o=")) {  // if Session ID is in the body
			fragment = strstr(fragment, " ");  // moves to the beginning of the value
			char *string_id;
			string_id = strtok(fragment, " ");  // stores string value
			if ((&q4s_session)->session_id != atoi(string_id)) {
				pthread_mutex_lock(&mutex_print);
				printf("\nSession ID of the message doesn't coincide\n");
				pthread_mutex_unlock(&mutex_print);
				memset(fragment, '\0', strlen(fragment));
				strcpy(copy_header, header);  // restore copy of header
				strcpy(copy_body, body);  // restores copy of body
				return false;
			} else {
				memset(fragment, '\0', strlen(fragment));
				strcpy(copy_header, header); // restores copy of header
				strcpy(copy_body, body);  // restores copy of body
			}
		} else {
			strcpy(copy_header, header); // restores copy of header
			strcpy(copy_body, body);  // restores copy of body
			pthread_mutex_lock(&mutex_print);
			printf("\nMissing Session-Id header field in the message received\n");
			pthread_mutex_unlock(&mutex_print);
			return false;
		}
	}

	if (strcmp(copy_start_line, "PING q4s://www.example.com Q4S/1.0") == 0
	|| strcmp(copy_start_line, "BWIDTH q4s://www.example.com Q4S/1.0") == 0) {
		// If there is a Sequence Number parameter in the header
		if (fragment = strstr(copy_header, "Sequence-Number")) {
			memset(fragment, '\0', strlen(fragment));
			strcpy(copy_start_line, start_line);  // restores copy of start line
			strcpy(copy_header, header); // restores copy of header
		} else {
			strcpy(copy_start_line, start_line);  // restores copy of start line
			strcpy(copy_header, header); // restores copy of header
			pthread_mutex_lock(&mutex_print);
			printf("\nMissing Sequence-Number header field in the message received\n");
			pthread_mutex_unlock(&mutex_print);
			return false;
		}
	}

	// Stores Q4S message
	strcpy((q4s_message)->start_line, start_line);
	strcpy((q4s_message)->header, header);
	strcpy((q4s_message)->body, body);

	return true;
}


// Q4S PARAMETER STORAGE FUNCTIONS

// Storage of Q4S parameters from a Q4S message
void store_parameters(type_q4s_session *q4s_session, type_q4s_message *q4s_message) {
	// Extracts start line
	char start_line[200];
	memset(start_line, '\0', strlen(start_line));
	strcpy(start_line, (q4s_message)->start_line);
	// Creates a copy of header to manipulate it
	char copy_start_line[200];
	memset(copy_start_line, '\0', strlen(copy_start_line));
  strcpy(copy_start_line, start_line);
	// Extracts header
	char header[500];
	memset(header, '\0', strlen(header));
	strcpy(header, (q4s_message)->header);
	// Creates a copy of header to manipulate it
	char copy_header[500];
	memset(copy_header, '\0', strlen(copy_header));
  strcpy(copy_header, header);
	// Extracts body
	char body[5000];
	memset(body, '\0', strlen(body));
	strcpy(body, (q4s_message)->body);
	// Creates a copy of body to manipulate it
	char copy_body[5000];
	memset(copy_body, '\0', strlen(copy_body));
  strcpy(copy_body, body);

  // Auxiliary variable
	char *fragment;

	// If session id is not known
	if ((q4s_session)->session_id < 0) {
		// If there is a Session ID parameter in the header
		if (fragment = strstr(copy_header, "Session-Id")) {
			fragment = fragment + 12;  // moves to the beginning of the value
			char *string_id;
			string_id = strtok(fragment, "\n");  // stores string value
			(q4s_session)->session_id = atoi(string_id);  // converts into int and stores
			memset(fragment, '\0', strlen(fragment));
			strcpy(copy_header, header);  // restore copy of header
		} else if (fragment = strstr(copy_body, "o=")) {  // if Session ID is in the body
			fragment = strstr(fragment, " ");  // moves to the beginning of the value
			char *string_id;
			string_id = strtok(fragment, " ");  // stores string value
			(q4s_session)->session_id = atoi(string_id);  // converts into int and stores
			memset(fragment, '\0', strlen(fragment));
			strcpy(copy_body, body);  // restores copy of header
		}
	}

  // If there is a Expires parameter in the header
	if (fragment = strstr(copy_header, "Expires")){
		fragment = fragment + 9;  // moves to the beginning of the value
		char *s_expires;
		s_expires = strtok(fragment, "\n");  // stores string value
		(q4s_session)->expiration_time = atoi(s_expires);  // converts into int and stores
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_header, header);  // restores copy of header
	}

  if (strcmp(copy_start_line, "PING q4s://www.example.com Q4S/1.0") == 0) {
		// If there is a Date parameter in the header
		if (fragment = strstr(copy_header, "Timestamp")){
			fragment = fragment + 11;  // moves to the beginning of the value
			char *s_date;
			s_date = strtok(fragment, "\n");  // stores string value
			strcpy((q4s_session)->server_timestamp, s_date);  // stores
			memset(fragment, '\0', strlen(fragment));
			strcpy(copy_header, header);  // restores copy of header
		}
		// If there is a Sequence Number parameter in the header
		if (fragment = strstr(copy_header, "Sequence-Number")){
			fragment = fragment + 17;  // moves to the beginning of the value
			char *s_seq_num;
			s_seq_num = strtok(fragment, "\n");  // stores string value
			(q4s_session)->seq_num_server = atoi(s_seq_num);  // converts into int and stores
			memset(fragment, '\0', strlen(fragment));
			strcpy(copy_header, header);  // restores copy of header
		}
		// If there is a Measurements parameter in the header
		if (fragment = strstr(copy_header, "Measurements")){
			fragment = fragment + 14;  // moves to the beginning of the value
			char *s_latency;
			strtok(fragment, "=");
			s_latency = strtok(NULL, ",");  // stores string value
			if (strcmp(s_latency," ") != 0) {
				(q4s_session)->latency_measure_server = atoi(s_latency);  // converts into int and stores
			}

			char *s_jitter;
			strtok(NULL, "=");
			s_jitter = strtok(NULL, ",");  // stores string value
			if (strcmp(s_jitter," ") != 0) {
				(q4s_session)->jitter_measure_server = atoi(s_jitter);  // converts into int and stores
			}

			char *s_pl;
			strtok(NULL, "=");
			s_pl = strtok(NULL, ",");  // stores string value
      if (strcmp(s_pl," ") != 0) {
				(q4s_session)->packetloss_measure_server = atof(s_pl);  // converts into int and stores
			}

			strtok(NULL, "=");
			strtok(NULL, "\n");
			memset(fragment, '\0', strlen(fragment));
			strcpy(copy_header, header);  // restores copy of header
		}
		strcpy(copy_start_line, start_line);  // restores copy of start line
	}

	if (strcmp(copy_start_line, "BWIDTH q4s://www.example.com Q4S/1.0") == 0) {
		// If there is a Sequence Number parameter in the header
		if (fragment = strstr(copy_header, "Sequence-Number")){
			fragment = fragment + 17;  // moves to the beginning of the value
			char *s_seq_num;
			s_seq_num = strtok(fragment, "\n");  // stores string value
			(q4s_session)->seq_num_server = atoi(s_seq_num);  // converts into int and stores
			memset(fragment, '\0', strlen(fragment));
			strcpy(copy_header, header);  // restores copy of header
		}
		// If there is a Measurements parameter in the header
		if (fragment = strstr(copy_header, "Measurements")){
			fragment = fragment + 14;  // moves to the beginning of the value
			strtok(fragment, "=");
			strtok(NULL, ",");
			strtok(NULL, "=");
			strtok(NULL, ",");

			char *s_pl;
			strtok(NULL, "=");
			s_pl = strtok(NULL, ",");  // stores string value
      if (strcmp(s_pl," ") != 0) {
				(q4s_session)->packetloss_measure_server = atof(s_pl);  // converts into int and stores
			}

			char *s_bw;
			strtok(NULL, "=");
			s_bw = strtok(NULL, "\n");  // stores string value
      if (strcmp(s_bw," ") != 0) {
				(q4s_session)->bw_measure_server = atoi(s_bw);  // converts into int and stores
			}
			memset(fragment, '\0', strlen(fragment));
			strcpy(copy_header, header);  // restores copy of header
		}
		strcpy(copy_start_line, start_line);  // restores copy of start line
	}

	if (strcmp(copy_start_line, "Q4S/1.0 200 OK") == 0) {
		// If there is a Sequence Number parameter in the header
		if (fragment = strstr(copy_header, "Sequence-Number")){
			fragment = fragment + 17;  // moves to the beginning of the value
			char *s_seq_num;
			s_seq_num = strtok(fragment, "\n");  // stores string value
			(q4s_session)->seq_num_confirmed = atoi(s_seq_num);  // converts into int and stores
			memset(fragment, '\0', strlen(fragment));
			strcpy(copy_header, header);  // restores copy of header
		}
		// If there is a Stage parameter in the header
		if (fragment = strstr(copy_header, "Stage")){
			fragment = fragment + 7;  // moves to the beginning of the value
			char *s_stage;
			s_stage = strtok(fragment, "\n");  // stores string value
			(q4s_session)->stage = atoi(s_stage);  // converts into int and stores
			memset(fragment, '\0', strlen(fragment));
			strcpy(copy_header, header);  // restores copy of header
		}
	}
  // If there is a QoS level parameter in the body
	if (fragment = strstr(copy_body, "a=qos-level:")) {
    fragment = fragment + 12;  // moves to the beginning of the value
		char *qos_level_up;
		qos_level_up = strtok(fragment, "/");  // stores string value
		(q4s_session)->qos_level[0] = atoi(qos_level_up);  // converts into int and stores
		char *qos_level_down;
		qos_level_down = strtok(NULL, "\n");  // stores string value
		(q4s_session)->qos_level[1] = atoi(qos_level_down);  // converts into int and stores
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_body, body);  // restores copy of body
	}

  // If there is an alert pause parameter in the body
	if (fragment = strstr(copy_body, "a=alert-pause:")){
		fragment = fragment + 14;  // moves to the beginning of the value
		char *alert_pause;
		alert_pause = strtok(fragment, "\n");  // stores string value
		(q4s_session)->alert_pause = atoi(alert_pause);  // converts into int and stores
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_body, body);  // restores copy of body
	}

	// If there is a measurement procedure parameter in the body
	if (fragment = strstr(copy_body, "a=measurement:procedure default(")) {
    fragment = fragment + 32;  // moves to the beginning of the value
		char *ping_interval_negotiation_client;
		ping_interval_negotiation_client = strtok(fragment, "/");  // stores string value
		(q4s_session)->ping_clk_negotiation_client = atoi(ping_interval_negotiation_client);  // converts into int and stores
		char *ping_interval_negotiation_server;
		ping_interval_negotiation_server = strtok(NULL, ",");  // stores string value
		(q4s_session)->ping_clk_negotiation_server = atoi(ping_interval_negotiation_server);  // converts into int and stores
		char *ping_interval_continuity;
		ping_interval_continuity = strtok(NULL, "/");  // stores string value
		(q4s_session)->ping_clk_continuity = atoi(ping_interval_continuity);  // converts into int and stores
		char *bwidth_interval;
		strtok(NULL, ",");
		bwidth_interval = strtok(NULL, ",");  // stores string value
		(q4s_session)->bwidth_clk = atoi(bwidth_interval);  // converts into int and stores
		char *window_size_lj;
		window_size_lj = strtok(NULL, "/");  // stores string value
		(q4s_session)->window_size_latency_jitter = atoi(window_size_lj);  // converts into int and stores
		char *window_size_pl;
		strtok(NULL, ",");
		window_size_pl = strtok(NULL, "/");  // stores string value
		(q4s_session)->window_size_packetloss = atoi(window_size_pl);  // converts into int and stores
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_body, body);  // restores copy of body
		num_samples_succeed = 4*max((q4s_session)->window_size_latency_jitter, (q4s_session)->window_size_packetloss);
	}

  // If there is an latency parameter in the body
	if (fragment = strstr(copy_body, "a=latency:")){
		fragment = fragment + 10;  // moves to the beginning of the value
		char *latency;
		latency = strtok(fragment, "\n");  // stores string value
		(q4s_session)->latency_th = atoi(latency);  // converts into int and stores
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_body, body);  // restores copy of body
	}

  // If there is an jitter parameter in the body
	if (fragment = strstr(copy_body, "a=jitter:")) {
    fragment = fragment + 9;  // moves to the beginning of the value
		char *jitter_up;
		jitter_up = strtok(fragment, "/");  // stores string value
		(q4s_session)->jitter_th[0] = atoi(jitter_up);  // converts into int and stores
		char *jitter_down;
		jitter_down = strtok(NULL, "\n");  // stores string value
		(q4s_session)->jitter_th[1] = atoi(jitter_down);  // converts into int and stores
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_body, body);  // restores copy of body
	}

  // If there is an bandwidth parameter in the body
	if (fragment = strstr(copy_body, "a=bandwidth:")) {
    fragment = fragment + 12;  // moves to the beginning of the value
		char *bw_up;
		bw_up = strtok(fragment, "/");  // stores string value
		(q4s_session)->bw_th[0] = atoi(bw_up);  // converts into int and stores
		char *bw_down;
		bw_down = strtok(NULL, "\n");  // stores string value
		(q4s_session)->bw_th[1] = atoi(bw_down);  // converts into int and stores
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_body, body);  // restores copy of body

		if ((q4s_session)->bw_th[0] > 0) {
	    int target_bwidth = (q4s_session)->bw_th[0]; // kbps
	    int message_size = MESSAGE_BWIDTH_SIZE * 8; // bits
			// Messages per ms (fractional value)
	    float messages_fract_per_ms = ((float) target_bwidth / (float) message_size);
			// Messages per ms (integer value)
	    int messages_int_per_ms = floor(messages_fract_per_ms);
			// Messages remaining per second
	    int messages_per_s[10];
			memset(messages_per_s, 0, sizeof(messages_per_s));
	    messages_per_s[0] = (int) ceil((messages_fract_per_ms - (float) messages_int_per_ms) * 1000);
	    int ms_per_message[11];
			memset(ms_per_message, 0, sizeof(ms_per_message));
			if (messages_int_per_ms > 0) {
				 ms_per_message[0] = 1;
			} else {
				ms_per_message[0] = 0;
			}

	    int divisor;

	    for (int i = 0; i < 10; i++) {
		    divisor = 2;
		    while (((float)1000/divisor) > (float) messages_per_s[i]) {
			    divisor++;
		    }
		    ms_per_message[i+1] = divisor;
				if (messages_per_s[i] - (int) (1000/divisor) == 0) {
					break;
				} else if (messages_per_s[i] - (int) (1000/divisor) <= 1) {
					ms_per_message[i+1]--;
					break;
				} else {
					messages_per_s[i+1] = messages_per_s[i] - (int) (1000/divisor);
			  }
	    }
	    (q4s_session)->bwidth_messages_per_ms = messages_int_per_ms;
			memset((q4s_session)->ms_per_bwidth_message, 0, sizeof((q4s_session)->ms_per_bwidth_message));
			(q4s_session)->ms_per_bwidth_message[0] = ms_per_message[0];
	    for (int j = 1; j < sizeof(ms_per_message); j++) {
		    if (ms_per_message[j] > 0) {
			    (q4s_session)->ms_per_bwidth_message[j] = ms_per_message[j];
		    } else {
			    break;
		    }
	    }
		}
	}

  // If there is an packetloss parameter in the body
	if (fragment = strstr(copy_body, "a=packetloss:")) {
    fragment = fragment + 13;  // moves to the beginning of the value
		char *pl_up;
		pl_up = strtok(fragment, "/");  // stores string value
		(q4s_session)->packetloss_th[0] = atof(pl_up);  // converts into int and stores
		char *pl_down;
		pl_down = strtok(NULL, "\n");  // stores string value
		(q4s_session)->packetloss_th[1] = atof(pl_down);  // converts into int and stores
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_body, body);  // restores copy of body
	}

}


// FUNCTIONS FOR UPDATING AND STORING MEASURES

// Updates and stores latency measures
void update_latency(type_q4s_session *q4s_session, int latency_measured) {
	int window_size  = (q4s_session)->window_size_latency_jitter;
	int ordered_window[window_size];

	// Updates array of latency samples and position of next sample
	(q4s_session)->latency_samples[pos_latency] = latency_measured;
	if (pos_latency == MAXNUMSAMPLES-1) {
	 pos_latency = 0;
	} else {
	 pos_latency++;
	}

	// If the window is not being filled yet
	if (pos_latency < window_size && (q4s_session)->latency_samples[pos_latency] == 0) {
		// Adds sample to window and exits function
		(q4s_session)->latency_window[pos_latency-1] = latency_measured;
		return;
	// Else if the window is continuous (not splitted)
	} else if (pos_latency >= window_size){
		// If the window is filled for the first time
		if (pos_latency == window_size && (q4s_session)->latency_samples[pos_latency] == 0) {
			// Adds sample to window
		  (q4s_session)->latency_window[pos_latency-1] = latency_measured;
		// Else if the window was already filled
		} else {
			int aux = 0;
			// Updates window
			for (int i = pos_latency-window_size; i < pos_latency; i++) {
				(q4s_session)->latency_window[aux] = (q4s_session)->latency_samples[i];
				aux++;
			}
		}
	// Else if the window is splitted in two
	} else {
		int aux = 0;
		// Updates window
		for (int i = MAXNUMSAMPLES+pos_latency-window_size; i < MAXNUMSAMPLES; i++) {
			(q4s_session)->latency_window[aux] = (q4s_session)->latency_samples[i];
			aux++;
		}
		for (int i = 0; i < pos_latency; i++) {
			(q4s_session)->latency_window[aux] = (q4s_session)->latency_samples[i];
			aux++;
		}
	}

	// Orders window samples
	for (int i = 0; i < window_size; i++) {
		ordered_window[i] = (q4s_session)->latency_window[i];
	}
	sort_array(ordered_window, window_size);

  // Calculates mean value using statistical median formula
	if (window_size % 2 == 0) {
		int median_elem1 = window_size/2;
		int median_elem2 = window_size/2 + 1;
		(q4s_session)->latency_measure_client = (ordered_window[median_elem1-1]
		  + ordered_window[median_elem2-1]) / 2;
	} else {
		int median_elem = (window_size + 1) / 2;
		(q4s_session)->latency_measure_client = ordered_window[median_elem-1];
	}
}

// Updates and stores latency measures
void update_jitter(type_q4s_session *q4s_session, int elapsed_time) {
	int window_size  = (q4s_session)->window_size_latency_jitter;

	// Updates array of elapsed time samples and position of next sample
	(q4s_session)->elapsed_time_samples[pos_elapsed_time] = elapsed_time;
	if (pos_elapsed_time == MAXNUMSAMPLES-1) {
	 pos_elapsed_time = 0;
	} else {
	 pos_elapsed_time++;
	}

	// If the window is not being filled yet
	if (pos_elapsed_time < max(window_size, 2) && (q4s_session)->elapsed_time_samples[pos_elapsed_time] == 0) {
		// Adds sample to window and exits function
		(q4s_session)->elapsed_time_window[pos_elapsed_time-1] = elapsed_time;
		return;
	// Else if the window is continuous (not splitted)
	} else if (pos_elapsed_time >= window_size){
		// If the window is filled for the first time
		if (pos_elapsed_time == window_size && (q4s_session)->elapsed_time_samples[pos_elapsed_time] == 0) {
			// Adds sample to window
		  (q4s_session)->elapsed_time_window[pos_elapsed_time-1] = elapsed_time;
		// Else if the window was already filled
		} else {
			int aux = 0;
			// Updates window
			for (int i = pos_elapsed_time-window_size; i < pos_elapsed_time; i++) {
				(q4s_session)->elapsed_time_window[aux] = (q4s_session)->elapsed_time_samples[i];
				aux++;
			}
		}
	// Else if the window is splitted in two
	} else {
		int aux = 0;
		// Updates window
		for (int i = MAXNUMSAMPLES+pos_elapsed_time-window_size; i < MAXNUMSAMPLES; i++) {
			(q4s_session)->elapsed_time_window[aux] = (q4s_session)->elapsed_time_samples[i];
			aux++;
		}
		for (int i = 0; i < pos_elapsed_time; i++) {
			(q4s_session)->elapsed_time_window[aux] = (q4s_session)->elapsed_time_samples[i];
			aux++;
		}
	}

  // Calculates mean value using arithmetic mean formula
	int elapsed_time_mean = 0;
	for (int i = 0; i < window_size - 1; i++) {
		elapsed_time_mean = elapsed_time_mean + (q4s_session)->elapsed_time_window[i];
	}
	elapsed_time_mean = elapsed_time_mean / (window_size - 1);
	(q4s_session)->jitter_measure_client = abs(elapsed_time - elapsed_time_mean);
}

// Updates and stores packetloss measures
void update_packetloss(type_q4s_session *q4s_session, int lost_packets) {
	int window_size  = (q4s_session)->window_size_packetloss;

	// Losses are represented with a "1"
	// Packets received are represented with a "-1"

	// Updates array of packetloss samples and position of next sample
	// If there is no packet lost this time
	if (lost_packets == 0) {
		(q4s_session)->packetloss_samples[pos_packetloss] = -1;
		if (pos_packetloss >= MAXNUMSAMPLES - 1) {
			pos_packetloss = 0;
		} else {
			pos_packetloss++;
		}
	// If there are lost packets
	} else {
		// If array of samples is to be overcome
		if (pos_packetloss + lost_packets >= MAXNUMSAMPLES) {
			// Fills array until the end
			for (int i = pos_packetloss; i < MAXNUMSAMPLES; i++) {
				(q4s_session)->packetloss_samples[i] = 1;
			}
			// Restart filling the array
			for (int i = 0; i < pos_packetloss + lost_packets - MAXNUMSAMPLES; i++) {
				(q4s_session)->packetloss_samples[i] = 1;
			}
			// Last sample to fill is the received packet (-1)
			(q4s_session)->packetloss_samples[pos_packetloss + lost_packets - MAXNUMSAMPLES] = -1;
			// Updates position of next sample
			pos_packetloss = pos_packetloss + lost_packets - MAXNUMSAMPLES + 1;
		// If array of samples is not being overcome
		} else {
			// Adds new samples
			for (int i = 0; i < lost_packets; i++) {
				(q4s_session)->packetloss_samples[pos_packetloss] = 1;
				pos_packetloss++;
			}
			// Last sample to add is the received packet
			(q4s_session)->packetloss_samples[pos_packetloss] = -1;
			pos_packetloss++;
		}
	}

	// If the window is not going to be filled yet
	if (pos_packetloss < window_size && (q4s_session)->packetloss_samples[pos_packetloss] == 0) {
		// Updates to window and exits function
		for (int i = 0; i < pos_packetloss; i++) {
			(q4s_session)->packetloss_window[i] = (q4s_session)->packetloss_samples[i];
		}
		return;
	// Else if the window is continuous (not splitted)
	} else if (pos_packetloss >= window_size){
		// If the window is fulfilled for the first time
		if (pos_packetloss == window_size && (q4s_session)->packetloss_samples[pos_packetloss] == 0) {
			// Updates window
			for (int i = 0; i < pos_packetloss; i++) {
				(q4s_session)->packetloss_window[i] = (q4s_session)->packetloss_samples[i];
			}
		// Else if the window was already fulfilled
		} else {
			int aux = 0;
			// Updates window
			for (int i = pos_packetloss-window_size; i < pos_packetloss; i++) {
				(q4s_session)->packetloss_window[aux] = (q4s_session)->packetloss_samples[i];
				aux++;
			}
		}
	// Else if the window is splitted in two
	} else {
		int aux = 0;
		// Updates window
		for (int i = MAXNUMSAMPLES+pos_packetloss-window_size; i < MAXNUMSAMPLES; i++) {
			(q4s_session)->packetloss_window[aux] = (q4s_session)->packetloss_samples[i];
			aux++;
		}
		for (int i = 0; i < pos_packetloss; i++) {
			(q4s_session)->packetloss_window[aux] = (q4s_session)->packetloss_samples[i];
			aux++;
		}
	}

	// Stores number of samples of the window indicating a loss
	int num_losses = 0;
	for (int i = 0; i < window_size; i++) {
		if ((q4s_session)->packetloss_window[i] == 1) {
			num_losses++;
		}
	}

  // Calculates updated value of packetloss
  (q4s_session)->packetloss_measure_client = ((float) num_losses / (float) window_size);
}



// CONNECTION FUNCTIONS

// Connection establishment with Q4S server
int connect_to_server() {
	host = gethostbyname("localhost"); // server assignment
	if (host == NULL) {
		pthread_mutex_lock(&mutex_print);
		printf("Incorrect host\n");
		pthread_mutex_unlock(&mutex_print);
		return -1;
	}
	if ((socket_TCP =  socket(AF_INET, SOCK_STREAM, 0)) < 0) { // socket assignment
    pthread_mutex_lock(&mutex_print);
		printf("Error when assigning the TCP socket: %s\n", strerror(errno));
    pthread_mutex_unlock(&mutex_print);
		return -1;
	}

  // Configures client for the TCP connection
	client_TCP.sin_family = AF_INET; // protocol assignment
	client_TCP.sin_port = htons(CLIENT_PORT_TCP); // port assignment
  client_TCP.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP address assignment (automatic)
	memset(client_TCP.sin_zero, '\0', 8); // fills padding with 0s

	// Assigns port to the client's TCP socket
	if (bind(socket_TCP, (struct sockaddr*)&client_TCP, sizeof(client_TCP)) < 0) {
    pthread_mutex_lock(&mutex_print);
		printf("Error when associating port with TCP socket: %s\n", strerror(errno));
    pthread_mutex_unlock(&mutex_print);
		close(socket_TCP);
		return -1;
	}

	if ((socket_UDP =  socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) { // socket assignment
    pthread_mutex_lock(&mutex_print);
		printf("Error when assigning the UDP socket: %s\n", strerror(errno));
    pthread_mutex_unlock(&mutex_print);
		return -1;
	}

	// Configures client for the UDP socket
	client_UDP.sin_family = AF_INET; // protocol assignment
	client_UDP.sin_port = htons(CLIENT_PORT_UDP); // port assignment
  client_UDP.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP address assignment (automatic)
	memset(client_UDP.sin_zero, '\0', 8); // fills padding with 0s

	// Assigns port to the client's UDP socket
	if (bind(socket_UDP, (struct sockaddr*)&client_UDP, sizeof(client_UDP)) < 0) {
    pthread_mutex_lock(&mutex_print);
		printf("Error when associating port with UDP socket: %s\n", strerror(errno));
    pthread_mutex_unlock(&mutex_print);
		close(socket_TCP);
		return -1;
	}

	// Especifies parameters of the server (UDP socket)
  server_UDP.sin_family = AF_INET; // protocol assignment
	server_UDP.sin_port = htons(HOST_PORT_UDP); // port assignment
	server_UDP.sin_addr = *((struct in_addr *)host->h_addr); // copies host IP address
  memset(server_UDP.sin_zero, '\0', 8); // fills padding with 0s

  // Especifies parameters of the server (TCP socket)
  server_TCP.sin_family = AF_INET; // protocol assignment
	server_TCP.sin_port = htons(HOST_PORT_TCP); // port assignment
	server_TCP.sin_addr = *((struct in_addr *)host->h_addr); // copies host IP address
  memset(server_TCP.sin_zero, '\0', 8); // fills padding with 0s

	// Connects to the host (Q4S server)
	if (connect(socket_TCP,(struct sockaddr *)&server_TCP, sizeof(server_TCP)) < 0) {
    pthread_mutex_lock(&mutex_print);
		printf ("Error when connecting to the host: %s\n", strerror(errno));
    pthread_mutex_unlock(&mutex_print);
		close(socket_TCP);
		return -1;
	}
	pthread_mutex_lock(&mutex_print);
	printf("\nConnected to %s:%d\n", inet_ntoa(server_TCP.sin_addr), htons(server_TCP.sin_port));
  pthread_mutex_unlock(&mutex_print);

	return 1;
}

// Delivery of Q4S message to the server using TCP
void send_message_TCP (char prepared_message[MAXDATASIZE]) {
  pthread_mutex_lock(&mutex_buffer_TCP);
	memset(buffer_TCP, '\0', sizeof(buffer_TCP));
	// Copies the message into the buffer
	strncpy (buffer_TCP, prepared_message, MAXDATASIZE);
	pthread_mutex_unlock(&mutex_buffer_TCP);

	// Sends the message to the server using the TCP socket assigned
	if (send(socket_TCP, buffer_TCP, MAXDATASIZE, 0) < 0) {
		pthread_mutex_lock(&mutex_print);
		printf("Error when sending TCP data: %s\n", strerror(errno));
		pthread_mutex_unlock(&mutex_print);
		close(socket_TCP);
		exit(0);
		return;
	}

  pthread_mutex_lock(&mutex_buffer_TCP);
	memset(buffer_TCP, '\0', sizeof(buffer_TCP));
	pthread_mutex_unlock(&mutex_buffer_TCP);
}

// Delivery of Q4S message to the server using UDP
void send_message_UDP (char prepared_message[MAXDATASIZE]) {
	char copy_buffer_UDP[MAXDATASIZE];
	memset(copy_buffer_UDP, '\0', sizeof(copy_buffer_UDP));

  pthread_mutex_lock(&mutex_buffer_UDP);
	memset(buffer_UDP, '\0', sizeof(buffer_UDP));
	// Copies the message into the buffer
	strncpy (buffer_UDP, prepared_message, MAXDATASIZE);
	strncpy (copy_buffer_UDP, buffer_UDP, MAXDATASIZE);
	pthread_mutex_unlock(&mutex_buffer_UDP);

	char *start_line;
	start_line = strtok(copy_buffer_UDP, "\n"); // stores first line of message

	// If it is a Q4S PING, stores the current time
	if (strcmp(start_line, "PING q4s://www.example.com Q4S/1.0") == 0) {
    pthread_mutex_lock(&mutex_tm_latency);
		int result;
		if ((&tm_latency_start1)->seq_number == -1) {
			result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_start1)->tm));
			(&tm_latency_start1)->seq_number = (&q4s_session)->seq_num_client;
		} else if ((&tm_latency_start2)->seq_number == -1) {
			result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_start2)->tm));
			(&tm_latency_start2)->seq_number = (&q4s_session)->seq_num_client;
		} else if ((&tm_latency_start3)->seq_number == -1) {
			result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_start3)->tm));
			(&tm_latency_start3)->seq_number = (&q4s_session)->seq_num_client;
		} else if ((&tm_latency_start4)->seq_number == -1) {
			result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_start4)->tm));
			(&tm_latency_start4)->seq_number = (&q4s_session)->seq_num_client;
		} else {
			int min_seq_num = min(min((&tm_latency_start1)->seq_number, (&tm_latency_start2)->seq_number),
		    min((&tm_latency_start3)->seq_number, (&tm_latency_start4)->seq_number));
				if (min_seq_num == (&tm_latency_start1)->seq_number) {
					result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_start1)->tm));
					(&tm_latency_start1)->seq_number = (&q4s_session)->seq_num_client;
				} else if (min_seq_num == (&tm_latency_start2)->seq_number) {
					result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_start2)->tm));
					(&tm_latency_start2)->seq_number = (&q4s_session)->seq_num_client;
				} else if (min_seq_num == (&tm_latency_start3)->seq_number) {
					result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_start3)->tm));
					(&tm_latency_start3)->seq_number = (&q4s_session)->seq_num_client;
				} else if (min_seq_num == (&tm_latency_start4)->seq_number) {
					result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_start4)->tm));
					(&tm_latency_start4)->seq_number = (&q4s_session)->seq_num_client;
				}
		}
		pthread_mutex_unlock(&mutex_tm_latency);
		if (result < 0) {
	  	pthread_mutex_lock(&mutex_print);
			printf("Error in clock_gettime(): %s\n", strerror(errno));
	  	pthread_mutex_unlock(&mutex_print);
			close(socket_UDP);
	  	return;
		}
	}
	// Sends the message to the server using the UDP socket assigned
	if (sendto(socket_UDP, buffer_UDP, MAXDATASIZE, 0, (struct sockaddr *) &server_UDP, slen) < 0) {
		pthread_mutex_lock(&mutex_print);
		printf("Error when sending UDP data: %s (%d)\n", strerror(errno), errno);
		pthread_mutex_unlock(&mutex_print);
		close(socket_UDP);
		exit(0);
		return;
	}
	pthread_mutex_lock(&mutex_buffer_UDP);
	memset(buffer_UDP, '\0', sizeof(buffer_UDP));
	pthread_mutex_unlock(&mutex_buffer_UDP);

	memset(copy_buffer_UDP, '\0', sizeof(copy_buffer_UDP));
}


//---------------------------------------------------------
// THREADS
//---------------------------------------------------------

// RECEPTION FUNCTIONS

// Reception of Q4S messages from the server (TCP socket)
// Thread function that checks if any message has arrived
void *thread_receives_TCP() {
	// Copy of the buffer (to avoid buffer modification)
	char copy_buffer_TCP[MAXDATASIZE];
	memset(copy_buffer_TCP, '\0', sizeof(copy_buffer_TCP));
	char copy_buffer_TCP_2[MAXDATASIZE];
	memset(copy_buffer_TCP_2, '\0', sizeof(copy_buffer_TCP_2));
	while(1) {
		// If error occurs when receiving
	  if (recv(socket_TCP, buffer_TCP, MAXDATASIZE, MSG_WAITALL) < 0) {
			pthread_mutex_lock(&mutex_print);
		  printf("Error when receiving TCP data: %s\n", strerror(errno));
			pthread_mutex_unlock(&mutex_print);
		  break;
	  }
		pthread_mutex_lock(&mutex_buffer_TCP);
		strcpy(copy_buffer_TCP, buffer_TCP);
		strcpy(copy_buffer_TCP_2, buffer_TCP);
		memset(buffer_TCP, '\0', sizeof(buffer_TCP));
		pthread_mutex_unlock(&mutex_buffer_TCP);

		pthread_mutex_lock(&mutex_session);
		// Validates and stores the received message to be analized later
		bool stored = store_message(copy_buffer_TCP, &q4s_session.message_received);
		pthread_mutex_unlock(&mutex_session);

		if (!stored) {
			continue;
		}

		// Auxiliary variable to identify type of Q4S message
		char *start_line;
		start_line = strtok(copy_buffer_TCP_2, "\n"); // stores first line of message
		// If it is a Q4S 200 OK message
		if (strcmp(start_line, "Q4S/1.0 200 OK") == 0) {
			pthread_mutex_lock(&mutex_flags);
			flags |= FLAG_RECEIVE_OK;
			pthread_mutex_unlock(&mutex_flags);
		// If it is a Q4S CANCEL message
		} else if (strcmp(start_line, "CANCEL q4s://www.example.com Q4S/1.0") == 0) {

			pthread_mutex_lock(&mutex_print);
			printf("\nI have received a Q4S CANCEL!\n");
			pthread_mutex_unlock(&mutex_print);

			pthread_mutex_lock(&mutex_flags);
			flags |= FLAG_RECEIVE_CANCEL;
			pthread_mutex_unlock(&mutex_flags);
		} else if (strstr(start_line, "Q4S/1.0 4") != NULL || strstr(start_line, "Q4S/1.0 5") != NULL
	 		|| strstr(start_line, "Q4S/1.0 6") != NULL) {
				pthread_mutex_lock(&mutex_print);
				printf("\nI have received a failure message:\n%s\n", copy_buffer_TCP);
				pthread_mutex_unlock(&mutex_print);

				num_failures++;

				delay(1000);

				if (num_failures < 5) {
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_CANCEL;
					pthread_mutex_unlock(&mutex_flags);
				} else {
					pthread_mutex_lock(&mutex_print);
				  printf("\n5 or more messages of failure received in this session\n");
					pthread_mutex_unlock(&mutex_print);

					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_RECEIVE_CANCEL;
					pthread_mutex_unlock(&mutex_flags);
				}

		} else {
			pthread_mutex_lock(&mutex_print);
		  printf("\nI have received an unidentified message\n");
			pthread_mutex_unlock(&mutex_print);
	  }
	  memset(copy_buffer_TCP, '\0', sizeof(copy_buffer_TCP));
	  memset(copy_buffer_TCP_2, '\0', sizeof(copy_buffer_TCP_2));
		if (cancel_TCP_thread) {
			break;
		}
	}
}

// Reception of Q4S messages from the server (UDP socket)
// Thread function that checks if any message has arrived
void *thread_receives_UDP() {
	// Copy of the buffer (to avoid buffer modification)
	char copy_buffer_UDP[MAXDATASIZE];
	memset(copy_buffer_UDP, '\0', sizeof(copy_buffer_UDP));
	char copy_buffer_UDP_2[MAXDATASIZE];
	memset(copy_buffer_UDP_2, '\0', sizeof(copy_buffer_UDP_2));
	while(1) {
		// If error occurs when receiving
	  if (recvfrom(socket_UDP, buffer_UDP, MAXDATASIZE, MSG_WAITALL,
			(struct sockaddr *) &server_UDP, &slen) < 0) {
			pthread_mutex_lock(&mutex_print);
		  printf("Error when receiving UDP data: %s\n", strerror(errno));
			pthread_mutex_unlock(&mutex_print);
		  break;
	  }
		pthread_mutex_lock(&mutex_buffer_UDP);
		strcpy(copy_buffer_UDP, buffer_UDP);
		strcpy(copy_buffer_UDP_2, buffer_UDP);
		memset(buffer_UDP, '\0', sizeof(buffer_UDP));
		pthread_mutex_unlock(&mutex_buffer_UDP);

		pthread_mutex_lock(&mutex_session);
		// Validates and stores the received message to be analized later
		bool stored = store_message(copy_buffer_UDP, &q4s_session.message_received);
		pthread_mutex_unlock(&mutex_session);

		if (!stored) {
			continue;
		}

		// Auxiliary variable to identify type of Q4S message
		char *start_line;
		start_line = strtok(copy_buffer_UDP_2, "\n"); // stores first line of message
		// If it is a Q4S 200 OK message
		if (strcmp(start_line, "Q4S/1.0 200 OK") == 0) {
			char field_seq_num[20];
			strcpy(field_seq_num, "Sequence-Number: ");
			char header[500];
			pthread_mutex_lock(&mutex_session);
			strcpy(header, (&q4s_session.message_received)->header);
			pthread_mutex_unlock(&mutex_session);

			if (strstr(header, field_seq_num) != NULL) {
				pthread_mutex_lock(&mutex_tm_latency);
				int result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_end)->tm));
				if (result < 0) {
			    pthread_mutex_lock(&mutex_print);
					printf("Error in clock_gettime(): %s\n", strerror(errno));
			    pthread_mutex_unlock(&mutex_print);
					close(socket_UDP);
				}
				pthread_mutex_unlock(&mutex_tm_latency);

				pthread_mutex_lock(&mutex_session);
	      // Stores parameters of the message (included Sequence Number)
				store_parameters(&q4s_session, &(q4s_session.message_received));
				int seq_num = (&q4s_session)->seq_num_confirmed;
				pthread_mutex_unlock(&mutex_session);

        pthread_mutex_lock(&mutex_tm_latency);
				(&tm_latency_end)->seq_number = seq_num;
				pthread_mutex_unlock(&mutex_tm_latency);

				pthread_mutex_lock(&mutex_flags);
				flags |= FLAG_RECEIVE_OK;
				pthread_mutex_unlock(&mutex_flags);
			}
		// If it is a Q4S PING message
		} else if (strcmp(start_line, "PING q4s://www.example.com Q4S/1.0") == 0) {
			char field_seq_num[20];
			strcpy(field_seq_num, "Sequence-Number: ");
			char header[500];
			pthread_mutex_lock(&mutex_session);
			strcpy(header, (&q4s_session.message_received)->header);
			pthread_mutex_unlock(&mutex_session);

			if (strstr(header, field_seq_num) != NULL) {
				int seq_num_before;
				int seq_num_after;
				int num_losses;

				pthread_mutex_lock(&mutex_session);
				seq_num_before = (&q4s_session)->seq_num_server;
	      // Stores parameters of the message (included Sequence Number)
				store_parameters(&q4s_session, &(q4s_session.message_received));
				seq_num_after = (&q4s_session)->seq_num_server;
				num_losses = seq_num_after - (seq_num_before + 1);
				num_packet_lost = num_losses;
				pthread_mutex_unlock(&mutex_session);

				pthread_mutex_lock(&mutex_tm_jitter);
				if (num_losses > 0) {
					num_ping = 0;
				}
				num_ping++;
				int result = 0;
				if (num_ping % 2 != 0) {
					result = clock_gettime(CLOCK_REALTIME, &tm1_jitter);
				} else {
					result = clock_gettime(CLOCK_REALTIME, &tm2_jitter);
				}
				if (result < 0) {
					pthread_mutex_lock(&mutex_print);
					printf("Error in clock_gettime(): %s\n", strerror(errno));
					pthread_mutex_unlock(&mutex_print);
					close(socket_UDP);
				}
				pthread_mutex_unlock(&mutex_tm_jitter);

			  pthread_mutex_lock(&mutex_flags);
			  flags |= FLAG_RECEIVE_PING;
			  pthread_mutex_unlock(&mutex_flags);

			}
		// If it is a Q4S BWIDTH message
	  } else if (strcmp(start_line, "BWIDTH q4s://www.example.com Q4S/1.0") == 0) {
			char field_seq_num[20];
			strcpy(field_seq_num, "Sequence-Number: ");
			char header[500];
			pthread_mutex_lock(&mutex_session);
			strcpy(header, (&q4s_session.message_received)->header);
			pthread_mutex_unlock(&mutex_session);

			if (strstr(header, field_seq_num) != NULL) {
				int seq_num_before;
				int seq_num_after;
				int num_losses;

				pthread_mutex_lock(&mutex_session);
				if (num_bwidth_received > 0) {
					num_bwidth_received++;
				}
				seq_num_before = (&q4s_session)->seq_num_server;
	      // Stores parameters of the message (included Sequence Number)
				store_parameters(&q4s_session, &(q4s_session.message_received));
				seq_num_after = (&q4s_session)->seq_num_server;
				if (seq_num_after == 0 && !bwidth_reception_timeout_activated) {
					// Starts timer for bwdith reception
					cancel_timer_reception_bwidth = false;
					pthread_create(&timer_reception_bwidth, NULL, (void*)bwidth_reception_timeout, NULL);
					num_bwidth_received = 1;
				}
				num_losses = seq_num_after - (seq_num_before + 1);
				num_packet_lost = num_losses;
				pthread_mutex_unlock(&mutex_session);

				pthread_mutex_lock(&mutex_flags);
			  flags |= FLAG_RECEIVE_BWIDTH;
			  pthread_mutex_unlock(&mutex_flags);

			}
		}
	  memset(copy_buffer_UDP, '\0', sizeof(copy_buffer_UDP));
	  memset(copy_buffer_UDP_2, '\0', sizeof(copy_buffer_UDP_2));
		if (cancel_UDP_thread) {
			break;
		}
  }
}

// TIMER FUNCTIONS

// Activates a flag when ping timeout has occurred in Stage 0
void *ping_timeout_0() {
	int ping_clk;
	while(1) {
		pthread_mutex_lock(&mutex_session);
		ping_clk = q4s_session.ping_clk_negotiation_client;
		pthread_mutex_unlock(&mutex_session);

    delay(ping_clk);

		pthread_mutex_lock(&mutex_flags);
		flags |= FLAG_TEMP_PING_0;
		pthread_mutex_unlock(&mutex_flags);

		if (cancel_timer_ping_0) {
			break;
		}
	}
}

// Activates a flag when preventive timeout has passed
void *end_measure_timeout() {
	pthread_mutex_lock(&mutex_session);
	end_measure_timeout_activated = true;
	int stage = q4s_session.stage;
	pthread_mutex_unlock(&mutex_session);
	int counter;
	bool delivery_server_finished;

	if (stage == 0) {
		while(1) {
			int ping_clk_server;
			int seq_num_init;
			int seq_num_now;
			pthread_mutex_lock(&mutex_session);
			ping_clk_server = q4s_session.ping_clk_negotiation_server;
			seq_num_init = q4s_session.seq_num_server;
			delivery_server_finished = (((&q4s_session)->latency_th <= 0 ||
				(&q4s_session)->latency_measure_server <= (&q4s_session)->latency_th)
				&& ((&q4s_session)->jitter_th[1] <= 0 ||
				(&q4s_session)->jitter_measure_client <= (&q4s_session)->jitter_th[1])
				&& ((&q4s_session)->packetloss_th[1] <= 0 ||
				(&q4s_session)->packetloss_measure_client <= (&q4s_session)->packetloss_th[1]));
			pthread_mutex_unlock(&mutex_session);
			// counter * ping_clk_server is the timeout, restarted when a Q4S PING is received
			counter = 10;
			seq_num_now = seq_num_init;
			while (seq_num_now == seq_num_init) {
				delay(ping_clk_server);
				counter--;
				pthread_mutex_lock(&mutex_session);
				seq_num_now = q4s_session.seq_num_server;
				pthread_mutex_unlock(&mutex_session);
				if (counter <= 0) {
					pthread_mutex_unlock(&mutex_print);
					flags |= FLAG_FINISH_PING;
					pthread_mutex_unlock(&mutex_flags);
					break;
				}
			}
			if (counter <= 0) {
				end_measure_timeout_activated = false;
				break;
			}
			if (cancel_timer_end_measure) {
				break;
			}
		}
	} else if (stage == 1) {
			int bwidth_clk;
			int interval_wait;
			int num_packet_received;
			while(1) {
				pthread_mutex_lock(&mutex_session);
				bwidth_clk = max(q4s_session.bwidth_clk, 2000);
				num_packet_received = num_bwidth_received;
				delivery_server_finished = (((&q4s_session)->bw_th[1] <= 0 ||
					(&q4s_session)->bw_measure_client >= (&q4s_session)->bw_th[1])
					&& ((&q4s_session)->packetloss_th[1] <= 0 ||
					(&q4s_session)->packetloss_measure_client <= (&q4s_session)->packetloss_th[1]));
				pthread_mutex_unlock(&mutex_session);
				// bwidth_clk is the timeout, restarted when a Q4S BWIDTH is received
				counter = 10;
				interval_wait = bwidth_clk/counter;
				while (num_packet_received == 0 && delivery_server_finished) {
					delay(interval_wait);
					counter--;
					pthread_mutex_lock(&mutex_session);
					num_packet_received = num_bwidth_received;
					delivery_server_finished = (((&q4s_session)->bw_th[1] <= 0 ||
						(&q4s_session)->bw_measure_client >= (&q4s_session)->bw_th[1])
						&& ((&q4s_session)->packetloss_th[1] <= 0 ||
						(&q4s_session)->packetloss_measure_client <= (&q4s_session)->packetloss_th[1]));
					pthread_mutex_unlock(&mutex_session);
					if (counter <= 0) {
						pthread_mutex_unlock(&mutex_print);
						flags |= FLAG_FINISH_BWIDTH;
						pthread_mutex_unlock(&mutex_flags);
						break;
					}
				}
				if (counter <= 0) {
					end_measure_timeout_activated = false;
					break;
				}
				if (cancel_timer_end_measure) {
					break;
				}
			}
	}
}

// Activates a flag when ping timeout has occurred in Stage 2
void *ping_timeout_2() {
	int ping_clk;
	while(1) {
		pthread_mutex_lock(&mutex_session);
		ping_clk = q4s_session.ping_clk_continuity;
		pthread_mutex_unlock(&mutex_session);

    delay(ping_clk);

		pthread_mutex_lock(&mutex_flags);
		flags |= FLAG_TEMP_PING_2;
		pthread_mutex_unlock(&mutex_flags);

		if (cancel_timer_ping_2) {
			break;
		}
	}
}

// Manages timeout of alert pause
void *alert_pause_timeout() {
	int pause_interval = q4s_session.alert_pause;
	pthread_mutex_lock(&mutex_session);
	q4s_session.alert_pause_activated = true;
	pthread_mutex_unlock(&mutex_session);

  delay(pause_interval);

	pthread_mutex_lock(&mutex_session);
	q4s_session.alert_pause_activated = false;
	pthread_mutex_unlock(&mutex_session);

}

// Manages timeout of bandwidth reception
void *bwidth_reception_timeout() {

	pthread_mutex_lock(&mutex_session);
	bwidth_reception_timeout_activated = true;
	int bwidth_clk = q4s_session.bwidth_clk; // time established for BWIDTH measure
	pthread_mutex_unlock(&mutex_session);

	delay(bwidth_clk);

	pthread_mutex_lock(&mutex_flags);
	flags |= FLAG_MEASURE_BWIDTH;
	pthread_mutex_unlock(&mutex_flags);
}

// Sends a Q4S BWIDTH burst and activates a flag when finished
void *bwidth_delivery() {
  int bwidth_clk; // time established for BWIDTH delivery
	int num_messages_to_send; // total number of messages to send in the burst
	int num_messages_sent; // number of messages sent in this burst
	int messages_per_ms; // BWIDTH messages to send each 1 ms
	int ms_per_message[11]; // intervals of ms for sending BWIDTH messages
	memset(ms_per_message, 0, sizeof(ms_per_message));
	int ms_iterated; // theoric ms passed
	int result; // auxiliary variable for error detection in timespec
	int elapsed_time; // variable used to adjust elapsed time
	int us_to_subtract; // ms accumulated to substract
	int us_passed_total;
	int delay_time;  // time to delay

	// Initializes parameters needed
	ms_iterated = 0;
	delay_time = 1000;
	pthread_mutex_lock(&mutex_session);
	num_messages_to_send = ceil((float) ((&q4s_session)->bw_th[0] * (&q4s_session)->bwidth_clk) / (float) (MESSAGE_BWIDTH_SIZE * 8));
	num_messages_sent = 0;
	q4s_session.seq_num_client = 0;
	bwidth_clk = q4s_session.bwidth_clk;
	messages_per_ms = q4s_session.bwidth_messages_per_ms;
	ms_per_message[0] = q4s_session.ms_per_bwidth_message[0];
	for (int i = 1; i < sizeof(ms_per_message); i++) {
		if (q4s_session.ms_per_bwidth_message[i] > 0) {
			ms_per_message[i] = q4s_session.ms_per_bwidth_message[i];
		} else {
			break;
		}
	}
	pthread_mutex_unlock(&mutex_session);

	// Start of Q4S BWIDTH delivery
	while(1) {
		if (ms_iterated == 0) {
			result = clock_gettime(CLOCK_REALTIME, &tm1_adjust);
		  if (result < 0) {
				pthread_mutex_lock(&mutex_print);
		    printf("Error in clock_gettime(): %s\n", strerror(errno));
				pthread_mutex_unlock(&mutex_print);
		  }
			result = clock_gettime(CLOCK_REALTIME, &tm1_jitter);
		  if (result < 0) {
				pthread_mutex_lock(&mutex_print);
		    printf("Error in clock_gettime(): %s\n", strerror(errno));
				pthread_mutex_unlock(&mutex_print);
		  }
		}
		// Sends a number specified of 1kB BWIDTH messages per 1 ms
		int j = 0;
		while (j < messages_per_ms) {
			pthread_mutex_lock(&mutex_session);
			// Fills q4s_session.message_to_send with the Q4S BWIDTH parameters
			create_bwidth(&q4s_session.message_to_send);
			// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
			prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
			// Sends the prepared message
			send_message_UDP(q4s_session.prepared_message);
			(&q4s_session)->seq_num_client++;
			num_messages_sent++;
			pthread_mutex_unlock(&mutex_session);
			j++;
		}
		// Sends (when appropiate) 1 or more extra BWIDTH message(s) of 1kB
		for (int k = 1; k < sizeof(ms_per_message); k++) {
			if (ms_per_message[k] > 0 && ms_iterated % ms_per_message[k] == 0) {
				pthread_mutex_lock(&mutex_session);
				// Fills q4s_session.message_to_send with the Q4S BWIDTH parameters
				create_bwidth(&q4s_session.message_to_send);
				// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
				prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
				// Sends the prepared message
				send_message_UDP(q4s_session.prepared_message);
				(&q4s_session)->seq_num_client++;
				num_messages_sent++;
				pthread_mutex_unlock(&mutex_session);
			} else if (ms_per_message[k] == 0) {
				break;
			}
		}
		// When the interval bwidth_clk has finished, delivery is completed
		if (ms_iterated >= bwidth_clk || num_messages_sent >= num_messages_to_send) {
			result = clock_gettime(CLOCK_REALTIME, &tm2_jitter);
		  if (result < 0) {
				pthread_mutex_lock(&mutex_print);
		    printf("Error in clock_gettime(): %s\n", strerror(errno));
				pthread_mutex_unlock(&mutex_print);
		  }

			elapsed_time = ms_elapsed(tm1_jitter, tm2_jitter);

			delay(2000);

			pthread_mutex_lock(&mutex_flags);
			flags |= FLAG_BWIDTH_BURST_SENT;
			pthread_mutex_unlock(&mutex_flags);
			break;
		}

		udelay(delay_time);

		ms_iterated++;

		result = clock_gettime(CLOCK_REALTIME, &tm2_adjust);
	  if (result < 0) {
			pthread_mutex_lock(&mutex_print);
	    printf("Error in clock_gettime(): %s\n", strerror(errno));
			pthread_mutex_unlock(&mutex_print);
	  }

		elapsed_time = us_elapsed(tm1_adjust, tm2_adjust);

		result = clock_gettime(CLOCK_REALTIME, &tm1_adjust);
		if (result < 0) {
			pthread_mutex_lock(&mutex_print);
			printf("Error in clock_gettime(): %s\n", strerror(errno));
			pthread_mutex_unlock(&mutex_print);
		}
		us_passed_total += elapsed_time;

		if (elapsed_time - delay_time > 0) {
			us_to_subtract += elapsed_time - delay_time;
		}
		delay_time = 1000 - min(us_to_subtract, 1000);
		us_to_subtract -= 1000 - delay_time;
	}
}

// Function exploring the keyboard
// Thread function for keystrokes detection and interpretation
void *thread_explores_keyboard () {
	int pressed_key;
	while(1) {
		// Pauses program execution for 10 ms
		delay(10);
		// Checks if a key has been pressed
		if(kbhit()) {
			// Stores pressed key
			pressed_key = kbread();
			switch(pressed_key) {
				// If "b" (of "Q4S BEGIN") has been pressed, FLAG_CONNECT is activated
				case 'b':
				  // Lock to guarantee mutual exclusion
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_CONNECT;
					pthread_mutex_unlock(&mutex_flags);
					break;
				// If "c" (of "cancel") has been pressed, FLAG_CANCEL is activated
        case 'c':
          // Lock to guarantee mutual exclusion
          pthread_mutex_lock(&mutex_flags);
          flags |= FLAG_CANCEL;
          pthread_mutex_unlock(&mutex_flags);
          break;
        // If any other key has been pressed, nothing happens
				default:
					break;
			}
		}
	}
}


//--------------------------------------------------------
// CHECK FUNCTIONS OF STATE MACHINE
//--------------------------------------------------------

// Checks if client wants to connect server
int check_connect (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_CONNECT);
  pthread_mutex_unlock(&mutex_flags);
  return result;
}

// Checks if a client wants to send a Q4S BEGIN to the server
int check_begin (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_BEGIN);
  pthread_mutex_unlock(&mutex_flags);
  return result;
}

// Checks if a Q4S 200 OK message has been received from the server
int check_receive_ok (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
  pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_RECEIVE_OK);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if client wants to go to Stage 0
int check_go_to_0 (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_GO_TO_0);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if client wants to go to Stage 1
int check_go_to_1 (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_GO_TO_1);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if client wants to go to Stage 2
int check_go_to_2 (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_GO_TO_2);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if ping timeout has occurred in Stage 0
int check_temp_ping_0 (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_TEMP_PING_0);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if ping timeout has occurred in Stage 2
int check_temp_ping_2 (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_TEMP_PING_2);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if q4s client has received a Q4S PING from server
int check_receive_ping (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_RECEIVE_PING);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if ping measure is to be finished
int check_finish_ping (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_FINISH_PING);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if client has sent a Q4S BWIDTH burst
int check_bwidth_burst_sent (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_BWIDTH_BURST_SENT);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if q4s client has received a Q4S BWIDTH
int check_receive_bwidth (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_RECEIVE_BWIDTH);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if q4s client has to measure bwidth
int check_measure_bwidth (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_MEASURE_BWIDTH);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if bwidth measure is to be finished
int check_finish_bwidth (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_FINISH_BWIDTH);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if client wants to cancel Q4S session
int check_cancel (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_CANCEL);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if q4s client has received a Q4S CANCEL from server
int check_receive_cancel (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_RECEIVE_CANCEL);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

//-------------------------------------------------------------
// ACTION FUNCTIONS OF STATE MACHINE
//------------------------------------------------------------

// Prepares for Q4S session
void Setup (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	// Puts every FLAG to 0
	flags = 0;
	pthread_mutex_unlock(&mutex_flags);
	// Tries to connect to Q4S server
	if (connect_to_server() < 0) {
		pthread_mutex_lock(&mutex_print);
		printf("Error when connecting to server\n");
    pthread_mutex_unlock(&mutex_print);
		exit(0);
	} else {
		// Initialize auxiliary variables
		num_packet_lost = 0;
		num_ping = 0;
		num_failures = 0;
		// Initialize session variables
		pthread_mutex_lock(&mutex_session);
	  q4s_session.session_id = -1;
		q4s_session.stage = -1;
		q4s_session.seq_num_server = -1;
		q4s_session.seq_num_client = 0;
		q4s_session.qos_level[0] = -1;
		q4s_session.qos_level[1] = -1;
		q4s_session.alert_pause = -1;
		q4s_session.latency_th = -1;
		q4s_session.jitter_th[0] = -1;
		q4s_session.jitter_th[1] = -1;
		q4s_session.bw_th[0] = -1;
		q4s_session.bw_th[1] = -1;
		q4s_session.packetloss_th[0] = -1;
		q4s_session.packetloss_th[1] = -1;
		q4s_session.jitter_measure_server = -1;
		q4s_session.jitter_measure_client = -1;
		q4s_session.packetloss_measure_server = -1;
		q4s_session.packetloss_measure_client = -1;
		q4s_session.bw_measure_server = -1;
		q4s_session.bw_measure_client = -1;
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_print);
		printf("\nWhenever you want to send a Q4S CANCEL, press 'c'\n");
	  pthread_mutex_unlock(&mutex_print);

		// Throws a thread to check the arrival of Q4S messages using TCP
		cancel_TCP_thread = false;
		pthread_create(&receive_TCP_thread, NULL, (void*)thread_receives_TCP, NULL);

		pthread_mutex_lock(&mutex_flags);
		flags |= FLAG_BEGIN;
		pthread_mutex_unlock(&mutex_flags);
	}
}

// Creates and sends a Q4S BEGIN message to the server
void Begin (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_BEGIN;
  pthread_mutex_unlock(&mutex_flags);

	pthread_mutex_lock(&mutex_print);
	printf("\nCreating a Q4S BEGIN\n");
	pthread_mutex_unlock(&mutex_print);

	pthread_mutex_lock(&mutex_session);
	// Fills q4s_session.message_to_send with the Q4S BEGIN parameters
	create_begin(&q4s_session.message_to_send);
  // Converts q4s_session.message_to_send into a message with correct format (prepared_message)
	prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
	// Sends the prepared message
  send_message_TCP(q4s_session.prepared_message);
	pthread_mutex_unlock(&mutex_session);

	pthread_mutex_lock(&mutex_print);
	printf("\nI have sent a Q4S BEGIN!\n");
	pthread_mutex_unlock(&mutex_print);
}

// Stores parameters received in the first 200 OK message from server
void Store (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_RECEIVE_OK;
  pthread_mutex_unlock(&mutex_flags);

	// Stores parameters of message received
	pthread_mutex_lock(&mutex_session);
  store_parameters(&q4s_session, &(q4s_session.message_received));

	pthread_mutex_lock(&mutex_print);
	printf("\nQ4S session has been established\n");
	pthread_mutex_unlock(&mutex_print);

	// Throws a thread to check the arrival of Q4S messages using UDP
	cancel_UDP_thread = false;
	pthread_create(&receive_UDP_thread, NULL, (void*)thread_receives_UDP, NULL);

	// If there are latency or jitter thresholds established
	if ((&q4s_session)->latency_th > 0 || (&q4s_session)->jitter_th[0] > 0
	  || (&q4s_session)->jitter_th[1] > 0) {
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_print);
		printf("\nGoing to Stage 0 (measure of latency, jitter and packetloss)\n");
		pthread_mutex_unlock(&mutex_print);
		// Lock to guarantee mutual exclusion
		pthread_mutex_lock(&mutex_flags);
		flags |= FLAG_GO_TO_0;
		pthread_mutex_unlock(&mutex_flags);
	// If there is a bandwidth threshold established
  } else if ((&q4s_session)->bw_th[0] > 0 ||  (&q4s_session)->bw_th[1] > 0) {
	  pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_print);
		printf("\nGoing to Stage 1 (measure of bandwidth and packetloss)\n");
		pthread_mutex_unlock(&mutex_print);

		// Lock to guarantee mutual exclusion
		pthread_mutex_lock(&mutex_flags);
		flags |= FLAG_GO_TO_1;
		pthread_mutex_unlock(&mutex_flags);
		// If there is a bandwidth threshold established
	} else if ((&q4s_session)->packetloss_th[0] > 0 ||  (&q4s_session)->packetloss_th[1] > 0) {
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_print);
		printf("\nGoing to Stage 0 (measure of latency, jitter and packetloss)\n");
		pthread_mutex_unlock(&mutex_print);

		// Lock to guarantee mutual exclusion
		pthread_mutex_lock(&mutex_flags);
		flags |= FLAG_GO_TO_0;
		pthread_mutex_unlock(&mutex_flags);
	} else {
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_print);
		printf("\nThere are no thresholds established for QoS parameters\n");
		printf("Press 'c' to send a Q4S CANCEL to the server\n");
		pthread_mutex_unlock(&mutex_print);
	}
}

// Creates and sends a Q4S READY 0 message to the server
void Ready0 (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_GO_TO_0;
  pthread_mutex_unlock(&mutex_flags);

  pthread_mutex_lock(&mutex_session);
	q4s_session.stage = 0;
	// Fills q4s_session.message_to_send with the Q4S READY 0 parameters
	create_ready0(&q4s_session.message_to_send);
  // Converts q4s_session.message_to_send into a message with correct format (prepared_message)
	prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
	// Sends the prepared message
	send_message_TCP(q4s_session.prepared_message);
	pthread_mutex_unlock(&mutex_session);
}

// Creates and sends a Q4S READY 1 message to the server
void Ready1 (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_GO_TO_1;
  pthread_mutex_unlock(&mutex_flags);


	pthread_mutex_lock(&mutex_session);
	q4s_session.stage = 1;
  // Fills q4s_session.message_to_send with the Q4S READY 1 parameters
	create_ready1(&q4s_session.message_to_send);
	// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
	prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
	// Sends the prepared message
	send_message_TCP(q4s_session.prepared_message);
	pthread_mutex_unlock(&mutex_session);
}

// Creates and sends a Q4S READY 2 message to the server
void Ready2 (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_GO_TO_2;
  pthread_mutex_unlock(&mutex_flags);

	pthread_mutex_lock(&mutex_session);
	q4s_session.stage = 2;
  // Fills q4s_session.message_to_send with the Q4S READY 1 parameters
	create_ready2(&q4s_session.message_to_send);
	// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
	prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
	// Sends the prepared message
	send_message_TCP(q4s_session.prepared_message);
	pthread_mutex_unlock(&mutex_session);
}

// Stores parameters received in the 200 OK message from the server
// Starts the timer for ping delivery
// Sends a Q4S PING message
void Ping_Init (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_RECEIVE_OK;
  pthread_mutex_unlock(&mutex_flags);

	// Stores parameters of message received (if present)
  pthread_mutex_lock(&mutex_session);
	store_parameters(&q4s_session, &(q4s_session.message_received));
	pthread_mutex_unlock(&mutex_session);

	// Initialize auxiliary variables
	num_packet_lost = 0;
	num_ping = 0;
	num_packet_since_alert = 0;
	num_bwidth_received = 0;

  pthread_mutex_lock(&mutex_tm_latency);
	(&tm_latency_start1)->seq_number = -1;
	(&tm_latency_start2)->seq_number = -1;
	(&tm_latency_start3)->seq_number = -1;
	(&tm_latency_start4)->seq_number = -1;
	(&tm_latency_end)->seq_number = -1;
	pthread_mutex_unlock(&mutex_tm_latency);

	pthread_mutex_lock(&mutex_session);
	// Initialize session variables
	q4s_session.seq_num_server = -1;
	q4s_session.seq_num_client = 0;
	q4s_session.packetloss_measure_client = -1;
	q4s_session.packetloss_measure_server = -1;
	q4s_session.latency_measure_server = 0;
	q4s_session.latency_measure_client = 0;
	q4s_session.jitter_measure_server = -1;
	q4s_session.jitter_measure_client = -1;
	memset((&q4s_session)->latency_samples, 0, MAXNUMSAMPLES);
	memset((&q4s_session)->latency_window, 0, MAXNUMSAMPLES);
	pos_latency = 0;
	memset((&q4s_session)->elapsed_time_samples, 0, MAXNUMSAMPLES);
	memset((&q4s_session)->elapsed_time_window, 0, MAXNUMSAMPLES);
	pos_elapsed_time = 0;
	memset((&q4s_session)->packetloss_samples, 0, MAXNUMSAMPLES);
	memset((&q4s_session)->packetloss_window, 0, MAXNUMSAMPLES);
	pos_packetloss = 0;
	// Fills q4s_session.message_to_send with the Q4S PING parameters
	create_ping(&q4s_session.message_to_send);
	// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
	prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
	// Sends the prepared message
	send_message_UDP(q4s_session.prepared_message);
	num_packet_since_alert++;

	// Initialize ping period if necessary
	if (q4s_session.ping_clk_negotiation_client <= 0) {
		q4s_session.ping_clk_negotiation_client = 200;
	}
	// Initialize ping period if necessary
	if (q4s_session.ping_clk_continuity <= 0) {
		q4s_session.ping_clk_continuity = 200;
	}
	int stage = q4s_session.stage;
	pthread_mutex_unlock(&mutex_session);

  if (stage == 2) {
		pthread_mutex_lock(&mutex_session);
		printf("\nStage 2 has started: Q4S PING exchange while application execution\n");
		pthread_mutex_unlock(&mutex_session);
		// Starts timer for ping delivery in Stage 2
		cancel_timer_ping_2 = false;
		pthread_create(&timer_ping_2, NULL, (void*)ping_timeout_2, NULL);
	} else {
		pthread_mutex_lock(&mutex_session);
		printf("\nStage 0 has started: Q4S PING exchange\n");
		pthread_mutex_unlock(&mutex_session);
		// Starts timer for ping delivery in Stage 0
		cancel_timer_ping_0 = false;
		pthread_create(&timer_ping_0, NULL, (void*)ping_timeout_0, NULL);
	}
}

// Sends a Q4S PING message
void Ping (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_TEMP_PING_0;
	flags &= ~FLAG_TEMP_PING_2;
  pthread_mutex_unlock(&mutex_flags);

	pthread_mutex_lock(&mutex_session);
  (&q4s_session)->seq_num_client++;
  if ((&q4s_session)->seq_num_client >= MAXNUMSAMPLES - 1) {
		(&q4s_session)->seq_num_client = 0;
	}
	// Fills q4s_session.message_to_send with the Q4S PING parameters
	create_ping(&q4s_session.message_to_send);
	// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
	prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
	// Sends the prepared message
	send_message_UDP(q4s_session.prepared_message);
	num_packet_since_alert++;
	pthread_mutex_unlock(&mutex_session);
}

// Updates Q4S measures
void Update (fsm_t* fsm) {
	pthread_mutex_lock(&mutex_flags);
	if (flags & FLAG_RECEIVE_PING) {
		flags &= ~FLAG_RECEIVE_PING;
	  pthread_mutex_unlock(&mutex_flags);

		pthread_mutex_lock(&mutex_session);
		int num_losses = num_packet_lost;
		num_packet_lost = 0;
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_tm_jitter);
		pthread_mutex_lock(&mutex_session);
    if ((&q4s_session)->jitter_th[1] > 0) {
			pthread_mutex_unlock(&mutex_session);
			int elapsed_time;

			if (num_ping % 2 == 0 && num_ping > 0) {
				elapsed_time = ms_elapsed(tm1_jitter, tm2_jitter);

	      pthread_mutex_lock(&mutex_session);
				update_jitter(&q4s_session, elapsed_time);
				pthread_mutex_unlock(&mutex_session);

			} else if (num_ping % 2 != 0 && num_ping > 1) {
				elapsed_time = ms_elapsed(tm2_jitter, tm1_jitter);

				pthread_mutex_lock(&mutex_session);
				update_jitter(&q4s_session, elapsed_time);
				pthread_mutex_unlock(&mutex_session);
			}
		} else {
			pthread_mutex_unlock(&mutex_session);
		}
		pthread_mutex_unlock(&mutex_tm_jitter);

		pthread_mutex_lock(&mutex_session);
		if ((&q4s_session)->jitter_th[0] > 0 && (&q4s_session)->jitter_measure_server > (&q4s_session)->jitter_th[0]) {
			num_packet_since_alert = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("Upstream jitter exceeds the threshold\n");
				pthread_mutex_unlock(&mutex_print);
				// Starts timer for alert pause
				cancel_timer_alert = false;
				pthread_create(&timer_alert, NULL, (void*)alert_pause_timeout, NULL);
			}
		} else if ((&q4s_session)->jitter_th[1] > 0 && (&q4s_session)->jitter_measure_client > (&q4s_session)->jitter_th[1]) {
			num_packet_since_alert = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("Downstream jitter exceeds the threshold\n");
				pthread_mutex_unlock(&mutex_print);
				memset((&q4s_session)->elapsed_time_samples, 0, MAXNUMSAMPLES);
				memset((&q4s_session)->elapsed_time_window, 0, MAXNUMSAMPLES);
				pos_elapsed_time = 0;
				// Starts timer for alert pause
				cancel_timer_alert = false;
				pthread_create(&timer_alert, NULL, (void*)alert_pause_timeout, NULL);
			}
		}
		pthread_mutex_unlock(&mutex_session);

    pthread_mutex_lock(&mutex_session);
		if ((&q4s_session)->packetloss_th[1] > 0) {
			update_packetloss(&q4s_session, num_losses);
		}
		pthread_mutex_unlock(&mutex_session);

    pthread_mutex_lock(&mutex_session);
		if ((&q4s_session)->packetloss_th[0] > 0 && (&q4s_session)->packetloss_measure_server > (&q4s_session)->packetloss_th[0]) {
			num_packet_since_alert = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("Upstream packetloss exceeds the threshold\n");
				pthread_mutex_unlock(&mutex_print);
				// Starts timer for alert pause
				cancel_timer_alert = false;
				pthread_create(&timer_alert, NULL, (void*)alert_pause_timeout, NULL);
			}
		} else if ((&q4s_session)->packetloss_th[1] > 0 && (&q4s_session)->packetloss_measure_client > (&q4s_session)->packetloss_th[1]) {
			num_packet_since_alert = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("Downstream packetloss exceeds the threshold\n");
				pthread_mutex_unlock(&mutex_print);
				memset((&q4s_session)->packetloss_samples, 0, MAXNUMSAMPLES);
				memset((&q4s_session)->packetloss_window, 0, MAXNUMSAMPLES);
				pos_packetloss = 0;
				// Starts timer for alert pause
				cancel_timer_alert = false;
				pthread_create(&timer_alert, NULL, (void*)alert_pause_timeout, NULL);
			}
		}
		pthread_mutex_unlock(&mutex_session);


    pthread_mutex_lock(&mutex_session);
		// Fills q4s_session.message_to_send with the Q4S 200 parameters
		create_200(&q4s_session.message_to_send);
		// Converts q4s_session.message_to_send into a message with correct format (prepared_message
		prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
		// Sends the prepared message
		send_message_UDP(q4s_session.prepared_message);
		pthread_mutex_unlock(&mutex_session);
		return;
	} else if (flags & FLAG_RECEIVE_OK) {
		flags &= ~FLAG_RECEIVE_OK;
		pthread_mutex_unlock(&mutex_flags);

    pthread_mutex_lock(&mutex_session);
    if ((&q4s_session)->latency_th > 0) {
			pthread_mutex_unlock(&mutex_session);

			pthread_mutex_lock(&mutex_tm_latency);
			if ((&tm_latency_start1)->seq_number == (&tm_latency_end)->seq_number) {
				int rtt = ms_elapsed((&tm_latency_start1)->tm, (&tm_latency_end)->tm);

				pthread_mutex_lock(&mutex_session);
				update_latency(&q4s_session, rtt/2);
				pthread_mutex_unlock(&mutex_session);
			} else if ((&tm_latency_start2)->seq_number == (&tm_latency_end)->seq_number) {
				int rtt = ms_elapsed((&tm_latency_start2)->tm, (&tm_latency_end)->tm);

				pthread_mutex_lock(&mutex_session);
				update_latency(&q4s_session, rtt/2);
				pthread_mutex_unlock(&mutex_session);
			} else if ((&tm_latency_start3)->seq_number == (&tm_latency_end)->seq_number) {
				int rtt = ms_elapsed((&tm_latency_start3)->tm, (&tm_latency_end)->tm);

				pthread_mutex_lock(&mutex_session);
				update_latency(&q4s_session, rtt/2);
				pthread_mutex_unlock(&mutex_session);
			} else if ((&tm_latency_start4)->seq_number == (&tm_latency_end)->seq_number) {
				int rtt = ms_elapsed((&tm_latency_start4)->tm, (&tm_latency_end)->tm);

				pthread_mutex_lock(&mutex_session);
				update_latency(&q4s_session, rtt/2);
				pthread_mutex_unlock(&mutex_session);
			}
			pthread_mutex_unlock(&mutex_tm_latency);
		} else {
			  pthread_mutex_unlock(&mutex_session);
		}

		pthread_mutex_lock(&mutex_session);
		if ((&q4s_session)->latency_th > 0 && (&q4s_session)->latency_measure_server > (&q4s_session)->latency_th) {
			num_packet_since_alert = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("Latency measured by server exceeds the threshold\n");
				pthread_mutex_unlock(&mutex_print);
				// Starts timer for alert pause
				cancel_timer_alert = false;
				pthread_create(&timer_alert, NULL, (void*)alert_pause_timeout, NULL);
			}
		} else if ((&q4s_session)->latency_th > 0 && (&q4s_session)->latency_measure_client > (&q4s_session)->latency_th) {
			num_packet_since_alert = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("Latency measured by client exceeds the threshold\n");
				pthread_mutex_unlock(&mutex_print);
				memset((&q4s_session)->latency_samples, 0, MAXNUMSAMPLES);
				memset((&q4s_session)->latency_window, 0, MAXNUMSAMPLES);
				pos_latency = 0;
				// Starts timer for alert pause
				cancel_timer_alert = false;
				pthread_create(&timer_alert, NULL, (void*)alert_pause_timeout, NULL);
			}
		}
		pthread_mutex_unlock(&mutex_session);

	} else if (flags & FLAG_RECEIVE_BWIDTH) {
	  flags &= ~FLAG_RECEIVE_BWIDTH;
    pthread_mutex_unlock(&mutex_flags);

		pthread_mutex_lock(&mutex_session);
		int num_losses = num_packet_lost;
		num_packet_lost = 0;
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_session);
		if ((&q4s_session)->packetloss_th[1] > 0) {
			update_packetloss(&q4s_session, num_losses);
		}
		pthread_mutex_unlock(&mutex_session);
	} else if (flags & FLAG_MEASURE_BWIDTH) {
	  flags &= ~FLAG_MEASURE_BWIDTH;
		pthread_mutex_unlock(&mutex_flags);

		pthread_mutex_lock(&mutex_session);
		(&q4s_session)->bw_measure_client = (num_bwidth_received * (MESSAGE_BWIDTH_SIZE * 8) / (&q4s_session)->bwidth_clk);
		int bw_measure_client = (&q4s_session)->bw_measure_client;
		num_bwidth_received = 0;
		bwidth_reception_timeout_activated = false;
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_session);
		if ((&q4s_session)->bw_th[0] > 0 && (&q4s_session)->bw_measure_server >= 0
		  && (&q4s_session)->bw_measure_server < (&q4s_session)->bw_th[0]) {
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("Upstream bandwidth doesn't reach the threshold: %d [%d]\n", (&q4s_session)->bw_th[0], (&q4s_session)->bw_measure_server);
				pthread_mutex_unlock(&mutex_print);
				// Starts timer for alert pause
				cancel_timer_alert = false;
				pthread_create(&timer_alert, NULL, (void*)alert_pause_timeout, NULL);
			}
		} else if ((&q4s_session)->bw_th[1] > 0 && (&q4s_session)->bw_measure_client >= 0
		  && (&q4s_session)->bw_measure_client < (&q4s_session)->bw_th[1]) {
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("Downstream bandwidth doesn't reach the threshold: %d [%d]\n", (&q4s_session)->bw_th[1], (&q4s_session)->bw_measure_client);
				pthread_mutex_unlock(&mutex_print);
				// Starts timer for alert pause
				cancel_timer_alert = false;
				pthread_create(&timer_alert, NULL, (void*)alert_pause_timeout, NULL);
			}
		}

		if ((&q4s_session)->packetloss_th[0] > 0 && (&q4s_session)->packetloss_measure_server > (&q4s_session)->packetloss_th[0]) {
			num_packet_since_alert = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("Upstream packetloss exceeds the threshold\n");
				pthread_mutex_unlock(&mutex_print);
				// Starts timer for alert pause
				cancel_timer_alert = false;
				pthread_create(&timer_alert, NULL, (void*)alert_pause_timeout, NULL);
			}
		} else if ((&q4s_session)->packetloss_th[1] > 0 && (&q4s_session)->packetloss_measure_client > (&q4s_session)->packetloss_th[1]) {
			num_packet_since_alert = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("Downstream packetloss exceeds the threshold\n");
				pthread_mutex_unlock(&mutex_print);
				memset((&q4s_session)->packetloss_samples, 0, MAXNUMSAMPLES);
				memset((&q4s_session)->packetloss_window, 0, MAXNUMSAMPLES);
				pos_packetloss = 0;
				// Starts timer for alert pause
				cancel_timer_alert = false;
				pthread_create(&timer_alert, NULL, (void*)alert_pause_timeout, NULL);
			}
		}
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_session);
		// Fills q4s_session.message_to_send with the Q4S BWIDTH parameters
		create_bwidth(&q4s_session.message_to_send);
		// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
		prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
		// Sends the prepared message
		send_message_UDP(q4s_session.prepared_message);
		(&q4s_session)->seq_num_client++;
		pthread_mutex_unlock(&mutex_session);

	} else {
		pthread_mutex_unlock(&mutex_flags);
	}

  pthread_mutex_lock(&mutex_session);
	int stage = q4s_session.stage;
	pthread_mutex_unlock(&mutex_session);

	if (stage == 0) {
		pthread_mutex_lock(&mutex_session);
		bool delivery_finished = num_packet_since_alert >= num_samples_succeed;
		pthread_mutex_unlock(&mutex_session);

		if (delivery_finished && !end_measure_timeout_activated) {
			cancel_timer_ping_0 = true;
			pthread_mutex_lock(&mutex_print);
			printf("\nFinished ping delivery from client\n");
			printf("\nWaiting server to finish ping delivery\n");
			pthread_mutex_unlock(&mutex_print);
			// Starts timer for preventive wait at the end of measures
			cancel_timer_end_measure = false;
			pthread_create(&timer_end_measure, NULL, (void*)end_measure_timeout, NULL);
		}
	}
}

// Decides the next Stage to go
void Decide (fsm_t* fsm) {
	pthread_mutex_lock(&mutex_flags);
	if (flags & FLAG_FINISH_PING) {
		// Puts every FLAG to 0
		flags = 0;
		pthread_mutex_unlock(&mutex_flags);

    pthread_mutex_lock(&mutex_session);
		if ((&q4s_session)->bw_th[0] > 0 ||  (&q4s_session)->bw_th[1] > 0) {
			  pthread_mutex_unlock(&mutex_session);

				pthread_mutex_lock(&mutex_print);
				printf("\nGoing to Stage 1 (measure of bandwidth and packetloss)\n");
				pthread_mutex_unlock(&mutex_print);

				// Lock to guarantee mutual exclusion
				pthread_mutex_lock(&mutex_flags);
				flags |= FLAG_GO_TO_1;
				pthread_mutex_unlock(&mutex_flags);
	  } else {
			pthread_mutex_unlock(&mutex_session);

			pthread_mutex_lock(&mutex_print);
		  printf("\nThere are no thresholds established for bandwidth\n");
			printf("\nNegotiation phase has finished\n");
			printf("\nGoing to Stage 2 (measurements while application execution)\n");
			pthread_mutex_unlock(&mutex_print);

			pthread_mutex_lock(&mutex_flags);
			flags |= FLAG_GO_TO_2;
			pthread_mutex_unlock(&mutex_flags);
	  }
	} else if (flags & FLAG_FINISH_BWIDTH) {
		// Puts every FLAG to 0
		flags = 0;
		pthread_mutex_unlock(&mutex_flags);

		pthread_mutex_lock(&mutex_print);
		printf("\nNegotiation phase has finished\n");
		printf("\nGoing to Stage 2 (measurements while application execution)\n");
		pthread_mutex_unlock(&mutex_print);

		pthread_mutex_lock(&mutex_flags);
		flags |= FLAG_GO_TO_2;
		pthread_mutex_unlock(&mutex_flags);
	}
}

// Starts timer for BWIDTH delivery
void Bwidth_Init (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_RECEIVE_OK;
  pthread_mutex_unlock(&mutex_flags);

	// Stores parameters of message received (if present)
  pthread_mutex_lock(&mutex_session);
	store_parameters(&q4s_session, &(q4s_session.message_received));
	pthread_mutex_unlock(&mutex_session);

	// Initialize auxiliary variables
	num_packet_lost = 0;
	num_ping = 0;
	num_packet_since_alert = 0;
	num_bwidth_received = 0;

	pthread_mutex_lock(&mutex_session);
	// Initialize session variables
	q4s_session.seq_num_server = -1;
	q4s_session.seq_num_client = 0;
	q4s_session.packetloss_measure_server = -1;
	q4s_session.packetloss_measure_client = -1;
	q4s_session.bw_measure_server = -1;
	q4s_session.bw_measure_client = -1;
	memset((&q4s_session)->latency_samples, 0, MAXNUMSAMPLES);
	memset((&q4s_session)->latency_window, 0, MAXNUMSAMPLES);
	pos_latency = 0;
	memset((&q4s_session)->elapsed_time_samples, 0, MAXNUMSAMPLES);
	memset((&q4s_session)->elapsed_time_window, 0, MAXNUMSAMPLES);
	pos_elapsed_time = 0;
	memset((&q4s_session)->packetloss_samples, 0, MAXNUMSAMPLES);
	memset((&q4s_session)->packetloss_window, 0, MAXNUMSAMPLES);
	pos_packetloss = 0;

	// Initialize bwidth period if necessary
	if (q4s_session.bwidth_clk <= 0) {
		q4s_session.bwidth_clk = 1000;
	}
	pthread_mutex_unlock(&mutex_session);
	pthread_mutex_lock(&mutex_session);
	printf("\nStage 1 has started: Q4S BWIDTH exchange\n");
	pthread_mutex_unlock(&mutex_session);
	// Starts timer for bwidth delivery
	cancel_timer_delivery_bwidth = false;
	pthread_create(&timer_delivery_bwidth, NULL, (void*)bwidth_delivery, NULL);
}

// Sends a Q4S BWIDTH message
void Bwidth_Decide (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_BWIDTH_BURST_SENT;
  pthread_mutex_unlock(&mutex_flags);

	pthread_mutex_lock(&mutex_session);
	int stage = q4s_session.stage;
	bool delivery_finished = (((&q4s_session)->bw_th[0] <= 0 ||
		(&q4s_session)->bw_measure_server >= (&q4s_session)->bw_th[0])
		&& ((&q4s_session)->packetloss_th[0] <= 0 ||
		(&q4s_session)->packetloss_measure_server <= (&q4s_session)->packetloss_th[0]));
	pthread_mutex_unlock(&mutex_session);

	if (stage == 1 && delivery_finished && !end_measure_timeout_activated) {
		cancel_timer_delivery_bwidth = true;
		pthread_mutex_lock(&mutex_print);
		printf("\nFinished bwidth delivery from client\n");
		printf("\nWaiting server to finish bwidth delivery\n");
		pthread_mutex_unlock(&mutex_print);
		// Starts timer for preventive wait at the end of measures
		cancel_timer_end_measure = false;
		pthread_create(&timer_end_measure, NULL, (void*)end_measure_timeout, NULL);
	} else {
		// Starts timer for bwidth delivery
		cancel_timer_delivery_bwidth = false;
		pthread_create(&timer_delivery_bwidth, NULL, (void*)bwidth_delivery, NULL);
	}
}

// Creates and sends a Q4S CANCEL message to the server
void Cancel (fsm_t* fsm) {
  // Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_CANCEL;
  pthread_mutex_unlock(&mutex_flags);


  pthread_mutex_lock(&mutex_session);
	// Fills q4s_session.message_to_send with the Q4S CANCEL parameters
	create_cancel(&q4s_session.message_to_send);
	// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
	prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
	// Sends the prepared message
	send_message_TCP(q4s_session.prepared_message);
	pthread_mutex_unlock(&mutex_session);
	pthread_mutex_lock(&mutex_print);
	printf("\nI have sent a Q4S CANCEL!\n");
	pthread_mutex_unlock(&mutex_print);

	// Cancels timers
	cancel_timer_ping_0 = true;
	cancel_timer_ping_2 = true;
	cancel_timer_delivery_bwidth = true;
	cancel_timer_reception_bwidth = true;
}

// Exits Q4S session
void Exit (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	// Puts every FLAG to 0
	flags = 0;
	pthread_mutex_unlock(&mutex_flags);

	finished = true;

	pthread_mutex_lock(&mutex_print);
  printf("\nQ4S session finished\n");
	pthread_mutex_unlock(&mutex_print);
}

//------------------------------------------------------
// EXECUTION OF MAIN PROGRAM
//------------------------------------------------------

int main () {
	// System configuration
	system_setup();

	// State machine: list of transitions
	// {OriginState, CheckFunction, DestinationState, ActionFunction}
	fsm_trans_t q4s_table[] = {
		  { WAIT_CONNECT, check_connect, WAIT_START, Setup },
			{ WAIT_START, check_begin,  HANDSHAKE, Begin },
			{ HANDSHAKE, check_receive_ok,  HANDSHAKE, Store },
			{ HANDSHAKE, check_go_to_0,  STAGE_0, Ready0 },
			{ HANDSHAKE, check_go_to_1,  STAGE_1, Ready1 },
			{ HANDSHAKE, check_cancel, TERMINATION, Cancel },
			{ HANDSHAKE, check_receive_cancel, END, Exit },
			{ STAGE_0, check_receive_ok, PING_MEASURE_0, Ping_Init },
			{ STAGE_0, check_cancel, TERMINATION, Cancel },
			{ STAGE_0, check_receive_cancel, END, Exit },
			{ PING_MEASURE_0, check_temp_ping_0, PING_MEASURE_0, Ping },
			{ PING_MEASURE_0, check_receive_ok, PING_MEASURE_0, Update },
			{ PING_MEASURE_0, check_receive_ping, PING_MEASURE_0, Update },
			{ PING_MEASURE_0, check_cancel, TERMINATION, Cancel},
			{ PING_MEASURE_0, check_finish_ping, WAIT_NEXT, Decide },
			{ PING_MEASURE_0, check_receive_cancel, END, Exit },
			{ WAIT_NEXT, check_go_to_1, STAGE_1, Ready1 },
			{ STAGE_1, check_receive_ok, BWIDTH_MEASURE, Bwidth_Init },
			{ STAGE_1, check_cancel, TERMINATION, Cancel },
			{ STAGE_1, check_receive_cancel, END, Exit },
			{ BWIDTH_MEASURE, check_bwidth_burst_sent, BWIDTH_MEASURE, Bwidth_Decide },
			{ BWIDTH_MEASURE, check_receive_bwidth, BWIDTH_MEASURE, Update },
			{ BWIDTH_MEASURE, check_measure_bwidth, BWIDTH_MEASURE, Update },
			{ BWIDTH_MEASURE, check_cancel, TERMINATION, Cancel},
			{ BWIDTH_MEASURE, check_receive_cancel, END, Exit },
			{ BWIDTH_MEASURE, check_finish_bwidth, WAIT_NEXT, Decide },
			{ WAIT_NEXT, check_go_to_2, STAGE_2, Ready2 },
			{ WAIT_NEXT, check_receive_cancel, END, Exit },
			{ STAGE_2, check_receive_ok, PING_MEASURE_2,  Ping_Init },
			{ STAGE_2, check_cancel, TERMINATION, Cancel },
			{ STAGE_2, check_receive_cancel, END, Exit },
			{ PING_MEASURE_2, check_temp_ping_2, PING_MEASURE_2, Ping },
			{ PING_MEASURE_2, check_receive_ok, PING_MEASURE_2, Update },
			{ PING_MEASURE_2, check_receive_ping, PING_MEASURE_2, Update },
			{ PING_MEASURE_2, check_cancel, TERMINATION, Cancel},
			{ PING_MEASURE_2, check_receive_cancel, END, Exit },
			{ TERMINATION, check_cancel, TERMINATION, Cancel },
			{ TERMINATION, check_receive_cancel, END,  Exit },
			{ -1, NULL, -1, NULL }
	};

  // State machine creation
	q4s_fsm = fsm_new (WAIT_CONNECT, q4s_table, NULL);

	// State machine initialitation
	fsm_setup (q4s_fsm);

	pthread_mutex_lock(&mutex_print);
	printf("Press 'b' to begin a Q4S session\n");
	pthread_mutex_unlock(&mutex_print);

	while (1) {
		// State machine operation
		fsm_fire (q4s_fsm);
		// Waits for CLK_FSM milliseconds
		delay (CLK_FSM);
		if (finished) {
			break;
		}
	}

	finished = false;

	// Cancels timers
	cancel_timer_ping_0 = true;
	cancel_timer_ping_2 = true;
	cancel_timer_delivery_bwidth = true;
	cancel_timer_reception_bwidth = true;
	// Cancels the threads receiving Q4S messages
	cancel_UDP_thread = true;
	pthread_mutex_lock(&mutex_session);
	pthread_mutex_lock(&mutex_print);
	pthread_mutex_lock(&mutex_buffer_UDP);
	pthread_cancel(receive_UDP_thread);
	pthread_mutex_unlock(&mutex_buffer_UDP);
	pthread_mutex_unlock(&mutex_print);
	pthread_mutex_unlock(&mutex_session);

  cancel_TCP_thread = true;
	pthread_mutex_lock(&mutex_session);
	pthread_mutex_lock(&mutex_print);
	pthread_mutex_lock(&mutex_buffer_TCP);
	pthread_cancel(receive_TCP_thread);
	pthread_mutex_unlock(&mutex_buffer_TCP);
	pthread_mutex_unlock(&mutex_print);
	pthread_mutex_unlock(&mutex_session);

	//delay(1000);

	// Closes connection with Q4S server
	close(socket_UDP);
  close(socket_TCP);

	//delay(1000);


	// State machine destruction
	fsm_destroy (q4s_fsm);

	// Cancels the keyboard thread
	cancel_keyboard_thread = true;
	return 0;
}
