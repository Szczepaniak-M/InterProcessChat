# Short-Message-Service
## Compilation:
To compile all files and get executable files, you can use `Makefile` by writing command `make` in a console.
In a result, you get to executable file: `server` and `client`. 
## Running
To run `server` type `./server` in a console.
To close `server` use `Ctrl + C`.
To run `client` type `./client` in a console.
If you are logged in, to close `client` use proper option.
If you aren't logged in, to close `client` use `Ctrl + C`.

If you have problem with too many messages in buffer and there is no space for new messages type:
`sudo sysctl -w kernel.msgmnb=65535`
This will change maximum space for messages in IPC queue to 65535 bytes .

## Files
### server.c
File with source code of `server` file. The task of this code is to process a request from users, 
sending message to other users / groups through the server and receiving messages from other users through the server.
Functions of server:
- read data about users and groups from `config.txt`
- Verifying the process of logging in by client
- Management the lists of users, online users and groups
- Sending list of online users to the client
- Sending list of members of the given group to the client
- Sending list of available groups to the client
- Receiving messages from clients
- Verifying the recipients of the messages
- Sending message to a user
- Sending message to a user in a given group

### client.c
File with source code of `client` file. The task of this code is to input data from console, make a request to server
sending message to other users / groups through the server and receiving messages from other users through the server.
Functions of client:
- Logging in
- Logging out
- Displaying list of online users
- Displaying list of members of the given group
- Displaying list of available groups
- Joining to the given group
- Leaving the given group
- Sending message to a user
- Sending messages to users in a given group
- Receiving and displaying messages provided by server from other users

The programs use 2 processes. The responsibility of the first process is receiving and displaying messages. 
The responsibility of the second process is executing user requests like sending messages or showing lists.

### structures.h
Header file contains definition of all structures using to communication client-server 
by IPC queue. The file contains also two functions to parsing string using by `server.c` and `client.c`.
The details of communication between clients are in file `PROTOCOL`


 
