#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#define SERVER_PORT 7778
#define MAX_IO_SIZE 15000
#define QLEN 5

#include <iostream>
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

int parseHttpMessage(string &str)
{
	string strGet = "GET /";
	string strHttp = "HTTP/";
	int startPos = str.find(strGet);
	int endPos = str.find(strHttp);
	if(startPos == string::npos || endPos == string::npos){
		fprintf(stderr, "GET or HTTP is lost!\n");
		return -1;
	}
	str = str.substr(startPos + strGet.length(), endPos - strGet.length() - 1);
	return 0;
}

int deleteChangeLine(char* buf, int iSize)
{
	int size = iSize - 1;
	buf[iSize] = '\0';
	while(buf[size] == '\n' || buf[size] == '\r'){
		buf[size--] = '\0';
	}
	return size;
}

void httpService(int sockfd)
{
	char sInputbuf[1000];
	string sInputString;
	string sApplicationName;
	char* vCommandWord[MAX_IO_SIZE];
	
	//get the string of file name and parameters
	setenv("PATH", ".", 1);
	int iInputSize = readline(sockfd, sInputbuf, 1000);
	iInputSize = deleteChangeLine(sInputbuf, iInputSize);
	fprintf(stderr, "%s\n", sInputbuf);
	sInputString.assign(sInputbuf);
	if(parseHttpMessage(sInputString) == -1){
		exit(EXIT_FAILURE);
	}
	//fprintf(stderr, "%s %d\n", sInputString.c_str(), sInputString.length());
	sApplicationName = sInputString;
	vCommandWord[0] = (char*) malloc(50);
	bzero(vCommandWord[0], 50);
	strcpy(vCommandWord[0], sApplicationName.c_str());
	
	//set file name and parameters
	int nParameterCount = 1;
	int pos = sInputString.find("?");
	if(pos != string::npos){
		sApplicationName = sInputString.substr(0, pos);
		bzero(vCommandWord[0], 50);
		strcpy(vCommandWord[0], sApplicationName.c_str());
		sInputString = sInputString.substr(pos+1);
		for(int i=1; i<=15; i++){
			int startpos = sInputString.find("=") + 1;
			int endpos = sInputString.find("&");
			if(startpos == string::npos)
				break;
			if(endpos == string::npos){//last parameter
				if(startpos == sInputString.length())//empty parameter -> break
					break;
			}
			else if(endpos == startpos)//empty parameter -> break
				break;
			vCommandWord[i] = (char*) malloc(32);
			bzero(vCommandWord[i], 32);
			strcpy(vCommandWord[i], sInputString.substr(startpos, endpos-startpos).c_str());
			//printf("%d %d %s\n", startpos, endpos, vCommandWord[i]);
			sInputString = sInputString.substr(endpos+1);
			nParameterCount++;
			if(endpos == string::npos)//last parameter
				break;
		}
	}
	
	vCommandWord[nParameterCount] = (char *) 0;
	/*
	fprintf(stderr, "%s ", sApplicationName.c_str());
	for(int i=0; i<nParameterCount; i++){
		fprintf(stderr, "%s ", vCommandWord[i]);
	}
	fprintf(stderr, "\n");
	*/

	if(dup2(sockfd, STDIN_FILENO) == -1){
		write(STDERR_FILENO, "dup socket to output error!\n", 28);
		exit(EXIT_FAILURE);
	}
	if(dup2(sockfd, STDOUT_FILENO) == -1){
		write(STDERR_FILENO, "dup socket to output error!\n", 28);
		exit(EXIT_FAILURE);
	}
	//printf("HTTP/1.1 200 OK\n");
	//printf("Content-Type: text/html\n\n");
	if(execvp(sApplicationName.c_str(), vCommandWord) == -1){
		fprintf(stderr, "execvp: %s\n", sys_errlist[errno]);
		exit(EXIT_FAILURE);
	}
	
	exit(EXIT_SUCCESS);
}

int passivesock(char* service, char* protocol, int qlen)
{
	struct servent   *pse; /* pointer to service information entry*/ 
	struct protoent *ppe; /* pointer to protocol information entry*/ 
	struct sockaddr_in sin; /* an Internet endpoint address */ 
	int  socketfd, type; /* socket descriptor and socket type */

	bzero((char *)&sin, sizeof(sin)); 
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	
	/* Map service name to port number */
	/*
	if ( pse = getservbyname(service, protocol) )
		sin.sin_port = pse->s_port; 
	else if ( (sin.sin_port = htons((u_short)atoi(service))) == 0 ){ 
		fprintf(stderr, "can't get \"%s\" service entry\n", service);
		exit(EXIT_FAILURE);
	}
	*/
	//use fix port: SERVER_PORT
	if ( (sin.sin_port = htons(SERVER_PORT)) == 0 ){ 
		fprintf(stderr, "can't get \"%s\" service entry\n", service);
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
	socketfd = socket(PF_INET, type, ppe->p_proto); 
	if (socketfd < 0){
		fprintf(stderr, "can't create socket: %s\n", sys_errlist[errno]);
		exit(EXIT_FAILURE);
	}
	 /* Bind the socket */
	if (bind(socketfd, (struct sockaddr *)&sin, sizeof(sin)) < 0){ 
		fprintf(stderr, "can't bind to %s port: %s\n", service, sys_errlist[errno]);
		exit(EXIT_FAILURE);		
	}		
	if (type == SOCK_STREAM && listen(socketfd, qlen) < 0){
		fprintf(stderr, "can't listen on %s port: %s\n", service, sys_errlist[errno]);
		exit(EXIT_FAILURE);
	}
		 
	return socketfd; 
}

int passiveTCP(char* service, int qlen)
{ 
	return passivesock(service, (char* )"tcp", qlen); 
}

int TCPechod(int fd){ 
	char  buf[BUFSIZ]; 
	int  cc; 
 
	//fprintf(stderr, "do TCPechod\n, %d", BUFSIZ);
	while (cc = readline(fd, buf, sizeof(buf))) {
		//fprintf(stderr, "read string: %s\n", buf);
		if (cc < 0){ 
			fprintf(stderr, "echo read: %s\n", sys_errlist[errno]);
			exit(EXIT_FAILURE);	
		}
		if (write(fd, buf, cc) < 0){
			fprintf(stderr, "echo write: %s\n", sys_errlist[errno]); 
			exit(EXIT_FAILURE);	
		}
	} 
	return 0; 
}

int main(int argc, char** argv){
	//char  *service = "echo";  /* service name or port number */ 
	struct sockaddr_in fsin;  /* the address of a client */
	socklen_t  alen;  /* length of client's address  */ 
	int  msock;  /* master server socket  */ 
	int  ssock;  /* slave server socket  */
	char sClientIp[INET_ADDRSTRLEN];
	int iClientPort;
	int	iShmFd; /* share memory fd  */
	int iUserId; /* User id*/
	
	int iRootPid = getpid();
	switch(argc){ 
		case  1: 
			break; 
		case  2: 
			//service = argv[1]; 
			break; 
		default: 
			fprintf(stderr, "usage: TCPechod [port]\n");
			exit(EXIT_SUCCESS);
	}
	
	//msock = passiveTCP(service, QLEN);
	msock = passiveTCP("echo", QLEN);
	//fprintf(stderr, "The socket is ready.\n");
	
	printf("Server start!\n");
	
	while(true){
		alen = sizeof(fsin); 
		ssock = accept(msock, (struct sockaddr *) &fsin, &alen);
		
		//fprintf(stderr, "I got a client.\n");		
		if (ssock < 0) { 
			if (errno == EINTR) 
				continue; 
			fprintf(stderr, "accept: %s\n", sys_errlist[errno]);
			exit(EXIT_FAILURE);			
		} 
		
		//Concurrent, Connection-Oriented Servers
		int pid = fork();
		switch (pid) {
			case -1:    
				fprintf(stderr, "fork: %s\n", sys_errlist[errno]);
				exit(EXIT_FAILURE);			
			case 0:    /* child */
				close(msock);
				
				//do service
				httpService(ssock);
				//maybe I will come to here
				close(ssock);
			default:  /* parent */ 
				close(ssock);
				//fprintf(stderr, "I am parent.\n");
				break;
		} 
	}
	
	return 0;
}
