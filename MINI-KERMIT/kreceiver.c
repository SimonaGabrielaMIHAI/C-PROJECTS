#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10001

//***Astept mesajul S de la sender***//
int waitMessageS(msg** rcvPointer) {
    int resendCounter = 0;
    (*rcvPointer) = receive_message_timeout(TIME);
    while((resendCounter < 3) && ((*rcvPointer) == NULL)) {
        //astept mesajul S de maxim 3 ori
        resendCounter++;
        (*rcvPointer) = receive_message_timeout(TIME);
    }
    if(resendCounter == 3) {
        //mesajul nu a mai venit
        return -1;
    }
    return 1;
}

//***Creez mesaj de tip ACK/NAK***//
void createMsg(int seq, msg** send, type msgType) {
    (*send)->payload[0] = (char) (0x01);
    (*send)->payload[1] = (char) (5);
    (*send)->payload[2] = (char) seq;
    (*send)->payload[3] = msgType;
    unsigned short crc = crc16_ccitt((*send)->payload, 4);
    memcpy(&((*send)->payload[4]), &crc, 2);
    (*send)->payload[6] = (char) (0x0D);
    (*send)->payload[7] = '\0';
}

//***Astept mesajul de la sender***//
void waitMessage(msg** rcvPointer) {
    (*rcvPointer) = receive_message_timeout(TIME);
    while( (*rcvPointer) == NULL) {
        (*rcvPointer) = receive_message_timeout(TIME);
    }
}


int main(int argc, char** argv) {
    msg r, t;
    msg* msgPointer = malloc(sizeof(msg));

    init(HOST, PORT);
    FILE *file = NULL;

    //formez partial numele fisierului de output
    char fileName[6 + MAXL];
    strncpy(fileName, "recv_", 5);
    fileName[5] = '\0';

    char myPayload[8 + MAXL];
    char charCrc[2];
    char buffer[MAXL + 1];
    int msgLen;
    int seq = 1;
    int rcvSeq = 0;
    int msgStatus = 0;
    int keepGoing = 1;
    unsigned short crc;
    unsigned short rcvCrc;


    //======= SEND INIT RESPONSE =======//

    while(1) {
        memset(&t, 0, sizeof(msg));
        memset(msgPointer, 0, sizeof(msg));
        msgStatus = waitMessageS(&msgPointer);
        r = (*msgPointer);

        if (msgStatus == -1) {
            //mesajul S nu a fost primit si se iese din program
            return -1;
        }

        if(r.payload[3] != SEND) {
            //mesajul nu este de tip send
            //conexiunea nu se face adecvat
            return 0;
        }

        //crc si seq primite
        memcpy(&rcvCrc, &r.payload[15], 2);
        rcvSeq = r.payload[2];

        //recalculare crc
        msgLen = 2 + (int) r.payload[1];
        memcpy(t.payload, r.payload, (size_t) msgLen);
        memcpy(myPayload, r.payload, 15);
        myPayload[15] = '\0';
        crc = crc16_ccitt(myPayload, 16);

        if (rcvCrc == crc) {
            //mesaj fara erori => raspuns ACK si se iese din while
            t.payload[2] = (char) seq;
            t.payload[3] = ACK;

            //recalculez crc cu noul nr de secventa
            memcpy(myPayload, t.payload, 15);
            myPayload[15] = '\0';
            crc = crc16_ccitt(myPayload, 15);
            memcpy(charCrc, &crc, 2);
            t.payload[15] = charCrc[0];
            t.payload[16] = charCrc[1];

            printf("\t\t\t[%d]ACK pentru [%d]\n", seq, rcvSeq);
            seq = (int) incrementSeq(seq);
            send_message(&t);
            break;
        } else {
            //mesaj corupt => raspuns NAK
            memset(msgPointer, 0, sizeof(msg));
            createMsg(seq, &msgPointer, NAK);
            t = (*msgPointer);
            printf("\t\t\t[%d]NAK pentru [%d]\n", seq, rcvSeq);
            seq = (int) incrementSeq(seq);
            send_message(&t);
        }
    }

    //======= RESPONSE =======//

    //se ruleaza pana cand se inchide conexiunea
    while(keepGoing) {
        memset(msgPointer, 0, sizeof(msg));
        memset(&t, 0, sizeof(msg));

        waitMessage(&msgPointer);
        //se iese din functie cu un mesaj diferit de NULL

        r = (*msgPointer);

        //255 e vazut ca -1
        if((int) r.payload[1] == -1) {
            msgLen = 257;
        } else {
            msgLen = 2 + (int) r.payload[1];
        }

        //recalculare crc
        memcpy(&rcvCrc, &r.payload[msgLen - 3], 2);
        memcpy(myPayload, r.payload, (size_t) (msgLen - 3));
        myPayload[msgLen - 3] = '\0';
        crc = crc16_ccitt(myPayload, (msgLen - 2));

        rcvSeq = (int) r.payload[2];

        //verificare crc
        if (rcvCrc == crc) {
            //mesaj corect => se trece la interpretarea datelor
            createMsg(seq, &msgPointer, ACK);
            t = (*msgPointer);
            printf("\t\t\t[%d]ACK pentru [%d]\n", seq, rcvSeq);
            seq = incrementSeq(seq);
            send_message(&t);

        } else {
            //mesaj corupt => NAK si se asteapta alt mesaj
            createMsg(seq, &msgPointer, NAK);
            t = (*msgPointer);
            printf("\t\t\t[%d]NAK pentru [%d]\n", seq, rcvSeq);
            seq = incrementSeq(seq);
            send_message(&t);
            continue;
        }

        //daca mesajul nu este corupt, se interpreteaza datele
        switch (r.payload[3]) {
            case HEADER :
                //se deschide fisierul pentru scriere, cu numele primit
                memcpy(&fileName[5], &r.payload[4], (size_t) (msgLen - 7));
                fileName[msgLen - 2] = '\0';
                file = fopen(fileName, "wb");
                if (!file) {
                    printf("\t\t\tUnable to open file\n");
                    return 1;
                }
                break;
            case DATA :
                //se scrie in fisier
                memcpy(&buffer, &r.payload[4], (size_t) (msgLen - 7));
                fwrite(buffer, (size_t) (msgLen - 7), 1, file);
                break;
            case ENDF :
                //inchiderea fisierului
                if(file) {
                    fclose(file);
                }
                break;
            case ENDT :
                //se va iesi ulterior din loop
                keepGoing = 0;
                break;
            default:
                printf("\t\t\tSomething wrong in type %c \n", r.payload[3]);
        }
    }
	free(msgPointer);
	return 0;
}
