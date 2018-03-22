#include "q4s_client.h"

// GENERAL VARIABLES

// FOR Q4S SESSION MANAGING
// Q4S session
static type_q4s_session q4s_session;
// Variable to store the flags
int flags = 0;

// FOR CONNECTION MANAGING
// Structs with info for the connection
struct sockaddr_in client, server;
// Struct with host info
struct hostent *host;
// Variable for socket assignment
int connection;
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

// Creation of Q4S BEGIN message
// Creates a default message unless client wants to suggest Q4S quality thresholds
void create_begin (type_q4s_message *q4s_message) {
  char input[100]; // to store user inputs
	printf("\nDo you want to specify desired quality thresholds? (yes/no): ");
  scanf("%s", input); // variable input stores user's answer

  char body[5000]; // it will be empty or filled with SDP parameters
	memset(body, '\0', sizeof(body)); // body is empty by default

  // If user wants to suggest Q4S quality thresholds
	if (strstr(input, "yes")) {
		// Default SDP parameters
		char v[100] = "v=0\n";
		char o[100] = "o=\n";
		char s[100] = "s=Q4S\n";
		char i[100] = "i=Q4S desired parameters\n";
		char t[100] = "t=0 0\n";

    // Includes user's suggestion about QoS levels (upstream and downstream)
		char a1[100] = "a=qos-level:";
		printf("\nEnter upstream QoS level (from 0 to 9): ");
	  scanf("%s", input);
		strcat(a1, input);
		strcat(a1, "/");
	  memset(input, '\0', strlen(input));
		printf("Enter downstream QoS level (from 0 to 9): ");
	  scanf("%s", input);
		strcat(a1, input);
		strcat(a1, "\n");
		memset(input, '\0', strlen(input));

    // Includes user's suggestion about latency threshold
		char a2[100] = "a=latency:";
		printf("Enter latency threshold (in ms): ");
	  scanf("%s", input);
		strcat(a2, input);
		strcat(a2, "\n");
	  memset(input, '\0', strlen(input));

    // Includes user's suggestion about jitter threshold (upstream and downstream)
		char a3[100] = "a=jitter:";
		printf("Enter upstream jitter threshold (in ms): ");
	  scanf("%s", input);
		strcat(a3, input);
		strcat(a3, "/");
	  memset(input, '\0', strlen(input));
		printf("Enter downstream jitter threshold (in ms): ");
	  scanf("%s", input);
		strcat(a3, input);
		strcat(a3, "\n");
	  memset(input, '\0', strlen(input));

    // Includes user's suggestion about bandwidth threshold (upstream and downstream)
		char a4[100] = "a=bandwidth:";
		printf("Enter upstream bandwidth threshold (in kbps): ");
	  scanf("%s", input);
		strcat(a4, input);
		strcat(a4, "/");
	  memset(input, '\0', strlen(input));
		printf("Enter downstream bandwidth threshold (in kbps): ");
	  scanf("%s", input);
		strcat(a4, input);
		strcat(a4, "\n");
	  memset(input, '\0', strlen(input));

    // Includes user's suggestion about packetloss threshold (upstream and downstream)
		char a5[100] = "a=packetloss:";
		printf("Enter upstream packetloss threshold (percentage from 0.00 to 1.00): ");
	  scanf("%s", input);
		strcat(a5, input);
		strcat(a5, "/");
	  memset(input, '\0', strlen(input));
		printf("Enter downstream packetloss threshold (percentage from 0.00 to 1.00): ");
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
	}

	char header[500]; // it will be filled with header fields
  memset(header, '\0', sizeof(header));

  // Prepares some header fields
	char h1[100] = "Content-Type: application/sdp\n";
	char h2[100] = "User-Agent: q4s-ua-experimental-1.0\n";

  // Includes body length in "Content Length" header field
	char h3[100] = "Content Length: ";
	int body_length = strlen(body);
	char s_body_length[10];
	sprintf(s_body_length, "%d", body_length);
  strcat(h3, s_body_length);
	strcat(h3, "\n");

  // Prepares header with header fields
	strcpy(header, h1);
	strcat(header, h2);
	strcat(header, h3);

  // Delegates in a request creation function
  create_request (q4s_message,"BEGIN", header, body);
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

  // If there is a Session ID parameter in the header
	if (fragment = strstr(copy_header, "Session-Id")) {
		fragment = fragment + 12;  // moves to the beginning of the value
		char *string_id;
		string_id = strtok(fragment, "\n");  // stores string value
		(q4s_session)->session_id = atoi(string_id);  // converts into int and stores
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_header, header);  // restore copy of header
		printf("Session ID stored: %d\n", (q4s_session)->session_id);
	} else if (fragment = strstr(copy_body, "o=")){  // if Session ID is in the body
		fragment = strstr(fragment, " ");  // moves to the beginning of the value
		char *string_id;
		string_id = strtok(fragment, " ");  // stores string value
		(q4s_session)->session_id = atoi(string_id);  // converts into int and stores
		memset(fragment, '\0', strlen(fragment));
		strcpy(copy_body, body);  // restores copy of header
		printf("Session ID stored: %d\n", (q4s_session)->session_id);
	}

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

// Connection establishment with Q4S server
int connect_to_server() {
	host = gethostbyname("localhost"); // server assignment
	if (host == NULL) {
		printf("Incorrect host\n");
		return -1;
	}
	if ((connection =  socket(AF_INET, SOCK_STREAM, 0)) < 0) { // socket assignment
		printf("Error when assigning the socket: %s\n", strerror(errno));
		return -1;
	}

  // Configures client for the connection
	client.sin_family = AF_INET; // protocol assignment
	client.sin_port = htons(CLIENT_PORT_TCP); // port assignment
  client.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP address assignment (automatic)
	memset(client.sin_zero, '\0', 8); // fills padding with 0s

	// Assigns port to the client's socket
	if (bind(connection, (struct sockaddr*)&client, sizeof(client)) < 0) {
		printf("Error when associating port with connection: %s\n", strerror(errno));
		close(connection);
		return -1;
	}

	printf("\nTCP socket assigned to %s:%d\n", inet_ntoa(client.sin_addr), htons(client.sin_port));

  // Especifies parameters of the server
  server.sin_family = AF_INET; // protocol assignment
	server.sin_port = htons(HOST_PORT_TCP); // port assignment
	server.sin_addr = *((struct in_addr *)host->h_addr); // copies host IP address
  memset(server.sin_zero, '\0', 8); // fills padding with 0s

  printf("Willing to connect to %s:%d\n", inet_ntoa(server.sin_addr), htons(server.sin_port));
	// Connects to the host (Q4S server)
	if (connect(connection,(struct sockaddr *)&server, sizeof(server)) < 0) {
		printf ("Error when connecting to the host: %s\n", strerror(errno));
		close(connection);
		return -1;
	}
	printf("Connected to %s:%d\n", inet_ntoa(server.sin_addr), htons(server.sin_port));
  return 1;
}

// Delivery of Q4S message to the server
void send_message (char prepared_message[MAXDATASIZE]) {
	// Copies the message into the buffer
	strncpy (buffer, prepared_message, MAXDATASIZE);
	// Sends the message to the server using the socket assigned
	send(connection, buffer, MAXDATASIZE, 0);
	printf("\nI have sent:\n%s\n", buffer);
	memset(buffer, '\0', sizeof(buffer));
}

// Reception of Q4S messages from the server
// Thread function that checks if any message has arrived
void *thread_receives() {
	while(1) {
		// If error occurs when receiving
	  if (recv(connection, buffer, MAXDATASIZE, MSG_WAITALL) < 0) {
		  printf("Error when receiving the data: %s\n", strerror(errno));
		  close(connection);
			exit(0);
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
		// If it is a Q4S 200 OK message
		if (strcmp(start_line, "Q4S/1.0 200 OK") == 0) {
			pthread_mutex_lock(&mutex);
			flags |= FLAG_RECEIVE_OK;
			pthread_mutex_unlock(&mutex);
			printf("I have received a Q4S 200 OK!\n");
		// If it is a Q4S CANCEL message
	} else if (strcmp(start_line, "CANCEL q4s://www.example.com Q4S/1.0") == 0) {
			pthread_mutex_lock(&mutex);
			flags |= FLAG_RECEIVE_CANCEL;
			pthread_mutex_unlock(&mutex);
			printf("I have received a Q4S CANCEL!\n");
		}
		// Stores the received message to be analized later
		store_message(buffer, &q4s_session.message_received);
	  memset(buffer, '\0', sizeof(buffer));
  }
}


// CHECK FUNCTIONS OF STATE MACHINE

// Checks if client wants to connect server
int check_connect (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex);
	result = (flags & FLAG_CONNECT);
  pthread_mutex_unlock(&mutex);
  return result;
}

// Checks if a client wants to send a Q4S BEGIN to the server
int check_begin (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex);
	result = (flags & FLAG_BEGIN);
  pthread_mutex_unlock(&mutex);
  return result;
}

// Checks if a Q4S 200 OK message has been received from the server
int check_receive_ok (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
  pthread_mutex_lock(&mutex);
	result = (flags & FLAG_RECEIVE_OK);
  pthread_mutex_unlock(&mutex);
	return result;
}

// Checks if client wants to cancel Q4S session
int check_cancel (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex);
	result = (flags & FLAG_CANCEL);
  pthread_mutex_unlock(&mutex);
	return result;
}

// Checks if q4s client has received a Q4S CANCEL from server
int check_receive_cancel (fsm_t* this) {
	int result;
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex);
	result = (flags & FLAG_RECEIVE_CANCEL);
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
	// Tries to connect to Q4S server
	if (connect_to_server() < 0) {
		exit(0);
	} else {
		printf("\nPress 'b' to send a Q4S BEGIN\n");
		// Throws a thread to check the arrival of Q4S messages
		pthread_create(&receive_thread, NULL, (void*)thread_receives, NULL);
	}
}

// Creates and sends a Q4S BEGIN message to the server
void Begin (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex);
  flags &= ~FLAG_BEGIN;
  pthread_mutex_unlock(&mutex);

  // Fills q4s_session.message_to_send with the Q4S BEGIN parameters
	create_begin(&q4s_session.message_to_send);
  // Converts q4s_session.message_to_send into a message with correct format (prepared_message)
  prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
  // Sends the prepared message
  send_message(q4s_session.prepared_message);
}

// Stores parameters received in the first 200 OK message from server
void Store (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex);
  flags &= ~FLAG_RECEIVE_OK;
  pthread_mutex_unlock(&mutex);
	// Stores parameters of message received
  store_parameters(&q4s_session, &(q4s_session.message_received));
	printf("\nPress 'c' to send a Q4S CANCEL\n");
}

// Creates and sends a Q4S CANCEL message to the server
void Cancel (fsm_t* fsm) {
  // Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex);
  flags &= ~FLAG_CANCEL;
  pthread_mutex_unlock(&mutex);

  // Fills q4s_session.message_to_send with the Q4S CANCEL parameters
	create_cancel(&q4s_session.message_to_send);
  // Converts q4s_session.message_to_send into a message with correct format (prepared_message)
  prepare_message(&(q4s_session.message_to_send), q4s_session.prepared_message);
  // Sends the prepared message
  send_message(q4s_session.prepared_message);
}

// Exits Q4S session
void Exit (fsm_t* fsm) {
	// Lock to guarantee mutual exclusion
	pthread_mutex_lock(&mutex);
	// Puts every FLAG to 0
	flags = 0;
	pthread_mutex_unlock(&mutex);

  // Cancels the thread receiving Q4S messages
	pthread_cancel(receive_thread);
	// Closes connection with Q4S server
  close(connection);
	printf("\nConnection has been closed\n");
	printf("Press 'q' to connect to the Q4S server\n");
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
				// If "q" (of "q4s") has been pressed, FLAG_CONNECT is activated
				case 'q':
				  // Lock to guarantee mutual exclusion
					pthread_mutex_lock(&mutex);
					flags |= FLAG_CONNECT;
					pthread_mutex_unlock(&mutex);
					break;
				// If "b" (of "begin") has been pressed, FLAG_BEGIN is activated
				case 'b':
				  // Lock to guarantee mutual exclusion
					pthread_mutex_lock(&mutex);
					flags |= FLAG_BEGIN;
					pthread_mutex_unlock(&mutex);
					break;
				// If "c" (of "cancel") has been pressed, FLAG_CANCEL is activated
        case 'c':
          // Lock to guarantee mutual exclusion
          pthread_mutex_lock(&mutex);
          flags |= FLAG_CANCEL;
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

	// State machine: list of transitions
	// {OriginState, CheckFunction, DestinationState, ActionFunction}
	fsm_trans_t q4s_table[] = {
		  { WAIT_CONNECT, check_connect, WAIT_START, Setup },
			{ WAIT_START, check_begin,  HANDSHAKE, Begin },
			{ HANDSHAKE, check_receive_ok,  HANDSHAKE, Store },
			{ HANDSHAKE, check_go_to_0,  STAGE_0, Ready0 },
			{ HANDSHAKE, check_go_to_1,  STAGE_1, Ready1 },
			{ STAGE_0, check_receive_ok, PING_MEASURE_0, Ping },
			{ PING_MEASURE_0, check_temp_ping_0, PING_MEASURE_0, Ping },
			{ PING_MEASURE_0, check_receive_ok, PING_MEASURE_0, Update },
			{ PING_MEASURE_0, check_receive_ping, PING_MEASURE_0, Update },
			{ PING_MEASURE_0, check_finish_ping, PING_MEASURE_0, Compare },
			{ PING_MEASURE_0, check_go_to_1, STAGE_1, Ready1 },
			{ STAGE_1, check_receive_ok, BWIDTH_MEASURE, Bwidth },
			{ BWIDTH_MEASURE, check_temp_bwidth, BWIDTH_MEASURE, Bwidth },
			{ BWIDTH_MEASURE, check_receive_bwidth, BWIDTH_MEASURE, Update },
			{ BWIDTH_MEASURE, check_finish_bwidth, BWIDTH_MEASURE, Compare },
			{ BWIDTH_MEASURE, check_go_to_2, STAGE_2, Ready2 },
			{ STAGE_2, check_receive_ok, PING_MEASURE_2,  Ping },
			{ PING_MEASURE_2, check_temp_ping_2, PING_MEASURE_2, Ping },
			{ PING_MEASURE_2, check_receive_ok, PING_MEASURE_2, Update },
			{ PING_MEASURE_2, check_receive_ping, PING_MEASURE_2, Update },
			{ PING_MEASURE_2, check_cancel, TERMINATION, Cancel},
			{ TERMINATION, check_receive_cancel, WAIT_CONNECT,  Exit },
			{ -1, NULL, -1, NULL }
	};

  // State machine creation
	fsm_t* q4s_fsm = fsm_new (WAIT_CONNECT, q4s_table, NULL);

	// State machine initialitation
	fsm_setup (q4s_fsm);
	printf("Press 'q' to connect to the Q4S server\n");

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
