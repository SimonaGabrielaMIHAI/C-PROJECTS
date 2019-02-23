#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define BUFLEN 256
#define STDIN 0

typedef struct {
    char lstName[13];
    char fstName[13];
    long cardNr;
    long pin;
    char password[9];
    float sold;
    bool isConnected;
    bool isLocked;
    bool currentlyUnlock;
    int countFailLog;
    int sock;
} Account;

enum State {
    Connected, Login, Missing, Fail, Locked
};

void readString(FILE *fp, char *str) {
    int c;
    int i = 0;
    while ((c = fgetc(fp)) != ' ' && c != '\n') {
        str[i] = c;
        i++;
    }
    str[i] = '\0';
}

void readLine(FILE *fp, Account *acc) {
    readString(fp, acc->lstName);
    readString(fp, acc->fstName);
    fscanf(fp, "%li %li ", &acc->cardNr, &acc->pin);
    readString(fp, acc->password);
    fscanf(fp, "%f\n", &acc->sold);
    acc->isConnected = false;
    acc->isLocked = false;
    acc->countFailLog = 0;
}

//debugging
void printAccount(Account acc) {
    printf("%s %s %ld %ld %s %.2f \n", acc.lstName, acc.fstName, acc.cardNr,
           acc.pin, acc.password, acc.sold);
}

bool checkPresence(Account *acc, int length, long card) {
    int i;
    for (i = 0; i < length; i++) {
        if (acc[i].cardNr == card) {
            return true;
        }
    }
    return false;
}

Account searchAccountByCard(Account *acc, int length, long card) {
    int i;
    for (i = 0; i < length; i++) {
        if (acc[i].cardNr == card) {
            return acc[i];
        }
    }
}

Account searchAccount(Account *acc, int length, int sock) {
    int i;
    for (i = 0; i < length; i++) {
        if (acc[i].sock == sock) {
            return acc[i];
        }
    }
}

enum State loginAccount(Account **acc, int length, long card, long pin, int sockfd) {
    int i;
    for (i = 0; i < length; i++) {
        if ((*acc)[i].cardNr == card) {
            if ((*acc)[i].isConnected) return Connected;
            if ((*acc)[i].isLocked) return Locked;
            if ((*acc)[i].pin == pin) {
                (*acc)[i].isConnected = true;
                (*acc)[i].currentlyUnlock = false;
                (*acc)[i].sock = sockfd;
                (*acc)[i].countFailLog = 0;
                return Login;
            } else {
                (*acc)[i].countFailLog++;
                if ((*acc)[i].countFailLog == 3) {
                    (*acc)[i].countFailLog = 0;
                    (*acc)[i].isLocked = true;
                    return Locked;
                }
                return Fail;
            }
        }
    }
    return Missing;
}

void logoutAccount(Account **acc, int length, int sock) {
    int i;
    for (i = 0; i < length; i++) {
        if ((*acc)[i].sock == sock) {
            (*acc)[i].sock = -1;
            (*acc)[i].isConnected = false;
        }
    }
}

void transfer(Account **acc, int length, int sock, long card, long sum) {
    int i;
    for (i = 0; i < length; i++) {
        if ((*acc)[i].cardNr == card) {
            (*acc)[i].sold += sum;
        }
        if ((*acc)[i].sock == sock) {
            (*acc)[i].sold -= sum;
        }
    }
}

void isUnlocking(Account **acc, int length, long card, bool x) {
    int i;
    for (i = 0; i < length; i++) {
        if ((*acc)[i].cardNr == card) {
            (*acc)[i].currentlyUnlock = x;
        }
    }
}


bool changeUnlock(Account **acc, int length, long card, char *password) {
    int i;
    for (i = 0; i < length; i++) {
        if ((*acc)[i].cardNr == card) {
            if (strcmp((*acc)[i].password, password) == 0) {
                (*acc)[i].isLocked = false;
                return true;
            }
        }
    }
    return false;
}

void error(char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int sockfd, sockudp, newsockfd, portno, clilen;
    char buffer[BUFLEN];
    char *ptrCh;
    struct sockaddr_in serv_addr, cli_addr;
    struct sockaddr_in serv_udp_addr, cli_udp_addr;
    unsigned int udpSize = sizeof(cli_udp_addr);
    int n, i;
    long currentCard;
    long currentPin;
    enum State currentState;
    Account currentAccount;

    fd_set read_fds;    //multimea de citire folosita in select()
    fd_set tmp_fds;    //multime folosita temporar
    int fdmax;        //valoare maxima file descriptor din multimea read_fds

    if (argc < 3) {
        fprintf(stderr, "Usage %s port_server data_file\n", argv[0]);
        exit(1);
    }

    //formarea bazei de date
    FILE *fp = fopen(argv[2], "r");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Can't open %s file\n", argv[2]);
        exit(1);
    }

    int MAX_CLIENTS;
    fscanf(fp, "%d\n", &MAX_CLIENTS);
    Account *database = (Account *) calloc(MAX_CLIENTS, sizeof(Account));

    for (i = 0; i < MAX_CLIENTS; i++) {
        readLine(fp, &database[i]);
    }

    fclose(fp);

    //golim multimea de descriptori de citire (read_fds) si multimea tmp_fds
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockudp = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0)
        error("ERROR opening TCP socket");
    if (sockudp < 0)
        error("ERROR opening UDP socket");

    portno = atoi(argv[1]);

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    memset((char *) &serv_udp_addr, 0, sizeof(serv_udp_addr));
    serv_udp_addr.sin_family = AF_INET;
    serv_udp_addr.sin_addr.s_addr = INADDR_ANY;
    serv_udp_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
        error("ERROR on TCP binding");
    if (bind(sockudp, (struct sockaddr *) &serv_udp_addr, sizeof(struct sockaddr)) < 0)
        error("ERROR on UDP binding");

    listen(sockfd, MAX_CLIENTS);

    //adaugam noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
    FD_SET(STDIN, &read_fds);
    FD_SET(sockfd, &read_fds);
    FD_SET(sockudp, &read_fds);
    fdmax = sockfd + 1;


    // main loop
    while (1) {
        tmp_fds = read_fds;
        if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
            error("ERROR in select");

        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds)) {
                if (i == STDIN) {
                    //mesaj de la consola
                    fgets(buffer, BUFLEN, stdin);
                    if (strncmp(buffer, "quit", 4) == 0) {
                        for (i = 5; i <= fdmax; i++) {
                            if (i != sockudp && i != STDIN) {
                                send(i, "quit", sizeof(buffer), 0);
                            }
                            close(i);
                        }
                        free(database);
                        return 0;
                    }
                }
                if (i == sockfd) {
                    // a venit ceva pe socketul inactiv(cel cu listen) = o noua conexiune
                    // actiunea serverului: accept()
                    clilen = sizeof(cli_addr);
                    if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) == -1) {
                        error("ERROR in accept");
                    } else {
                        //adaug noul socket intors de accept() la multimea descriptorilor de citire
                        FD_SET(newsockfd, &read_fds);
                        if (newsockfd > fdmax) {
                            fdmax = newsockfd;
                        }
                    }
                    printf("Noua conexiune de la %s, port %d, socket_client %d\n", inet_ntoa(cli_addr.sin_addr),
                           ntohs(cli_addr.sin_port), newsockfd);
                } else if (i == sockudp) {
                    //mesaj pe UDP
                    memset(buffer, 0, BUFLEN);
                    n = recvfrom(sockudp, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_udp_addr, &udpSize);
                    if (n < 0)
                        error("ERROR rcv UDP from client");
                    printf("Am primit de la clientul de pe socketul UDP mesajul: %s\n", buffer);

                    if (strncmp(buffer, "unlock", 6) == 0) {
                        ptrCh = strtok(buffer, " ");    //login
                        ptrCh = strtok(NULL, " ");    //nr card
                        currentCard = atoi(ptrCh);
                        if (!checkPresence(database, MAX_CLIENTS, currentCard)) {
                            memset(buffer, 0, BUFLEN);
                            strncpy(buffer, "-4 : Numar card inexistent", 26);
                            n = sendto(sockudp, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_udp_addr,
                                       sizeof(cli_udp_addr));
                            if (n < 0)
                                error("ERROR sending UDP msg to client <unlock>");
                        } else {
                            currentAccount = searchAccountByCard(database, MAX_CLIENTS, currentCard);
                            if (!currentAccount.isLocked) {
                                memset(buffer, 0, BUFLEN);
                                strncpy(buffer, "-6 : Operatie esuata", 20);
                                n = sendto(sockudp, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_udp_addr,
                                           sizeof(cli_udp_addr));
                                if (n < 0)
                                    error("ERROR sending UDP msg to client <unlock>");
                            } else if (currentAccount.currentlyUnlock) {
                                memset(buffer, 0, BUFLEN);
                                strncpy(buffer, "-7 : Deblocare esuata", 21);
                                n = sendto(sockudp, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_udp_addr,
                                           sizeof(cli_udp_addr));
                                if (n < 0)
                                    error("ERROR sending UDP msg to client <unlock>");
                            } else {
                                isUnlocking(&database, MAX_CLIENTS, currentCard, true);
                                memset(buffer, 0, BUFLEN);
                                strncpy(buffer, "Trimite parola secreta", 22);
                                n = sendto(sockudp, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_udp_addr,
                                           sizeof(cli_udp_addr));
                                if (n < 0)
                                    error("ERROR sending UDP msg to client <unlock>");
                            }
                        }
                    } else {
                        //se primeste parola secreta
                        ptrCh = strtok(buffer, " ");    //nr card
                        currentCard = atoi(ptrCh);
                        ptrCh = strtok(NULL, " ");       //parola
                        char *password = (char *) calloc(strlen(ptrCh), sizeof(Account));
                        memcpy(password, ptrCh, strlen(ptrCh));
                        password[strlen(ptrCh)] = '\0';
                        memset(buffer, 0, BUFLEN);
                        if (changeUnlock(&database, MAX_CLIENTS, currentCard, password)) {
                            strncpy(buffer, "Card deblocat", 13);
                            n = sendto(sockudp, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_udp_addr,
                                       sizeof(cli_udp_addr));
                            if (n < 0)
                                error("ERROR sending UDP msg to client <password>");
                        } else {
                            isUnlocking(&database, MAX_CLIENTS, currentCard, false);
                            strncpy(buffer, "-7 : Deblocare esuata", 21);
                            n = sendto(sockudp, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_udp_addr,
                                       sizeof(cli_udp_addr));
                            if (n < 0)
                                error("ERROR sending UDP msg to client <password>");
                        }
                        free(password);

                    }

                } else {
                    // am primit date pe unul din socketii cu care vorbesc cu clientii
                    //actiunea serverului: recv()
                    memset(buffer, 0, BUFLEN);
                    if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
                        if (n == 0) {
                            //conexiunea s-a inchis
                            printf("server: socket %d hung up\n", i);
                        } else {
                            error("ERROR in recv <hung up>");
                        }
                        close(i);
                        FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul
                    } else { //recv intoarce > 0
                        printf("Am primit de la clientul de pe socketul %d, mesajul: %s\n", i, buffer);
                        if (strncmp(buffer, "login", 5) == 0) {
                            ptrCh = strtok(buffer, " ");    //login
                            ptrCh = strtok(NULL, " ");       //nr card
                            currentCard = atoi(ptrCh);
                            ptrCh = strtok(NULL, " ");       //pin
                            currentPin = atoi(ptrCh);
                            currentState = loginAccount(&database, MAX_CLIENTS, currentCard, currentPin, i);

                            memset(buffer, 0, BUFLEN);
                            if (currentState == Missing) {
                                strncpy(buffer, "-4 : Numar card inexistent", 26);
                                n = send(i, buffer, sizeof(buffer), 0);
                                if (n < 0)
                                    error("ERROR sending msg to client <login>");
                            } else if (currentState == Fail) {
                                strncpy(buffer, "-3 : Pin gresit", 15);
                                n = send(i, buffer, sizeof(buffer), 0);
                                if (n < 0)
                                    error("ERROR sending msg to client <login>");
                            } else if (currentState == Connected) {
                                strncpy(buffer, "-2 : Sesiune deja deschisa", 26);
                                n = send(i, buffer, sizeof(buffer), 0);
                                if (n < 0)
                                    error("ERROR sending msg to client <login>");
                            } else if (currentState == Locked) {
                                strncpy(buffer, "-5 : Card blocat", 16);
                                n = send(i, buffer, sizeof(buffer), 0);
                                if (n < 0)
                                    error("ERROR sending msg to client <login>");
                            } else if (currentState == Login) {
                                currentAccount = searchAccountByCard(database, MAX_CLIENTS, currentCard);
                                strcpy(buffer, "Welcome ");
                                strcat(buffer, currentAccount.lstName);
                                strcat(buffer, " ");
                                strcat(buffer, currentAccount.fstName);
                                n = send(i, buffer, sizeof(buffer), 0);
                                if (n < 0)
                                    error("ERROR sending msg to client <login>");
                            }
                        } else if (strncmp(buffer, "logout", 6) == 0) {
                            logoutAccount(&database, MAX_CLIENTS, i);
                            memset(buffer, 0, BUFLEN);
                            strncpy(buffer, "Clientul a fost deconectat", 26);
                            n = send(i, buffer, sizeof(buffer), 0);
                            if (n < 0)
                                error("ERROR sending msg to client <logout>");
                        } else if (strncmp(buffer, "listsold", 8) == 0) {
                            currentAccount = searchAccount(database, MAX_CLIENTS, i);
                            memset(buffer, 0, BUFLEN);
                            n = snprintf(buffer, sizeof(buffer), "%.2f", (double) currentAccount.sold);
                            if (n < 0)
                                error("ERROR in conversion");
                            n = send(i, buffer, sizeof(buffer), 0);
                            if (n < 0)
                                error("ERROR sending msg to client <listsold>");
                        } else if (strncmp(buffer, "transfer", 8) == 0) {
                            ptrCh = strtok(buffer, " ");    //transfer
                            ptrCh = strtok(NULL, " ");       //nr card
                            currentCard = atoi(ptrCh);
                            ptrCh = strtok(NULL, " ");      //suma
                            currentPin = atoi(ptrCh);       //reciclez variabila de pin xD

                            //verificarea cardului
                            if (checkPresence(database, MAX_CLIENTS, currentCard)) {
                                currentAccount = searchAccount(database, MAX_CLIENTS, i);
                                memset(buffer, 0, BUFLEN);
                                if (currentAccount.sold >= currentPin) {
                                    currentAccount = searchAccountByCard(database, MAX_CLIENTS, currentCard);
                                    snprintf(buffer, sizeof(buffer), "Transfer %.2f", (double) currentPin);
                                    strcat(buffer, " catre ");
                                    strcat(buffer, currentAccount.lstName);
                                    strcat(buffer, " ");
                                    strcat(buffer, currentAccount.fstName);
                                    strcat(buffer, " ? [y/n]");
                                    n = send(i, buffer, sizeof(buffer), 0);
                                    if (n < 0)
                                        error("ERROR sending msg to client <transfer>");
                                } else {
                                    strncpy(buffer, "-8 : Fonduri insuficiente", 25);
                                    n = send(i, buffer, sizeof(buffer), 0);
                                    if (n < 0)
                                        error("ERROR sending msg to client <transfer>");
                                }

                            } else {
                                memset(buffer, 0, BUFLEN);
                                strncpy(buffer, "-4 : Numar card inexistent", 26);
                                n = send(i, buffer, sizeof(buffer), 0);
                                if (n < 0)
                                    error("ERROR sending msg to client <transfer>");
                            }
                        } else if (strncmp(buffer, "confirm", 7) == 0) {
                            ptrCh = strtok(buffer, " ");    //confirm
                            ptrCh = strtok(NULL, " ");       //nr card
                            currentCard = atoi(ptrCh);
                            ptrCh = strtok(NULL, " ");       //suma
                            currentPin = atoi(ptrCh);
                            transfer(&database, MAX_CLIENTS, i, currentCard, currentPin);
                            memset(buffer, 0, BUFLEN);
                            strncpy(buffer, "Transfer realizat cu succes", 27);
                            n = send(i, buffer, sizeof(buffer), 0);
                            if (n < 0)
                                error("ERROR sending msg to client <confirm>");
                        }
                    }

                }
            }
        }
    }
}