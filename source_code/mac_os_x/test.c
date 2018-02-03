#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wmissing-noreturn"

// The size of all types of ships
#define CARRIER_SIZE 5
#define BATTLESHIP_SIZE 4
#define SUBMARINE_SIZE 3
#define DESTROYER_SIZE 2

// The abbreviations of all kinds of ships
#define CARRIER_LETTER 'C'
#define BATTLESHIP_LETTER 'B'
#define SUBMARINE_LETTER 'S'
#define DESTORYER_LETTER 'D'

// The qunatity of all kinds of ships
#define CARRIER_QUANTITY 1
#define BATTLESHIP_QUANTITY 2
#define SUBMARINE_QUANTITY 3
#define DESTROYER_QUANTITY 4

// The total number of ships
#define NB_SHIPS 10

// The size of the main board and the mark board
#define BOARD_HEIGHT 10
#define BOARD_WIDTH 10

#define BACKLOG 10
#define MAXSIZE 1024
#define BUFFER_SIZE 1024

#define TRUE 1
#define FALSE 0

// Represent a point on the main board or the mark board
typedef struct
{
    char letter;
    char digit;
} Point;

// Represent a ship
typedef struct
{
    int ship_size;
    char ship_letter;
} Ship;

/* Global variables */
char main_board[BOARD_HEIGHT][BOARD_WIDTH];

char mark_board[BOARD_HEIGHT][BOARD_WIDTH];

Point fire_position;

int fire_result;

Ship *ships[NB_SHIPS];

int here_port;

int there_port;

char *there_ip_address;

int my_turn = TRUE;

/**
 * @brief Print the main board and the mark board
 * @param main_board the board on which we can see the ships
 * @param mark_board the board on which we put "X" or "x"
 */
void
print_board(char main_board[BOARD_HEIGHT][BOARD_WIDTH], char mark_board[BOARD_HEIGHT][BOARD_WIDTH])
{
    printf("Main board              Mark board\n");

    for (int i = 0; i < BOARD_HEIGHT; i++) {
        printf("%d ", i);

        for (int j = 0; j < BOARD_WIDTH; j++)
            printf("%c ", main_board[i][j]);

        printf("  ");

        for (int k = 0; k < BOARD_WIDTH; k++)
            printf("%c ", mark_board[i][k]);

        printf("\n");
    }

    printf("  ");

    // Print letters A to J
    for (int i = 0; i < BOARD_WIDTH; i++)
        printf("%c ", 'A' + i);

    // Print two spaces
    printf("  ");

    // Re-print letters A to J
    for (int i = 0; i < BOARD_WIDTH; i++)
        printf("%c ", 'A' + i);

    // Return to new line
    printf("\n\n");
}

/**
 * @brief check if a point is in the scope of the main board
 * @param position the position to check validity
 * @return if the position is in the scope of the main board, reuturn TRUE, otherwise return FALSE
 */
int
point_on_board(Point position)
{
    char letter = position.letter;
    char digit = position.digit;

    if (letter < 'A' || letter > 'Z' || digit < '0' | digit > '9')
        return FALSE;

    return TRUE;
}

/**
 * @brief print a line of asterisks to help us better see the content on the console
 */
void
separate(void)
{
    printf("\n\n******************************************************************\n\n");
}

/**
 * @brief initialize a board using the "-" sign
 * @param board the board to initiate
 */
void
initialize_board(char board[BOARD_HEIGHT][BOARD_WIDTH])
{
    for (int i = 0; i < BOARD_HEIGHT; i++)
        for (int j = 0; j < BOARD_WIDTH; j++)
            board[i][j] = '-';
}

/**
 * @brief decides wether to turn a upper case letter to its lower case version
 * @param position the position that our opponent puts fire on
 * @return if our enemy successfully puts fire on our ship, return TRUE, otherwise return FALSE
 */
int
adjust_main_board(Point position)
{
    char letter = position.letter;
    char digit = position.digit;
    int row = digit - '0';
    int col = letter - 'A';
    char *c = &main_board[row][col];

    if (*c == '-')
        return FALSE;

    if (*c >= 'A' && *c <= 'Z') {
        *c = tolower(*c);
        return TRUE;
    }
    // It is not possible to come to this line of code,
    // but if ever, it means that there is a fatal error
    printf("Fatal error here\n");
    return TRUE;
}

/**
 * @brief adjust the mark board according to the global variables "fire_position" & "fire_result"
 */
void
adjust_mark_board(void)
{
    char letter = fire_position.letter;
    char digit = fire_position.digit;
    int row = digit - '0';
    int col = letter - 'A';

    // fire success
    if (fire_result == TRUE) {
        mark_board[row][col] = 'x';
    }
    else if (fire_result == FALSE) {
        mark_board[row][col] = 'X';
    }
    else {
        printf("Fatal error in adjust_mark_board() method");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief check if the game is over, the game is over when there are no capital letters on the main board anymore
 * @return if game is over return TRUE, otherwise return FALSE
 */
int
check_game_over(void)
{
    for (int i = 0; i < BOARD_HEIGHT; i++)
        for (int j = 0; j < BOARD_WIDTH; j++)
            if (main_board[i][j] >= 'A' && main_board[i][j] <= 'Z')
                return FALSE;
    return TRUE;
}

/**
 * @brief the function to be put in a thread to launch the server so that an opponent can connect to us
 * @return this function would be called in a thread, so it must return (void *), we have no other choices
 */
void *
server_thread(void *data)
{
    struct sockaddr_in server; // Local endpoint
    struct sockaddr_in dest; // Remote endpoint
    int socket_fd; // Local endpoint socket descriptor
    int client_fd; // Remote endpoint socket descriptor
    int num; // Number of bytes received
    socklen_t size; // The size of the "socketaddr_in" structure

    char buffer[BUFFER_SIZE]; // The buffer to put the message received in
    int yes = TRUE;
    char buffer_reply_positive[BUFFER_SIZE]; // Buffer to accommodate the positive reply
    char buffer_reply_negative[BUFFER_SIZE]; // Buffer to accommodate the negative reply

    const char *reply = "reply";
    const char *positive = "positive";
    const char *negative = "negative";
    const char *game_over = "game-over";

    // Get a random socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Socket failure!!\n");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    memset(&server, 0, sizeof(server));
    memset(&dest, 0, sizeof(dest));
    server.sin_family = AF_INET;
    server.sin_port = htons(here_port);
    server.sin_addr.s_addr = INADDR_ANY;

    if ((bind(socket_fd, (struct sockaddr *) &server, sizeof(struct sockaddr))) == -1) { //sizeof(struct sockaddr)
        fprintf(stderr, "Binding Failure\n");
        exit(EXIT_FAILURE);
    }

    if ((listen(socket_fd, BACKLOG)) == -1) {
        fprintf(stderr, "Listening Failure\n");
        exit(EXIT_FAILURE);
    }

    // Only treat one client
    size = sizeof(struct sockaddr_in);

    if ((client_fd = accept(socket_fd, (struct sockaddr *) &dest, &size)) == -1) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    printf("Server got connection from client %s:%d\n", inet_ntoa(dest.sin_addr), dest.sin_port);

    while (TRUE) {
        // Receive message from client, put message received in "buffer"
        if ((num = recv(client_fd, buffer, BUFFER_SIZE, 0)) == -1) {
            perror("recv");
            exit(EXIT_FAILURE);
        }
        else if (num == 0) {
            printf("Connection closed\n");
            break;
        }

        buffer[num] = '\0';
        printf("Server received message from client: %s\n", buffer);

        // case 1
        if (strncmp(game_over, buffer, strlen(game_over)) == 0) {
            printf("(case 1)Game over, you won the game!\n");
            exit(EXIT_SUCCESS);
        } // case 2
        else if (strncmp(reply, buffer, strlen(reply)) == 0) {

            my_turn = TRUE;
            printf("(case 2)Reply(positive/negative)\n");

            if (strncmp(positive, buffer + strlen(reply) + 1, strlen(positive)) == 0) {
                fire_result = TRUE;

            }
            else if (strncmp(negative, buffer + strlen(reply) + 1, strlen(negative)) == 0) {
                fire_result = FALSE;
            }
            else {
                printf("Fatal error, protocol issues!");
                exit(EXIT_FAILURE);
            }

            adjust_mark_board();
        } // case 3
        else if (buffer[0] >= 'A' && buffer[0] <= 'Z' && buffer[1] >= '0' && buffer[1] <= '9') {

            my_turn = TRUE;
            printf("(case 3)[A-Z][0-9]\n");

            Point position;
            position.letter = buffer[0];
            position.digit = buffer[1];
            sprintf(buffer_reply_positive, "reply(positive)");
            sprintf(buffer_reply_negative, "reply(negative);");
            buffer_reply_positive[strlen("reply(positive)")] = '\0';
            buffer_reply_negative[strlen("reply(negative)")] = '\0';

            if (adjust_main_board(position) == TRUE) {

                if ((send(client_fd, buffer_reply_positive, strlen(buffer_reply_positive), 0)) == -1) {
                    fprintf(stderr, "Failure Sending Message\n");
                    close(client_fd);
                    break;
                }
                else {
                    printf("Client sent message to server: %s\n", buffer_reply_positive);
                }

            }
            else {

                if ((send(client_fd, buffer_reply_negative, strlen(buffer_reply_negative), 0)) == -1) {
                    fprintf(stderr, "Failure Sending Message\n");
                    close(client_fd);
                    break;
                }
                else {
                    printf("Client sent message to server: %s\n", buffer_reply_negative);
                }

            }
        }
        else {
            printf("(case 4)not possible!");
            exit(EXIT_FAILURE);
        }

        // Check if game is over
        if (check_game_over() == TRUE) {
            printf("Game is over");
            if ((send(client_fd, game_over, strlen(buffer), 0)) == -1) {
                fprintf(stderr, "Failure Sending Message\n");
                close(client_fd);
                break;
            }

            printf("Game over, you lost the game");
            exit(EXIT_SUCCESS);
        }

        // Reprint the board until "game over"
        print_board(main_board, mark_board);

    } //End of Inner While...
    //Close Connection Socket
    close(client_fd);
    close(socket_fd);
    return NULL;
}

/**
 * @brief check the content of the message before sending it to the server
 * @return if no errors, return TRUE, otherwise return FALSE
 */
int
check_message_before_sending(char letter, char digit)
{
    if (letter < 'A' || letter > 'Z' || digit < '0' || digit > '9')
        return FALSE;

    return TRUE;
}

/**
 * @brief the function to be put in a thread to connect to a distant server
 * @return this function would be called in a thread, so it must return (void *), we have no other choices
 */
void *
client_thread(void *data)
{
    struct sockaddr_in server_info;
    struct hostent *he;
    int socket_fd, num;
    char buffer[BUFFER_SIZE];

    char buffer_reply[BUFFER_SIZE];
    /* Case 2 */
    const char *reply = "reply";
    const char *positive = "positive";
    const char *negative = "negative";

    if ((he = gethostbyname(there_ip_address)) == NULL) {
        fprintf(stderr, "Cannot get host name\n");
        exit(EXIT_FAILURE);
    }

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Socket Failure!!\n");
        exit(EXIT_FAILURE);
    }

    memset(&server_info, 0, sizeof(server_info));
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(there_port);
    server_info.sin_addr = *((struct in_addr *) he->h_addr);
    if (connect(socket_fd, (struct sockaddr *) &server_info, sizeof(struct sockaddr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    while (TRUE) {

        while (TRUE) {
            fgets(buffer, MAXSIZE - 1, stdin);

            if (my_turn == FALSE) {
                printf("It is not your turn yet, please wait...\n");
                continue;
            }

            // Before sending a message, we check its validity
            if (check_message_before_sending(buffer[0], buffer[1]) == TRUE) {
                break;
            }
            else {
                printf("Attack code error: %c%c!\n", buffer[0], buffer[1]);
                printf("Client: Enter Data for Server: ");
            }
        }

        if ((send(socket_fd, buffer, strlen(buffer), 0)) == -1) {
            fprintf(stderr, "Client failed to send message to server\n");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }
        else {
            my_turn = FALSE;

            printf("Client sent message to server: %s\n", buffer);
            // Set fire_position before sending message
            fire_position.letter = buffer[0];
            fire_position.digit = buffer[1];

            Point position;
            position.letter = fire_position.letter;
            position.digit = fire_position.digit;

            if (point_on_board(position) == FALSE) {
                separate();
                printf("The point you entered is not on the main board\n");
                separate();
            }

            // Receive message from client, put message received in "buffer"
            if ((num = recv(socket_fd, buffer_reply, BUFFER_SIZE, 0)) == -1) {
                perror("recv");
                exit(EXIT_FAILURE);
            }
            else if (num == 0) {
                printf("Connection closed\n");
                break;
            }
            buffer_reply[num] = '\0';
            printf("Client received message from server: %s\n", buffer_reply);

            // case 2
            if (strncmp(reply, buffer_reply, strlen(reply)) == 0) {
                printf("(case 2)Reply");
                if (strncmp(positive, buffer_reply + strlen(reply) + 1, strlen(positive)) == 0) {
                    fire_result = TRUE;
                }
                else if (strncmp(negative, buffer_reply + strlen(reply) + 1, strlen(negative)) == 0) {
                    fire_result = FALSE;
                }
                else {
                    printf("Fatal error, protocol issues!");
                    exit(EXIT_FAILURE);
                }
                adjust_mark_board();
            }
            print_board(main_board, mark_board);
        }
    }
    close(socket_fd);
    return NULL;
}

/**
 * @brief initialize the global variable "ships"
 */
void
initialize_ships(void)
{
    for (int i = 0; i < NB_SHIPS; i++) {
        // Allocate memory to the ship
        // This memory will be released in the end of the program
        ships[i] = (Ship *) malloc(sizeof(Ship));
        switch (i) {
            // The 1st ship in the array is a "Carrier"
            // In total 1 carrier
            case 0:
                ships[i]->ship_size = CARRIER_SIZE;
                ships[i]->ship_letter = CARRIER_LETTER;
                break;
                // The 2nd & 3rd ships in the array are "Battleship"s
                // In total 2 battleships
            case 1:
            case 2:
                ships[i]->ship_size = BATTLESHIP_SIZE;
                ships[i]->ship_letter = BATTLESHIP_LETTER;
                break;
                // The 4th to 6th ships in the array are submarines
                // In total 3 submarines
            case 3:
            case 4:
            case 5:
                ships[i]->ship_size = SUBMARINE_SIZE;
                ships[i]->ship_letter = SUBMARINE_LETTER;
                break;
                // The 7th to 10th ships in the array are destroyers
                // In total 4 destroyers
            case 6:
            case 7:
            case 8:
            case 9:
                ships[i]->ship_size = DESTROYER_SIZE;
                ships[i]->ship_letter = DESTORYER_LETTER;
                break;
            default:break;
        }
    }
}

/**
 * @brief check if a point is in the scope of the board
 * @param row the row of the point
 * @param col the column of the point
 * @return if the point is in the scope of the board, return TRUE, otherwise return FALSE
 */
int
point_in_scope(int row, int col)
{
    if (row < 0 || row > BOARD_HEIGHT - 1 || col < 0 || col > BOARD_WIDTH - 1)
        return FALSE;

    return TRUE;
}

/**
 * @brief checks if a line intersect with an existing ship
 * @param r1 the row of the first point
 * @param c1 the column of the first point
 * @param r2 the row of the second point
 * @param c2 the column of the second point
 * @return if the line intersects with an existing ship, return TRUE, otherwise return FALSE
 */
int
if_intersection(int r1, int c1, int r2, int c2)
{
    if (r1 == r2) {
        if (c1 > c2) {
            for (int i = c2; i <= c1; i++)
                if (isalpha(main_board[r1][i]))
                    return TRUE;
        }
        else {
            for (int i = c1; i <= c2; i++)
                if (isalpha(main_board[r1][i]))
                    return TRUE;
        }
    }
    else {
        if (r1 > r2) {
            for (int i = r2; i <= r1; i++)
                if (isalpha(main_board[i][c1]))
                    return TRUE;
        }
        else {
            for (int i = r1; i <= r2; i++)
                if (isalpha(main_board[i][c1]))
                    return TRUE;
        }
    }

    return FALSE;
}

/**
 * @brief put a given type of ship on the main board
 * @param r1 the row of one side of the ship
 * @param c1 the column of one side of the ship
 * @param r2 the row of the other side of the ship
 * @param c2 the column of the other side of the ship
 * @param ship_letter the letter to be put on the main board to represent the type of the ship
 */
void
put_ship_on_main_board(int r1, int c1, int r2, int c2, char ship_letter)
{
    if (r1 == r2) {
        if (c1 > c2) {
            for (int i = c2; i <= c1; i++)
                main_board[r1][i] = ship_letter;
        }
        else {
            for (int i = c1; i <= c2; i++)
                main_board[r1][i] = ship_letter;
        }
    }
    else {
        if (r1 > r2) {
            for (int i = r2; i <= r1; i++)
                main_board[i][c1] = ship_letter;
        }
        else {
            for (int i = r1; i <= r2; i++)
                main_board[i][c1] = ship_letter;
        }
    }
}

/**
 * @brief randomly put all ships in the main board with the given ship array
 */
void
randomly_set_main_board(void)
{
    srand(time(NULL));
    for (int i = 0; i < NB_SHIPS; i++) {

        Ship *s = ships[i];
        int l = s->ship_size;

        int r1;
        int c1;
        int r2;
        int c2;

        while (1) {

            r1 = rand() % BOARD_HEIGHT;
            c1 = rand() % BOARD_WIDTH;

            if (point_in_scope(r1, c1 + l - 1)) {
                r2 = r1;
                c2 = c1 + l - 1;
            }
            else if (point_in_scope(r1 - l + 1, c1)) {
                r2 = r1 - l + 1;
                c2 = c1;
            }
            else if (point_in_scope(r1, c1 - l + 1)) {
                r2 = r1;
                c2 = c1 - l + 1;
            }
            else if (point_in_scope(r1 + l - 1, c1)) {
                r2 = r1 + l - 1;
                c2 = c1;
            }
            else {
                continue;
            }

            if (if_intersection(r1, c1, r2, c2))
                continue;
            else {
                put_ship_on_main_board(r1, c1, r2, c2, s->ship_letter);
                break;
            }
        }
    }
}

void
print_rest_ships(void)
{
    printf("The rest ships\n\n");
    for (int i = 0; i < NB_SHIPS; i++) {
        Ship *s = ships[i];
        if (s != NULL) {
            printf("%c ", s->ship_letter);
            for (int i = 0; i < s->ship_size; i++)
                printf("*");
            printf("\n");
        }
    }
    printf("\n");
}

/**
 * @brief get the distance between two points
 * @param row1 the row of the first point
 * @param col1 the column of the fisrt point
 * @param row2 the row of the second point
 * @param col2 the column of the second point
 * @return the distance between the tw points
 */
int
distance(int row1, int col1, int row2, int col2)
{
    return abs(row1 + col1 - row2 - col2) + 1;
}

/**
 * @brief every time use inputs a string to put a ship on the main board, we check that the validity of this input
 * @param buf the char array representing the user input
 * @return if no error in the user input, return TRUE, otherwise return FALSE
 */
int
check_user_input(char buf[BUFFER_SIZE])
{

    // Check the ship letter validity
    char ship_letter = buf[0];

    separate();
    printf("The ship_letter is: %c\n", ship_letter);
    separate();

    if (ship_letter != CARRIER_LETTER
        && ship_letter != BATTLESHIP_LETTER
        && ship_letter != SUBMARINE_LETTER
        && ship_letter != DESTORYER_LETTER)
        return FALSE;

    char l1 = buf[1];
    char d1 = buf[2];
    char l2 = buf[3];
    char d2 = buf[4];

    if (l1 < 'A' || l1 > 'J' || l2 < 'A' || l2 > 'J')
        return FALSE;

    if (d1 < '0' || d1 > '9' || d2 < '0' || d2 > '9')
        return FALSE;

    // The two points must be on the same row or the same column
    if (l1 != l2 && d1 != d2)
        return FALSE;

    // Check if the distance between the two points is the length of the ship
    int row1 = d1 - '0';
    int col1 = l1 - 'A';
    int row2 = d2 - '0';
    int col2 = l2 - 'A';

    int length = distance(row1, col1, row2, col2);

    separate();
    printf("length = %d\n", length);
    separate();

    switch (ship_letter) {
        case CARRIER_LETTER:
            if (length != CARRIER_SIZE)
                return FALSE;
            break;
        case BATTLESHIP_LETTER:
            if (length != BATTLESHIP_SIZE)
                return FALSE;
            break;
        case SUBMARINE_LETTER:
            if (length != SUBMARINE_SIZE)
                return FALSE;
            break;
        case DESTORYER_LETTER:
            if (length != DESTROYER_SIZE)
                return FALSE;
            break;
    }

    return TRUE;
}

/**
 * @brief retrieve a ship from the ship array (global variable) "ships"
 * @param ship_letter the type of ship to be retrieved
 */
void
retrieve_a_ship(char ship_letter)
{
    for (int i = 0; i < NB_SHIPS; i++)
        if (ships[i] != NULL && ships[i]->ship_letter == ship_letter) {
            free(ships[i]);
            ships[i] = NULL;
        }
}

void
clear_input_buffer()
{
    char c;
    while ((c = getchar()) != '\n' && c != EOF) {
    }
}

/**
 * @brief manually set the main board in a loop
 */
void
manually_set_main_board(void)
{

    int cq = CARRIER_QUANTITY;
    int bq = BATTLESHIP_QUANTITY;
    int sq = SUBMARINE_QUANTITY;
    int dq = DESTROYER_QUANTITY;

    char buf_input[BUFFER_SIZE];
    char *error = NULL;

    while (1) {
        separate();
        print_rest_ships();
        print_board(main_board, mark_board);

        printf("\tRules:\n");
        printf("\t\t1. Enter \"CA0A4\" to put a carrier on the main board between point \"A0\" and \"A4\", etc.\n");
        printf("\t\t2. The two points entered must be on the same line\n");
        printf("\t\t3. The two points entered cannot be out of scope\n");
        printf("\t\t4. The distance between the two points must be the size of the chosen ship\n\n");

        if (error != NULL) {
            separate();
            printf("%s\n", error);
            separate();
        }

        printf("Put a ship on the main board please: ");

        fgets(buf_input, BUFFER_SIZE, stdin);

        printf("The user input is: %s", buf_input);

        if (!check_user_input(buf_input)) {
            error = "User input error!";
            continue;
        }
        else {
            error = NULL;

            int row1 = buf_input[2] - '0';
            int col1 = buf_input[1] - 'A';
            int row2 = buf_input[4] - '0';
            int col2 = buf_input[3] - 'A';

            char ship_letter = buf_input[0];

            switch (ship_letter) {
                case CARRIER_LETTER:
                    if (cq == 0) {
                        error = "Carrier not available";
                        continue;
                    }
                    break;
                case BATTLESHIP_LETTER:
                    if (bq == 0) {
                        error = "Battleship not available";
                        continue;
                    }
                    break;
                case SUBMARINE_LETTER:
                    if (sq == 0) {
                        error = "Submarines not available";
                        continue;
                    }
                    break;
                case DESTORYER_LETTER:
                    if (dq == 0) {
                        error = "Destroyers not available";
                        continue;
                    }
                    break;
                default:error = "Ship letter not correct";
                    break;
            }

            put_ship_on_main_board(row1, col1, row2, col2, ship_letter);

            switch (ship_letter) {
                case CARRIER_LETTER:cq--;
                    break;
                case BATTLESHIP_LETTER:bq--;
                    break;
                case SUBMARINE_LETTER:sq--;
                    break;
                case DESTORYER_LETTER:dq--;
                    break;
                default:break;
            }
            retrieve_a_ship(ship_letter);
        }

        if (cq == 0 && bq == 0 && sq == 0 && dq == 0) {
            printf("Set ships over");
            return;
        }
    }
}

void
clean_up_memory()
{
    for (int i = 0; i < NB_SHIPS; i++)
        if (ships[i] != NULL)
            free(ships[i]);
}

/**
 * @breif check if an IP address is valid
 * @param ipAddress
 * @return
 */
int
isValidIpAddress(char *ipAddress)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
    return result != 0;
}

int
main(int argc, char *argv[])
{

    pthread_t server;
    pthread_t client;
    void *data;

    initialize_board(main_board);
    initialize_board(mark_board);
    initialize_ships();

    print_board(mark_board, main_board);

    while (1) {
        printf("Choose from the following options:\n");
        printf("\t1. Randomly put ships on the main board.\n");
        printf("\t2. Manually put ships on the main board.\n");
        printf("\t\tYour choice: ");
        int choice;
        scanf("%d", &choice);
        if (choice == 1) {
            randomly_set_main_board();
            break;
        }
        else if (choice == 2) {
            manually_set_main_board();
            break;
        }
        else {
            printf("*****Please enter 1 or 2!*****\n");
            continue;
        }
    }

    printf("\n");

    print_board(main_board, mark_board);

    // Let the user specify the port on which to launch the server
    int port_here = 0;
    printf("Please specify the port to launch the server at localhost: ");
    scanf("%d", &port_here);
    here_port = port_here;


    // Launch server thread
    if (pthread_create(&server, NULL, server_thread, data)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }
    else {
        printf("Server launched at localhost:%d\n", port_here);
    }

    // Let the user specify the IP address to connect to
    char ip_buffer[20];
    char *ip_error = "IP address not valid!";

    while (1) {
        clear_input_buffer();
        printf("Please specify the IP address to which you want to connect: ");
        // Get user input (don't take line returns '\n')
        scanf("%[^\n]s", ip_buffer);
        if (isValidIpAddress(ip_buffer)) {
            printf("intput ip okay");
            break;
        }
        else {
            printf("Ip address not valid");
        }
        printf("\n");
        printf("%s\n", ip_error);
    }

    printf("The ip address entered is: %s\n", ip_buffer);
    there_ip_address = ip_buffer;

    // Let the user specify the port binded to the IP address above
    clear_input_buffer();
    int port_there = 0;
    printf("Please specify the port to which you want to connect: ");
    scanf("%d", &port_there);
    there_port = port_there;

    clear_input_buffer();
    char c;
    printf("Press any key to continue launch client\n");
    scanf("%c", &c);

    // Try to connect to the distant server
    if (pthread_create(&client, NULL, client_thread, data)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }
    else {
        printf("Connected to distant server on %s:%d\n", there_ip_address, there_port);
    }

    // Wait for the server thread to terminate
    if (pthread_join(server, NULL)) {
        fprintf(stderr, "Error joining thread\n");
        return 2;
    }

    // Wait for the client thread to terminate
    if (pthread_join(client, NULL)) {
        fprintf(stderr, "Error joining thread\n");
        return 2;
    }

    print_board(mark_board, main_board);
    printf("Game begins!\n");
    printf("Input attack instructions: ");

    clean_up_memory();
    return EXIT_SUCCESS;
}

#pragma clang diagnostic pop
