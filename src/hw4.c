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

typedef struct Shot {
    char result;
    int row;
    int col;
}Shot;

typedef struct Player {
    int** board; //this is mostly used to see if ships overlap
    int width;
    int height;
    int pieces[5][4]; //stores initial information about pieces being placed
    int ships_remaining;
    Shot *shot_history;
    int num_shots;

} Player;

//make sure char* msg is always a string literal, or make sure to add the null-terminator
// void send_message(int fd, char* msg) {
//     send(fd, msg, strlen(msg)+1, 0);
// }

void initializePlayer(Player *player, int width, int height);
void processQuery(Player *player, char *str);
int processShoot(Player *pShotting, Player *pOther, char *buffer);
int processInitialize(Player * player, char *buffer);
int placeShips(Player *player);
int placeShip(Player *player, int piece_type, int piece_rotation, int column, int row, int cur_ship);
void clearPieces(Player *player);
void clearBoard(Player *player);
int checkBounds(Player *player, int piece_type, int piece_rotation, int column, int row);\
int numOfShips(Player *pOther);
int alreadyShot(Player *pShooting, int row, int col);
void freePlayer(Player *player);

int main()
{
    // Server Setup --------------------------------------------------------------------------->
    int listen_fd1, conn_fd1, listen_fd2, conn_fd2;
    struct sockaddr_in address1, address2;
    int addrlen1 = sizeof(address1);
    int addrlen2 = sizeof(address2);
    char buffer[BUFFER_SIZE] = {0};
    int opt1 = 1; int opt2 = 1;

    // Create socket
    if ((listen_fd1 = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
    {
        // printEror("socket 1 failed");
        exit(EXIT_FAILURE);
    }
    if ((listen_fd2 = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
    {
        // pEor("socket 2 failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(listen_fd1, SOL_SOCKET, SO_REUSEADDR, &opt1, (socklen_t)sizeof(opt1))) {
        // pEor("setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(listen_fd2, SOL_SOCKET, SO_REUSEADDR, &opt2, (socklen_t)sizeof(opt2))) {
        // pEor("setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))");
        exit(EXIT_FAILURE);
    }

    address1.sin_family = AF_INET;
    // address1.sin_addr.s_addr = INADDR_ANY;
    inet_pton(AF_INET, "127.0.0.1", &address1.sin_addr);
    address1.sin_port = htons(PORT1);

    address2.sin_family = AF_INET;
    // address2.sin_addr.s_addr = INADDR_ANY;
    inet_pton(AF_INET, "127.0.0.1", &address2.sin_addr);
    address2.sin_port = htons(PORT2);

    if (bind(listen_fd1, (struct sockaddr *)&address1, (socklen_t)sizeof(address1)) < 0)
    {
        // pEor("[Server] bind() failed for port 2201.");
        exit(EXIT_FAILURE);
    }
    if (bind(listen_fd2, (struct sockaddr *)&address2, (socklen_t)sizeof(address2)) < 0)
    {
        // pEor("[Server] bind() failed for port 2202.");
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd1, 1) < 0)
    {
        // pEor("[Server] listen() failed for port 2201.");
        exit(EXIT_FAILURE);
    }
    printf("[Server] Running on port %d\n", PORT1);

    if (listen(listen_fd2, 1) < 0)
    {
        // pEor("[Server] listen() failed for port 2202.");
        exit(EXIT_FAILURE);
    }
    printf("[Server] Running on port %d\n", PORT2);

    struct sockaddr_in client1, client2;
    int client1len, client2len;
    client1len = sizeof(client1);
    client2len = sizeof(client2);
    conn_fd1 = accept(listen_fd1, (struct sockaddr *)&client1, (socklen_t *)&client1len);
    // read(conn_fd1, buffer, BUFFER_SIZE);

    printf("Client 1 accepted\n");
    conn_fd2 = accept(listen_fd2, (struct sockaddr *)&client2, (socklen_t *)&client2len);
    
    // if ((conn_fd1 = accept(listen_fd1, (struct sockaddr *)&client1, (socklen_t *)&client1len)) < 0)
    // {
    //     // pEor("[Server] accept() failed for port 2201.");
    //     exit(EXIT_FAILURE);
    // }
    // if ((conn_fd2 = accept(listen_fd2, (struct sockaddr *)&client2, (socklen_t *)&client2len)) < 0)
    // {
    //     // pEor("[Server] accept() failed for port 2202.");
    //     exit(EXIT_FAILURE);
    // }
    
    printf("after both accepts\n");
    

    bool begin = false;
    bool initialize = false;
    int turn = 1;
    Player p1, p2;
    bool initialized = false;
    int width, height;
    width = 0; height = 0;
    bool gameOver = false;
    while (!gameOver)
    {
        memset(buffer, 0, BUFFER_SIZE);
        if(turn == 1) {
            // printf("\n");
            // printf("in turn 1\n");
            // printf("Buffer is %s\n", buffer);
            
            read(conn_fd1, buffer, BUFFER_SIZE);
            if (strcmp(buffer, "F") == 0)
            {
                // send_message(conn_fd1, "H 0");
                send(conn_fd1, "H 0", 4, 0);
                read(conn_fd2, buffer, BUFFER_SIZE);
                send(conn_fd2, "H 1", 4, 0);
                // send_message(conn_fd2, "H 1");
                gameOver = true;
                continue;
            }
            else if(!begin) {
                if(buffer[0] != 'B') send(conn_fd1, "E 100", 8, 0);//send_message(conn_fd1, "E 100"); //not a B request
                else if(sscanf(buffer, "B %d %d", &width, &height) != 2) {
                    width = 0; height = 0;
                    send(conn_fd1, "E 200", 8, 0);//send_message(conn_fd1, "E 200"); //not valid parameters of B request
                } 
                else {
                    if(width < 10 || height < 10) {
                        width = 0; height = 0; //just in case, height and width are set back to 0
                        send(conn_fd1, "E 200", 8, 0);//send_message(conn_fd1, "E 200"); //parameters are invalid size (at least 10x10)
                    } 
                    else {
                        turn = 2;
                        send(conn_fd1, "A", 2, 0); //if everything's good, then height and width are set correct
                    }
                }
            }
            else if(!initialize) {
                if(buffer[0] != 'I') send(conn_fd1, "E 101", 8, 0);//send_message(conn_fd1, "E 101"); //not an I request
                else {
                    int res = processInitialize(&p1, buffer);
                    printf("After processinitialize Res: %d\n", res);
                    if(res) {
                        char temp[20];
                        sprintf(temp, "E %d", res);
                        send(conn_fd1, temp, 8, 0);
                        //send_message(conn_fd1, temp);
                    }
                    else {
                        res = placeShips(&p1);
                        printf("Else Res is %d\n", res);
                        if(!res) {
                            send(conn_fd1, "E 303", 8, 0);//send_message(conn_fd1, "E 303"); 
                        }
                        else {
                            turn = 2;
                            memset(buffer, 0, BUFFER_SIZE);
                            send(conn_fd1, "A", 2, 0);
                        }
                    }
                    
                }
            }

            // else {
            //     //if it's a "Q" packet
            //     if(strcmp(buffer, "Q") == 0) {
            //         memset(buffer, 0, BUFFER_SIZE);
            //         processQuery(&p1, buffer);
            //         send(conn_fd1, buffer, strlen(buffer)+1, 0);
            //     }
            //     else if(buffer[0] == 'S') {
            //         int res = processShoot(&p1, &p2, buffer);
            //         char temp[20];
            //         if(res) {
            //             sprintf(temp, "E %d", res);
            //             send(conn_fd1, temp, 8, 0);
            //         }
            //         //if res == 0, that means it's a success, so make the string to return "R (ships_remaning) (miss or hit)"
            //         else {
            //             char result = p1.shot_history[p1.num_shots-1].result;
            //             int ships = p1.ships_remaining;
            //             sprintf(temp, "R %d %c", ships, result);
            //             int turn = 2;
            //             send(conn_fd1, temp, strlen(temp)+1, 0);
            //         }
            //     }
            //     //if none, then it's invalid
            //     else {
            //         send(conn_fd1, "E 102", 8, 0);//send_message(conn_fd1, "E 102");
            //     }
            // }   
            
        }

        // //Player 2
        else {
            // printf("\n");
            // printf("in turn 2\n");
            // printf("Buffer is %s\n", buffer);
            read(conn_fd2, buffer, BUFFER_SIZE);
            if (strcmp(buffer, "F") == 0)
            {
                // send_message(conn_fd1, "H 0");
                send(conn_fd2, "H 0", 4, 0);
                read(conn_fd1, buffer, BUFFER_SIZE);
                send(conn_fd1, "H 1", 4, 0);
                // send_message(conn_fd2, "H 1");
                gameOver = true;
                continue;
            }
            else if(!begin) {
                // int width, height;
                if(strcmp(buffer, "B")) {
                    if(buffer[0] != 'B') send(conn_fd2, "E 100", 8, 0);//send_message(conn_fd2, "E 100");
                    else send(conn_fd2, "E 200", 8, 0);
                } 
                else {
                    initializePlayer(&p1, width, height);
                    initializePlayer(&p2, width, height);
                    printf("Players initialized %d x %d\n", width, height);
                    initialized = true;
                    begin = true;
                    turn = 1;
                    send(conn_fd2, "A", 2, 0);// send_message(conn_fd2, "A");
                }
            }
            else if(!initialize) {
                if(buffer[0] != 'I') send(conn_fd2, "E 101", 8, 0);//send_message(conn_fd2, "E 101"); //not an I request
                else {
                    int res = processInitialize(&p2, buffer);
                    if(res) {
                        char temp[20];
                        sprintf(temp, "E %d", res);
                        send(conn_fd2, temp, strlen(temp)+1, 0);
                    }
                    else {
                        res = placeShips(&p2);
                        if(res == 0) {
                            send(conn_fd2, "E 303", 8, 0);//send_message(conn_fd2, "E 303"); 
                        }
                        else {
                            turn = 1;
                            memset(buffer, 0, BUFFER_SIZE);
                            initialize = true;
                            send(conn_fd2, "A", 2, 0);//send_message(conn_fd2, "A");
                        }
                    }
                }
            }
            // else {
            //     //if it's a "Q" packet
            //     if(strcmp(buffer, "Q") == 0) {
            //         memset(buffer, 0, BUFFER_SIZE);
            //         processQuery(&p2, buffer);
            //         send(conn_fd2, buffer, strlen(buffer)+1, 0);// send_message(conn_fd2, buffer);
            //     }
            //     else if(buffer[0] == 'S') {
            //         int res = processShoot(&p2, &p1, buffer);
            //         char temp[20];
            //         if(res) {
            //             sprintf(temp, "E %d", res);
            //             send(conn_fd2, temp, strlen(temp)+1, 0);// send_message(conn_fd2, temp);
            //         }
            //         //if res == 0, that means it's a success, so make the string to return "R (ships_remaning) (miss or hit)"
            //         else {
            //             char result = p2.shot_history[p2.num_shots-1].result;
            //             int ships = p2.ships_remaining;
            //             sprintf(temp, "R %d %c", ships, result);
            //             int turn = 1;
            //             send(conn_fd2, temp, strlen(temp)+1, 0);// send_message(conn_fd2, temp);
            //         }
            //     }
            //     //if none, then it's invalid
            //     else {
            //         send(conn_fd2, "E 102", 8, 0);// send_message(conn_fd2, "E 102");
            //     }
            // }

        }
        
    }
    printf("[Server] Shutting down.\n");

    

    if(initialized) {
        freePlayer(&p1);
        freePlayer(&p2);
    }
    
    
    
    
    close(conn_fd1);
    close(conn_fd2);
    close(listen_fd1);
    close(listen_fd2);
    printf("closed all ports\n");
    return EXIT_SUCCESS;
}

void freePlayer(Player *player) {
    // printf("in free player\n");
    free(player -> shot_history);
    // printf("after freeing shothistory\n");
    int height = player -> height;
    for(int i = 0; i < height; i++) {
        free(player->board[i]);
        // printf("freed board[%d]\n", i);
    }
    // printf("after for loop\n");
    free(player->board);
}

void initializePlayer(Player *player, int width, int height) {
    player -> board = malloc(height * 8);
    player -> height = height;
    player -> width = width;
    player -> shot_history = malloc(12 * width * height);
    player -> ships_remaining = 5;
    player -> num_shots = 0;
    clearPieces(player);
    for(int i = 0; i < height; i++) {
        player->board[i] = malloc(width * 4);
    }
    clearBoard(player);
}

int processShoot(Player *pShooting, Player *pOther, char *buffer) {
    char *p = (buffer + 2);

    int i = 0;
    while(*p != '\0') {
        i++;
        while(*p != ' ' && *p != '\0') {p++;}
        if(*p != '\0') p++;
    }
    if(i != 2) return 202;
    
    p = (buffer + 2);

    int row, col;
    int value = sscanf(p, "%d %d", &row, &col);
    if(value != 2) return 202;
    if(row < 0 || row >= pOther -> height) return 400;
    if(col < 0 || col >= pOther -> width) return 400;
    if(alreadyShot(pShooting, row, col)) return 401;

    pShooting->shot_history[pShooting->num_shots].col = col;
    pShooting->shot_history[pShooting->num_shots].row = row;
    
    if(pOther->board[row][col] != 0) { 
        pShooting->shot_history[pShooting->num_shots].result = 'H';
        pOther->board[row][col] = 0; //set it to 0 so that when it's checked later the ship won't be there
        int num_ships = numOfShips(pOther);
        pShooting->ships_remaining = num_ships;
    }
    else pShooting->shot_history[pShooting->num_shots].result = 'M';
    pShooting->num_shots++;
    return 0;
}

int numOfShips(Player *pOther) {
    //checks if the list contains each of the five types of ships
    bool one, two, three, four, five;
    one = false; two = false; three = false; four = false; five = false;
    int num_ships = 5;
    for(int r = 0; r < pOther->height; r++) {
        for(int c = 0; c < pOther->width; c++) {
            if(!one & pOther->board[r][c] == 1) one = true;
            if(!two & pOther->board[r][c] == 2) two = true;
            if(!three & pOther->board[r][c] == 3) three = true;
            if(!four & pOther->board[r][c] == 4) four = true;
            if(!five & pOther->board[r][c] == 5) five = true;
        }
    }
    if(!one) num_ships--;
    if(!two) num_ships--;
    if(!three) num_ships--;
    if(!four) num_ships--;
    if(!five) num_ships--;

    return num_ships;
}
//checks if player already shot at the given row and column
int alreadyShot(Player *pShooting, int row, int col) {
    if (pShooting-> num_shots == 0) return 0;
    for(int i = 0; i < pShooting -> num_shots; i++) {
        if(pShooting->shot_history[i].col == col && pShooting->shot_history[i].row == row) return 1;
    }
    return 0;
}

int processInitialize(Player *player, char *buffer) {
    if(strlen(buffer) < 3) return 201;
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
    int cur_Eor = 999;
    for(int i = 0; i < 5; i++) {
        int value = sscanf(p, "%d %d %d %d", &piece_type, &piece_rotation, &column, &row);
        if(value != 4) return 201;
        if(piece_type <= 0 || piece_type > 7) return 300;
        else if(piece_rotation <= 0 || piece_rotation > 4) {
            if(cur_Eor > 301) cur_Eor = 301;
        } 
        else {
            int value = checkBounds(player, piece_type, piece_rotation, column, row);
            printf("checkBounds is %d for ship %d\n", value, i+1);
            if(value) {
                if(cur_Eor > 302) cur_Eor = 302;
            }
        }
        for(int i = 0; i < 4; i++) {
            while(*p != ' ' && *p != '\0') p++;
            if(*p == '\n') break;
            p++;
        }
    }
    if(cur_Eor != 999) return cur_Eor;
    
    
    p = (buffer + 2);

    for(int i = 0; i < 5; i++) {
        int value = sscanf(p, "%d %d %d %d", &piece_type, &piece_rotation, &column, &row);
        
        player -> pieces[i][0] = piece_type;
        player -> pieces[i][1] = piece_rotation;
        player -> pieces[i][2] = column;
        player -> pieces[i][3] = row;

        for(int i = 0; i < 4; i++) {
            while(*p != ' ' && *p != '\0') p++;
            if(*p == '\0') break;
            p++;
        }
    }
    return 0;
}

void processQuery(Player *player, char *str) {
    int num_shots = player -> num_shots;
    strcpy(str, "G 5");
    if(num_shots == 0) {
        return;
    }
    for(int i = 0; i < num_shots; i++) {
        char temp[12]; ///this will have enough space for ' 'H 100 100\0
        int col = player->shot_history[i].col; //always be 4 bytes
        int row = player->shot_history[i].row; //always be 4 bytes
        char result = player->shot_history[i].result; // always be 1 character, 1 byte
        sprintf(temp, " %c %d %d", result, col, row);
        strcat(str, temp);
    }
}

int checkBounds(Player *player, int piece_type, int piece_rotation, int column, int row) {
    int width = player->width; int height = player->height;
    printf("in check bounds with width %d and height %d. Column: %d Row: %d Piece:%d Rotation:%d\n", width, height, column, row, piece_type, piece_rotation);
    if(column < 0 || column >= player -> width) return 302;
    if(row < 0 || row >= player->height) return 302;
    switch(piece_type) {
        case 1:
            if(row > height-2) return 302;  
            if(column > width-2) return 302;
            break;
        case 2:
            if(piece_rotation == 1 || piece_rotation == 3) {
                if(column >= width) return 302;
                if(row > height-4) return 302;
            }
            else {
                if(row >= height) return 302;
                if(column > width-4) return 302;
                
            }
            break;
        case 3:
            if(piece_rotation == 1 || piece_rotation == 3) {
                if(row - 1 < 0) return 302;
                if(row - 1 > height - 2) return 302;
                if(column > width - 3) return 302;
            }
            else {
                if(row > height - 3) return 302;
                if(column > width - 2) return 302;
            }
            break;
        //shape 4
        case 4:
            printf("in case 4 checkBOunds with rotation %d\n", piece_rotation);
            if(piece_rotation == 1 || piece_rotation == 3) {
                if(row > height-3) {
                    printf("invalid row\n");
                    return 302;
                } 
                if(column > width-2) {
                    printf("invalid column\n");
                    return 302;
                } 
            }
            else if(piece_rotation == 2) {
                if(column > width - 3) return 302;
                if(row > height - 2) return 302;
            }
            else {
               if(column > width - 3) return 302;
               if(row - 1 < 0) return 302;
               if(row-1 > height-2) return 302;
            }   
            break;
        case 5:
            if(piece_rotation == 1 || piece_rotation == 3) {
                if(column > width - 3) return 302;
                if(row > height - 2) return 302;
            }
            else {
                if(column > width -2 ) return 302;
                if(row - 1 < 0) return 302;
                if(row-1 > height-3) return 302;
            }
            break;
        case 6:
            if(piece_rotation == 2 || piece_rotation == 4) {
               if(column > width - 3) return 302;
               if(row > height - 2) return 302;
            }
            else if(piece_rotation == 1) {
                if(column > width -2) return 302;
                if(row - 2 < 0) return 302;
            }
            else {
                if(column > width -2) return 302;
                if(row + 3 > height) return 302;
            }
            break;
        default:
            printf("in default\n");
            if(piece_rotation == 1 || piece_rotation == 3) {
                if(column + 3 > width) return 302;
                
                if(piece_rotation == 1) {
                    if(row + 1 > height) return 302;
                }
                else {
                    if(row - 1 < 0) return 302;
                }
            }
            //rotation 2 or 4
            else {
                if(column + 2 > width) return 302;
                if(piece_rotation == 2) {
                    if(row - 1 < 0) return 302;
                    if(row + 2 > height) return 302;
                }
                else {
                    if(row + 3 > height) return 302;
                }
            }
    }
    return 0;
}

int placeShips(Player *player) {
    int piece_type, piece_rotation, column, row;

    //board to be used to check for ship overlap
    for(int r = 0; r < 5; r++) {
        piece_type = player->pieces[r][0];
        piece_rotation = player->pieces[r][1];
        column = player->pieces[r][2];
        row = player->pieces[r][3];
        if(placeShip(player, piece_type, piece_rotation, column, row, r+1) == 0) return 0;
    }
    return 1;
}


int placeShip(Player *player, int piece_type, int piece_rotation, int column, int row, int cur_ship) {
    switch(piece_type) {
        case 1:
            for(int r = 0; r < 2; r++) {
                for(int c = 0; c < 2; c++) {
                    if(player->board[r+row][c+column] != 0) {
                        clearBoard(player);
                        clearPieces(player);
                        return 0;
                    }
                    player->board[r+row][c+column] = cur_ship;
                }
            }
            break;
        case 2:
            if(piece_rotation == 1 || piece_rotation == 3) {
                for(int r = 0; r < 4; r++) {
                    if(player->board[r+row][column] != 0) {
                        clearBoard(player);
                        clearPieces(player);
                        printf("Ship %d, Shape 2, rotation 1 or 2, board not empty\n", cur_ship);
                        return 0;
                    }
                    player->board[r+row][column] = cur_ship;
                }
            }
            else {
                for(int c = 0; c < 4; c++) {
                    if(player->board[row][c+column] != 0) {
                        clearBoard(player);
                        clearPieces(player);
                        return 0;
                    }
                    player->board[row][c+column] = cur_ship;
                }
            }
            break;
        case 3:
            if(piece_rotation == 1 || piece_rotation == 3) {
                for(int r = 0; r < 2; r++) {
                    if(r == 0) {
                        for(int c = 1; c < 3; c++) {
                            if(player->board[r + row-1][c + column] != 0) {
                                clearBoard(player);
                                clearPieces(player);
                                return 0;
                            }
                            player->board[r+row-1][c+column] = cur_ship;
                            
                        }
                    }
                    else {
                        for(int c = 0; c < 2; c++) {
                            if(player->board[r + row-1][c + column] != 0) {
                                clearBoard(player);
                                clearPieces(player);
                                return 0;
                            }
                            player->board[r+row-1][c+column] = cur_ship;
                        }
                    }
                    
                }
            }
            //rotation 2 and 4
            else {
                for(int c = 0; c < 2; c++) {
                    if(c == 0) {
                        for(int r = 0; r < 2; r++) {
                            if(player->board[r + row][c + column]  != 0) {
                                clearBoard(player);
                                clearPieces(player);
                                return 0;
                            }
                            player->board[r+row][c+column] = cur_ship;
                        }
                    }
                    else {
                        for(int r = 1; r < 3; r++) {
                            if(player->board[r + row][c + column]  != 0) {
                                clearBoard(player);
                                clearPieces(player);
                                return 0;
                            }
                            player->board[r+row][c+column] = cur_ship;
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
                            if(player->board[r + row][c + column]  != 0) {
                                clearBoard(player);
                                clearPieces(player);
                                printf("In shape 4 rotation 1 eror with ship %d\n", cur_ship);
                                return 0;
                            }
                            player->board[r+row][c+column] = cur_ship;
                            printf("In c== 0Ship %d inserted at col %d and row %d\n", cur_ship, c+column, r+row);
                        }
                    }
                    else {
                        if(player->board[2+row][c+column] != 0) {
                            clearBoard(player);
                            clearPieces(player);
                            return 0;
                        }
                        player->board[2+row][c+column] = cur_ship;
                        printf("Else Ship %d inserted at col %d and row %d\n", cur_ship, c+column, 2+row);
                    }
                }
               
            }
            else if(piece_rotation == 2) {
                for(int r = 0; r < 2; r++) {
                    if(r == 0) {
                        for(int c = 0; c < 3; c++) {
                            if(player->board[r + row][c + column]  != 0) {
                                clearBoard(player);
                                clearPieces(player);
                                return 0;
                            }
                            player->board[r+row][c+column] = cur_ship;
                        }
                    }
                    else {
                        if(player->board[r+row][column]  != 0) {
                            clearBoard(player);
                            return 0;
                        }
                        player->board[r+row][column] = cur_ship;
                    }
                }
            }
            else if(piece_rotation == 3) {
                 for(int c = 0; c < 2; c++) {
                    if(c == 1) {
                        for(int r = 0; r < 3; r++) {
                            if(player->board[r + row][c + column]  != 0) {
                                clearBoard(player);
                                clearPieces(player);
                                return 0;
                            }
                            player->board[r+row][c+column] = cur_ship;
                        }
                    }
                    else {
                        if(player->board[row][column] != 0) {
                            clearBoard(player);
                            clearPieces(player);
                            return 0;
                        }
                        player->board[row][column] = cur_ship;
                    }
                }
            }
            else {
                 for(int r = 0; r < 2; r++) {
                    if(r == 1) {
                        for(int c = 0; c < 3; c++) {
                            if(player->board[r + row - 1][c + column]  != 0) {
                                clearBoard(player);
                                clearPieces(player);
                                return 0;
                            }
                            player->board[r+row-1][c+column] = cur_ship;
                        }
                    }
                    else {
                        if(player->board[row-1][2+column]  != 0) {
                            clearBoard(player);
                            clearPieces(player);
                            return 0;
                        }
                        player->board[row-1][2+column] = cur_ship;
                    }
                }
            }   
            break;
        case 5:
            if(piece_rotation == 1 || piece_rotation == 3) {
                for(int r = 0; r < 2; r++) {
                    if(r == 0) {
                        for(int c = 0; c < 2; c++) {
                            if(player->board[r+row][c+column]  != 0) {
                                clearBoard(player);
                                clearPieces(player);
                                return 0;
                            }
                            player->board[r+row][c+column] = cur_ship;
                        } 
                    }
                    else {
                        for(int c = 1; c < 3; c++) {
                            if(player->board[r+row][c+column]  != 0) {
                                clearBoard(player);
                                clearPieces(player);
                                return 0;
                            }
                            player->board[r+row][c+column] = cur_ship;
                        }
                    }
                }
            }
            //rotations 2 and 4
            else {
                for(int c = 0; c < 2; c++) {
                    if(c == 0) {
                        for(int r = 0; r < 2; r++) {
                            if(player->board[r+row][c+column]  != 0) {
                                clearBoard(player);
                                clearPieces(player);
                                return 0;
                            }
                            player->board[r+row][c+column] = cur_ship;
                        }
                    }
                    else {
                        for(int r = 0; r< 2; r++) {
                            if(player->board[r+row-1][c+column]  != 0) {
                                clearBoard(player);
                                clearPieces(player);
                                return 0;
                            }
                            player->board[r+row-1][c+column] = cur_ship;
                        }
                    }
                }
            }
            break;
        case 6:
            switch(piece_rotation) {
                case 1:
                    for(int c = 0; c < 2; c++) {
                        if(c = 1) {
                            for(int r = 0; r < 3; r++) {
                                if(player->board[r+row-2][c+column]  != 0) {
                                    clearBoard(player);
                                    clearPieces(player);
                                    return 0;
                                }
                                player->board[r+row-2][c+column] = cur_ship;
                            }
                        }
                        else {
                            if(player->board[row][column]  != 0) {
                                clearBoard(player);
                                clearPieces(player);
                                return 0;
                            }
                            player->board[row][column] = cur_ship;
                        }
                    }
                    break;
                
                case 2:
                    for(int r = 0; r < 2; r++) {
                        if(r == 1) {
                            for(int c = 0; c < 3; c++) {
                                if(player->board[r+row][c+column]  != 0) {
                                    clearBoard(player);
                                    clearPieces(player);
                                    return 0;
                                }
                                player->board[r+row][c+column] = cur_ship;
                            }
                        }
                        else {
                            if(player->board[row][column]  != 0) {
                                clearBoard(player);
                                clearPieces(player);
                                return 0;
                            }
                            player->board[row][column] = cur_ship;
                        }
                    }
                    break;
                
                case 3:
                    for(int c = 0; c < 2; c++) {
                        if(c == 0) {
                            for(int r = 0; r < 3; r++) {
                                if(player->board[r+row][column]  != 0) {
                                    clearBoard(player);
                                    clearPieces(player);
                                    return 0;
                                }
                                player->board[r+row][column] = cur_ship;
                            }
                        }
                        else {
                            if(player->board[row][column+c] != 0) {
                                    clearBoard(player);
                                    clearPieces(player);
                                    return 0;
                                }
                            player->board[row][column+c] = cur_ship;
                        }
                    }
                    break;
                //rotation 4
                default:
                    for(int r = 0; r < 2; r++) {
                        if(r == 0) {
                            for(int c = 0; c < 3; c++) {
                                if(player->board[row][c+column]  != 0) {
                                    clearBoard(player);
                                    clearPieces(player);
                                    return 0;
                                }
                                player->board[row][c+column] = cur_ship;
                            }
                        }
                        else {
                            if(player->board[r+row][column+2]  != 0) {
                                clearBoard(player);
                                clearPieces(player);
                                return 0;
                            }
                            player->board[r+row][column+2] = cur_ship;
                        }
                    }
            }
            break;
        default:
            switch(piece_rotation) {
                case 1:
                    for(int r = 0; r < 2; r++) {
                        if(r == 0) {
                            for(int c = 0; c < 3; c++) {
                                if(player->board[row][column+c] == 1) {
                                    clearBoard(player);
                                    clearPieces(player);
                                    return 0;
                                }
                                player->board[row][column+c] = 1;
                            }
                        }
                        else {
                            if(player->board[r+row][column+1] == 1) {
                                clearBoard(player);
                                clearPieces(player);
                                return 0;
                            }
                            player->board[r+row][column+1] = 1;
                        }
                    }
                    break;


                case 2:
                    for(int c = 0; c < 2; c++) {
                        if(c == 1) {
                            for(int r = 0; r < 3; r++) {
                                if(player->board[row+r][column+c] == 1) {
                                    clearBoard(player);
                                    clearPieces(player);
                                    return 0;
                                }
                                player->board[row+r][column+c] = 1;
                            }
                        }
                        else {
                             if(player->board[row+1][column+c] == 1) {
                                    clearBoard(player);
                                    clearPieces(player);
                                    return 0;
                                }
                            player->board[row+1][column+c] = 1;
                        }
                    }
                    break;

                case 3:
                    for(int r = 0; r < 2; r++) {
                        if(r == 1) {
                            for(int c = 0; c < 3; c++) {
                                if(player->board[r+row][column+c] == 1) {
                                    clearBoard(player);
                                    clearPieces(player);
                                    return 0;
                                }
                                player->board[r+row][column+c] = 1;
                            }
                        }
                        else {
                            if(player->board[row][column+1] == 1) {
                                clearBoard(player);
                                clearPieces(player);
                                return 0;
                            }
                            player->board[row][column+1] = 1;
                        }
                    }
                    break;

                default:
                    for(int c = 0; c < 2; c++) {
                        if(c == 0) {
                            for(int r = 0; r < 3; r++) {
                                if(player->board[row+r][column+c] == 1) {
                                    clearBoard(player);
                                    clearPieces(player);
                                    return 0;
                                }
                                player->board[row+r][column+c] = 1;
                            }
                        }
                        else {
                             if(player->board[row+1][column+c] == 1) {
                                    clearBoard(player);
                                    clearPieces(player);
                                    return 0;
                                }
                            player->board[row+1][column+c] = 1;
                        }
                    }
            }
    }
    return 1;
}



void clearPieces(Player *player) {
    for(int r = 0; r < 5; r++) {
        for(int c = 0; c < 4; c++) {
            player->pieces[r][c] = 0;
        }
    }
}
void clearBoard(Player *player) {
    for(int r = 0; r < player->height; r++) {
        for(int c = 0; c < player->width; c++) {
            player->board[r][c] = 0;
        }
    }
}
