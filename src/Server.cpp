#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <vector>
#include <regex>
#include <chrono>
#include <unordered_map>

// PORT
unsigned int PORT = 6379;
// DEFAULT ROLE
std::string ROLE("role:master");
// OFFSET
int master_repl_offset = 0;
// MASTER HOST 
std::string master_host = "";
int master_port = 0;
// REPLICA ID
std::string master_replid = "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb";

// Creating Key Value Pair Storage Containter
std::unordered_map<std::string, std::pair<std::string, std::chrono::time_point<std::chrono::system_clock>> > db_container;


// Handling SET Protocol with Expiry
void handle_set(const std::vector<std::string> arguments){
  // Setting max time in case without expiry
  std::chrono::time_point<std::chrono::system_clock> time = 
        std::chrono::time_point<std::chrono::system_clock>::max();

  // Handling Set with expiry
  if(arguments.size() >= 4 && arguments[3] == "px"){
    // Resetting allotted time
    time = std::chrono::system_clock::now() + std::chrono::milliseconds(stoi(arguments[4]));
  }

  // Setting Hash Table values
  db_container[ arguments[1] ] = {arguments[2], time};
          
}

// Sending Handshake
void send_hand_shake(){
  int replica_fd = socket(AF_INET, SOCK_STREAM, 0);
  
  struct sockaddr_in master_addr;
  master_addr.sin_family = AF_INET;
  master_addr.sin_port = htons(master_port);
  master_addr.sin_addr.s_addr = INADDR_ANY; 

  if(connect(replica_fd, (struct sockaddr *) &master_addr, sizeof(master_addr)) == -1) 
  {
    std::cerr << "Replica failed to connect to master\n";
  }
  std::string ping{"*1\r\n$4\r\nping\r\n"};
  send(replica_fd, ping.c_str(), ping.size(), 0);
 
}

/// @brief Handles GET command
/// @param arguments Containing commands and incoming arguments
std::string handle_get(const std::vector<std::string> arguments){
  
  // Setting initial Key
  std::string key = arguments[1];
  auto it = db_container.find(key);
  // Key found
  if( it != db_container.end() ){

    // Check Expiration
    if( it->second.second <= std::chrono::system_clock::now()){
      db_container.erase(it);
      
      return "$-1\r\n";
    }
    else{
      const std::string& value = it->second.first;
      return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
    }
  }
  return "$-1\r\n";
}

/// @brief Handles Redis Commands Protocols as raw string 
/// @param str Encoded Raw String Literal
/// @return Array containing command(s) and incoming arguments
std::vector<std::string> handle_raw_string_protocol( std::string str){
  std::string rt;
  std::vector<std::string> arguments;
  int total_args = str[1] - '0';

  for( int it = 0, args = total_args; it < str.length()-1; it++ ){

      if( str[it] == 'n' && str[it+1] != '$' && args != 0){
        int first_found = str.find_first_of('\\', it);
          if( first_found != std::string::npos ){
            std::string r = str.substr(it + 1, first_found - it - 1  );
            arguments.push_back( r );
            args--;
        }
      }
  }

      return arguments;    
}


/// @brief Performs commands with given arguments from array
/// @param arguments Array containing commands 
/// @return Encoded Action Response
std::string parse_and_respond(const std::vector<std::string> arguments){
  // Setting the commnand from first element in array
  std::string command = arguments[0];
  // Check if command is ECHO
  if (command == "ECHO" ) {
    // Construct the response
    return "$" + std::to_string(arguments[1].size())
          + "\r\n" + arguments[1] + "\r\n";
    }
    // Check if command is to PING
  else if (command == "PING") {
    return "+PONG\r\n";
    }
  // Handling Set Command 
  else if( command == "SET" ) {
    // Setting key value pair
    handle_set(arguments);
    return "$2\r\nOK\r\n";
  }
  // Handling Get Command 
  else if (command == "GET") {
    return handle_get(arguments);
  }
  else if (command == "INFO"){
    if( (arguments[1]) == "replication"){
      ROLE = ROLE + "\n" +"master_replid:" + master_replid + "\n";
      ROLE = ROLE +"master_repl_offset:"+std::to_string(master_repl_offset);
    }
    return "$"+std::to_string(ROLE.length())+"\r\n"+ROLE+"\r\n";
  }
  else {
    return "-ERR unknown command\r\n";
  }
}

/// @brief Handles Redis Command Protocols as string literals
/// @param str Encoded string literal
/// @return Array containg command(s) and incoming arguments
std::vector<std::string> handle_protocol(std::string str){
  std::vector<std::string> arguments;
    size_t pos = 0;
    // Extract the number of arguments
    if (str[pos] == '*') {
        pos++;
        size_t endPos = str.find("\r\n", pos);
        int total_args = std::stoi(str.substr(pos, endPos - pos));
        pos = endPos + 2;

        for (int i = 0; i < total_args; i++) {
            if (str[pos] == '$') {
                pos++;
                endPos = str.find("\r\n", pos);
                int arg_len = std::stoi(str.substr(pos, endPos - pos));
                pos = endPos + 2;

                std::string arg = str.substr(pos, arg_len);
                arguments.push_back(arg);
                pos += arg_len + 2; 
            }
        }
    }else{
      arguments.push_back("PONG");

    }
  

    return arguments;
}


void handle_calls(int client_fd) {
    // Buffer for storing data
    char buffer[1024];

    while (true) {
        // Clear the buffer
        memset(buffer, 0, sizeof(buffer));
        
        // Reading up to 1024 bytes from client into buffer
        ssize_t bytes_read = read(client_fd, buffer, 1024);
        if (bytes_read <= 0) {
            break;
        }
        
        // Ensure the buffer is null-terminated
        buffer[bytes_read] = '\0';
  
       std::string letter(buffer);
       auto arguments = handle_protocol(letter);

        // Process the command
        std::string command = parse_and_respond(arguments);

        // Send the response to the client
        send(client_fd, command.c_str(), command.size(), 0);
    }
    close(client_fd);
  
}


int main(int argc, char **argv) {
  
  // Reading Arguments from Command Line
   for (int i = 0; i < argc; i++) {
        // Comparing and set port flag
        if (strcmp(argv[i], "--port")==0) {
            PORT = std::stoi(argv[++i]);
          // Comparing and setting the replication flag
        } else if (strcmp(argv[i], "--replicaof")==0) {
            ROLE = std::string("role:slave");
            std::string master = argv[++i];
            //std::cout<<master<<"here"<<std::endl;
            master_host = master.substr(0, master.find(" "));
            master_port = std::stoi(master.substr(master.find(" ") + 1));
            send_hand_shake();
        }
    }

  // First Terminal: ./Server
  // Second Terminal: nc 127.0.0.1 6379
  std::cout << "Logs from your program will appear here!\n";

  // Obtaining a Socket that returns an Fd
  // An Fd is integer that refers to something in the linux kernel
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  
  // Setsockopt is used to configure various aspects of the socket
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }

  // Binding on the wildcard address: 0.0.0.0:1234:
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  
  std::cout << "Waiting for a client to connect...\n";
  
  int client_fd;
  //client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
  // Creating Buffer for storing information and 
  // handling multiple connections from the same client
  char buffer[1024] = {0};
  // Using thread vector for holding concurrent calls 
  std::vector<std::thread> thread_handler; 

  // Reading in input continously
  while(true){
    // Connect to Client
    client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    // Connected Message
    std::cout << "Client connected\n";
    // Pushing threads to handle concurrent client calls
    thread_handler.push_back(std::thread(handle_calls, client_fd));
  }

  close(server_fd);



  
  

  return 0;
}
