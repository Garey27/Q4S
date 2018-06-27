# Q4S

// Compiling files

gcc q4s_server -o server_exec -pthread -lm -lcrypto
gcc q4s_client -o client_exec -pthread -lm -lcrypto


// Executing files

./server_exec
./client_exec

