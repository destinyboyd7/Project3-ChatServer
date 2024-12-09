/* System Header Files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <netdb.h>
#include <ctype.h>
#include <pthread.h>

/* Local Header Files */
#include "list.h"

#define MAX_READERS 25
#define TRUE   1  
#define FALSE  0  
#define PORT 8888  
#define delimiters " "
#define max_clients  30
#define DEFAULT_ROOM "Lobby"
#define MAXBUFF   2096
#define BACKLOG 2 


// prototypes
typedef struct client_node {
  int socket_fd;
  struct client_node *next;
}ClientNode;

typedef struct chat_room {
  char *room_name;
  ClientNode *clients;
  struct chat_room *next;
}RoomNode;

typedef struct user{
  int socket;         
  char *username;      
  int connected_to;   
} User;


//Global Variables 
extern struct node *head;
extern pthread_mutex_t mutex;
extern RoomNode *room_list_head;
extern RoomNode *default_room;
extern const char *server_MOTD;
extern User users[max_clients];
extern int user_count; 

//function prototypes
int get_server_socket(char *hostname, char *port);
int start_server(int serv_socket, int backlog);
int accept_client(int serv_sock);
void sigintHandler(int sig_num);
void *client_receive(void *ptr);
int room_exists(const char *room_name);
void create_room(const char *room_name);
void join_room(const char *room_name, int client);
void leave_room(const char *room_name, int client);
char *get_room_list();
char *get_users_in_room(const char *room_name);
User *find_user_by_name(const char *username, User *users, int user_count);
User *find_user_by_socket(int socket, User *users, int user_count);
void log_message(const char *message);
void send_response(int, uint, const char *message);
void intial_default_room();
RoomNode *find_room_by_name(const char *room_name);
int connect_users(User *current_user, User *target_user);
void disconnect_users(User *current_user, User *target_user);
void rename_user(int socket, const char *new_username, User *users, int user_count);
void start_read();
void end_read();
void start_write();
void end_write();