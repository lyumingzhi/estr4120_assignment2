#ifndef MYFTP_H
#define MYFTP_H
#define PAYLEN 1025
// #define SERVERNUM 5
// #define K 2
// #define N 5
// #define BLOCKSIZE 4096
struct message_s{
	unsigned char protocol[5];
	unsigned char type;
	unsigned int length;
	unsigned int server_id;
	unsigned int file_length;
}__attribute__((packed));
typedef struct stripe
{
	int sid;
	unsigned char **data_block;
	unsigned char **parity_block;
	uint8_t *encode_matrix;
	uint8_t *table;
}Stripe;
int verify_filename(char name_to_search[],char filename[]);
void print_payload(unsigned char payload[],int len);
void construct_blockcode(char blockcode[],char filename[], int sid, int fd_code);
int ceiling(float x);
void tranp_file_data(int accept_fd, char filename[], char path[]);
void recv_file_data(int fd, char filename[],char path[]);
#endif MYFTP_H
