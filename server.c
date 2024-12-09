#include "server.h"

int chat_serv_sock_fd; //server socket

/////////////////////////////////////////////
// USE THESE LOCKS AND COUNTER TO SYNCHRONIZE

int numReaders = 0; // keep count of the number of readers

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  // mutex lock
pthread_mutex_t rw_lock = PTHREAD_MUTEX_INITIALIZER;  // read/write lock

/////////////////////////////////////////////

RoomNode *room_list_head = NULL;
RoomNode *default_room = NULL;
struct node *head = NULL;
User users[max_clients];
int user_count = 0;

char const *server_MOTD = "Thanks for connecting to the BisonChat Server.\n\nchat>";

int chat_serv_sock_fd;

//default_room
void intial_default_room(){
  default_room = malloc(sizeof(RoomNode));
  if (!default_room){
    perror("failed default room");
    exit(EXIT_FAILURE);
  }

  default_room-> room_name = strdup(DEFAULT_ROOM);
  default_room->clients = NULL;
  default_room->next = NULL;
  printf("Default room '%s' created successfully.\n", default_room->room_name);
}

struct node *insertFirstU(struct node *head, int socket, char *username) {
    struct node *new_node = malloc(sizeof(struct node));
    if (!new_node) {
        perror("Failed to allocate memory for new user node");
        return head;
    }
    new_node->socket = socket;
    strncpy(new_node->username, username, sizeof(new_node->username) - 1);
    new_node->username[sizeof(new_node->username) - 1] = '\0'; // Ensure null termination
    new_node->room = NULL;  // Assuming users have a room field
    new_node->next = head;
    return new_node;
}

int get_server_socket(char *hostname, char *port) {
    int opt = TRUE;   
    int master_socket;
    struct sockaddr_in address; 
    
    //create a master socket  
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)   
    {   
      perror("socket failed");   
      exit(EXIT_FAILURE);   
    }   
    //set master socket to allow multiple connections ,  
    //this is just a good habit, it will work without this  
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,sizeof(opt)) < 0 )   
    {   
      perror("setsockopt");   
      exit(EXIT_FAILURE);   
    }   
     
    //type of socket created  
    address.sin_family = AF_INET;   
    address.sin_addr.s_addr = INADDR_ANY;   
    address.sin_port = htons(PORT);   
         
    //bind the socket to localhost port 8888  
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)   
    {   
      perror("bind failed");   
      exit(EXIT_FAILURE);   
    }   

   return master_socket;
}

int start_server(int serv_socket, int backlog) {
  int status = 0;
  if ((status = listen(serv_socket, backlog)) == -1) {
    printf("socket listen error\n");
    return -1;
  }
  return status;
}

int accept_client(int serv_sock) {
  int reply_sock_fd = -1;
  socklen_t sin_size = sizeof(struct sockaddr_storage);
  struct sockaddr_storage client_addr;
   //char client_printable_addr[INET6_ADDRSTRLEN];

   // accept a connection request from a client
   // the returned file descriptor from accept will be used
   // to communicate with this client.
   if ((reply_sock_fd = accept(serv_sock,(struct sockaddr *)&client_addr, &sin_size)) == -1) {
      printf("socket accept error\n");
      return -1;
   }
   return reply_sock_fd;
}

RoomNode *find_room_by_name(const char *room_name){
  RoomNode *current = room_list_head;
  while (current) {
    if (strcmp(current->room_name, room_name) == 0) {
      return current;
    }
    current = current->next;;
  }
  return NULL;
}

int room_exists(const char *room_name){
  return find_room_by_name(room_name) != NULL;
}

void create_room(const char *room_name) {
  start_write();
  RoomNode *new_room = malloc(sizeof(RoomNode));
  new_room->room_name = strdup(room_name);
  new_room->clients = NULL;
  new_room->next = room_list_head;
  room_list_head = new_room;
  end_write();
}

void join_room(const char *room_name, int client){
  start_write();
  RoomNode *room = find_room_by_name(room_name);
  if(!room){
    end_write();
    return;
  }
  ClientNode *new_client = malloc(sizeof(ClientNode));
  if (!new_client) {
    perror("Failed to add user to room");
    return;
  }
  new_client->socket_fd = client;
  new_client->next = room->clients;
  room->clients = new_client;
  end_write();
}

void leave_room(const char *room_name, int client){
  RoomNode *room = find_room_by_name(room_name);
  if(!room) return;

  ClientNode **indirect = &room->clients;
  while (*indirect && (*indirect)->socket_fd != client){
    indirect = &(*indirect)->next;
  }
  if (indirect){
    ClientNode *to_free = *indirect;
    *indirect = to_free->next;
    free(to_free);
  }
}

char* get_room_list() {
  start_read();
  char* list = (char *)malloc(1024);
  if(!list) {
    perror("Failed to allocate memory for room list.");
    end_read();
    return strdup("Error generating room list. ");
  }
  list[0] = '\0';
  RoomNode *room = room_list_head;
  while (room) {
    strcat(list, room->room_name);
    strcat(list, "\n");
    room = room->next;
  }
  end_read();
  return list;
}

char* get_users_in_room(const char *room_name){
  start_read();
  RoomNode *room = find_room_by_name(room_name);
  if (!room) {
    end_read();
    return NULL;
  }
  
  char *list = (char *)malloc(1024);
  if (!list) {
    perror("Failed to allocate memory for user list.");
    end_read();
    return strdup("Error generating user list.");
  }
  list[0] = '\0';
  ClientNode *client = room->clients; 

  while(client) {
    char temp[50];
    sprintf(temp, "Client %d\n", client->socket_fd);
    strcat(list, temp);
    client = client->next;
  }
  end_read();
  return list;
}

User* find_user_by_name(const char *username, User *users, int user_count) {
  for (int i = 0; i < user_count; i++) {
    if (strcmp(users[i].username, username) == 0){
      return &users[i];
    }
  }
  return NULL;
}

User *find_user_by_socket(int socket, User *users, int user_count){
  for (int i = 0; i < user_count; i++){
    if (users[i]. socket == socket) {
      return &users[i];
    }
  }
  return NULL;
}

void rename_user(int socket, const char *new_username, User *users, int user_count){
  User *user = find_user_by_socket(socket, users, user_count);
  if(user){
    free(user->username);
    user->username = strdup(new_username);
  }
}

int connect_users(User *current_user, User *target_user){
  start_write();
  if(current_user->connected_to != 0 || target_user->connected_to != 0){
    end_write();
    return -1;
  }
  current_user->connected_to = target_user->socket;
  target_user->connected_to = current_user->socket;
  end_write();
  return 0;
}

void disconnect_users(User *current_user, User *target_user){
  if(current_user->connected_to != 0){
    User *target_user = find_user_by_socket(current_user->connected_to, users, user_count);
    if (target_user){
      target_user->connected_to = 0;
    }
    current_user->connected_to = 0;
  }
}

void start_read(){
  pthread_mutex_lock(&rw_lock);
  numReaders++;
  if (numReaders == 1){
    pthread_mutex_lock(&mutex);
  }
  pthread_mutex_unlock(&rw_lock);
}

void end_read(){
  pthread_mutex_lock(&rw_lock);
  numReaders--;
  if(numReaders == 0){
    pthread_mutex_unlock(&mutex);
  }
  pthread_mutex_unlock(&rw_lock);
}

void start_write(){
  pthread_mutex_lock(&mutex);
}

void end_write(){
  pthread_mutex_unlock(&mutex);
}

int main(int argc, char **argv) {

  signal(SIGINT, sigintHandler);

  char *hostname = "127.0.0.1";
 char *port = "8888";
  //////////////////////////////////////////////////////
  // create the default room for all clients to join when 
  // initially connecting
  //  
  //    TODO
  //////////////////////////////////////////////////////
  intial_default_room();


  // Open server socket
  chat_serv_sock_fd = get_server_socket(hostname,port);

  // step 3: get ready to accept connections
  if(start_server(chat_serv_sock_fd, BACKLOG) == -1) {
    printf("start server error\n");
    close(chat_serv_sock_fd);
    exit(1);
  }
   
  printf("Server Launched! Listening on PORT: %d\n", PORT);
    
  //Main execution loop
  while(1) {
    //Accept a connection, start a thread
    int new_client = accept_client(chat_serv_sock_fd);
    if(new_client != -1) {
      perror("Error accepting client connection");
      continue;
    }
    pthread_t client_thread;
    if (pthread_create(&client_thread, NULL, client_receive, (void *)&new_client) != 0){
      perror("Error accepting client connection");
      close(new_client);
      continue;
    }
    pthread_detach(client_thread);
  }
  close(chat_serv_sock_fd);
  return 0;
}



/* Handle SIGINT (CTRL+C) */
void sigintHandler(int sig_num) {
   printf("\nServer shutting down in 5 seconds...\n");
   
   //////////////////////////////////////////////////////////
   //Closing client sockets and freeing memory from user list
   //
   //       TODO
   //////////////////////////////////////////////////////////

   const char *shutdown_message = "Server shutting down in 5 seconds...\n";
   pthread_mutex_lock(&mutex);

   //struct client_node *current_client = default_room->clients;
   ClientNode *current_client = default_room->clients;
   while (current_client){
      if (send(current_client->socket_fd, shutdown_message, strlen(shutdown_message), 0) == -1){
        perror("Error sending shutdown message");
      }
      current_client = current_client->next;
   }
   

   sleep(5);

   printf("--------CLOSING ACTIVE USERS--------\n");
   current_client = default_room->clients;
   while (current_client) {
    //struct client_node *temp = current_client;
    ClientNode *temp_client = current_client;
    close(temp_client->socket_fd);
    current_client = current_client->next;
    free(temp_client);
   }
   default_room->clients = NULL;
      //struct room_node *current_room = room_list_head;
   RoomNode *current_room = room_list_head;
   while(current_room){
    //struct client_node *temp_client = temp_client;
    ClientNode *room_client = current_room->clients;
    while(room_client){
      ClientNode *temp_client = room_client;
      close(temp_client->socket_fd);
      room_client = room_client->next;
      free(temp_client);

    }

    RoomNode *temp_room = current_room;
    current_room = current_room->next;
    free(temp_room->room_name);
    free(temp_room);
   }

   pthread_mutex_unlock(&mutex);
   

   if (close(chat_serv_sock_fd) == -1) {
    perror("Error closing server socket");
   }

   printf("Server shutdown complete. Goodbye!\n");
   close(chat_serv_sock_fd);

   exit(0);
}


