#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <zconf.h>
#include <stdbool.h>

#define BUFLEN 256
#define STDIN 0

enum Type {
    IBANK, CLIENT, UNLOCK, TRANSFER
};


void writeMsgFrom(FILE *fp, char *command, char *answer, enum Type x) {
    if (x == IBANK) {
        printf("IBANK> %s\n\n", answer);
        fwrite(command, 1, strlen(command), fp);
        fwrite("\n", 1, 1, fp);
        fwrite("IBANK> ", 1, 7, fp);
        fwrite(answer, 1, strlen(answer), fp);
        fwrite("\n", 1, 1, fp);
        return;
    }
    if (x == CLIENT) {
        printf("%s\n\n", answer);
        fwrite(command, 1, strlen(command), fp);
        fwrite("\n", 1, 1, fp);
        fwrite(answer, 1, strlen(answer), fp);
        fwrite("\n", 1, 1, fp);
        return;
    }
    if (x == UNLOCK) {
        printf("UNLOCK> %s\n", answer);
        fwrite(command, 1, strlen(command), fp);
        fwrite("\n", 1, 1, fp);
        fwrite("UNLOCK> ", 1, 7, fp);
        fwrite(answer, 1, strlen(answer), fp);
        fwrite("\n", 1, 1, fp);
        return;
    }
    if (x == TRANSFER) {
        printf("IBANK> %s\n", answer);
        fwrite(command, 1, strlen(command), fp);
        fwrite("\n", 1, 1, fp);
        fwrite("IBANK> ", 1, 7, fp);
        fwrite(answer, 1, strlen(answer), fp);
        fwrite("\n", 1, 1, fp);
        return;
    }
}

void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[]) {
    int sockfd, sockudp, n, i;
    struct sockaddr_in serv_addr;
    struct sockaddr_in serv_udp_addr;
    unsigned int udpSize = sizeof(serv_udp_addr);
    fd_set read_fds;    //multimea de citire folosita in select()
    fd_set tmp_fds;    //multime folosita temporar
    int fdmax;        //valoare maxima file descriptor din multimea read_fds

    char buffer[BUFLEN];
    char rcvBuffer[BUFLEN];
    if (argc < 3) {
        fprintf(stderr, "Usage %s IP_server port_server\n", argv[0]);
        exit(0);
    }

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockudp = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening TCP socket");
    if (sockudp < 0)
        error("ERROR opening UDP socket");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_addr.sin_addr);

    serv_udp_addr.sin_family = AF_INET;
    serv_udp_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_udp_addr.sin_addr);


    //fisierul clientului
    char logfile[30] = "client-";
    char pid[10];
    sprintf(pid, "%d", getpid());
    strcat(logfile, pid);
    strcat(logfile, ".log");

    printf("File log: %s\n", logfile);
    FILE *fp = fopen(logfile, "w");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Can't open %s file\n", logfile);
        exit(1);
    }

    FD_SET(STDIN, &read_fds);
    FD_SET(sockfd, &read_fds);
    fdmax = sockfd;
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting TCP");

    //VARIABILELE CLIENTULUI
    bool isLogged = false;
    char *ptrCh = NULL;
    long card, sum;
    char copyCard[50];
    char copyBuff[50];

    while (1) {
        tmp_fds = read_fds;
        if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
            error("ERROR in select");
        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds) && i == STDIN) {
                //input de la consola
                memset(buffer, 0, BUFLEN);
                memset(rcvBuffer, 0, BUFLEN);
                fgets(buffer, BUFLEN - 1, stdin);

                buffer[strlen(buffer) - 1] = '\0';

                if (strncmp(buffer, "login", 5) == 0) {
                    if (isLogged) {
                        strncpy(rcvBuffer, "-2 : Sesiune deja deschisa", 26);
                        writeMsgFrom(fp, buffer, rcvBuffer, IBANK);
                    } else {

                        n = send(sockfd, buffer, sizeof(buffer), 0);
                        if (n < 0)
                            error("ERROR in send <login>");
                        n = recv(sockfd, rcvBuffer, sizeof(rcvBuffer), 0);
                        if (n < 0)
                            error("ERROR in recv <login>");
                        if (strncmp(rcvBuffer, "Welcome", 6) == 0) {
                            isLogged = true;
                        }
                        writeMsgFrom(fp, buffer, rcvBuffer, IBANK);
                        ptrCh = strtok(buffer, " ");    //login
                        ptrCh = strtok(NULL, " ");       //nr card
                        strcpy(copyCard, ptrCh);
                    }
                } else if (strncmp(buffer, "logout", 6) == 0) {
                    if (isLogged) {
                        n = send(sockfd, buffer, sizeof(buffer), 0);
                        if (n < 0)
                            error("ERROR in send <logout>");
                        n = recv(sockfd, rcvBuffer, sizeof(rcvBuffer), 0);
                        if (n < 0)
                            error("ERROR in recv <logout>");
                        isLogged = false;
                        writeMsgFrom(fp, buffer, rcvBuffer, IBANK);
                    } else {
                        strncpy(rcvBuffer, "-1 : Clientul nu este autentificat", 34);
                        writeMsgFrom(fp, buffer, rcvBuffer, CLIENT);
                    }
                } else if (strncmp(buffer, "listsold", 8) == 0) {
                    if (isLogged) {
                        n = send(sockfd, buffer, sizeof(buffer), 0);
                        if (n < 0)
                            error("ERROR in send <listsold>");
                        n = recv(sockfd, rcvBuffer, sizeof(rcvBuffer), 0);
                        if (n < 0)
                            error("ERROR in recv <listsold>");
                        writeMsgFrom(fp, buffer, rcvBuffer, IBANK);

                    } else {
                        strncpy(rcvBuffer, "-1 : Clientul nu este autentificat", 34);
                        writeMsgFrom(fp, buffer, rcvBuffer, CLIENT);
                    }
                } else if (strncmp(buffer, "transfer", 8) == 0) {
                    if (isLogged) {

                        n = send(sockfd, buffer, sizeof(buffer), 0);
                        if (n < 0)
                            error("ERROR in send <transfer>");
                        n = recv(sockfd, rcvBuffer, sizeof(rcvBuffer), 0);
                        if (n < 0)
                            error("ERROR in recv <transfer>");
                        if (strncmp(rcvBuffer, "Transfer", 8) != 0) {
                            writeMsgFrom(fp, buffer, rcvBuffer, IBANK);
                        } else {
                            writeMsgFrom(fp, buffer, rcvBuffer, TRANSFER);
                            ptrCh = strtok(buffer, " ");    //transfer
                            ptrCh = strtok(NULL, " ");       //nr card
                            card = atoi(ptrCh);
                            ptrCh = strtok(NULL, " ");      //suma
                            sum = atoi(ptrCh);
                            snprintf(rcvBuffer, sizeof(buffer), "confirm %lu %lu", card, sum);

                            memset(buffer, 0, BUFLEN);
                            fgets(buffer, BUFLEN - 1, stdin);
                            buffer[strlen(buffer) - 1] = '\0';

                            if (strncmp(buffer, "y", 1) == 0) {
                                n = send(sockfd, rcvBuffer, sizeof(rcvBuffer), 0);
                                if (n < 0)
                                    error("ERROR in send <confirm>");
                                n = recv(sockfd, rcvBuffer, sizeof(rcvBuffer), 0);
                                writeMsgFrom(fp, buffer, rcvBuffer, IBANK);
                            } else {
                                writeMsgFrom(fp, buffer, "-9 : Operatie anulata", IBANK);
                            }
                        }

                    } else {
                        strncpy(rcvBuffer, "-1 : Clientul nu este autentificat", 34);
                        writeMsgFrom(fp, buffer, rcvBuffer, CLIENT);
                    }
                } else if (strncmp(buffer, "unlock", 8) == 0) {
                    memset(buffer, 0, BUFLEN);
                    strncpy(buffer, "unlock ", 7);
                    strcat(buffer, copyCard);
                    n = sendto(sockudp, buffer, sizeof(buffer), 0, (struct sockaddr *) &serv_udp_addr,
                               sizeof(serv_udp_addr));
                    if (n < 0)
                        error("ERROR in UDP send <unlock>");
                    n = recvfrom(sockudp, rcvBuffer, sizeof(buffer), 0,
                                 (struct sockaddr *) &serv_udp_addr, &udpSize);
                    if (n < 0)
                        error("ERROR in UDP send <unlock>");
                    writeMsgFrom(fp, buffer, rcvBuffer, UNLOCK);
                    if (strncmp(rcvBuffer, "Trimite", 7) == 0) {
                        memset(buffer, 0, BUFLEN);
                        strcpy(copyBuff, "");
                        fgets(buffer, BUFLEN - 1, stdin);
                        buffer[strlen(buffer) - 1] = '\0';

                        strcat(copyBuff, copyCard);
                        strcat(copyBuff, " ");
                        strcat(copyBuff, buffer);


                        n = sendto(sockudp, copyBuff, sizeof(buffer), 0,
                                   (struct sockaddr *) &serv_udp_addr,
                                   sizeof(serv_udp_addr));
                        if (n < 0)
                            error("ERROR in UDP send <password>");
                        n = recvfrom(sockudp, rcvBuffer, sizeof(buffer), 0,
                                     (struct sockaddr *) &serv_udp_addr,
                                     &udpSize);
                        if (n < 0)
                            error("ERROR in UDP send <password>");
                        writeMsgFrom(fp, buffer, rcvBuffer, UNLOCK);
                        printf("\n");
                    } else printf("\n");
                } else if (strncmp(buffer, "quit", 4) == 0) {
                    n = send(sockfd, "logout", sizeof(buffer), 0);
                    if (n < 0)
                        error("ERROR in send <quit>");
                    n = recv(sockfd, buffer, sizeof(buffer), 0);
                    if (n < 0)
                        error("ERROR in recv <quit>");
                    close(sockfd);
                    fclose(fp);
                    return 0;
                }

            } else if (FD_ISSET(i, &tmp_fds) && i == sockfd) {
                //mesaj de la server
                if (i == sockfd)
                    memset(rcvBuffer, 0, BUFLEN);
                n = recv(sockfd, rcvBuffer, sizeof(rcvBuffer), 0);
                if (n < 0)
                    error("ERROR in recv msg from server");
                if (strncmp(rcvBuffer, "quit", 4) == 0) {
                    printf("Server disconnected\n");
                    close(sockfd);
                    fclose(fp);
                    return 0;
                }
            }
        }

    }

}


