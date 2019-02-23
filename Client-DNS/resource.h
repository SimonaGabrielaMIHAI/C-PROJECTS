#define PORT 53
#define BUFLEN 1024
#define MAXNAME 255
#define MAXREPLY 10

#define A 1     
#define NS 2    
#define CNAME 5 
#define MX 15   
#define SOA 6   
#define TXT 16

typedef struct {
	unsigned short id; 
	unsigned char rd :1; 
	unsigned char tc :1; 
	unsigned char aa :1; 
	unsigned char opcode :4; 
	unsigned char qr :1; 
	unsigned char rcode :4;
	unsigned char z :3;
	unsigned char ra :1;
	unsigned short qdcount;
	unsigned short ancount;
	unsigned short nscount;
	unsigned short arcount;
} dns_header_t;

typedef struct {
	unsigned short qtype;
	unsigned short qclass;
} dns_question_t;

typedef struct {
	unsigned short type;
	unsigned short class;
	unsigned int ttl;
	unsigned short rdlength;
} dns_rr_t;
