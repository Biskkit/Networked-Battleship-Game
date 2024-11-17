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
    int width;
    int height;
    int pieces[5][4];
    int** shot_history;
    int num_shots;

} Player;

//make sure char* msg is always a string literal, or make sure to add the null-terminator
void send_message(int fd, char* msg) {
    send(fd, msg, strlen(msg)+1, 0);
}

void initializeBoard(Player *player, int width, int height);
int processInitialize(Player * player, char *buffer);
int placeShips(Player *player);
void clearShips(Player *player);

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
        
        //Code for INITIALIZE packet
        else if(!initialize) {
            switch(turn) {
                case 1:
                    read(conn_fd1, buffer, BUFFER_SIZE);
                    if(buffer[0] != 'I') send_message(conn_fd1, "ERR 101"); //not a I request
                    else {
                        int res = initializePacket(&p1, buffer);
                        if(res) {
                            char temp[1];
                            sprintf(temp, "%d", res);
                            char e[] = "ERR ";
                            send_message(conn_fd1, strcat(e, temp));
                            free(temp);
                        }
                    }
                    break;
                default:
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


//ALSO initializes player fields
void initializeBoard(Player *player, int width, int height) {
    player -> board = malloc(height * 8);
    player -> height = height;
    player -> width = width;
    clearShips(player);
    for(int i = 0; i < height; i++) {
        player->board[i] = calloc(width, 4);
    }
}


int processInitialize(Player *player, char *buffer) {
    
    
    char *p = (buffer + 2);
    int piece_type, piece_rotation, column, row;
    //check if there's more than 20 values
    int i = 0;
    while(*p != '\0') {
        i++;
        while(*p != ' ' && *p != '\0') {p++;}
        if(*p != '\0') p++;
    }

    if(i != 20) return 201;

    p = (buffer + 2);
    int cur_error = 999;
    for(int i = 0; i < 5; i++) {
        int value = sscanf(p, "%d %d %d %d", &piece_type, &piece_rotation, &column, &row);
        if(piece_type <= 0 || piece_type > 7) cur_error = 300;
        if(piece_rotation <= 0 || piece_rotation > 4) {
            if(cur_error > 301) cur_error = 301;
        } 
        if(column < 0 || column >= player -> width) {
            if(cur_error > 302) cur_error = 302; 
        }
        if(row < 0 || row >= player -> height) {
            if(cur_error > 302) cur_error = 302;
        } 
        if(cur_error != 999) return cur_error;

        for(int i = 0; i < 4; i++) {
            while(*p != ' ') p++;
            p++;
        }
    }
    
    p = (buffer + 2);

    for(int i = 0; i < 5; i++) {
        int value = sscanf(p, "%d %d %d %d", &piece_type, &piece_rotation, &column, &row);
        
        player -> pieces[i][0] = piece_type;
        player -> pieces[i][1] = piece_rotation;
        player -> pieces[i][2] = column;
        player -> pieces[i][3] = row;

        for(int i = 0; i < 4; i++) {
            while(*p != ' ') p++;
            p++;
        }
    }
    return 0;
}



int placeShips(Player *player) {
    int piece_type, piece_rotation, column, row;
    for(int r = 0; r < 5; r++) {
        piece_type = player->pieces[r][0];
        piece_rotation = player->pieces[r][1];
        column = player->pieces[r][2];
        row = player->pieces[r][3];
        
    }
}
void clearShips(Player *player) {
    for(int r = 0; r < 5; r++) {
        for(int c = 0; c < 4; c++) {
            player->pieces[r][c] = 0;
        }
    }
}