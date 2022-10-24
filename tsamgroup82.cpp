//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000 
//
// Author: Jacky Mallett (jacky@ru.is)
//
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>
#include <list>
#include <queue>

#include <iostream>
#include <sstream>
#include <thread>
#include <map>

#include <time.h>
#include <thread>
#include <unistd.h>

// fix SOCK_NONBLOCK for OSX
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#define BACKLOG  5          // Allowed length of queue of waiting connections
#define BUFFER_SIZE  1025
//
// Globals
//

std::string GROUP = "P3_GROUP82";
std::string IP = "130.208.243.61";
std::string PORT;

// Simple class for handling connections from clients.
//
// Client(int socket) - socket to send/receive traffic from client.

class Client
{
  public:
    int sock;              // socket of client connection
    std::string name;           // Limit length of name of client's user

    Client(int socket) : sock(socket){} 

    ~Client(){}            // Virtual destructor defined for base class
};

class Server
{
  public:
    int socket;              // socket of Server connection
    std::string ip;
    int port;
    std::string group;      // GroupID
    std::vector<std::string> messages;
    Server(int socket, std::string ip, int port, std::string group) : 
        socket{socket},
        ip{ip},
        port{port},
        group{group}
        {};
    ~Server(){};            // Virtual destructor defined for base Server
};
class Holder
{
    public:
        std::string group;
        Holder(std::string group) : group{group} {};
        ~Holder(){};
};
// Note: map is not necessarily the most efficient method to use here,
// especially for a server with large numbers of simulataneous connections,
// where performance is also expected to be an issue.
//
// Quite often a simple array can be used as a lookup table, 
// (indexed on socket no.) sacrificing memory for speed.

std::map<int, Client*> clients; // Lookup table for socketid per Client information
std::map<std::string, Server*> oneHopServers;
std::map<std::string, Server*> servers; // Lookup table for groupId per Server information
std::map<int, Holder*> maps; // Lookup table mapping socket to ip

std::vector<std::string> messages; // Message meant for this server, kept for client to fetch

// Open socket for specified port.
//
// Returns -1 if unable to create the socket for any reason.

void send_message(int socket, std::string cmd){
    typedef unsigned char Byte;
    Byte SOH[1] = {0x01};
    Byte EOT[1] = {0x04};
    std::string newCmd;

    newCmd.append((const char*)SOH, 1);
    newCmd += cmd;
    newCmd.append((const char*)EOT, 1);

    std::cout << "SENT <"<<newCmd << "> TO SERVER"<<std::endl;

    send(socket, newCmd.c_str(), newCmd.length(),0);

}
void listenServer(int serverSocket)
{
    int nread;                                  // Bytes read from socket
    char buffer[1025];                          // Buffer for reading input

    while(true)
    {
       memset(buffer, 0, sizeof(buffer));
       nread = read(serverSocket, buffer, sizeof(buffer));

       if(nread == 0)                      // Server has dropped us
       {
          printf("Over and Out\n");
          exit(0);
       }
       else if(nread > 0)
       {
          printf("%s\n", buffer);
       }
    }
}

int open_socket(int portno)
{
   struct sockaddr_in sk_addr;   // address settings for bind()
   int sock;                     // socket opened for this port
   int set = 1;                  // for setsockopt

   // Create socket for connection. Set to be non-blocking, so recv will
   // return immediately if there isn't anything waiting to be read.
#ifdef __APPLE__     
   if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      perror("Failed to open socket");
      return(-1);
   }
#else
   if((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
   {
     perror("Failed to open socket");
    return(-1);
   }
#endif

   // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
   // program exit.

   if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
   {
      perror("Failed to set SO_REUSEADDR:");
   }
   set = 1;
#ifdef __APPLE__     
   if(setsockopt(sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
   {
     perror("Failed to set SOCK_NOBBLOCK");
   }
#endif
   memset(&sk_addr, 0, sizeof(sk_addr));

   sk_addr.sin_family      = AF_INET;
   sk_addr.sin_addr.s_addr = inet_addr(IP.c_str());
   sk_addr.sin_port        = htons(portno);

   // Bind to socket to listen for connections from clients

   if(bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
   {
      perror("Failed to bind to socket:");
      return(-1);
   }
   else
   {
      return(sock);
   }
}


int connect_socket(std::string ip, std::string port)
{
   struct sockaddr_in sk_addr;   // address settings for bind()
   int sock;                     // socket opened for this port
   int set = 1;                  // for setsockopt

   // Create socket for connection. Set to be non-blocking, so recv will
   // return immediately if there isn't anything waiting to be read.
#ifdef __APPLE__     
   if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      perror("Failed to open socket");
      return(-1);
   }
#else
   if((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
   {
     perror("Failed to open socket");
    return(-1);
   }
#endif

   // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
   // program exit.

   if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
   {
      perror("Failed to set SO_REUSEADDR:");
   }
   set = 1;
#ifdef __APPLE__     
   if(setsockopt(sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
   {
     perror("Failed to set SOCK_NOBBLOCK");
   }
#endif
   memset(&sk_addr, 0, sizeof(sk_addr));

   sk_addr.sin_family      = AF_INET;
   sk_addr.sin_addr.s_addr = inet_addr(ip.c_str());
   sk_addr.sin_port        = htons(std::stoi(port));

   // Bind to socket to listen for connections from clients

    if(connect(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr) )< 0)
   {
       // EINPROGRESS means that the connection is still being setup. Typically this
       // only occurs with non-blocking sockets. (The serverSocket above is explicitly
       // not in non-blocking mode, so this check here is just an example of how to
       // handle this properly.)
       if(errno != EINPROGRESS)
       {
         printf("Failed to open socket to server: %s\n", ip.c_str());
         perror("Connect failed: ");
         exit(0);
       }
       
   }

   else {
    return sock;
   }
   return sock;

}
// Close a client's connection, remove it from the client list, and
// tidy up select sockets afterwards.


void closeConnection(int Socket, fd_set *openSockets, int *maxfds)
{

     printf("Client closed connection: %d\n", Socket);

     // If this client's socket is maxfds then the next lowest
     // one has to be determined. Socket fd's can be reused by the Kernel,
     // so there aren't any nice ways to do this.

     close(Socket);      

     if(*maxfds == Socket)
     {
        for(auto const& p : clients)
        {
            *maxfds = std::max(*maxfds, p.second->sock);
        }
     }

     // And remove from the list of open sockets.

     FD_CLR(Socket, openSockets);

}


void fetchCommand(int socket) {
    std::string command = "FETCH_MSG," + GROUP;
    send_message(socket, command);

}

// Process command from client on the server
std::vector<std::string> fetchTokens(char *buffer){

    std::vector<std::string> tokens;
    std::string token;
    std::string subtoken;
    std::stringstream stream(buffer);
    
    while(std::getline(stream, token, ',')){
        if(int pos = token.find(';') != std::string::npos){
            subtoken = token.substr(0, pos);
            tokens.push_back(subtoken);
            subtoken = token.substr(pos+1, -1);
            tokens.push_back(subtoken);
        }
        else{
            tokens.push_back(token);
        }
    }
    return tokens;
}

std::vector<std::string> get_message(char *buffer, int commas){
    //This function returns a vector of strings, last is the message and the other are the tokens
    std::vector<std::string> tokens;
    std::string token;
    std::string subtoken;
    
    std::stringstream stream(buffer);
    
    if (getline(stream,token))
    {   
        std::string del = ",";

        int c = 0;
        while(c<commas){
            subtoken = token.substr(0, token.find(del));
            tokens.push_back(subtoken);
            token = token.substr(token.find(del)+1, -1);
            c++;
        }
        tokens.push_back(token);
    }
    return tokens;
}
std::vector<std::string> get_message(std::string token, int commas){
    //This function returns a vector of strings, last is the message and the other are the tokens
    std::vector<std::string> tokens;
    std::string subtoken;


 

    std::string del = ",";

    int c = 0;
    
    while(c<commas){
       
        subtoken = token.substr(0, token.find(del));
        tokens.push_back(subtoken);
        token = token.substr(token.find(del)+1, token.length());

        c++;

    }
    tokens.push_back(token);

    return tokens;
}
// Iterates over the open
void broadcast() {
    
}

std::string filter(char* message) {
    //Checking for SOH
    if(message[0] != 0x01) {
        return "";
    }

    int i;
    // If we encounter a EOT return the message in between
    // Otherwise return an empty string
    for(i = 1; i < BUFFER_SIZE; i++) {
        if(message[i] == 0x04)
            return std::string(message + 1, i - 1);
    }

    return "";
}
std::pair<char*,char*> getip(int newfd) {
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    int res = getpeername(newfd, (struct sockaddr *)&addr, &addr_size);
    char *clientip = new char[20];
    char *port = new char[20];
    strcpy(clientip, inet_ntoa(addr.sin_addr));
    strcpy(port, std::to_string(addr.sin_port).c_str());

    return std::make_pair(clientip, port);
}

void Command(int Socket, fd_set *openSockets, int *maxfds, 
                  char *buffer) 
{

    std::vector<std::string> tokens;
    std::vector<std::string> b;
    // Split command  into tokens for parsing
    std::string token = filter(buffer);
    std::string x = "";
    std::pair<char*, char*> p;

    tokens = get_message(token, 1);
    int c = tokens[0].compare("JOIN");

    if(tokens.size()==0){
        std::string msg = "SOH or EOT was not found sowwy\nMessage was not proccesed";
        send(Socket, msg.c_str(), msg.length(), 0);
    }
    else if((tokens[0].compare("JOIN")) == 0)
    { 
        p = getip(Socket);

        typedef unsigned char Byte;
        std::cout << "WENT IN JOIN" << std::endl;
        maps[Socket] = new Holder(tokens[1]);

        servers[tokens[1]] = new Server(Socket, p.first, std::stoi(p.second), tokens[1]);
        std::string msg;
       
        msg = msg + "SERVERS," + 
        GROUP + "," + 
        IP + "," +
        PORT + ";";
        for(auto const& server : servers)
        {
            msg += server.second->group + "," + server.second->ip+ "," + std::to_string(server.second->port)+";";

        }
        msg.pop_back();
        send_message(Socket, msg);

    }

    else if(tokens[0].compare("SERVERS") == 0){
        std::cout << "WENT IN SERVERS" << std::endl;
        b = get_message(tokens[1], 2);
        servers[b[0]]->ip = b[1];
        servers[b[0]]->port = std::stoi(b[2]);

        int c = 0;
        //maps[Socket] = new Holder(tokens[c+1]);
        //std::stoi(tokens[c+2]
        //servers[tokens[1]] = new Server(Socket, tokens[c+1], std::stoi(tokens[c+2]), tokens[c+3]);
        //
        //more, try to connect to other servers

    }
        
    // Fetches message from that socket 
    else if((tokens[0].compare("KEEPALIVE")) == 0){
        if(tokens[1] != "0"){

        }
        else{
            fetchCommand(Socket);
        }

    }
    else if((tokens[0].compare("FETCH_MSGS")) == 0){
        if(tokens.size() == 2) {
            std::string groupId = tokens[1];

            // Groupid is 1hop away
            if (servers.find(groupId) != servers.end()) {
                if(servers[groupId]->messages.size() != 0){
                    std::string msg = servers[groupId]->messages[servers[groupId]->messages.size()-1];
                    servers[groupId]->messages.pop_back();
                    send_message(servers[groupId]->socket, msg);
                }
                else{
                    std::string msg = "FETCH_MSGS," + groupId;
                    for(auto server = servers.begin(); server != servers.end(); server++) {
                        send_message(server->second->socket, msg);
                    }
                }
                
            }
            else{
                std::string msg = "FETCH_MSGS," + groupId;
                for(auto server = servers.begin(); server != servers.end(); server++) {
                    send_message(server->second->socket, msg);
                }
            }
        }
    }
    else if((tokens[0].compare("SEND_MSGS")) == 0){
        b = get_message(tokens[1], 2);

        std::string toGroupId = b[0];
        std::string fromGroupId = b[1];
        std::string message = b[2];
        
        // Check if the message is meant for us

        if(toGroupId == GROUP)
            messages.push_back(message);
        
        std::string msg = toGroupId +","+ fromGroupId +","+ message;

        // Check if message is meant for someone connected in 1 hop distance
        std::map<std::string, Server*>::iterator server;
        if((server = servers.find(toGroupId)) != servers.end()){
            send_message(server->second->socket, msg);
        }
        else{
            // Else just broadcast it to everyone in 1 hop distance
            for(auto server = servers.begin(); server != servers.end(); server++) {
                send_message(server->second->socket, msg);
            }
        }
        

    }
    //STATUSREP
    //
    // This is slightly fragile, since it's relying on the order
    // of evaluation of the if statement.
    else if((tokens[0].compare("SEND")) == 0){
        b = get_message(tokens[1], 1);


        std::string msg = "SEND_MSG," + b[0]+","+GROUP + "," + b[1];

        for(auto server = servers.begin(); server != servers.end(); server++) {

            send_message(server->second->socket, msg);
        }
    }
    else if((tokens[0].compare("FETCH")) == 0){

        std::string msg = "SEND FETCH_MSGS," + b[1];

        for(auto server = servers.begin(); server != servers.end(); server++) {

            send_message(server->second->socket, msg);
        }
    }
    else if((tokens[0].compare("QUERYSERVERS")) == 0){
        b = get_message(tokens[1], 1);


        std::string msg = "";

        for(auto server = servers.begin(); server != servers.end(); server++) {

            msg += server->second->group + ",";
        }
        msg.pop_back();
        send_message(Socket, msg);
    }
    else
    {
        std::cout << "Unknown command from client:" << buffer << std::endl;
    }
    
     
}

int main(int argc, char* argv[])
{
    bool finished;
    int listenSock;                 // Socket for connections to server
    int clientSock;                 // Socket of connecting client
    fd_set openSockets;             // Current open sockets 
    fd_set readSockets;             // Socket list for select()        
    fd_set exceptSockets;           // Exception socket list
    int maxfds;                     // Passed to select() as max fd in set
    struct sockaddr_in client;
    socklen_t clientLen;
    char buffer[BUFFER_SIZE];              // buffer for reading from clients
    int nwrite;
    std::vector<std::string> arguments;
    std::vector<std::string> b;
    std::string group;
    std::string custom;
    int x;
    if(argc != 3)
    {
        printf("Usage: chat_server <ip port> <s port>\n");
        exit(0);
    }

    PORT = argv[1];

    // Setup socket for server to listen to
    
    listenSock = open_socket(atoi(argv[1])); 
    printf("Listening on port: %d\n", atoi(argv[1]));

    if(listen(listenSock, BACKLOG) < 0)
    {
        printf("Listen failed on port %s\n", argv[1]);
        exit(0);
    }
    else 
    // Add listen socket to socket set we are monitoring
    {   

        FD_ZERO(&openSockets);
        FD_SET(listenSock, &openSockets);
        maxfds = listenSock;
    }

    finished = false;
    std::string ip = "130.208.243.61";
    std::string port = argv[2];
    std::cout << "1" << std::endl;
    int sock = connect_socket(ip, port);
    FD_SET(sock, &openSockets);
   maxfds = std::max(maxfds, sock);

    time_t timer;
    time_t delta;

    time(&timer);

    while(!finished)
    {     
        std::string cmd = "";
        std::thread t1([&]() {
            std::cin >> cmd;
            });
        std::this_thread::sleep_for(std::chrono::seconds(5));
        t1.detach();

        delta = time(NULL) - timer;

        if(delta > 70) {
            group = maps[sock]->group;
            x = servers[group]->messages.size();
            custom = "KEEPALIVE,"+std::to_string(x);

            send_message(sock, custom);
            //send(sock, custom.c_str(),custom.length(),0);
            time(&timer);
        }
        
        if (!cmd.empty()){
            
            
            
            send_message(sock, cmd);


        }

        // Get modifiable copy of readSockets
        readSockets = exceptSockets = openSockets;
        clients[sock] = new Client(sock);
        //memset(buffer, 0, sizeof(buffer));

        // Look at sockets and see which ones have something to be read()
        struct timeval tv = {1, 0}; 

        int n = select(maxfds + 1, &readSockets, NULL, &exceptSockets, &tv);
        if(errno == EINTR) {
            std::cout << "timeout" << std::endl;
        }

        else if(n < 0)
        {
            perror("select failed - closing down\n");
            finished = true;
        }
        else
        {
            // First, accept  any new connections to the server on the listening socket
            if(FD_ISSET(listenSock, &readSockets))
            {
               clientSock = accept(listenSock, (struct sockaddr *)&client,
                                   &clientLen);

               //std::string msg = "JOIN,"+GROUP + ","+ IP + ","+ PORT;
               //send(clientSock, msg.c_str(), msg.length(),0);
               // Add new client to the list of open sockets
               FD_SET(clientSock, &openSockets);

               // And update the maximum file descriptor
               maxfds = std::max(maxfds, clientSock) ;

               // create a new client to store information.
               clients[clientSock] = new Client(clientSock);

               // Decrement the number of sockets waiting to be dealt with
               n--;

               printf("Client connected on server: %d\n", clientSock);
            }
            // Now check for commands from clients
            std::list<Client *> disconnectedClients;  
            while(n-- > 0)
            {
               for(auto const& pair : clients)
               {
                  Client *client = pair.second;
                  
                  if(FD_ISSET(client->sock, &readSockets))
                  {
                      // recv() == 0 means client has closed connection
                      if(recv(client->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0)
                      {    
                          //std::cout << buffer << std::endl;
                          disconnectedClients.push_back(client);
                          closeConnection(client->sock, &openSockets, &maxfds);
                          if(maps.find(client->sock) == maps.end()){
                            if (servers.find(maps[client->sock]->group) == servers.end()){
                                servers.erase(maps[client->sock]->group);
                            }
                            maps.erase(client->sock);
                          }
                          memset(buffer, 0, sizeof(buffer));
                      }
                      // We don't check for -1 (nothing received) because select()
                      // only triggers if there is something on the socket for us.
                      else
                      {
                          std::cout << "SERVER: " << client->sock << " sent-> " << buffer << std::endl;
                          Command(client->sock, &openSockets, &maxfds, buffer);
                          memset(buffer, 0, sizeof(buffer));
                      }
                  }
               }
               // Remove client from the clients list
               for(auto const& c : disconnectedClients)
                  clients.erase(c->sock);
            }
        }
    }
}
