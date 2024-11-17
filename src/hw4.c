#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdbool.h>

#define PORT1 2201
#define PORT2 2202
#define BUFFER_SIZE 1024


typedef struct Player {
    int** board;
    int pieces[5][4];
    int** shot_history;
    int num_shots;

} Player;

//make sure char* msg is always a string literal, or make sure to add the null-terminator
void send_message(int fd, char* msg) {
    send(fd, msg, strlen(msg)+1, 0);
}

void initializeBoard(Player *player, int width, int height);

int main()
{
    // Server Setup --------------------------------------------------------------------------->
    int listen_fd1, conn_fd1, listen_fd2, conn_fd2;
    struct sockaddr_in address1, address2;
    int addrlen1 = sizeof(address1);
    int addrlen2 = sizeof(address2);
    char buffer[BUFFER_SIZE] = {0};
    int opt1 = 0; int opt2 = 0;

    // Create socket
    if ((listen_fd1 = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket 1 failed");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(listen_fd1, SOL_SOCKET, SO_REUSEADDR, &opt1, sizeof(opt1))) {
        perror("setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))");
        exit(EXIT_FAILURE);
    }

    address1.sin_family = AF_INET;
    address1.sin_addr.s_addr = INADDR_ANY;
    address1.sin_port = htons(PORT1);

    if (bind(listen_fd1, (struct sockaddr *)&address1, sizeof(address1)) < 0)
    {
        perror("[Server] bind() failed for port 2201.");
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd1, 1) < 0)
    {
        perror("[Server] listen() failed for port 2201.");
        exit(EXIT_FAILURE);
    }
    printf("[Server] Running on port %d\n", PORT1);

    if ((listen_fd2 = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket 2 failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(listen_fd2, SOL_SOCKET, SO_REUSEADDR, &opt2, sizeof(opt2))) {
        perror("setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))");
        exit(EXIT_FAILURE);
    }

    address2.sin_family = AF_INET;
    address2.sin_addr.s_addr = INADDR_ANY;
    address2.sin_port = htons(PORT2);

    if (bind(listen_fd2, (struct sockaddr *)&address2, sizeof(address2)) < 0)
    {
        perror("[Server] bind() failed for port 2202.");
        exit(EXIT_FAILURE);
    }
    
    if (listen(listen_fd2, 1) < 0)
    {
        perror("[Server] listen() failed for port 2202.");
        exit(EXIT_FAILURE);
    }
    printf("[Server] Running on port %d\n", PORT2);


    if ((conn_fd1 = accept(listen_fd1, (struct sockaddr *)&address1, &addrlen1)) < 0)
    {
        perror("[Server] accept() failed for port 2201.");
        exit(EXIT_FAILURE);
    }

    if ((conn_fd2 = accept(listen_fd2, (struct sockaddr *)&address2, &addrlen2)) < 0)
    {
        perror("[Server] accept() failed for port 2202.");
        exit(EXIT_FAILURE);
    }
    

    bool begin = false;
    bool initialize = false;
    int turn = 1;
    Player p1, p2;
    int width, height;
    while (1)
    {
        // printf("in while loop");
        memset(buffer, 0, BUFFER_SIZE);
        //checks if game has begun yet or not, if not, then only check for "B" packets
        //This is all code for the BEGIN packet
        if(!begin) {
            switch(turn) {
                //check for player 1's begin packet
                case 1:
                    read(conn_fd1, buffer, BUFFER_SIZE);
                    
                    if(buffer[0] != 'B') send_message(conn_fd1, "ERR 100"); //not a B request
                    else if(sscanf(buffer, "B %d %d", &width, &height) != 2) {
                        width = 0; height = 0;
                        send_message(conn_fd1, "ERR 200"); //not valid parameters of B request
                    } 
                    else {
                        if(width < 10 || height < 10) {
                            width = 0; height = 0; //just in case, height and width are set back to 0
                            send_message(conn_fd1, "ERR 200"); //parameters are invalid size (at least 10x10)
                        } 
                        else {
                            turn = 2;
                            send_message(conn_fd1, "A"); //if everything's good, then height and width are set correct
                        }
                    }
                    break;
                //check for player 2's begin packet
                default:
                    read(conn_fd2, buffer, BUFFER_SIZE);
                    if(strcmp(buffer, "B")) send_message(conn_fd2, "ERR 100");
                    else {
                        initializeBoard(&p1, width, height);
                        initializeBoard(&p2, width, height);
                        begin = true;
                        turn = 1;
                        send_message(conn_fd2, "A");
                    }
            }
        }
        
        
    }
    printf("[Server] Shutting down.\n");

    close(conn_fd1);
    close(listen_fd1);
    close(conn_fd2);
    close(listen_fd2);
    return EXIT_SUCCESS;
}

void initializeBoard(Player *player, int width, int height) {
    player -> board = malloc(height * 8);
    for(int i = 0; i < height; i++) {
        player->board[i] = calloc(width, 4);
    }
}


