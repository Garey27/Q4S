# Q4S

// Compiling files

gcc q4s_server_verbose -o server_exec -pthread -lm -lcrypto
gcc q4s_client_verbose -o client_exec -pthread -lm -lcrypto


// Verbose mode (default)

./server_exec
./client_exec

// Non-verbose mode

./server_exec 2</dev/null
./client_exec 2</dev/null
