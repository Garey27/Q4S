#include "q4s_server.h"

//-------------------------------------------------------
// VARIABLES
//-------------------------------------------------------

// FOR Q4S SESSION MANAGING
// Q4S session
static type_q4s_session q4s_session;
// Variable to store the flags
long int flags = 0;
// Variable to store state machine
fsm_t* q4s_fsm;

// FOR CONNECTION MANAGING
// Structs with info for the connection
struct sockaddr_in server_TCP, client_TCP, server_UDP, client_UDP;
// Variable for socket assignments
int server_socket_TCP, client_socket_TCP, server_socket_UDP;
// Variable with struct length
socklen_t longc, slen;
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
// Thread to check reception of redirected messages
pthread_t receive_redirected_thread;
// Variable used to cancel a thread respecting the mutex
bool cancel_redirected_thread;
// Thread to check pressed keys on the keyboard
pthread_t keyboard_thread;
// Variable used to cancel a thread respecting the mutex
bool cancel_keyboard_thread;
// Thread acting as timer for ping delivery in Stage 0
pthread_t timer_ping_0;
// Variable used to cancel a thread respecting the mutex
bool cancel_timer_ping_0;
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
// Thread acting as timer for recovery pause
pthread_t timer_recover;
// Variable used to cancel a thread respecting the mutex
bool cancel_timer_recover;
// Thread acting as timer for expiration time
pthread_t timer_expire;
// Variable used to cancel a thread respecting the mutex
bool cancel_timer_expire;
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
// Variable that stores the QoS values at the beginning of the session
int initial_qos_level[2];
// Variable that indicates if new parameters are to be established
bool set_new_parameters = false;
// Variable that indicates if the server is ready to go to next Stage
bool server_ready = false;
// Variable that indicates if bwidth_reception_timeout is activated
bool bwidth_reception_timeout_activated = false;
// Variable that indicates if something has been received from client
bool received;
// Indicates the position of the last sample in the array
int pos_latency;
// Indicates the position of the last sample in the array
int pos_elapsed_time;
// Indicates the position of the last sample in the array
int pos_packetloss;
// Variable that stores the number of successful samples needed to pass to next stage
int num_samples_succeed;
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
// Variable that specifies the status code of a failure occurred
int failure_code;
// Variable that specifies the sintax problem of a 400 failure
char sintax_problem_400[50];


// FOR PROCESS MANAGING
// Variables to store processes ID and identify processes
int pid2 = 1;
int pid3 = 1;
int pid4 = 1;
// Variables for mutual exclusion for process shared variables
pthread_mutex_t *mutex_process;
pthread_mutexattr_t attrmutex;
// Variables to indicate the state of each process
// 0 = no process, 1 = process listening, 2 = process executing
static int *state_proc1;
static int *state_proc2;
static int *state_proc3;
static int *state_proc4;
// Variables to store the q4s session id for each process
static int *s_id_proc1;
static int *s_id_proc2;
static int *s_id_proc3;
static int *s_id_proc4;
// Variables to store the id of each child process
static int *p_id_proc2;
static int *p_id_proc3;
static int *p_id_proc4;
// Variables that act as buffers for each process
static char *buffer_proc1;
static char *buffer_proc2;
static char *buffer_proc3;
static char *buffer_proc4;
// Variable to store self process UDP client
struct sockaddr_in my_client_UDP;
// Variables to store UDP clients for each process
static struct sockaddr_in *client_proc1;
static struct sockaddr_in *client_proc2;
static struct sockaddr_in *client_proc3;
static struct sockaddr_in *client_proc4;


//---------------------------------------------------------
// AUXILIARY FUNCTIONS
//---------------------------------------------------------


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

// Creates a random id based on timestamp
int create_random_id () {
	struct timespec tm;
	clock_gettime(CLOCK_REALTIME, &tm);
	srand(tm.tv_nsec * tm.tv_sec);
	int random_id = rand()%MAXSESSIONID;
	return random_id;
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
	slen = sizeof(client_UDP);
	longc = sizeof(client_TCP);

	pthread_mutex_init(&mutex_flags, NULL);
	pthread_mutex_init(&mutex_session, NULL);
	pthread_mutex_init(&mutex_buffer_TCP, NULL);
	pthread_mutex_init(&mutex_buffer_UDP, NULL);
	pthread_mutex_init(&mutex_print, NULL);
	pthread_mutex_init(&mutex_tm_latency, NULL);
	pthread_mutex_init(&mutex_tm_jitter, NULL);

	cancel_TCP_thread = false;
	cancel_UDP_thread = false;
	cancel_redirected_thread = false;
	cancel_timer_ping_0 = false;
	cancel_timer_ping_2 = false;
	cancel_timer_alert = false;
	cancel_timer_recover = false;
	cancel_timer_delivery_bwidth = false;
	cancel_timer_reception_bwidth = false;
	cancel_keyboard_thread = false;

	pthread_mutexattr_init(&attrmutex);
	pthread_mutexattr_setpshared(&attrmutex, PTHREAD_PROCESS_SHARED);

	mutex_process = mmap(NULL, sizeof *mutex_process, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	pthread_mutex_init(mutex_process, &attrmutex);

	state_proc1 = mmap(NULL, sizeof *state_proc1, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  state_proc2 = mmap(NULL, sizeof *state_proc2, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  state_proc3 = mmap(NULL, sizeof *state_proc3, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  state_proc4 = mmap(NULL, sizeof *state_proc4, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	s_id_proc1 = mmap(NULL, sizeof *s_id_proc1, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  s_id_proc2 = mmap(NULL, sizeof *s_id_proc2, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  s_id_proc3 = mmap(NULL, sizeof *s_id_proc3, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  s_id_proc4 = mmap(NULL, sizeof *s_id_proc4, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	p_id_proc2 = mmap(NULL, sizeof *p_id_proc2, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	p_id_proc3 = mmap(NULL, sizeof *p_id_proc3, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	p_id_proc4 = mmap(NULL, sizeof *p_id_proc4, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	buffer_proc1 = mmap(NULL, MAXDATASIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  buffer_proc2 = mmap(NULL, MAXDATASIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  buffer_proc3 = mmap(NULL, MAXDATASIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  buffer_proc4 = mmap(NULL, MAXDATASIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	client_proc1 = mmap(NULL, sizeof *client_proc1, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  client_proc2 = mmap(NULL, sizeof *client_proc2, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  client_proc3 = mmap(NULL, sizeof *client_proc3, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  client_proc4 = mmap(NULL, sizeof *client_proc4, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);


	*s_id_proc1 = -1;
	*s_id_proc2 = -1;
	*s_id_proc3 = -1;
	*s_id_proc4 = -1;

  pid2 = 1;
  pid3 = 1;
  pid4 = 1;

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

	pthread_mutex_lock(mutex_process);
	int proc_state_sum = *state_proc1 + *state_proc2 + *state_proc3 + *state_proc4;
	pthread_mutex_unlock(mutex_process);

	if (strcmp(request_method, "BEGIN") == 0) {
		int session_id = create_random_id();
		(&q4s_session)->session_id = session_id;

		if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
			pthread_mutex_lock(mutex_process);
			*s_id_proc1 = session_id;
			pthread_mutex_unlock(mutex_process);
		} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
			pthread_mutex_lock(mutex_process);
			*s_id_proc2 = session_id;
			pthread_mutex_unlock(mutex_process);
		} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
			pthread_mutex_lock(mutex_process);
			*s_id_proc3 = session_id;
			pthread_mutex_unlock(mutex_process);
		} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
			pthread_mutex_lock(mutex_process);
			*s_id_proc4 = session_id;
			pthread_mutex_unlock(mutex_process);
		}

		char s_id[10];
		sprintf(s_id, "%d", session_id);

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
		strcpy(o, "o=q4s-UA ");
		strcat(o, s_id);
		strcat(o, " 2353687637 IN IP4 127.0.0.1\n");
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
		char a19[100];
		memset(a19, '\0', sizeof(a19));

    // Fixed parameters

		// Default expiration time for session
		strcpy(h3, "Expires: ");
		char s_exp_time[10];
		sprintf(s_exp_time, "%d", EXPIRATIONTIME);
		strcat(h3, s_exp_time);
		strcat(h3, "\n");

		// Reactive is the default scenary
		strcpy(a2, "a=alerting-mode:Reactive\n");
		// Includes client IP direction
		strcpy(a5, "a=public-address:client IP4 ");
		strcat(a5, inet_ntoa(client_TCP.sin_addr));
		strcat(a5, "\n");
		// Includes server IP direction
		strcpy(a6, "a=public-address:server IP4 ");
		strcat(a6, inet_ntoa(server_TCP.sin_addr));
		strcat(a6, "\n");

		strcpy(a11, "a=flow:app clientListeningPort TCP/10000-20000\n");
		strcpy(a12, "a=flow:app clientListeningPort UDP/15000-18000\n");
		strcpy(a13, "a=flow:app serverListeningPort TCP/56000\n");
		strcpy(a14, "a=flow:app serverListeningPort UDP/56000\n");
		// Includes ports permitted for Q4S sessions
		strcpy(a15, "a=flow:q4s clientListeningPort UDP/");
		char s_port[10];
		sprintf(s_port, "%d", CLIENT_PORT_UDP);
		strcat(a15, s_port);
		strcat(a15, "\n");
		memset(s_port, '\0', strlen(s_port));
		strcpy(a16, "a=flow:q4s clientListeningPort TCP/");
		sprintf(s_port, "%d", CLIENT_PORT_TCP);
		strcat(a16, s_port);
		strcat(a16, "\n");
		memset(s_port, '\0', strlen(s_port));
		strcpy(a17, "a=flow:q4s serverListeningPort UDP/");
		sprintf(s_port, "%d", HOST_PORT_UDP);
		strcat(a17, s_port);
		strcat(a17, "\n");
		memset(s_port, '\0', strlen(s_port));
		strcpy(a18, "a=flow:q4s serverListeningPort TCP/");
		sprintf(s_port, "%d", HOST_PORT_TCP);
		strcat(a18, s_port);
		strcat(a18, "\n");
		memset(s_port, '\0', strlen(s_port));

		// Stores user inputs
		char input[100];

		if (proc_state_sum == 3) {
			memset(input, '\0', sizeof(input));
			pthread_mutex_lock(&mutex_print);
			printf("\nDo you want to set default parameters? (yes/no): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		}

		if (proc_state_sum != 3 || strstr(input, "yes")) {
			memset(input, '\0', sizeof(input));

			// Default SDP parameters
			strcpy(a1, "a=qos-level:0/0\n");
	    strcpy(a3, "a=alert-pause:5000\n");
			strcpy(a4, "a=recovery-pause:5000\n");
	    strcpy(a7, "a=latency:50\n");
	    strcpy(a8, "a=jitter:100/100\n");
	    strcpy(a9, "a=bandwidth:100/1000\n");
	    strcpy(a10, "a=packetloss:0.20/0.20\n");
			strcpy(a19, "a=measurement:procedure default(50/50,75/75,2000,10/10,15/15)");

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

			// Includes user input about recovery pause
		  strcpy(a4, "a=recovery-pause:");
			pthread_mutex_lock(&mutex_print);
		  printf("Enter recovery pause (in ms): ");
			pthread_mutex_unlock(&mutex_print);
	    scanf("%s", input);
		  strcat(a4, input);
		  strcat(a4, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about latency threshold (showing value suggested by client)
		  strcpy(a7, "a=latency:");
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
		  strcat(a7, input);
		  strcat(a7, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about jitter thresholds (showing values suggested by client)
		  strcpy(a8, "a=jitter:");
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
		  strcat(a8, input);
		  strcat(a8, "/");
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
		  strcat(a8, input);
		  strcat(a8, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about bandwidth thresholds (showing values suggested by client)
		  strcpy(a9, "a=bandwidth:");
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
		  strcat(a9, input);
		  strcat(a9, "/");
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
		  strcat(a9, input);
		  strcat(a9, "\n");
	    memset(input, '\0', strlen(input));

	    // Includes user input about packetloss thresholds (showing values suggested by client)
		  strcpy(a10, "a=packetloss:");
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
		  strcat(a10, input);
		  strcat(a10, "/");
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
		  strcat(a10, input);
		  strcat(a10, "\n");
	    memset(input, '\0', strlen(input));

			// Includes user input about measurement procedure
		  strcpy(a19, "a=measurement:procedure default(");
			pthread_mutex_lock(&mutex_print);
		  printf("Enter interval of time between Q4S PING requests sent by client in NEGOTIATION phase (in ms): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a19, input);
			strcat(a19, "/");
			memset(input, '\0', strlen(input));
			pthread_mutex_lock(&mutex_print);
			printf("Enter interval of time between Q4S PING requests sent by server in NEGOTIATION phase (in ms): ");
			pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a19, input);
			strcat(a19, ",");
			memset(input, '\0', strlen(input));
			pthread_mutex_lock(&mutex_print);
			printf("Enter interval of time between Q4S PING requests sent by client in CONTINUITY phase (in ms): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a19, input);
			strcat(a19, "/");
			memset(input, '\0', strlen(input));
      pthread_mutex_lock(&mutex_print);
			printf("Enter interval of time between Q4S PING requests sent by server in CONTINUITY phase (in ms): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a19, input);
			strcat(a19, ",");
			memset(input, '\0', strlen(input));
      pthread_mutex_lock(&mutex_print);
			printf("Enter interval for Q4S BWIDTH measures in NEGOTIATION phase (in ms): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a19, input);
			strcat(a19, ",");
			memset(input, '\0', strlen(input));
      pthread_mutex_lock(&mutex_print);
			printf("Enter window size for jitter and latency calculations for client (< 300): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a19, input);
			strcat(a19, "/");
			memset(input, '\0', strlen(input));
      pthread_mutex_lock(&mutex_print);
			printf("Enter window size for jitter and latency calculations for server (< 300): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a19, input);
			strcat(a19, ",");
			memset(input, '\0', strlen(input));
      pthread_mutex_lock(&mutex_print);
			printf("Enter window size for packet loss calculations for client (< 300): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a19, input);
			strcat(a19, "/");
			memset(input, '\0', strlen(input));
      pthread_mutex_lock(&mutex_print);
			printf("Enter window size for packet loss calculations for server (< 300): ");
      pthread_mutex_unlock(&mutex_print);
			scanf("%s", input);
		  strcat(a19, input);
			strcat(a19, ")");
		  strcat(a19, "\n");
	    memset(input, '\0', strlen(input));

		}

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
		strcat(body, a19);

		// Includes current time
		strcpy(h1, "Date: ");
		const char * now = current_time();
		strcat(h1, now);
		strcat(h1, "\n");
		// Type of the content
		strcpy(h2,"Content-Type: application/sdp\n");
		// Creates a MD5 hash for the sdp and includes it in the header
		char hash[33];
	  MD5mod(body, hash);
		strcpy(h4, "Signature: ");
		strcat(h4, hash);
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

	} else if (strcmp(request_method, "PING") == 0) {
		if (proc_state_sum == 3 && new_param) {
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
		  printf("\nEnter upstream QoS level (from 0 to 9) [Actual value: %d]: ", (&q4s_session)->qos_level[0]);
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
		  printf("Enter downstream QoS level (from 0 to 9) [Actual value: %d]: ", (&q4s_session)->qos_level[1]);
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
		  printf("Enter latency threshold (in ms) [Actual value: %d]: ", (&q4s_session)->latency_th);
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
		  printf("Enter upstream jitter threshold (in ms) [Actual value: %d]: ", (&q4s_session)->jitter_th[0]);
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
		  printf("Enter downstream jitter threshold (in ms) [Actual value: %d]: ", (&q4s_session)->jitter_th[1]);
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
		  printf("Enter upstream bandwidth threshold (in kbps) [Actual value: %d]: ", (&q4s_session)->bw_th[0]);
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
		  printf("Enter downstream bandwidth threshold (in kbps) [Actual value: %d]: ", (&q4s_session)->bw_th[1]);
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
		  printf("Enter upstream packetloss threshold (percentage from 0.00 to 1.00) [Actual value: %.2f]: ", (&q4s_session)->packetloss_th[0]);
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
		  printf("Enter downstream packetloss threshold (percentage from 0.00 to 1.00) [Actual value: %.2f]: ", (&q4s_session)->packetloss_th[1]);
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

			// Creates a MD5 hash for the sdp and includes it in the header
			char hash[33];
		  MD5mod(body, hash);
			strcpy(h5, "Signature: ");
			strcat(h5, hash);
			strcat(h5, "\n");
		}

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
		if (strlen(h5) > 0) {
			strcat(header, h5);
		}

	} else if (strcmp(request_method, "READY0") == 0) {
		if (proc_state_sum == 3 && new_param) {
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
		  printf("\nEnter upstream QoS level (from 0 to 9) [Actual value: %d]: ", (&q4s_session)->qos_level[0]);
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
		  printf("Enter downstream QoS level (from 0 to 9) [Actual value: %d]: ", (&q4s_session)->qos_level[1]);
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
		  printf("Enter latency threshold (in ms) [Actual value: %d]: ", (&q4s_session)->latency_th);
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
		  printf("Enter upstream jitter threshold (in ms) [Actual value: %d]: ", (&q4s_session)->jitter_th[0]);
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
		  printf("Enter downstream jitter threshold (in ms) [Actual value: %d]: ", (&q4s_session)->jitter_th[1]);
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
		  printf("Enter upstream bandwidth threshold (in kbps) [Actual value: %d]: ", (&q4s_session)->bw_th[0]);
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
		  printf("Enter downstream bandwidth threshold (in kbps) [Actual value: %d]: ", (&q4s_session)->bw_th[1]);
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
		  printf("Enter upstream packetloss threshold (percentage from 0.00 to 1.00) [Actual value: %.2f]: ", (&q4s_session)->packetloss_th[0]);
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
		  printf("Enter downstream packetloss threshold (percentage from 0.00 to 1.00) [Actual value: %.2f]: ", (&q4s_session)->packetloss_th[1]);
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

			// Creates a MD5 hash for the sdp and includes it in the header
			char hash[33];
		  MD5mod(body, hash);
			strcpy(h4, "Signature: ");
			strcat(h4, hash);
			strcat(h4, "\n");
		}

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
		if (strlen(h4) > 0) {
			strcat(header, h4);
		}

	} else if (strcmp(request_method, "READY1") == 0) {

		if (proc_state_sum == 3 && new_param) {
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
		  printf("\nEnter upstream QoS level (from 0 to 9) [Actual value: %d]: ", (&q4s_session)->qos_level[0]);
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
		  printf("Enter downstream QoS level (from 0 to 9) [Actual value: %d]: ", (&q4s_session)->qos_level[1]);
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
		  printf("Enter latency threshold (in ms) [Actual value: %d]: ", (&q4s_session)->latency_th);
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
		  printf("Enter upstream jitter threshold (in ms) [Actual value: %d]: ", (&q4s_session)->jitter_th[0]);
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
		  printf("Enter downstream jitter threshold (in ms) [Actual value: %d]: ", (&q4s_session)->jitter_th[1]);
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
		  printf("Enter upstream bandwidth threshold (in kbps) [Actual value: %d]: ", (&q4s_session)->bw_th[0]);
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
		  printf("Enter downstream bandwidth threshold (in kbps) [Actual value: %d]: ", (&q4s_session)->bw_th[1]);
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
		  printf("Enter upstream packetloss threshold (percentage from 0.00 to 1.00) [Actual value: %.2f]: ", (&q4s_session)->packetloss_th[0]);
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
		  printf("Enter downstream packetloss threshold (percentage from 0.00 to 1.00) [Actual value: %.2f]: ", (&q4s_session)->packetloss_th[1]);
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

			// Creates a MD5 hash for the sdp and includes it in the header
			char hash[33];
		  MD5mod(body, hash);
			strcpy(h4, "Signature: ");
			strcat(h4, hash);
			strcat(h4, "\n");
		}

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
		if (strlen(h4) > 0) {
			strcat(header, h4);
		}

	} else if (strcmp(request_method, "READY2") == 0) {

		if (proc_state_sum == 3 && new_param) {
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
		  printf("\nEnter upstream QoS level (from 0 to 9) [Actual value: %d]: ", (&q4s_session)->qos_level[0]);
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
		  printf("Enter downstream QoS level (from 0 to 9) [Actual value: %d]: ", (&q4s_session)->qos_level[1]);
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
		  printf("Enter latency threshold (in ms) [Actual value: %d]: ", (&q4s_session)->latency_th);
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
		  printf("Enter upstream jitter threshold (in ms) [Actual value: %d]: ", (&q4s_session)->jitter_th[0]);
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
		  printf("Enter downstream jitter threshold (in ms) [Actual value: %d]: ", (&q4s_session)->jitter_th[1]);
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
		  printf("Enter upstream bandwidth threshold (in kbps) [Actual value: %d]: ", (&q4s_session)->bw_th[0]);
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
		  printf("Enter downstream bandwidth threshold (in kbps) [Actual value: %d]: ", (&q4s_session)->bw_th[1]);
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
		  printf("Enter upstream packetloss threshold (percentage from 0.00 to 1.00) [Actual value: %.2f]: ", (&q4s_session)->packetloss_th[0]);
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
		  printf("Enter downstream packetloss threshold (percentage from 0.00 to 1.00) [Actual value: %.2f]: ", (&q4s_session)->packetloss_th[1]);
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

			// Creates a MD5 hash for the sdp and includes it in the header
			char hash[33];
			MD5mod(body, hash);
			strcpy(h5, "Signature: ");
			strcat(h5, hash);
			strcat(h5, "\n");
		}

		// Includes session ID
		strcpy(h1, "Session-Id: ");
		char s_session_id[20];
		sprintf(s_session_id, "%d", (&q4s_session)->session_id);
		strcat(h1, s_session_id);
		strcat(h1, "\n");
    // Includes Stage number
		strcpy(h2, "Stage: 2\n");
		// Includes Trigger URI
		strcpy(h3, "Trigger-URI: http://www.example.com/app_start\n");
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
		if (strlen(h5) > 0) {
			strcat(header, h5);
		}

	} else {
		// Includes current time
		strcpy(h1, "Date: ");
	  const char * now = current_time();
		strcat(h1, now);
		strcat(h1, "\n");

		// Type of the content
		strcpy(h2,"Content-Type: application/sdp\n");

		// Prepares header with header fields
		strcpy(header, h1);
		strcat(header, h2);
	}

  // Delegates in a response creation function
  create_response (q4s_message, "200", "OK", header, body);
}

// Creation of Q4S 400 Bad Request message
// The request could not be understood due to malformed syntax
void create_400 (type_q4s_message *q4s_message, char sintax_problem[50]) {
	char header[500];  // to store header of the message
  memset(header, '\0', sizeof(header));

	char body[5000];  // to store body of the message
	memset(body, '\0', sizeof(body));

	char h1[100];
	memset(h1, '\0', sizeof(h1));

	char h2[100];
	memset(h2, '\0', sizeof(h2));

	char reason_phrase[100];
	memset(reason_phrase, '\0', sizeof(reason_phrase));

	strcpy(reason_phrase, "Bad Request: ");
	strcat(reason_phrase, sintax_problem);

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");

	// Includes current time
	strcpy(h2, "Date: ");
	const char * now = current_time();
	strcat(h2, now);
	strcat(h2, "\n");

	// Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);

	// Delegates in a response creation function
  create_response (q4s_message, "400", reason_phrase, header, body);
}

// Creation of Q4S 413 Request Entity Too Large
// The request entity-body is larger than the one that the server is willing to process
void create_413 (type_q4s_message *q4s_message) {
	char header[500];  // to store header of the message
  memset(header, '\0', sizeof(header));

	char body[5000];  // to store body of the message
	memset(body, '\0', sizeof(body));

	char h1[100];
	memset(h1, '\0', sizeof(h1));

	char h2[100];
	memset(h2, '\0', sizeof(h2));

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");

	// Includes current time
	strcpy(h2, "Date: ");
	const char * now = current_time();
	strcat(h2, now);
	strcat(h2, "\n");

	// Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);

	// Delegates in a response creation function
  create_response (q4s_message, "413", "Request Entity Too Large", header, body);
}

// Creation of Q4S 414 Request-URI Too Long
// The Request-URI is longer than the one that the server accepts
void create_414 (type_q4s_message *q4s_message) {
	char header[500];  // to store header of the message
  memset(header, '\0', sizeof(header));

	char body[5000];  // to store body of the message
	memset(body, '\0', sizeof(body));

	char h1[100];
	memset(h1, '\0', sizeof(h1));

	char h2[100];
	memset(h2, '\0', sizeof(h2));

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");

	// Includes current time
	strcpy(h2, "Date: ");
	const char * now = current_time();
	strcat(h2, now);
	strcat(h2, "\n");

	// Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);

	// Delegates in a response creation function
  create_response (q4s_message, "414", "Request-URI Too Long", header, body);
}

// Creation of Q4S 415 Unsupported Media Type
// the message body of the request is in a format not supported by the server
void create_415 (type_q4s_message *q4s_message) {
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

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");

	// Includes current time
	strcpy(h2, "Date: ");
	const char * now = current_time();
	strcat(h2, now);
	strcat(h2, "\n");

	// Includes formats accepted
	strcpy(h3, "Accept: application/sdp\n");

	// Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);
	strcat(header, h3);

	// Delegates in a response creation function
  create_response (q4s_message, "415", "Unsupported Media Type", header, body);
}

// Creation of Q4S 416 Unsupported URI Scheme
// The scheme of the URI in the Request-URI is unknown to the server
void create_416 (type_q4s_message *q4s_message) {
	char header[500];  // to store header of the message
  memset(header, '\0', sizeof(header));

	char body[5000];  // to store body of the message
	memset(body, '\0', sizeof(body));

	char h1[100];
	memset(h1, '\0', sizeof(h1));

	char h2[100];
	memset(h2, '\0', sizeof(h2));

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");

	// Includes current time
	strcpy(h2, "Date: ");
	const char * now = current_time();
	strcat(h2, now);
	strcat(h2, "\n");

	// Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);

	// Delegates in a response creation function
  create_response (q4s_message, "416", "Unsupported URI Scheme", header, body);
}

// Creation of Q4S 501 Not Implemented
// The server doesn't recognize the request method and it is not capable of supporting it
void create_501 (type_q4s_message *q4s_message) {
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

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");

	// Includes current time
	strcpy(h2, "Date: ");
	const char * now = current_time();
	strcat(h2, now);
	strcat(h2, "\n");

	// Includes allowed methods
	strcpy(h3, "Allow: BEGIN, READY, PING, BWIDTH, CANCEL\n");

	// Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);
	strcat(header, h3);

	// Delegates in a response creation function
  create_response (q4s_message, "501", "Not Implemented", header, body);
}

// Creation of Q4S 505 Version Not Supported
// The server does not support the Q4S protocol version that was used in the request
void create_505 (type_q4s_message *q4s_message) {
	char header[500];  // to store header of the message
  memset(header, '\0', sizeof(header));

	char body[5000];  // to store body of the message
	memset(body, '\0', sizeof(body));

	char h1[100];
	memset(h1, '\0', sizeof(h1));

	char h2[100];
	memset(h2, '\0', sizeof(h2));

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");

	// Includes current time
	strcpy(h2, "Date: ");
	const char * now = current_time();
	strcat(h2, now);
	strcat(h2, "\n");

	// Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);

	// Delegates in a response creation function
  create_response (q4s_message, "505", "Version Not Supported", header, body);
}

// Creation of Q4S 600 Session Does Not Exist
// The Session-Id is not valid
void create_600 (type_q4s_message *q4s_message) {
	char header[500];  // to store header of the message
  memset(header, '\0', sizeof(header));

	char body[5000];  // to store body of the message
	memset(body, '\0', sizeof(body));

	char h1[100];
	memset(h1, '\0', sizeof(h1));

	char h2[100];
	memset(h2, '\0', sizeof(h2));

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");

	// Includes current time
	strcpy(h2, "Date: ");
	const char * now = current_time();
	strcat(h2, now);
	strcat(h2, "\n");

	// Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);

	// Delegates in a response creation function
  create_response (q4s_message, "600", "Session Does Not Exist", header, body);
}

// Creation of Q4S 601 Quality Level Not Allowed
// The QOS level requested is not allowed for the pair client/server
void create_601 (type_q4s_message *q4s_message) {
	char header[500];  // to store header of the message
  memset(header, '\0', sizeof(header));

	char body[5000];  // to store body of the message
	memset(body, '\0', sizeof(body));

	char h1[100];
	memset(h1, '\0', sizeof(h1));

	char h2[100];
	memset(h2, '\0', sizeof(h2));

	// Includes session ID
	strcpy(h1, "Session-Id: ");
	char s_session_id[20];
	sprintf(s_session_id, "%d", (&q4s_session)->session_id);
	strcat(h1, s_session_id);
	strcat(h1, "\n");

	// Includes current time
	strcpy(h2, "Date: ");
	const char * now = current_time();
	strcat(h2, now);
	strcat(h2, "\n");

	// Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);

	// Delegates in a response creation function
  create_response (q4s_message, "601", "Quality Level Not Allowed", header, body);
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
	if((&q4s_session)->jitter_measure_server >= 0 && (&q4s_session)->jitter_th[0]) {
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
	if((&q4s_session)->bw_measure_server >= 0 && (&q4s_session)->bw_th[0]) {
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
	if((&q4s_session)->jitter_measure_server >= 0 && (&q4s_session)->jitter_th[0]) {
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
	if((&q4s_session)->bw_measure_server >= 0 && (&q4s_session)->bw_th[0]) {
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

// Redirection of received message to other process if necessary
// Checks if message is directed to other process and redirects it if proceeds
bool redirect (char received_message[MAXDATASIZE], struct sockaddr_in client) {
	char copy_message[MAXDATASIZE];
	memset(copy_message, '\0', MAXDATASIZE);
	strcpy(copy_message, received_message);

	char s_id[20];
	sprintf(s_id, "%d", q4s_session.session_id);

	bool not_for_me = (strstr(copy_message, s_id) == NULL);
	strcpy(copy_message, received_message);

	if (not_for_me) {
		if (pid2 != 0 && pid3 != 0 && pid4 != 0) {

			pthread_mutex_lock(mutex_process);

			if (*state_proc2 == 2) {
				char s_id_p2[20];
				sprintf(s_id_p2, "%d", *s_id_proc2);
				bool for_p2 = strstr(copy_message, s_id_p2);
				strcpy(copy_message, received_message);
				if (for_p2) {
					strcpy(buffer_proc2, received_message);
					*client_proc2 = client;
					pthread_mutex_unlock(mutex_process);
					return true;
				}
			}

			if (*state_proc3 == 2) {
				char s_id_p3[20];
				sprintf(s_id_p3, "%d", *s_id_proc3);
				bool for_p3 = strstr(copy_message, s_id_p3);
				strcpy(copy_message, received_message);
				if (for_p3) {
					strcpy(buffer_proc3, received_message);
					*client_proc3 = client;
					pthread_mutex_unlock(mutex_process);
					return true;
				}
			}

			if (*state_proc4 == 2) {
				char s_id_p4[20];
				sprintf(s_id_p4, "%d", *s_id_proc4);
				bool for_p4 = strstr(copy_message, s_id_p4);
				strcpy(copy_message, received_message);
				if (for_p4) {
					strcpy(buffer_proc4, received_message);
					*client_proc4 = client;
					pthread_mutex_unlock(mutex_process);
					return true;
				}
			}

			pthread_mutex_unlock(mutex_process);

		} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {

			pthread_mutex_lock(mutex_process);

			if (*state_proc1 == 2) {
				char s_id_p1[20];
				sprintf(s_id_p1, "%d", *s_id_proc1);
				bool for_p1 = strstr(copy_message, s_id_p1);
				strcpy(copy_message, received_message);
				if (for_p1) {
					strcpy(buffer_proc1, received_message);
					*client_proc1 = client;
					pthread_mutex_unlock(mutex_process);
					return true;
				}
			}

			if (*state_proc3 == 2) {
				char s_id_p3[20];
				sprintf(s_id_p3, "%d", *s_id_proc3);
				bool for_p3 = strstr(copy_message, s_id_p3);
				strcpy(copy_message, received_message);
				if (for_p3) {
					strcpy(buffer_proc3, received_message);
					*client_proc3 = client;
					pthread_mutex_unlock(mutex_process);
					return true;
				}
			}

			if (*state_proc4 == 2) {
				char s_id_p4[20];
				sprintf(s_id_p4, "%d", *s_id_proc4);
				bool for_p4 = strstr(copy_message, s_id_p4);
				strcpy(copy_message, received_message);
				if (for_p4) {
					strcpy(buffer_proc4, received_message);
					*client_proc4 = client;
					pthread_mutex_unlock(mutex_process);
					return true;
				}
			}

			pthread_mutex_unlock(mutex_process);

		} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {

			pthread_mutex_lock(mutex_process);

			if (*state_proc1 == 2) {
				char s_id_p1[20];
				sprintf(s_id_p1, "%d", *s_id_proc1);
				bool for_p1 = strstr(copy_message, s_id_p1);
				strcpy(copy_message, received_message);
				if (for_p1) {
					strcpy(buffer_proc1, received_message);
					*client_proc1 = client;
					pthread_mutex_unlock(mutex_process);
					return true;
				}
			}

			if (*state_proc2 == 2) {
				char s_id_p2[20];
				sprintf(s_id_p2, "%d", *s_id_proc2);
				bool for_p2 = strstr(copy_message, s_id_p2);
				strcpy(copy_message, received_message);
				if (for_p2) {
					strcpy(buffer_proc2, received_message);
					*client_proc2 = client;
					pthread_mutex_unlock(mutex_process);
					return true;
				}
			}

			if (*state_proc4 == 2) {
				char s_id_p4[20];
				sprintf(s_id_p4, "%d", *s_id_proc4);
				bool for_p4 = strstr(copy_message, s_id_p4);
				strcpy(copy_message, received_message);
				if (for_p4) {
					strcpy(buffer_proc4, received_message);
					*client_proc4 = client;
					pthread_mutex_unlock(mutex_process);
					return true;
				}
			}

			pthread_mutex_unlock(mutex_process);

		} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {

			pthread_mutex_lock(mutex_process);

			if (*state_proc1 == 2) {
				char s_id_p1[20];
				sprintf(s_id_p1, "%d", *s_id_proc1);
				bool for_p1 = strstr(copy_message, s_id_p1);
				strcpy(copy_message, received_message);
				if (for_p1) {
					strcpy(buffer_proc1, received_message);
					*client_proc1 = client;
					pthread_mutex_unlock(mutex_process);
					return true;
				}
			}

			if (*state_proc2 == 2) {
				char s_id_p2[20];
				sprintf(s_id_p2, "%d", *s_id_proc2);
				bool for_p2 = strstr(copy_message, s_id_p2);
				strcpy(copy_message, received_message);
				if (for_p2) {
					strcpy(buffer_proc2, received_message);
					*client_proc2 = client;
					pthread_mutex_unlock(mutex_process);
					return true;
				}
			}

			if (*state_proc3 == 2) {
				char s_id_p3[20];
				sprintf(s_id_p3, "%d", *s_id_proc3);
				bool for_p3 = strstr(copy_message, s_id_p3);
				strcpy(copy_message, received_message);
				if (for_p3) {
					strcpy(buffer_proc3, received_message);
					*client_proc3 = client;
					pthread_mutex_unlock(mutex_process);
					return true;
				}
			}
		}

		pthread_mutex_unlock(mutex_process);
	}
	return false;
}

// Validation and storage of a Q4S message just received
// Converts from char[MAXDATASIZE] to type_q4s_message
bool store_message (char received_message[MAXDATASIZE], type_q4s_message *q4s_message) {
	if (strlen(received_message) > 5700) {
		failure_code = 413;
		return false;
	}

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
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
		failure_code = 414;
		return false;
	}

	if (strstr(copy_start_line, "\\") != NULL) {
		strcpy(copy_start_line, start_line);
		failure_code = 416;
		return false;
	}
	strcpy(copy_start_line, start_line);

	if (strstr(copy_start_line, "\a") != NULL) {
		strcpy(copy_start_line, start_line);
		failure_code = 416;
		return false;
	}
	strcpy(copy_start_line, start_line);

	if (strstr(copy_start_line, "\b") != NULL) {
		strcpy(copy_start_line, start_line);
		failure_code = 416;
		return false;
	}
	strcpy(copy_start_line, start_line);

	if (strstr(copy_start_line, "\f") != NULL) {
		strcpy(copy_start_line, start_line);
		failure_code = 416;
		return false;
	}
	strcpy(copy_start_line, start_line);

	if (strstr(copy_start_line, "\r") != NULL) {
		strcpy(copy_start_line, start_line);
		failure_code = 416;
		return false;
	}
	strcpy(copy_start_line, start_line);

	if (strstr(copy_start_line, "\t") != NULL) {
		strcpy(copy_start_line, start_line);
		failure_code = 416;
		return false;
	}
	strcpy(copy_start_line, start_line);

	if (strstr(copy_start_line, "\v") != NULL) {
		strcpy(copy_start_line, start_line);
		failure_code = 416;
		return false;
	}
	strcpy(copy_start_line, start_line);

	if (strstr(copy_start_line, "<") != NULL) {
		failure_code = 416;
		return false;
	}
	strcpy(copy_start_line, start_line);

	if (strstr(copy_start_line, ">") != NULL) {
		strcpy(copy_start_line, start_line);
		failure_code = 416;
		return false;
	}
	strcpy(copy_start_line, start_line);

	// Auxiliary variable
	char *fragment;
	bool response;

	fragment = strtok(copy_start_line, " ");

	if (strcmp(fragment, "BEGIN") == 0 || strcmp(fragment, "READY") == 0
		|| strcmp(fragment, "PING") == 0 || strcmp(fragment, "BWIDTH") == 0
		|| strcmp(fragment, "CANCEL") == 0) {
			response = false;
	} else if (strcmp(fragment, "Q4S/1.0") == 0) {
		response = true;
	} else {
		strcpy(copy_start_line, start_line);
		failure_code = 501;
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
					failure_code = 505;
					return false;
				}
			} else {
				strcpy(copy_start_line, start_line);
				failure_code = 416;
				return false;
			}
		} else {
			strcpy(copy_start_line, start_line);
			failure_code = 416;
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
			printf("\nP%d: MD5 hash of the sdp doesn't coincide\n", n_process);
			printf("\nSignature: %s\n", signature);
			printf("\nHash: %s\n", hash);
      pthread_mutex_unlock(&mutex_print);
			failure_code = 400;
			strcpy(sintax_problem_400, "Integrity of message has been violated");
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
				failure_code = 415;
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
				failure_code = 601;
				return false;
			}
			char *qos_level_down;
			qos_level_down = strtok(NULL, "\n");  // stores string value
			if (atoi(qos_level_down) > 9 || atoi(qos_level_down) < 0) {
				failure_code = 601;
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
				printf("\nP%d: Session ID of the message doesn't coincide\n", n_process);
				printf("\nID received: %d\n", atoi(string_id));
				printf("\nActual ID: %d\n", (&q4s_session)->session_id);
				pthread_mutex_unlock(&mutex_print);
				memset(fragment, '\0', strlen(fragment));
				strcpy(copy_header, header);  // restore copy of header
				failure_code = 600;
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
				printf("\nP%d: Session ID of the message doesn't coincide\n", n_process);
				pthread_mutex_unlock(&mutex_print);
				memset(fragment, '\0', strlen(fragment));
				strcpy(copy_header, header);  // restore copy of header
				strcpy(copy_body, body);  // restores copy of body
				failure_code = 600;
				return false;
			} else {
				memset(fragment, '\0', strlen(fragment));
				strcpy(copy_header, header); // restores copy of header
				strcpy(copy_body, body);  // restores copy of body
			}
		} else {
			strcpy(copy_header, header); // restores copy of header
			strcpy(copy_body, body);  // restores copy of body
			failure_code = 400;
			strcpy(sintax_problem_400, "Missing Session-Id header field");
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
			failure_code = 400;
			strcpy(sintax_problem_400, "Missing Sequence-Number header field");
			return false;
		}
	}

	if (strcmp(copy_start_line, "READY q4s://www.example.com Q4S/1.0") == 0) {
		// If there is a Stage parameter in the header
		if (fragment = strstr(copy_header, "Stage")) {
			fragment = fragment + 7;
			char *s_stage;
			s_stage = strtok(fragment, "\n");  // stores string value
			if (atoi(s_stage) < 0 || atoi(s_stage) > 2) {
				memset(fragment, '\0', strlen(fragment));
				strcpy(copy_start_line, start_line);  // restores copy of start line
				strcpy(copy_header, header); // restores copy of header
				failure_code = 400;
				strcpy(sintax_problem_400, "Incorrect value of Stage header field");
				return false;
			} else {
				memset(fragment, '\0', strlen(fragment));
				strcpy(copy_header, header);  // restore copy of header
				strcpy(copy_start_line, start_line);  // restores copy of start line
			}
		} else {
			strcpy(copy_start_line, start_line);  // restores copy of start line
			strcpy(copy_header, header); // restores copy of header
			failure_code = 400;
			strcpy(sintax_problem_400, "Missing Stage header field");
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

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

  // Auxiliary variable
	char *fragment;

	fprintf(stderr,"\n");

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
			strcpy((q4s_session)->client_timestamp, s_date);  // stores
			memset(fragment, '\0', strlen(fragment));
			strcpy(copy_header, header);  // restores copy of header
      pthread_mutex_lock(&mutex_print);
			fprintf(stderr, "P%d: Client timestamp stored: %s\n", n_process, (q4s_session)->client_timestamp);
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
			fprintf(stderr,"P%d: Sequence number of client stored: %d\n", n_process, (q4s_session)->seq_num_client);
      pthread_mutex_unlock(&mutex_print);
		}
		// If there is a Measurements parameter in the header
		if (fragment = strstr(copy_header, "Measurements")){
			fragment = fragment + 14;  // moves to the beginning of the value
			char *s_latency;
			strtok(fragment, "=");
			s_latency = strtok(NULL, ",");  // stores string value
			if (strcmp(s_latency," ") != 0) {
				(q4s_session)->latency_measure_client = atoi(s_latency);  // converts into int and stores
				pthread_mutex_lock(&mutex_print);
				fprintf(stderr,"P%d: Latency measure of client stored: %d\n", n_process, (q4s_session)->latency_measure_client);
	      pthread_mutex_unlock(&mutex_print);
			}

			char *s_jitter;
			strtok(NULL, "=");
			s_jitter = strtok(NULL, ",");  // stores string value
			if (strcmp(s_jitter," ") != 0) {
				(q4s_session)->jitter_measure_client = atoi(s_jitter);  // converts into int and stores
				pthread_mutex_lock(&mutex_print);
				fprintf(stderr,"P%d: Jitter measure of client stored: %d\n", n_process, (q4s_session)->jitter_measure_client);
	      pthread_mutex_unlock(&mutex_print);
			}

			char *s_pl;
			strtok(NULL, "=");
			s_pl = strtok(NULL, ",");  // stores string value
      if (strcmp(s_pl," ") != 0) {
				(q4s_session)->packetloss_measure_client = atof(s_pl);  // converts into int and stores
				pthread_mutex_lock(&mutex_print);
				fprintf(stderr,"P%d: Packetloss measure of client stored: %.2f\n", n_process, (q4s_session)->packetloss_measure_client);
	      pthread_mutex_unlock(&mutex_print);
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
			(q4s_session)->seq_num_client = atoi(s_seq_num);  // converts into int and stores
			memset(fragment, '\0', strlen(fragment));
			strcpy(copy_header, header);  // restores copy of header
      pthread_mutex_lock(&mutex_print);
			fprintf(stderr, "P%d: Sequence number of client stored: %d\n", n_process, (q4s_session)->seq_num_client);
      pthread_mutex_unlock(&mutex_print);
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
				(q4s_session)->packetloss_measure_client = atof(s_pl);  // converts into int and stores
				pthread_mutex_lock(&mutex_print);
				fprintf(stderr, "P%d: Packetloss measure of client stored: %.2f\n", n_process, (q4s_session)->packetloss_measure_client);
	      pthread_mutex_unlock(&mutex_print);
			}

			char *s_bw;
			strtok(NULL, "=");
			s_bw = strtok(NULL, "\n");  // stores string value
      if (strcmp(s_bw," ") != 0) {
				(q4s_session)->bw_measure_client = atoi(s_bw);  // converts into int and stores
				pthread_mutex_lock(&mutex_print);
				fprintf(stderr, "P%d: Bandwidth measure of client stored: %d\n", n_process, (q4s_session)->bw_measure_client);
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
			fprintf(stderr,"P%d: Sequence number confirmed: %d\n", n_process, (q4s_session)->seq_num_confirmed);
			pthread_mutex_unlock(&mutex_print);
		}
		strcpy(copy_start_line, start_line);  // restores copy of start line
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

	// If there is an alert pause parameter in the body
	if (fragment = strstr(copy_body, "a=recovery-pause:")){
		fragment = fragment + 17;  // moves to the beginning of the value
		char *recovery_pause;
		recovery_pause = strtok(fragment, "\n");  // stores string value
		(q4s_session)->recovery_pause = atoi(recovery_pause);  // converts into int and stores
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_body, body);  // restores copy of body
	}

	// If there is a measurement procedure parameter in the body
	if (fragment = strstr(copy_body, "a=measurement:procedure default(")) {
    fragment = fragment + 32;  // moves to the beginning of the value
		char *ping_interval_negotiation;
		strtok(fragment, "/");
		ping_interval_negotiation = strtok(NULL, ",");  // stores string value
		(q4s_session)->ping_clk_negotiation = atoi(ping_interval_negotiation);  // converts into int and stores
		char *ping_interval_continuity;
		strtok(NULL, "/");
		ping_interval_continuity = strtok(NULL, ",");  // stores string value
		(q4s_session)->ping_clk_continuity = atoi(ping_interval_continuity);  // converts into int and stores
		char *bwidth_interval;
		bwidth_interval = strtok(NULL, ",");  // stores string value
		(q4s_session)->bwidth_clk = atoi(bwidth_interval);  // converts into int and stores
		char *window_size_lj;
		strtok(NULL, "/");
		window_size_lj = strtok(NULL, ",");  // stores string value
		(q4s_session)->window_size_latency_jitter = atoi(window_size_lj);  // converts into int and stores
		char *window_size_pl;
		strtok(NULL, "/");
		window_size_pl = strtok(NULL, ")");  // stores string value
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

		if ((q4s_session)->bw_th[1] > 0) {
	    int target_bwidth = (q4s_session)->bw_th[1]; // kbps
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
			pthread_mutex_lock(&mutex_print);
			fprintf(stderr,"\nP%d: Messages fract per second: %f", n_process, messages_fract_per_ms);
			fprintf(stderr,"\nP%d: Messages int per second: %d", n_process, messages_int_per_ms);
			fprintf(stderr,"\nP%d: Bwidth threshold: %d", n_process, (q4s_session)->bw_th[1]);
			fprintf(stderr,"\nP%d: Bwidth clk: %d", n_process, (q4s_session)->bwidth_clk);
			fprintf(stderr,"\nP%d: Messages per ms: %d", n_process, (q4s_session)->bwidth_messages_per_ms);
			fprintf(stderr,"\nP%d: Ms per message (%d): %d\n", n_process, 1, (q4s_session)->ms_per_bwidth_message[0]);
			for (int i = 1; i < sizeof(ms_per_message); i++) {
				if (ms_per_message[i] > 0) {
					fprintf(stderr,"\nP%d: Ms per message (%d): %d\n", n_process, i+1, (q4s_session)->ms_per_bwidth_message[i]);
				} else {
					break;
				}
			}
			pthread_mutex_unlock(&mutex_print);
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

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

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
		  pthread_mutex_lock(&mutex_print);
		  fprintf(stderr, "\nP%d: Window size reached for latency measure\n", n_process);
		  pthread_mutex_unlock(&mutex_print);
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
		(q4s_session)->latency_measure_server = (ordered_window[median_elem1-1]
		  + ordered_window[median_elem2-1]) / 2;
	} else {
		int median_elem = (window_size + 1) / 2;
		(q4s_session)->latency_measure_server = ordered_window[median_elem-1];
	}
	pthread_mutex_lock(&mutex_print);
	fprintf(stderr,"P%d: Updated value of latency measure: %d\n", n_process, (q4s_session)->latency_measure_server);
	pthread_mutex_unlock(&mutex_print);
}

// Updates and stores latency measures
void update_jitter(type_q4s_session *q4s_session, int elapsed_time) {
	int window_size  = (q4s_session)->window_size_latency_jitter;

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

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
		  pthread_mutex_lock(&mutex_print);
		  fprintf(stderr, "\nP%d: Window size reached for jitter measure\n", n_process);
		  pthread_mutex_unlock(&mutex_print);
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
	(q4s_session)->jitter_measure_server = abs(elapsed_time - elapsed_time_mean);
	pthread_mutex_lock(&mutex_print);
	fprintf(stderr, "P%d: Updated value of jitter measure: %d\n", n_process, (q4s_session)->jitter_measure_server);
  pthread_mutex_unlock(&mutex_print);
}

// Updates and stores packetloss measures
void update_packetloss(type_q4s_session *q4s_session, int lost_packets) {
	int window_size  = (q4s_session)->window_size_packetloss;

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

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
		  pthread_mutex_lock(&mutex_print);
		  fprintf(stderr,"\nP%d: Window size reached for packetloss measure\n", n_process);
		  pthread_mutex_unlock(&mutex_print);
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
  (q4s_session)->packetloss_measure_server = ((float) num_losses / (float) window_size);
	pthread_mutex_lock(&mutex_print);
	fprintf(stderr,"P%d: Updated value of packetloss measure: %.2f\n", n_process, (q4s_session)->packetloss_measure_server);
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
	if (listen(server_socket_TCP, 1) < 0 ) { // listening
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


// Wait for client connection
void wait_new_connection(){
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

	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		pthread_mutex_lock(mutex_process);
		*state_proc1 = 2;
		pthread_mutex_unlock(mutex_process);

		pthread_mutex_lock(&mutex_print);
		printf("P1: Connected to %s:%d\n", inet_ntoa(client_TCP.sin_addr), htons(client_TCP.sin_port));
	  pthread_mutex_unlock(&mutex_print);
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		pthread_mutex_lock(mutex_process);
		*state_proc2 = 2;
		pthread_mutex_unlock(mutex_process);

		pthread_mutex_lock(&mutex_print);
		printf("P2: Connected to %s:%d\n", inet_ntoa(client_TCP.sin_addr), htons(client_TCP.sin_port));
	  pthread_mutex_unlock(&mutex_print);
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		pthread_mutex_lock(mutex_process);
		*state_proc3 = 2;
		pthread_mutex_unlock(mutex_process);

		pthread_mutex_lock(&mutex_print);
		printf("P3: Connected to %s:%d\n", inet_ntoa(client_TCP.sin_addr), htons(client_TCP.sin_port));
	  pthread_mutex_unlock(&mutex_print);
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		pthread_mutex_lock(mutex_process);
		*state_proc4 = 2;
		pthread_mutex_unlock(mutex_process);

		pthread_mutex_lock(&mutex_print);
		printf("P4: Connected to %s:%d\n", inet_ntoa(client_TCP.sin_addr), htons(client_TCP.sin_port));
	  pthread_mutex_unlock(&mutex_print);
	}

	// Initialize client parameters for UDP socket
	my_client_UDP.sin_family = AF_INET; // protocol assignment
	my_client_UDP.sin_port = htons(0); // port assignment
  my_client_UDP.sin_addr.s_addr = inet_addr("0.0.0.0"); // IP address assignment (automatic)
	memset(my_client_UDP.sin_zero, '\0', 8); // fills padding with 0s

	pthread_mutex_lock(&mutex_flags);
	flags |= FLAG_NEW_CONNECTION;
	pthread_mutex_unlock(&mutex_flags);
}

// Delivery of Q4S message to a Q4S client using TCP
void send_message_TCP (char prepared_message[MAXDATASIZE]) {
	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}
	pthread_mutex_lock(&mutex_buffer_TCP);
	memset(buffer_TCP, '\0', sizeof(buffer_TCP));
	// Copies the message into the buffer
	strncpy (buffer_TCP, prepared_message, MAXDATASIZE);
	pthread_mutex_unlock(&mutex_buffer_TCP);
	// Sends the message to the server using the TCP socket assigned
	if (send(client_socket_TCP, buffer_TCP, MAXDATASIZE, 0) < 0) {
		pthread_mutex_lock(&mutex_print);
		printf("P%d: Error when sending TCP data: %s\n", n_process, strerror(errno));
		pthread_mutex_unlock(&mutex_print);
		close(client_socket_TCP);
		exit(0);
		return;
	}
	pthread_mutex_lock(&mutex_buffer_TCP);
	memset(buffer_TCP, '\0', sizeof(buffer_TCP));
  pthread_mutex_unlock(&mutex_buffer_TCP);

	pthread_mutex_lock(&mutex_print);
	fprintf(stderr,"\nP%d: I have sent:\n%s\n", n_process, prepared_message);
	pthread_mutex_unlock(&mutex_print);
}

// Delivery of Q4S message to a Q4S client using UDP
void send_message_UDP (char prepared_message[MAXDATASIZE]) {
	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}
	pthread_mutex_lock(&mutex_buffer_UDP);
	memset(buffer_UDP, '\0', sizeof(buffer_UDP));
	// Copies the message into the buffer
	strncpy (buffer_UDP, prepared_message, MAXDATASIZE);
	pthread_mutex_unlock(&mutex_buffer_UDP);

	char copy_buffer_UDP[MAXDATASIZE];
	memset(copy_buffer_UDP, '\0', sizeof(copy_buffer_UDP));
	strncpy (copy_buffer_UDP, prepared_message, MAXDATASIZE);

  bool is_bwidth_message = false;
	char *start_line;
	start_line = strtok(copy_buffer_UDP, "\n"); // stores first line of message
	// If it is a Q4S PING, stores the current time
	if (strcmp(start_line, "PING q4s://www.example.com Q4S/1.0") == 0) {
		pthread_mutex_lock(&mutex_tm_latency);
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
		pthread_mutex_unlock(&mutex_tm_latency);
		if (result < 0) {
	  	pthread_mutex_lock(&mutex_print);
			printf("P%d: Error in clock_gettime(): %s\n", n_process, strerror(errno));
	  	pthread_mutex_unlock(&mutex_print);
			close(server_socket_UDP);
	  	return;
		}
	} else if (strcmp(start_line, "BWIDTH q4s://www.example.com Q4S/1.0") == 0) {
		is_bwidth_message = true;
  }

	// Sends the message to the server using the UDP socket assigned
	if (sendto(server_socket_UDP, buffer_UDP, MAXDATASIZE, 0, (struct sockaddr *) &my_client_UDP, slen) < 0) {
		pthread_mutex_lock(&mutex_print);
		printf("P%d: Error when sending UDP data: %s (%d)\n", n_process, strerror(errno), errno);
		pthread_mutex_unlock(&mutex_print);
		close(server_socket_UDP);
		exit(0);
		return;
	}
	pthread_mutex_lock(&mutex_buffer_UDP);
	memset(buffer_UDP, '\0', sizeof(buffer_UDP));
	pthread_mutex_unlock(&mutex_buffer_UDP);

	memset(copy_buffer_UDP, '\0', sizeof(copy_buffer_UDP));

	if (is_bwidth_message) {
		pthread_mutex_lock(&mutex_print);
		fprintf(stderr,"\nP%d: I have sent a Q4S BWIDTH!\n", n_process);
		pthread_mutex_unlock(&mutex_print);
	} else {
		pthread_mutex_lock(&mutex_print);
		fprintf(stderr,"\nP%d: I have sent:\n%s\n", n_process, prepared_message);
		pthread_mutex_unlock(&mutex_print);
	}
}


//-------------------------------------------------------
// THREADS
//-------------------------------------------------------

// RECEPTION FUNCTIONS

// Reception of Q4S messages (TCP socket)
// Thread function that checks if any message has arrived
void *thread_receives_TCP() {
	// Copy of the buffer (to avoid buffer modification)
	char copy_buffer_TCP[MAXDATASIZE];
	memset(copy_buffer_TCP, '\0', sizeof(copy_buffer_TCP));
	char copy_buffer_TCP_2[MAXDATASIZE];
	memset(copy_buffer_TCP_2, '\0', sizeof(copy_buffer_TCP_2));
	while(1) {
		int n_process;
		if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
			n_process = 1;
		} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
			n_process = 2;
		} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
			n_process = 3;
		} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
			n_process = 4;
		}
	  // If error occurs when receiving
	  if (recv(client_socket_TCP, buffer_TCP, MAXDATASIZE, MSG_WAITALL) < 0) {
			pthread_mutex_lock(&mutex_print);
		  printf("P%d: Error when receiving TCP data: %s\n", n_process, strerror(errno));
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
			pthread_mutex_lock(&mutex_flags);
			flags |= FLAG_FAILURE;
			pthread_mutex_unlock(&mutex_flags);
			continue;
		}

		pthread_mutex_lock(&mutex_session);
		received = true;
		pthread_mutex_unlock(&mutex_session);

		// Auxiliary variable to identify type of Q4S message
		char *start_line;
		start_line = strtok(copy_buffer_TCP_2, "\n"); // stores first line of message
	  if (strcmp(start_line, "BEGIN q4s://www.example.com Q4S/1.0") == 0) {

			pthread_mutex_lock(&mutex_print);
			printf("\nP%d: I have received a Q4S BEGIN!\n", n_process);
			pthread_mutex_unlock(&mutex_print);

		  pthread_mutex_lock(&mutex_flags);
		  flags |= FLAG_RECEIVE_BEGIN;
		  pthread_mutex_unlock(&mutex_flags);
	  } else if (strcmp(start_line, "READY q4s://www.example.com Q4S/1.0") == 0) {
			char header[500];
			memset(header, '\0', sizeof(header));
			pthread_mutex_lock(&mutex_session);
      strcpy(header, (&q4s_session.message_received)->header);
			pthread_mutex_unlock(&mutex_session);

			if (strstr(header, "Stage: 0") != NULL) {
				pthread_mutex_lock(&mutex_print);
			  printf("\nP%d: I have received a Q4S READY 0!\n", n_process);
				pthread_mutex_unlock(&mutex_print);

				pthread_mutex_lock(&mutex_flags);
			  flags |= FLAG_RECEIVE_READY0;
			  pthread_mutex_unlock(&mutex_flags);
			} else if (strstr(header, "Stage: 1") != NULL) {
				pthread_mutex_lock(&mutex_print);
			  printf("\nP%d: I have received a Q4S READY 1!\n", n_process);
				pthread_mutex_unlock(&mutex_print);

				pthread_mutex_lock(&mutex_flags);
			  flags |= FLAG_RECEIVE_READY1;
			  pthread_mutex_unlock(&mutex_flags);
			} else if (strstr(header, "Stage: 2") != NULL) {
				pthread_mutex_lock(&mutex_print);
			  printf("\nP%d: I have received a Q4S READY 2!\n", n_process);
				pthread_mutex_unlock(&mutex_print);

				pthread_mutex_lock(&mutex_flags);
			  flags |= FLAG_RECEIVE_READY2;
			  pthread_mutex_unlock(&mutex_flags);
			} else {
				pthread_mutex_unlock(&mutex_session);

				pthread_mutex_lock(&mutex_print);
			  fprintf(stderr,"\nP%d: I have received a Q4S READY without stage specified\n", n_process);
				pthread_mutex_unlock(&mutex_print);
		  }
		} else if (strcmp(start_line, "Q4S/1.0 200 OK") == 0) {

			pthread_mutex_lock(&mutex_print);
			fprintf(stderr,"\nP%d: I have received a Q4S 200 OK!\n", n_process);
			pthread_mutex_unlock(&mutex_print);

			pthread_mutex_lock(&mutex_flags);
			flags |= FLAG_RECEIVE_OK;
			pthread_mutex_unlock(&mutex_flags);
		// If it is a Q4S BWIDTH message
	  } else if (strcmp(start_line, "CANCEL q4s://www.example.com Q4S/1.0") == 0) {

			pthread_mutex_lock(&mutex_print);
			 printf("\nP%d: I have received a Q4S CANCEL!\n", n_process);
			pthread_mutex_unlock(&mutex_print);

		  pthread_mutex_lock(&mutex_flags);
		  flags |= FLAG_RECEIVE_CANCEL;
		  pthread_mutex_unlock(&mutex_flags);
		} else {
			pthread_mutex_lock(&mutex_print);
		  fprintf(stderr,"\nP%d: I have received an unidentified message\n", n_process);
      pthread_mutex_unlock(&mutex_print);
		}
		memset(copy_buffer_TCP, '\0', sizeof(copy_buffer_TCP));
		memset(copy_buffer_TCP_2, '\0', sizeof(copy_buffer_TCP_2));
		if (cancel_TCP_thread) {
			break;
		}
  }
}

// Reception of Q4S messages (UDP socket)
// Thread function that checks if any message has arrived
void *thread_receives_UDP() {
	// Copy of the buffer (to avoid buffer modification)
	char copy_buffer_UDP[MAXDATASIZE];
	memset(copy_buffer_UDP, '\0', sizeof(copy_buffer_UDP));
	char copy_buffer_UDP_2[MAXDATASIZE];
	memset(copy_buffer_UDP_2, '\0', sizeof(copy_buffer_UDP_2));
	while(1) {
		int n_process;
		if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
			n_process = 1;
		} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
			n_process = 2;
		} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
			n_process = 3;
		} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
			n_process = 4;
		}
	  // If error occurs when receiving
	  if (recvfrom(server_socket_UDP, buffer_UDP, MAXDATASIZE, MSG_WAITALL,
			(struct sockaddr *) &client_UDP, &slen) < 0) {
			pthread_mutex_lock(&mutex_print);
		  printf("P%d: Error when receiving UDP data: %s\n", n_process, strerror(errno));
			pthread_mutex_unlock(&mutex_print);
		  break;
    }

		pthread_mutex_lock(&mutex_buffer_UDP);
		strcpy(copy_buffer_UDP, buffer_UDP);
		strcpy(copy_buffer_UDP_2, buffer_UDP);
		memset(buffer_UDP, '\0', sizeof(buffer_UDP));
		pthread_mutex_unlock(&mutex_buffer_UDP);

		// Redirects message to other process if necessary
		pthread_mutex_lock(&mutex_session);
		bool redirected = redirect(copy_buffer_UDP, client_UDP);
		pthread_mutex_unlock(&mutex_session);

		strcpy(copy_buffer_UDP, copy_buffer_UDP_2);

		if (redirected) {
			pthread_mutex_lock(&mutex_print);
			fprintf(stderr,"\nP%d: I have redirected a message received\n", n_process);
			pthread_mutex_unlock(&mutex_print);
			continue;
		}

		pthread_mutex_lock(&mutex_session);
		// Validates and stores the received message to be analized later
		bool stored = store_message(copy_buffer_UDP, &q4s_session.message_received);
		pthread_mutex_unlock(&mutex_session);

		if (!stored) {
			pthread_mutex_lock(&mutex_flags);
			flags |= FLAG_FAILURE;
			pthread_mutex_unlock(&mutex_flags);
			continue;
		}

		pthread_mutex_lock(&mutex_session);
		if (strcmp(inet_ntoa(my_client_UDP.sin_addr), "0.0.0.0") == 0 || ntohs(my_client_UDP.sin_port) == 0) {
			my_client_UDP = client_UDP;
			fprintf(stderr, "\nP%d: My_client_UDP stored: %s:%d\n", n_process, inet_ntoa(my_client_UDP.sin_addr), ntohs(my_client_UDP.sin_port));
		}
		received = true;
		pthread_mutex_unlock(&mutex_session);

		// Auxiliary variable to identify type of Q4S message
		char *start_line;
		start_line = strtok(copy_buffer_UDP_2, "\n"); // stores first line of message
	  if (strcmp(start_line, "PING q4s://www.example.com Q4S/1.0") == 0) {
      char field_seq_num[20];
			memset(field_seq_num, '\0', sizeof(field_seq_num));
			strcpy(field_seq_num, "Sequence-Number: ");
			char header[500];
			memset(header, '\0', sizeof(header));
			pthread_mutex_lock(&mutex_session);
			strcpy(header, (&q4s_session.message_received)->header);
			pthread_mutex_unlock(&mutex_session);

			if (strstr(header, field_seq_num) != NULL) {
				pthread_mutex_lock(&mutex_print);
				fprintf(stderr,"\nP%d: I have received a Q4S PING!\n", n_process);
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
					printf("P%d: Error in clock_gettime(): %s\n", n_process, strerror(errno));
					pthread_mutex_unlock(&mutex_print);
					close(server_socket_UDP);
				}
				pthread_mutex_unlock(&mutex_tm_jitter);

				pthread_mutex_lock(&mutex_flags);
				flags |= FLAG_RECEIVE_PING;
				pthread_mutex_unlock(&mutex_flags);
			} else {
				pthread_mutex_lock(&mutex_print);
				fprintf(stderr,"\nP%d: I have received a Q4S PING without sequence number specified\n", n_process);
				pthread_mutex_unlock(&mutex_print);
			}
		} else if (strcmp(start_line, "Q4S/1.0 200 OK") == 0) {
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
					printf("P%d: Error in clock_gettime(): %s\n", n_process, strerror(errno));
			    pthread_mutex_unlock(&mutex_print);
					close(server_socket_UDP);
				}
				pthread_mutex_unlock(&mutex_tm_latency);

				pthread_mutex_lock(&mutex_print);
				fprintf(stderr,"\nP%d: I have received a Q4S 200 OK!\n", n_process);
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
			  fprintf(stderr,"\nP%d: I have received a Q4S 200 OK without sequence number specified\n", n_process);
				pthread_mutex_unlock(&mutex_print);
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
				pthread_mutex_lock(&mutex_print);
			  fprintf(stderr,"\nP%d: I have received a Q4S BWIDTH!\n", n_process);
	      pthread_mutex_unlock(&mutex_print);

				int seq_num_before;
				int seq_num_after;
				int num_losses;

				pthread_mutex_lock(&mutex_session);
				if (num_bwidth_received > 0) {
					num_bwidth_received++;
				}
				seq_num_before = (&q4s_session)->seq_num_client;
	      // Stores parameters of the message (included Sequence Number)
				store_parameters(&q4s_session, &(q4s_session.message_received));
				seq_num_after = (&q4s_session)->seq_num_client;
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

			} else {
				pthread_mutex_lock(&mutex_print);
			  fprintf(stderr,"\nP%d: I have received a Q4S BWIDTH without sequence number specified\n", n_process);
				pthread_mutex_unlock(&mutex_print);
			}

		} else {
			pthread_mutex_lock(&mutex_print);
		  fprintf(stderr,"\nP%d: I have received an unidentified message\n", n_process);
      pthread_mutex_unlock(&mutex_print);
		}
		memset(copy_buffer_UDP, '\0', sizeof(copy_buffer_UDP));
		memset(copy_buffer_UDP_2, '\0', sizeof(copy_buffer_UDP_2));
		if (cancel_UDP_thread) {
			break;
		}
	}
}

// Reception of redirected Q4S messages (UDP socket)
// Thread function that checks if any redirected message has arrived
void *thread_receives_redirected() {
	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}
	// Copy of the buffer (to avoid buffer modification)
	char copy_buffer_redirected[MAXDATASIZE];
	memset(copy_buffer_redirected, '\0', sizeof(copy_buffer_redirected));
	char copy_buffer_redirected_2[MAXDATASIZE];
	memset(copy_buffer_redirected_2, '\0', sizeof(copy_buffer_redirected_2));

	while(1) {
		memset(copy_buffer_redirected, '\0', sizeof(copy_buffer_redirected));
		memset(copy_buffer_redirected_2, '\0', sizeof(copy_buffer_redirected_2));
		if (n_process == 1) {
			pthread_mutex_lock(mutex_process);
			strcpy(copy_buffer_redirected, buffer_proc1);
			memset(buffer_proc1, '\0', MAXDATASIZE);
			pthread_mutex_unlock(mutex_process);
		} else if (n_process == 2) {
			pthread_mutex_lock(mutex_process);
			strcpy(copy_buffer_redirected, buffer_proc2);
			memset(buffer_proc2, '\0', MAXDATASIZE);
			pthread_mutex_unlock(mutex_process);
		} else if (n_process == 3) {
			pthread_mutex_lock(mutex_process);
			strcpy(copy_buffer_redirected, buffer_proc3);
			memset(buffer_proc3, '\0', MAXDATASIZE);
			pthread_mutex_unlock(mutex_process);
		} else if (n_process == 4) {
			pthread_mutex_lock(mutex_process);
			strcpy(copy_buffer_redirected, buffer_proc4);
			memset(buffer_proc4, '\0', MAXDATASIZE);
			pthread_mutex_unlock(mutex_process);
		}

		if (!strlen(copy_buffer_redirected) > 0) {
			continue;
		}

		char s_id[20];
		pthread_mutex_lock(&mutex_session);
		sprintf(s_id, "%d", q4s_session.session_id);
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_print);
		fprintf(stderr,"\nP%d: I have received a redirected message\n", n_process);
		pthread_mutex_unlock(&mutex_print);

		strcpy(copy_buffer_redirected_2, copy_buffer_redirected);

		pthread_mutex_lock(&mutex_session);
		// Validates and stores the received message to be analized later
		bool stored = store_message(copy_buffer_redirected, &q4s_session.message_received);
		pthread_mutex_unlock(&mutex_session);

		if (!stored) {
			pthread_mutex_lock(&mutex_flags);
			flags |= FLAG_FAILURE;
			pthread_mutex_unlock(&mutex_flags);
			continue;
		}

		struct sockaddr_in client_UDP;

		if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
			pthread_mutex_lock(mutex_process);
			client_UDP = *client_proc1;
			memset(client_proc1, 0, sizeof(*client_proc1));
			pthread_mutex_unlock(mutex_process);
		} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
			pthread_mutex_lock(mutex_process);
			client_UDP = *client_proc2;
			memset(client_proc2, 0, sizeof(*client_proc2));
			pthread_mutex_unlock(mutex_process);
		} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
			pthread_mutex_lock(mutex_process);
			client_UDP = *client_proc3;
			memset(client_proc3, 0, sizeof(*client_proc3));
			pthread_mutex_unlock(mutex_process);
		} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
			pthread_mutex_lock(mutex_process);
			client_UDP = *client_proc4;
			memset(client_proc4, 0, sizeof(*client_proc4));
			pthread_mutex_unlock(mutex_process);
		}

		pthread_mutex_lock(&mutex_session);
		if (strcmp(inet_ntoa(my_client_UDP.sin_addr), "0.0.0.0") == 0 || ntohs(my_client_UDP.sin_port) == 0) {
				my_client_UDP = client_UDP;
				fprintf(stderr, "\nP%d: My_client_UDP stored: %s:%d\n", n_process, inet_ntoa(my_client_UDP.sin_addr), ntohs(my_client_UDP.sin_port));
		}
		received = true;
		pthread_mutex_unlock(&mutex_session);

		// Auxiliary variable to identify type of Q4S message
		char *start_line;
		start_line = strtok(copy_buffer_redirected_2, "\n"); // stores first line of message
	  if (strcmp(start_line, "PING q4s://www.example.com Q4S/1.0") == 0) {
      char field_seq_num[20];
			memset(field_seq_num, '\0', sizeof(field_seq_num));
			strcpy(field_seq_num, "Sequence-Number: ");
			char header[500];
			memset(header, '\0', sizeof(header));
			pthread_mutex_lock(&mutex_session);
			strcpy(header, (&q4s_session.message_received)->header);
			pthread_mutex_unlock(&mutex_session);

			if (strstr(header, field_seq_num) != NULL) {
				pthread_mutex_lock(&mutex_print);
				fprintf(stderr,"\nP%d: I have received a Q4S PING!\n", n_process);
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
					printf("P%d: Error in clock_gettime(): %s\n", n_process, strerror(errno));
					pthread_mutex_unlock(&mutex_print);
					close(server_socket_UDP);
				}
				pthread_mutex_unlock(&mutex_tm_jitter);

				pthread_mutex_lock(&mutex_flags);
				flags |= FLAG_RECEIVE_PING;
				pthread_mutex_unlock(&mutex_flags);
			} else {
				pthread_mutex_lock(&mutex_print);
				fprintf(stderr,"\nP%d: I have received a Q4S PING without sequence number specified\n", n_process);
				pthread_mutex_unlock(&mutex_print);
			}
		} else if (strcmp(start_line, "Q4S/1.0 200 OK") == 0) {
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
					printf("P%d: Error in clock_gettime(): %s\n", n_process, strerror(errno));
			    pthread_mutex_unlock(&mutex_print);
					close(server_socket_UDP);
				}
				pthread_mutex_unlock(&mutex_tm_latency);

				pthread_mutex_lock(&mutex_print);
				fprintf(stderr,"\nP%d: I have received a Q4S 200 OK!\n", n_process);
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
			  fprintf(stderr,"\nP%d: I have received a Q4S 200 OK without sequence number specified\n", n_process);
				pthread_mutex_unlock(&mutex_print);
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
				pthread_mutex_lock(&mutex_print);
			  fprintf(stderr,"\nP%d: I have received a Q4S BWIDTH!\n", n_process);
	      pthread_mutex_unlock(&mutex_print);

				int seq_num_before;
				int seq_num_after;
				int num_losses;

				pthread_mutex_lock(&mutex_session);
				if (num_bwidth_received > 0) {
					num_bwidth_received++;
				}
				seq_num_before = (&q4s_session)->seq_num_client;
	      // Stores parameters of the message (included Sequence Number)
				store_parameters(&q4s_session, &(q4s_session.message_received));
				seq_num_after = (&q4s_session)->seq_num_client;
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

			} else {
				pthread_mutex_lock(&mutex_print);
			  fprintf(stderr,"\nP%d: I have received a Q4S BWIDTH without sequence number specified\n", n_process);
				pthread_mutex_unlock(&mutex_print);
			}

		} else {
			pthread_mutex_lock(&mutex_print);
		  fprintf(stderr,"\nP%d: I have received an unidentified message\n", n_process);
      pthread_mutex_unlock(&mutex_print);
		}
		memset(copy_buffer_redirected, '\0', sizeof(copy_buffer_redirected));
		memset(copy_buffer_redirected_2, '\0', sizeof(copy_buffer_redirected_2));
		if (cancel_redirected_thread) {
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
		ping_clk = q4s_session.ping_clk_negotiation;
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
	pthread_mutex_lock(&mutex_session);
	q4s_session.alert_pause_activated = true;
	int pause_interval = q4s_session.alert_pause;
	pthread_mutex_unlock(&mutex_session);

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

	pthread_mutex_lock(&mutex_print);
	fprintf(stderr,"\nP%d: Alert pause started\n", n_process);
	pthread_mutex_unlock(&mutex_print);

  delay(pause_interval);

	pthread_mutex_lock(&mutex_session);
	q4s_session.alert_pause_activated = false;
	pthread_mutex_unlock(&mutex_session);

	pthread_mutex_lock(&mutex_print);
	fprintf(stderr,"\nP%d: Alert pause finished\n", n_process);
	pthread_mutex_unlock(&mutex_print);

	pthread_mutex_lock(&mutex_session);
	if (q4s_session.stage == 2
		&& (q4s_session.latency_th <= 0 || q4s_session.latency_measure_server <= q4s_session.latency_th)
	  && (q4s_session.latency_th <= 0 || q4s_session.latency_measure_client <= q4s_session.latency_th)
		&& (q4s_session.jitter_th[0] <= 0 || q4s_session.jitter_measure_server <= q4s_session.jitter_th[0])
    && (q4s_session.jitter_th[1] <= 0 || q4s_session.jitter_measure_client <= q4s_session.jitter_th[1])
		&& (q4s_session.bw_th[0] <= 0 || q4s_session.bw_measure_server >= q4s_session.bw_th[0])
    && (q4s_session.bw_th[1] <= 0 || q4s_session.bw_measure_client >= q4s_session.bw_th[1])
		&& (q4s_session.packetloss_th[0] <= 0 || q4s_session.packetloss_measure_server <= q4s_session.packetloss_th[0])
	  && (q4s_session.packetloss_th[1] <= 0 || q4s_session.packetloss_measure_client <= q4s_session.packetloss_th[1])) {

    // Starts timer for recovery pause
		cancel_timer_recover = false;
		pthread_create(&timer_recover, NULL, (void*)recovery_pause_timeout, NULL);
	}
	pthread_mutex_unlock(&mutex_session);
}

// Manages timeout of recovery pause
void *recovery_pause_timeout() {
	pthread_mutex_lock(&mutex_session);
	int pause_interval = q4s_session.recovery_pause;
	pthread_mutex_unlock(&mutex_session);

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

	pthread_mutex_lock(&mutex_print);
	fprintf(stderr,"\nP%d: Recovery pause started\n", n_process);
	pthread_mutex_unlock(&mutex_print);

  delay(pause_interval);

	pthread_mutex_lock(&mutex_print);
	fprintf(stderr,"\nP%d: Recovery pause finished\n", n_process);
	pthread_mutex_unlock(&mutex_print);

	pthread_mutex_lock(&mutex_session);
	if ((q4s_session.latency_th <= 0 || q4s_session.latency_measure_server <= q4s_session.latency_th)
	  && (q4s_session.latency_th <= 0 || q4s_session.latency_measure_client <= q4s_session.latency_th)
		&& (q4s_session.jitter_th[0] <= 0 || q4s_session.jitter_measure_server <= q4s_session.jitter_th[0])
    && (q4s_session.jitter_th[1] <= 0 || q4s_session.jitter_measure_client <= q4s_session.jitter_th[1])
		&& (q4s_session.bw_th[0] <= 0 || q4s_session.bw_measure_server >= q4s_session.bw_th[0])
    && (q4s_session.bw_th[1] <= 0 || q4s_session.bw_measure_client >= q4s_session.bw_th[1])
		&& (q4s_session.packetloss_th[0] <= 0 || q4s_session.packetloss_measure_server <= q4s_session.packetloss_th[0])
	  && (q4s_session.packetloss_th[1] <= 0 || q4s_session.packetloss_measure_client <= q4s_session.packetloss_th[1])) {
			pthread_mutex_lock(&mutex_flags);
			flags |= FLAG_RECOVER;
			pthread_mutex_unlock(&mutex_flags);
	}
	pthread_mutex_unlock(&mutex_session);
}

// Activates a flag when expiration time has passed
void *expiration_timeout() {
	int expiration_time;
	int counter = 10;
	pthread_mutex_lock(&mutex_session);
	expiration_time = q4s_session.expiration_time;
	pthread_mutex_unlock(&mutex_session);

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

	while(1) {
		if (cancel_timer_expire) {
			break;
		}
		delay(expiration_time/10);
		counter--;
		pthread_mutex_lock(&mutex_session);
		if (received) {
			received = false;
			pthread_mutex_unlock(&mutex_session);
			counter = 10;
		} else if (counter <= 0) {
			pthread_mutex_unlock(&mutex_session);

			pthread_mutex_lock(&mutex_print);
			printf("\nP%d: Expiration time has finished\n", n_process);
			pthread_mutex_unlock(&mutex_print);

			pthread_mutex_lock(&mutex_flags);
			flags |= FLAG_RECEIVE_CANCEL;
			pthread_mutex_unlock(&mutex_flags);

			break;
		} else {
			pthread_mutex_unlock(&mutex_session);
		}
	}
}

// Manages timeout of bandwidth reception
void *bwidth_reception_timeout() {

	pthread_mutex_lock(&mutex_session);
	bwidth_reception_timeout_activated = true;
	int bwidth_clk = q4s_session.bwidth_clk; // time established for BWIDTH measure
	pthread_mutex_unlock(&mutex_session);

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

	pthread_mutex_lock(&mutex_print);
  fprintf(stderr,"\nP%d: Interval of Q4S BWIDTH reception has started\n", n_process);
	pthread_mutex_unlock(&mutex_print);

	delay(bwidth_clk);

	pthread_mutex_lock(&mutex_print);
  fprintf(stderr,"\nP%d: Interval of Q4S BWIDTH reception has finished\n", n_process);
	pthread_mutex_unlock(&mutex_print);

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
	num_messages_to_send = ceil((float) ((&q4s_session)->bw_th[1] * (&q4s_session)->bwidth_clk) / (float) (MESSAGE_BWIDTH_SIZE * 8));
	num_messages_sent = 0;
	q4s_session.seq_num_server = 0;
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

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

	pthread_mutex_lock(&mutex_print);
	fprintf(stderr,"\nP%d: Bwidth clk: %d", n_process, bwidth_clk);
	fprintf(stderr,"\nP%d: Messages per ms: %d", n_process, messages_per_ms);
	fprintf(stderr,"\nP%d: Ms per message (%d): %d\n", n_process, 1, ms_per_message[0]);
	for (int i = 1; i < sizeof(ms_per_message); i++) {
		if (ms_per_message[i] > 0) {
			fprintf(stderr,"\nP%d: Ms per message (%d): %d\n", n_process, i+1, ms_per_message[i]);
		} else {
			break;
		}
	}
	pthread_mutex_unlock(&mutex_print);

	// Start of Q4S BWIDTH delivery
	while(1) {
		if (ms_iterated == 0) {
			result = clock_gettime(CLOCK_REALTIME, &tm1_adjust);
		  if (result < 0) {
				pthread_mutex_lock(&mutex_print);
		    printf("P%d: Error in clock_gettime(): %s\n", n_process, strerror(errno));
				pthread_mutex_unlock(&mutex_print);
		  }
			result = clock_gettime(CLOCK_REALTIME, &tm1_jitter);
		  if (result < 0) {
				pthread_mutex_lock(&mutex_print);
		    printf("P%d: Error in clock_gettime(): %s\n", n_process, strerror(errno));
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
			(&q4s_session)->seq_num_server++;
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
				(&q4s_session)->seq_num_server++;
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
		    printf("P%d: Error in clock_gettime(): %s\n", n_process, strerror(errno));
				pthread_mutex_unlock(&mutex_print);
		  }

			elapsed_time = ms_elapsed(tm1_jitter, tm2_jitter);

			pthread_mutex_lock(&mutex_print);
	    fprintf(stderr,"\nP%d: A whole burst of Q4S BWIDTH has been sent: %d messages\n", n_process, num_messages_sent);
			fprintf(stderr,"P%d: Interval for delivery elapsed: %d\n", n_process, elapsed_time);
			fprintf(stderr,"P%d: Theoric interval for delivery: %d\n", n_process, bwidth_clk);
			fprintf(stderr,"P%d: Ms passed: %d\n", n_process, us_passed_total/1000);
			fprintf(stderr,"P%d: Ms to subtract: %d\n", n_process, us_to_subtract/1000);
			pthread_mutex_unlock(&mutex_print);

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
	    printf("P%d: Error in clock_gettime(): %s\n", n_process, strerror(errno));
			pthread_mutex_unlock(&mutex_print);
	  }

		elapsed_time = us_elapsed(tm1_adjust, tm2_adjust);

		result = clock_gettime(CLOCK_REALTIME, &tm1_adjust);
		if (result < 0) {
			pthread_mutex_lock(&mutex_print);
			printf("P%d: Error in clock_gettime(): %s\n", n_process, strerror(errno));
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

//--------------------------------------------------------
// CHECK FUNCTIONS OF STATE MACHINE
//--------------------------------------------------------

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

// Checks if there is a failure
int check_failure (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_FAILURE);
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
	if (result && !server_ready) {
		result = 0;
	}
  return result;
}

// Checks if client wants to start stage 1 (Q4S READY 1 is received)
int check_receive_ready1 (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_RECEIVE_READY1);
  pthread_mutex_unlock(&mutex_flags);
	if (result && !server_ready) {
		result = 0;
	}
  return result;
}

// Checks if client wants to start stage 2 (Q4S READY 2 is received)
int check_receive_ready2 (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_RECEIVE_READY2);
  pthread_mutex_unlock(&mutex_flags);
	if (result && !server_ready) {
		result = 0;
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


// Checks if an alert has to be triggered
int check_alert (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_ALERT);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if a recovery message has to be triggered
int check_recover (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_RECOVER);
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

// Checks if server has sent a Q4S BWIDTH burst
int check_bwidth_burst_sent (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_BWIDTH_BURST_SENT);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if q4s server has received a Q4S BWIDTH
int check_receive_bwidth (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_RECEIVE_BWIDTH);
  pthread_mutex_unlock(&mutex_flags);
	return result;
}

// Checks if q4s server has to measure bwidth
int check_measure_bwidth (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	result = (flags & FLAG_MEASURE_BWIDTH);
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

//---------------------------------------------------------
// ACTION FUNCTIONS OF STATE MACHINE
//---------------------------------------------------------

// Prepares for Q4S session
void Setup (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	// Puts every FLAG to 0
	flags = 0;
	pthread_mutex_unlock(&mutex_flags);

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

	pthread_mutex_lock(&mutex_session);
	// Initialize auxiliary variables
	initial_qos_level[0] = 0;
	initial_qos_level[1] = 0;
	num_packet_lost = 0;
	pos_latency = 0;
	pos_elapsed_time = 0;
	pos_packetloss = 0;
	num_ping = 0;
	num_packet_since_alert = 0;
	num_bwidth_received = 0;
	failure_code = 0;
	num_failures = 0;
	// Initialize session variables
  q4s_session.session_id = -1;
	q4s_session.stage = -1;
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
	q4s_session.jitter_measure_server = -1;
	q4s_session.jitter_measure_client = -1;
	q4s_session.packetloss_measure_server = -1;
	q4s_session.packetloss_measure_client = -1;
	q4s_session.bw_measure_server = -1;
	q4s_session.bw_measure_client = -1;
	pthread_mutex_unlock(&mutex_session);

	pthread_mutex_lock(&mutex_print);
	printf("\nP%d: Whenever you want to set new parameters in a Q4S 200 OK, press 'n'", n_process);
  pthread_mutex_unlock(&mutex_print);

	pthread_mutex_lock(&mutex_print);
	printf("\nP%d: Waiting for a Q4S BEGIN from the client\n", n_process);
  pthread_mutex_unlock(&mutex_print);

  // Throws a thread to check the arrival of Q4S messages using TCP
	cancel_TCP_thread = false;
	pthread_create(&receive_TCP_thread, NULL, (void*)thread_receives_TCP, NULL);
}

// Creates and sends a Q4S 4xx/5xx/6xx message to the client
void Send_Failure (fsm_t* fsm) {
	pthread_mutex_lock(&mutex_flags);
	flags &= ~FLAG_FAILURE;
	pthread_mutex_unlock(&mutex_flags);

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

	int failure = failure_code;

	if (failure == 400) {
		char sintax_problem[50];
		strcpy(sintax_problem, sintax_problem_400);
		// Fills q4s_session.message_to_send with the Q4S 400 parameters
		create_400(&q4s_session.message_to_send, sintax_problem);
		// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
		prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
		// Sends the message to the Q4S client
		send_message_TCP(q4s_session.prepared_message);
	} else if (failure == 413) {
		// Fills q4s_session.message_to_send with the Q4S 413 parameters
		create_413(&q4s_session.message_to_send);
		// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
		prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
		// Sends the message to the Q4S client
		send_message_TCP(q4s_session.prepared_message);
	} else if (failure == 414) {
		// Fills q4s_session.message_to_send with the Q4S 414 parameters
		create_414(&q4s_session.message_to_send);
		// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
		prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
		// Sends the message to the Q4S client
		send_message_TCP(q4s_session.prepared_message);
	} else if (failure == 415) {
		// Fills q4s_session.message_to_send with the Q4S 415 parameters
		create_415(&q4s_session.message_to_send);
		// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
		prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
		// Sends the message to the Q4S client
		send_message_TCP(q4s_session.prepared_message);
	} else if (failure == 416) {
		// Fills q4s_session.message_to_send with the Q4S 416 parameters
		create_416(&q4s_session.message_to_send);
		// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
		prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
		// Sends the message to the Q4S client
		send_message_TCP(q4s_session.prepared_message);
	} else if (failure == 501) {
		// Fills q4s_session.message_to_send with the Q4S 501 parameters
		create_501(&q4s_session.message_to_send);
		// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
		prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
		// Sends the message to the Q4S client
		send_message_TCP(q4s_session.prepared_message);
	} else if (failure == 505) {
		// Fills q4s_session.message_to_send with the Q4S 505 parameters
		create_505(&q4s_session.message_to_send);
		// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
		prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
		// Sends the message to the Q4S client
		send_message_TCP(q4s_session.prepared_message);
	} else if (failure == 600) {
		// Fills q4s_session.message_to_send with the Q4S 600 parameters
		create_600(&q4s_session.message_to_send);
		// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
		prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
		// Sends the message to the Q4S client
		send_message_TCP(q4s_session.prepared_message);
	} else if (failure == 601) {
		// Fills q4s_session.message_to_send with the Q4S 601 parameters
		create_601(&q4s_session.message_to_send);
		// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
		prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
		// Sends the message to the Q4S client
		send_message_TCP(q4s_session.prepared_message);
	}

	num_failures++;
	if (num_failures >= 3) {
		pthread_mutex_lock(&mutex_print);
		printf("\nP%d: 3 or more failures have occurred in this session\n", n_process);
		pthread_mutex_unlock(&mutex_print);

		pthread_mutex_lock(&mutex_flags);
		flags |= FLAG_RECEIVE_CANCEL;
		pthread_mutex_unlock(&mutex_flags);
	}

}

// Creates and sends a Q4S 200 OK message to the client
void Respond_OK (fsm_t* fsm) {
	char request_method[20];
	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}
	pthread_mutex_lock(&mutex_flags);
	if (flags & FLAG_RECEIVE_BEGIN) {
		flags &= ~FLAG_RECEIVE_BEGIN;
	  pthread_mutex_unlock(&mutex_flags);
		// Indicates request method received
		strcpy(request_method, "BEGIN");

		pthread_mutex_lock(&mutex_session);
		// Stores parameters of the Q4S BEGIN (if present)
		store_parameters(&q4s_session, &(q4s_session.message_received));

		pthread_mutex_lock(&mutex_print);
		printf("\nP%d: Creating a Q4S 200 OK\n", n_process);
		pthread_mutex_unlock(&mutex_print);

		// Fills q4s_session.message_to_send with the Q4S 200 OK parameters
		create_200 (&q4s_session.message_to_send, request_method, set_new_parameters);
		set_new_parameters = false;
		// Stores parameters added to the message
		store_parameters(&q4s_session, &(q4s_session.message_to_send));
		initial_qos_level[0] = q4s_session.qos_level[0];
		initial_qos_level[1] = q4s_session.qos_level[1];
	  // Converts q4s_session.message_to_send into a message with correct format (prepared_message)
		prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
		// Sends the message to the Q4S client
		send_message_TCP(q4s_session.prepared_message);
		pthread_mutex_unlock(&mutex_session);
		server_ready = true;

		// Throws a thread to control expiration timeout
		cancel_timer_expire = false;
		pthread_create(&timer_expire, NULL, (void*)expiration_timeout, NULL);
		// Throws a thread to check the arrival of Q4S messages using UDP
		cancel_UDP_thread = false;
		pthread_create(&receive_UDP_thread, NULL, (void*)thread_receives_UDP, NULL);
		// Throws a thread to check the arrival of redirected messages
		cancel_redirected_thread = false;
		pthread_create(&receive_redirected_thread, NULL, (void*)thread_receives_redirected, NULL);


	} else if (flags & FLAG_RECEIVE_READY0) {
		// Puts every FLAG to 0
		flags = 0;
		pthread_mutex_unlock(&mutex_flags);
		server_ready = false;
		// Indicates request method received
		strcpy(request_method, "READY0");

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
		num_packet_since_alert = 0;
		num_bwidth_received = 0;
		// Initialize session variables
		q4s_session.stage = 0;
		q4s_session.seq_num_server = 0;
		q4s_session.seq_num_client = -1;
		q4s_session.jitter_measure_server = -1;
		q4s_session.jitter_measure_client = -1;
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
		server_ready = false;
		// Indicates request method received
		strcpy(request_method, "READY1");

		pthread_mutex_lock(&mutex_session);
		// Initialize auxiliary variables
		num_packet_lost = 0;
		num_ping = 0;
		num_packet_since_alert = 0;
		num_bwidth_received = 0;
		// Initialize session variables
		q4s_session.stage = 1;
		q4s_session.seq_num_server = 0;
		q4s_session.seq_num_client = -1;
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
	} else if (flags & FLAG_RECEIVE_READY2) {
		// Puts every FLAG to 0
		flags = 0;
		pthread_mutex_unlock(&mutex_flags);
		server_ready = false;
		// Indicates request method received
		strcpy(request_method, "READY2");

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
		num_packet_since_alert = 0;
		num_bwidth_received = 0;
		// Initialize session variables
		q4s_session.stage = 2;
		q4s_session.seq_num_server = 0;
		q4s_session.seq_num_client = -1;
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
  } else if (flags & FLAG_RECEIVE_PING) {
		flags &= ~FLAG_RECEIVE_PING;
		pthread_mutex_unlock(&mutex_flags);

		pthread_mutex_lock(&mutex_session);
		int num_losses = num_packet_lost;
		num_packet_lost = 0;
		if ((&q4s_session)->packetloss_th[0] > 0) {
			if (num_losses > 0) {
				pthread_mutex_lock(&mutex_print);
				fprintf(stderr,"\nP%d: Loss of %d Q4S PING(s) detected\n", n_process, num_losses);
				pthread_mutex_unlock(&mutex_print);
			}
			update_packetloss(&q4s_session, num_losses);
		}
		// Initialize ping period if necessary
		if (q4s_session.ping_clk_negotiation <= 0) {
			q4s_session.ping_clk_negotiation = 100;
		}
		// Initialize ping period if necessary
		if (q4s_session.ping_clk_continuity <= 0) {
			q4s_session.ping_clk_continuity = 100;
		}
		int stage = q4s_session.stage;
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_print);
		fprintf(stderr,"\nP%d: We are in Stage %d\n", n_process, stage);
		pthread_mutex_unlock(&mutex_print);

		// Indicates request method received
		strcpy(request_method, "PING");

		if (stage == 2) {
			pthread_mutex_lock(&mutex_session);
			printf("\nP%d: Stage 2 has started -> Q4S PING exchange while application execution\n", n_process);
			pthread_mutex_unlock(&mutex_session);
			// Starts timer for ping delivery
			cancel_timer_ping_2 = false;
			pthread_create(&timer_ping_2, NULL, (void*)ping_timeout_2, NULL);
		} else {
			pthread_mutex_lock(&mutex_session);
			printf("\nP%d: Stage 0 has started -> Q4S PING exchange\n", n_process);
			pthread_mutex_unlock(&mutex_session);
			// Starts timer for ping delivery
			cancel_timer_ping_0 = false;
			pthread_create(&timer_ping_0, NULL, (void*)ping_timeout_0, NULL);
		}

		pthread_mutex_lock(&mutex_session);
		// Fills q4s_session.message_to_send with the Q4S 200 OK parameters
		create_200 (&q4s_session.message_to_send, request_method, set_new_parameters);
    if (set_new_parameters) {
			// Stores parameters added to the message
			store_parameters(&q4s_session, &(q4s_session.message_to_send));
			if (initial_qos_level[0] > q4s_session.qos_level[0] || initial_qos_level[1] > q4s_session.qos_level[1]) {
				// Starts timer for recovery pause
				cancel_timer_recover = false;
				pthread_create(&timer_recover, NULL, (void*)recovery_pause_timeout, NULL);
			}
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
	flags &= ~FLAG_TEMP_PING_2;
  pthread_mutex_unlock(&mutex_flags);

	pthread_mutex_lock(&mutex_session);
  if ((&q4s_session)->seq_num_server >= MAXNUMSAMPLES - 1) {
		(&q4s_session)->seq_num_server = 0;
	}
	// Fills q4s_session.message_to_send with the Q4S PING parameters
	create_ping(&q4s_session.message_to_send);
	// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
	prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
	// Sends the message to the Q4S client
	send_message_UDP(q4s_session.prepared_message);
  (&q4s_session)->seq_num_server++;
	num_packet_since_alert++;
	pthread_mutex_unlock(&mutex_session);
}

// Updates Q4S measures and compares them with the thresholds
void Update (fsm_t* fsm) {
	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}
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
				fprintf(stderr,"P%d: Elapsed time stored: %d\n", n_process, elapsed_time);
	      pthread_mutex_unlock(&mutex_print);

	      pthread_mutex_lock(&mutex_session);
				update_jitter(&q4s_session, elapsed_time);
				pthread_mutex_unlock(&mutex_session);

			} else if (num_ping % 2 != 0 && num_ping > 1) {
				elapsed_time = ms_elapsed(tm2_jitter, tm1_jitter);

	      pthread_mutex_lock(&mutex_print);
				fprintf(stderr,"P%d: Elapsed time stored: %d\n", n_process, elapsed_time);
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
				fprintf(stderr,"\nP%d: Loss of %d Q4S PING(s) detected\n", n_process, num_losses);
				pthread_mutex_unlock(&mutex_print);
			}
			update_packetloss(&q4s_session, num_losses);
		}
		pthread_mutex_unlock(&mutex_session);

    pthread_mutex_lock(&mutex_session);
		if ((&q4s_session)->jitter_th[0] > 0 && (&q4s_session)->jitter_measure_server > (&q4s_session)->jitter_th[0]) {
			num_packet_since_alert = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("\nP%d: Upstream jitter exceeds the threshold: %d [%d]", n_process, (&q4s_session)->jitter_th[0], (&q4s_session)->jitter_measure_server);
				pthread_mutex_unlock(&mutex_print);
				memset((&q4s_session)->elapsed_time_samples, 0, MAXNUMSAMPLES);
				memset((&q4s_session)->elapsed_time_window, 0, MAXNUMSAMPLES);
				pos_elapsed_time = 0;
				if ((&q4s_session)->qos_level[0] == 9) {
					pthread_mutex_lock(&mutex_print);
					printf("\nP%d: Reached maximum qos level\n", n_process);
					pthread_mutex_unlock(&mutex_print);
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_RECEIVE_CANCEL;
					pthread_mutex_unlock(&mutex_flags);
				} else {
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_ALERT;
					pthread_mutex_unlock(&mutex_flags);
				}
			}
		} else if ((&q4s_session)->jitter_th[1] > 0 && (&q4s_session)->jitter_measure_client > (&q4s_session)->jitter_th[1]) {
			num_packet_since_alert = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("\nP%d: Downstream jitter exceeds the threshold: %d [%d]", n_process, (&q4s_session)->jitter_th[1], (&q4s_session)->jitter_measure_client);
				pthread_mutex_unlock(&mutex_print);
				if ((&q4s_session)->qos_level[1] == 9) {
					pthread_mutex_lock(&mutex_print);
					printf("\nP%d: Reached maximum qos level\n", n_process);
					pthread_mutex_unlock(&mutex_print);
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_RECEIVE_CANCEL;
					pthread_mutex_unlock(&mutex_flags);
				} else {
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_ALERT;
					pthread_mutex_unlock(&mutex_flags);
				}
			}
		}
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_session);
		if ((&q4s_session)->packetloss_th[0] > 0 && (&q4s_session)->packetloss_measure_server > (&q4s_session)->packetloss_th[0]) {
			num_packet_since_alert = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("\nP%d: Upstream packetloss exceeds the threshold: %.2f [%.2f]", n_process, (&q4s_session)->packetloss_th[0], (&q4s_session)->packetloss_measure_server);
				pthread_mutex_unlock(&mutex_print);
				memset((&q4s_session)->packetloss_samples, 0, MAXNUMSAMPLES);
				memset((&q4s_session)->packetloss_window, 0, MAXNUMSAMPLES);
				pos_packetloss = 0;
				if ((&q4s_session)->qos_level[0] == 9) {
					pthread_mutex_lock(&mutex_print);
					printf("\nP%d: Reached maximum qos level\n", n_process);
					pthread_mutex_unlock(&mutex_print);
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_RECEIVE_CANCEL;
					pthread_mutex_unlock(&mutex_flags);
				} else {
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_ALERT;
					pthread_mutex_unlock(&mutex_flags);
				}
			}
		} else if ((&q4s_session)->packetloss_th[1] > 0 && (&q4s_session)->packetloss_measure_client > (&q4s_session)->packetloss_th[1]) {
			num_packet_since_alert = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("\nP%d: Downstream packetloss exceeds the threshold: %.2f [%.2f]", n_process, (&q4s_session)->packetloss_th[1], (&q4s_session)->packetloss_measure_client);
				pthread_mutex_unlock(&mutex_print);
				if ((&q4s_session)->qos_level[1] == 9) {
					pthread_mutex_lock(&mutex_print);
					printf("\nP%d: Reached maximum qos level\n", n_process);
					pthread_mutex_unlock(&mutex_print);
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_RECEIVE_CANCEL;
					pthread_mutex_unlock(&mutex_flags);
				} else {
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_ALERT;
					pthread_mutex_unlock(&mutex_flags);
				}
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
				fprintf(stderr,"\nP%d: RTT = %d ms, and latency = %d ms\n", n_process, rtt, rtt/2);
				pthread_mutex_unlock(&mutex_print);

				pthread_mutex_lock(&mutex_session);
				update_latency(&q4s_session, rtt/2);
				pthread_mutex_unlock(&mutex_session);

			} else if ((&tm_latency_start2)->seq_number == (&tm_latency_end)->seq_number) {
				int rtt = ms_elapsed((&tm_latency_start2)->tm, (&tm_latency_end)->tm);

				pthread_mutex_lock(&mutex_print);
				fprintf(stderr,"\nP%d: RTT = %d ms, and latency = %d ms\n", n_process, rtt, rtt/2);
				pthread_mutex_unlock(&mutex_print);

				pthread_mutex_lock(&mutex_session);
				update_latency(&q4s_session, rtt/2);
				pthread_mutex_unlock(&mutex_session);

			} else if ((&tm_latency_start3)->seq_number == (&tm_latency_end)->seq_number) {
				int rtt = ms_elapsed((&tm_latency_start3)->tm, (&tm_latency_end)->tm);

				pthread_mutex_lock(&mutex_print);
				fprintf(stderr,"\nP%d: RTT = %d ms, and latency = %d ms\n", n_process, rtt, rtt/2);
				pthread_mutex_unlock(&mutex_print);

				pthread_mutex_lock(&mutex_session);
				update_latency(&q4s_session, rtt/2);
				pthread_mutex_unlock(&mutex_session);

			} else if ((&tm_latency_start4)->seq_number == (&tm_latency_end)->seq_number) {
				int rtt = ms_elapsed((&tm_latency_start4)->tm, (&tm_latency_end)->tm);

				pthread_mutex_lock(&mutex_print);
				fprintf(stderr,"\nP%d: RTT = %d ms, and latency = %d ms\n", n_process, rtt, rtt/2);
				pthread_mutex_unlock(&mutex_print);

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
				printf("\nP%d: Latency measured by server exceeds the threshold: %d [%d]", n_process, (&q4s_session)->latency_th, (&q4s_session)->latency_measure_server);
				pthread_mutex_unlock(&mutex_print);
				memset((&q4s_session)->latency_samples, 0, MAXNUMSAMPLES);
				memset((&q4s_session)->latency_window, 0, MAXNUMSAMPLES);
				pos_latency = 0;
				if ((&q4s_session)->qos_level[0] == 9) {
					pthread_mutex_lock(&mutex_print);
					printf("\nP%d: Reached maximum qos level\n", n_process);
					pthread_mutex_unlock(&mutex_print);
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_RECEIVE_CANCEL;
					pthread_mutex_unlock(&mutex_flags);
				} else {
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_ALERT;
					pthread_mutex_unlock(&mutex_flags);
				}
			}
		} else if ((&q4s_session)->latency_th > 0 && (&q4s_session)->latency_measure_client > (&q4s_session)->latency_th) {
			num_packet_since_alert = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("\nP%d: Latency measured by client exceeds the threshold: %d [%d]", n_process, (&q4s_session)->latency_th, (&q4s_session)->latency_measure_client);
				pthread_mutex_unlock(&mutex_print);
				if ((&q4s_session)->qos_level[1] == 9) {
					pthread_mutex_lock(&mutex_print);
					printf("\nP%d: Reached maximum qos level\n", n_process);
					pthread_mutex_unlock(&mutex_print);
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_RECEIVE_CANCEL;
					pthread_mutex_unlock(&mutex_flags);
				} else {
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_ALERT;
					pthread_mutex_unlock(&mutex_flags);
				}
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
				fprintf(stderr,"\nP%d: Loss of %d Q4S BWIDTH(s) detected\n", n_process, num_losses);
				pthread_mutex_unlock(&mutex_print);
			}
			update_packetloss(&q4s_session, num_losses);
		}
		pthread_mutex_unlock(&mutex_session);
	} else if (flags & FLAG_MEASURE_BWIDTH) {
	  flags &= ~FLAG_MEASURE_BWIDTH;
		pthread_mutex_unlock(&mutex_flags);

		pthread_mutex_lock(&mutex_session);
		if (bwidth_reception_timeout_activated) {
			(&q4s_session)->bw_measure_server = (num_bwidth_received * (MESSAGE_BWIDTH_SIZE * 8) / (&q4s_session)->bwidth_clk);
			int bw_measure_server = (&q4s_session)->bw_measure_server;
			num_bwidth_received = 0;
			bwidth_reception_timeout_activated = false;
			pthread_mutex_lock(&mutex_print);
			fprintf(stderr,"\nP%d: Bandwidth measure: %d\n", n_process, bw_measure_server);
			pthread_mutex_unlock(&mutex_print);
			// Fills q4s_session.message_to_send with the Q4S BWIDTH parameters
			create_bwidth(&q4s_session.message_to_send);
			// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
			prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
			// Sends the prepared message
			send_message_UDP(q4s_session.prepared_message);
			(&q4s_session)->seq_num_server++;
		}
		pthread_mutex_unlock(&mutex_session);

		pthread_mutex_lock(&mutex_session);
		if ((&q4s_session)->bw_th[0] > 0 && (&q4s_session)->bw_measure_server >= 0
		  && (&q4s_session)->bw_measure_server < (&q4s_session)->bw_th[0]) {
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("\nP%d: Upstream bandwidth doesn't reach the threshold: %d [%d]", n_process, (&q4s_session)->bw_th[0], (&q4s_session)->bw_measure_server);
				pthread_mutex_unlock(&mutex_print);
				if ((&q4s_session)->qos_level[0] == 9) {
					pthread_mutex_lock(&mutex_print);
					printf("\nP%d: Reached maximum qos level\n", n_process);
					pthread_mutex_unlock(&mutex_print);
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_RECEIVE_CANCEL;
					pthread_mutex_unlock(&mutex_flags);
				} else {
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_ALERT;
					pthread_mutex_unlock(&mutex_flags);
				}
			}
		} else if ((&q4s_session)->bw_th[1] > 0 && (&q4s_session)->bw_measure_client >= 0
		  && (&q4s_session)->bw_measure_client < (&q4s_session)->bw_th[1]) {
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("\nP%d: Downstream bandwidth doesn't reach the threshold: %d [%d]", n_process, (&q4s_session)->bw_th[1], (&q4s_session)->bw_measure_client);
				pthread_mutex_unlock(&mutex_print);
				if ((&q4s_session)->qos_level[1] == 9) {
					pthread_mutex_lock(&mutex_print);
					printf("\nP%d: Reached maximum qos level\n", n_process);
					pthread_mutex_unlock(&mutex_print);
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_RECEIVE_CANCEL;
					pthread_mutex_unlock(&mutex_flags);
				} else {
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_ALERT;
					pthread_mutex_unlock(&mutex_flags);
				}
			}
		}

		if ((&q4s_session)->packetloss_th[0] > 0 && (&q4s_session)->packetloss_measure_server > (&q4s_session)->packetloss_th[0]) {
			num_packet_since_alert = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("\nP%d: Upstream packetloss exceeds the threshold: %.2f [%.2f]", n_process, (&q4s_session)->packetloss_th[0], (&q4s_session)->packetloss_measure_server);
				pthread_mutex_unlock(&mutex_print);
				memset((&q4s_session)->packetloss_samples, 0, MAXNUMSAMPLES);
				memset((&q4s_session)->packetloss_window, 0, MAXNUMSAMPLES);
				pos_packetloss = 0;
				if ((&q4s_session)->qos_level[0] == 9) {
					pthread_mutex_lock(&mutex_print);
					printf("\nP%d: Reached maximum qos level\n", n_process);
					pthread_mutex_unlock(&mutex_print);
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_RECEIVE_CANCEL;
					pthread_mutex_unlock(&mutex_flags);
				} else {
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_ALERT;
					pthread_mutex_unlock(&mutex_flags);
				}
			}
		} else if ((&q4s_session)->packetloss_th[1] > 0 && (&q4s_session)->packetloss_measure_client > (&q4s_session)->packetloss_th[1]) {
			num_packet_since_alert = 0;
			if ((&q4s_session)->alert_pause_activated == false) {
				pthread_mutex_lock(&mutex_print);
				printf("\nP%d: Downstream packetloss exceeds the threshold: %.2f [%.2f]", n_process, (&q4s_session)->packetloss_th[1], (&q4s_session)->packetloss_measure_client);
				pthread_mutex_unlock(&mutex_print);
				if ((&q4s_session)->qos_level[1] == 9) {
					pthread_mutex_lock(&mutex_print);
					printf("\nP%d: Reached maximum qos level\n", n_process);
					pthread_mutex_unlock(&mutex_print);
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_RECEIVE_CANCEL;
					pthread_mutex_unlock(&mutex_flags);
				} else {
					pthread_mutex_lock(&mutex_flags);
					flags |= FLAG_ALERT;
					pthread_mutex_unlock(&mutex_flags);
				}
			}
		}
		pthread_mutex_unlock(&mutex_session);
	} else {
    pthread_mutex_unlock(&mutex_flags);
	}

	pthread_mutex_lock(&mutex_session);
	int stage = q4s_session.stage;
	pthread_mutex_unlock(&mutex_session);

	if (stage == 0 && !server_ready) {
		pthread_mutex_lock(&mutex_session);
		bool delivery_finished = num_packet_since_alert >= num_samples_succeed;
		pthread_mutex_unlock(&mutex_session);

		if (delivery_finished) {
			cancel_timer_ping_0 = true;

			pthread_mutex_lock(&mutex_session);
			int latency_th = q4s_session.latency_th;
			int latency_measure = max(q4s_session.latency_measure_server, q4s_session.latency_measure_client);
			int jitter_th_up = q4s_session.jitter_th[0];
			int jitter_measure_server = q4s_session.jitter_measure_server;
			int jitter_th_down = q4s_session.jitter_th[1];
			int jitter_measure_client = q4s_session.jitter_measure_client;
			int bw_th_up = q4s_session.bw_th[0];
			int bw_measure_server = q4s_session.bw_measure_server;
			int bw_th_down = q4s_session.bw_th[1];
			int bw_measure_client = q4s_session.bw_measure_client;
			float packetloss_th_up = q4s_session.packetloss_th[0];
			float packetloss_measure_server = q4s_session.packetloss_measure_server;
			float packetloss_th_down = q4s_session.packetloss_th[1];
			float packetloss_measure_client = q4s_session.packetloss_measure_client;
			pthread_mutex_unlock(&mutex_session);

			char message[5000];
			memset(message, '\0', sizeof(message));

			char s_value[10];
			memset(s_value, '\0', strlen(s_value));
			strcpy(message, "Application flow thresholds [Measured values]: \n");

			strcat(message, "Latency: ");
			sprintf(s_value, "%d", latency_th);
			strcat(message, s_value);
			strcat(message, " [");
			memset(s_value, '\0', strlen(s_value));
			if (latency_measure <= 0) {
				strcpy(s_value, " ");
			} else {
				sprintf(s_value, "%d", latency_measure);
			}
			strcat(message, s_value);
			strcat(message, "]\n");
			memset(s_value, '\0', strlen(s_value));

			strcat(message, "Upstream jitter: ");
			sprintf(s_value, "%d", jitter_th_up);
			strcat(message, s_value);
			strcat(message, " [");
			memset(s_value, '\0', strlen(s_value));
			if (jitter_measure_server < 0) {
				strcpy(s_value, " ");
			} else {
				sprintf(s_value, "%d", jitter_measure_server);
			}
			strcat(message, s_value);
			strcat(message, "]\n");
			memset(s_value, '\0', strlen(s_value));

			strcat(message, "Downstream jitter: ");
			sprintf(s_value, "%d", jitter_th_down);
			strcat(message, s_value);
			strcat(message, " [");
			memset(s_value, '\0', strlen(s_value));
			if (jitter_measure_client < 0) {
				strcpy(s_value, " ");
			} else {
				sprintf(s_value, "%d", jitter_measure_client);
			}
			strcat(message, s_value);
			strcat(message, "]\n");
			memset(s_value, '\0', strlen(s_value));

			strcat(message, "Upstream bandwidth: ");
			sprintf(s_value, "%d", bw_th_up);
			strcat(message, s_value);
			strcat(message, " [");
			memset(s_value, '\0', strlen(s_value));
			if (bw_measure_server < 0) {
				strcpy(s_value, " ");
			} else {
				sprintf(s_value, "%d", bw_measure_server);
			}
			strcat(message, s_value);
			strcat(message, "]\n");
			memset(s_value, '\0', strlen(s_value));

			strcat(message, "Downstream bandwidth: ");
			sprintf(s_value, "%d", bw_th_down);
			strcat(message, s_value);
			strcat(message, " [");
			memset(s_value, '\0', strlen(s_value));
			if (bw_measure_client < 0) {
				strcpy(s_value, " ");
			} else {
				sprintf(s_value, "%d", bw_measure_client);
			}
			strcat(message, s_value);
			strcat(message, "]\n");
			memset(s_value, '\0', strlen(s_value));

			strcat(message, "Upstream packetloss: ");
			sprintf(s_value, "%.2f", packetloss_th_up);
			strcat(message, s_value);
			strcat(message, " [");
			memset(s_value, '\0', strlen(s_value));
			if (packetloss_measure_server < 0) {
				strcpy(s_value, " ");
			} else {
				sprintf(s_value, "%.2f", packetloss_measure_server);
			}
			strcat(message, s_value);
			strcat(message, "]\n");
			memset(s_value, '\0', strlen(s_value));

			strcat(message, "Downstream packetloss: ");
			sprintf(s_value, "%.2f", packetloss_th_down);
			strcat(message, s_value);
			strcat(message, " [");
			memset(s_value, '\0', strlen(s_value));
			if (packetloss_measure_client < 0) {
				strcpy(s_value, " ");
			} else {
				sprintf(s_value, "%.2f", packetloss_measure_client);
			}
			strcat(message, s_value);
			strcat(message, "]\n");
			memset(s_value, '\0', strlen(s_value));

			pthread_mutex_lock(&mutex_print);
			printf("\nP%d:%s\n", n_process, message);
			printf("\nP%d: Finished ping delivery from server", n_process);
			printf("\nP%d: Waiting client to finish and initiate next Stage\n", n_process);
			pthread_mutex_unlock(&mutex_print);
			server_ready = true;
		}
	}
}

// Triggers an alert to the Actuator
void Alert (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_ALERT;
  pthread_mutex_unlock(&mutex_flags);

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

	pthread_mutex_lock(&mutex_session);
	int latency_th = q4s_session.latency_th;
	int latency_measure = max(q4s_session.latency_measure_server, q4s_session.latency_measure_client);
	int jitter_th_up = q4s_session.jitter_th[0];
	int jitter_measure_server = q4s_session.jitter_measure_server;
	int jitter_th_down = q4s_session.jitter_th[1];
	int jitter_measure_client = q4s_session.jitter_measure_client;
	int bw_th_up = q4s_session.bw_th[0];
	int bw_measure_server = q4s_session.bw_measure_server;
	int bw_th_down = q4s_session.bw_th[1];
	int bw_measure_client = q4s_session.bw_measure_client;
	float packetloss_th_up = q4s_session.packetloss_th[0];
	float packetloss_measure_server = q4s_session.packetloss_measure_server;
	float packetloss_th_down = q4s_session.packetloss_th[1];
	float packetloss_measure_client = q4s_session.packetloss_measure_client;
	pthread_mutex_unlock(&mutex_session);

  // Creates an alert notification
	char alert_message[5000];
	memset(alert_message, '\0', sizeof(alert_message));
	strcpy(alert_message, "ALERT NOTIFICATION\n\n");
	strcat(alert_message, "Client IP address: ");
	strcat(alert_message, inet_ntoa(client_TCP.sin_addr));
	strcat(alert_message, "\n");
	strcat(alert_message, "Server IP address: ");
	strcat(alert_message, inet_ntoa(server_TCP.sin_addr));
	strcat(alert_message, "\n");
	char s_port[10];
	strcat(alert_message, "Client Listening Port UDP/");
	memset(s_port, '\0', strlen(s_port));
	sprintf(s_port, "%d", CLIENT_PORT_UDP);
	strcat(alert_message, s_port);
	strcat(alert_message, "\n");
	memset(s_port, '\0', strlen(s_port));
	strcat(alert_message, "Client Listening Port TCP/");
	sprintf(s_port, "%d", CLIENT_PORT_TCP);
	strcat(alert_message, s_port);
	strcat(alert_message, "\n");
	memset(s_port, '\0', strlen(s_port));
	strcat(alert_message, "Server Listening Port UDP/");
	sprintf(s_port, "%d", HOST_PORT_UDP);
	strcat(alert_message, s_port);
	strcat(alert_message, "\n");
	memset(s_port, '\0', strlen(s_port));
	strcat(alert_message, "Server Listening Port TCP/");
	sprintf(s_port, "%d", HOST_PORT_TCP);
	strcat(alert_message, s_port);
	strcat(alert_message, "\n\n");
	memset(s_port, '\0', strlen(s_port));

  char s_value[10];
	memset(s_value, '\0', strlen(s_value));
  strcat(alert_message, "Application flow thresholds [Measured values]: \n");

	strcat(alert_message, "Latency: ");
	sprintf(s_value, "%d", latency_th);
	strcat(alert_message, s_value);
	strcat(alert_message, " [");
	memset(s_value, '\0', strlen(s_value));
	if (latency_measure <= 0) {
		strcpy(s_value, " ");
	} else {
		sprintf(s_value, "%d", latency_measure);
	}
	strcat(alert_message, s_value);
	strcat(alert_message, "]\n");
	memset(s_value, '\0', strlen(s_value));

	strcat(alert_message, "Upstream jitter: ");
	sprintf(s_value, "%d", jitter_th_up);
	strcat(alert_message, s_value);
	strcat(alert_message, " [");
	memset(s_value, '\0', strlen(s_value));
	if (jitter_measure_server < 0) {
		strcpy(s_value, " ");
	} else {
		sprintf(s_value, "%d", jitter_measure_server);
	}
	strcat(alert_message, s_value);
	strcat(alert_message, "]\n");
	memset(s_value, '\0', strlen(s_value));

	strcat(alert_message, "Downstream jitter: ");
	sprintf(s_value, "%d", jitter_th_down);
	strcat(alert_message, s_value);
	strcat(alert_message, " [");
	memset(s_value, '\0', strlen(s_value));
	if (jitter_measure_client < 0) {
		strcpy(s_value, " ");
	} else {
		sprintf(s_value, "%d", jitter_measure_client);
	}
	strcat(alert_message, s_value);
	strcat(alert_message, "]\n");
	memset(s_value, '\0', strlen(s_value));

	strcat(alert_message, "Upstream bandwidth: ");
	sprintf(s_value, "%d", bw_th_up);
	strcat(alert_message, s_value);
	strcat(alert_message, " [");
	memset(s_value, '\0', strlen(s_value));
	if (bw_measure_server < 0) {
		strcpy(s_value, " ");
	} else {
		sprintf(s_value, "%d", bw_measure_server);
	}
	strcat(alert_message, s_value);
	strcat(alert_message, "]\n");
	memset(s_value, '\0', strlen(s_value));

	strcat(alert_message, "Downstream bandwidth: ");
	sprintf(s_value, "%d", bw_th_down);
	strcat(alert_message, s_value);
	strcat(alert_message, " [");
	memset(s_value, '\0', strlen(s_value));
	if (bw_measure_client < 0) {
		strcpy(s_value, " ");
	} else {
		sprintf(s_value, "%d", bw_measure_client);
	}
	strcat(alert_message, s_value);
	strcat(alert_message, "]\n");
	memset(s_value, '\0', strlen(s_value));

	strcat(alert_message, "Upstream packetloss: ");
	sprintf(s_value, "%.2f", packetloss_th_up);
	strcat(alert_message, s_value);
	strcat(alert_message, " [");
	memset(s_value, '\0', strlen(s_value));
	if (packetloss_measure_server < 0) {
		strcpy(s_value, " ");
	} else {
		sprintf(s_value, "%.2f", packetloss_measure_server);
	}
	strcat(alert_message, s_value);
	strcat(alert_message, "]\n");
	memset(s_value, '\0', strlen(s_value));

	strcat(alert_message, "Downstream packetloss: ");
	sprintf(s_value, "%.2f", packetloss_th_down);
	strcat(alert_message, s_value);
	strcat(alert_message, " [");
	memset(s_value, '\0', strlen(s_value));
	if (packetloss_measure_client < 0) {
		strcpy(s_value, " ");
	} else {
		sprintf(s_value, "%.2f", packetloss_measure_client);
	}
	strcat(alert_message, s_value);
	strcat(alert_message, "]\n");
	memset(s_value, '\0', strlen(s_value));

	pthread_mutex_lock(&mutex_print);
	printf("\nP%d: An alert notification has been triggered\n", n_process);
	fprintf(stderr, "\n%s\n", alert_message);
	pthread_mutex_unlock(&mutex_print);


	// Starts timer for alert pause
	cancel_timer_alert = false;
	pthread_create(&timer_alert, NULL, (void*)alert_pause_timeout, NULL);
}

// Triggers a recovery message to the Actuator
void Recover (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_RECOVER;
  pthread_mutex_unlock(&mutex_flags);

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

	pthread_mutex_lock(&mutex_session);
	int latency_th = q4s_session.latency_th;
	int latency_measure = max(q4s_session.latency_measure_server, q4s_session.latency_measure_client);
	int jitter_th_up = q4s_session.jitter_th[0];
	int jitter_measure_server = q4s_session.jitter_measure_server;
	int jitter_th_down = q4s_session.jitter_th[1];
	int jitter_measure_client = q4s_session.jitter_measure_client;
	int bw_th_up = q4s_session.bw_th[0];
	int bw_measure_server = q4s_session.bw_measure_server;
	int bw_th_down = q4s_session.bw_th[1];
	int bw_measure_client = q4s_session.bw_measure_client;
	float packetloss_th_up = q4s_session.packetloss_th[0];
	float packetloss_measure_server = q4s_session.packetloss_measure_server;
	float packetloss_th_down = q4s_session.packetloss_th[1];
	float packetloss_measure_client = q4s_session.packetloss_measure_client;
	pthread_mutex_unlock(&mutex_session);

	// Creates a recovery notification
	char recovery_message[5000];
	memset(recovery_message, '\0', sizeof(recovery_message));
	strcpy(recovery_message, "RECOVERY NOTIFICATION\n\n");
	strcat(recovery_message, "Client IP address: ");
	strcat(recovery_message, inet_ntoa(client_TCP.sin_addr));
	strcat(recovery_message, "\n");
	strcat(recovery_message, "Server IP address: ");
	strcat(recovery_message, inet_ntoa(server_TCP.sin_addr));
	strcat(recovery_message, "\n");
	char s_port[10];
	strcat(recovery_message, "Client Listening Port UDP/");
	memset(s_port, '\0', strlen(s_port));
	sprintf(s_port, "%d", CLIENT_PORT_UDP);
	strcat(recovery_message, s_port);
	strcat(recovery_message, "\n");
	memset(s_port, '\0', strlen(s_port));
	strcat(recovery_message, "Client Listening Port TCP/");
	sprintf(s_port, "%d", CLIENT_PORT_TCP);
	strcat(recovery_message, s_port);
	strcat(recovery_message, "\n");
	memset(s_port, '\0', strlen(s_port));
	strcat(recovery_message, "Server Listening Port UDP/");
	sprintf(s_port, "%d", HOST_PORT_UDP);
	strcat(recovery_message, s_port);
	strcat(recovery_message, "\n");
	memset(s_port, '\0', strlen(s_port));
	strcat(recovery_message, "Server Listening Port TCP/");
	sprintf(s_port, "%d", HOST_PORT_TCP);
	strcat(recovery_message, s_port);
	strcat(recovery_message, "\n\n");
	memset(s_port, '\0', strlen(s_port));

	char s_value[10];
	memset(s_value, '\0', strlen(s_value));
	strcat(recovery_message, "Application flow thresholds [Measured values]: \n");

	strcat(recovery_message, "Latency: ");
	sprintf(s_value, "%d", latency_th);
	strcat(recovery_message, s_value);
	strcat(recovery_message, " [");
	memset(s_value, '\0', strlen(s_value));
	if (latency_measure <= 0) {
		strcpy(s_value, " ");
	} else {
		sprintf(s_value, "%d", latency_measure);
	}
	strcat(recovery_message, s_value);
	strcat(recovery_message, "]\n");
	memset(s_value, '\0', strlen(s_value));

	strcat(recovery_message, "Upstream jitter: ");
	sprintf(s_value, "%d", jitter_th_up);
	strcat(recovery_message, s_value);
	strcat(recovery_message, " [");
	memset(s_value, '\0', strlen(s_value));
	if (jitter_measure_server < 0) {
		strcpy(s_value, " ");
	} else {
		sprintf(s_value, "%d", jitter_measure_server);
	}
	strcat(recovery_message, s_value);
	strcat(recovery_message, "]\n");
	memset(s_value, '\0', strlen(s_value));

	strcat(recovery_message, "Downstream jitter: ");
	sprintf(s_value, "%d", jitter_th_down);
	strcat(recovery_message, s_value);
	strcat(recovery_message, " [");
	memset(s_value, '\0', strlen(s_value));
	if (jitter_measure_client < 0) {
		strcpy(s_value, " ");
	} else {
		sprintf(s_value, "%d", jitter_measure_client);
	}
	strcat(recovery_message, s_value);
	strcat(recovery_message, "]\n");
	memset(s_value, '\0', strlen(s_value));

	strcat(recovery_message, "Upstream bandwidth: ");
	sprintf(s_value, "%d", bw_th_up);
	strcat(recovery_message, s_value);
	strcat(recovery_message, " [");
	memset(s_value, '\0', strlen(s_value));
	if (bw_measure_server < 0) {
		strcpy(s_value, " ");
	} else {
		sprintf(s_value, "%d", bw_measure_server);
	}
	strcat(recovery_message, s_value);
	strcat(recovery_message, "]\n");
	memset(s_value, '\0', strlen(s_value));

	strcat(recovery_message, "Downstream bandwidth: ");
	sprintf(s_value, "%d", bw_th_down);
	strcat(recovery_message, s_value);
	strcat(recovery_message, " [");
	memset(s_value, '\0', strlen(s_value));
	if (bw_measure_client < 0) {
		strcpy(s_value, " ");
	} else {
		sprintf(s_value, "%d", bw_measure_client);
	}
	strcat(recovery_message, s_value);
	strcat(recovery_message, "]\n");
	memset(s_value, '\0', strlen(s_value));

	strcat(recovery_message, "Upstream packetloss: ");
	sprintf(s_value, "%.2f", packetloss_th_up);
	strcat(recovery_message, s_value);
	strcat(recovery_message, " [");
	memset(s_value, '\0', strlen(s_value));
	if (packetloss_measure_server < 0) {
		strcpy(s_value, " ");
	} else {
		sprintf(s_value, "%.2f", packetloss_measure_server);
	}
	strcat(recovery_message, s_value);
	strcat(recovery_message, "]\n");
	memset(s_value, '\0', strlen(s_value));

	strcat(recovery_message, "Downstream packetloss: ");
	sprintf(s_value, "%.2f", packetloss_th_down);
	strcat(recovery_message, s_value);
	strcat(recovery_message, " [");
	memset(s_value, '\0', strlen(s_value));
	if (packetloss_measure_client < 0) {
		strcpy(s_value, " ");
	} else {
		sprintf(s_value, "%.2f", packetloss_measure_client);
	}
	strcat(recovery_message, s_value);
	strcat(recovery_message, "]\n");
	memset(s_value, '\0', strlen(s_value));

	pthread_mutex_lock(&mutex_print);
	printf("\nP%d: A recovery notification has been triggered\n", n_process);
	fprintf(stderr, "\n%s\n", recovery_message);
	pthread_mutex_unlock(&mutex_print);

}

// Starts delivery of Q4S BWIDTH messages
void Bwidth_Init (fsm_t* fsm) {
	pthread_mutex_lock(&mutex_flags);
	flags &= ~FLAG_INIT_BWIDTH;
	pthread_mutex_unlock(&mutex_flags);

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

	pthread_mutex_lock(&mutex_session);
	// Initialize ping period if necessary
	if (q4s_session.bwidth_clk <= 0) {
		q4s_session.bwidth_clk = 1000;
	}
	pthread_mutex_unlock(&mutex_session);

	pthread_mutex_lock(&mutex_session);
	printf("\nP%d: Stage 1 has started -> Q4S BWIDTH exchange\n", n_process);
	pthread_mutex_unlock(&mutex_session);

	// Starts timer for bwidth delivery
	cancel_timer_delivery_bwidth = false;
	pthread_create(&timer_delivery_bwidth, NULL, (void*)bwidth_delivery, NULL);
}

// Decides whether start a new Q4S BWIDTH burst or not
void Bwidth_Decide (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_BWIDTH_BURST_SENT;
  pthread_mutex_unlock(&mutex_flags);

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

	pthread_mutex_lock(&mutex_session);
	int stage = q4s_session.stage;
	bool delivery_finished = (((&q4s_session)->bw_th[1] <= 0 ||
		(&q4s_session)->bw_measure_client >= (&q4s_session)->bw_th[1])
		&& ((&q4s_session)->packetloss_th[1] <= 0 ||
		(&q4s_session)->packetloss_measure_client <= (&q4s_session)->packetloss_th[1]));
	pthread_mutex_unlock(&mutex_session);

	if (stage == 1 && delivery_finished) {
		cancel_timer_delivery_bwidth = true;

		pthread_mutex_lock(&mutex_session);
		int latency_th = q4s_session.latency_th;
		int latency_measure = max(q4s_session.latency_measure_server, q4s_session.latency_measure_client);
		int jitter_th_up = q4s_session.jitter_th[0];
		int jitter_measure_server = q4s_session.jitter_measure_server;
		int jitter_th_down = q4s_session.jitter_th[1];
		int jitter_measure_client = q4s_session.jitter_measure_client;
		int bw_th_up = q4s_session.bw_th[0];
		int bw_measure_server = q4s_session.bw_measure_server;
		int bw_th_down = q4s_session.bw_th[1];
		int bw_measure_client = q4s_session.bw_measure_client;
		float packetloss_th_up = q4s_session.packetloss_th[0];
		float packetloss_measure_server = q4s_session.packetloss_measure_server;
		float packetloss_th_down = q4s_session.packetloss_th[1];
		float packetloss_measure_client = q4s_session.packetloss_measure_client;
		pthread_mutex_unlock(&mutex_session);

		char message[5000];
		memset(message, '\0', sizeof(message));

		char s_value[10];
		memset(s_value, '\0', strlen(s_value));
		strcpy(message, "Application flow thresholds [Measured values]: \n");

		strcat(message, "Latency: ");
		sprintf(s_value, "%d", latency_th);
		strcat(message, s_value);
		strcat(message, " [");
		memset(s_value, '\0', strlen(s_value));
		if (latency_measure <= 0) {
			strcpy(s_value, " ");
		} else {
			sprintf(s_value, "%d", latency_measure);
		}
		strcat(message, s_value);
		strcat(message, "]\n");
		memset(s_value, '\0', strlen(s_value));

		strcat(message, "Upstream jitter: ");
		sprintf(s_value, "%d", jitter_th_up);
		strcat(message, s_value);
		strcat(message, " [");
		memset(s_value, '\0', strlen(s_value));
		if (jitter_measure_server < 0) {
			strcpy(s_value, " ");
		} else {
			sprintf(s_value, "%d", jitter_measure_server);
		}
		strcat(message, s_value);
		strcat(message, "]\n");
		memset(s_value, '\0', strlen(s_value));

		strcat(message, "Downstream jitter: ");
		sprintf(s_value, "%d", jitter_th_down);
		strcat(message, s_value);
		strcat(message, " [");
		memset(s_value, '\0', strlen(s_value));
		if (jitter_measure_client < 0) {
			strcpy(s_value, " ");
		} else {
			sprintf(s_value, "%d", jitter_measure_client);
		}
		strcat(message, s_value);
		strcat(message, "]\n");
		memset(s_value, '\0', strlen(s_value));

		strcat(message, "Upstream bandwidth: ");
		sprintf(s_value, "%d", bw_th_up);
		strcat(message, s_value);
		strcat(message, " [");
		memset(s_value, '\0', strlen(s_value));
		if (bw_measure_server < 0) {
			strcpy(s_value, " ");
		} else {
			sprintf(s_value, "%d", bw_measure_server);
		}
		strcat(message, s_value);
		strcat(message, "]\n");
		memset(s_value, '\0', strlen(s_value));

		strcat(message, "Downstream bandwidth: ");
		sprintf(s_value, "%d", bw_th_down);
		strcat(message, s_value);
		strcat(message, " [");
		memset(s_value, '\0', strlen(s_value));
		if (bw_measure_client < 0) {
			strcpy(s_value, " ");
		} else {
			sprintf(s_value, "%d", bw_measure_client);
		}
		strcat(message, s_value);
		strcat(message, "]\n");
		memset(s_value, '\0', strlen(s_value));

		strcat(message, "Upstream packetloss: ");
		sprintf(s_value, "%.2f", packetloss_th_up);
		strcat(message, s_value);
		strcat(message, " [");
		memset(s_value, '\0', strlen(s_value));
		if (packetloss_measure_server < 0) {
			strcpy(s_value, " ");
		} else {
			sprintf(s_value, "%.2f", packetloss_measure_server);
		}
		strcat(message, s_value);
		strcat(message, "]\n");
		memset(s_value, '\0', strlen(s_value));

		strcat(message, "Downstream packetloss: ");
		sprintf(s_value, "%.2f", packetloss_th_down);
		strcat(message, s_value);
		strcat(message, " [");
		memset(s_value, '\0', strlen(s_value));
		if (packetloss_measure_client < 0) {
			strcpy(s_value, " ");
		} else {
			sprintf(s_value, "%.2f", packetloss_measure_client);
		}
		strcat(message, s_value);
		strcat(message, "]\n");
		memset(s_value, '\0', strlen(s_value));

		pthread_mutex_lock(&mutex_print);
		printf("\nP%d: %s\n", n_process, message);
		printf("\nP%d: Finished bwidth delivery from server\n", n_process);
		printf("\nP%d: Waiting client to finish and initiate next Stage\n", n_process);
		pthread_mutex_unlock(&mutex_print);
		server_ready = true;
	} else {
		if (!bwidth_reception_timeout_activated) {
			pthread_mutex_lock(&mutex_flags);
			flags |= FLAG_MEASURE_BWIDTH;
			pthread_mutex_unlock(&mutex_flags);
		}
		// Starts timer for bwidth delivery
		cancel_timer_delivery_bwidth = false;
		pthread_create(&timer_delivery_bwidth, NULL, (void*)bwidth_delivery, NULL);
	}
}

// Tells Actuator to release the resources
void Release (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
  pthread_mutex_lock(&mutex_flags);
  flags &= ~FLAG_RECEIVE_CANCEL;
	pthread_mutex_unlock(&mutex_flags);

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

	// Cancels timers
	cancel_timer_ping_0 = true;
	cancel_timer_ping_2 = true;
	cancel_timer_delivery_bwidth = true;
	cancel_timer_reception_bwidth = true;

	// Creates a cancel notification
	char cancel_message[5000];
	memset(cancel_message, '\0', sizeof(cancel_message));
	strcpy(cancel_message, "CANCEL NOTIFICATION\n\n");
	strcat(cancel_message, "Client IP address: ");
	strcat(cancel_message, inet_ntoa(client_TCP.sin_addr));
	strcat(cancel_message, "\n");
	strcat(cancel_message, "Server IP address: ");
	strcat(cancel_message, inet_ntoa(server_TCP.sin_addr));
	strcat(cancel_message, "\n");
	char s_port[10];
	strcat(cancel_message, "Client Listening Port UDP/");
	memset(s_port, '\0', strlen(s_port));
	sprintf(s_port, "%d", CLIENT_PORT_UDP);
	strcat(cancel_message, s_port);
	strcat(cancel_message, "\n");
	memset(s_port, '\0', strlen(s_port));
	strcat(cancel_message, "Client Listening Port TCP/");
	sprintf(s_port, "%d", CLIENT_PORT_TCP);
	strcat(cancel_message, s_port);
	strcat(cancel_message, "\n");
	memset(s_port, '\0', strlen(s_port));
	strcat(cancel_message, "Server Listening Port UDP/");
	sprintf(s_port, "%d", HOST_PORT_UDP);
	strcat(cancel_message, s_port);
	strcat(cancel_message, "\n");
	memset(s_port, '\0', strlen(s_port));
	strcat(cancel_message, "Server Listening Port TCP/");
	sprintf(s_port, "%d", HOST_PORT_TCP);
	strcat(cancel_message, s_port);
	strcat(cancel_message, "\n\n");
	memset(s_port, '\0', strlen(s_port));

  // Send a cancel notification to Actuator
	pthread_mutex_lock(&mutex_print);
	printf("\nP%d: A cancel notificaction has been triggered\n", n_process);
	fprintf(stderr, "\n%s\n", cancel_message);
  pthread_mutex_unlock(&mutex_print);

	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
	flags |= FLAG_RELEASED;
	pthread_mutex_unlock(&mutex_flags);
}

// Sends a Q4S CANCEL message to the client, and exits connection with that client
void Cancel (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex_flags);
  // Puts every flag to 0
  flags = 0;
	pthread_mutex_unlock(&mutex_flags);

	int n_process;
	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		n_process = 1;
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		n_process = 2;
	} else if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
		n_process = 3;
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		n_process = 4;
	}

  pthread_mutex_lock(&mutex_session);
	// Fills q4s_session.message_to_send with the Q4S CANCEL parameters
	create_cancel(&q4s_session.message_to_send);
	// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
	prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
	// Sends the message to the Q4S client
	send_message_TCP(q4s_session.prepared_message);
	pthread_mutex_unlock(&mutex_session);

	pthread_mutex_lock(&mutex_print);
	printf("\nP%d: I have sent a Q4S CANCEL!\n", n_process);
	pthread_mutex_unlock(&mutex_print);

	// Cancels timers
	cancel_timer_ping_0 = true;
	cancel_timer_ping_2 = true;
	cancel_timer_delivery_bwidth = true;
	cancel_timer_reception_bwidth = true;
	cancel_timer_expire = true;
	// Cancels the threads receiving Q4S messages
	cancel_UDP_thread = true;
	pthread_mutex_lock(&mutex_session);
	pthread_mutex_lock(&mutex_print);
	pthread_mutex_lock(&mutex_buffer_UDP);
	pthread_cancel(receive_UDP_thread);
	pthread_mutex_unlock(&mutex_buffer_UDP);
	pthread_mutex_unlock(&mutex_print);
	pthread_mutex_unlock(&mutex_session);

	cancel_redirected_thread = true;
	pthread_mutex_lock(&mutex_session);
	pthread_mutex_lock(&mutex_print);
	pthread_mutex_lock(&mutex_buffer_UDP);
	pthread_cancel(receive_redirected_thread);
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

	delay(1000);

	// Closes connection with Q4S client
  close(client_socket_TCP);
	pthread_mutex_lock(&mutex_print);
  printf("\nP%d: Closing connection\n", n_process);
	pthread_mutex_unlock(&mutex_print);

	delay(1000);

	if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		pthread_mutex_lock(mutex_process);
		*state_proc1 = 1;
		*s_id_proc1 = -1;
		pthread_mutex_lock(&mutex_print);
		fprintf(stderr, "pid2: %d, pid3: %d, pid4: %d\n", *p_id_proc2, *p_id_proc3, *p_id_proc4);
		pthread_mutex_unlock(&mutex_print);
		if (*state_proc2 == 1) {
			*state_proc2 = 0;
			pthread_mutex_lock(&mutex_print);
			fprintf(stderr, "Killing process 2\n");
			pthread_mutex_unlock(&mutex_print);
			kill(*p_id_proc2, SIGKILL);
		}
		if (*state_proc3 == 1) {
			*state_proc3 = 0;
			pthread_mutex_lock(&mutex_print);
			fprintf(stderr, "Killing process 3\n");
			pthread_mutex_unlock(&mutex_print);
			kill(*p_id_proc3, SIGKILL);
		}
		if (*state_proc4 == 1) {
			*state_proc4 = 0;
			pthread_mutex_lock(&mutex_print);
			fprintf(stderr, "Killing process 4\n");
			pthread_mutex_unlock(&mutex_print);
			kill(*p_id_proc4, SIGKILL);
		}
		pthread_mutex_unlock(mutex_process);

		pthread_mutex_lock(&mutex_print);
		printf("P1: Waiting for a connection with a new client\n");
	  pthread_mutex_unlock(&mutex_print);
	} else if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
		pthread_mutex_lock(mutex_process);
		*s_id_proc2 = -1;
		if (*state_proc1 != 1 && *state_proc3 != 1 && *state_proc4 != 1) {
			*state_proc2 = 1;
			pthread_mutex_unlock(mutex_process);

			pthread_mutex_lock(&mutex_print);
			printf("P2: Waiting for a connection with a new client\n");
		  pthread_mutex_unlock(&mutex_print);
		} else {
			*state_proc2 = 0;
			pthread_mutex_unlock(mutex_process);
			// State machine destruction
			fsm_destroy (q4s_fsm);
			pthread_mutex_lock(&mutex_print);
			printf("P2: Exiting process\n");
		  pthread_mutex_unlock(&mutex_print);
			exit(EXIT_SUCCESS);
		}
	} else if (pid2 != 0 && pid3 == 0 && pid4 != 0) {
		pthread_mutex_lock(mutex_process);
		*s_id_proc3 = -1;
		if (*state_proc1 != 1 && *state_proc2 != 1 && *state_proc4 != 1) {
			*state_proc3 = 1;
			pthread_mutex_unlock(mutex_process);

			pthread_mutex_lock(&mutex_print);
			printf("P3: Waiting for a connection with a new client\n");
		  pthread_mutex_unlock(&mutex_print);
		} else {
			*state_proc3 = 0;
			pthread_mutex_unlock(mutex_process);
			// State machine destruction
			fsm_destroy (q4s_fsm);
			pthread_mutex_lock(&mutex_print);
			printf("P3: Exiting process\n");
		  pthread_mutex_unlock(&mutex_print);
			exit(EXIT_SUCCESS);
		}
	} else if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
		pthread_mutex_lock(mutex_process);
		*s_id_proc4 = -1;
		if (*state_proc1 != 1 && *state_proc2 != 1 && *state_proc3 != 1) {
			*state_proc4 = 1;
			pthread_mutex_unlock(mutex_process);

			pthread_mutex_lock(&mutex_print);
			printf("P4: Waiting for a connection with a new client\n");
		  pthread_mutex_unlock(&mutex_print);
		} else {
			*state_proc4 = 0;
			pthread_mutex_unlock(mutex_process);

			// State machine destruction
			fsm_destroy (q4s_fsm);
			pthread_mutex_lock(&mutex_print);
			printf("P4: Exiting process\n");
		  pthread_mutex_unlock(&mutex_print);
			exit(EXIT_SUCCESS);
		}
	}
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
			{ WAIT_CONNECTION, check_new_connection, WAIT_START, Setup },
			{ WAIT_START, check_failure,  WAIT_START, Send_Failure },
			{ WAIT_START, check_receive_begin,  HANDSHAKE, Respond_OK },
			{ HANDSHAKE, check_failure,  HANDSHAKE, Send_Failure },
			{ HANDSHAKE, check_receive_ready0,  STAGE_0, Respond_OK },
			{ HANDSHAKE, check_receive_ready1,  STAGE_1, Respond_OK },
			{ HANDSHAKE, check_receive_cancel, TERMINATION, Release },
			{ STAGE_0, check_failure, STAGE_0, Send_Failure },
			{ STAGE_0, check_receive_cancel, TERMINATION, Release },
			{ STAGE_0, check_receive_ping, PING_MEASURE_0, Respond_OK },
			{ PING_MEASURE_0, check_failure,  PING_MEASURE_0, Send_Failure },
			{ PING_MEASURE_0, check_temp_ping_0, PING_MEASURE_0, Ping },
			{ PING_MEASURE_0, check_receive_ok, PING_MEASURE_0, Update },
			{ PING_MEASURE_0, check_receive_ping, PING_MEASURE_0, Update },
			{ PING_MEASURE_0, check_alert, PING_MEASURE_0, Alert },
			{ PING_MEASURE_0, check_receive_ready1, STAGE_1, Respond_OK },
			{ PING_MEASURE_0, check_receive_ready2, STAGE_2, Respond_OK },
			{ PING_MEASURE_0, check_receive_cancel, TERMINATION, Release },
			{ STAGE_1, check_failure, STAGE_1, Send_Failure },
			{ STAGE_1, check_receive_cancel, TERMINATION, Release },
			{ STAGE_1, check_init_bwidth, BWIDTH_MEASURE, Bwidth_Init },
			{ BWIDTH_MEASURE, check_failure, BWIDTH_MEASURE, Send_Failure },
			{ BWIDTH_MEASURE, check_bwidth_burst_sent, BWIDTH_MEASURE, Bwidth_Decide },
			{ BWIDTH_MEASURE, check_measure_bwidth, BWIDTH_MEASURE, Update },
			{ BWIDTH_MEASURE, check_alert, BWIDTH_MEASURE, Alert },
			{ BWIDTH_MEASURE, check_receive_ready2, STAGE_2, Respond_OK },
			{ BWIDTH_MEASURE, check_receive_cancel, TERMINATION, Release },
			{ STAGE_2, check_failure, STAGE_2, Send_Failure },
			{ STAGE_2, check_receive_cancel, TERMINATION, Release },
			{ STAGE_2, check_receive_ping, PING_MEASURE_2, Respond_OK },
			{ PING_MEASURE_2, check_failure,  PING_MEASURE_2, Send_Failure },
			{ PING_MEASURE_2, check_temp_ping_2, PING_MEASURE_2, Ping },
			{ PING_MEASURE_2, check_receive_ok, PING_MEASURE_2, Update },
			{ PING_MEASURE_2, check_receive_ping, PING_MEASURE_2, Update },
			{ PING_MEASURE_2, check_alert, PING_MEASURE_2, Alert },
			{ PING_MEASURE_2, check_recover, PING_MEASURE_2, Recover },
			{ PING_MEASURE_2, check_receive_cancel, TERMINATION, Release },
			{ TERMINATION, check_released, WAIT_CONNECTION,  Cancel },
			{ -1, NULL, -1, NULL }
	};

	// Server starts listening
	start_listening_UDP();
	start_listening_TCP();

  if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
		pthread_mutex_lock(mutex_process);
    *state_proc1 = 1;
		pthread_mutex_unlock(mutex_process);

		// State machine creation
		q4s_fsm = fsm_new (WAIT_CONNECTION, q4s_table, NULL);

		// State machine initialitation
		fsm_setup (q4s_fsm);

		pthread_mutex_lock(&mutex_print);
		printf("P1: Waiting for a connection with a new client\n");
		pthread_mutex_unlock(&mutex_print);

		while (1) {
			// State machine operation
			fsm_fire (q4s_fsm);
			// Waits for CLK_FSM milliseconds
			delay (CLK_FSM);
			pthread_mutex_lock(mutex_process);
			if (*state_proc1 != 1 && *state_proc2 == 0 && *state_proc3 != 1 && *state_proc4 != 1) {
				*state_proc2 = 1;
				pthread_mutex_unlock(mutex_process);
				pid2 = fork();
				if (pid2 != 0 && pid3 != 0 && pid4 != 0) {
					pthread_mutex_lock(mutex_process);
					*p_id_proc2 = pid2;
					pthread_mutex_unlock(mutex_process);
				}
		  } else {
				pthread_mutex_unlock(mutex_process);
			}
			if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
				// State machine destruction
				fsm_destroy (q4s_fsm);
				break;
			}
		}
	}

	if (pid2 == 0 && pid3 != 0 && pid4 != 0) {

		// State machine creation
		q4s_fsm = fsm_new (WAIT_CONNECTION, q4s_table, NULL);

		// State machine initialitation
		fsm_setup (q4s_fsm);

		pthread_mutex_lock(&mutex_print);
		printf("P2: Waiting for a connection with a new client\n");
		pthread_mutex_unlock(&mutex_print);

		while (1) {
			// State machine operation
			fsm_fire (q4s_fsm);
			// Waits for CLK_FSM milliseconds
			delay (CLK_FSM);
			pthread_mutex_lock(mutex_process);
			if (*state_proc1 != 1 && *state_proc2 != 1 && *state_proc3 == 0 && *state_proc4 != 1) {
				*state_proc3 = 1;
				pthread_mutex_unlock(mutex_process);
				pid3 = fork();
				if (pid2 == 0 && pid3 != 0 && pid4 != 0) {
					pthread_mutex_lock(mutex_process);
					*p_id_proc3 = pid3;
					pthread_mutex_unlock(mutex_process);
				}
		  } else {
				pthread_mutex_unlock(mutex_process);
			}
			if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
				// State machine destruction
				fsm_destroy (q4s_fsm);
				break;
			}
		}
	}

	if (pid2 == 0 && pid3 == 0 && pid4 != 0) {

		// State machine creation
		q4s_fsm = fsm_new (WAIT_CONNECTION, q4s_table, NULL);

		// State machine initialitation
		fsm_setup (q4s_fsm);

		pthread_mutex_lock(&mutex_print);
		printf("P3: Waiting for a connection with a new client\n");
		pthread_mutex_unlock(&mutex_print);

		while (1) {
			// State machine operation
			fsm_fire (q4s_fsm);
			// Waits for CLK_FSM milliseconds
			delay (CLK_FSM);
			pthread_mutex_lock(mutex_process);
			if (*state_proc1 != 1 && *state_proc2 != 1 && *state_proc3 != 1 && *state_proc4 == 0) {
				*state_proc4 = 1;
				pthread_mutex_unlock(mutex_process);
				pid4 = fork();
				if (pid2 == 0 && pid3 == 0 && pid4 != 0) {
					pthread_mutex_lock(mutex_process);
					*p_id_proc4 = pid4;
					pthread_mutex_unlock(mutex_process);
				}
		  } else {
				pthread_mutex_unlock(mutex_process);
			}
			if (pid2 == 0 && pid3 == 0 && pid4 == 0) {
				// State machine destruction
				fsm_destroy (q4s_fsm);
				break;
			}
		}
	}

	if (pid2 == 0 && pid3 == 0 && pid4 == 0) {

		// State machine creation
		q4s_fsm = fsm_new (WAIT_CONNECTION, q4s_table, NULL);

		// State machine initialitation
		fsm_setup (q4s_fsm);

		pthread_mutex_lock(&mutex_print);
		printf("P4: Waiting for a connection with a new client\n");
		pthread_mutex_unlock(&mutex_print);

		while (1) {
			// State machine operation
			fsm_fire (q4s_fsm);
			// Waits for CLK_FSM milliseconds
			delay (CLK_FSM);
		}
	}

	pthread_mutex_destroy(mutex_process);
	pthread_mutexattr_destroy(&attrmutex);

	return 0;
}
