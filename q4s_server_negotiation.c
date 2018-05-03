#include "q4s_server_negotiation.h"

// GENERAL VARIABLES

// FOR Q4S SESSION MANAGING
// Q4S session
static type_q4s_session q4s_session;
// Variable to store the flags
int flags = 0;

// FOR CONNECTION MANAGING
// Structs with info for the connection
struct sockaddr_in server_TCP, client_TCP, server_UDP, client_UDP;
// Variable for socket assignments
int server_socket_TCP, client_socket_TCP, server_socket_UDP;
// Variable with struct length
socklen_t longc;
// Variable for TCP socket buffer
char buffer_TCP[MAXDATASIZE];
// Variable for UDP socket buffer
char buffer_UDP[MAXDATASIZE];

// FOR THREAD MANAGING
// Thread to check reception of TCP data
pthread_t receive_TCP_thread;
// Thread to check reception of UDP data
pthread_t receive_UDP_thread;
// Thread to check pressed keys on the keyboard
pthread_t keyboard_thread;
// Thread acting as timer for ping delivery
pthread_t timer_ping;
// Thread acting as timer for bwidth delivery
pthread_t timer_bwidth;
// Thread acting as timer for alert pause
pthread_t timer_alert;
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

// AUXILIARY VARIABLES
bool set_new_parameters = false;
// Variable used to obviate packet losses in jitter measure
int num_ping;
// Variable that indicates the number of new packet losses occurred
int num_packet_lost;
// Variable that indicates number of latency measures made by client
int num_latency_measures_client;
// Variable that indicates number of jitter measures made by client
int num_jitter_measures_client;
// Variable that indicates number of packetloss measures made by client
int num_packetloss_measures_client;
// Variable that indicates number of latency measures made by server
int num_latency_measures_server;
// Variable that indicates number of jitter measures made by server
int num_jitter_measures_server;
// Variable that indicates number of packetloss measures made by server
int num_packetloss_measures_server;
// Variable that shows number of Q4S BWIDTHs received in a period
int num_bwidth_received;


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

// Returns interval between two moments of time in ms
int ms_elapsed(struct timespec tm1, struct timespec tm2) {
  unsigned long long t = 1000 * (tm2.tv_sec - tm1.tv_sec)
    + (tm2.tv_nsec - tm1.tv_nsec) / 1000000;
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


// INITIALITATION FUNCTIONS

// System initialitation
// Creates a thread to explore keyboard and configures mutex
int system_setup (void) {
	pthread_mutex_init(&mutex_flags, NULL);
	pthread_mutex_init(&mutex_session, NULL);
	pthread_mutex_init(&mutex_buffer_TCP, NULL);
	pthread_mutex_init(&mutex_buffer_UDP, NULL);
	pthread_mutex_init(&mutex_print, NULL);
	pthread_mutex_init(&mutex_tm_latency, NULL);
	pthread_mutex_init(&mutex_tm_jitter, NULL);
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

// Creation of Q4S 200 OK message
// Creates a message asking server user some Q4S parameters
void create_200 (type_q4s_message *q4s_message, char request_method[20], bool new_param) {
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

	char h5[100];
	memset(h5, '\0', sizeof(h5));

	if (strcmp(request_method, "BEGIN") == 0) {
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

		strcpy(v, "v=0\n");
		strcpy(o, "o=q4s-UA 53655765 2353687637 IN IP4 127.0.0.1\n");
		strcpy(s, "s=Q4S\n");
		strcpy(i, "i=Q4S parameters\n");
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
		char a6[100];
		memset(a6, '\0', sizeof(a6));
		char a7[100];
		memset(a7, '\0', sizeof(a7));
		char a8[100];
		memset(a8, '\0', sizeof(a8));
		char a9[100];
		memset(a9, '\0', sizeof(a9));
		char a10[100];
		memset(a10, '\0', sizeof(a10));
		char a11[100];
		memset(a11, '\0', sizeof(a11));
		char a12[100];
		memset(a12, '\0', sizeof(a12));
		char a13[100];
		memset(a13, '\0', sizeof(a13));
		char a14[100];
		memset(a14, '\0', sizeof(a14));
		char a15[100];
		memset(a15, '\0', sizeof(a15));
		char a16[100];
		memset(a16, '\0', sizeof(a16));
		char a17[100];
		memset(a17, '\0', sizeof(a17));
		char a18[100];
		memset(a18, '\0', sizeof(a18));

    // Fixed parameters

		// Reactive is the default scenary
		strcpy(a2, "a=alerting-mode:Reactive\n");
		// Includes client IP direction
		strcpy(a4, "a=public-address:client IP4 ");
		strcat(a4, inet_ntoa(client_TCP.sin_addr));
		strcat(a4, "\n");
		// Includes server IP direction
		strcpy(a5, "a=public-address:server IP4 ");
		strcat(a5, inet_ntoa(server_TCP.sin_addr));
		strcat(a5, "\n");

		strcpy(a10, "a=flow:app clientListeningPort TCP/10000-20000\n");
		strcpy(a11, "a=flow:app clientListeningPort UDP/15000-18000\n");
		strcpy(a12, "a=flow:app serverListeningPort TCP/56000\n");
		strcpy(a13, "a=flow:app serverListeningPort UDP/56000\n");
		// Includes ports permitted for Q4S sessions
		strcpy(a14, "a=flow:q4s clientListeningPort UDP/");
		char s_port[10];
		sprintf(s_port, "%d", CLIENT_PORT_UDP);
		strcat(a14, s_port);
		strcat(a14, "\n");
		memset(s_port, '\0', strlen(s_port));
		strcpy(a15, "a=flow:q4s clientListeningPort TCP/");
		sprintf(s_port, "%d", CLIENT_PORT_TCP);
		strcat(a15, s_port);
		strcat(a15, "\n");
		memset(s_port, '\0', strlen(s_port));
		strcpy(a16, "a=flow:q4s serverListeningPort UDP/");
		sprintf(s_port, "%d", HOST_PORT_UDP);
		strcat(a16, s_port);
		strcat(a16, "\n");
		memset(s_port, '\0', strlen(s_port));
		strcpy(a17, "a=flow:q4s serverListeningPort TCP/");
		sprintf(s_port, "%d", HOST_PORT_TCP);
		strcat(a17, s_port);
		strcat(a17, "\n");
		memset(s_port, '\0', strlen(s_port));

    // Stores user inputs
	  char input[100];
		memset(input, '\0', sizeof(input));
		pthread_mutex_lock(&mutex_print);
		printf("\nDo you want to set default parameters? (yes/no): ");
		pthread_mutex_unlock(&mutex_print);
    scanf("%s", input);

		if (strstr(input, "yes")) {
			memset(input, '\0', sizeof(input));

			// Default SDP parameters
			strcpy(a1, "a=qos-level:0/0\n");
	    strcpy(a3, "a=alert-pause:1000\n");
	    strcpy(a6, "a=latency:200\n");
	    strcpy(a7, "a=jitter:500/500\n");
	    strcpy(a8, "a=bandwidth:100/100\n");
	    strcpy(a9, "a=packetloss:0.50/0.50\n");
			strcpy(a18, "a=measurement:procedure default(200/100,200/100,1000,40/80,100/256)");

      // Default expiration time for session
      strcpy(h3, "Expires: 6000\n");

		} else {
      memset(input, '\0', strlen(input));

			char client_desire[50]; // to store client suggestions
		  char s_value[10];  // to store string values

	    // Includes user input about QoS levels (showing values suggested by client)
	    strcpy(a1, "a=qos-level:");
		  if (q4s_session.qos_level[0] >= 0) {
			  memset(client_desire, '\0', strlen(client_desire));
			  memset(s_value, '\0', strlen(s_value));
			  strcpy(client_desire, "[Client desire: ");
			  sprintf(s_value, "%d", q4s_session.qos_level[0]);
			  strcat(client_desire, s_value);
			  strcat(client_desire, "]");
		  }
			pthread_mutex_lock(&mutex_print);
		  printf("\nEnter upstream QoS level (from 0 to 9) %s: ", client_desire);
			pthread_mutex_unlock(&mutex_print);
	    scanf("%s", input);
		  strcat(a1, input);
		  strcat(a1, "/");
	    memset(input, '\0', strlen(input));
		  if (q4s_session.qos_level[1] >= 0) {
			  memset(client_desire, '\0', strlen(client_desire));
			  memset(s_value, '\0', strlen(s_value));
			  strcpy(client_desire, "[Client desire: ");
			  sprintf(s_value, "%d", q4s_session.qos_level[1]);
			  strcat(client_desire, s_value);
			  strcat(client_desire, "]");
		  }
			pthread_mutex_lock(&mutex_print);
		  printf("Enter downstream QoS level (from 0 to 9) %s: ", client_desire);
			pthread_mutex_unlock(&mutex_print);
	    scanf("%s", input);
		  strcat(a1, input);
		  strcat(a1, "\n");
		  memset(input, '\0', strlen(input));

	    // Includes user input about alert pause
		  strcpy(a3, "a=alert-pause:");
			pthread_mutex_lock(&mutex_print);
		  printf("Enter alert pause (in ms): ");
			pthread_mutex_unlock(&mutex_print);
	    scanf("%s", input);
		  strcat(a3, input);
		  strcat(a3, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about latency threshold (showing value suggested by client)
		  strcpy(a6, "a=latency:");
		  if (q4s_session.latency_th >= 0) {
			  memset(client_desire, '\0', strlen(client_desire));
			  memset(s_value, '\0', strlen(s_value));
			  strcpy(client_desire, "[Client desire: ");
			  sprintf(s_value, "%d", q4s_session.latency_th);
			  strcat(client_desire, s_value);
			  strcat(client_desire, "]");
		  }
			pthread_mutex_lock(&mutex_print);
		  printf("Enter latency threshold (in ms) %s: ", client_desire);
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a6, input);
		  strcat(a6, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about jitter thresholds (showing values suggested by client)
		  strcpy(a7, "a=jitter:");
		  if (q4s_session.jitter_th[0] >= 0) {
			  memset(client_desire, '\0', strlen(client_desire));
			  memset(s_value, '\0', strlen(s_value));
			  strcpy(client_desire, "[Client desire: ");
			  sprintf(s_value, "%d", q4s_session.jitter_th[0]);
			  strcat(client_desire, s_value);
			  strcat(client_desire, "]");
		  }
			pthread_mutex_lock(&mutex_print);
		  printf("Enter upstream jitter threshold (in ms) %s: ", client_desire);
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a7, input);
		  strcat(a7, "/");
	    memset(input, '\0', strlen(input));
		  if (q4s_session.jitter_th[1] >= 0) {
			  memset(client_desire, '\0', strlen(client_desire));
			  memset(s_value, '\0', strlen(s_value));
			  strcpy(client_desire, "[Client desire: ");
			  sprintf(s_value, "%d", q4s_session.jitter_th[1]);
			  strcat(client_desire, s_value);
			  strcat(client_desire, "]");
		  }
			pthread_mutex_lock(&mutex_print);
		  printf("Enter downstream jitter threshold (in ms) %s: ", client_desire);
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a7, input);
		  strcat(a7, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about bandwidth thresholds (showing values suggested by client)
		  strcpy(a8, "a=bandwidth:");
		  if (q4s_session.bw_th[0] >= 0) {
			  memset(client_desire, '\0', strlen(client_desire));
			  memset(s_value, '\0', strlen(s_value));
			  strcpy(client_desire, "[Client desire: ");
			  sprintf(s_value, "%d", q4s_session.bw_th[0]);
			  strcat(client_desire, s_value);
			  strcat(client_desire, "]");
		  }
			pthread_mutex_lock(&mutex_print);
		  printf("Enter upstream bandwidth threshold (in kbps) %s: ", client_desire);
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a8, input);
		  strcat(a8, "/");
	    memset(input, '\0', strlen(input));
		  if (q4s_session.bw_th[1] >= 0) {
			  memset(client_desire, '\0', strlen(client_desire));
			  memset(s_value, '\0', strlen(s_value));
			  strcpy(client_desire, "[Client desire: ");
			  sprintf(s_value, "%d", q4s_session.bw_th[1]);
			  strcat(client_desire, s_value);
			  strcat(client_desire, "]");
		  }
			pthread_mutex_lock(&mutex_print);
		  printf("Enter downstream bandwidth threshold (in kbps) %s: ", client_desire);
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a8, input);
		  strcat(a8, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about packetloss thresholds (showing values suggested by client)
		  strcpy(a9, "a=packetloss:");
		  if (q4s_session.packetloss_th[0] >= 0) {
			  memset(client_desire, '\0', strlen(client_desire));
			  memset(s_value, '\0', strlen(s_value));
			  strcpy(client_desire, "[Client desire: ");
			  sprintf(s_value, "%.*f", 2, q4s_session.packetloss_th[0]);
			  strcat(client_desire, s_value);
			  strcat(client_desire, "]");
		  }
			pthread_mutex_lock(&mutex_print);
		  printf("Enter upstream packetloss threshold (percentage from 0.00 to 1.00) %s: ", client_desire);
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a9, input);
		  strcat(a9, "/");
	    memset(input, '\0', strlen(input));
		  if (q4s_session.packetloss_th[1] >= 0) {
			  memset(client_desire, '\0', strlen(client_desire));
			  memset(s_value, '\0', strlen(s_value));
			  strcpy(client_desire, "[Client desire: ");
			  sprintf(s_value, "%.*f", 2, q4s_session.packetloss_th[1]);
			  strcat(client_desire, s_value);
			  strcat(client_desire, "]");
		  }
			pthread_mutex_lock(&mutex_print);
		  printf("Enter downstream packetloss threshold (percentage from 0.00 to 1.00) %s: ", client_desire);
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a9, input);
		  strcat(a9, "\n");
	    memset(input, '\0', strlen(input));

			// Includes user input about measurement procedure
		  strcpy(a18, "a=measurement:procedure default(");
			pthread_mutex_lock(&mutex_print);
		  printf("Enter interval of time between Q4S PING requests sent by client in NEGOTIATION phase (in ms): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a18, input);
			strcat(a18, "/");
			memset(input, '\0', strlen(input));
			pthread_mutex_lock(&mutex_print);
			printf("Enter interval of time between Q4S PING requests sent by server in NEGOTIATION phase (in ms): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a18, input);
			strcat(a18, ",");
			memset(input, '\0', strlen(input));
			pthread_mutex_lock(&mutex_print);
			printf("Enter interval of time between Q4S PING requests sent by client in CONTINUITY phase (in ms): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a18, input);
			strcat(a18, "/");
			memset(input, '\0', strlen(input));
      pthread_mutex_lock(&mutex_print);
			printf("Enter interval of time between Q4S PING requests sent by server in CONTINUITY phase (in ms): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a18, input);
			strcat(a18, ",");
			memset(input, '\0', strlen(input));
      pthread_mutex_lock(&mutex_print);
			printf("Enter interval of time between Q4S BWIDTH requests in NEGOTIATION phase (in ms): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a18, input);
			strcat(a18, ",");
			memset(input, '\0', strlen(input));
      pthread_mutex_lock(&mutex_print);
			printf("Enter window size for jitter and latency calculations for client: ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a18, input);
			strcat(a18, "/");
			memset(input, '\0', strlen(input));
      pthread_mutex_lock(&mutex_print);
			printf("Enter window size for jitter and latency calculations for server: ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a18, input);
			strcat(a18, ",");
			memset(input, '\0', strlen(input));
      pthread_mutex_lock(&mutex_print);
			printf("Enter window size for packet loss calculations for client: ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a18, input);
			strcat(a18, "/");
			memset(input, '\0', strlen(input));
      pthread_mutex_lock(&mutex_print);
			printf("Enter window size for packet loss calculations for server: ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a18, input);
			strcat(a18, ")");
		  strcat(a18, "\n");
	    memset(input, '\0', strlen(input));

			// Includes user input about expiration time of Q4S session
			strcpy(h3, "Expires: ");
			pthread_mutex_lock(&mutex_print);
			printf("Enter expiration time of the Q4S session (in ms): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			strcat(h3, input);
			strcat(h3, "\n");
			memset(input, '\0', strlen(input));
		}
		// Includes current time
		strcpy(h1, "Date: ");
		const char * now = current_time();
		strcat(h1, now);
		strcat(h1, "\n");
		// Type of the content
		strcpy(h2,"Content-Type: application/sdp\n");
		// Includes signature if necessary
		strcpy(h4, "Signature: \n");
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
		strcat(body, a6);
		strcat(body, a7);
		strcat(body, a8);
		strcat(body, a9);
		strcat(body, a10);
		strcat(body, a11);
		strcat(body, a12);
		strcat(body, a13);
		strcat(body, a14);
		strcat(body, a15);
		strcat(body, a16);
		strcat(body, a17);
		strcat(body, a18);

	} else if (strcmp(request_method, "PING") == 0) {
		// Includes session ID
		strcpy(h1, "Session-Id: ");
		char s_session_id[20];
		sprintf(s_session_id, "%d", (&q4s_session)->session_id);
		strcat(h1, s_session_id);
		strcat(h1, "\n");
		// Includes sequence number of Q4S PING received
		strcpy(h2, "Sequence-Number: ");
		char s_seq_num[10];
		sprintf(s_seq_num, "%d", (&q4s_session)->seq_num_client);
		strcat(h2, s_seq_num);
		strcat(h2, "\n");
		// Includes body length in "Content Length" header field
		strcpy(h3, "Content Length: ");
		int body_length = strlen(body);
		char s_body_length[10];
		sprintf(s_body_length, "%d", body_length);
	  strcat(h3, s_body_length);
		strcat(h3, "\n");
		// Includes timestamp of client
		strcpy(h4, "Timestamp: ");
		strcat(h4, (&q4s_session)->client_timestamp);
		strcat(h4, "\n");

		// Prepares header with header fields
		strcpy(header, h1);
		strcat(header, h2);
	  strcat(header, h3);
		strcat(header, h4);

		if (new_param) {
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

			strcpy(v, "v=0\n");
			strcpy(o, "o=q4s-UA 53655765 2353687637 IN IP4 127.0.0.1\n");
			strcpy(s, "s=Q4S\n");
			strcpy(i, "i=Q4S new parameters\n");
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

			char input[20];
			memset(input, '\0', sizeof(input));

			char s_field[20];
			memset(s_field, '\0', sizeof(s_field));

			pthread_mutex_lock(&mutex_print);
		  printf("\nEnter new parameters (or 'k' to keep the same)\n");
			pthread_mutex_unlock(&mutex_print);

			// Includes user input about QoS levels (showing values suggested by client)
	    strcpy(a1, "a=qos-level:");
			pthread_mutex_lock(&mutex_print);
		  printf("\nEnter upstream QoS level (from 0 to 9): ");
			pthread_mutex_unlock(&mutex_print);
	    scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->qos_level[0]);
				strcat(a1, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a1, input);
			}
		  strcat(a1, "/");
	    memset(input, '\0', strlen(input));

			pthread_mutex_lock(&mutex_print);
		  printf("Enter downstream QoS level (from 0 to 9): ");
			pthread_mutex_unlock(&mutex_print);
	    scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->qos_level[1]);
				strcat(a1, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a1, input);
			}
		  strcat(a1, "\n");
		  memset(input, '\0', strlen(input));

	    // Includes user input about latency threshold (showing value suggested by client)
		  strcpy(a2, "a=latency:");
			pthread_mutex_lock(&mutex_print);
		  printf("Enter latency threshold (in ms): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->latency_th);
				strcat(a2, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a2, input);
			}
		  strcat(a2, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about jitter thresholds (showing values suggested by client)
		  strcpy(a3, "a=jitter:");
			pthread_mutex_lock(&mutex_print);
		  printf("Enter upstream jitter threshold (in ms): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->jitter_th[0]);
				strcat(a3, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a3, input);
			}
		  strcat(a3, "/");
	    memset(input, '\0', strlen(input));

			pthread_mutex_lock(&mutex_print);
		  printf("Enter downstream jitter threshold (in ms): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->jitter_th[1]);
				strcat(a3, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a3, input);
			}
		  strcat(a3, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about bandwidth thresholds (showing values suggested by client)
		  strcpy(a4, "a=bandwidth:");
			pthread_mutex_lock(&mutex_print);
		  printf("Enter upstream bandwidth threshold (in kbps): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->bw_th[0]);
				strcat(a4, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a4, input);
			}
		  strcat(a4, "/");
	    memset(input, '\0', strlen(input));

			pthread_mutex_lock(&mutex_print);
		  printf("Enter downstream bandwidth threshold (in kbps): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->bw_th[1]);
				strcat(a4, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a4, input);
			}
		  strcat(a4, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about packetloss thresholds (showing values suggested by client)
		  strcpy(a5, "a=packetloss:");
			pthread_mutex_lock(&mutex_print);
		  printf("Enter upstream packetloss threshold (percentage from 0.00 to 1.00): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%.2f", (&q4s_session)->packetloss_th[0]);
				strcat(a5, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a5, input);
			}
		  strcat(a5, "/");
	    memset(input, '\0', strlen(input));

			pthread_mutex_lock(&mutex_print);
		  printf("Enter downstream packetloss threshold (percentage from 0.00 to 1.00): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%.2f", (&q4s_session)->packetloss_th[1]);
				strcat(a5, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a5, input);
			}
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
		}

	} else if (strcmp(request_method, "READY0") == 0) {
		// Includes session ID
		strcpy(h1, "Session-Id: ");
		char s_session_id[20];
		sprintf(s_session_id, "%d", (&q4s_session)->session_id);
		strcat(h1, s_session_id);
		strcat(h1, "\n");
    // Includes Stage number
		strcpy(h2, "Stage: 0\n");
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

		if (new_param) {
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

			strcpy(v, "v=0\n");
			strcpy(o, "o=q4s-UA 53655765 2353687637 IN IP4 127.0.0.1\n");
			strcpy(s, "s=Q4S\n");
			strcpy(i, "i=Q4S new parameters\n");
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

			char input[20];
			memset(input, '\0', sizeof(input));

			char s_field[20];
			memset(s_field, '\0', sizeof(s_field));

			pthread_mutex_lock(&mutex_print);
		  printf("\nEnter new parameters (or 'k' to keep the same)\n");
			pthread_mutex_unlock(&mutex_print);

			// Includes user input about QoS levels (showing values suggested by client)
	    strcpy(a1, "a=qos-level:");
			pthread_mutex_lock(&mutex_print);
		  printf("\nEnter upstream QoS level (from 0 to 9): ");
			pthread_mutex_unlock(&mutex_print);
	    scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->qos_level[0]);
				strcat(a1, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a1, input);
			}
		  strcat(a1, "/");
	    memset(input, '\0', strlen(input));

			pthread_mutex_lock(&mutex_print);
		  printf("Enter downstream QoS level (from 0 to 9): ");
			pthread_mutex_unlock(&mutex_print);
	    scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->qos_level[1]);
				strcat(a1, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a1, input);
			}
		  strcat(a1, "\n");
		  memset(input, '\0', strlen(input));

	    // Includes user input about latency threshold (showing value suggested by client)
		  strcpy(a2, "a=latency:");
			pthread_mutex_lock(&mutex_print);
		  printf("Enter latency threshold (in ms): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->latency_th);
				strcat(a2, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a2, input);
			}
		  strcat(a2, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about jitter thresholds (showing values suggested by client)
		  strcpy(a3, "a=jitter:");
			pthread_mutex_lock(&mutex_print);
		  printf("Enter upstream jitter threshold (in ms): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->jitter_th[0]);
				strcat(a3, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a3, input);
			}
		  strcat(a3, "/");
	    memset(input, '\0', strlen(input));

			pthread_mutex_lock(&mutex_print);
		  printf("Enter downstream jitter threshold (in ms): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->jitter_th[1]);
				strcat(a3, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a3, input);
			}
		  strcat(a3, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about bandwidth thresholds (showing values suggested by client)
		  strcpy(a4, "a=bandwidth:");
			pthread_mutex_lock(&mutex_print);
		  printf("Enter upstream bandwidth threshold (in kbps): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->bw_th[0]);
				strcat(a4, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a4, input);
			}
		  strcat(a4, "/");
	    memset(input, '\0', strlen(input));

			pthread_mutex_lock(&mutex_print);
		  printf("Enter downstream bandwidth threshold (in kbps): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->bw_th[1]);
				strcat(a4, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a4, input);
			}
		  strcat(a4, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about packetloss thresholds (showing values suggested by client)
		  strcpy(a5, "a=packetloss:");
			pthread_mutex_lock(&mutex_print);
		  printf("Enter upstream packetloss threshold (percentage from 0.00 to 1.00): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%.2f", (&q4s_session)->packetloss_th[0]);
				strcat(a5, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a5, input);
			}
		  strcat(a5, "/");
	    memset(input, '\0', strlen(input));

			pthread_mutex_lock(&mutex_print);
		  printf("Enter downstream packetloss threshold (percentage from 0.00 to 1.00): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%.2f", (&q4s_session)->packetloss_th[1]);
				strcat(a5, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a5, input);
			}
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
		}

	} else if (strcmp(request_method, "READY1") == 0) {
		// Includes session ID
		strcpy(h1, "Session-Id: ");
		char s_session_id[20];
		sprintf(s_session_id, "%d", (&q4s_session)->session_id);
		strcat(h1, s_session_id);
		strcat(h1, "\n");
    // Includes Stage number
		strcpy(h2, "Stage: 1\n");
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

		if (new_param) {
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

			strcpy(v, "v=0\n");
			strcpy(o, "o=q4s-UA 53655765 2353687637 IN IP4 127.0.0.1\n");
			strcpy(s, "s=Q4S\n");
			strcpy(i, "i=Q4S new parameters\n");
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

			char input[20];
			memset(input, '\0', sizeof(input));

			char s_field[20];
			memset(s_field, '\0', sizeof(s_field));

			pthread_mutex_lock(&mutex_print);
		  printf("\nEnter new parameters (or 'k' to keep the same)\n");
			pthread_mutex_unlock(&mutex_print);

			// Includes user input about QoS levels (showing values suggested by client)
	    strcpy(a1, "a=qos-level:");
			pthread_mutex_lock(&mutex_print);
		  printf("\nEnter upstream QoS level (from 0 to 9): ");
			pthread_mutex_unlock(&mutex_print);
	    scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->qos_level[0]);
				strcat(a1, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a1, input);
			}
		  strcat(a1, "/");
	    memset(input, '\0', strlen(input));

			pthread_mutex_lock(&mutex_print);
		  printf("Enter downstream QoS level (from 0 to 9): ");
			pthread_mutex_unlock(&mutex_print);
	    scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->qos_level[1]);
				strcat(a1, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a1, input);
			}
		  strcat(a1, "\n");
		  memset(input, '\0', strlen(input));

	    // Includes user input about latency threshold (showing value suggested by client)
		  strcpy(a2, "a=latency:");
			pthread_mutex_lock(&mutex_print);
		  printf("Enter latency threshold (in ms): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->latency_th);
				strcat(a2, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a2, input);
			}
		  strcat(a2, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about jitter thresholds (showing values suggested by client)
		  strcpy(a3, "a=jitter:");
			pthread_mutex_lock(&mutex_print);
		  printf("Enter upstream jitter threshold (in ms): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->jitter_th[0]);
				strcat(a3, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a3, input);
			}
		  strcat(a3, "/");
	    memset(input, '\0', strlen(input));

			pthread_mutex_lock(&mutex_print);
		  printf("Enter downstream jitter threshold (in ms): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->jitter_th[1]);
				strcat(a3, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a3, input);
			}
		  strcat(a3, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about bandwidth thresholds (showing values suggested by client)
		  strcpy(a4, "a=bandwidth:");
			pthread_mutex_lock(&mutex_print);
		  printf("Enter upstream bandwidth threshold (in kbps): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->bw_th[0]);
				strcat(a4, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a4, input);
			}
		  strcat(a4, "/");
	    memset(input, '\0', strlen(input));

			pthread_mutex_lock(&mutex_print);
		  printf("Enter downstream bandwidth threshold (in kbps): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%d", (&q4s_session)->bw_th[1]);
				strcat(a4, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a4, input);
			}
		  strcat(a4, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about packetloss thresholds (showing values suggested by client)
		  strcpy(a5, "a=packetloss:");
			pthread_mutex_lock(&mutex_print);
		  printf("Enter upstream packetloss threshold (percentage from 0.00 to 1.00): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%.2f", (&q4s_session)->packetloss_th[0]);
				strcat(a5, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a5, input);
			}
		  strcat(a5, "/");
	    memset(input, '\0', strlen(input));

			pthread_mutex_lock(&mutex_print);
		  printf("Enter downstream packetloss threshold (percentage from 0.00 to 1.00): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
			if (strcmp(input, "k") == 0) {
				sprintf(s_field, "%.2f", (&q4s_session)->packetloss_th[1]);
				strcat(a5, s_field);
				memset(s_field, '\0', sizeof(s_field));
			} else {
				strcat(a5, input);
			}
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
		}
	} else {
		// Includes current time
		strcpy(h1, "Date: ");
	  const char * now = current_time();
		strcat(h1, now);
		strcat(h1, "\n");

		// Type of the content
		strcpy(h2,"Content-Type: application/sdp\n");
		// Includes signature if necessary
		strcpy(h3, "Signature: \n");

		// Prepares header with header fields
		strcpy(header, h1);
		strcat(header, h2);
	  strcat(header, h3);
	}

  // Delegates in a response creation function
  create_response (q4s_message, "200", "OK", header, body);
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

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");
	// Includes sequence number of Q4S PING to send
	strcpy(h2, "Sequence-Number: ");
	char s_seq_num[10];
	sprintf(s_seq_num, "%d", (&q4s_session)->seq_num_server);
	strcat(h2, s_seq_num);
	strcat(h2, "\n");
	// Includes timestamp
	strcpy(h3, "Timestamp: ");
	const char * now = current_time();
	strcat(h3, now);
	strcat(h3, "\n");
  // Includes measures
	strcpy(h4, "Measurements: ");
	strcat(h4, "l=");
	if((&q4s_session)->latency_measure_server && (&q4s_session)->latency_th) {
		char s_latency[6];
		sprintf(s_latency, "%d", (&q4s_session)->latency_measure_server);
    strcat(h4, s_latency);
	} else {
		strcat(h4, " ");
	}
	strcat(h4, ", j=");
	if((&q4s_session)->jitter_measure_server && (&q4s_session)->jitter_th[0]) {
		char s_jitter[6];
		sprintf(s_jitter, "%d", (&q4s_session)->jitter_measure_server);
    strcat(h4, s_jitter);
	} else {
		strcat(h4, " ");
	}
	strcat(h4, ", pl=");
	if((&q4s_session)->packetloss_measure_server >= 0 && (&q4s_session)->packetloss_th[0]) {
		char s_packetloss[6];
		sprintf(s_packetloss, "%.2f", (&q4s_session)->packetloss_measure_server);
    strcat(h4, s_packetloss);
	} else {
		strcat(h4, " ");
	}
	strcat(h4, ", bw=");
	if((&q4s_session)->bw_measure_server && (&q4s_session)->bw_th[0]) {
		char s_bw[6];
		sprintf(s_bw, "%d", (&q4s_session)->bw_measure_server);
    strcat(h4, s_bw);
	} else {
		strcat(h4, " ");
	}
	strcat(h4, "\n");

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
  create_request (q4s_message,"PING", header, body);
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

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");
	// Includes sequence number of Q4S PING to send
	strcpy(h2, "Sequence-Number: ");
	char s_seq_num[10];
	sprintf(s_seq_num, "%d", (&q4s_session)->seq_num_server);
	strcat(h2, s_seq_num);
	strcat(h2, "\n");
	// Includes content type
	strcpy(h3, "Content-Type: text\n");
	// Includes measures
	strcpy(h4, "Measurements: ");
	strcat(h4, "l=");
	if((&q4s_session)->latency_measure_server && (&q4s_session)->latency_th) {
		char s_latency[6];
		sprintf(s_latency, "%d", (&q4s_session)->latency_measure_server);
    strcat(h4, s_latency);
	} else {
		strcat(h4, " ");
	}
	strcat(h4, ", j=");
	if((&q4s_session)->jitter_measure_server && (&q4s_session)->jitter_th[0]) {
		char s_jitter[6];
		sprintf(s_jitter, "%d", (&q4s_session)->jitter_measure_server);
    strcat(h4, s_jitter);
	} else {
		strcat(h4, " ");
	}
	strcat(h4, ", pl=");
	if((&q4s_session)->packetloss_measure_server >= 0 && (&q4s_session)->packetloss_th[0]) {
		char s_packetloss[6];
		sprintf(s_packetloss, "%.2f", (&q4s_session)->packetloss_measure_server);
    strcat(h4, s_packetloss);
	} else {
		strcat(h4, " ");
	}
	strcat(h4, ", bw=");
	if((&q4s_session)->bw_measure_server && (&q4s_session)->bw_th[0]) {
		char s_bw[6];
		sprintf(s_bw, "%d", (&q4s_session)->bw_measure_server);
    strcat(h4, s_bw);
	} else {
		strcat(h4, " ");
	}
	strcat(h4, "\n");
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

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");

	strcpy(h2, "Expires: 0\n");

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

// Storage of a Q4S message just received
// Converts from char[MAXDATASIZE] to type_q4s_message
void store_message (char received_message[MAXDATASIZE], type_q4s_message *q4s_message) {
  // Auxiliary variables
	char *fragment1;
  char *fragment2;

  char start_line[200]; // to store start line
  char header[500];  // to store header
  char body[5000];  // to store body

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

  // Stores Q4S message
	strcpy((q4s_message)->start_line, start_line);
  strcpy((q4s_message)->header, header);
  strcpy((q4s_message)->body, body);
}

// Q4S PARAMETER STORAGE FUNCTION

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

	printf("\n");

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
	//		pthread_mutex_lock(&mutex_print);
	//		printf("Session ID stored: %d\n", (q4s_session)->session_id);
  //    pthread_mutex_unlock(&mutex_print);
		} else if (fragment = strstr(copy_body, "o=")) {  // if Session ID is in the body
		  fragment = strstr(fragment, " ");  // moves to the beginning of the value
		  char *string_id;
		  string_id = strtok(fragment, " ");  // stores string value
		  (q4s_session)->session_id = atoi(string_id);  // converts into int and stores
		  memset(fragment, '\0', strlen(fragment));
		  strcpy(copy_body, body);  // restores copy of header
  //    pthread_mutex_lock(&mutex_print);
	//	  printf("Session ID stored: %d\n", (q4s_session)->session_id);
  //    pthread_mutex_unlock(&mutex_print);
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
//    pthread_mutex_lock(&mutex_print);
//		printf("Expiration time stored: %d\n", (q4s_session)->expiration_time);
//    pthread_mutex_unlock(&mutex_print);
	}

	if (strcmp(copy_start_line, "PING q4s://www.example.com Q4S/1.0") == 0
    || strcmp(copy_start_line, "BWIDTH q4s://www.example.com Q4S/1.0") == 0) {
		// If there is a Date parameter in the header
		if (fragment = strstr(copy_header, "Timestamp")){
			fragment = fragment + 11;  // moves to the beginning of the value
			char *s_date;
			s_date = strtok(fragment, "\n");  // stores string value
			strcpy((q4s_session)->client_timestamp, s_date);  // stores
			memset(fragment, '\0', strlen(fragment));
			strcpy(copy_header, header);  // restores copy of header
      pthread_mutex_lock(&mutex_print);
			printf("Client timestamp stored: %s\n", (q4s_session)->client_timestamp);
      pthread_mutex_unlock(&mutex_print);
		}
		// If there is a Sequence Number parameter in the header
		if (fragment = strstr(copy_header, "Sequence-Number")){
			fragment = fragment + 17;  // moves to the beginning of the value
			char *s_seq_num;
			s_seq_num = strtok(fragment, "\n");  // stores string value
			(q4s_session)->seq_num_client = atoi(s_seq_num);  // converts into int and stores
			memset(fragment, '\0', strlen(fragment));
			strcpy(copy_header, header);  // restores copy of header
      pthread_mutex_lock(&mutex_print);
			printf("Sequence number of client stored: %d\n", (q4s_session)->seq_num_client);
      pthread_mutex_unlock(&mutex_print);
		}
		// If there is a Measurements parameter in the header
		if (fragment = strstr(copy_header, "Measurements")){
			fragment = fragment + 14;  // moves to the beginning of the value
			char *s_latency;
			strtok(fragment, "=");
			s_latency = strtok(NULL, ",");  // stores string value
			if (strcmp(s_latency," ") != 0) {
				num_latency_measures_client++;
				(q4s_session)->latency_measure_client = atoi(s_latency);  // converts into int and stores
				pthread_mutex_lock(&mutex_print);
				printf("Latency measure of client stored: %d\n", (q4s_session)->latency_measure_client);
	      pthread_mutex_unlock(&mutex_print);
			}

			char *s_jitter;
			strtok(NULL, "=");
			s_jitter = strtok(NULL, ",");  // stores string value
			if (strcmp(s_jitter," ") != 0) {
				num_jitter_measures_client++;
				(q4s_session)->jitter_measure_client = atoi(s_jitter);  // converts into int and stores
				pthread_mutex_lock(&mutex_print);
				printf("Jitter measure of client stored: %d\n", (q4s_session)->jitter_measure_client);
	      pthread_mutex_unlock(&mutex_print);
			}

			char *s_pl;
			strtok(NULL, "=");
			s_pl = strtok(NULL, ",");  // stores string value
      if (strcmp(s_pl," ") != 0) {
				num_packetloss_measures_client++;
				(q4s_session)->packetloss_measure_client = atof(s_pl);  // converts into int and stores
				pthread_mutex_lock(&mutex_print);
				printf("Packetloss measure of client stored: %.2f\n", (q4s_session)->packetloss_measure_client);
	      pthread_mutex_unlock(&mutex_print);
			}

			char *s_bw;
			strtok(NULL, "=");
			s_bw = strtok(NULL, "\n");  // stores string value
      if (strcmp(s_bw," ") != 0) {
				(q4s_session)->bw_measure_client = atoi(s_bw);  // converts into int and stores
				pthread_mutex_lock(&mutex_print);
				printf("Bandwidth measure of client stored: %d\n", (q4s_session)->bw_measure_client);
	      pthread_mutex_unlock(&mutex_print);
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
			pthread_mutex_lock(&mutex_print);
			printf("Sequence number confirmed: %d\n", (q4s_session)->seq_num_confirmed);
			pthread_mutex_unlock(&mutex_print);
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
//    pthread_mutex_lock(&mutex_print);
//		printf("QoS levels stored: %d/%d\n", (q4s_session)->qos_level[0],
//		  (q4s_session)->qos_level[1]);
//		pthread_mutex_unlock(&mutex_print);

	}

  // If there is an alert pause parameter in the body
	if (fragment = strstr(copy_body, "a=alert-pause:")){
		fragment = fragment + 14;  // moves to the beginning of the value
		char *alert_pause;
		alert_pause = strtok(fragment, "\n");  // stores string value
		(q4s_session)->alert_pause = atoi(alert_pause);  // converts into int and stores
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_body, body);  // restores copy of body
//    pthread_mutex_lock(&mutex_print);
//		printf("Alert pause stored: %d\n", (q4s_session)->alert_pause);
//    pthread_mutex_unlock(&mutex_print);
	}

  // If there is an latency parameter in the body
	if (fragment = strstr(copy_body, "a=latency:")){
		fragment = fragment + 10;  // moves to the beginning of the value
		char *latency;
		latency = strtok(fragment, "\n");  // stores string value
		(q4s_session)->latency_th = atoi(latency);  // converts into int and stores
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_body, body);  // restores copy of body
//    pthread_mutex_lock(&mutex_print);
//		printf("Latency threshold stored: %d\n", (q4s_session)->latency_th);
//    pthread_mutex_unlock(&mutex_print);
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
//    pthread_mutex_lock(&mutex_print);
//		printf("Jitter thresholds stored: %d/%d\n", (q4s_session)->jitter_th[0],
//		  (q4s_session)->jitter_th[1]);
//		pthread_mutex_unlock(&mutex_print);

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
//    pthread_mutex_lock(&mutex_print);
//		printf("Bandwidth thresholds stored: %d/%d\n", (q4s_session)->bw_th[0],
//		  (q4s_session)->bw_th[1]);
//    pthread_mutex_unlock(&mutex_print);

		if ((q4s_session)->bw_th[1] > 0) {
	    int target_bwidth = (q4s_session)->bw_th[1]; // kbps
	    int message_size = MESSAGE_BWIDTH_SIZE * 8; // bits
	    float messages_fract_per_ms = ((float) target_bwidth / (float) message_size);
	    int messages_int_per_ms = floor(messages_fract_per_ms);

	    int messages_per_s[10];
	    messages_per_s[0] = (int) ((messages_fract_per_ms - (float) messages_int_per_ms) * 1000);
	    int ms_per_message[11];
	    ms_per_message[0] = 1;

	    int divisor;

	    for (int i = 0; i < 10; i++) {
		    divisor = 2;
		    while ((int) (1000/divisor) > messages_per_s[i]) {
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
	    for (int j = 0; j < sizeof(ms_per_message); j++) {
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
//		pthread_mutex_lock(&mutex_print);
//		printf("Packetloss thresholds stored: %.*f/%.*f\n", 2,
//		  (q4s_session)->packetloss_th[0], 2, (q4s_session)->packetloss_th[1]);
//    pthread_mutex_unlock(&mutex_print);
	}

	// If there is a measurement procedure parameter in the body
	if (fragment = strstr(copy_body, "a=measurement:procedure default(")) {
    fragment = fragment + 32;  // moves to the beginning of the value
		char *ping_interval_negotiation;
		strtok(fragment, "/");
		ping_interval_negotiation = strtok(NULL, ",");  // stores string value
		(q4s_session)->ping_clk_negotiation = atoi(ping_interval_negotiation);  // converts into int and stores
//    pthread_mutex_lock(&mutex_print);
//		printf("Interval between Q4S PING stored (NEGOTIATION): %d\n", (q4s_session)->ping_clk_negotiation);
//    pthread_mutex_unlock(&mutex_print);
		char *ping_interval_continuity;
		strtok(NULL, "/");
		ping_interval_continuity = strtok(NULL, ",");  // stores string value
		(q4s_session)->ping_clk_continuity = atoi(ping_interval_continuity);  // converts into int and stores
//    pthread_mutex_lock(&mutex_print);
//		printf("Interval between Q4S PING stored (CONTINUITY): %d\n", (q4s_session)->ping_clk_continuity);
//    pthread_mutex_unlock(&mutex_print);
		char *bwidth_interval;
		bwidth_interval = strtok(NULL, ",");  // stores string value
		(q4s_session)->bwidth_clk = atoi(bwidth_interval);  // converts into int and stores
//    pthread_mutex_lock(&mutex_print);
//		printf("Interval between Q4S BWIDTH stored: %d\n", (q4s_session)->bwidth_clk);
//    pthread_mutex_unlock(&mutex_print);
		char *window_size_lj;
		strtok(NULL, "/");
		window_size_lj = strtok(NULL, ",");  // stores string value
		(q4s_session)->window_size_latency_jitter = atoi(window_size_lj);  // converts into int and stores
//    pthread_mutex_lock(&mutex_print);
//		printf("Window size for latency and jitter stored: %d\n", (q4s_session)->window_size_latency_jitter);
//    pthread_mutex_unlock(&mutex_print);
		char *window_size_pl;
		strtok(NULL, "/");
		window_size_pl = strtok(NULL, ")");  // stores string value
		(q4s_session)->window_size_packetloss = atoi(window_size_pl);  // converts into int and stores
//    pthread_mutex_lock(&mutex_print);
//		printf("Window size for packet loss stored: %d\n", (q4s_session)->window_size_packetloss);
//    pthread_mutex_unlock(&mutex_print);
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_body, body);  // restores copy of body
	}

}


// FUNCTIONS FOR UPDATING AND STORING MEASURES

// Updates and stores latency measures
void update_latency(type_q4s_session *q4s_session, int latency_measured) {
   int num_samples = 0;
	 int window_size  = (q4s_session)->window_size_latency_jitter;
	 for (int i=0; i < sizeof((q4s_session)->latency_samples); i++) {
		 if ((q4s_session)->latency_samples[i] > 0) {
			 num_samples++;
		 } else {
			 break;
		 }
	 }
	 if (num_samples == window_size && window_size > 0) {
		 for (int j=0; j < num_samples - 1; j++) {
			 (q4s_session)->latency_samples[j] = (q4s_session)->latency_samples[j+1];
		 }
		 (q4s_session)->latency_samples[num_samples - 1] = latency_measured;
		 pthread_mutex_lock(&mutex_print);
 		 printf("\nWindow size reached for latency measure\n");
     pthread_mutex_unlock(&mutex_print);
	 } else {
		 (q4s_session)->latency_samples[num_samples] = latency_measured;
		 num_samples++;
	 }

   sort_array((q4s_session)->latency_samples, num_samples);
	 if (num_samples % 2 == 0) {
		 int median_elem1 = num_samples/2;
		 int median_elem2 = num_samples/2 + 1;
		 (q4s_session)->latency_measure_server = ((q4s_session)->latency_samples[median_elem1-1]
		   + (q4s_session)->latency_samples[median_elem2-1]) / 2;
	 } else {
		 int median_elem = (num_samples + 1) / 2;
		 (q4s_session)->latency_measure_server = (q4s_session)->latency_samples[median_elem-1];
	 }
	 pthread_mutex_lock(&mutex_print);
	 printf("Updated value of latency measure: %d\n", (q4s_session)->latency_measure_server);
	 pthread_mutex_unlock(&mutex_print);
}

// Updates and stores latency measures
void update_jitter(type_q4s_session *q4s_session, int elapsed_time) {
	int num_samples = 0;
	int window_size  = (q4s_session)->window_size_latency_jitter;
	for (int i=0; i < sizeof((q4s_session)->elapsed_time_samples); i++) {
		if ((q4s_session)->elapsed_time_samples[i] > 0) {
			num_samples++;
		} else {
			break;
		}
	}

	if (num_samples == window_size && window_size > 0) {
		for (int j=0; j < num_samples - 1; j++) {
			(q4s_session)->elapsed_time_samples[j] = (q4s_session)->elapsed_time_samples[j+1];
		}
		(q4s_session)->elapsed_time_samples[num_samples - 1] = elapsed_time;
		pthread_mutex_lock(&mutex_print);
		printf("\nWindow size reached for jitter measure\n");
		pthread_mutex_unlock(&mutex_print);
	} else {
		(q4s_session)->elapsed_time_samples[num_samples] = elapsed_time;
		num_samples++;
	}

	if (num_samples <= 1) {
		return;
	} else {
		int elapsed_time_mean = 0;
		for (int j=0; j < num_samples - 1; j++) {
			elapsed_time_mean = elapsed_time_mean + (q4s_session)->elapsed_time_samples[j];
		}
		elapsed_time_mean = elapsed_time_mean / (num_samples - 1);
		(q4s_session)->jitter_measure_server = abs(elapsed_time - elapsed_time_mean);
		pthread_mutex_lock(&mutex_print);
		printf("Updated value of jitter measure: %d\n", (q4s_session)->jitter_measure_server);
    pthread_mutex_unlock(&mutex_print);
		num_jitter_measures_server++;
	}
}

// Updates and stores packetloss measures
void update_packetloss(type_q4s_session *q4s_session, int lost_packets) {
	// Number of packets taken in account for measure
	int num_samples = 0;
	// Number of packets missed
	int num_losses = 0;
	// Maximum number of packets taken in account
	int window_size = (q4s_session)->window_size_packetloss;
	if (window_size == 0 || window_size > MAXNUMSAMPLES - 100) {
		window_size = MAXNUMSAMPLES - 100;
	}

  // Discovers current value of num_samples and num_losses
	for (int i=0; i < sizeof((q4s_session)->packetloss_samples); i++) {
		// Losses are represented with a "1"
		if ((q4s_session)->packetloss_samples[i] > 0) {
			num_losses++;
		} else if ((q4s_session)->packetloss_samples[i] == 0) {
			break;
		}
		// Packets received are represented with a "-1"
		num_samples++;
	}

  // If there are no packets lost in this measure
	if (lost_packets == 0) {
		// Next sample is set to -1
		(q4s_session)->packetloss_samples[num_samples] = -1;
		num_samples++;

		// If window_size has been overcome
		if (num_samples > window_size) {
			pthread_mutex_lock(&mutex_print);
			printf("\nWindow size reached for packetloss measure\n");
			pthread_mutex_unlock(&mutex_print);
			// If first sample is a loss, num_losses is decreased by 1
			if ((q4s_session)->packetloss_samples[0] > 0) {
				num_losses--;
			}
			// Array of samples is moved one position to the left
			for (int k=0; k < num_samples - 1; k++) {
				(q4s_session)->packetloss_samples[k] = (q4s_session)->packetloss_samples[k+1];
			}
			// The old last position is put to 0
			(q4s_session)->packetloss_samples[num_samples - 1] = 0;
			// Decreases num_samples by 1
			num_samples--;
		}

	} else {  // if there are losses
		// If window_size is to be overcome
		if (num_samples+lost_packets+1 > window_size) {
			pthread_mutex_lock(&mutex_print);
			printf("\nWindow size reached for packetloss measure\n");
			pthread_mutex_unlock(&mutex_print);
		}
		// Lost samples are set to 1
		for (int j=0; j < lost_packets; j++) {
			// If window_size has been overcome
			if (num_samples >= window_size) {
				// If first sample is a loss, num_losses is decreased by 1
				if ((q4s_session)->packetloss_samples[0] > 0) {
					num_losses--;
				}
				// Array of samples is moved one position to the left
				for (int k=0; k < num_samples - 1; k++) {
					(q4s_session)->packetloss_samples[k] = (q4s_session)->packetloss_samples[k+1];
				}
				// The old last position is put to 0
				(q4s_session)->packetloss_samples[num_samples - 1] = 0;
				// Decreases num_samples by 1
				num_samples--;
			}
      (q4s_session)->packetloss_samples[num_samples] = 1;
			num_losses++;
			num_samples++;
		}
		// If window_size has been overcome
		if (num_samples >= window_size) {
 		  // If first sample is a loss, num_losses is decreased by 1
 			if ((q4s_session)->packetloss_samples[0] > 0) {
 				num_losses--;
 			}
 			// Array of samples is moved one position to the left
 			for (int k=0; k < num_samples - 1; k++) {
 				(q4s_session)->packetloss_samples[k] = (q4s_session)->packetloss_samples[k+1];
 			}
 			// The old last position is put to 0
 			(q4s_session)->packetloss_samples[num_samples - 1] = 0;
 			// Decreases num_samples by 1
 			num_samples--;
	  }
		// Last sample is set to -1 (received packet)
		(q4s_session)->packetloss_samples[num_samples] = -1;
		num_samples++;
	}

  // Calculates updated value of packetloss
  (q4s_session)->packetloss_measure_server = ((float) num_losses / (float) num_samples);
	pthread_mutex_lock(&mutex_print);
	printf("Updated value of packetloss measure: %.2f\n", (q4s_session)->packetloss_measure_server);
  pthread_mutex_unlock(&mutex_print);
}



// CONNECTION FUNCTIONS

// Start up of Q4S server for TCP connections
void start_listening_TCP() {
	// Creates socket
	if ((server_socket_TCP = socket (AF_INET, SOCK_STREAM, 0)) < 0){
    pthread_mutex_lock(&mutex_print);
		printf("Error when assigning the TCP socket\n");
    pthread_mutex_unlock(&mutex_print);
		return;
	}

	// Configures server for the connection
	server_TCP.sin_family = AF_INET; // protocol assignment
	server_TCP.sin_port = htons(HOST_PORT_TCP); // port assignment
  server_TCP.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP address assignment (automatic)
	memset(server_TCP.sin_zero, '\0', 8); // fills padding with 0s

	// Assigns port to the socket
	if (bind(server_socket_TCP, (struct sockaddr*)&server_TCP, sizeof(server_TCP)) < 0) {
    pthread_mutex_lock(&mutex_print);
		printf("Error when associating port with TCP socket: %s\n", strerror(errno));
    pthread_mutex_unlock(&mutex_print);
		close(server_socket_TCP);
		return;
	}
	if (listen(server_socket_TCP, MAX_CONNECTIONS) < 0 ) { // listening
    pthread_mutex_lock(&mutex_print);
		printf("Error in listen(): %s\n", strerror(errno));
    pthread_mutex_unlock(&mutex_print);
		close(server_socket_TCP);
    return;
	}
  pthread_mutex_lock(&mutex_print);
	printf("Listening in %s:%d for TCP\n", inet_ntoa(server_TCP.sin_addr), ntohs(server_TCP.sin_port));
  pthread_mutex_unlock(&mutex_print);
}

// Start up of Q4S server for TCP connections
void start_listening_UDP() {
	// Creates socket
	if ((server_socket_UDP = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
    pthread_mutex_lock(&mutex_print);
		printf("Error when assigning the UDP socket\n");
    pthread_mutex_unlock(&mutex_print);
		return;
	}

	// Configures server parameters for UDP socket
	server_UDP.sin_family = AF_INET; // protocol assignment
	server_UDP.sin_port = htons(HOST_PORT_UDP); // port assignment
  server_UDP.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP address assignment (automatic)
	memset(server_UDP.sin_zero, '\0', 8); // fills padding with 0s

	// Assigns port to the socket
	if (bind(server_socket_UDP, (struct sockaddr*)&server_UDP, sizeof(server_UDP)) < 0) {
    pthread_mutex_lock(&mutex_print);
		printf("Error when associating port with UDP socket: %s\n", strerror(errno));
    pthread_mutex_unlock(&mutex_print);
		close(server_socket_UDP);
		return;
	}

  pthread_mutex_lock(&mutex_print);
	printf("Listening in %s:%d for UDP\n", inet_ntoa(server_UDP.sin_addr), ntohs(server_UDP.sin_port));
  pthread_mutex_unlock(&mutex_print);
}


// Wait of client connection
void wait_new_connection(){
  longc = sizeof(client_TCP); // variable with size of the struct
	// Waits for a connection
	client_socket_TCP = accept(server_socket_TCP, (struct sockaddr *)&client_TCP, &longc);
	if (client_socket_TCP < 0) {
    pthread_mutex_lock(&mutex_print);
		printf("Error when accepting TCP traffic: %s\n", strerror(errno));
    pthread_mutex_unlock(&mutex_print);
		close(server_socket_TCP);
		exit(0);
		return;
	}
	pthread_mutex_lock(&mutex_flags);
	flags |= FLAG_NEW_CONNECTION;
	pthread_mutex_unlock(&mutex_flags);

  pthread_mutex_lock(&mutex_print);
	printf("Connected with %s:%d\n", inet_ntoa(client_TCP.sin_addr), htons(client_TCP.sin_port));
  pthread_mutex_unlock(&mutex_print);
}

// Delivery of Q4S message to a Q4S client using TCP
void send_message_TCP (char prepared_message[MAXDATASIZE]) {
	pthread_mutex_lock(&mutex_buffer_TCP);
	memset(buffer_TCP, '\0', sizeof(buffer_TCP));
	// Copies the message into the buffer
	strncpy (buffer_TCP, prepared_message, MAXDATASIZE);
	pthread_mutex_unlock(&mutex_buffer_TCP);
	// Sends the message to the server using the TCP socket assigned
	if (send(client_socket_TCP, buffer_TCP, MAXDATASIZE, 0) < 0) {
		pthread_mutex_lock(&mutex_print);
		printf("Error when sending TCP data: %s\n", strerror(errno));
		pthread_mutex_unlock(&mutex_print);
		close(client_socket_TCP);
		exit(0);
		return;
	}
	pthread_mutex_lock(&mutex_buffer_TCP);
	memset(buffer_TCP, '\0', sizeof(buffer_TCP));
  pthread_mutex_unlock(&mutex_buffer_TCP);

	pthread_mutex_lock(&mutex_print);
	printf("\nI have sent:\n%s\n", prepared_message);
	pthread_mutex_unlock(&mutex_print);
}

// Delivery of Q4S message to a Q4S client using UDP
void send_message_UDP (char prepared_message[MAXDATASIZE]) {
	int slen = sizeof(client_UDP);

	pthread_mutex_lock(&mutex_buffer_UDP);
	memset(buffer_UDP, '\0', sizeof(buffer_UDP));
	// Copies the message into the buffer
	strncpy (buffer_UDP, prepared_message, MAXDATASIZE);
	pthread_mutex_unlock(&mutex_buffer_UDP);

	char copy_buffer_UDP[MAXDATASIZE];
	memset(copy_buffer_UDP, '\0', sizeof(copy_buffer_UDP));
	strncpy (copy_buffer_UDP, prepared_message, MAXDATASIZE);

	char *start_line;
	start_line = strtok(copy_buffer_UDP, "\n"); // stores first line of message
	// If it is a Q4S PING, stores the current time
	if (strcmp(start_line, "PING q4s://www.example.com Q4S/1.0") == 0) {
		int result;
		if ((&tm_latency_start1)->seq_number == -1) {
			result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_start1)->tm));
			(&tm_latency_start1)->seq_number = (&q4s_session)->seq_num_server;
		} else if ((&tm_latency_start2)->seq_number == -1) {
			result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_start2)->tm));
			(&tm_latency_start2)->seq_number = (&q4s_session)->seq_num_server;
		} else if ((&tm_latency_start3)->seq_number == -1) {
			result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_start3)->tm));
			(&tm_latency_start3)->seq_number = (&q4s_session)->seq_num_server;
		} else if ((&tm_latency_start4)->seq_number == -1) {
			result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_start4)->tm));
			(&tm_latency_start4)->seq_number = (&q4s_session)->seq_num_server;
		} else {
			int min_seq_num = min(min((&tm_latency_start1)->seq_number, (&tm_latency_start2)->seq_number),
		    min((&tm_latency_start3)->seq_number, (&tm_latency_start4)->seq_number));
				if (min_seq_num == (&tm_latency_start1)->seq_number) {
					result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_start1)->tm));
					(&tm_latency_start1)->seq_number = (&q4s_session)->seq_num_server;
				} else if (min_seq_num == (&tm_latency_start2)->seq_number) {
					result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_start2)->tm));
					(&tm_latency_start2)->seq_number = (&q4s_session)->seq_num_server;
				} else if (min_seq_num == (&tm_latency_start3)->seq_number) {
					result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_start3)->tm));
					(&tm_latency_start3)->seq_number = (&q4s_session)->seq_num_server;
				} else if (min_seq_num == (&tm_latency_start4)->seq_number) {
					result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_start4)->tm));
					(&tm_latency_start4)->seq_number = (&q4s_session)->seq_num_server;
				}
		}

		if (result < 0) {
	    pthread_mutex_lock(&mutex_print);
			printf("Error in clock_gettime(): %s\n", strerror(errno));
	    pthread_mutex_unlock(&mutex_print);
			close(server_socket_UDP);
	    return;
		}
	}
	// Sends the message to the server using the UDP socket assigned
	if (sendto(server_socket_UDP, buffer_UDP, MAXDATASIZE, 0, (struct sockaddr *) &client_UDP, slen) < 0) {
		pthread_mutex_lock(&mutex_print);
		printf("Error when sending UDP data: %s\n", strerror(errno));
		pthread_mutex_unlock(&mutex_print);
		close(server_socket_UDP);
		exit(0);
		return;
	}
	pthread_mutex_lock(&mutex_buffer_UDP);
	memset(buffer_UDP, '\0', sizeof(buffer_UDP));
	pthread_mutex_unlock(&mutex_buffer_UDP);

	memset(copy_buffer_UDP, '\0', sizeof(copy_buffer_UDP));

	pthread_mutex_lock(&mutex_print);
	printf("\nI have sent:\n%s\n", prepared_message);
	pthread_mutex_unlock(&mutex_print);
}

// Reception of Q4S messages (TCP socket)
// Thread function that checks if any message has arrived
void *thread_receives_TCP() {
	// Copy of the buffer (to avoid buffer modification)
	char copy_buffer_TCP[MAXDATASIZE];
	memset(copy_buffer_TCP, '\0', sizeof(copy_buffer_TCP));
	char copy_buffer_TCP_2[MAXDATASIZE];
	memset(copy_buffer_TCP_2, '\0', sizeof(copy_buffer_TCP_2));
	while(1) {
	  // If error occurs when receiving
	  if (recv(client_socket_TCP, buffer_TCP, MAXDATASIZE, MSG_WAITALL) < 0) {
			pthread_mutex_lock(&mutex_print);
		  printf("Error when receiving the data: %s\n", strerror(errno));
			pthread_mutex_unlock(&mutex_print);
		  close(server_socket_TCP);
		  return NULL;
    }
		pthread_mutex_lock(&mutex_buffer_TCP);
		// If nothing has been received
		if (strlen(buffer_TCP) == 0) {
			pthread_mutex_unlock(&mutex_buffer_TCP);
		  return NULL;
    }
		strcpy(copy_buffer_TCP, buffer_TCP);
		strcpy(copy_buffer_TCP_2, buffer_TCP);
		memset(buffer_TCP, '\0', sizeof(buffer_TCP));
		pthread_mutex_unlock(&mutex_buffer_TCP);
		// Auxiliary variable to identify type of Q4S message
		char *start_line;
		start_line = strtok(copy_buffer_TCP, "\n"); // stores first line of message
	  if (strcmp(start_line, "BEGIN q4s://www.example.com Q4S/1.0") == 0) {
			// Stores the received message to be analized later
			pthread_mutex_lock(&mutex_session);
			store_message(copy_buffer_TCP_2, &q4s_session.message_received);
			pthread_mutex_unlock(&mutex_session);

			pthread_mutex_lock(&mutex_print);
			printf("\nI have received a Q4S BEGIN!\n");
			pthread_mutex_unlock(&mutex_print);

		  pthread_mutex_lock(&mutex_flags);
		  flags |= FLAG_RECEIVE_BEGIN;
		  pthread_mutex_unlock(&mutex_flags);
	  } else if (strcmp(start_line, "READY q4s://www.example.com Q4S/1.0") == 0) {
			// Stores the received message to be analized later
			pthread_mutex_lock(&mutex_session);
			store_message(copy_buffer_TCP_2, &q4s_session.message_received);
			if (strstr((&q4s_session.message_received)->header, "Stage: 0") != NULL) {
				pthread_mutex_unlock(&mutex_session);

				pthread_mutex_lock(&mutex_print);
			  printf("\nI have received a Q4S READY 0!\n");
				pthread_mutex_unlock(&mutex_print);

				pthread_mutex_lock(&mutex_flags);
			  flags |= FLAG_RECEIVE_READY0;
			  pthread_mutex_unlock(&mutex_flags);
			} else if (strstr((&q4s_session.message_received)->header, "Stage: 1") != NULL) {
				pthread_mutex_unlock(&mutex_session);

				pthread_cancel(timer_ping);

				pthread_mutex_lock(&mutex_print);
			  printf("\nI have received a Q4S READY 1!\n");
				pthread_mutex_unlock(&mutex_print);

				pthread_mutex_lock(&mutex_flags);
			  flags |= FLAG_RECEIVE_READY1;
			  pthread_mutex_unlock(&mutex_flags);
			} else {
				pthread_mutex_unlock(&mutex_session);

				pthread_mutex_lock(&mutex_print);
			  printf("\nI have received a Q4S READY without stage specified\n");
				pthread_mutex_unlock(&mutex_print);
		  }
		} else if (strcmp(start_line, "Q4S/1.0 200 OK") == 0) {
			// Stores the received message to be analized later
			pthread_mutex_lock(&mutex_session);
			store_message(copy_buffer_TCP_2, &q4s_session.message_received);
			pthread_mutex_unlock(&mutex_session);

			pthread_mutex_lock(&mutex_print);
			printf("I have received a Q4S 200 OK!\n");
			pthread_mutex_unlock(&mutex_print);

			pthread_mutex_lock(&mutex_flags);
			flags |= FLAG_RECEIVE_OK;
			pthread_mutex_unlock(&mutex_flags);
		// If it is a Q4S BWIDTH message
	  } else if (strcmp(start_line, "CANCEL q4s://www.example.com Q4S/1.0") == 0) {
			// Stores the received message to be analized later
			pthread_mutex_lock(&mutex_session);
			store_message(copy_buffer_TCP_2, &q4s_session.message_received);
			pthread_mutex_unlock(&mutex_session);

			pthread_mutex_lock(&mutex_print);
			printf("\nI have received a Q4S CANCEL!\n");
			pthread_mutex_unlock(&mutex_print);

		  pthread_mutex_lock(&mutex_flags);
		  flags |= FLAG_RECEIVE_CANCEL;
		  pthread_mutex_unlock(&mutex_flags);
		} else {
			pthread_mutex_lock(&mutex_print);
		  printf("\nI have received an unidentified message\n");
      pthread_mutex_unlock(&mutex_print);
		}
		memset(copy_buffer_TCP, '\0', sizeof(copy_buffer_TCP));
		memset(copy_buffer_TCP_2, '\0', sizeof(copy_buffer_TCP_2));
  }
}

// Reception of Q4S messages (UDP socket)
// Thread function that checks if any message has arrived
void *thread_receives_UDP() {
	int slen = sizeof(client_UDP);
	// Copy of the buffer (to avoid buffer modification)
	char copy_buffer_UDP[MAXDATASIZE];
	memset(copy_buffer_UDP, '\0', sizeof(copy_buffer_UDP));
	char copy_buffer_UDP_2[MAXDATASIZE];
	memset(copy_buffer_UDP_2, '\0', sizeof(copy_buffer_UDP_2));
	while(1) {
	  // If error occurs when receiving
	  if (recvfrom(server_socket_UDP, buffer_UDP, MAXDATASIZE, MSG_WAITALL,
			(struct sockaddr *) &client_UDP, &slen) < 0) {
			pthread_mutex_lock(&mutex_print);
		  printf("Error when receiving the data: %s\n", strerror(errno));
			pthread_mutex_unlock(&mutex_print);
		  close(server_socket_UDP);
		  return NULL;
    }
		pthread_mutex_lock(&mutex_buffer_UDP);
		// If nothing has been received
		if (strlen(buffer_UDP) == 0) {
			pthread_mutex_unlock(&mutex_buffer_UDP);
		  return NULL;
    }
		strcpy(copy_buffer_UDP, buffer_UDP);
		strcpy(copy_buffer_UDP_2, buffer_UDP);
		memset(buffer_UDP, '\0', sizeof(buffer_UDP));
		pthread_mutex_unlock(&mutex_buffer_UDP);
		// Auxiliary variable to identify type of Q4S message
		char *start_line;
		start_line = strtok(copy_buffer_UDP, "\n"); // stores first line of message
	  if (strcmp(start_line, "PING q4s://www.example.com Q4S/1.0") == 0) {
      char field_seq_num[20];
			strcpy(field_seq_num, "Sequence-Number: ");
			char header[500];
			pthread_mutex_lock(&mutex_session);
			// Stores the received message to be analized later
			store_message(copy_buffer_UDP_2, &q4s_session.message_received);
			strcpy(header, (&q4s_session.message_received)->header);
			pthread_mutex_unlock(&mutex_session);

			if (strstr(header, field_seq_num) != NULL) {
				pthread_mutex_lock(&mutex_print);
				printf("\nI have received a Q4S PING!\n");
				pthread_mutex_unlock(&mutex_print);

				int seq_num_before;
				int seq_num_after;
				int num_losses;

				pthread_mutex_lock(&mutex_session);
				seq_num_before = (&q4s_session)->seq_num_client;
				// Stores parameters of the message (included Sequence Number)
				store_parameters(&q4s_session, &(q4s_session.message_received));
				seq_num_after = (&q4s_session)->seq_num_client;
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
					close(server_socket_UDP);
				}
				pthread_mutex_unlock(&mutex_tm_jitter);

				pthread_mutex_lock(&mutex_flags);
				flags |= FLAG_RECEIVE_PING;
				pthread_mutex_unlock(&mutex_flags);
			} else {
				pthread_mutex_lock(&mutex_print);
				printf("\nI have received a Q4S PING without sequence number specified\n");
				pthread_mutex_unlock(&mutex_print);
			}
		} else if (strcmp(start_line, "Q4S/1.0 200 OK") == 0) {
			char field_seq_num[20];
			strcpy(field_seq_num, "Sequence-Number: ");
			char header[500];
			pthread_mutex_lock(&mutex_session);
			// Stores the received message to be analized later
			store_message(copy_buffer_UDP_2, &q4s_session.message_received);
			strcpy(header, (&q4s_session.message_received)->header);
			pthread_mutex_unlock(&mutex_session);

			if (strstr(header, field_seq_num) != NULL) {
				pthread_mutex_lock(&mutex_tm_latency);
				int result = clock_gettime(CLOCK_REALTIME, (&(&tm_latency_end)->tm));
				if (result < 0) {
			    pthread_mutex_lock(&mutex_print);
					printf("Error in clock_gettime(): %s\n", strerror(errno));
			    pthread_mutex_unlock(&mutex_print);
					close(server_socket_UDP);
				}
				pthread_mutex_unlock(&mutex_tm_latency);

				pthread_mutex_lock(&mutex_print);
				printf("\nI have received a Q4S 200 OK!\n");
				pthread_mutex_unlock(&mutex_print);

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
			} else {
				pthread_mutex_lock(&mutex_print);
			  printf("\nI have received a Q4S 200 OK without sequence number specified\n");
				pthread_mutex_unlock(&mutex_print);
			}
		// If it is a Q4S BWIDTH message
		} else if (strcmp(start_line, "BWIDTH q4s://www.example.com Q4S/1.0") == 0) {
			char field_seq_num[20];
			strcpy(field_seq_num, "Sequence-Number: ");
			char header[500];
			pthread_mutex_lock(&mutex_session);
			// Stores the received message to be analized later
			store_message(copy_buffer_UDP_2, &q4s_session.message_received);
			strcpy(header, (&q4s_session.message_received)->header);
			pthread_mutex_unlock(&mutex_session);

			if (strstr(header, field_seq_num) != NULL) {
				pthread_mutex_lock(&mutex_print);
			  printf("\nI have received a Q4S BWIDTH!\n");
	      pthread_mutex_unlock(&mutex_print);

				int seq_num_before;
				int seq_num_after;
				int num_losses;

				pthread_mutex_lock(&mutex_session);
				num_bwidth_received++;
				seq_num_before = (&q4s_session)->seq_num_client;
	      // Stores parameters of the message (included Sequence Number)
				store_parameters(&q4s_session, &(q4s_session.message_received));
				seq_num_after = (&q4s_session)->seq_num_client;
				num_losses = seq_num_after - (seq_num_before + 1);
				num_packet_lost = num_losses;
				pthread_mutex_unlock(&mutex_session);

				pthread_mutex_lock(&mutex_flags);
			  flags |= FLAG_RECEIVE_BWIDTH;
			  pthread_mutex_unlock(&mutex_flags);

			} else {
				pthread_mutex_lock(&mutex_print);
			  printf("\nI have received a Q4S BWIDTH without sequence number specified\n");
				pthread_mutex_unlock(&mutex_print);
			}

		} else {
			pthread_mutex_lock(&mutex_print);
		  printf("\nI have received an unidentified message\n");
      pthread_mutex_unlock(&mutex_print);
		}
		memset(copy_buffer_UDP, '\0', sizeof(copy_buffer_UDP));
		memset(copy_buffer_UDP_2, '\0', sizeof(copy_buffer_UDP_2));
  }
}


// TIMER FUNCTIONS

// Activates a flag when ping timeout has occurred
void *ping_timeout() {
	int ping_clk;
	while(1) {
		pthread_mutex_lock(&mutex_session);
		ping_clk = q4s_session.ping_clk_negotiation;
		pthread_mutex_unlock(&mutex_session);

    delay(ping_clk);

		pthread_mutex_lock(&mutex_flags);
		flags |= FLAG_TEMP_PING_0;
		pthread_mutex_unlock(&mutex_flags);
	}
}

// Activates a flag when bwidth timeout has occurred
void *bwidth_timeout() {
	int bwidth_clk; // time established for BWIDTH delivery
	int messages_per_ms; // BWIDTH messages to send each 1 ms
	int ms_per_message[11]; // intervals of ms for sending BWIDTH messages
	int ms_delayed; // ms passed

  while (1) {
		// Initializes parameters needed
		ms_delayed = 0;
		pthread_mutex_lock(&mutex_session);
		bwidth_clk = q4s_session.bwidth_clk;
		messages_per_ms = q4s_session.bwidth_messages_per_ms;
		for (int i = 0; i < sizeof(ms_per_message); i++) {
			if (q4s_session.ms_per_bwidth_message[i] > 0) {
				ms_per_message[i] = q4s_session.ms_per_bwidth_message[i];
			} else {
				break;
			}
		}
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_print);
		printf("\nBwidth clk: %d", bwidth_clk);
		printf("\nMessages per ms: %d", messages_per_ms);
		for (int i = 0; i < sizeof(ms_per_message); i++) {
			if (ms_per_message[i] > 0) {
				printf("\nMs per message (%d): %d\n", i+1, ms_per_message[i]);
			} else {
				break;
			}
		}
		pthread_mutex_unlock(&mutex_print);

		// Start of Q4S BWIDTH delivery
		while(1) {
		  // Sends a number specified of 1kB BWIDTH messages per 1 ms
		  int j = 0;
		  while (j < messages_per_ms) {
			  pthread_mutex_lock(&mutex_flags);
			  flags |= FLAG_TEMP_BWIDTH;
			  pthread_mutex_unlock(&mutex_flags);
			  j++;
		  }
			// Sends (when appropiate) 1 or more extra BWIDTH message(s) of 1kB
			for (int k = 1; k < sizeof(ms_per_message); k++) {
				if (ms_per_message[k] > 0 && ms_delayed % ms_per_message[k] == 0) {
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_TEMP_BWIDTH;
					pthread_mutex_unlock(&mutex_flags);
				}
			}
			// When the interval bwidth_clk has finished, delivery is completed
			// Now we update the value of bandwidth measured and send a last Q4S BWIDTH
			if (ms_delayed == bwidth_clk) {
				pthread_mutex_lock(&mutex_session);
				(&q4s_session)->bw_measure_server = (num_bwidth_received * 8000 / bwidth_clk);
        pthread_mutex_unlock(&mutex_session);
				num_bwidth_received = 0;
				pthread_mutex_lock(&mutex_flags);
				flags |= FLAG_TEMP_BWIDTH;
				pthread_mutex_unlock(&mutex_flags);
				break;
			}
			// Delays 1 ms
			delay(ms_per_message[0]);
			ms_delayed++;
		}
	}

}

// Activates a flag when ping timeout has occurred
void *alert_pause_timeout() {
	int pause_interval = q4s_session.alert_pause;
	pthread_mutex_lock(&mutex_session);
	q4s_session.alert_pause_activated = true;
	pthread_mutex_unlock(&mutex_session);

	pthread_mutex_lock(&mutex_print);
	printf("\nAlert pause started\n");
	pthread_mutex_unlock(&mutex_print);

  delay(pause_interval);

	pthread_mutex_lock(&mutex_session);
	q4s_session.alert_pause_activated = false;
	pthread_mutex_unlock(&mutex_session);

	pthread_mutex_lock(&mutex_print);
	printf("\nAlert pause finished\n");
	pthread_mutex_unlock(&mutex_print);
}


// CHECK FUNCTIONS OF STATE MACHINE

// Checks if new client has connected to the server
int check_new_connection (fsm_t* this) {
	int result;
	wait_new_connection();
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_NEW_CONNECTION);
  pthread_mutex_unlock(&mutex_flags);
  return result;
}

// Checks if client wants to start a q4s session (Q4S BEGIN received)
int check_receive_begin (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_RECEIVE_BEGIN);
  pthread_mutex_unlock(&mutex_flags);
  return result;
}

// Checks if client wants to start stage 0 (Q4S READY 0 is received)
int check_receive_ready0 (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_RECEIVE_READY0);
  pthread_mutex_unlock(&mutex_flags);
  return result;
}

// Checks if client wants to start stage 1 (Q4S READY 1 is received)
int check_receive_ready1 (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_RECEIVE_READY1);
  pthread_mutex_unlock(&mutex_flags);
	if (result > 0) {
		pthread_cancel(receive_UDP_thread);
	}
  return result;
}

// Checks if q4s server has received a Q4S PING from client
int check_receive_ping (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_RECEIVE_PING);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if a Q4S 200 OK message has been received from the client
int check_receive_ok (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
  pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_RECEIVE_OK);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if ping timeout has occurred
int check_temp_ping_0 (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_TEMP_PING_0);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if an alert has to be triggered
int check_alert (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_ALERT);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if q4s server has to start Q4S BWIDTH delivery
int check_init_bwidth (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_INIT_BWIDTH);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if bwidth timeout has occurred
int check_temp_bwidth (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_TEMP_BWIDTH);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if q4s server has received a Q4S BWIDTH from client
int check_receive_bwidth (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_RECEIVE_BWIDTH);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if client wants to cancel a q4s session (Q4S CANCEL received)
int check_receive_cancel (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_RECEIVE_CANCEL);
  pthread_mutex_unlock(&mutex_flags);
  return result;
}

// Checks if Actuator has already released the resources
int check_released (fsm_t* this) {
	int result;
  // Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_RELEASED);
  pthread_mutex_unlock(&mutex_flags);
  return result;
}


// ACTION FUNCTIONS OF STATE MACHINE

// Prepares for Q4S session
void Setup (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	// Puts every FLAG to 0
	flags = 0;
	pthread_mutex_unlock(&mutex_flags);

	pthread_mutex_lock(&mutex_session);
	// Initialize auxiliary variables
	num_packet_lost = 0;
	num_ping = 0;
	num_latency_measures_client = 0;
	num_jitter_measures_client = 0;
	num_packetloss_measures_client = 0;
	num_bwidth_received = 0;
	num_latency_measures_server = 0;
	num_jitter_measures_server = 0;
	num_packetloss_measures_server = 0;
	// Initialize session variables
  q4s_session.session_id = -1;
	q4s_session.seq_num_client = -1;
	q4s_session.seq_num_server = 0;
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
	q4s_session.packetloss_measure_server = -1;
	pthread_mutex_unlock(&mutex_session);

	pthread_mutex_lock(&mutex_print);
	printf("\nWhenever you want to set new parameters in a Q4S 200 OK, press 'n'\n");
  pthread_mutex_unlock(&mutex_print);

  // Throws a thread to check the arrival of Q4S messages using TCP
	pthread_create(&receive_TCP_thread, NULL, (void*)thread_receives_TCP, NULL);
}

// Creates and sends a Q4S 200 OK message to the client
void Respond_ok (fsm_t* fsm) {
	char request_method[20];
	pthread_mutex_lock(&mutex_flags);
	if (flags & FLAG_RECEIVE_BEGIN) {
		flags &= ~FLAG_RECEIVE_BEGIN;
	  pthread_mutex_unlock(&mutex_flags);
		// Indicates request method received
		strcpy(request_method, "BEGIN");

		pthread_mutex_lock(&mutex_session);
		// Stores parameters of the Q4S BEGIN (if present)
		store_parameters(&q4s_session, &(q4s_session.message_received));
		// Fills q4s_session.message_to_send with the Q4S 200 OK parameters
		create_200 (&q4s_session.message_to_send, request_method, set_new_parameters);
		set_new_parameters = false;
		// Stores parameters added to the message
		store_parameters(&q4s_session, &(q4s_session.message_to_send));
	  // Converts q4s_session.message_to_send into a message with correct format (prepared_message)
		prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
		// Sends the message to the Q4S client
		send_message_TCP(q4s_session.prepared_message);
		pthread_mutex_unlock(&mutex_session);
	} else if (flags & FLAG_RECEIVE_READY0) {
		// Puts every FLAG to 0
		flags = 0;
		pthread_mutex_unlock(&mutex_flags);
		// Indicates request method received
		strcpy(request_method, "READY0");
		// Throws a thread to check the arrival of Q4S messages using UDP
		pthread_create(&receive_UDP_thread, NULL, (void*)thread_receives_UDP, NULL);

		pthread_mutex_lock(&mutex_tm_latency);
		(&tm_latency_start1)->seq_number = -1;
		(&tm_latency_start2)->seq_number = -1;
		(&tm_latency_start3)->seq_number = -1;
		(&tm_latency_start4)->seq_number = -1;
		(&tm_latency_end)->seq_number = -1;
		pthread_mutex_unlock(&mutex_tm_latency);

		pthread_mutex_lock(&mutex_session);
		// Initialize auxiliary variables
		num_packet_lost = 0;
		num_ping = 0;
		num_latency_measures_client = 0;
		num_jitter_measures_client = 0;
		num_packetloss_measures_client = 0;
		num_bwidth_received = 0;
		num_latency_measures_server = 0;
		num_jitter_measures_server = 0;
		num_packetloss_measures_server = 0;
		// Initialize session variables
		q4s_session.seq_num_server = 0;
		q4s_session.seq_num_client = -1;
		q4s_session.packetloss_measure_client = -1;
		memset((&q4s_session)->latency_samples, 0, MAXNUMSAMPLES);
		memset((&q4s_session)->elapsed_time_samples, 0, MAXNUMSAMPLES);
		memset((&q4s_session)->packetloss_samples, 0, MAXNUMSAMPLES);
		memset((&q4s_session)->bw_samples, 0, MAXNUMSAMPLES);
		// Fills q4s_session.message_to_send with the Q4S 200 OK parameters
		create_200 (&q4s_session.message_to_send, request_method, set_new_parameters);
		set_new_parameters = false;
		// Stores parameters added to the message
		store_parameters(&q4s_session, &(q4s_session.message_to_send));
	  // Converts q4s_session.message_to_send into a message with correct format (prepared_message)
		prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
		// Sends the message to the Q4S client
		send_message_TCP(q4s_session.prepared_message);
		pthread_mutex_unlock(&mutex_session);

	} else if (flags & FLAG_RECEIVE_READY1) {
		// Puts every FLAG to 0
		flags = 0;
		pthread_mutex_unlock(&mutex_flags);
		// Indicates request method received
		strcpy(request_method, "READY1");
		// Throws a thread to check the arrival of Q4S messages using UDP
		pthread_create(&receive_UDP_thread, NULL, (void*)thread_receives_UDP, NULL);

		pthread_mutex_lock(&mutex_session);
		// Initialize auxiliary variables
		num_packet_lost = 0;
		num_ping = 0;
		num_latency_measures_client = 0;
		num_jitter_measures_client = 0;
		num_packetloss_measures_client = 0;
		num_bwidth_received = 0;
		num_latency_measures_server = 0;
		num_jitter_measures_server = 0;
		num_packetloss_measures_server = 0;
		// Initialize session variables
		q4s_session.seq_num_server = 0;
		q4s_session.seq_num_client = -1;
		q4s_session.packetloss_measure_client = -1;
		memset((&q4s_session)->latency_samples, 0, MAXNUMSAMPLES);
		memset((&q4s_session)->elapsed_time_samples, 0, MAXNUMSAMPLES);
		memset((&q4s_session)->packetloss_samples, 0, MAXNUMSAMPLES);
		memset((&q4s_session)->bw_samples, 0, MAXNUMSAMPLES);
		// Fills q4s_session.message_to_send with the Q4S 200 OK parameters
		create_200 (&q4s_session.message_to_send, request_method, set_new_parameters);
		set_new_parameters = false;
		// Stores parameters added to the message
		store_parameters(&q4s_session, &(q4s_session.message_to_send));
	  // Converts q4s_session.message_to_send into a message with correct format (prepared_message)
		prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
		// Sends the message to the Q4S client
		send_message_TCP(q4s_session.prepared_message);
		pthread_mutex_unlock(&mutex_session);
		pthread_mutex_lock(&mutex_flags);
		flags |= FLAG_INIT_BWIDTH;
		pthread_mutex_unlock(&mutex_flags);

	} else if (flags & FLAG_RECEIVE_PING) {
		flags &= ~FLAG_RECEIVE_PING;
		pthread_mutex_unlock(&mutex_flags);

		pthread_mutex_lock(&mutex_session);
		int num_losses = num_packet_lost;
		num_packet_lost = 0;
		if ((&q4s_session)->packetloss_th[0] > 0) {
			if (num_losses > 0) {
				pthread_mutex_lock(&mutex_print);
				printf("\nLoss of %d Q4S PING(s) detected\n", num_losses);
				pthread_mutex_unlock(&mutex_print);
			}
			update_packetloss(&q4s_session, num_losses);
			num_packetloss_measures_server++;
		}
		// Initialize ping period if necessary
		if (q4s_session.ping_clk_negotiation <= 0) {
			q4s_session.ping_clk_negotiation = 100;
		}
		// Initialize ping period if necessary
		if (q4s_session.ping_clk_continuity <= 0) {
			q4s_session.ping_clk_continuity = 100;
		}
		pthread_mutex_unlock(&mutex_session);

		// Indicates request method received
		strcpy(request_method, "PING");
		// Starts timer for ping delivery
		pthread_create(&timer_ping, NULL, (void*)ping_timeout, NULL);
		pthread_mutex_lock(&mutex_session);
		// Fills q4s_session.message_to_send with the Q4S 200 OK parameters
		create_200 (&q4s_session.message_to_send, request_method, set_new_parameters);
    if (set_new_parameters) {
			// Stores parameters added to the message
			store_parameters(&q4s_session, &(q4s_session.message_to_send));
		}
		set_new_parameters = false;
	  // Converts q4s_session.message_to_send into a message with correct format (prepared_message)
		prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
		// Sends the message to the Q4S client
		send_message_UDP(q4s_session.prepared_message);
		pthread_mutex_unlock(&mutex_session);
	} else {
		pthread_mutex_unlock(&mutex_flags);
	}
}

// Sends a Q4S PING message
void Ping (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_TEMP_PING_0;
  pthread_mutex_unlock(&mutex_flags);


	pthread_mutex_lock(&mutex_session);
	// Fills q4s_session.message_to_send with the Q4S PING parameters
	create_ping(&q4s_session.message_to_send);
	// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
	prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
	// Sends the message to the Q4S client
	send_message_UDP(q4s_session.prepared_message);
  (&q4s_session)->seq_num_server++;
	pthread_mutex_unlock(&mutex_session);

}

// Updates Q4S measures and compares them with the thresholds
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
    if ((&q4s_session)->jitter_th[0] > 0) {
			pthread_mutex_unlock(&mutex_session);
			int elapsed_time;

			if (num_ping % 2 == 0 && num_ping > 0) {
				elapsed_time = ms_elapsed(tm1_jitter, tm2_jitter);

	      pthread_mutex_lock(&mutex_print);
				printf("Elapsed time stored: %d\n", elapsed_time);
	      pthread_mutex_unlock(&mutex_print);

	      pthread_mutex_lock(&mutex_session);
				update_jitter(&q4s_session, elapsed_time);
				pthread_mutex_unlock(&mutex_session);

			} else if (num_ping % 2 != 0 && num_ping > 1) {
				elapsed_time = ms_elapsed(tm2_jitter, tm1_jitter);

	      pthread_mutex_lock(&mutex_print);
				printf("Elapsed time stored: %d\n",elapsed_time);
	      pthread_mutex_unlock(&mutex_print);

				pthread_mutex_lock(&mutex_session);
				update_jitter(&q4s_session, elapsed_time);
				pthread_mutex_unlock(&mutex_session);
			}
		} else {
			pthread_mutex_unlock(&mutex_session);
		}
    pthread_mutex_unlock(&mutex_tm_jitter);

		pthread_mutex_lock(&mutex_session);
		if ((&q4s_session)->packetloss_th[0] > 0) {
			if (num_losses > 0) {
				pthread_mutex_lock(&mutex_print);
				printf("\nLoss of %d Q4S PING(s) detected\n", num_losses);
				pthread_mutex_unlock(&mutex_print);
			}
			update_packetloss(&q4s_session, num_losses);
			num_packetloss_measures_server++;
		}
		pthread_mutex_unlock(&mutex_session);

    pthread_mutex_lock(&mutex_session);
		if ((&q4s_session)->jitter_th[0] > 0 && (&q4s_session)->jitter_measure_server > (&q4s_session)->jitter_th[0]) {
			pthread_mutex_lock(&mutex_print);
			printf("Upstream jitter exceeds the threshold\n");
			pthread_mutex_unlock(&mutex_print);
			num_jitter_measures_server = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_flags);
				flags |= FLAG_ALERT;
				pthread_mutex_unlock(&mutex_flags);
			}
		} else if ((&q4s_session)->jitter_th[1] > 0 && (&q4s_session)->jitter_measure_client > (&q4s_session)->jitter_th[1]) {
			pthread_mutex_lock(&mutex_print);
			printf("Downstream jitter exceeds the threshold\n");
			pthread_mutex_unlock(&mutex_print);
			num_jitter_measures_client = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_flags);
				flags |= FLAG_ALERT;
				pthread_mutex_unlock(&mutex_flags);
			}
		}
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_session);
		if ((&q4s_session)->packetloss_th[0] > 0 && (&q4s_session)->packetloss_measure_server > (&q4s_session)->packetloss_th[0]) {
			pthread_mutex_lock(&mutex_print);
			printf("Upstream packetloss exceeds the threshold\n");
			pthread_mutex_unlock(&mutex_print);
			num_packetloss_measures_server = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_flags);
				flags |= FLAG_ALERT;
				pthread_mutex_unlock(&mutex_flags);
			}
		} else if ((&q4s_session)->packetloss_th[1] > 0 && (&q4s_session)->packetloss_measure_client > (&q4s_session)->packetloss_th[1]) {
			pthread_mutex_lock(&mutex_print);
			printf("Downstream packetloss exceeds the threshold\n");
			pthread_mutex_unlock(&mutex_print);
			num_packetloss_measures_client = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_flags);
				flags |= FLAG_ALERT;
				pthread_mutex_unlock(&mutex_flags);
			}
		}
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_session);
		// Fills q4s_session.message_to_send with the Q4S 200 OK parameters
		create_200(&q4s_session.message_to_send, "PING", set_new_parameters);
		set_new_parameters = false;
		// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
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

				pthread_mutex_lock(&mutex_print);
				printf("\nRTT is: %d ms, and latency is: %d ms\n", rtt, rtt/2);
				pthread_mutex_unlock(&mutex_print);

				pthread_mutex_lock(&mutex_session);
				update_latency(&q4s_session, rtt/2);
				pthread_mutex_unlock(&mutex_session);

				num_latency_measures_client++;
			} else if ((&tm_latency_start2)->seq_number == (&tm_latency_end)->seq_number) {
				int rtt = ms_elapsed((&tm_latency_start2)->tm, (&tm_latency_end)->tm);

				pthread_mutex_lock(&mutex_print);
				printf("\nRTT is: %d ms, and latency is: %d ms\n", rtt, rtt/2);
				pthread_mutex_unlock(&mutex_print);

				pthread_mutex_lock(&mutex_session);
				update_latency(&q4s_session, rtt/2);
				pthread_mutex_unlock(&mutex_session);

				num_latency_measures_client++;
			} else if ((&tm_latency_start3)->seq_number == (&tm_latency_end)->seq_number) {
				int rtt = ms_elapsed((&tm_latency_start3)->tm, (&tm_latency_end)->tm);

				pthread_mutex_lock(&mutex_print);
				printf("\nRTT is: %d ms, and latency is: %d ms\n", rtt, rtt/2);
				pthread_mutex_unlock(&mutex_print);

				pthread_mutex_lock(&mutex_session);
				update_latency(&q4s_session, rtt/2);
				pthread_mutex_unlock(&mutex_session);

				num_latency_measures_client++;
			} else if ((&tm_latency_start4)->seq_number == (&tm_latency_end)->seq_number) {
				int rtt = ms_elapsed((&tm_latency_start4)->tm, (&tm_latency_end)->tm);

				pthread_mutex_lock(&mutex_print);
				printf("\nRTT is: %d ms, and latency is: %d ms\n", rtt, rtt/2);
				pthread_mutex_unlock(&mutex_print);

				pthread_mutex_lock(&mutex_session);
				update_latency(&q4s_session, rtt/2);
				pthread_mutex_unlock(&mutex_session);

				num_latency_measures_client++;
			}
			pthread_mutex_unlock(&mutex_tm_latency);
		} else {
			pthread_mutex_unlock(&mutex_session);
		}

    pthread_mutex_lock(&mutex_session);
		if ((&q4s_session)->latency_th > 0 && (&q4s_session)->latency_measure_server > (&q4s_session)->latency_th) {
			pthread_mutex_lock(&mutex_print);
			printf("Latency measured by server exceeds the threshold\n");
			pthread_mutex_unlock(&mutex_print);
			num_latency_measures_server = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_flags);
				flags |= FLAG_ALERT;
				pthread_mutex_unlock(&mutex_flags);
			}
		} else if ((&q4s_session)->latency_th > 0 && (&q4s_session)->latency_measure_client > (&q4s_session)->latency_th) {
			pthread_mutex_lock(&mutex_print);
			printf("Latency measured by client exceeds the threshold\n");
			pthread_mutex_unlock(&mutex_print);
			num_latency_measures_client = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_flags);
				flags |= FLAG_ALERT;
				pthread_mutex_unlock(&mutex_flags);
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
    if ((&q4s_session)->packetloss_th[0] > 0) {
			if (num_losses > 0) {
				pthread_mutex_lock(&mutex_print);
				printf("\nLoss of %d Q4S BWIDTH(s) detected\n", num_losses);
				pthread_mutex_unlock(&mutex_print);
			}
			update_packetloss(&q4s_session, num_losses);
			num_packetloss_measures_server++;
		}
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_session);
		if ((&q4s_session)->packetloss_th[0] > 0 && (&q4s_session)->packetloss_measure_server > (&q4s_session)->packetloss_th[0]) {
			pthread_mutex_lock(&mutex_print);
			printf("Upstream packetloss exceeds the threshold\n");
			pthread_mutex_unlock(&mutex_print);
			num_packetloss_measures_server = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_flags);
				flags |= FLAG_ALERT;
				pthread_mutex_unlock(&mutex_flags);
			}
		} else if ((&q4s_session)->packetloss_th[1] > 0 && (&q4s_session)->packetloss_measure_client > (&q4s_session)->packetloss_th[1]) {
			pthread_mutex_lock(&mutex_print);
			printf("Downstream packetloss exceeds the threshold\n");
			pthread_mutex_unlock(&mutex_print);
			num_packetloss_measures_client = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_flags);
				flags |= FLAG_ALERT;
				pthread_mutex_unlock(&mutex_flags);
			}
		}
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_session);
		if ((&q4s_session)->bw_th[0] > 0 && (&q4s_session)->bw_measure_server > 0
		  && (&q4s_session)->bw_measure_server < (&q4s_session)->bw_th[0]) {
			pthread_mutex_lock(&mutex_print);
			printf("Upstream bandwidth doesn't reach the threshold\n");
			pthread_mutex_unlock(&mutex_print);
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_flags);
				flags |= FLAG_ALERT;
				pthread_mutex_unlock(&mutex_flags);
			}
		} else if ((&q4s_session)->bw_th[1] > 0 && (&q4s_session)->bw_measure_client > 0
		  && (&q4s_session)->bw_measure_client < (&q4s_session)->bw_th[1]) {
			pthread_mutex_lock(&mutex_print);
			printf("Downstream bandwidth doesn't reach the threshold\n");
			pthread_mutex_unlock(&mutex_print);
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_flags);
				flags |= FLAG_ALERT;
				pthread_mutex_unlock(&mutex_flags);
			}
		}
		pthread_mutex_unlock(&mutex_session);

	} else {
    pthread_mutex_unlock(&mutex_flags);
	}

  pthread_mutex_lock(&mutex_session);
	bool stage_0_finished = ((&q4s_session)->latency_th <= 0 ||
	  (num_latency_measures_client >= NUMSAMPLESTOSUCCEED && num_latency_measures_server >= NUMSAMPLESTOSUCCEED + 1))
		&& ((&q4s_session)->jitter_th[0] <= 0 || num_jitter_measures_server >= NUMSAMPLESTOSUCCEED + 1)
		&& ((&q4s_session)->jitter_th[1] <= 0 || num_jitter_measures_client >= NUMSAMPLESTOSUCCEED)
		&& ((&q4s_session)->packetloss_th[0] <= 0 || num_packetloss_measures_server >= NUMSAMPLESTOSUCCEED + 1)
		&& ((&q4s_session)->packetloss_th[1] <= 0 || num_packetloss_measures_client >= NUMSAMPLESTOSUCCEED);

	bool no_stage_0 = (&q4s_session)->latency_th <= 0 && (&q4s_session)->jitter_th[0] <= 0
	  && (&q4s_session)->jitter_th[1] <= 0;
  pthread_mutex_unlock(&mutex_session);

	if (stage_0_finished && !no_stage_0) {
		pthread_cancel(timer_ping);
		pthread_mutex_lock(&mutex_print);
		printf("\nStage 0 has finished succesfully\n");
		pthread_mutex_unlock(&mutex_print);
	}

	bool stage_1_finished = ((&q4s_session)->bw_th[0] <= 0 ||
		(&q4s_session)->bw_measure_server >= (&q4s_session)->bw_th[0])
		&& ((&q4s_session)->bw_th[1] <= 0 || (&q4s_session)->bw_measure_client >= (&q4s_session)->bw_th[1])
		&& ((&q4s_session)->packetloss_th[0] <= 0 || num_packetloss_measures_server > NUMSAMPLESTOSUCCEED + 1)
		&& ((&q4s_session)->packetloss_th[1] <= 0 || num_packetloss_measures_client > NUMSAMPLESTOSUCCEED);

	bool no_stage_1 = (&q4s_session)->bw_th[0] <= 0 && (&q4s_session)->bw_th[1] <= 0;
	if (stage_1_finished && !no_stage_1) {
		pthread_cancel(timer_bwidth);
		pthread_mutex_lock(&mutex_print);
		printf("\nStage 1 has finished succesfully\n");
		pthread_mutex_unlock(&mutex_print);
	}
}

// Triggers an alert to the Actuator
void Alert (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_ALERT;
  pthread_mutex_unlock(&mutex_flags);

	pthread_mutex_lock(&mutex_print);
	printf("\nAn alert notification has been sent to the Actuator\n");
	pthread_mutex_unlock(&mutex_print);

	// Starts timer for ping delivery
	pthread_create(&timer_alert, NULL, (void*)alert_pause_timeout, NULL);

}

// Starts delivery of Q4S BWIDTH messages
void Bwidth_Init (fsm_t* fsm) {
	pthread_mutex_lock(&mutex_flags);
	flags &= ~FLAG_INIT_BWIDTH;
	pthread_mutex_unlock(&mutex_flags);

	pthread_mutex_lock(&mutex_session);
	// Initialize ping period if necessary
	if (q4s_session.bwidth_clk <= 0) {
		q4s_session.bwidth_clk = 1000;
	}
	pthread_mutex_unlock(&mutex_session);

	// Starts timer for bwidth delivery
	pthread_create(&timer_bwidth, NULL, (void*)bwidth_timeout, NULL);
}

// Sends a Q4S BWIDTH message
void Bwidth (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_TEMP_BWIDTH;
  pthread_mutex_unlock(&mutex_flags);

	pthread_mutex_lock(&mutex_session);
	// Fills q4s_session.message_to_send with the Q4S BWIDTH parameters
	create_bwidth(&q4s_session.message_to_send);
	// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
	prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
	// Sends the prepared message
	send_message_UDP(q4s_session.prepared_message);
	(&q4s_session)->seq_num_server++;
	pthread_mutex_unlock(&mutex_session);
}

// Tells Actuator to release the resources
void Release (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
  pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_RECEIVE_CANCEL;
	pthread_mutex_unlock(&mutex_flags);

	// Cancels timers
	pthread_cancel(timer_ping);
	pthread_cancel(timer_bwidth);

  // Send a cancel notification to Actuator
	pthread_mutex_lock(&mutex_print);
	printf("\nPress 'r' key when reserved resources have been released\n");
  pthread_mutex_unlock(&mutex_print);
}

// Sends a Q4S CANCEL message to the client, and exits connection with that client
void Cancel (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  // Puts every flag to 0
  flags = 0;
	pthread_mutex_unlock(&mutex_flags);

  pthread_mutex_lock(&mutex_session);
	// Fills q4s_session.message_to_send with the Q4S CANCEL parameters
	create_cancel(&q4s_session.message_to_send);
	// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
	prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
	// Sends the message to the Q4S client
	send_message_TCP(q4s_session.prepared_message);
	pthread_mutex_unlock(&mutex_session);

	// Cancels timers
	pthread_cancel(timer_ping);
	pthread_cancel(timer_bwidth);
  // Cancels the threads receiving Q4S messages
  pthread_cancel(receive_TCP_thread);
  pthread_cancel(receive_UDP_thread);
	// Closes connection with Q4S client
  close(client_socket_TCP);

	pthread_mutex_lock(&mutex_print);
  printf("Connection has been closed\n");
	printf("Waiting for a connection with a new client\n");
  pthread_mutex_unlock(&mutex_print);
}

// FUNCTION EXPLORING THE KEYBOARD

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
				// If "r" (of "release") has been pressed, FLAG_RELEASED is activated
				case 'r':
				  // Lock to guarantee mutual exclusion
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_RELEASED;
					pthread_mutex_unlock(&mutex_flags);
					break;
				// If "n" (of "new parameters") has been pressed, set_new_parameters is set true
				case 'n':
			    set_new_parameters = true;
			    break;
        // If any other key has been pressed, nothing happens
				default:
					break;
			}
		}
	}
}

// EXECUTION OF MAIN PROGRAM
int main () {
	// System configuration
	system_setup();

	// Server starts listening
	start_listening_UDP();
	start_listening_TCP();

	// State machine: list of transitions
	// {OriginState, CheckFunction, DestinationState, ActionFunction}
	fsm_trans_t q4s_table[] = {
		{ WAIT_CONNECTION, check_new_connection, WAIT_START, Setup},
		{ WAIT_START, check_receive_begin,  HANDSHAKE, Respond_ok },
		{ HANDSHAKE, check_receive_ready0,  STAGE_0, Respond_ok },
		{ HANDSHAKE, check_receive_ready1,  STAGE_1, Respond_ok },
		{ HANDSHAKE, check_receive_cancel, TERMINATION, Release },
		{ STAGE_0, check_receive_ping, PING_MEASURE_0, Respond_ok },
		{ PING_MEASURE_0, check_temp_ping_0, PING_MEASURE_0, Ping },
		{ PING_MEASURE_0, check_receive_ok, PING_MEASURE_0, Update },
		{ PING_MEASURE_0, check_receive_ping, PING_MEASURE_0, Update },
		{ PING_MEASURE_0, check_alert, PING_MEASURE_0, Alert },
		{ PING_MEASURE_0, check_receive_ready1, STAGE_1, Respond_ok },
		{ PING_MEASURE_0, check_receive_cancel, TERMINATION, Release },
		{ STAGE_1, check_init_bwidth, BWIDTH_MEASURE, Bwidth_Init },
		{ BWIDTH_MEASURE, check_temp_bwidth, BWIDTH_MEASURE, Bwidth },
		{ BWIDTH_MEASURE, check_receive_bwidth, BWIDTH_MEASURE, Update },
		{ BWIDTH_MEASURE, check_alert, BWIDTH_MEASURE, Alert },
		{ BWIDTH_MEASURE, check_receive_cancel, TERMINATION, Release },
		{ TERMINATION, check_released, WAIT_CONNECTION,  Cancel },
		{ -1, NULL, -1, NULL }
	};

  // State machine creation
	fsm_t* q4s_fsm = fsm_new (WAIT_CONNECTION, q4s_table, NULL);

	// State machine initialitation
	fsm_setup (q4s_fsm);

	pthread_mutex_lock(&mutex_print);
	printf("Waiting for a connection with a new client\n");
	pthread_mutex_unlock(&mutex_print);

	while (1) {
		// State machine operation
		fsm_fire (q4s_fsm);
		// Waits for CLK_MS milliseconds
		delay (CLK_MS);
	}

	// State machine destruction
	fsm_destroy (q4s_fsm);
	// Threads destruction
	pthread_cancel(receive_TCP_thread);
	pthread_cancel(receive_UDP_thread);
	pthread_cancel(keyboard_thread);
	pthread_cancel(timer_ping);
	pthread_cancel(timer_bwidth);
	return 0;
}
