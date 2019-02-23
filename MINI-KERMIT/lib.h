#ifndef LIB
#define LIB

typedef struct {
    int len;
    char payload[1400];
} msg;

typedef enum {
    SEND = 'S',
    HEADER = 'F',
    DATA = 'D',
    ENDF = 'Z',
    ENDT = 'B',
    ACK = 'Y',
    NAK = 'N',
    ERROR = 'E'
} type;

static const int MAXL = 250;
static const int TIME = 5000;

void init(char* remote, int remote_port);
void set_local_port(int port);
void set_remote(char* ip, int port);
int send_message(const msg* m);
int recv_message(msg* r);
msg* receive_message_timeout(int timeout); //timeout in milliseconds
unsigned short crc16_ccitt(const void *buf, int len);
char incrementSeq(int seq);
unsigned char intToChar(int x);

#endif

