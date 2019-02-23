#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include "resource.h"

int usage(char* name)
{
    printf("Usage:\t%s\t<NAME/IP>\t<TYPE>\n", name);
    return 1;
}

int getType(char *type) {
    if(strcmp(type, "A") == 0) {
        return A;
    }
    if(strcmp(type, "NS") == 0) {
        return NS;
    }
    if(strcmp(type, "CNAME") == 0) {
        return CNAME;
    }
    if(strcmp(type, "MX") == 0) {
        return MX;
    }
    if(strcmp(type, "SOA") == 0) {
        return SOA;
    }
    if(strcmp(type, "TXT") == 0) {
        return TXT;
    }
    return 0;
}

char* getSymbol(int type) {
    if(type == A) {
        return "A";
    }
    if(type == NS) {
        return "NS";
    }
    if(type == CNAME) {
        return "CNAME";
    }
    if(type == MX) {
        return "MX";
    }
    if(type == SOA) {
        return "SOA";
    }
    if(type == TXT) {
        return "TXT";
    }
    return NULL;
}

bool checkLine(char *buf) {
    if(strncmp(buf, "#", 1) == 0) {
        return false;
    }
    if(strncmp(buf, " ", 1) == 0) {
        return false;
    }
    if(strncmp(buf, "\n", 1) == 0) {
        return false;
    }
    return true;
}

void compressionReverse(int start, char *rcv,
                        int* size, char *result) {
    unsigned int skip = 0, nameCount = 0;
    unsigned char var;
    int index = start;
    bool flag = false;
    *size = 0;
    while(rcv[index] != 0) {
        var = (unsigned char) rcv[index];
        skip = 0;
        //11000000
        if(var >= 192){
            //este pointer, trebuie sa se sara
            //2^8 = 256
            //11000000 00000000
            index = (unsigned int) (var * 256 + (unsigned char) rcv[index + 1] - (192<<8));
            skip = 1;
            flag = true;
        }
        else {
            result[nameCount] = rcv[index];
            nameCount++;
        }
        if(skip == 0) {
            if(!flag) {
                //nu s-a intalnit pointer inca
                (*size) = (*size) + 1;
            }
            index++;
        }
    }
    if(flag) {
        (*size) = (*size) + 2;
    }
    result[nameCount] = '\0';
    if(skip == 1) {
        (*size) = (*size) + 1;
    }
    int nr = result[0]; //nr de caractere pana la urmatorul nr
    int i, current = 0;
    //se pun punctele
    while(nr != 0) {
        for(i = 0; i < nr; i++) {
            result[current] = result[current + 1];
            current++;
        }
        result[current] = '.';
        current++;
        nr = result[current];
    }
    result[current] = '\0';

}

void error(char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        return usage(argv[0]);
    }

    int TYPE = getType(argv[2]);
    if(strncmp(argv[2], "PTR", 3) == 0) {
        printf("Neimplementat\n");
        exit(0);
    }
    int ret, answers;
    unsigned char ip[4];
    int recvIndex;
    char buf[BUFLEN];
    char sender[4 * BUFLEN];    //aloc adresa mesajului de trimis
    char receiver[4 * BUFLEN];
    char* host = malloc((strlen(argv[1]) + 1) * sizeof(char));
    char* hostRcv = malloc(BUFLEN * sizeof(char));
    char* nameServer = malloc(BUFLEN * sizeof(char));
    memcpy(host, argv[1], strlen(argv[1]));
    strcat(host, ".");
    char* server = NULL;
    bool flag = false;

    //Structures
    dns_rr_t* reply[MAXREPLY];

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    //*SENDER*
    //dimensiunea fara lungimea numelui
    int sizeMsg = sizeof(dns_header_t) + sizeof(dns_question_t);
    dns_header_t* dnsHeader = (dns_header_t*) &sender;

    dnsHeader->id = htons(1);
    dnsHeader->rd = 1;
    dnsHeader->tc = 0;
    dnsHeader->aa = 0;
    dnsHeader->opcode = 0;
    dnsHeader->qr = 0;
    dnsHeader->rcode = 0;
    dnsHeader->z = 0;
    dnsHeader->ra = 0;
    dnsHeader->qdcount = htons(1);
    dnsHeader->ancount = 0;
    dnsHeader->nscount = 0;
    dnsHeader->arcount = 0;

    //se continua "append-ul" de unde se termina structura precedenta
    unsigned char* qname = (unsigned char*) &sender[sizeof(dns_header_t)];
    memset(qname, 0, strlen(host));

    //numara caracterele pana la '.' si ataseaza valoarea
    int count = 0, i, k = 0, j;
    for(i = 0; i < strlen(host); i++) {
        if(host[i] == '.') {
            qname[k] = (unsigned char) (i - count);
            k++;
            for(; count < i; count++) {
                qname[k] = (unsigned char) host[count];
                k++;
            }
            count++;
        }
    }
    qname[k]= '\0';
    sizeMsg = (int) (sizeMsg + strlen((const char *) qname) + 1);

    dns_question_t* dnsQuestion = (dns_question_t*)
            &sender[sizeMsg - sizeof(dns_question_t)];

    dnsQuestion->qclass = htons(1);
    dnsQuestion->qtype = htons((uint16_t) TYPE);

    //UDP
    int sockudp;
    struct sockaddr_in serv_udp_addr;
    fd_set read_fds;
    unsigned int udpSize = sizeof(serv_udp_addr);

    //FILES
    FILE *servfile, *logfile, *msg;
    servfile = fopen("dns_servers.conf", "r");
    logfile = fopen("dns.log", "a");
    msg = fopen("message.log", "w");

    for(i = 0; i < sizeMsg; i++) {
        fprintf(msg, "%x ", sender[i]);
    }

    if(servfile == NULL || logfile == NULL)
        error("Error in files\n");

    while(fgets(buf, sizeof(buf), servfile)!= NULL) {
        if(!checkLine(buf)) {
            continue;
        }
        flag = false;
        //se ia numele serverului (pana la space sau newline)
        server = strtok(buf, " \n");
        printf("SERVER %s\n", server);

        //conectare la server DNS
        sockudp = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockudp < 0)
            error("ERROR opening UDP socket\n");
        memset((char *) &serv_udp_addr, 0, sizeof(serv_udp_addr));
        serv_udp_addr.sin_family = AF_INET;
        serv_udp_addr.sin_addr.s_addr = inet_addr(server);
        serv_udp_addr.sin_port = htons(PORT);

        ret = (int) sendto(sockudp, &sender, (size_t) sizeMsg, 0, (struct sockaddr *) &serv_udp_addr,
                           sizeof(serv_udp_addr));
        if (ret < 0)
            error("ERROR sending UDP msg to client");
        printf("Se trimit %d\n", ret);

        FD_ZERO(&read_fds);
        FD_SET(sockudp, &read_fds);
        ret = select(sockudp + 1, &read_fds, NULL, NULL, &timeout);
        if (ret == -1)
            error("ERROR in select");
        if (ret == 0) {
            printf("Timeout\n");
            continue;
        }
        if (FD_ISSET(sockudp, &read_fds)) {
            ret = (int) recvfrom(sockudp, &receiver, sizeof(receiver), 0, (struct sockaddr *) &serv_udp_addr,
                                 &udpSize);
            if (ret < 0)
                error("ERROR rcv UDP msg from client");
            printf("Se primesc %d\n", ret);

            dnsHeader = (dns_header_t*) &receiver;

            fprintf(logfile, "; %s - %s %s\n\n", server, argv[1], argv[2]);

            answers = ntohs(dnsHeader->ancount);
            if(answers == 0) {
                printf("Answer not found\n");
                flag = true;
            }
            if (!flag) {
                //avem raspuns si scriem in fisier YAY
                fprintf(logfile, ";; ANSWER SECTION:\n");
                recvIndex = sizeMsg;

                //se parcurg toate raspunsurile
                for(i = 0; i < answers; i++) {
                    memset(nameServer, 0, BUFLEN);
                    memset(hostRcv, 0, BUFLEN);
                    compressionReverse(recvIndex, receiver, &count, hostRcv);
                    recvIndex += count;
                    reply[i] = (dns_rr_t*) &receiver[recvIndex];
                    recvIndex += sizeof(dns_rr_t) - 2;
                    if(ntohs(reply[i]->type) == A) {
                        for (j = 0; j < 4; j++) {
                            ip[j] = (unsigned char) receiver[recvIndex];
                            recvIndex++;
                        }
                        fprintf(logfile, "%s IN %s %d.%d.%d.%d\n", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), ip[0], ip[1], ip[2], ip[3]);
                    }
                    else if(ntohs(reply[i]->type) == NS) {
                        compressionReverse(recvIndex, receiver, &count, nameServer);
                        recvIndex += count;
                        fprintf(logfile, "%s IN %s %s\n", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), nameServer);
                    }
                    else if(ntohs(reply[i]->type) == MX) {
                        ret = (unsigned char) receiver[recvIndex + 1];
                        recvIndex += 2;
                        compressionReverse(recvIndex, receiver, &count, nameServer);
                        recvIndex += count;
                        fprintf(logfile, "%s IN %s %d %s\n", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), ret, nameServer);
                    }
                    else if(ntohs(reply[i]->type) == CNAME) {
                        compressionReverse(recvIndex, receiver, &count, nameServer);
                        recvIndex += count;
                        fprintf(logfile, "%s IN %s %s\n", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), nameServer);
                    }
                    else if(ntohs(reply[i]->type) == SOA) {
                        compressionReverse(recvIndex, receiver, &count, nameServer);
                        recvIndex += count;
                        fprintf(logfile, "%s IN %s %s ", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), nameServer);
                        memset(nameServer, 0, BUFLEN);
                        compressionReverse(recvIndex, receiver, &count, nameServer);
                        recvIndex += count;
                        ret = ((unsigned char) receiver[recvIndex] * (1<<24) +
                               (unsigned char) receiver[recvIndex + 1] * (1<<16) +
                               (unsigned char) receiver[recvIndex + 2] * (1<<8) +
                               (unsigned char) receiver[recvIndex + 3]);
                        recvIndex += 4;
                        fprintf(logfile, "%s %d ", nameServer, ret);
                        ret = ((unsigned char) receiver[recvIndex] * (1<<24) +
                               (unsigned char) receiver[recvIndex + 1] * (1<<16) +
                               (unsigned char) receiver[recvIndex + 2] * (1<<8) +
                               (unsigned char) receiver[recvIndex + 3]);
                        recvIndex += 4;
                        fprintf(logfile, "%d ", ret);
                        ret = ((unsigned char) receiver[recvIndex] * (1<<24) +
                               (unsigned char) receiver[recvIndex + 1] * (1<<16) +
                               (unsigned char) receiver[recvIndex + 2] * (1<<8) +
                               (unsigned char) receiver[recvIndex + 3]);
                        recvIndex += 4;
                        fprintf(logfile, "%d ", ret);
                        ret = ((unsigned char) receiver[recvIndex] * (1<<24) +
                               (unsigned char) receiver[recvIndex + 1] * (1<<16) +
                               (unsigned char) receiver[recvIndex + 2] * (1<<8) +
                               (unsigned char) receiver[recvIndex + 3]);
                        recvIndex += 4;
                        fprintf(logfile, "%d ", ret);
                        ret = ((unsigned char) receiver[recvIndex] * (1<<24) +
                               (unsigned char) receiver[recvIndex + 1] * (1<<16) +
                               (unsigned char) receiver[recvIndex + 2] * (1<<8) +
                               (unsigned char) receiver[recvIndex + 3]);
                        recvIndex += 4;
                        fprintf(logfile, "%d \n", ret);
                    }
                    else if(ntohs(reply[i]->type) == TXT) {
                        fprintf(logfile, "%s IN %s %s ", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), nameServer);
                        ret = (unsigned char) receiver[recvIndex];
                        recvIndex++;
                        for(j = 0; j < ret; j++) {
                            fprintf(logfile, "%c", (unsigned char) receiver[recvIndex]);
                            recvIndex++;
                        }
                        fprintf(logfile, "\n");
                    }
                }
            }
            flag = false;

//=============================================================================
            //“Authority” si “Additional" -- srry pt copy paste
            answers = ntohs(dnsHeader->nscount);
            if(answers == 0) {
                printf("Authority not found\n");
                flag = true;
            }
            if (!flag) {
                //avem raspuns si scriem in fisier YAY
                fprintf(logfile, ";; AUTHORITY SECTION:\n");
                recvIndex = sizeMsg;

                //se parcurg toate raspunsurile
                for(i = 0; i < answers; i++) {
                    memset(nameServer, 0, BUFLEN);
                    memset(hostRcv, 0, BUFLEN);
                    compressionReverse(recvIndex, receiver, &count, hostRcv);
                    recvIndex += count;
                    reply[i] = (dns_rr_t*) &receiver[recvIndex];
                    recvIndex += sizeof(dns_rr_t) - 2;
                    if(ntohs(reply[i]->type) == A) {
                        for (j = 0; j < 4; j++) {
                            ip[j] = (unsigned char) receiver[recvIndex];
                            recvIndex++;
                        }
                        fprintf(logfile, "%s IN %s %d.%d.%d.%d\n", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), ip[0], ip[1], ip[2], ip[3]);
                    }
                    else if(ntohs(reply[i]->type) == NS) {
                        compressionReverse(recvIndex, receiver, &count, nameServer);
                        recvIndex += count;
                        fprintf(logfile, "%s IN %s %s\n", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), nameServer);
                    }
                    else if(ntohs(reply[i]->type) == MX) {
                        ret = (unsigned char) receiver[recvIndex + 1];
                        recvIndex += 2;
                        compressionReverse(recvIndex, receiver, &count, nameServer);
                        recvIndex += count;
                        fprintf(logfile, "%s IN %s %d %s\n", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), ret, nameServer);
                    }
                    else if(ntohs(reply[i]->type) == CNAME) {
                        compressionReverse(recvIndex, receiver, &count, nameServer);
                        recvIndex += count;
                        fprintf(logfile, "%s IN %s %s\n", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), nameServer);
                    }
                    else if(ntohs(reply[i]->type) == SOA) {
                        compressionReverse(recvIndex, receiver, &count, nameServer);
                        recvIndex += count;
                        fprintf(logfile, "%s IN %s %s ", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), nameServer);
                        memset(nameServer, 0, BUFLEN);
                        compressionReverse(recvIndex, receiver, &count, nameServer);
                        recvIndex += count;
                        ret = ((unsigned char) receiver[recvIndex] * (1<<24) +
                               (unsigned char) receiver[recvIndex + 1] * (1<<16) +
                               (unsigned char) receiver[recvIndex + 2] * (1<<8) +
                               (unsigned char) receiver[recvIndex + 3]);
                        recvIndex += 4;
                        fprintf(logfile, "%s %d ", nameServer, ret);
                        ret = ((unsigned char) receiver[recvIndex] * (1<<24) +
                               (unsigned char) receiver[recvIndex + 1] * (1<<16) +
                               (unsigned char) receiver[recvIndex + 2] * (1<<8) +
                               (unsigned char) receiver[recvIndex + 3]);
                        recvIndex += 4;
                        fprintf(logfile, "%d ", ret);
                        ret = ((unsigned char) receiver[recvIndex] * (1<<24) +
                               (unsigned char) receiver[recvIndex + 1] * (1<<16) +
                               (unsigned char) receiver[recvIndex + 2] * (1<<8) +
                               (unsigned char) receiver[recvIndex + 3]);
                        recvIndex += 4;
                        fprintf(logfile, "%d ", ret);
                        ret = ((unsigned char) receiver[recvIndex] * (1<<24) +
                               (unsigned char) receiver[recvIndex + 1] * (1<<16) +
                               (unsigned char) receiver[recvIndex + 2] * (1<<8) +
                               (unsigned char) receiver[recvIndex + 3]);
                        recvIndex += 4;
                        fprintf(logfile, "%d ", ret);
                        ret = ((unsigned char) receiver[recvIndex] * (1<<24) +
                               (unsigned char) receiver[recvIndex + 1] * (1<<16) +
                               (unsigned char) receiver[recvIndex + 2] * (1<<8) +
                               (unsigned char) receiver[recvIndex + 3]);
                        recvIndex += 4;
                        fprintf(logfile, "%d \n", ret);
                    }
                    else if(ntohs(reply[i]->type) == TXT) {
                        fprintf(logfile, "%s IN %s %s ", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), nameServer);
                        ret = (unsigned char) receiver[recvIndex];
                        recvIndex++;
                        for(j = 0; j < ret; j++) {
                            fprintf(logfile, "%c", (unsigned char) receiver[recvIndex]);
                            recvIndex++;
                        }
                        fprintf(logfile, "\n");
                    }
                }
            }
            flag = false;

            answers = ntohs(dnsHeader->nscount);
            if(answers == 0) {
                printf("Additional not found\n");
                flag = true;
            }

            if (!flag) {
                //avem raspuns si scriem in fisier YAY
                fprintf(logfile, ";; ADDITIONAL SECTION:\n");
                recvIndex = sizeMsg;

                //se parcurg toate raspunsurile
                for(i = 0; i < answers; i++) {
                    memset(nameServer, 0, BUFLEN);
                    memset(hostRcv, 0, BUFLEN);
                    compressionReverse(recvIndex, receiver, &count, hostRcv);
                    recvIndex += count;
                    reply[i] = (dns_rr_t*) &receiver[recvIndex];
                    recvIndex += sizeof(dns_rr_t) - 2;
                    if(ntohs(reply[i]->type) == A) {
                        for (j = 0; j < 4; j++) {
                            ip[j] = (unsigned char) receiver[recvIndex];
                            recvIndex++;
                        }
                        fprintf(logfile, "%s IN %s %d.%d.%d.%d\n", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), ip[0], ip[1], ip[2], ip[3]);
                    }
                    else if(ntohs(reply[i]->type) == NS) {
                        compressionReverse(recvIndex, receiver, &count, nameServer);
                        recvIndex += count;
                        fprintf(logfile, "%s IN %s %s\n", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), nameServer);
                    }
                    else if(ntohs(reply[i]->type) == MX) {
                        ret = (unsigned char) receiver[recvIndex + 1];
                        recvIndex += 2;
                        compressionReverse(recvIndex, receiver, &count, nameServer);
                        recvIndex += count;
                        fprintf(logfile, "%s IN %s %d %s\n", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), ret, nameServer);
                    }
                    else if(ntohs(reply[i]->type) == CNAME) {
                        compressionReverse(recvIndex, receiver, &count, nameServer);
                        recvIndex += count;
                        fprintf(logfile, "%s IN %s %s\n", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), nameServer);
                    }
                    else if(ntohs(reply[i]->type) == SOA) {
                        compressionReverse(recvIndex, receiver, &count, nameServer);
                        recvIndex += count;
                        fprintf(logfile, "%s IN %s %s ", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), nameServer);
                        memset(nameServer, 0, BUFLEN);
                        compressionReverse(recvIndex, receiver, &count, nameServer);
                        recvIndex += count;
                        ret = ((unsigned char) receiver[recvIndex] * (1<<24) +
                               (unsigned char) receiver[recvIndex + 1] * (1<<16) +
                               (unsigned char) receiver[recvIndex + 2] * (1<<8) +
                               (unsigned char) receiver[recvIndex + 3]);
                        recvIndex += 4;
                        fprintf(logfile, "%s %d ", nameServer, ret);
                        ret = ((unsigned char) receiver[recvIndex] * (1<<24) +
                               (unsigned char) receiver[recvIndex + 1] * (1<<16) +
                               (unsigned char) receiver[recvIndex + 2] * (1<<8) +
                               (unsigned char) receiver[recvIndex + 3]);
                        recvIndex += 4;
                        fprintf(logfile, "%d ", ret);
                        ret = ((unsigned char) receiver[recvIndex] * (1<<24) +
                               (unsigned char) receiver[recvIndex + 1] * (1<<16) +
                               (unsigned char) receiver[recvIndex + 2] * (1<<8) +
                               (unsigned char) receiver[recvIndex + 3]);
                        recvIndex += 4;
                        fprintf(logfile, "%d ", ret);
                        ret = ((unsigned char) receiver[recvIndex] * (1<<24) +
                               (unsigned char) receiver[recvIndex + 1] * (1<<16) +
                               (unsigned char) receiver[recvIndex + 2] * (1<<8) +
                               (unsigned char) receiver[recvIndex + 3]);
                        recvIndex += 4;
                        fprintf(logfile, "%d ", ret);
                        ret = ((unsigned char) receiver[recvIndex] * (1<<24) +
                               (unsigned char) receiver[recvIndex + 1] * (1<<16) +
                               (unsigned char) receiver[recvIndex + 2] * (1<<8) +
                               (unsigned char) receiver[recvIndex + 3]);
                        recvIndex += 4;
                        fprintf(logfile, "%d \n", ret);
                    }
                    else if(ntohs(reply[i]->type) == TXT) {
                        fprintf(logfile, "%s IN %s %s ", hostRcv,
                                getSymbol(ntohs(reply[i]->type)), nameServer);
                        ret = (unsigned char) receiver[recvIndex];
                        recvIndex++;
                        for(j = 0; j < ret; j++) {
                            fprintf(logfile, "%c", (unsigned char) receiver[recvIndex]);
                            recvIndex++;
                        }
                        fprintf(logfile, "\n");
                    }
                }
            }
        }
    }
    fprintf(logfile, "\n\n");

    free(host);
    free(hostRcv);
    free(nameServer);
    fclose(servfile);
    fclose(logfile);
    fclose(msg);
    return 0;
}