#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>	// "struct sockaddr_in"
#include <arpa/inet.h>	// "in_addr_t"
#include <malloc.h>
#include <dirent.h>
#include <getopt.h>
#include <isa-l.h>
#include <ctype.h>
#include "myftp.h"
#include <sys/select.h>
int N,SERVERNUM,K,BLOCKSIZE=0;
void client_recv_file_data(int fd[], char filename[],char path[],int work_node[]);
void client_send_file_data(int fd[], Stripe *stripe, char filename[]);
void send_encode_data(int fd[],Stripe* stripe);
void client_send_file_data(int fd[], Stripe *stripe, char filename[]){
	fd_set sendfds;
	int i;
	int j, m;
	int max_sd=0;
	int len=0;
	unsigned char payload[PAYLEN];
	memset(payload,0,PAYLEN);
	int *have_send=(int*)malloc(sizeof(int)*N);
	for(i=0;i<N;i++){
		have_send[i]=0;
	}
	for(i=0;i<N;i++){
		char blockcode[1026];
		construct_blockcode(blockcode,filename,  stripe->sid,  i);
		if((len=send(fd[i],blockcode,sizeof(blockcode),0))!=sizeof(blockcode)){
			perror("fail to send blockcode\n");
			exit(1);
		}
	}
	while(1){
		FD_ZERO(&sendfds);
		for(i=0;i<N;i++){
			FD_SET(fd[i],&sendfds);
			max_sd=(max_sd>fd[i])?max_sd:fd[i];
		}
		if(select(max_sd+1,NULL,&sendfds,NULL,NULL)<0){
			printf("select failed\n");
			exit(-1);
		}
		for(i=0;i<N;i++){
			if(FD_ISSET(fd[i],&sendfds)){
				int min=(BLOCKSIZE-have_send[i]>sizeof(payload))?sizeof(payload):(BLOCKSIZE-have_send[i]);
				if(have_send[i]!=BLOCKSIZE){
					for(m=0;m<min;m++){
						payload[m]=stripe->data_block[i][have_send[i]+m];
					}
					if((len=send(fd[i],payload,min,0))<0){
						printf("Can not send it to the %d server\n",i);
						exit(1);
					}
					// printf("len: %d min: %d data_block: %d\n",len,min,BLOCKSIZE);
					// print_payload(payload,len);
					have_send[i]+=len;
					memset(payload,0,sizeof(payload));
				}
				// for(j=0;j<=sizeof(stripe->data_block[i])/(sizeof(payload)-1);j++){
				// 	for(m=0;m<sizeof(payload)-1;m++){
				// 		payload[m]=stripe->data_block[i][j*(sizeof(payload)-1)+m];
				// 	}
				// 	if((len=send(fd[i],payload,sizeof(payload)-1,0))<0){
				// 		printf("Can not send it to the %d server\n",i);
				// 		exit(1);
				// 	}
				// 	have_send[i]+=len;
				// 	memset(payload,0,sizeof(payload));
				// }
			}
		}
		int sendAll=0;
		for(i=0;i<N;i++){
			if(have_send[i]==BLOCKSIZE){
				sendAll+=1;
			}
		}
		if(sendAll==N){
			break;
		}
	}
}
// void send_encode_data(int fd[N],Stripe*stripe){
// 	int i;
// 	for(i=0;i<N;i++){
		 
// 	}
// }
void encode_data(int fd[N],Stripe* stripe,char filename[]){
	int i,j,m,c,e,ret;
	int k=K,p=N-K,len=BLOCKSIZE,n=N;
	// uint8_t *frag_ptrs[BLOCKSIZE];
	// uint8_t *encode_matrix, errors_matrix;
	// uint8_t *invert_matrix, *temp_matrix;
	// uint8_t *g_tbls;
	// Stripe *stripe;

	// encode_matrix=(uint8_t *)malloc(sizeof(uint8_t)*((k+p)*k));
	// errors_matrix=(uint8_t *)malloc(sizeof(uint8_t)*(k*k));
	// invert_matrix=(uint8_t *)malloc(sizeof(uint8_t)*(k*k));
	// g_tbls=(uint8_t *)malloc(sizeof(uint8_t)*(k*p*32));
	// printf("can i reach here 6\n");
	stripe->encode_matrix=(uint8_t*)malloc(sizeof(uint8_t)*(n*k));
	stripe->table=(uint8_t*)malloc(sizeof(uint8_t)*(32*p*k));
	// printf("can i reach here 5\n");
	gf_gen_rs_matrix(stripe->encode_matrix,k+p,k);
	// printf("can i reach here 7\n");
	ec_init_tables(k,p,&stripe->encode_matrix[k*k],stripe->table);
	// printf("can i reach here 8\n");
	ec_encode_data(BLOCKSIZE,k,p,stripe->table,stripe->data_block,stripe->parity_block);
	// printf("can i reach here 9\n");
	// for(i=0;i<N;i++){
	// 	// printf("%dth row:",i);
	// 	for(j=0;j<len;j++){
	// 		//printf("%d ",stripe->data_block[i][j]);
	// 	}
	// 	// printf("\n");
	// }
	client_send_file_data(fd,stripe,filename);
	
}

void divide_file_into_blocks(int fd[],char filename[],char path[]){
	int i;
	char filepath[1025]="";
	strcat(filepath,path);
	strcat(filepath,filename);
		// printf("can i reach here\n");
	FILE * file=NULL;
	// printf("can i reach here\n");
	if((file=fopen(filepath,"rb"))<0){
		perror("fail to open the file\n");
	}
	// printf("can i reach here\n");
	unsigned int filelength=0;
	char c;
	c=fgetc(file);
	while(!feof(file)){
		filelength+=1;
		// printf("char is %X\n",c);
		c=fgetc(file);
	}

	// printf("file length is %d\n",filelength);
	fclose(file);


	struct message_s header;
	int len;
	char payload[PAYLEN];
	header.protocol[0]='m';
	header.protocol[1]='y';
	header.protocol[2]='f';
	header.protocol[3]='t';
	header.protocol[4]='p';
	header.type=0xFF;
	header.length=BLOCKSIZE*ceiling((float)filelength/(BLOCKSIZE*K));
	header.file_length= htonl(filelength);
	header.length = htonl(header.length);


	if((file=fopen(filepath,"rb"))<0){
		perror("fail to open the file\n");
	}
	for( i=0;i<N;i++){
		if((len=send(fd[i],&header,sizeof(header),0))<0){
			perror("Not all servers connecting to the client\n");
			exit(1);
		}
	}
	// printf("can i reach here1\n");
	Stripe* stripe=(Stripe *)malloc(sizeof(Stripe));
	// printf("can i reach here6\n");
	stripe->data_block=(unsigned char**)malloc(sizeof(unsigned char * )*N);
	// unsigned char ** data_block=(unsigned char**)malloc(sizeof(unsigned char * )*N);


	// printf("can i reach here4\n");
	stripe->parity_block=(unsigned char**)&(stripe->data_block[K]);
	for(i=0;i<N;i++){
		// printf("can i reach here3\n");
		stripe->data_block[i]=(unsigned char*)malloc(sizeof(unsigned char )*BLOCKSIZE);
		// printf("ith data_block: %d\n",BLOCKSIZE);
		memset(stripe->data_block[i],0,BLOCKSIZE);
	}
	// 
	// memset(payload,0,PAYLEN);
	int if_finish=0;
	unsigned int file_length=0;
	// int stripe_num=0;
	header.length = ntohl(header.length);
	stripe->sid=0;
	for(i=0;i<=(filelength-1)/(BLOCKSIZE*K);i++){
		int j;
		for(j=0;j<K;j++){
			int y;
			for(y=0;y<BLOCKSIZE;y++){
				stripe->data_block[j][y]=fgetc(file);
				if(!feof(file)){

					file_length+=1;
				}
				else{//finish reading  the file
					if_finish=1;
					encode_data(fd,stripe,filename);
					stripe->sid+=1;
					// printf("can i reach here 10\n");
					int x;
					for(x=0;x<N;x++){
						memset(stripe->data_block[x],0,BLOCKSIZE);
					}
					break;
				}
			}
			if(if_finish==1){
				break;
			}

		}

		if(if_finish==0){
			
			encode_data(fd,stripe,filename);
			stripe->sid+=1;
			int x;
			for(x=0;x<N;x++){
				memset(stripe->data_block[x],0,BLOCKSIZE);
			}

		}


	}
	// printf("Finish dividing the file\n");
	
	fclose(file);

}
void list_file(int fd[], struct message_s header){
	// memset(command,0,5);
	int len;
	int i;
	header.protocol[0]='m';
	header.protocol[1]='y';
	header.protocol[2]='f';
	header.protocol[3]='t';
	header.protocol[4]='p';

	header.type=0xA1;
	// buf.type=0xA1;
	header.length=0;

	header.length = htonl(header.length);
	// buf=htonl(buf);
	for(i=0;i<N;i++)
	{
		if(fd[i]!=-1){
			// printf("fd: %d\n",fd[i]);
			if((len=(send(fd[i],&header,sizeof(header),0)))<0){
				perror("can not send request for list\n");
			}
			// printf("send the command to server\n");
			struct message_s message_from_server;
			// printf("size : %d %d\n",sizeof(message_from_server),sizeof(message_from_server.payload));
			if((len=(recv(fd[i],&message_from_server,sizeof(message_from_server),0)))<0){
				perror("can not recv the message from server\n");
			}
			// printf("recv the message\n");
			else{
				char payload[PAYLEN];
				memset(payload,0,sizeof(payload));
				message_from_server.length = ntohl(message_from_server.length);
				// printf("size:%d\n",message_from_server.length);
				printf("list: \n");
				int j;
				for( j=0;j<=(int)(message_from_server.length-1)/sizeof(payload);j++){
					int lenleft=sizeof(payload);
					while(lenleft!=0){
						if((len=(recv(fd[i],payload,sizeof(payload),0)))<0){
							perror("can not receive the file list from server\n");
							exit(1);
						}
						lenleft-=len;
						if(lenleft<0){
							printf("lenleft < 0\n");
						}
					}
					
					printf("%s\n",payload);
					
				}
			}
			break;
		}
	}
	for(i=0;i<SERVERNUM;i++){
		if(fd[i]!=-1){
			close(fd[i]);
		}
	}
	printf("Done\n");
}
void select_recv_payload(int fd[],int work_node[], Stripe *stripe,int length){
	int i,j;
	int len;
	fd_set recvfds;
	int max_sd;
	int recvAll=0;
	int *have_recv=(int*)malloc(sizeof(int)*N);
	unsigned char payload[PAYLEN];
	memset(payload,0,sizeof(payload));

	for(i=0;i<N;i++){
		have_recv[i]=0;
	}
	while(1){
		FD_ZERO(&recvfds);
		for(i=0;i<N;i++){
			
			if( work_node[i]==1){
				FD_SET(fd[i],&recvfds);
				max_sd=(max_sd>fd[i])?max_sd:fd[i];
			}
		
		}
		if(select(max_sd+1,&recvfds,NULL,NULL,NULL)<0){
			printf("select failed\n");
			exit(-1);
		}		
		for(i=0;i<N;i++){
			if(FD_ISSET(fd[i],&recvfds)){
				// int min=(BLOCKSIZE-have_send[i]>sizeof(payload))?sizeof(payload):(BLOCKSIZE-have_send[i]);
				// struct message_s header;
				memset(payload,0,sizeof(payload));
				if(work_node[i]==1 && have_recv[i]!=length){
					if(sizeof(payload)<length-have_recv[i]){
						if((len=recv(fd[i],payload,sizeof(payload),0))<0){
							printf("Can not recv the payload from the %d server\n",i);
							exit(1);
						}
					}
					else{
						if((len=recv(fd[i],payload,length-have_recv[i],0))<0){
							printf("Can not recv the payload from the %d server\n",i);
							exit(1);
						}
					}
					// work_node[i]=1;
					// printf("can i reach here \n");
					for(j=0;j<len;j++){
						// printf("have recv i: %d, j:%d, have_recv:%d, length: %d\n",i,j,have_recv[i],length);
						stripe->data_block[i][j+have_recv[i]]=payload[j];
					}	
					// printf("can i reach here 6\n");
					have_recv[i]+=len;
					if(have_recv[i]==length){
						recvAll+=1;
					}
					// printf(" have_recv:%d, length: %d i:%i\n",have_recv[i],length,i);
					if(have_recv[i]>length){
						printf("recv extra data\n");
						exit(1);
					}
				}
				if(recvAll==K){
					// return header;

					break;
				}
			}
		}

		if(recvAll==K){
			break;
		}
	}
}
struct message_s select_recv_header(int fd[],int work_node[]){
	int i;
	int len;
	fd_set recvfds;
	int max_sd;
	int recvAll=0;
	int *have_recv=(int*)malloc(sizeof(int)*N);
	for(i=0;i<N;i++){
		have_recv[i]=0;
	}
	while(1){
		FD_ZERO(&recvfds);
		for(i=0;i<N;i++){
		
			if( work_node[i]==1){
				FD_SET(fd[i],&recvfds);
				max_sd=(max_sd>fd[i])?max_sd:fd[i];
			}

		}
		if(select(max_sd+1,&recvfds,NULL,NULL,NULL)<0){
			printf("select failed\n");
			exit(-1);
		}		
		for(i=0;i<N;i++){
			if(FD_ISSET(fd[i],&recvfds)){
				// int min=(BLOCKSIZE-have_send[i]>sizeof(payload))?sizeof(payload):(BLOCKSIZE-have_send[i]);
				struct message_s header;
				if(work_node[i]==1 && have_recv[i]==0){
					// printf("severid: %d\n",i);
					memset(&header,0,sizeof(header));
					if((len=recv(fd[i],&header,sizeof(header),0))!=sizeof(header)){
						// printf("Can not recv the header from the %d server\n",i);
						// printf("recv %d\n",len);
						exit(1);
					}
					have_recv[i]=1;
					// work_node[i]=1;
					recvAll+=1;
				}
				if(recvAll==K){
					return header;
					break;
				}
			}
		}

		if(recvAll==K){
			break;
		}
	}

}
void select_send_content(int fd[],int work_node[],void * content,int size_to_send,int if_firsttime){
	int i,j;
	int len;
	fd_set sendfds;
	int max_sd;
	int sendAll=0;
	int *have_send=(int*)malloc(sizeof(int)*N);
	for(i=0;i<N;i++){
		have_send[i]=0;
	}
	while(1){
		FD_ZERO(&sendfds);
		// FD_SET(2,&sendfds);
		for(i=0;i<N;i++){
			if(if_firsttime==0 ){
				if( work_node[i]==1){
					FD_SET(fd[i],&sendfds);
					max_sd=(max_sd>fd[i])?max_sd:fd[i];
				}
			}
			else{
				FD_SET(fd[i],&sendfds);
				max_sd=(max_sd>fd[i])?max_sd:fd[i];
			}
		}

		struct timeval timelimit;
		timelimit.tv_sec=20;
		if(select(max_sd+1,NULL,&sendfds,NULL,&timelimit)<0){
			// printf("exception: %s",explain_select(max_sd+1,NULL,&sendfds,NULL,&timelimit));
			printf("select failed\n");
			exit(-1);
		}
		if(if_firsttime==0)		
		{
			for(i=0;i<N;i++){
				if(FD_ISSET(fd[i],&sendfds)){
					// int min=(BLOCKSIZE-have_send[i]>sizeof(payload))?sizeof(payload):(BLOCKSIZE-have_send[i]);
					if(work_node[i]==1 && have_send[i]!=1){
						if((len=send(fd[i],content,size_to_send,0))!=size_to_send){
							printf("Can not send the header to the %d server\n",i);
							exit(1);
						}
						// printf("i have send %d size_to_send: %d i:%d\n",len,size_to_send,i);
						sendAll+=1;
						have_send[i]=1;
					}
					if(sendAll==K){
						break;
					}
				}
			}
		}
		else{
			// timeout=0
			// if(FD_ISSET(2,&sendfds)){
			// 	printf("i can connect to an unexisted socket\n");
			// 	exit(1);
			// }
			for(i=0;i<N;i++){
				if(FD_ISSET(fd[i],&sendfds)){
					
					if(work_node[i]!=1){
						// printf("%dth server can be connected fd: %d\n",i,fd[i]);
						if((len=send(fd[i],content,size_to_send,0))!=size_to_send){
							printf("Can not send the header to the %d server\n",i);
							exit(1);
						}
						work_node[i]=1;
						sendAll+=1;
						// timeout=1;
					}
					if(sendAll==K){
						for(j=0;j<N;j++){
							if(work_node[j]==0){
								close(fd[j]);
							}
						}
						break;
					}
				}
			}
			
			// printf("time is out\n");
			if(sendAll<K){
				printf("no enough servers are online\n");
				for(i=0;i<N;i++){
					close(fd[i]);
				}
				exit(1);
			}

			

		}

		if(sendAll==K){
			break;
		}
	}
}
void select_send_serverid(int fd[],int work_node[]){
	int i;
	int len;
	fd_set sendfds;
	int max_sd;
	int sendAll=0;
	int *have_send=(int*)malloc(sizeof(int)*N);
	for(i=0;i<N;i++){
		have_send[i]=0;
	}
	while(1){
		FD_ZERO(&sendfds);
		for(i=0;i<N;i++){
			if( work_node[i]==1){
				FD_SET(fd[i],&sendfds);
				max_sd=(max_sd>fd[i])?max_sd:fd[i];
			}
			
		}
		if(select(max_sd+1,NULL,&sendfds,NULL,NULL)<0){
			printf("select failed\n");
			exit(-1);
		}
		
		for(i=0;i<N;i++){
			if(FD_ISSET(fd[i],&sendfds)){
				// int min=(BLOCKSIZE-work_node[i]>sizeof(payload))?sizeof(payload):(BLOCKSIZE-work_node[i]);
				struct message_s header;
				header.server_id=i;
				if(work_node[i]==1 && have_send[i]!=1){
					if((len=send(fd[i],&header,sizeof(header),0))!=sizeof(header)){
						printf("Can not send the header to the %d server\n",i);
						exit(1);
					}
					sendAll+=1;
					have_send[i]==1;
				}
				if(sendAll==K){
					break;
				}
			}
		}

		if(sendAll==K){
			break;
		}
	}
}
void stripe_decode(Stripe * stripe,int work_node[],FILE* file,int lenleft){
	int i,j,h=0;
	uint8_t *encode_matrix=malloc(sizeof(uint8_t)*(N*K));
	uint8_t *errors_matrix=malloc(sizeof(uint8_t)*(K*K));
	uint8_t *invert_matrix=malloc(sizeof(uint8_t)*(K*K));
	uint8_t *decode_matrix=malloc(sizeof(uint8_t)*(N*K));
	uint8_t *table=(uint8_t*)malloc(sizeof(uint8_t)*(32*(N-K)*K));
	gf_gen_rs_matrix(encode_matrix,N,K);

	for(i=0;i<N;i++){
		if(work_node[i]==1){
			for(j=0;j<K;j++){
				errors_matrix[K*h+j]=encode_matrix[K*i+j];
			}
			h+=1;
		}
	}
	gf_invert_matrix(errors_matrix,invert_matrix,K);
	h=0;
	// int failed_block[K];
	// for(i=0;i<K;i++){
	// 	failed_block[i]=0;
	// }
	int failed_blocks_num=0;
	for(i=0;i<K;i++){
		if(work_node[i]!=1){
			for(j=0;j<K;j++){
				decode_matrix[K*h+j]=invert_matrix[K*i+j];
			}
			h+=1;
			failed_blocks_num+=1;
		}
	}
	// printf("in decode: can i reach here\n");
	ec_init_tables(K,N-K,decode_matrix,table);
		// printf("in decode: can i reach here2\n");
	unsigned char** normal_datablock=(unsigned char**)malloc(sizeof(unsigned char*)*(K-failed_blocks_num));
	unsigned char** failed_datablock=(unsigned char**)malloc(sizeof(unsigned char*)*(failed_blocks_num));
	int g=0;
	// h=0;
	// 	printf("in decode: can i reach here4\n");
	// for(i=0;i<K;i++){
	// 	if(work_node[i]==1){
	// 		normal_datablock[g]=(unsigned char*)malloc(sizeof(unsigned char)*BLOCKSIZE);
	// 		for(j=0;j<BLOCKSIZE;j++){
	// 			normal_datablock[g][j]=stripe->data_block[i][j];
	// 		}
	// 		g++;
	// 	}
	// 	else{
	// 		failed_datablock[h]=(unsigned char*)malloc(sizeof(unsigned char)*BLOCKSIZE);
	// 		for(j=0;j<BLOCKSIZE;j++){
	// 			// failed_datablock[h][j]=stripe->data_block[i][j];
	// 			failed_datablock[h][j]=0;
	// 		}
	// 		h++;
	// 	}
	// }

	h=0;
	for(i=0;i<N;i++){
		if(work_node[i]==1){
			normal_datablock[h]=stripe->data_block[i];
			h++;
		}
	}
	for(i=0;i<failed_blocks_num;i++){
		failed_datablock[i]=(unsigned char*)malloc(sizeof(unsigned char)*BLOCKSIZE);
	}

	// printf("in decode: can i reach here3,failed_datablock: %d\n",failed_blocks_num);
	ec_encode_data(BLOCKSIZE,K,failed_blocks_num,table,normal_datablock,failed_datablock);
	// printf("in decode: can i reach here5\n");
	int len_to_write=0;
	if(lenleft>BLOCKSIZE*K){
		len_to_write=BLOCKSIZE*K;
	}
	else{
		len_to_write=lenleft;
	}
	g=0;
	h=0;
	// printf("len_to_write is %d\n",len_to_write);
	for(i=0;i<K;i++){
		if(len_to_write==0){
			break;
		}
		if(work_node[i]==1){
			if(len_to_write>BLOCKSIZE){
				if((fwrite(normal_datablock[g],BLOCKSIZE,1,file))<0){
					perror("fail to write the file\n");
				}
				len_to_write-=BLOCKSIZE;
			}
			else{
				if((fwrite(normal_datablock[g],len_to_write,1,file))<0){
					perror("fail to write the file\n");
				}
				len_to_write-=len_to_write;
			}
			g++;
		}
		else{
			if(len_to_write>BLOCKSIZE){
				if((fwrite(failed_datablock[h],BLOCKSIZE,1,file))<0){
					perror("fail to write the file\n");
				}
				len_to_write-=BLOCKSIZE;
			}
			else{
				if((fwrite(failed_datablock[h],len_to_write,1,file))<0){
					perror("fail to write the file\n");
				}
				len_to_write-=len_to_write;
			}
			h++;
		}
	}
	fflush(file);
	return;
}
void client_recv_file_data(int fd[],char filename[],char path[],int work_node[]){
	int stripenum=0;
	int i,j;
	unsigned char payload[PAYLEN];
	fd_set sendfds;
	int max_sd=0;
	// int *have_send=(int*)malloc((int)*N);
	// for(i=0;i<N;i++){
	// 	have_send[i]=0;
	// }
	// printf("can i reach here 1\n");
	select_send_serverid(fd,work_node);
	int filelength=0;
	struct message_s header=select_recv_header(fd,work_node);
	// printf("can i reach here 2\n");
	if(header.type!=0xFF){
		printf("wrong type!\n");
		exit(1);
	}
	header.file_length=ntohl(header.file_length);
	header.length=ntohl(header.length);
	filelength=header.length;

	int lenleft=header.file_length;

	Stripe* stripe=(Stripe*)malloc(sizeof(Stripe));
	stripe->data_block=(unsigned char**)malloc(sizeof(unsigned char*)*N);
	for(i=0;i<N;i++){
		stripe->data_block[i]=(char*)malloc(sizeof(unsigned char)*BLOCKSIZE);
	}
	FILE * file=NULL;
	char filepath[1024]="";
	strcat(filepath,filename);

	if((file=fopen(filepath,"wb"))==NULL){
		printf("faile to open file: %s\n",filepath);
	}
	for(i=0;i<ceiling((float)filelength/(BLOCKSIZE*N));i++){
		// printf("can i reach here 3\n");
		select_recv_payload(fd,work_node,stripe,BLOCKSIZE);
		// printf("can i reach here4\n");

		// printf("lenleft: %d\n",lenleft);
		stripe_decode(stripe,work_node,file,lenleft);

		if(i<ceiling((float)filelength/(BLOCKSIZE*N))-1){
			lenleft-=BLOCKSIZE*K;
		}
		
		struct message_s header;
		header.type=0xD1;
		// printf("i have done collect a stripe\n");
		select_send_content(fd,work_node,&header,sizeof(header),0);	 
	}
	fclose(file);
	printf("Done\n");
	
}
void get_file(int fd[SERVERNUM],   char * command,char filename[]){
	struct message_s header;
	int i;
	int len;
	int mr_fd=0;
	int ms_fd=0;
	fd_set recv_fds;
	fd_set send_fds;
	// if(command[3]!=' '){
	// 	printf("c:%s\n",command);
	// 	perror("wrong format\n");
	// 	exit(1);
	// }
	// for (i=3;i<strlen(command);i++){
	// 	if(command[i]!=' '){
	// 		break;
	// 	}
	// }
	// const char *tmp=&command[i];
	// char filename[32];
	char payload[PAYLEN]="";
	// strcpy(filename,tmp);
	header.protocol[0]='m';
	header.protocol[1]='y';
	header.protocol[2]='f';
	header.protocol[3]='t';
	header.protocol[4]='p';

	header.type=0xB1;
	
	strcpy(payload,filename);
	header.length=strlen(filename);

	header.length = htonl(header.length);
	int ava_server=0;
	int *work_node=(int*)malloc(N*sizeof(int));
	for(i=0;i<N;i++){
		work_node[i]=0;
	}
	select_send_content(fd,work_node,&header,sizeof(header),1);
	for(i=0;i<N;i++){
		if(work_node[i]==1){
				// printf("the %d server is on\n",i);
			}
	}
	select_send_content(fd,work_node,payload,sizeof(payload),0);
	struct message_s result_of_get=select_recv_header(fd,work_node);
	if(result_of_get.type==0xB2){
		printf("servers successfully find the file\nplease wait for a while\n");
		// recv_file_data(fd, filename,"");
	}
	else{
		printf("fail to find the file\n");
		exit(1);
	}
	char path[2]="";
	client_recv_file_data(fd, filename,path,work_node);

}

void put_file(int fd[SERVERNUM], char command[],char filename[]){
	struct message_s header;
	int i;
	int len;
	// if(command[3]!=' '){
	// 	printf("c:%s\n",command);
	// 	perror("wrong format\n");
	// 	exit(1);
	// }
	// for (i=3;i<strlen(command);i++){
	// 	if(command[i]!=' '){
	// 		break;
	// 	}
	// }
	// const char *tmp=&command[i];
	// char filename[32];
	char payload[PAYLEN]="";
	int f_exist=0;
	// strcpy(filename,tmp);
	strcpy(payload,filename);
	header.protocol[0]='m';
	header.protocol[1]='y';
	header.protocol[2]='f';
	header.protocol[3]='t';
	header.protocol[4]='p';

	header.type=0xC1;
	
	header.length = strlen(filename);
	header.length = htonl(header.length);

	DIR * dir;
	struct dirent * ptr;
	if ((dir=opendir("."))==NULL){
		perror("can not find the responding data\n");
	}
	while((ptr=readdir(dir))!=NULL){
		if(strcmp(payload,ptr->d_name)==0){
			f_exist=1;
			break;
		}
	}
	if(f_exist==0){
		// printf("can not find the file locally\n" );
		perror("can not find the file locally\n");
		exit(1);
	}
	else{
		printf("find the file\nplease wait for a while\n");
		for(i=0;i<N;i++){
			if((len=(send(fd[i],&header,sizeof(header),0)))<0){
				perror("can not send request to server");
			}
		}
		//printf("send the header\n");
		for(i=0;i<N;i++){
			if((len=(send(fd[i],payload,sizeof(payload),0)))<0){
				perror("can not send request to server");
			}
		}
		//printf("send the file name\n");
		struct message_s message_from_server;
		for(i=0;i<N;i++){
			if((len=(recv(fd[i],&message_from_server,sizeof(message_from_server),0)))<0){
				perror("can not recv request to client");
			}
			if(message_from_server.type!=0xC2){
				perror("can't receive header from all servers\n");
			}
		}
		divide_file_into_blocks(fd,filename,"");
		
	}
	printf("Done\n");
}
void main_task(in_addr_t *ip, unsigned short *port,char command[],char filename[])
{
	int i;
	int j;

	struct message_s header;
	// struct all_data buf;
	int *fd=(int*)malloc(sizeof(int)*SERVERNUM);
	// char command[100];
	// memset(command,0,100);
	struct sockaddr_in addr;
	unsigned int addrlen=sizeof (struct sockaddr_in);
	int len;

	// scanf("%s", command);
	// printf("command: %s",command);

	for( i=0;i<SERVERNUM;i++)
	{	
		fd[i]=socket(AF_INET, SOCK_STREAM,0);
		if(fd[i]== -1){
			perror("socket()");
			exit(1);
		}
		// printf("fd[%d]: %d\n",i,fd[i]);
		memset(&addr, 0, sizeof(struct sockaddr_in));
		addr.sin_family=AF_INET;
		addr.sin_addr.s_addr=ip[i];
		addr.sin_port=htons(port[i]);
		// printf("connect: %d\n",port[i]);
		if(connect(fd[i], (struct sockaddr *) &addr, addrlen)==-1){
			printf("connection to the %dth server is failed\n",i);
			close(fd[i]);
			fd[i]=-1;
			// exit(1);
		}
	}
	// while(strcmp(command,"list")!=0 && strncmp(command,"get",3)!=0&& strncmp(command,"put",3)!=0){
	// 	printf("command: (1) list, (2) get filename, or (3) put filename\n");
	// 	// scanf("%s", command);
	// 	// gets(command);
	// 	fgets(command,100,stdin);
	// 	command[strlen(command)-1]='\0';
	// 	printf("command: %s\n",command);
	// 	// scanf("%s", command);
	// }
	printf("command: %s\n",command);
	if (strcmp(command,"list")==0){
		// printf("what is wrong\n");
		list_file(fd,header);
	}
	if (strcmp(command,"get")==0){
		get_file(fd,command,filename);
	}
	if (strcmp(command,"put")==0){
		for(i=0;i<N;i++){
			if(fd[i]<0){
				printf("Not all servers are connected\n");
				for(j=0;j<N;j++){
					if(fd[j]<0){
						close(fd[i]);
					}
				}
				exit(1);
			}
		}
		put_file(fd,command,filename);
		// printf("Sent nothing\n");
	}

	for(j=0;j<N;j++){
		if(fd[i]<0){
			close(fd[i]);
		}
	}
}

int main(int argc, char **argv){
	
	unsigned short *port;
	if (argc !=4 && argc!=3){
		fprintf(stderr,"Usage: %s [configeration file] <list|get|input> <file>\n", argv[0]);
		exit(1);
	}

	in_addr_t *ip;
	// if ((ip=inet_addr(argv[1]))==-1){
	// 	perror("inet_addr()");
	// 	exit(1);
	// }

	FILE *config_file;
	if((config_file=fopen(argv[1],"r"))<0){
		printf("No such file\n");
		exit(1);
	}
	
	char line[100];
	int line_num=0;
	while(fgets(line,100,config_file)!=NULL){
		if(line_num==0){
			// printf("can i reach here 1\n");
			SERVERNUM=atoi(line);
			// printf("can i reach here 12\n");
			N=SERVERNUM;
			// line_num++;
			port=(unsigned short*)malloc(sizeof(unsigned short)*N);
			ip=(in_addr_t*)malloc(sizeof(in_addr_t)*N);
			// printf("can i reach here 13 N: %d\n",N);
		}
		if(line_num==1){
			// printf("can i reach here 14\n");
			K=atoi(line);
			// printf("can i reach here 15 K:%d\n",K);
			// line_num++;
		}
		if(line_num==2){
			// printf("can i reach here 16\n");
			BLOCKSIZE=atoi(line);
			// printf("can i reach here 17 BLOCKSIZE: %d\n",BLOCKSIZE);
			// line_num++;
		}
		if(line_num>2){
			int i;
			char tmp_ip[100];
			char tmp_port[100];
			memset(tmp_ip,0,sizeof(tmp_ip));
			memset(tmp_port,0,sizeof(tmp_ip));
			// printf("can i reach here 18\n");
			for(i=0;i<strlen(line);i++){
				if(line[i]==':'){
					// printf("can i reach here 120\n");
					int j;
					for(j=0;j<i;j++){
						tmp_ip[j]=line[j];
					}
					// printf("can i reach here 19 %s|\n",tmp_ip);
					if((ip[line_num-3]=inet_addr(tmp_ip))==-1){
						perror("inet_addr()");
						exit(1);
					}
										// printf("can i reach here 121\n");

					if(line[strlen(line)-1]=='\n'){
						for(j=i+1;j<strlen(line)-1;j++){
							tmp_port[j-i-1]=line[j];
						}
					}
					else{
						for(j=i+1;j<strlen(line);j++){
							tmp_port[j-i-1]=line[j];
						}
					}
										// printf("can i reach here 123\n");

					port[line_num-3]=atoi(tmp_port);
										// printf("can i reach here 124 port%d\n",port[line_num-3]);

					// line_num++;
					break;
				}
			}
		}
		line_num++;
	}
	printf("N: %d, SERVERNUM: %d, K %d,BLOCKSIZE %d\n",N,SERVERNUM,K,BLOCKSIZE);
	// exit(1);

	// port=atoi(argv[2]);
	// unsigned short port1[10];
	// port1[0]=12345;
	// port1[1]=12344;
	// port1[2]=12343;
	// port1[3]=12342;
	// port1[4]=12341;
	char command[10];
	char filename[1024];
	memset(command,0,sizeof(command));
	memset(filename,0,sizeof(filename));
	strcpy(command,argv[2]);
	if(argc==4){
		strcpy(filename,argv[3]);
	}
	if(strcmp(command,"list")!=0 && strcmp(command,"get")!=0&& strcmp(command,"put")!=0){
		printf("command: (1) list, (2) get filename, or (3) put filename\n");
	}
	if(strcmp(command,"get")==0 || strcmp(command,"put")==0){
		if (argc!=4){
			printf("command: (1) list, (2) get filename, or (3) put filename\n");
		}
	}
	main_task(ip,port,command,filename);
	return 0;
}
