#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include "structures.h"

void logIn(long *clientId);

void logOut(long *clientId);

void readFromBuffer(char *buffer, int size, char *destination);

void showOnlineUser(long clientId);

void showMembers(long clientId);

void showAvailableGroups(long clientId);

void joinGroup(long clientId);

void leaveGroup(long clientId);

void sendMessage(long clientId, char typeOfMessage);

void getMessage(long clientId);

void readShowResponse(long clientId);


int main() {
    while (1) {
        long clientId = -1;
        logIn(&clientId);
        int logged = 1;
        int process = fork();
        while (logged) {
            if (process == 0) {
                getMessage(clientId);
            } else {
                char textPrint[] = "Choose option by writing correct number: \n"
                                   "1 - show online users \n"
                                   "2 - show members of group \n"
                                   "3 - show available groups \n"
                                   "4 - send message to user \n"
                                   "5 - send message to group \n"
                                   "6 - join group \n"
                                   "7 - leave group\n"
                                   "8 - log out\n"
                                   "9 - close program \n";

                write(1, textPrint, strlen(textPrint) + 1);

                int size = read(0, textPrint, 3);
                fflush(stdin);
                char input[4];
                readFromBuffer(textPrint, size, input);
                int counter = 0;
                int option = readInt(&counter, input);

                switch (option) {
                    case 1:
                        showOnlineUser(clientId);
                        break;
                    case 2:
                        showMembers(clientId);
                        break;
                    case 3:
                        showAvailableGroups(clientId);
                        break;
                    case 4:
                        sendMessage(clientId, 'u');
                        break;
                    case 5:
                        sendMessage(clientId, 'g');
                        break;
                    case 6:
                        joinGroup(clientId);
                        break;
                    case 7:
                        leaveGroup(clientId);
                        break;
                    case 8:
                        logOut(&clientId);
                        logged = 0;
                        break;
                    case 9:
                        logOut(&clientId);
                        exit(0);
                    default:
                        strcpy(textPrint, "Wrong argument. Try once again \n");
                        write(1, textPrint, strlen(textPrint) + 1);
                }
            }
        }
    }
}

void logIn(long *clientId) {

    int loginQueue = msgget(1026, 0666 | IPC_CREAT);

    while (*clientId == -1) {
        char buffer[64];
        int size;
        LogInRequest logInRequest;
        logInRequest.recipient = 1;
        logInRequest.sender = getpid();
        int loginRequestSize = sizeof(logInRequest) - sizeof(long);

        strcpy(buffer, "Enter the login: ");
        write(1, buffer, strlen(buffer) + 1);
        size = read(0, buffer, 64);
        readFromBuffer(buffer, size, logInRequest.login);

        strcpy(buffer, "Enter the password: ");
        write(1, buffer, strlen(buffer) + 1);
        size = read(0, buffer, 64);
        readFromBuffer(buffer, size, logInRequest.password);

        msgsnd(loginQueue, &logInRequest, loginRequestSize, 0);

        int requestQueue = msgget(1024, 0666 | IPC_CREAT);
        ServerRequest serverRequest = {getpid(), 1, ""};
        int requestSize = sizeof(serverRequest) - sizeof(long);
        msgsnd(requestQueue, &serverRequest, requestSize, 0);

        LogInResponse logInResponse;
        int loginResponseSize = sizeof(logInResponse) - sizeof(long);
        msgrcv(loginQueue, &logInResponse, loginResponseSize, getpid(), 0);
        *clientId = logInResponse.idClient;

        if (*clientId == -1) {
            strcpy(buffer, "Try once again\n");
            write(1, buffer, strlen(buffer));
        } else {
            strcpy(buffer, "Successfully logged in\n");
            write(1, buffer, strlen(buffer));
        }
    }
}

void logOut(long *clientId) {
    int requestQueue = msgget(1024, 0666 | IPC_CREAT);
    ServerRequest serverRequest = {*clientId, 2, ""};
    int requestSize = sizeof(serverRequest) - sizeof(long);
    msgsnd(requestQueue, &serverRequest, requestSize, 0);
    *clientId = -1;
}

void showOnlineUser(long clientId) {
    int requestQueue = msgget(1024, 0666 | IPC_CREAT);
    ServerRequest serverRequest = {clientId, 3, ""};
    int requestSize = sizeof(serverRequest) - sizeof(long);
    msgsnd(requestQueue, &serverRequest, requestSize, 0);
    char textPrint[64];
    strcpy(textPrint, "\nList of online Users\n");
    write(1, textPrint, strlen(textPrint) + 1);
    readShowResponse(clientId);
}

void showMembers(long clientId) {
    int requestQueue = msgget(1024, 0666 | IPC_CREAT);
    ServerRequest serverRequest = {clientId, 4,};
    int requestSize = sizeof(serverRequest) - sizeof(long);

    char textBuffer[64] = "Enter the group name: ";
    write(1, textBuffer, strlen(textBuffer) + 1);
    int size = read(0, textBuffer, 64);
    readFromBuffer(textBuffer, size, serverRequest.extra);

    msgsnd(requestQueue, &serverRequest, requestSize, 0);

    char textPrint[64];
    strcpy(textPrint, "\nList of members of the group:\n");
    write(1, textPrint, strlen(textPrint) + 1);
    readShowResponse(clientId);
}

void showAvailableGroups(long clientId) {
    int requestQueue = msgget(1024, 0666 | IPC_CREAT);
    ServerRequest serverRequest = {clientId, 5};
    int requestSize = sizeof(serverRequest) - sizeof(long);
    msgsnd(requestQueue, &serverRequest, requestSize, 0);
    char textPrint[64];
    strcpy(textPrint, "\nList of available groups\n");
    write(1, textPrint, strlen(textPrint) + 1);
    readShowResponse(clientId);
}

void readShowResponse(long clientId) {
    int showQueue = msgget(1027, 0666 | IPC_CREAT);
    ShowResponse showResponse;
    int responseSize = sizeof(showResponse) - sizeof(long);
    msgrcv(showQueue, &showResponse, responseSize, clientId, 0);
    char textPrint[64];
    if (showResponse.listLength == -1) {
        strcpy(textPrint, "There is no group with this name\n\n");
        write(1, textPrint, strlen(textPrint) + 1);
    } else {

        for (int i = 0; i < showResponse.listLength; i++) {
            readString(0, showResponse.list, textPrint);
            write(1, textPrint, strlen(textPrint) + 1);
            strcpy(textPrint, "\n");
            write(1, textPrint, strlen(textPrint) + 1);
            if (i < showResponse.listLength - 1)
                msgrcv(showQueue, &showResponse, responseSize, clientId, 0);
        }
        write(1, textPrint, strlen(textPrint) + 1);
    }
}

void joinGroup(long clientId) {
    int requestQueue = msgget(1024, 0666 | IPC_CREAT);
    ServerRequest serverRequest = {clientId, 6};
    int requestSize = sizeof(serverRequest) - sizeof(long);

    char textBuffer[64] = "Enter the group name: ";
    write(1, textBuffer, strlen(textBuffer) + 1);
    int size = read(0, textBuffer, 64);
    readFromBuffer(textBuffer, size, serverRequest.extra);

    msgsnd(requestQueue, &serverRequest, requestSize, 0);

}

void leaveGroup(long clientId) {
    int requestQueue = msgget(1024, 0666 | IPC_CREAT);
    ServerRequest serverRequest = {clientId, 7};
    int requestSize = sizeof(serverRequest) - sizeof(long);

    char textBuffer[64] = "Enter the group name: ";
    write(1, textBuffer, strlen(textBuffer) + 1);
    int size = read(0, textBuffer, 64);
    readFromBuffer(textBuffer, size, serverRequest.extra);

    msgsnd(requestQueue, &serverRequest, requestSize, 0);

}

void sendMessage(long clientId, char typeOfMessage) {
    int messageQueue = msgget(1025, 0666 | IPC_CREAT);
    Message message = {1, typeOfMessage, "", "", clientId, ""};
    int messageSize = sizeof(message) - sizeof(long);

    char textBuffer[1024] = "Enter the recipient name: ";
    write(1, textBuffer, strlen(textBuffer) + 1);
    int size = read(0, textBuffer, 64);
    readFromBuffer(textBuffer, size, message.nameRecipient);

    strcpy(textBuffer, "Enter the text of message: ");
    write(1, textBuffer, strlen(textBuffer) + 1);
    size = read(0, textBuffer, 2048);
    readFromBuffer(textBuffer, size, message.text);

    msgsnd(messageQueue, &message, messageSize, 0);

    int requestQueue = msgget(1024, 0666 | IPC_CREAT);
    ServerRequest serverRequest = {clientId, 8};
    int requestSize = sizeof(serverRequest) - sizeof(long);
    msgsnd(requestQueue, &serverRequest, requestSize, 0);
}

void getMessage(long clientId) {
    int messageQueue = msgget(1025, 0666 | IPC_CREAT);
    Message message;
    int messageSize = sizeof(message) - sizeof(long);
    msgrcv(messageQueue, &message, messageSize, clientId, 0);
    char printText[160];
    if (message.typeOfMessage == 'u')
        sprintf(printText, "You get direct message from %s\n", message.nameSender);
    else if (message.typeOfMessage == 'g')
        sprintf(printText, "You get message in %s from %s\n", message.nameRecipient, message.nameSender);
    else if (message.typeOfMessage == 'k')
        exit(0);
    else
        strcpy(printText,"");
    write(1, printText, strlen(printText) + 1);
    write(1, message.text, strlen(message.text) + 1);
    strcpy(printText, "\n");
    write(1, printText, strlen(printText) + 1);
}

void readFromBuffer(char *buffer, int size, char *destination) {
    strncpy(destination, buffer, size);
    destination[size - 1] = '\0';
}


