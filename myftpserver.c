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
#include <dirent.h>
#include <malloc.h>
#include <pthread.h>
#include "myftp.h"
#include <ctype.h>
int N,SERVERNUM,K,BLOCKSIZE=0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int client_count = 0;
void server_send_file_data(int fd, char filename[],char path[]);
void server_send_file_data(int fd, char filename[],char path[]){
	// printf("can i reach here\n");\	int len;
	char payload[PAYLEN];
	struct message_s header;
	int server_id;
	int k;
	int stripenum=0;
	DIR * dir;
	struct dirent * ptr;
	int f_exist=0;
	int len;

	if((len=recv(fd,&header,sizeof(header),0))<0){
		perror("fail to recv server_id\n");
	}
	server_id=header.server_id;

	char filepath[1026]="";
	char blockcode[1026]="";
	


	if ((dir=opendir("data"))==NULL){
		perror("can not find the responding data");
	}
	while((ptr=readdir(dir))!=NULL){
		memset(blockcode,0,sizeof(blockcode));
		memset(filepath,0,sizeof(filepath));
		strcat(filepath,path);
		construct_blockcode(blockcode,filename,stripenum,server_id);
		// printf("block name is: %s\n",blockcode);
		// strcat(filepath,blockcode);	
		// printf("filepath name is: %s| compared filename is %s\n",blockcode,ptr->d_name);

		if(strcmp(blockcode,ptr->d_name)==0){
			f_exist=1;
			// printf("find the block file\n");
			stripenum++;
			
			closedir(dir);
			if ((dir=opendir("data"))==NULL){
				perror("can not find the responding data");
			}
		}

	}
	
	if(f_exist==0 || stripenum<0){
		perror("error: there are not such block\n");
		exit(1);
	}

	unsigned int true_length=0;
	FILE * info_file=NULL;
	char tmp[6]="info_";
	char info_filepath[1028]="";
	strcat(info_filepath,path);
	strcat(info_filepath,"info_");
	strcat(info_filepath,filename);
	if((info_file=fopen(info_filepath,"rb"))==NULL){
		printf("can not create info file\n");
	}
	while(1){
		if(feof(info_file)){
			break;
		}
		char digit=fgetc(info_file);
		if(digit>='0' && digit<='9'){
			true_length=true_length*10+digit-'0';
		}
		// printf("digit is %c %d\n", digit,digit);
	}
	fflush(info_file);
	fclose(info_file);


	header.protocol[0]='m';
	header.protocol[1]='y';
	header.protocol[2]='f';
	header.protocol[3]='t';
	header.protocol[4]='p';
	header.type=0xFF;
	header.length=(stripenum)*BLOCKSIZE*N;
	header.length = htonl(header.length);
	header.file_length=htonl(true_length);
	// printf("true_length is %d\n",true_length);
	// int filelength=header.length;
	// printf("file length is %d\n",filelength);
	// close(file);
	if((len=send(fd,&header,sizeof(header),0))<0){
		perror("cannot send header to client\n");
	}
	header.length = ntohl(header.length);

	// printf("stripenum: %d\n",stripenum);
	for(k=0;k<stripenum;k++)
	{
		FILE * file=NULL;
		memset(blockcode,0,sizeof(blockcode));
		memset(filepath,0,sizeof(filepath));
		strcat(filepath,path);
		construct_blockcode(blockcode,filename,k,server_id);
		strcat(filepath,blockcode);	

		if((file=fopen(filepath,"rb"))<0){
			perror("fail to open the file\n");
		}

		 // char character[2]="";
		memset(payload,0,PAYLEN);
		int if_finish=0;
		unsigned int file_length=0;

		
		
		int i;
		for(i=0;i<=(BLOCKSIZE-1)/(sizeof(payload));i++){
			int j;
			for(j=0;j<sizeof(payload);j++){
				// memset(character,0,sizeof(character));
				// character[0]=fgetc(file);
				payload[j]=fgetc(file);
				//printf("char: %c",payload[j]);
				if(!feof(file)){
					// printf("can i reach here,%d %d %s\n",j,strlen(payload), character);
					// printf("char is %X\n",character[0]);
					// strcat(payload,character);
					file_length+=1;
					// printf("can i reach there,\n");
				}
				else{
					// printf("start sending another stripe\n");
					if((len=send(fd,payload,BLOCKSIZE-sizeof(payload)*i,0))<0){
						printf("can not send payload to client\n");
						exit(-1);
					}
					if(len!=BLOCKSIZE){
						printf("the data sent is incomplete len:%d, BLOCKSIZE: %d i: %d\n",len,BLOCKSIZE,i);
					}
					memset(payload,0,PAYLEN);
					if_finish=1;
					break;
				}
			}

			if(if_finish==0){
				while((len=send(fd,payload,sizeof(payload),0))<0){
					perror("can not send payload to client\n");

				} 	

			}

			memset(payload,0,PAYLEN);

		}

		struct message_s finish_data_header;
		while((len=recv(fd,&finish_data_header,sizeof(finish_data_header),0))<0){
			// perror("The client has not recv all the data\n");
			// printf("why i am stuck\n");
		}
		if(finish_data_header.type==0xD1){
			// printf("Sending finished\n");
		}
		
		
		fclose(file);


	}
	printf("Done to send file\n");	
}

void server_receive_file_data(int fd,char filename[], char path[]){
	int len;
	int i;
	struct message_s header;
	char payload[PAYLEN];
	char blockcode[1026];
	memset(blockcode,0,sizeof(blockcode));
	if((len=recv(fd,&header,sizeof(header),0))<0){
		perror("cannot recv the header from server\n");
	}

	header.length = ntohl(header.length);
	header.file_length=ntohl(header.file_length);
	// printf("length: %d\n",header.length);
	if(header.type!=0xFF){
		
		perror("type is wrong \n");

	}
	// FILE* fileInformation=NULL;
	// char FIpath[1026]="";
	// char tmp[10]="info_";
	// strcat(FIpath,path);
	// strcat(FIpath,tmp);
	// strcat(FIpath,filename);
	// while((fileInformation=fopen(filepath,"wb"))==NULL){
	// 	printf("can not open file_Info\n");
	// }
	// char Info[10]="";
	// snprintf(Info,10,"%d",ceiling((float)header.length/BLOCKSIZE));
	// if((fwrite(Info,strlen(Info),1,fileInformation))<0){

	// }
	FILE * info_file=NULL;
	char info_filepath[1028]="";
	char tmp[6]="info_";
	strcat(info_filepath,path);
	strcat(info_filepath,tmp);
	strcat(info_filepath,filename);
	// printf("what is problem1\n");
	if((info_file=fopen(info_filepath,"wb"))==NULL){
		printf("can not create info file\n");
	}
	// printf("what is problem4\n");

	char length_to_file[20]="";
	snprintf(length_to_file,10,"%d",header.file_length);

	if(fwrite(length_to_file,strlen(length_to_file),1,info_file)<0){
		printf("can not write info file\n");
	}
	// printf("what is problem2\n");
	fflush(info_file);
	fclose(info_file);
	// printf("what is problem3\n");

	for(i=0;i<ceiling((float)header.length/BLOCKSIZE);i++){
		FILE * downfile=NULL;
		char filepath[1026]="";
		// char code_stripe[10]="";
		// char code_server="server";
		strcat(filepath,path);
		// snprintf(code_stripe,100,"%d",i);
		// strcat(filepath,code_stripe);
		if((len=recv(fd,blockcode,sizeof(blockcode),0))!=sizeof(blockcode)){
			perror("cannot receive block name\n");
		}
		strcat(filepath,blockcode);
		//printf("cat the filename\n");
		if((downfile=fopen(filepath,"wb"))==NULL){
			printf("open file: %s\n",filepath);
		}


		// printf("begin to download file from client\n");
		int len_left=BLOCKSIZE;
		while(len_left!=0){
			// printf("i am in\n");
			int min;
			memset(payload,0,PAYLEN);
			min=(sizeof(payload)>len_left)?len_left:sizeof(payload);
			if((len=recv(fd,payload,min,0))<0){
				perror("fail to recv file data from client\n");
				exit(1);
			}
			print_payload(payload,len);
			if((fwrite(payload,len,1,downfile))<0){
				printf("something wrong to write file!\n");
			}
			len_left-=len;
			if(len_left<0){
				printf("len_left<0! %d\n",len_left);
				exit(1);
			}

		}
		fflush(downfile);
		fclose(downfile);
	}
}
int extract_filename(struct dirent *ptr,char * payload, int index){
	int i;
	int count=0;
	// printf("filename:%s\n", ptr->d_name);
	if(strlen(ptr->d_name)<=5){
		return index;
	}
	if(strncmp(ptr->d_name,"info_",5)==0){
		// printf("filename: ");
		for(i=5;i<strlen(ptr->d_name);i++){
			payload[index]=ptr->d_name[i];
			// printf("%c",ptr->d_name[i]);
			index+=1;
		}
		// printf("\n");
		return index;
	}
	return index;
}
void list_reply(int accept_fd){
	char payload[PAYLEN];
	memset(payload,0,PAYLEN);
	int len;
	DIR * dir;
	struct dirent * ptr;
	struct message_s header;
	int fn_len=1;
	header.length=0;
	// printf("i am in\n");
	if ((dir=opendir("data"))==NULL){
		perror("can not find the responding data");
	}
	int index=0;
	int tmp_index=0;
	while((ptr=readdir(dir))!=NULL){
		if(index<PAYLEN){
			if(index<(tmp_index=extract_filename(ptr,payload,index))){
				index=tmp_index;
				strcat(payload,"\n");
				index++;
			}
			//printf("name: %s\n",ptr->d_name);
		}
		else{
			// fn_len+=1;
			// memset(payload,0,PAYLEN);
			// strcat(payload,ptr->d_name);
			// strcat(payload,"\n");
			// //printf("name: %s\n",ptr->d_name);
			printf("filename list exceed PAYLEN\n");
		}
	}
	// if(fn_len==0){
	// 	fn_len=1;
	// }
 
	//printf("what is wrong\n");
	// printf("the file list is: %s",filelist);
	closedir(dir);

	header.type=0xA2;
	// char tmpmessage[]="myftp";
	// header.protocol=(unsigned char *)tmpmessage ;
	header.protocol[0]='m';
	header.protocol[1]='y';
	header.protocol[2]='f';
	header.protocol[3]='t';
	header.protocol[4]='p';

	header.length=(strlen(payload));
	// printf("header: %d\n",header.length);
	
	header.length = htonl(header.length);

	memset(payload,0,PAYLEN);
	//strcpy(message_to_client.payload,filelist);
	index=0;
	if((len=(send(accept_fd,&header, sizeof(header),0)))<0){
		perror("can not send request to client");
	}
	else{
		if ((dir=opendir("data"))==NULL){
			perror("can not find the responding data");
		}
		while((ptr=readdir(dir))!=NULL){
			if(index<PAYLEN){
				int tmp_len=0;
				if(index<(tmp_len=extract_filename(ptr,payload,index))){
					index=tmp_len;
					strcat(payload,"\n");
					index++;
				}
				//printf("name: %s\n",payload);
			}
			else{
				//printf("name: %s\n",payload);
				// if((len=(send(accept_fd,payload,sizeof(payload),0)))<0){
				// 	perror("can not send the file name to client\n");
				// }
				// memset(payload,0,PAYLEN);
				// strcat(payload,ptr->d_name);
				// strcat(payload,"\n");
				//printf("name: %s\n",ptr->d_name);
				printf("filename list exceed PAYLEN\n");
			}
		
		}
		if(fn_len==1){
			// printf("file list: %s",payload);
			if((len=(send(accept_fd,payload,sizeof(payload),0)))<0){
				perror("can not send the file name to client\n");
			}
			// printf("len: %d\n",len);
		}
	}
	closedir(dir);
	//printf("Sending finished\n");
	// free(message_to_client.payload);
	printf("Done to send list\n");
}


void reply_request_file(int accept_fd, struct message_s buf, char payload[]){
	DIR * dir;
	struct dirent * ptr;
	struct message_s get_reply;
	int f_exist=0;
	int len;
	if ((dir=opendir("data"))==NULL){
		perror("can not find the responding data");
	}
	while((ptr=readdir(dir))!=NULL){
		// printf("file name:%s\n", buf.payload);
		if(verify_filename(payload,ptr->d_name)==0){
			f_exist=1;
			get_reply.type=0xB2;
			get_reply.protocol[0]='m';
			get_reply.protocol[1]='y';
			get_reply.protocol[2]='f';
			get_reply.protocol[3]='t';
			get_reply.protocol[4]='p';
			get_reply.length=0;
			printf("find the file %s\n",payload);
			break;
		}
	}
	closedir(dir);
	get_reply.length = htonl(get_reply.length);

	if(f_exist==0){
		get_reply.type=0xB3;
		// printf("cannot find the file\n");
	}
	if((len=(send(accept_fd,&get_reply,sizeof(get_reply),0)))<0){
		perror("can not send request to client");
	}
	if(f_exist==1){
		char path[]="data/";
		server_send_file_data( accept_fd, payload,path);
	}
	//printf("Sending finished\n"); 
}

void put_recv_file(int accept_fd){
	struct message_s header;
	int len;
	char payload[PAYLEN];
	char filename[PAYLEN];
	if((len=recv(accept_fd,payload,sizeof(payload),0))<0){
		perror("cannot send header to client\n");
	}
	strcpy(filename,payload);

	header.protocol[0]='m';
	header.protocol[1]='y';
	header.protocol[2]='f';
	header.protocol[3]='t';
	header.protocol[4]='p';
	header.type=0xC2;
	header.length=0;

	header.length = htonl(header.length);

	if((len=send(accept_fd,&header,sizeof(header),0))<0){
		perror("cannot send header to client\n");
	}

	server_receive_file_data(accept_fd,filename,"data/");
	printf("Done to recv file\n");
}

void *pthread_loop(int* sDescriptor){
	int accept_fd = (int)sDescriptor;
	int len;
	struct message_s buf;
	 
		if ((len = (recv(accept_fd,&buf,sizeof(buf),0)))<0){
			perror("can not recv the commend");
			pthread_mutex_lock(&mutex);
				client_count--;
			pthread_mutex_unlock(&mutex);
		}
		if(len==0){
			close(accept_fd);
			pthread_mutex_lock(&mutex);
			client_count--;
			pthread_mutex_unlock(&mutex);
			return;
		}
		
		if(buf.type==0xA1){
			list_reply(accept_fd);
		}
		if(buf.type==0xB1){
			unsigned char payload[PAYLEN];
			if ((len=(recv(accept_fd,payload,sizeof(payload),0)))<0){
				perror("can not recv the commend\n");
				pthread_mutex_lock(&mutex);
					client_count--;
				pthread_mutex_unlock(&mutex);
			}
			// printf("filename: %s len: %d\n",payload,len);
			reply_request_file( accept_fd, buf,payload);
		}
		if(buf.type==0xC1){
			put_recv_file(accept_fd);
		}
		close(accept_fd);
	pthread_mutex_lock(&mutex);
		client_count--;
	pthread_mutex_unlock(&mutex);
	//printf("%d\n",client_count);
}

void main_loop(unsigned short port){
	int fd, accept_fd, count;
	struct sockaddr_in addr;
	struct sockaddr_in tmp_addr;
	struct sockaddr_in listendAddr, connectedAddr, peerAddr;
	int connectedAddrLen;
	unsigned int addrlen= sizeof(struct sockaddr_in);
	fd=socket(AF_INET, SOCK_STREAM,0);
	if (fd==-1){
		perror("socket()");
		exit(1);
	}

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family= AF_INET;
	// addr.sin_addr.s_addr=htonl("192.168.0.1");
	addr.sin_addr.s_addr=htonl(INADDR_ANY);
	addr.sin_port=htons(port);
	// printf("server: %d\n",INADDR_ANY );
	if(bind(fd, (struct sockaddr *) &addr, sizeof(addr))==-1)
	{
		perror("bind()");
		exit(-1);
	}
	if (listen(fd, 1024)==-1){
		perror("listen()");
		exit(1);
	}
	printf("[To stop the server: press Ctrl + C]\n");
	
	pthread_t thr;
	while(1){
		//printf("%d\n",client_count);
		if(client_count<11){
			if((accept_fd=accept(fd,(struct sockaddr *) &tmp_addr, &addrlen))==-1){
				perror("accept()");
				exit(1);
			}
			connectedAddrLen = sizeof(connectedAddr);
			getsockname(accept_fd, (struct sockaddr *)&connectedAddr, &connectedAddrLen);
			printf("connected server address = %s:%d\n", inet_ntoa(connectedAddr.sin_addr), ntohs(connectedAddr.sin_port));
			pthread_mutex_lock(&mutex);
				client_count++;
			pthread_mutex_unlock(&mutex);
			if(pthread_create(&thr, NULL, pthread_loop, accept_fd)!=0){
				printf("fail to create thread\n");
				close(accept_fd);
				pthread_mutex_lock(&mutex);
					client_count--;
				pthread_mutex_unlock(&mutex);
			}
		}
	}
}
int main(int argc, char **argv){
	unsigned short port;
	int server_id;
	if (argc!=3){
		fprintf(stderr, "Usage: %s configeration file\n",argv[0]);
		exit(1);
	}
	FILE *config_file=fopen(argv[1],"r");
	char line[100];
	int line_num=0;
	while(fgets(line,100,config_file)!=NULL){
		if(line_num==0){
			// printf("can i reach here 1\n");
			SERVERNUM=atoi(line);
			// printf("can i reach here 12\n");
			N=SERVERNUM;
			// line_num++;
			// printf("can i reach here 13 N: %d\n",N);
		}
		if(line_num==1){
			// printf("can i reach here 14\n");
			K=atoi(line);
			// printf("can i reach here 15 K:%d\n",K);
			// line_num++;
		}
		if(line_num==2){
			server_id=atoi(line);
		}
		if(line_num==3){
			// printf("can i reach here 16\n");
			BLOCKSIZE=atoi(line);
			// printf("can i reach here 17 BLOCKSIZE: %d\n",BLOCKSIZE);
			// line_num++;
		}
		if(line_num==4){
			port=atoi(line);
			printf("port: %d\n",port);
		}
		line_num++;
	} 

	port=atoi(argv[2]);
	main_loop(port);
	return 0;
} 
