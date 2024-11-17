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

void initializePlayer(Player *player, int width, int height);
int processInitialize(Player * player, char *buffer);
int placeShips(Player *player);
int placeShip(Player *player, int piece_type, int piece_rotation, int column, int row);
void clearShips(Player *player);
int checkBounds(Player *player, int piece_type, int piece_rotation, int column, int row);

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
                        initializePlayer(&p1, width, height);
                        initializePlayer(&p2, width, height);
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
                    if(buffer[0] != 'I') send_message(conn_fd1, "ERR 101"); //not an I request
                    else {
                        int res = processInitialize(&p1, buffer);
                        if(res) {
                            char temp[20];
                            sprintf(temp, "ERR %d", res);
                            send_message(conn_fd1, temp);
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
void initializePlayer(Player *player, int width, int height) {
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
        if(value != 4) return 201;
        if(piece_type <= 0 || piece_type > 7) return 300;
        else if(piece_rotation <= 0 || piece_rotation > 4) {
            if(cur_error > 301) cur_error = 301;
        } 
        else if(column < 0 || column >= player -> width) {
            if(cur_error > 302) cur_error = 302; 
        }
        else if(row < 0 || row >= player -> height) {
            if(cur_error > 302) cur_error = 302;
        }
        else {
            if(checkBounds(player, piece_type, piece_rotation, column, row)) {
                if(cur_error > 302) cur_error = 302;
            }
        }
        for(int i = 0; i < 4; i++) {
            while(*p != ' ') p++;
            p++;
        }
    }
    if(cur_error != 999) return cur_error;
    
    
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

int checkBounds(Player *player, int piece_type, int piece_rotation, int column, int row) {
    int width = player->width; int height = player->height;
    switch(piece_type) {
        case 1:
            for(int r = 0; r < 2; r++) {
                if(r+row >= height) return 302;  
                if(r+column >= width) return 302;
            }
            break;
        case 2:
            if(piece_rotation == 1 || piece_rotation == 3) {
                if(column >= width) return 302;
                for(int r = 0; r < 4; r++) {
                    if(r+row >= height) return 302;
                }
            }
            else {
                if(row >= height) return 302;
                for(int c = 0; c < 4; c++) {
                    if(c+column >= width) return 302;
                }
            }
            break;
        case 3:
            if(piece_rotation == 1 || piece_rotation == 3) {
                for(int r = 0; r < 2; r++) {
                    if(r+row-1 >= height) return 302;
                }
                for(int c = 0; c < 3; c++) {
                    if(c+column >= width) return 302;
                }
            }
            else {
                for(int r = 0; r < 3; r++) {
                    if(r+row >= height) return 302;
                }
                for(int c = 0; c < 2; c++) {
                    if(c+column >= width) return 302;
                }
            }
            break;
        //shape 4
        case 4:
            if(piece_rotation == 1 || piece_rotation == 3) {
                for(int r = 0; r < 3; r++) {
                    if(r+row >= height) return 302;
                }
                for(int c = 0; c < 2; c++) {
                    if(c+column >= width) return 302;
                }
            }
            if(piece_rotation == 2) {
                for(int r = 0; r < 2; r++) {
                    if(r+row >= height) return 302;
                }
                for(int c = 0; c < 3; c++) {
                    if(c+column >= width) return 302;
                }
            }
            else {
                for(int r = 0; r < 2; r++) {
                    if(r+row-1 >= height) return 302;
                }
                for(int c = 0; c < 3; c++) {
                    if(c+column >= width) return 302;
                }
            }   
            break;
        case 5:
            if(piece_rotation == 1 || piece_rotation == 3) {
                for(int r = 0; r < 2; r++) {
                    if(r+row >= height) return 302;
                }
                for(int c = 0; c < 3; c++) {
                    if(c+column >= width) return 302;
                }
            }
            else {
                for(int r = 0; r < 3; r++) {
                    if(r+row-1 >= height) return 302;
                }
                for(int c = 0; c < 2; c++) {
                    if(c+column >= width) return 302;
                }
            }
            break;
        case 6:
            if(piece_rotation == 2 || piece_rotation == 4) {
                for(int r = 0; r < 2; r++) {
                    if(r+row >= height) return 302;
                }
                for(int c = 0; c < 3; c++) {
                    if(c+column >= width) return 302;
                }
            }
            else if(piece_rotation == 1) {
                for(int r = 0; r < 3; r++) {
                    if(r+row-2 >= height) return 302;
                }
                for(int c = 0; c < 2; c++) {
                    if(c+column >= width) return 302;
                }
            }
            else {
                for(int r = 0; r < 3; r++) {
                    if(r+row >= height) return 302;
                }
                for(int c = 0; c < 2; c++) {
                    if(c+column >= width) return 302;
                }
            }
            break;
        default:
            if(piece_rotation == 1 || piece_rotation == 3) {
                for(int c = 0; c < 3; c++) {
                    if(c+column >= width) return 302;
                }
                
                if(piece_rotation == 1) {
                    for(int r = 0; r < 2; r++) {
                        if(r+row >= height) return 302;
                    }
                }
                else {
                    for(int r = 0; r < 2; r++) {
                        if(r+row-1 >= height) return 302;
                    }
                }
            }
            //rotation 2 or 4
            else {
                for(int c = 0; c < 2; c++) {
                    if(c+column >= width) return 302;
                }
                if(piece_rotation == 2) {
                    for(int r = 0; r < 3; r++) {
                        if(r+row-1 >= height) return 302;
                    }
                }
                else {
                    for(int r = 0; r < 3; r++) {
                        if(r+row >= height) return 302;
                    }
                }
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
int placeShip(Player *player, int piece_type, int piece_rotation, int column, int row) {
    switch(piece_type) {
        case 1:
            for(int r = 0; r < 2; r++) {
                for(int c = 0; c < 2; c++) {
                    if(player->board[r+row][c+column] == 1) {
                        clearShips(player);
                        return 0;
                    }
                    player->board[r+row][c+column] = 1;
                }
            }
            break;
        case 2:
            if(piece_rotation == 1 || piece_rotation == 3) {
                for(int r = 0; r < 4; r++) {
                    if(player->board[r+row][column] == 1) {
                        clearShips(player);
                        return 0;
                    }
                    player->board[r+row][column] = 1;
                }
            }
            else {
                for(int c = 0; c < 4; c++) {
                    if(player->board[row][c+column] == 1) {
                        clearShips(player);
                        return 0;
                    }
                    player->board[row][c+column] = 1;
                }
            }
            break;
        case 3:
            if(piece_rotation == 1 || piece_rotation == 3) {
                for(int r = 0; r < 2; r++) {
                    if(r == 0) {
                        for(int c = 1; c < 3; c++) {
                            if(player->board[r + row-1][c + column] == 1) {
                                clearShips(player);
                                return 0;
                            }
                            player->board[r+row-1][c+column] = 1;
                        }
                    }
                    else {
                        for(int c = 0; c < 2; c++) {
                            if(player->board[r + row-1][c + column] == 1) {
                                clearShips(player);
                                return 0;
                            }
                            player->board[r+row-1][c+column] = 1;
                        }
                    }
                    
                }
            }
            //rotation 2 and 4
            else {
                for(int c = 0; c < 2; c++) {
                    if(c == 0) {
                        for(int r = 0; r < 2; r++) {
                            if(player->board[r + row][c + column] == 1) {
                                clearShips(player);
                                return 0;
                            }
                            player->board[r+row][c+column] = 1;
                        }
                    }
                    else {
                        for(int r = 1; r < 3; r++) {
                            if(player->board[r + row][c + column] == 1) {
                                clearShips(player);
                                return 0;
                            }
                            player->board[r+row][c+column] = 1;
                        }
                    }
                }
               
            }
            break;
        //shape 4
        case 4:
            if(piece_rotation == 1) {
                for(int c = 0; c < 2; c++) {
                    if(c == 0) {
                        for(int r = 0; r < 3; r++) {
                            if(player->board[r + row][c + column] == 1) {
                                clearShips(player);
                                return 0;
                            }
                            player->board[r+row][c+column] = 1;
                        }
                    }
                    else {
                        if(player->board[2+row][c+column] == 1) {
                            clearShips(player);
                            return 0;
                        }
                        player->board[2+row][c+column] = 1;
                    }
                }
               
            }
            else if(piece_rotation == 2) {
                for(int r = 0; r < 2; r++) {
                    if(r == 0) {
                        for(int c = 0; c < 3; c++) {
                            if(player->board[r + row][c + column] == 1) {
                                clearShips(player);
                                return 0;
                            }
                            player->board[r+row][c+column] = 1;
                        }
                    }
                    else {
                        if(player->board[r+row][column] == 1) {
                            clearShips(player);
                            return 0;
                        }
                        player->board[r+row][column] = 1;
                    }
                }
            }
            else if(piece_rotation == 3) {
                 for(int c = 0; c < 2; c++) {
                    if(c == 1) {
                        for(int r = 0; r < 3; r++) {
                            if(player->board[r + row][c + column] == 1) {
                                clearShips(player);
                                return 0;
                            }
                            player->board[r+row][c+column] = 1;
                        }
                    }
                    else {
                        if(player->board[row][column] == 1) {
                            clearShips(player);
                            return 0;
                        }
                        player->board[row][column] = 1;
                    }
                }
            }
            else {
                 for(int r = 0; r < 2; r++) {
                    if(r == 1) {
                        for(int c = 0; c < 3; c++) {
                            if(player->board[r + row - 1][c + column] == 1) {
                                clearShips(player);
                                return 0;
                            }
                            player->board[r+row-1][c+column] = 1;
                        }
                    }
                    else {
                        if(player->board[row-1][2+column] == 1) {
                            clearShips(player);
                            return 0;
                        }
                        player->board[row-1][2+column] = 1;
                    }
                }
            }   
            break;
        case 5:
            if(piece_rotation == 1 || piece_rotation == 3) {
                for(int r = 0; r < 2; r++) {
                    if(r == 0) {
                        for(int c = 0; c < 2; c++) {
                            if(player->board[r+row][c+column] == 1) {
                                clearShips(player);
                                return 0;
                            }
                            player->board[r+row][c+column] = 1;
                        } 
                    }
                    else {
                        for(int c = 1; c < 3; c++) {
                            if(player->board[r+row][c+column] == 1) {
                                clearShips(player);
                                return 0;
                            }
                            player->board[r+row][c+column] = 1;
                        }
                    }
                }
            }
            //rotations 2 and 4
            else {
                for(int c = 0; c < 2; c++) {
                    if(c == 0) {
                        for(int r = 0; r < 2; r++) {
                            if(player->board[r+row][c+column] == 1) {
                                clearShips(player);
                                return 0;
                            }
                            player->board[r+row][c+column] = 1;
                        }
                    }
                    else {
                        for(int r = 0; r< 2; r++) {
                            if(player->board[r+row-1][c+column] == 1) {
                                clearShips(player);
                                return 0;
                            }
                            player->board[r+row-1][c+column] = 1;
                        }
                    }
                }
            }
            break;
        case 6:
            switch(piece_rotation) {
                case 1:
                    break;
                
                case 2:
                    break;
                
                case 3:
                    break;
                
                default:
            }
            break;
        default:
            if(piece_rotation == 1 || piece_rotation == 3) {
                for(int c = 0; c < 3; c++) {
                    if(c+column >= width) return 302;
                }
                
                if(piece_rotation == 1) {
                    for(int r = 0; r < 2; r++) {
                        if(r+row >= height) return 302;
                    }
                }
                else {
                    for(int r = 0; r < 2; r++) {
                        if(r+row-1 >= height) return 302;
                    }
                }
            }
            //rotation 2 or 4
            else {
                for(int c = 0; c < 2; c++) {
                    if(c+column >= width) return 302;
                }
                if(piece_rotation == 2) {
                    for(int r = 0; r < 3; r++) {
                        if(r+row-1 >= height) return 302;
                    }
                }
                else {
                    for(int r = 0; r < 3; r++) {
                        if(r+row >= height) return 302;
                    }
                }
            }
    }
    return 1;
}


void clearShips(Player *player) {
    for(int r = 0; r < 5; r++) {
        for(int c = 0; c < 4; c++) {
            player->pieces[r][c] = 0;
        }
    }
}