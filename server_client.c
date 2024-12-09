//Destiny Boyd, Mahlet Zegeye, Destin Gilbert, Cameron Carter
#include "server.h"

#define DEFAULT_ROOM "Lobby"

// USE THESE LOCKS AND COUNTER TO SYNCHRONIZE
extern int numReaders;
extern pthread_mutex_t rw_lock;
extern pthread_mutex_t mutex;

//struct node *head = NULL;

extern const char *server_MOTD;


/*
 *Main thread for each client.  Receives all messages
 *and passes the data off to the correct function.  Receives
 *a pointer to the file descriptor for the socket the thread
 *should listen on
 */

char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}

void *client_receive(void *ptr) {
  int client = *(int *) ptr;  // socket
  
  int received, i;
  char buffer[MAXBUFF], sbuffer[MAXBUFF];  //data buffer of 2K  
  char tmpbuf[MAXBUFF];  //data temp buffer of 1K  
  char cmd[MAXBUFF], username[20];
  char *arguments[80];
  struct node *current_users; 

  // Creating the guest user name
  sprintf(username,"guest%d", client);
  head = insertFirstU(head, client , username);
  head->room = strdup(DEFAULT_ROOM);

  // Add the guest user to the global `users` array
  pthread_mutex_lock(&mutex);
  users[user_count].socket = client;
  users[user_count].username = strdup(username);
  users[user_count].connected_to = 0;
  user_count++;
  pthread_mutex_unlock(&mutex);

  // Add the GUEST to the DEFAULT ROOM (i.e. Lobby)
  pthread_mutex_lock(&mutex);
  join_room(DEFAULT_ROOM, client);
  pthread_mutex_unlock(&mutex);

  send(client, server_MOTD, strlen(server_MOTD) , 0 ); // Send Welcome Message of the Day.

  while (1) {   
    if ((received = read(client, buffer, MAXBUFF)) != 0) {
      buffer[received] = '\0'; 
      strcpy(cmd, buffer);  
      strcpy(sbuffer, buffer);
        
      /////////////////////////////////////////////////////
      // we got some data from a client

      // 1. Tokenize the input in buf (split it on whitespace)

      // get the first token 

      arguments[0] = strtok(cmd, delimiters);

      // walk through other tokens 

      i = 0;
      while( arguments[i] != NULL ) {
        arguments[++i] = strtok(NULL, delimiters); 
        if (arguments[i-1]){
          strcpy(arguments[i-1], trimwhitespace(arguments[i-1]));
        }
      } 

      // Arg[0] = command
      // Arg[1] = user or room
             
      /////////////////////////////////////////////////////
      // 2. Execute command: TODO
      if(strcmp(arguments[0], "create") == 0)
      {
        printf("create room: %s\n", arguments[1]); 
              
        // perform the operations to create room arg[1]
        
        pthread_mutex_lock(&mutex);
        if (room_exists(arguments[1])) {
          sprintf(buffer, "Room %s already exists. \nchat>", arguments[1]);
        }
        else{
          create_room(arguments[1]);
          sprintf(buffer, "Room %s created. \nchat>", arguments[1]);
        }
        pthread_mutex_unlock(&mutex);
        send(client , buffer , strlen(buffer) , 0 ); // send back to client
      }
      else if (strcmp(arguments[0], "join") == 0)
      {
        printf("join room: %s\n", arguments[1]);  

        // perform the operations to join room arg[1]
        pthread_mutex_lock(&mutex);
        if (room_exists(arguments[1])){
          leave_room(DEFAULT_ROOM, client);
          join_room(arguments[1], client);
          sprintf(buffer, "You have joined room %s. \nchat>", arguments[1]);
        }
        else{
          sprintf(buffer, "Room %s does not exist. \nchat>", arguments[1]);
        }
        pthread_mutex_unlock(&mutex);
        send(client , buffer , strlen(buffer) , 0 ); // send back to client
        }
      else if (strcmp(arguments[0], "leave") == 0)
      {
        printf("leave room: %s\n", arguments[1]); 

        // perform the operations to leave room arg[1]
        pthread_mutex_lock(&mutex);
        if (strcmp(DEFAULT_ROOM, arguments[1]) == 0){
          sprintf(buffer, "You cannot leave the default room '%s'.\nchat>", DEFAULT_ROOM);
        }
        else{
          RoomNode *room = find_room_by_name(arguments[1]);
          if (room){
            leave_room(arguments[1], client);
            join_room(DEFAULT_ROOM, client);
            sprintf(buffer, "Left room %s and joined Lobby. \nchat>", arguments[1]);
          }
          pthread_mutex_unlock(&mutex);
          send(client, buffer, strlen(buffer), 0 ); // send back to client
        }
      }   
      else if (strcmp(arguments[0], "connect") == 0)
      {
        printf("connect to user: %s \n", arguments[1]);

        // perform the operations to connect user with socket = client from arg[1]
        pthread_mutex_lock(&mutex);             
        User *target_user =  find_user_by_name(arguments[1], users, user_count);
        User *current_user = find_user_by_socket(client, users, user_count);

        if (!current_user){
          sprintf(buffer, "Error: Current user not found. \nchat>");
        }
        else if (!target_user){
          sprintf(buffer, "User %s does not exist.\nchat>", arguments[1]);
        }
        else if (target_user->socket == client) {
          sprintf(buffer, "You cannot connect to yourself.\nchat>");
        }
        else if(target_user->connected_to != 0){
          sprintf(buffer, "User %s is already connected to someone else\nchat>", arguments[1]);
        }
        else{
          if (connect_users(current_user, target_user) == 0){
            sprintf(buffer, "Connected to %s. \nchat>", arguments[1]);
                }
          else {
            sprintf(buffer, "Failed to connect to %s.\nchat>", arguments[1]);
          }
        }
               
        pthread_mutex_unlock(&mutex);
        send(client , buffer , strlen(buffer) , 0 ); // send back to client
      }
      else if (strcmp(arguments[0], "disconnect") == 0)
      {             
        printf("disconnect from user: %s\n", arguments[1]);
               
        // perform the operations to disconnect user with socket = client from arg[1]
        pthread_mutex_lock(&mutex);

        User *current_user = find_user_by_socket(client, users, user_count);
        if (!current_user){
          sprintf(buffer, "Error: Curent user not found. \nchat>");
        }
        else if (current_user->connected_to == 0){
                sprintf(buffer, "You are not connected to any user.\nchat>");
        }
        else {
          User *target_user = find_user_by_socket(current_user->connected_to, users, user_count);
          if (target_user){
            disconnect_users(current_user, target_user);
            sprintf(buffer, "Disconnected from user '%s'. \nchat>", target_user->username);
          }
          else {
            current_user->connected_to = 0;
            sprintf(buffer, "Disconnected, but the connected user could not be identified.\nchat>");
          }
        }
        pthread_mutex_unlock(&mutex); 
        send(client, buffer, strlen(buffer), 0 ); // send back to client
      }                  
      else if (strcmp(arguments[0], "rooms") == 0)
      {
        printf("List all the rooms \n");
              
        // must add put list of users into buffer to send to client
        pthread_mutex_lock(&mutex);
        char *room_list = get_room_list();
        if (room_list && strlen(room_list) > 0){
          sprintf(buffer, "Available Rooms:\n%s\nchat>", room_list);
        }
        else{
          sprintf(buffer, "No roomsare currently available. \nchat>");
        }
        pthread_mutex_unlock(&mutex);
        send(client, buffer, strlen(buffer), 0 ); // send back to client
        
        if (room_list){
          free(room_list);
        }                            
                
      }   
      else if (strcmp(arguments[0], "users") == 0)
      {
        printf("List all the users\n");
              
        // must add put list of users into buffer to send to client
        pthread_mutex_lock(&mutex);
        char *user_list = get_users_in_room(DEFAULT_ROOM);
                
        User *current_user = find_user_by_socket(client, users, user_count);
        if (!current_user){
          sprintf(buffer, "User not found. \nchat>");
          pthread_mutex_unlock(&mutex);
          send(client, buffer, strlen(buffer), 0 );
          free(user_list);
        }  

        char *direct_connections = (char *)malloc(512);
        if (!direct_connections){
          perror("Failed to allocate direct connections list");
          pthread_mutex_unlock(&mutex);
          send(client, "Error generating direct connections list. \nchat>", 46, 0);
          free(user_list);
        } 

        direct_connections[0] = '\0';
        if (current_user->connected_to){
          User *connected_user = find_user_by_socket(current_user->connected_to, users, user_count);
          if (connected_user) {
            sprintf(direct_connections, "Directly connected to: %s\n", connected_user->username);
          } 
          else {
            strcpy(direct_connections, "Directly connected to: Unknown\n");
          }
        } 
        else {
          strcpy(direct_connections, "No direct connections.\n");
        }

        sprintf(buffer, "Users in room %s:\n%s\nchat>", DEFAULT_ROOM, user_list);
        pthread_mutex_unlock(&mutex);
        send(client , buffer , strlen(buffer) , 0 ); // send back to client
        free(user_list);
        free(direct_connections); 
      }                           
      else if (strcmp(arguments[0], "login") == 0)
      {
        if (arguments[1] == NULL){
          sprintf(buffer, "login <username>\nchat>");
          send(client, buffer, strlen(buffer), 0 ); // send back to client
        }  
        pthread_mutex_lock(&mutex);
              
        if (find_user_by_name(arguments[1], users, user_count)){
          sprintf(buffer, "Username '%s' is alredy taken. Please choose another \n chat>", arguments[1]);
          pthread_mutex_unlock(&mutex);
          send(client, buffer, strlen(buffer), 0 );
        }
        RoomNode *room = room_list_head;
        while (room) {
          ClientNode *client_node = room->clients;
          while (client_node) {
            if (client_node->socket_fd == client) {
              sprintf(buffer, "User '%s' is now known as '%s'.\nchat>", "guest", arguments[1]);
              break;
            }
            client_node = client_node->next;
          }
          room = room->next;
        }
        //rename their guestID to username. Make sure any room or DMs have the updated username. 
        rename_user(client, arguments[1], users, user_count);       
        sprintf(buffer, "You are now logged in as '%s' . \nchat>", arguments[1]);
        pthread_mutex_unlock(&mutex);
        send(client, buffer, strlen(buffer), 0 ); // send back to client
      } 
      else if (strcmp(arguments[0], "help") == 0 )
      {
        sprintf(buffer, "login <username> - \"login with username\" \ncreate <room> - \"create a room\" \njoin <room> - \"join a room\" \nleave <room> - \"leave a room\" \nusers - \"list all users\" \nrooms -  \"list all rooms\" \nconnect <user> - \"connect to user\" \nexit - \"exit chat\" \n");
         send(client , buffer , strlen(buffer) , 0 ); // send back to client 
      }
      else if (strcmp(arguments[0], "exit") == 0 || strcmp(arguments[0], "logout") == 0)
      {
        printf("User %d is exiting.\n", client);
        pthread_mutex_lock(&mutex);
        User *current_user = find_user_by_socket(client, users, user_count);
        if(current_user){
          RoomNode *room = room_list_head;
          while(room) {
            leave_room(room->room_name, client);
            room = room->next;
          }

          if(current_user->connected_to != 0){
            User *target_user = find_user_by_socket(current_user->connected_to, users, user_count);
            if (target_user) {
              target_user->connected_to = 0;
            }
            current_user->connected_to = 0;;
          }

          for (int i = 0; i< user_count; i++){
            if (users[i].socket == client) {
              free(users[i].username);
              users[i] = users[user_count - 1];
              user_count--;
              break;
            }
          }
        }
        
        pthread_mutex_unlock(&mutex);
        //Remove the initiating user from all rooms and direct connections, then close the socket descriptor.
        close(client);
        printf("User %d has exited.\n", client);
      }                         
      else { 
        /////////////////////////////////////////////////////////////
        // 3. sending a message
           
        // send a message in the following format followed by the promt chat> to the appropriate receipients based on rooms, DMs
        // ::[userfrom]> <message>
        User *current_user = find_user_by_socket(client, users, user_count);
        const char *username = current_user ? current_user->username : "UnknownUser";
        sprintf(tmpbuf,"\n::%s> %s\nchat>", username, sbuffer);
        strcpy(sbuffer, tmpbuf);

        if (current_user->connected_to != 0) {
          // Send DM to directly connected user
          User *target_user = find_user_by_socket(current_user->connected_to, users, user_count);
          if (target_user) {
            send(target_user->socket, tmpbuf, strlen(tmpbuf), 0);
          } else {
            sprintf(buffer, "Error: Could not find the connected user.\nchat>");
            send(client, buffer, strlen(buffer), 0);
          }
        } else {     
          RoomNode *room = find_room_by_name(DEFAULT_ROOM);
          if (room) {
            ClientNode *client_node = room->clients;
            while (client_node) {
            if (client_node->socket_fd != client){
              send(client_node->socket_fd, tmpbuf, strlen(tmpbuf), 0);
            } 
            client_node = client_node->next;
            }
          }
          else {
            sprintf(buffer, "Error: Room not found.\nchat>");
            send(client, buffer, strlen(buffer), 0);
          }
        }   
      }
      pthread_mutex_unlock(&mutex);  
    }
    memset(buffer, 0, MAXBUFF);
  }
  return NULL;
} 