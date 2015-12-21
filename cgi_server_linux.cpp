#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#define MAX_IO_SIZE 15000
#define MAX_CONEECT_SERVER 5
#define QLEN 5
#define F_CONNECTING 0
#define F_READING 1
#define F_WRITING 2
#define F_DONE 3
char* connectServerIp[MAX_CONEECT_SERVER];
uint16_t connectServerPort[MAX_CONEECT_SERVER];
char* batchFileName[MAX_CONEECT_SERVER];
int iSetNum;

using namespace std;
int readline(int fd,char *ptr,int maxlen)
{
  int n, rc;
  char c;
  *ptr = 0;
  
  //fprintf(stderr, "do readline, %d\n", fd);
  for(n=1; n<maxlen; n++)
  {
    rc=read(fd,&c,1);
    if(rc== 1)
    {
      *ptr++ = c;
      if(c=='\n')  break;
    }
    else if(rc==0)
    {
      if(n==1)     return 0;
      else         break;
    }
    else return(-1);
  }
  return n;
}

int connectsock(int index, char* protocol)
{ 
	struct protoent *ppe; /* pointer to protocol information entry*/ 
	struct sockaddr_in sin; /* an Internet endpoint address */ 
	int  sockfd, type; /* socket descriptor and socket type */

	bzero((char *)&sin, sizeof(sin)); 
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	
	//use fix port
	if ( (sin.sin_port = htons(connectServerPort[index])) == 0 ){ 
		fprintf(stderr, "can't get \"%d\" service entry\n", connectServerPort[index]);
		exit(EXIT_FAILURE);
	}
	
	//use fix ip
	if ( (sin.sin_addr.s_addr = inet_addr(connectServerIp[index])) == INADDR_NONE ){
		fprintf(stderr, "can't get \"%s\" host entry\n", connectServerIp[index]);
		exit(EXIT_FAILURE);
	}
	
	 /* Map protocol name to protocol number */ 
	if( (ppe = getprotobyname(protocol)) == 0){
		fprintf(stderr, "can't get \"%s\" protocol entry\n", protocol); 
		exit(EXIT_FAILURE);
	}
	
	/* Use protocol to choose a socket type */ 
	if (strcmp(protocol, "udp") == 0) 
		type = SOCK_DGRAM; 
	else 
		type = SOCK_STREAM; 
 
    /* Allocate a socket */ 
	sockfd = socket(PF_INET, type, ppe->p_proto); 
	if (sockfd < 0){
		fprintf(stderr, "can't create socket: %s\n", sys_errlist[errno]);
		exit(EXIT_FAILURE);
	}
	
	/* Connect the socket */
	if (connect(sockfd, (struct sockaddr *)&sin, sizeof(sin)) < 0){
		fprintf(stderr, "can't connect to %s:%d: %s\n", connectServerIp[index], connectServerPort[index], sys_errlist[errno]);
		exit(EXIT_FAILURE);
	}
		 
	return sockfd; 
}

int connectTCP(int index){
	return connectsock(index, "tcp");
}

void printMessage(char* cs, int index, bool bIsInputString){
	string str;
	str.assign(cs);
	
	//replace '<' by &lt
	int pos = str.find("<");
	while(pos != string::npos){
		str = str.substr(0, pos).append("&lt").append(str.substr(pos+1));
		pos = str.find("<");
	}
	
	//replace '>' by &gt
	pos = str.find(">");
	while(pos != string::npos){
		str = str.substr(0, pos).append("&gt").append(str.substr(pos+1));
		pos = str.find(">");
	}
	
	//remove endline
	if(str.find("\r") != string::npos)
		str = str.substr(0, str.find("\r"));
	else
		str = str.substr(0, str.find("\n"));
	
	printf("<script>document.all['m%d'].innerHTML += \"", index);
	if(bIsInputString)
		printf("<b>");
	printf("%s", str.c_str());
	if(bIsInputString)
		printf("</b>");
	printf("<br>\";</script>\n");
	fflush(stdout);
}

void service(){
	int nConnection = iSetNum;
	struct sockaddr_in fsin;  /* the address of a client */
	socklen_t  alen;  /* length of client's address  */ 
	int  sockfd[MAX_CONEECT_SERVER];  /* socket fd */
	FILE* fp[MAX_CONEECT_SERVER];
	int flag[MAX_CONEECT_SERVER];
	int nfds;
	fd_set rfds; /* readable file descriptors*/
	fd_set wfds; /* writable file descriptors*/
	fd_set rs; /* active file descriptors*/
	fd_set ws; /* active file descriptors*/
	int status[MAX_CONEECT_SERVER];
	int error;
	socklen_t x;
	
	for(int i=0; i<iSetNum; i++){
		fprintf(stderr, "Connect to Server %d: %s:%d\n", i+1, connectServerIp[i], connectServerPort[i]);
		//connect to server and oper the batch file
		sockfd[i] = connectTCP(i);
		fp[i] = fopen(batchFileName[i], "r");
		if(fp[i] == NULL){
			fprintf(stderr, "batchFileName[%d]:%s is not exist\n", i, batchFileName[i]);
			//exit(EXIT_FAILURE);
		}
		
		//set nonblcking flag
		flag[i] = fcntl(sockfd[i], F_GETFL, 0);
		fcntl(sockfd[i], F_SETFL, flag[i] | O_NONBLOCK);
		//fclose(fp[i]);
	}
	fprintf(stderr, "Ready to service.\n");
	
	//do read/write
	nfds = FD_SETSIZE;
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&rs);
	FD_ZERO(&ws);
	for(int i=0; i<iSetNum; i++){
		FD_SET(sockfd[i], &rs);
		FD_SET(sockfd[i], &ws);
		status[i] = F_CONNECTING;
	}
	rfds = rs;
	wfds = ws;
	
	while(nConnection > 0){
		memcpy(&rfds, &rs, sizeof(rfds)); 
		memcpy(&wfds, &ws, sizeof(wfds));
		if (select(nfds, &rfds, &wfds, (fd_set*)0, (struct timeval*)0) < 0 ){
			fprintf(stderr, "select\n");
			exit(EXIT_FAILURE);
		}
		
		for(int i=0; i<iSetNum; i++){
			if (status[i] == F_CONNECTING && (FD_ISSET(sockfd[i], &rfds) || FD_ISSET(sockfd[i], &wfds))){
				if (getsockopt(sockfd[i], SOL_SOCKET, SO_ERROR, &error, &x) < 0 || error != 0) {
					// non-blocking connect failed
					fprintf(stderr, "getsockopt: %s\n", sys_errlist[errno]);
					exit(EXIT_FAILURE);
				}
				status[i] = F_READING;
				FD_CLR(sockfd[i], &ws);
			}
			else if(status[i] == F_WRITING && FD_ISSET(sockfd[i], &wfds)){
				char buf[MAX_IO_SIZE];
				string str;
				int iNeedWrite;
				int iOutputSize;
				
				if(feof(fp[i])){
					fprintf(stderr, "the file %d is end\n", i);
					fclose(fp[i]);
					status[i] = F_DONE;
					nConnection--;
					continue;
				}
				
				//write something
				fprintf(stderr, "writing to server %d\n", i);
				bzero(buf, MAX_IO_SIZE);
				fgets(buf, MAX_IO_SIZE, fp[i]);
				iNeedWrite = str.assign(buf).length();
				iOutputSize = write(sockfd[i], buf, iNeedWrite);
				fprintf(stderr, "wrote %s to server %d\n", buf, i);
				iNeedWrite -= iOutputSize;
				if (iOutputSize <= 0 || iNeedWrite <= 0) {
					// write finished
					printMessage(buf, i, true);
					FD_CLR(sockfd[i], &ws);
					status[i] = F_READING;
					FD_SET(sockfd[i], &rs);
				}
				fflush(stdout);
				
			}
			else if (status[i] == F_READING && FD_ISSET(sockfd[i], &rfds) ) {
				//read something
				fprintf(stderr, "reading from server %d\n", i);
				char buf[MAX_IO_SIZE];
				bzero(buf, MAX_IO_SIZE);
				int iInuptSize = readline(sockfd[i], buf, MAX_IO_SIZE);
				fprintf(stderr, "read %s from server %d\n", buf, i);
				printMessage(buf, i, false);
				if (iInuptSize <= 0) {
					// read finished
					FD_CLR(sockfd[i], &rs);
					status[i] = F_WRITING;
					FD_SET(sockfd[i], &ws);
				}
				fflush(stdout);
			}
		}
	}
	
	//printf("Server start!\n");
	
}

void initEnv(int argc, char** argv){
	iSetNum = (argc-1) / 3;
	if((argc-1) % 3 != 0){
		fprintf(stderr, "argc=%d\n", argc);
		exit(EXIT_FAILURE);
	}
	for(int i=0; i<iSetNum; i++){
		connectServerIp[i] = (char*) malloc(32);
		bzero(connectServerIp[i], 32);
		strcpy(connectServerIp[i], argv[i*3+1]);
		connectServerPort[i] = (uint16_t) strtoul(argv[i*3+2], NULL, 0);
		batchFileName[i] = (char*) malloc(32);
		bzero(batchFileName[i], 32);
		strcpy(batchFileName[i], argv[i*3+3]);
	}
}

void printHttpHeader(){
	
	printf("<html>\n");
    printf("<head>\n");
	printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />\n");
    printf("<title>Network Programming Homework 3</title>\n");
    printf("</head>\n");
}

void printHttpEnd(){
	printf("</html>\n");
	fflush(stdout);
}

void printBodyStart(){
	printf("<body bgcolor=#336699>\n");
    printf("<font face=\"Courier New\" size=2 color=#FFFF99>\n");
    printf("<table width=\"800\" border=\"1\">\n");
    printf("<tr>");
	for(int i=0; i<iSetNum; i++){
		printf("<td>%s:%d</td>", connectServerIp[i], connectServerPort[i]);
	}
    printf("\n");
    printf("<tr>\n");
	for(int i=0; i<iSetNum; i++){
		printf("<td valign=\"top\" id=\"m%d\">", i);
	}
    printf("</tr>\n");
    printf("</table>\n");
	fflush(stdout);
}

void printBodyEnd(){
	printf("</font>\n");
	printf("</body>\n");
	fflush(stdout);
}

int main(int argc, char** argv){
	initEnv(argc, argv);
	printHttpHeader();
	printBodyStart();
	service();
	printBodyEnd();
	printHttpEnd();
	return 0;
}
