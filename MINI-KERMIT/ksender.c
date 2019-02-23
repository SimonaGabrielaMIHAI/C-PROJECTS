#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10000


void waitMessageAndResend(msg** rcvPointer, msg* send) {
    (*rcvPointer) = receive_message_timeout(TIME);
    while( (*rcvPointer) == NULL) {
        switch (send->payload[3]) {
            case SEND :
                printf("[%d]Send-Init RESEND\n", send->payload[2]);
                break;
            case HEADER :
                printf("[%d]File Header RESEND\n", send->payload[2]);
                break;
            case DATA :
                printf("[%d]Data RESEND\n", send->payload[2]);
                break;
            case ENDF:
                printf("[%d]EOF RESEND\n", send->payload[2]);
                break;
            case ENDT:
                printf("[%d]EOT RESEND\n", send->payload[2]);
                break;
            default:
                printf("Something wrong in type %c \n", send->payload[3]);
        }

        send_message(send);
        (*rcvPointer) = receive_message_timeout(TIME);
    }
}


int main(int argc, char** argv) {
    msg t, r;
    msg* msgPointer = malloc(sizeof(msg));

    init(HOST, PORT);
    FILE *file;
    int currentFile = 1;
    int currentSeq = 0;
    int countData = 1;
    int filenameLen;
    int size;
    int readSize;
    char buffer[MAXL + 1];
    char myPayload[8 + MAXL];
    char soh = intToChar(0x01);
    char charLen;
    char charSeq;
    char msgType;
    char charMAXL = (char) (MAXL);
    char charTIME = intToChar(TIME);
    char charCrc[2];
    unsigned short crc;

    char NPAD = intToChar(0x00);
    char PADC = intToChar(0x00);
    char EOL = intToChar(0x0D);
    char QCTL = intToChar(0x00);
    char QBIN = intToChar(0x00);
    char CHKT = intToChar(0x00);
    char REPT = intToChar(0x00);
    char CAPA = intToChar(0x00);
    char R = intToChar(0x00);


    //======= SEND INIT =======//

    //formez pachetul de tip S
    charLen = (char) 16;
    msgType = SEND;
    //structura campului DATA pentru pachet de tip SEND
    //are dimens de 11 bytes, la care se adauga 5 bytes
    //de la celelalte campuri care urmeaza campului LEN

    t.payload[0] = soh;
    t.payload[1] = charLen;
    t.payload[3] = msgType;
    t.payload[4] = charMAXL;
    t.payload[5] = charTIME;
    t.payload[6] = NPAD;
    t.payload[7] = PADC;
    t.payload[8] = EOL;
    t.payload[9] = QCTL;
    t.payload[10] = QBIN;
    t.payload[11] = CHKT;
    t.payload[12] = REPT;
    t.payload[13] = CAPA;
    t.payload[14] = R;

    do {
        memset(&myPayload, 0, sizeof(myPayload));
        t.payload[2] = intToChar(currentSeq);

        //crc se face doar pe primii 15 bytes
        //restul sunt oricum 0 din memset
        memcpy(myPayload, t.payload, 15);
        myPayload[15] = '\0';

        //crc se face pe intervalul 0-15
        //inclusiv '\0'
        crc = crc16_ccitt(myPayload, 16);
        memcpy(charCrc, &crc, 2);

        t.payload[15] = charCrc[0];
        t.payload[16] = charCrc[1];
        t.payload[17] = EOL;
        t.payload[18] = '\0';
        t.len = (int) strlen(t.payload);

        printf("[%d]Send-Init\n", currentSeq);
        currentSeq = incrementSeq(currentSeq);
        send_message(&t);

        waitMessageAndResend(&msgPointer, &t);
        r = (*msgPointer);
    }while (r.payload[3] != ACK);


    //======= PENTRU FIECARE FISIER =======//

    while (currentFile < argc) {
        //setez string-urile pe 0
        memset(&t, 0, sizeof(msg));
        memset(&buffer, 0, sizeof(buffer));
        memset(&myPayload, 0, sizeof(myPayload));

        file = fopen(argv[currentFile], "rb");
        if (!file) {
            printf("Unable to open file\n");
            return 1;
        }

        //======= FILE HEADER =======//

        //formez pachetul de tip F
        filenameLen = (int) strlen(argv[currentFile]);
        charLen = intToChar(5 + filenameLen);
        msgType = HEADER;
        do {
            memset(&myPayload, 0, sizeof(myPayload));
            charSeq = intToChar(currentSeq);

            t.payload[0] = soh;
            t.payload[1] = charLen;
            t.payload[2] = charSeq;
            t.payload[3] = msgType;

            memcpy(&t.payload[4], argv[currentFile], (size_t) filenameLen);
            size = filenameLen + 4;
            memcpy(myPayload, t.payload, (size_t) (size));
            myPayload[size] = '\0';
            crc = crc16_ccitt(myPayload, size + 1);
            memcpy(charCrc, &crc, 2);

            t.payload[size] = charCrc[0];
            t.payload[size + 1] = charCrc[1];
            t.payload[size + 2] = EOL;
            t.payload[size + 3] = '\0';
            t.len = (int) strlen(t.payload);

            printf("[%d]File Header %d\n", currentSeq, currentFile);
            currentSeq = incrementSeq(currentSeq);
            send_message(&t);

            waitMessageAndResend(&msgPointer, &t);
            r = (*msgPointer);
        }while (r.payload[3] != ACK);

        //======= DATA =======//

        msgType = DATA;
        readSize = MAXL;
        while(MAXL == readSize) {
            memset(&t, 0, sizeof(msg));
            memset(buffer, 0, sizeof(buffer));

            //datele se transmit cu dimensiunea citita din fisier (readSize)
            readSize = (int) fread(buffer, sizeof(char), (size_t) MAXL, file);
            memcpy(&t.payload[4], buffer, (size_t) readSize);
            charLen = intToChar(5 + readSize);
            do {
                memset(&myPayload, 0, sizeof(myPayload));
                charSeq = intToChar(currentSeq);
                t.payload[0] = soh;
                t.payload[1] = charLen;
                t.payload[2] = charSeq;
                t.payload[3] = msgType;

                size = readSize + 4;
                memcpy(myPayload, t.payload, (size_t) size);
                myPayload[size] = '\0';

                crc = crc16_ccitt(myPayload, size + 1);
                memcpy(charCrc, &crc, 2);

                t.payload[size] = charCrc[0];
                t.payload[size + 1] = charCrc[1];
                t.payload[size + 2] = EOL;
                t.payload[size + 3] = '\0';
                t.len = (int) strlen(t.payload);

                printf("[%d]Data %d\n", currentSeq, countData);
                currentSeq = incrementSeq(currentSeq);
                send_message(&t);

                waitMessageAndResend(&msgPointer, &t);
                r = (*msgPointer);
            }while (r.payload[3] != ACK);
            countData++;

        }

        //======= END OF FILE =======//

        memset(&t, 0, sizeof(msg));
        charLen = intToChar(5);
        msgType = ENDF;
        do {
            memset(&myPayload, 0, sizeof(myPayload));
            charSeq = intToChar(currentSeq);
            t.payload[0] = soh;
            t.payload[1] = charLen;
            t.payload[2] = charSeq;
            t.payload[3] = msgType;

            memcpy(myPayload, t.payload, 4);
            myPayload[4] = '\0';

            crc = crc16_ccitt(myPayload, 5);
            memcpy(charCrc, &crc, 2);

            t.payload[4] = charCrc[0];
            t.payload[5] = charCrc[1];
            t.payload[6] = EOL;
            t.payload[7] = '\0';
            t.len = (int) strlen(t.payload);

            printf("[%d]EOF\n", currentSeq);
            currentSeq = incrementSeq(currentSeq);
            send_message(&t);

            waitMessageAndResend(&msgPointer, &t);
            r = (*msgPointer);
        }while (r.payload[3] != ACK);

        fclose(file);
        currentFile++;
    }

    //======= END OF TRANSACTION =======//

    memset(&t, 0, sizeof(msg));
    charLen = intToChar(5);
    msgType = ENDT;
    do {
        memset(&myPayload, 0, sizeof(myPayload));
        charSeq = intToChar(currentSeq);
        t.payload[0] = soh;
        t.payload[1] = charLen;
        t.payload[2] = charSeq;
        t.payload[3] = msgType;

        memcpy(myPayload, t.payload, 4);
        myPayload[4] = '\0';

        crc = crc16_ccitt(myPayload, 5);
        memcpy(charCrc, &crc, 2);

        t.payload[4] = charCrc[0];
        t.payload[5] = charCrc[1];
        t.payload[6] = EOL;
        t.payload[7] = '\0';
        t.len = (int) strlen(t.payload);

        printf("[%d]EOT\n", currentSeq);
        send_message(&t);

        waitMessageAndResend(&msgPointer, &t);
        r = (*msgPointer);
    }while (r.payload[3] != ACK);

    free(msgPointer);
    return 0;
}
