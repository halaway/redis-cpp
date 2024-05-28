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

std::string parse_and_respond(const std::string& input) {
    // Define regex patterns for the Redis protocol
    std::regex array_size_pattern(R"(\*(\d+)\r\n)");
    std::regex bulk_string_pattern(R"(\$(\d+)\r\n(.+?)\r\n)");

    std::smatch matches;
    
    // Check if input starts with array size pattern
    if (!std::regex_search(input, matches, array_size_pattern)) {
        return "-ERR Protocol error: invalid multibulk length\r\n";
    }

    int array_size = std::stoi(matches[1].str());
    
    // Find the positions of the command and arguments
    size_t offset = matches.position(0) + matches.length(0);
    std::string command, argument;
    
    // Extract command
    if (std::regex_search(input.begin() + offset, input.end(), matches, bulk_string_pattern)) {
        command = matches[2].str();
        offset += matches.position(0) + matches.length(0);
    }

    // Extract argument
    if (std::regex_search(input.begin() + offset, input.end(), matches, bulk_string_pattern)) {
        argument = matches[2].str();
    }

    // Check if the command is ECHO
    if (command == "ECHO" && array_size == 2) {
        // Construct the response
        return "$" + std::to_string(argument.size()) + "\r\n" + argument + "\r\n";
    }
    else if (command == "PING" && array_size == 1) {
        return "+PONG\r\n";
    } 
    
    else {
        return "-ERR unknown command\r\n";
    }
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
        
        // Process the command
        std::string command = parse_and_respond(buffer);
        
        // Send the response to the client
        send(client_fd, command.c_str(), command.size(), 0);
    }
    close(client_fd);


  /*
  memset(buffer, '\0', 256);
  std::string PING_STRING("*1\r\n$4\r\nPING\r\n");
  
  while (read(client_fd, buffer, 256))
  {
    if(memcmp(buffer,"REDIS_PING", PING_STRING.size()))
    {
      send(client_fd, "+PONG\r\n", 7, 0);
    }
    
  }
  */
  
}

int main(int argc, char **argv) {
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
  server_addr.sin_port = htons(6379);
  
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
  std::cout << "Client connected\n";

  // Creating Buffer for storing information and 
  // handling multiple connections from the same client
  char buffer[1024] = {0};
  // Using thread vector for holding concurrent calls 
  std::vector<std::thread> thread_handler; 

  // Reading in input continously
  while(true){
    // Connect to Client
    client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    // Pushing threads to handle concurrent client calls
    thread_handler.push_back(std::thread(handle_calls, client_fd));
  }

  close(server_fd);


  return 0;
}