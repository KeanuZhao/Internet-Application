# Internet-Application
Project from Internet Application course in BUPT 

Requirements Analysis<br>
Development Environment<br>
1. Windows system with FileZilla client and FileZilla server 
2. Oracle VirtualBox with Linux Ubuntu<br>
3. C language<br>

Detailed Function Requirements<br>
1. Upload and Download. This part of function contains several sub-functions: Firstly, the completion of control connection and data connection. Control connection is responsible for passing control commands between client and server, such as USER, PASS, PORT, PASV, STOR, RETR. Data connection is responsible for passing the contents in a designated file. Control connection is established by proxy_cmd_socket, connect_cmd_socket and accept_cmd_socket and data connection is established by proxy_data_socket, connect_data_socket and accept_data_socket. Secondly, the reconnection of control connection and data connection. In the first connection, client sends USER, PASS, PORT(or PASV), MLSD command to server and obtains the file directory of server, after that, data connection and control connection should be cut off. Otherwise, if you want to download/upload a file, the control connection and data connection cannot be established again because of the existence of previous one, then download/upload failure occurs.<br>

2. Cache. The precondition is downloading smoothly. To achieve this goal, two catchers(buffers) are indispensable. One at the accept_cmd_socket and the other at the accept_data_socket in active mode or connect_data_socket in passive mode. At accept_cmd_socket, the first catcher need to intercepts the RETR command to get the file name because it follows “RETR”. At accept_data_socket or connect_data_socket, the second catcher needs to store all the data from the socket. Then write the contents of the second catcher into the a file whose name is stored in the contents of the first catcher. There is an exception, if first catcher is empty but the second isn’t, it indicates that the contents of the second catcher are directory which is unnecessary to be stored in a file.
