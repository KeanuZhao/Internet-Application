#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include<errno.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<sys/wait.h>
#include <string.h>
#include <fcntl.h>

#define BUFSIZE 100
#define BUFFSIZE 255

void modifyPassiveResponse(char* str);// Pass in the original 227 command and return new 227 command 
int getPort(char* str);//Pass in the original 227 command and return the port number

void getFilename(char* str, char* filename);//Pass in the RETR command and get the filename
void saveFile(char* filecontents, char* filename,int size);//Store the filecontent in a file
void setCacheCatalog(char* filename);//Write the filename into a file which contains downloaded files
int compareCacheCatalog(char* filename);//Judge whether the file is in the catalog file
void readFile(char *filename);//Read contents of a file
void pasv_mode();//passive mode


char* resetport(char* buffer);//Pass in the original PORT command and return new PORT command 
int convert(char buff[]);//Pass in the original PORT command and return the port number

void bindAndListenSocket(int sock,int port);//Bind and listen a socket
int acceptCmdSocket(int sock);//Accept 
int acceptDataSocket(int sock);//Accept
void connectToServer(int sock,int port);//Connect
void connectToClient(int sock,int port);//Connect
void actv_mode();


char passive_response[255];//
int data_send_times=0; //Every time increase 1 to set up new port
char* filecontents; //The contents of cache file 
int fd_cache = 0; //Cache file pointer
int cachefile_size = 0; //The size of cache file 


int main(int argc, const char *argv[]){
	if(strcmp(argv[1],"PASV")==0)
		pasv_mode();
	else if(strcmp(argv[1],"ACTV")==0)
		actv_mode();
}

void pasv_mode(){
	
 	//printf("%s", argv[1]);
	
	fd_set master_set, working_set;  
    struct timeval timeout;          
    int proxy_cmd_socket    = 0;     
    int accept_cmd_socket   = 0;     
    int connect_cmd_socket  = 0;     
    int proxy_data_socket   = 0;     
    int accept_data_socket  = 0;     
    int connect_data_socket = 0;     
    int selectResult = 0;     
    int select_sd = 25;    
    int server_data_port; //The data connection port of server
    char filename[255];
    int send_cache = 0;//1 for the file existing in the proxy 
    
    
    FD_ZERO(&master_set);
    bzero(&timeout, sizeof(timeout));
    

	
    //Set up proxy_cmd_socket, bind and listen
    struct sockaddr_in myCmdAddr;
    bzero(&myCmdAddr,sizeof(myCmdAddr));//Zero the struct
   	myCmdAddr.sin_family = AF_INET;//server family, ipv4
	myCmdAddr.sin_addr.s_addr = htonl(INADDR_ANY);//Any ip address allocated by operating system
	myCmdAddr.sin_port = htons(21);//server port,parameter is int    
    proxy_cmd_socket = socket(PF_INET, SOCK_STREAM, 0); 
    bind(proxy_cmd_socket,(struct sockaddr*)&myCmdAddr, sizeof(myCmdAddr)); 
    if(listen(proxy_cmd_socket,5)<0)
    	printf("\nListen failed.");
    FD_SET(proxy_cmd_socket, &master_set);  
    
    

    
    timeout.tv_sec = 6000;    
    timeout.tv_usec = 0;    
    
    while(1){
    	
    	FD_ZERO(&working_set); 
        memcpy(&working_set, &master_set, sizeof(master_set)); 
        
      
        selectResult = select(select_sd, &working_set, NULL, NULL, &timeout);
        
        // fail
        if (selectResult < 0) {
            perror("select() failed\n");
            exit(1);
        }
        
        // timeout
        if (selectResult == 0) {
            printf("select() timed out.\n");
            continue;
        }
        

        for (int i = 0; i < select_sd; i++){
	        	if (FD_ISSET(i, &working_set)){
	        		
	        		 
		        		if (i == proxy_cmd_socket){
							printf("\nproxy_cmd_socket!\n");
							//Accept, set up the connection between client and proxy
							struct sockaddr_in clientAddr;
							socklen_t clientAddrlen;
							clientAddrlen = sizeof(clientAddr);
							if((accept_cmd_socket = accept(proxy_cmd_socket,(struct sockaddr*)&clientAddr,&clientAddrlen))>0)
								printf("\nClient is accepted!");								
	
							//Connect, set up the connection between proxy and server
							struct sockaddr_in serverAddr;
							memset(&serverAddr, 0, sizeof(serverAddr));/*Zero out structure*/
							serverAddr.sin_family = AF_INET; /* Internet addr family */
							if(inet_aton("192.168.56.1",&serverAddr.sin_addr)<0)
								printf("Error: %d",errno);/*Server IP address*/
							serverAddr.sin_port = htons(21); /* FTP Server port */
		                    connect_cmd_socket = socket(PF_INET, SOCK_STREAM, 0);
		                    if((connect(connect_cmd_socket,(struct sockaddr*)&serverAddr, sizeof(serverAddr)))< 0)
								printf("\nConnection failure %d",errno);
							else printf("\nConnection succeed\n");
		                    
                            
		                    FD_SET(accept_cmd_socket, &master_set);
		                    FD_SET(connect_cmd_socket, &master_set);		                    
		                }
		                //Transfer the message from server to client
		                if (i == connect_cmd_socket){
		                	printf("\nconnect_cmd_socket!\n");
              				int recvstrLen;
							char recv_str[255];
							recvstrLen = recv(connect_cmd_socket,recv_str,sizeof(recv_str),0);
							if(recvstrLen<0)
								printf("Recieve fail. %d\n",errno);
							else if(strncmp(recv_str,"227",3) == 0){// passive mode
							
								printf("Receiving strlen: %d\n",strlen(recv_str));
								
								modifyPassiveResponse(recv_str);// Pass in the original 227 command and return new 227 command 
								server_data_port = getPort(recv_str); ////Pass in the original 227 command and return the port number 
							 	printf("Send to client: %s \n",passive_response);
							 	send(accept_cmd_socket,passive_response,strlen(passive_response),0);
							}else{recv_str[recvstrLen]='\0';printf("Send to client: %s ",recv_str);send(accept_cmd_socket,recv_str,strlen(recv_str),0);}//发送给client收到的来自服务器的reply 
                		}
                		//Transfer the message from client to server
       		         	if (i == accept_cmd_socket){
    		         		printf("\naccept_cmd_socket!\n");	
	         		       	char buff[BUFSIZE] = {0};
	         		       	if (read(i, buff, BUFSIZE) == 0){
	       		         		close(i); 
                        		close(connect_cmd_socket);
		                        FD_CLR(i, &master_set);
		                        FD_CLR(connect_cmd_socket, &master_set);
	       		         	}else if(strncmp(buff,"MLSD",4) == 0){// If MLSD, reset filename 
								memset(filename, 0, sizeof(filename));
	       		         		strcpy(filename,"NULL");
								printf("Send to server: %s",buff);
								send(connect_cmd_socket,buff,strlen(buff),0);
	         		       	}else if(strncmp(buff,"RETR",4) == 0){//cache filename
	       		         		memset(filename, 0, sizeof(filename));//Reset filename 
	       		         		strcpy(filename,"NULL");//Initialize filename with NULL
	         		       		getFilename(buff,filename);// Get the filename
								
								if(compareCacheCatalog(filename)==1){//if file exist
									send_cache = 1;

	         		       			strcat(filename,"\n");
									readFile(filename);
	         		       			
	         		       		}else{//if file doesn't exist, be ready to cache such file, and send cmd to server
									printf("Cache file: %s\n",filename);
									setCacheCatalog(filename);
									printf("Send to server: %s",buff);
									send(connect_cmd_socket,buff,strlen(buff),0);
		       		         	}
	         		       	}else if(strncmp(buff,"PASV",4) == 0){
	         		       		close(proxy_data_socket);//sestart socket4 
	         		       		data_send_times = data_send_times + 1;//Used to get new data connection port number of proxy
	         		       		struct sockaddr_in myDataAddr;
							    bzero(&myDataAddr,sizeof(myDataAddr));//Zero the struct
							   	myDataAddr.sin_family = AF_INET;//server family, ipv4
								myDataAddr.sin_addr.s_addr = htonl(INADDR_ANY);//Any ip address allocated by operating system
								int proxy_client_portnum;
								proxy_client_portnum = 9999 + data_send_times;
								myDataAddr.sin_port = htons(proxy_client_portnum);//server port,parameter is int   
							    proxy_data_socket = socket(PF_INET, SOCK_STREAM, 0); 
							    bind(proxy_data_socket,(struct sockaddr*)&myDataAddr, sizeof(myDataAddr)); 
							    if(listen(proxy_data_socket,5)<0)
							    	printf("\nListen failed.");   
							    FD_SET(proxy_data_socket, &master_set);
	         		       		printf("Send to server: %s ",buff);
								send(connect_cmd_socket,buff,strlen(buff),0);
							}else{
	         		       		printf("Send to server: %s ",buff);
	         		       		send(connect_cmd_socket,buff,strlen(buff),0);
	         		       	}
         		       	} 
		                
    					//Transfer the message from client to server
		                if (i == proxy_data_socket){
							printf("\nproxy_data_socket!\n");		                	
		             
		                	//Accept, set up the data connection between proxy and server
							struct sockaddr_in clientAddr2;
							socklen_t clientAddr2len;
							clientAddr2len = sizeof(clientAddr2);
							if((accept_data_socket = accept(proxy_data_socket,(struct sockaddr*)&clientAddr2,&clientAddr2len))>0)
								printf("\nData connection to client is accepted!");
								
							//Coonect, set up the data connection between client and proxy
							struct sockaddr_in serverAddr2;
							memset(&serverAddr2, 0, sizeof(serverAddr2));/*Zero out structure*/
							serverAddr2.sin_family = AF_INET; /* Internet addr family */
							if(inet_aton("192.168.56.1",&serverAddr2.sin_addr)<0)
								printf("Error: %d",errno);/*Server IP address*/
							serverAddr2.sin_port = htons(server_data_port); /* FTP Server port */
		                    connect_data_socket = socket(PF_INET, SOCK_STREAM, 0);
		                    if((connect(connect_data_socket,(struct sockaddr*)&serverAddr2, sizeof(serverAddr2)))< 0)
								printf("\nData connection failure %d",errno);
							else printf("\nData connection to server succeed\n");
							
       					
		                    FD_SET(accept_data_socket, &master_set);
		                    FD_SET(connect_data_socket, &master_set);
		                    
		                    if(send_cache == 1){// If file exists, send the contents from proxy to client directly
                    				char connectionAccepted[]="150 Connection accepted\r\n";
									send(accept_cmd_socket,connectionAccepted,strlen(connectionAccepted),0);
									printf("Send to client: %s",connectionAccepted);
	         		       			printf("Be ready to send cache: %s",filecontents);
									if(send(accept_data_socket,filecontents,cachefile_size,0)<0)
										printf("\nSend cache fail. Errno: %d\n",errno);//Download from proxy
	         		       			else printf("Send cache: %s",filecontents);
	         		       			char transferOK[]="226 Transfer OK\r\n";
									send(accept_cmd_socket,transferOK,strlen(transferOK),0);
									printf("Send to client: %s",transferOK);
	         		       			close(accept_data_socket);                       
			                        FD_CLR(accept_data_socket, &master_set);
									close(connect_data_socket);
									FD_CLR(connect_data_socket, &master_set);
									send_cache = 0;
                    		}
								                	
                		}
                		//Transfer the data from server to client
                		if (i == accept_data_socket){
                			printf("\naccept_data_socket!\n");
        		       		char buff2[BUFSIZE] = {0};
        		       		int size;
	         		       	if ((size = read(i, buff2, BUFSIZE) )== 0){
	       		         		close(accept_data_socket);                     
		                        FD_CLR(accept_data_socket, &master_set);
								close(connect_data_socket);
								FD_CLR(connect_data_socket, &master_set);
	       		         	}else{
	       		         		printf("Send to server: %s ",buff2);
	       		         		if(strcmp("NULL",filename)!=0){
									saveFile(buff2,filename,size);
		       		         		printf("\nSaved: %s, len=%d",filename,strlen(filename));

								}else printf("Filename: %s",filename);
	         		       		if(send(connect_data_socket,buff2,size,0)<0)
									printf("PASV Data connection: send to server fail. Errno: %d", errno);
	       		         	}
		                }
		                //Transfer the data from client to server
		                if (i == connect_data_socket){
              				printf("\nconnect_data_socket!\n");
						  	int recvstr2Len;
							char recv_str2[BUFSIZE];
							recvstr2Len = recv(connect_data_socket,recv_str2,sizeof(recv_str2),0);
							printf("Recieve from server: %s",recv_str2) ;
							if(recvstr2Len<0)
								printf("Recieve fail. %d\n",errno);
							else if(recvstr2Len == 0){
	       		         		close(accept_data_socket);                     
		                        FD_CLR(accept_data_socket, &master_set);
								close(connect_data_socket);
								FD_CLR(connect_data_socket, &master_set);
							}
							else {
								recv_str2[recvstr2Len]='\0';printf("Send to client: %s ",recv_str2);
								if(strcmp("NULL",filename)!=0){
									saveFile(recv_str2,filename,recvstr2Len);
		       		         		printf("\nSaved: %s, len=%d",filename,strlen(filename));
		       		         	
								}else printf("Filename: %s",filename);
								if(send(accept_data_socket,recv_str2,recvstr2Len,0)<0)
									printf("PASV Data connection: send to client fail. Errno: %d", errno);}
                		}

		        }
        }
    }
}

void modifyPassiveResponse(char* str){
	char tail[255];
	int flag;
	char cmd[255]; 

	//printf("%s\n",str);
	
	for(int i=0;i<strlen(str);i++){
		if(str[i]==')')
			flag = i+1;
	}
	//printf("Flag=%d\n",flag);
	
	memset(tail, 0, sizeof(tail));
	//printf("Tail:%s\n",tail); 
	int j=0;
	int last_num;
	char send_str[70];
	for(int i=flag;i<strlen(str);i++){	
		//printf("Ch:%c\n",str[i]);
		tail[j]=str[i];
		j++;
	}
	//printf("Tail: %s\n",tail);
	last_num = 15 + data_send_times;
	sprintf(send_str,"227 Entering Passive Mode (192,168,56,101,39,%d)",last_num);
	strcpy(cmd,send_str);
	strcat(cmd,tail);
	strcpy(passive_response,cmd);
	
	//printf("Cmd: %s",cmd);
}

int getPort(char* str){
	char num1[3];
	char num2[3];
	int port;
	
	memset(num1, 0, sizeof(num1));
	num1[0]=str[40];
	num1[1]=str[41]; 
	
	memset(num2, 0, sizeof(num2));
	num2[0]=str[43];
	num2[1]=str[44]; 
	
	printf("num1:%s, num2:%s\n",num1,num2);
	port = atoi(num1)*256+atoi(num2);
	
	printf("port: %d\n",port); 
	
	return port;
} 

void getFilename(char* str, char* filename){
	char tail[255];
	int flag;
	
	for(int i=0;i<strlen(str);i++){
		if(str[i]==' ')
			flag = i+1;
	}
	
	memset(tail, 0, sizeof(tail));
	//printf("Tail:%s\n",tail); 
	int j=0;
	char send_str[70];
	for(int i=flag;i<strlen(str)-2;i++){//去掉末尾两位迷之问号 
		//printf("Ch:%c\n",str[i]);
		tail[j]=str[i];
		j++;
	}
	//printf("Tail: %s\n",tail);
	strcpy(filename,tail);
	
}

void saveFile(char* filecontents, char* filename, int size){
	
	if(fd_cache > 0){
		printf("\nDirectly write: %s\n",filename);
		write(fd_cache,filecontents,size);
	}else if((fd_cache = open(filename,O_RDWR|O_APPEND)) > 0 ){
		printf("\nOpen: %s\n",filename);
		write(fd_cache,filecontents,size);
	}else{
		printf("\nCreate: %s\n",filename);
		fd_cache = creat(filename,0777);
		write(fd_cache,filecontents,size);
	}
	
	if(size < 255 && fd_cache>0)
		{close(fd_cache);fd_cache = 0;}

}

void setCacheCatalog(char* filename){
	int fd;
	strcat(filename,"\n");
	if((fd = open("cache_catalog", O_RDWR|O_APPEND))>0){
		write(fd,filename,strlen(filename));
		close(fd);
	}else{
		fd  = creat("cache_catalog",0777);
		write(fd,filename,strlen(filename));
		close(fd);
	}		
}

int compareCacheCatalog(char* filename){
	int file_length;
	int fd;
	char buf[2];
	char existing_filename[255];
	memset(existing_filename, 0, sizeof(existing_filename));
	
	if((fd = open("cache_catalog", O_RDONLY))>0){
		file_length = (int)lseek(fd,0,SEEK_END) - (int)lseek(fd,0,SEEK_SET)+ 1;
		for(int i=1;i<file_length;i++){
			read(fd,buf,1);
			if(buf[0]=='\n'){
				//printf("%s\n",existing_filename);
				if(strcmp(filename,existing_filename)==0)
					{printf("\nFile \"%s\" exists!",existing_filename);close(fd);return 1;}
				memset(existing_filename, 0, sizeof(existing_filename));
				lseek(fd,i,SEEK_SET);
				continue;
			}else{
				strcat(existing_filename,buf);
				lseek(fd,i,SEEK_SET);
			}
		}
		close(fd);
		return 0;
	}else{
		fd  = creat("cache_catalog",0777);
		close(fd);
		return 0;
	}
	
}
void readFile(char *filename){
	//Count file size
	int fd;
	fd = open(filename,O_RDONLY);
	int length = (int)lseek(fd,0,SEEK_END) - (int)lseek(fd,0,SEEK_SET)+ 1;
	filecontents = (char*)malloc(length+1);
	memset(filecontents, 0, sizeof(filecontents));
	int read_size=0;
	cachefile_size = read(fd,filecontents,length);
	printf("\nFile size: %d\n",length);
	printf("Read size: %d",cachefile_size);
	printf("\nFile contents: %s\n", filecontents);	
	close(fd);	

	
}

char* resetport(char* buffer,int last2,int last1){

	int in = 0;
	int i = 0;
	char *p[10];//used to record the string which is separated by ','
	char *q = (char*)calloc(BUFFSIZE,sizeof(char));//used to record the other parts except 
	
	while((p[in]=strtok(buffer,","))!=NULL) {
		in++;
		buffer=NULL; 
	}
	strcpy(q,p[0]);//PORT 192
	strcat(q,",");//,
	for(i=1;i<(in-3);i++){//168,56,
		strcat(q,p[i]);
		strcat(q,",");
	}
	strcat(q,"101");//101
	strcat(q,",");//,
	
 	char *lasttwo = (char *)calloc(BUFFSIZE,sizeof(char));
	char *lastone = (char *)calloc(BUFFSIZE,sizeof(char));	
	
 	sprintf(lasttwo,"%d",last2);
  	sprintf(lastone,"%d",last1); 		     	
   	strcat(q,lasttwo);
   	strcat(q,",");
    strcat(q,lastone);
    strcat(q,"\r\n");	
	return q;
}

int convert(char buff[]){
	char *p;
	int last1;
	int last2;
	int count;
	p = strsep(&buff,",");
	for(count=0;count<5;count++){	
		p = strsep(&buff,",");
		if(count==3){
			last2 = atoi(p);	
		}
		if(count==4){
			last1 = atoi(p);	
		}
	}
	return last1 + last2 * 256;
}

void bindAndListenSocket(int sock,int port){
	
	int bin;
	int lst;
	struct sockaddr_in proxy;
	bzero(&proxy,sizeof(proxy));
	proxy.sin_family = AF_INET;
	proxy.sin_port = htons(port);
	proxy.sin_addr.s_addr = htonl(INADDR_ANY);
	if((bin = bind(sock,(struct sockaddr *)&proxy,sizeof(proxy))) == -1){
		printf("bind error\n");
		exit(-1);	
	}
	if((lst = listen(sock,6)) == -1){
		printf("listen error\n");
		exit(-1);	
	}

	
}

int acceptCmdSocket(int sock){
	int acpt;
	socklen_t addrlen;
	struct sockaddr_in client;
	addrlen = sizeof(client);
	if((acpt = accept(sock,(struct sockaddr *)&client,&addrlen)) == -1){
		printf("accept error");
		exit(-1);	
	}
	return acpt;
}

int acceptDataSocket(int sock){
	int acpt;
	socklen_t addrlen;
	struct sockaddr_in server;
	addrlen = sizeof(server);
	if((acpt = accept(sock,(struct sockaddr *)&server,&addrlen)) == -1){
		printf("accept error");
		exit(-1);	
	}
	return acpt;
}

void connectToServer(int sock,int port){
	int cont = 0;
	struct in_addr in;
	struct sockaddr_in server;
	bzero(&server,sizeof(server));
	inet_aton("192.168.56.1",&in);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = in.s_addr;
	server.sin_port = htons(port);
	if((cont=connect(sock,(struct sockaddr *)&server,sizeof(server))) < 0){
		printf("connect error1\n");
		printf("Error: %d\n",errno);
		exit(-1);	
	}
	printf("Connected\n");
	//return cont;
}

void connectToClient(int sock,int port){
	printf("Connect to %d\n",port);
	int cont;
	struct in_addr in;
	struct sockaddr_in server;
	bzero(&server,sizeof(server));
	inet_aton("192.168.56.1",&in);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = in.s_addr;
	server.sin_port = htons(port);
	if((cont=connect(sock,(struct sockaddr *)&server,sizeof(server))) < 0){
		printf("connect error2\n");
		exit(-1);	
	}
	printf("Connected2\n");
}

void actv_mode(){
	fd_set master_set, working_set;  
    struct timeval timeout;          
    int proxy_cmd_socket    = 0;     
    int accept_cmd_socket   = 0;    
    int connect_cmd_socket  = 0;     
    int proxy_data_socket   = 0;     
    int accept_data_socket  = 0;    
    int connect_data_socket = 0;     
	int selectResult = 0;     
    int select_sd = 25;    
    int newport = 0;
    int last2 = 97;
    int last1 = 168;
   	char filename[255];
 	int i;
	int send_cache = 0;
	int pasv = 0;//if it is passive modal, pasv = 1

    
    
    FD_ZERO(&master_set);   
    bzero(&timeout, sizeof(timeout));
    
    //Set up proxy_cmd_socket, bind and listen
	if((proxy_cmd_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		printf("proxy_cmd_socket faild\n");
	}

	int on = 1;
	if((setsockopt(proxy_cmd_socket, SOL_SOCKET, SO_REUSEADDR,&on,sizeof(on))) < 0){
		perror("setsockopt faild.");
		exit(1);
	}
    bindAndListenSocket(proxy_cmd_socket,21);
    FD_SET(proxy_cmd_socket, &master_set); 
   
	//Set up proxy_data_socket, bind and listen
	if((proxy_data_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		printf("proxy_data_socket faild\n");
	}
	

	int on1 = 1;
	if((setsockopt(proxy_data_socket, SOL_SOCKET, SO_REUSEADDR,&on1,sizeof(on1))) < 0){
		perror("setsockopt faild.");
		exit(1);
	}
	bindAndListenSocket(proxy_data_socket,last2*256+last1);

	FD_SET(proxy_data_socket, &master_set);
    
    timeout.tv_sec = 6000;    
    timeout.tv_usec = 0;    
    
    while (1) {
        FD_ZERO(&working_set); 
        memcpy(&working_set, &master_set, sizeof(master_set));
        
        
        selectResult = select(select_sd, &working_set, NULL, NULL, &timeout);
        
        
        if (selectResult < 0) {
            perror("select() failed\n");
            exit(1);
        }
        
        
        if (selectResult == 0) {
            printf("select() timed out.\n");
            continue;
        }
        
      
        for (i = 0; i < select_sd; i++) {
            
            if (FD_ISSET(i, &working_set)) {
            	
            	
                if (i == proxy_cmd_socket) {
                	printf("ENTER PROXYCMDSOCKET\n");
                	//Accept, set up the connection between client and proxy
                    accept_cmd_socket = acceptCmdSocket(proxy_cmd_socket); 
					if ((connect_cmd_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
						printf("connect_cmd_socketet() failed.\n");
						//Connect, set up the connection between proxy and server
                    connectToServer(connect_cmd_socket,21);
                    FD_SET(accept_cmd_socket, &master_set);
                    FD_SET(connect_cmd_socket, &master_set);
                }
                
                //Transfer the message from client to server
                if (i == accept_cmd_socket) {
                	printf("ENTER ACCEPTCMDSOCKET\n");
                	char buff1[BUFFSIZE] = {0};
              		memset(buff1,0,BUFFSIZE);
                	int size = 0;
                	
                	size = read(i, buff1, BUFFSIZE);
                    if (size <= 0) {
                        close(i); 
                        close(connect_cmd_socket);
                        FD_CLR(i, &master_set);
                        FD_CLR(connect_cmd_socket, &master_set);
                        i = 0;
                        connect_cmd_socket = 0;
                    }
					else if(strncmp(buff1,"PORT",4)==0){//active mode
                        	
                    	printf("receive from client: %s\n",buff1);
                    	char *equ = (char *)calloc(BUFFSIZE,sizeof(char));
						strcpy(equ,buff1);	
						char* temp = resetport(equ,last2,last1); // new PORT message
						char buf[BUFFSIZE];
                    	memset(buf, 0, sizeof(buf));
                    	strcpy(buf,temp);
                    	printf("send to server: %s \n",buf);
						write(connect_cmd_socket, buf, strlen(buf));
					 	newport = convert(buff1);// client data connection port
                    	printf("new port is : %d\n",newport);
					}else if(strncmp(buff1,"MLSD",4) == 0){// If MLSD, reset filename 
   								memset(filename, 0, sizeof(filename));
	       		         		strcpy(filename,"NULL");
								printf("Send to server: %s",buff1);
								send(connect_cmd_socket,buff1,strlen(buff1),0);
					}else if(strncmp(buff1,"RETR",4) == 0){
	       		         		memset(filename, 0, sizeof(filename));//Reset filename 
	       		         		strcpy(filename,"NULL");//Initialize filename with NULL
	         		       		getFilename(buff1,filename);//cache filename
								if(compareCacheCatalog(filename)==1){//if file exist
									send_cache = 1;
	         		       			strcat(filename,"\n");
									readFile(filename);
									printf("YIDUUUUUUUUUUUUUUUUUUU cachefile_size: %d\n",cachefile_size);
									char connectionAccepted[]="150 Connection accepted\r\n";
									send(accept_cmd_socket,connectionAccepted,strlen(connectionAccepted),0);
									printf("Send to client: %s\n",connectionAccepted);
	         		       			printf("Be ready to send cache: %s\n",filecontents);
	         		       			if ((connect_data_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
										printf("connect_data_socketet() failed.\n");										
									connectToClient(connect_data_socket,newport);
									if(send(connect_data_socket,filecontents,cachefile_size,0)<0)
										printf("\nSend cache fail. Errno: %d\n",errno);//Download from proxy
	         		       			else printf("Send cache: %s\n",filecontents);
	         		       			char transferOK[]="226 Transfer OK\r\n";
									send(accept_cmd_socket,transferOK,strlen(transferOK),0);
									printf("Send to client: %s\n",transferOK);
									close(connect_data_socket);
									FD_CLR(connect_data_socket, &master_set);
									send_cache = 0;
	         		       	
	         		       		}else{//if file doesn't exist, be ready to cache such file, and send cmd to server
									printf("Cache file: %s\n",filename);
									setCacheCatalog(filename);
									printf("Send to server: %s",buff1);
									send(connect_cmd_socket,buff1,strlen(buff1),0);
		       		         	}
	         		       	}	
					else{
						printf("send to server:%s \n",buff1);
						write(connect_cmd_socket, buff1, strlen(buff1));
					}
							
                    
                }
                //Transfer the message from server to client
                if (i == connect_cmd_socket) {
                	printf("ENTER CONNECTCMDSOCKET\n");
                  	char buff2[BUFFSIZE] = {0};
     	          	memset(buff2, 0, BUFFSIZE);
                  	int size = 0;
                  	size = read(i, buff2, BUFFSIZE);
               		if (size <= 0) {
                        close(i); 
                        close(accept_cmd_socket);
						FD_CLR(i, &master_set);
                        FD_CLR(accept_cmd_socket,&master_set);
                        i = 0;
						accept_cmd_socket = 0; 
                    }
					else {
						buff2[size] = '\0';

						
						printf("send to client: %s\n",buff2);
						write(accept_cmd_socket, buff2, strlen(buff2));
						
						
  					}
                  
                }
                
                if (i == proxy_data_socket) {
                  
			  		printf("ENTER PROXYDATASOCKET\n");
				//Accept, set up the data connection between server and proxy

						accept_data_socket = acceptDataSocket(proxy_data_socket);
					//Connect, set up the connection between proxy and client
						if ((connect_data_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
							printf("connect_data_socketet() failed.\n");
						
						connectToClient(connect_data_socket,newport);
                    FD_SET(accept_data_socket, &master_set);
                    FD_SET(connect_data_socket, &master_set);
                   
 				}
                  
                //Transfer the data from server to client
                if (i == accept_data_socket) {
                	printf("ENTER ACCEPTDATASOCKET\n");
                    char buff3[BUFFSIZE] = {0};
                    memset(buff3, 0, BUFFSIZE);
                	int size = 0;
                	size = read(i, buff3, BUFFSIZE);
                    if (size <= 0) {
                        close(i); 
                        close(proxy_data_socket);
                        close(connect_data_socket);
                        FD_CLR(i, &master_set);
                        FD_CLR(proxy_data_socket,&master_set);
                        FD_CLR(connect_data_socket, &master_set);                  
                        connect_data_socket = 0;
                        accept_data_socket = 0;
                        proxy_data_socket = 0;                     
						//restart the socket4
						if((proxy_data_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
							printf("proxy_data_socket faild\n");
						}

				    	last1++; //new listening port of proxy
					
						bindAndListenSocket(proxy_data_socket,last2*256+last1);
                       	FD_SET(proxy_data_socket, &master_set);

                    } 
					else {
							 buff3[size] = '\0';
								if(strcmp("NULL",filename)!=0){
									saveFile(buff3,filename,size);
		       		         		printf("\nSaved: %s, len=%d",filename,strlen(filename));

								}else printf("Filename: %s",filename);
                       
						//printf("data connect receive from client: %s \n",buff3);	
						write(connect_data_socket, buff3, size);
                    }
                    
                }
                //Transfer the data from client to server
                if (i == connect_data_socket) {
                	printf("ENTER CONNECTDATASOCKET\n");
                    char buff4[BUFFSIZE] = {0};                    
     	          	memset(buff4, 0, BUFFSIZE);
                  	int size = 0;
                  	size = read(i, buff4, BUFFSIZE);
               		if (size <= 0) {
                        close(i);
                        close(proxy_data_socket);
                        close(accept_data_socket);

                        FD_CLR(i, &master_set);
                        FD_CLR(proxy_data_socket,&master_set);
                        FD_CLR(accept_data_socket,&master_set);
                        connect_data_socket = 0;
                        accept_data_socket = 0;
                        proxy_data_socket = 0;
                        //restart the socket 4
						if((proxy_data_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
							printf("proxy_data_socket faild\n");
						}
						last1++;// new listening port of proxy

						
						int on3 = 1;
						if((setsockopt(proxy_data_socket, SOL_SOCKET, SO_REUSEADDR,&on3,sizeof(on3))) < 0){
							perror("setsockopt faild.");
							exit(1);
						}
						bindAndListenSocket(proxy_data_socket,last2*256+last1);

                       	FD_SET(proxy_data_socket, &master_set);
                    } 
					else {
						if(strcmp("NULL",filename)!=0){
							saveFile(buff4,filename,size);
	   		         		//printf("\nSaved: %s, len=%d\n",filename,strlen(filename));
	   		         		memset(filename, 0, sizeof(filename));
	   		         		strcpy(filename,"NULL");
						}
						else 
							printf("Filename: %s\n",filename);
						buff4[size] = '\0';
						
						//printf("receive from server:%s \n",buff4);

						write(accept_data_socket, buff4, size);
                    }
                }
            }
        }
    }
}
