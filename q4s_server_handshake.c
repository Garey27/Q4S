#include "q4s_server_handshake.h"

// GENERAL VARIABLES

// FOR Q4S SESSION MANAGING
// Q4S session
static type_q4s_session q4s_session;
// Variable to store the flags
int flags = 0;

// FOR CONNECTION MANAGING
// Structs with info for the connection
struct sockaddr_in server, client;
// Variable for socket assignments
int server_connection, client_connection;
// Variable with struct length
socklen_t longc;
// Variable for socket buffer
char buffer[MAXDATASIZE];

// FOR THREAD MANAGING
// Thread to check reception of data
pthread_t receive_thread;
// Thread to check pressed keys on the keyboard
pthread_t keyboard_thread;
// Variable for mutual exclusion
pthread_mutex_t mutex;


// GENERAL FUNCTIONS

// Waits until next FSM clock activation
void delay (int milliseconds) {
	long pause;
	clock_t now, then;
	pause = milliseconds*(CLOCKS_PER_SEC/1000);
	now = then = clock();
	while((now-then) < pause) {
		now = clock();
	}
}

// Returns current time formatted to be included in Q4S header field "Date"
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


// INITIALITATION FUNCTIONS

// System initialitation
// Creates a thread to explore keyboard and configures mutex
int system_setup (void) {
	pthread_mutex_init(&mutex, NULL);
	int x;
	// Throws a thread to explore PC keyboard
	pthread_create(&keyboard_thread, NULL, (void*)thread_explores_keyboard, NULL);
	return 1;
}

// State machine initialitation
// Puts every flag to 0
void fsm_setup(fsm_t* juego_fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex);
	flags = 0;
	pthread_mutex_unlock(&mutex);
}


// Q4S MESSAGE RELATED FUNCTIONS

// Creation of Q4S 200 OK message
// Creates a message asking server user some Q4S parameters
void create_200 (type_q4s_message *q4s_message) {
	char header[500];  // to store header of the message
  memset(header, '\0', sizeof(header));

	char body[5000];  // to store body of the message
	memset(body, '\0', sizeof(body));

	char input[100]; // to store user inputs
	char client_desire[50]; // to store client suggestions
	char s_value[10];  // to store string values

  // Default SDP parameters
	char v[100] = "v=0\n";
	char o[100] = "o=q4s-UA 53655765 2353687637 IN IP4 127.0.0.1\n";
	char s[100] = "s=Q4S\n";
	char i[100] = "i=Q4S parameters\n";
	char t[100] = "t=0 0\n";

  // Includes user input about QoS levels (showing values suggested by client)
  char a1[100] = "a=qos-level:";
	if (q4s_session.qos_level[0] >= 0) {
		memset(client_desire, '\0', strlen(client_desire));
		memset(s_value, '\0', strlen(s_value));
		strcpy(client_desire, "[Client desire: ");
		sprintf(s_value, "%d", q4s_session.qos_level[0]);
		strcat(client_desire, s_value);
		strcat(client_desire, "]");
	}
	printf("\nEnter upstream QoS level (from 0 to 9) %s: ", client_desire);
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
	printf("Enter downstream QoS level (from 0 to 9) %s: ", client_desire);
  scanf("%s", input);
	strcat(a1, input);
	strcat(a1, "\n");
	memset(input, '\0', strlen(input));

  // Reactive is the default scenary
	char a2[100] = "a=alerting-mode:Reactive\n";

  // Includes user input about alert pause
	char a3[100] = "a=alert-pause:";
	printf("Enter alert pause (in ms): ");
  scanf("%s", input);
	strcat(a3, input);
	strcat(a3, "\n");
  memset(input, '\0', strlen(input));

  // Includes client IP direction
	char a4[100] = "a=public-address:client IP4 ";
	strcat(a4, inet_ntoa(client.sin_addr));
	strcat(a4, "\n");

  // Includes server IP direction
	char a5[100] = "a=public-address:server IP4 ";
	strcat(a5, inet_ntoa(server.sin_addr));
	strcat(a5, "\n");

  // Includes user input about latency threshold (showing value suggested by client)
	char a6[100] = "a=latency:";
	if (q4s_session.latency_th >= 0) {
		memset(client_desire, '\0', strlen(client_desire));
		memset(s_value, '\0', strlen(s_value));
		strcpy(client_desire, "[Client desire: ");
		sprintf(s_value, "%d", q4s_session.latency_th);
		strcat(client_desire, s_value);
		strcat(client_desire, "]");
	}
	printf("Enter latency threshold (in ms) %s: ", client_desire);
  scanf("%s", input);
	strcat(a6, input);
	strcat(a6, "\n");
  memset(input, '\0', strlen(input));

  // Includes user input about jitter thresholds (showing values suggested by client)
	char a7[100] = "a=jitter:";
	if (q4s_session.jitter_th[0] >= 0) {
		memset(client_desire, '\0', strlen(client_desire));
		memset(s_value, '\0', strlen(s_value));
		strcpy(client_desire, "[Client desire: ");
		sprintf(s_value, "%d", q4s_session.jitter_th[0]);
		strcat(client_desire, s_value);
		strcat(client_desire, "]");
	}
	printf("Enter upstream jitter threshold (in ms) %s: ", client_desire);
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
	printf("Enter downstream jitter threshold (in ms) %s: ", client_desire);
  scanf("%s", input);
	strcat(a7, input);
	strcat(a7, "\n");
  memset(input, '\0', strlen(input));

  // Includes user input about bandwidth thresholds (showing values suggested by client)
	char a8[100] = "a=bandwidth:";
	if (q4s_session.bw_th[0] >= 0) {
		memset(client_desire, '\0', strlen(client_desire));
		memset(s_value, '\0', strlen(s_value));
		strcpy(client_desire, "[Client desire: ");
		sprintf(s_value, "%d", q4s_session.bw_th[0]);
		strcat(client_desire, s_value);
		strcat(client_desire, "]");
	}
	printf("Enter upstream bandwidth threshold (in kbps) %s: ", client_desire);
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
	printf("Enter downstream bandwidth threshold (in kbps) %s: ", client_desire);
  scanf("%s", input);
	strcat(a8, input);
	strcat(a8, "\n");
  memset(input, '\0', strlen(input));

  // Includes user input about packetloss thresholds (showing values suggested by client)
	char a9[100] = "a=packetloss:";
	if (q4s_session.packetloss_th[0] >= 0) {
		memset(client_desire, '\0', strlen(client_desire));
		memset(s_value, '\0', strlen(s_value));
		strcpy(client_desire, "[Client desire: ");
		sprintf(s_value, "%.*f", 2, q4s_session.packetloss_th[0]);
		strcat(client_desire, s_value);
		strcat(client_desire, "]");
	}
	printf("Enter upstream packetloss threshold (percentage from 0.00 to 1.00) %s: ", client_desire);
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
	printf("Enter downstream packetloss threshold (percentage from 0.00 to 1.00) %s: ", client_desire);
  scanf("%s", input);
	strcat(a9, input);
	strcat(a9, "\n");
  memset(input, '\0', strlen(input));

  // Includes ports permitted in application flows
	char a10[100] = "a=flow:app clientListeningPort TCP/10000-20000\n";
	char a11[100] = "a=flow:app clientListeningPort UDP/15000-18000\n";
	char a12[100] = "a=flow:app serverListeningPort TCP/56000\n";
	char a13[100] = "a=flow:app serverListeningPort UDP/56000\n";

  // Includes ports permitted for Q4S sessions
	char a14[100] = "a=flow:q4s clientListeningPort UDP/";
	char s_port[10];
	sprintf(s_port, "%d", CLIENT_PORT_UDP);
	strcat(a14, s_port);
	strcat(a14, "\n");
	memset(s_port, '\0', strlen(s_port));
	char a15[100] = "a=flow:q4s clientListeningPort TCP/";
	sprintf(s_port, "%d", CLIENT_PORT_TCP);
	strcat(a15, s_port);
	strcat(a15, "\n");
	memset(s_port, '\0', strlen(s_port));
	char a16[100] = "a=flow:q4s serverListeningPort UDP/";
	sprintf(s_port, "%d", HOST_PORT_UDP);
	strcat(a16, s_port);
	strcat(a16, "\n");
	memset(s_port, '\0', strlen(s_port));
	char a17[100] = "a=flow:q4s serverListeningPort TCP/";
	sprintf(s_port, "%d", HOST_PORT_TCP);
	strcat(a17, s_port);
	strcat(a17, "\n");
	memset(s_port, '\0', strlen(s_port));

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

  // Includes current time
	char h1[100] = "Date: ";
  const char * now = current_time();
	strcat(h1, now);
	strcat(h1, "\n");

	// Type of the content (default)
	char h2[100] = "Content-Type: application/sdp\n";

  // Includes user input about expiration time of Q4S session
	char h3[100] = "Expires: ";
	printf("Enter expiration time of the Q4S session (in ms): ");
  scanf("%s", input);
	strcat(h3, input);
	strcat(h3, "\n");
  memset(input, '\0', strlen(input));

  // Includes signature if necessary
	char h4[100] = "Signature: \n";

  // Includes body length in "Content Length" header field
	char h5[100] = "Content Length: ";
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

  // Delegates in a response creation function
  create_response (q4s_message, "200", "OK", header, body);
}

// Creation of Q4S CANCEL message
// Creates a default CANCEL message
void create_cancel (type_q4s_message *q4s_message) {
	char header[500];
  memset(header, '\0', sizeof(header));

  // Creates default header fields
	char h1[100] = "Content-Type: application/sdp\n";
	char h2[100] = "User-Agent: q4s-ua-experimental-1.0\n";
	char h3[100] = "Content-Length: 0\n";

	// Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);
	strcat(header, h3);

	// Creates an empty body
	char body[5000];
	memset(body, '\0', sizeof(body));

  // Delegates in a request creation function
  create_request (q4s_message,"CANCEL", header, body);
}

// Creation of Q4S requests
// Receives parameters to create the request line (start line)
// Receives prepared header and body from more specific functions
// Stores start line, header and body in a q4s message received as parameter
void create_request (type_q4s_message *q4s_message, char method[10],
	char header[500], char body[5000]) {
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

  // Copies header + body
  fragment1 = strstr(received_message, "\n");
  // Obtains start line
  strncpy(start_line, received_message, strlen(received_message)-strlen(fragment1));
  // Quits initial "\n"
  fragment1 = fragment1 + 1;
  // Copies body
  fragment2 = strstr(received_message, "\n\n");
  // Obtains header
  strncpy(header, fragment1, strlen(fragment1)-(strlen(fragment2)-1));
  // Quits initial "\n\n"
  fragment2 = fragment2 + 2;
  // Obtains body
  strncpy(body, fragment2, strlen(fragment2));

  // Stores Q4S message
	strcpy((q4s_message)->start_line, start_line);
  strcpy((q4s_message)->header, header);
  strcpy((q4s_message)->body, body);
}

// Q4S PARAMETER STORAGE FUNCTION

// Storage of Q4S parameters from a Q4S message
void store_parameters(type_q4s_session *q4s_session, type_q4s_message *q4s_message) {
  // Extracts header
	char header[500];
	strcpy(header, (q4s_message)->header);
	// Creates a copy of header to manipulate it
	char copy_header[5000];
  strcpy(copy_header, header);
	// Extracts body
	char body[5000];
	strcpy(body, (q4s_message)->body);
	// Creates a copy of body to manipulate it
	char copy_body[5000];
  strcpy(copy_body, body);

  // Auxiliary variable
	char *fragment;

	printf("\n");

  // If there is a Expires parameter in the header
	if (fragment = strstr(copy_header, "Expires")){
		fragment = fragment + 9;  // moves to the beginning of the value
		char *s_expires;
		s_expires = strtok(fragment, "\n");  // stores string value
		(q4s_session)->expiration_time = atoi(s_expires);  // converts into int and stores
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_header, header);  // restores copy of header
		printf("Expiration time stored: %d\n", (q4s_session)->expiration_time);
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
		printf("QoS levels stored: %d/%d\n", (q4s_session)->qos_level[0],
		  (q4s_session)->qos_level[1]);
	}

  // If there is an alert pause parameter in the body
	if (fragment = strstr(copy_body, "a=alert-pause:")){
		fragment = fragment + 14;  // moves to the beginning of the value
		char *alert_pause;
		alert_pause = strtok(fragment, "\n");  // stores string value
		(q4s_session)->alert_pause = atoi(alert_pause);  // converts into int and stores
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_body, body);  // restores copy of body
		printf("Alert pause stored: %d\n", (q4s_session)->alert_pause);
	}

  // If there is an latency parameter in the body
	if (fragment = strstr(copy_body, "a=latency:")){
		fragment = fragment + 10;  // moves to the beginning of the value
		char *latency;
		latency = strtok(fragment, "\n");  // stores string value
		(q4s_session)->latency_th = atoi(latency);  // converts into int and stores
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_body, body);  // restores copy of body
		printf("Latency threshold stored: %d\n", (q4s_session)->latency_th);
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
		printf("Jitter thresholds stored: %d/%d\n", (q4s_session)->jitter_th[0],
		  (q4s_session)->jitter_th[1]);
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
		printf("Bandwidth thresholds stored: %d/%d\n", (q4s_session)->bw_th[0],
		  (q4s_session)->bw_th[1]);
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
		printf("Packetloss thresholds stored: %.*f/%.*f\n", 2,
		  (q4s_session)->packetloss_th[0], 2, (q4s_session)->packetloss_th[1]);
	}

}

// CONNECTION FUNCTIONS

// Start up of Q4S server
void start_listening() {
	// Creates socket
	if ((server_connection = socket (AF_INET, SOCK_STREAM, 0)) < 0){
		printf("Error when assigning the socket\n");
		return;
	}

	// Configures server for the connection
	server.sin_family = AF_INET; // protocol assignment
	server.sin_port = htons(HOST_PORT_TCP); // port assignment
  server.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP address assignment (automatic)
	memset(server.sin_zero, '\0', 8); // fills padding with 0s

	// Assigns port to the socket
	if (bind(server_connection, (struct sockaddr*)&server, sizeof(server)) < 0) {
		printf("Error when associating port with connection: %s\n", strerror(errno));
		close(server_connection);
		return;
	}
	if (listen(server_connection, MAX_CONNECTIONS) < 0 ) { // listening
    printf("Error in listen(): %s\n", strerror(errno));
		close(server_connection);
    return;
	}
	printf("Listening in %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
}

// Wait of client connection
void wait_new_connection(){
  longc = sizeof(client); // variable with size of the struct
	// Waits for a connection
	client_connection = accept(server_connection, (struct sockaddr *)&client, &longc);
	if (client_connection < 0) {
		printf("Error when accepting traffic: %s\n", strerror(errno));
		close(server_connection);
		exit(0);
		return;
	}
	pthread_mutex_lock(&mutex);
	flags |= FLAG_NEW_CONNECTION;
	pthread_mutex_unlock(&mutex);
	printf("Connected with %s:%d\n", inet_ntoa(client.sin_addr), htons(client.sin_port));
}

// Delivery of Q4S message to a Q4S client
void send_message (char prepared_message[MAXDATASIZE]) {
	// Copies the message into the buffer
	strncpy (buffer, prepared_message, MAXDATASIZE);
	// Sends the message to the server using the socket assigned
	send(client_connection, buffer, MAXDATASIZE, 0);
	printf("\nI have sent:\n%s\n", buffer);
	memset(buffer, '\0', sizeof(buffer));
}

// Reception of Q4S messages
// Thread function that checks if any message has arrived
void *thread_receives() {
	while(1) {
	  // If error occurs when receiving
	  if (recv(client_connection, buffer, MAXDATASIZE, MSG_WAITALL) < 0) {
		  printf("Error when receiving the data: %s\n", strerror(errno));
		  close(server_connection);
		  return NULL;
	  // If nothing has been received
	  } else if (strlen(buffer) == 0) {
		  return NULL;
    }
		// Copy of the buffer (to avoid buffer modification)
		char copy_buffer[MAXDATASIZE];
		strcpy(copy_buffer, buffer);
		// Auxiliary variable to identify type of Q4S message
		char *start_line;
		start_line = strtok(copy_buffer, "\n"); // stores first line of message
	  if (strcmp(start_line, "BEGIN q4s://www.example.com Q4S/1.0") == 0) {
		  pthread_mutex_lock(&mutex);
		  flags |= FLAG_RECEIVE_BEGIN;
		  pthread_mutex_unlock(&mutex);
		  printf("\nI have received a Q4S BEGIN!\n");
	  } else if (strcmp(start_line, "CANCEL q4s://www.example.com Q4S/1.0") == 0) {
		  pthread_mutex_lock(&mutex);
		  flags |= FLAG_RECEIVE_CANCEL;
		  pthread_mutex_unlock(&mutex);
		  printf("\nI have received a Q4S CANCEL!\n");
	  }
		// Stores the received message to be analized later
		store_message(buffer, &q4s_session.message_received);
	  memset(buffer, '\0', sizeof(buffer));
  }
}


// CHECK FUNCTIONS OF STATE MACHINE

// Checks if new client has connected to the server
int check_new_connection (fsm_t* this) {
	int result;
	wait_new_connection();
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex);
	result = (flags & FLAG_NEW_CONNECTION);
  pthread_mutex_unlock(&mutex);
  return result;
}


// Checks if client wants to start a q4s session (Q4S BEGIN received)
int check_receive_begin (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex);
	result = (flags & FLAG_RECEIVE_BEGIN);
  pthread_mutex_unlock(&mutex);
  return result;
}

// Checks if client wants to cancel a q4s session (Q4S CANCEL received)
int check_receive_cancel (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex);
	result = (flags & FLAG_RECEIVE_CANCEL);
  pthread_mutex_unlock(&mutex);
  return result;
}

// Checks if Actuator has already released the resources
int check_released (fsm_t* this) {
	int result;
  // Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex);
	result = (flags & FLAG_RELEASED);
  pthread_mutex_unlock(&mutex);
  return result;
}


// ACTION FUNCTIONS OF STATE MACHINE

// Prepares for Q4S session
void Setup (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex);
	// Puts every FLAG to 0
	flags = 0;
	pthread_mutex_unlock(&mutex);

  // Initialize all numeric session parameters to -1
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

  // Throws a thread to check the arrival of Q4S messages
	pthread_create(&receive_thread, NULL, (void*)thread_receives, NULL);
}

// Creates and sends a Q4S 200 OK message to the client
void Respond_ok (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex);
  flags &= ~FLAG_RECEIVE_BEGIN;
  pthread_mutex_unlock(&mutex);

  store_parameters(&q4s_session, &(q4s_session.message_received));

	// Fills q4s_session.message_to_send with the Q4S 200 OK parameters
	create_200 (&q4s_session.message_to_send);
	// Stores parameters added to the message
	store_parameters(&q4s_session, &(q4s_session.message_to_send));
  // Converts q4s_session.message_to_send into a message with correct format (prepared_message)
  prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
	// Sends the message to the Q4S client
	send_message(q4s_session.prepared_message);
}

// Tells Actuator to release the resources
void Release (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
  pthread_mutex_lock(&mutex);
  flags &= ~FLAG_RECEIVE_CANCEL;
	pthread_mutex_unlock(&mutex);

  // Send a cancel notification to Actuator
	printf("\nAs Actuator, press 'r' key when you have released the reserved resources \n");
}

// Sends a Q4S CANCEL message to the client, and exits connection with that client
void Cancel (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex);
  // Puts every flag to 0
  flags = 0;
	pthread_mutex_unlock(&mutex);

  // Fills q4s_session.message_to_send with the Q4S CANCEL parameters
  create_cancel(&q4s_session.message_to_send);
	// Converts q4s_session.message_to_send into a message with correct format (prepared_message)
  prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
  // Sends the message to the Q4S client
	send_message(q4s_session.prepared_message);

  // Cancels the thread receiving Q4S messages
  pthread_cancel(receive_thread);
	// Closes connection with Q4S client
  close(client_connection);
  printf("Connection has been closed\n");
	printf("Waiting for a connection with a new client\n");
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
					pthread_mutex_lock(&mutex);
					flags |= FLAG_RELEASED;
					pthread_mutex_unlock(&mutex);
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
	start_listening();

	// State machine: list of transitions
	// {OriginState, CheckFunction, DestinationState, ActionFunction}
	fsm_trans_t q4s_table[] = {
		  { WAIT_CONNECTION, check_new_connection, WAIT_START, Setup},
			{ WAIT_START, check_receive_begin,  HANDSHAKE, Respond_ok },
			{ HANDSHAKE, check_receive_cancel,  TERMINATION, Release },
			{ TERMINATION, check_released, WAIT_CONNECTION,  Cancel },
			{ -1, NULL, -1, NULL }
	};

  // State machine creation
	fsm_t* q4s_fsm = fsm_new (WAIT_CONNECTION, q4s_table, NULL);

	// State machine initialitation
	fsm_setup (q4s_fsm);
	printf("Waiting for a connection with a new client\n");

	while (1) {
		// State machine operation
		fsm_fire (q4s_fsm);
		// Waits for CLK_MS milliseconds
		delay (CLK_MS);
	}

	// State machine destruction
	fsm_destroy (q4s_fsm);
	// Threads destruction
	pthread_cancel(receive_thread);
	pthread_cancel(keyboard_thread);
	return 0;
}
