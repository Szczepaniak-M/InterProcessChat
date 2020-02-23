#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <signal.h>
#include "structures.h"

void logIn(int *clientId, int *queueId);

void logOut(int *clientId, int *queueId);

void sendRequest(int queueId, int typeOfRequest, char *textPrint);

void sendMessage(int clientId, int queueId, int typeOfMessage);

void getMessage(int queueId);

void readShowResponse(int queueId);

int main() {
    int clientId = -1;
    int queueId = -1;
    while (1) {
        logIn(&clientId, &queueId);
        int logged = 1;
        int process = fork();
        while (logged) {
            if (process == 0) {
                getMessage(queueId);
            } else {
                char textPrint[] = "\nChoose option by writing correct number: \n"
                                   "1 - show online users \n"
                                   "2 - show available groups \n"
                                   "3 - show members of group \n"
                                   "4 - send message to user \n"
                                   "5 - send message to group \n"
                                   "6 - join group \n"
                                   "7 - leave group\n"
                                   "8 - block user\n"
                                   "9 - block group\n"
                                   "10 - unblock user\n"
                                   "11 - unblock group\n"
                                   "12 - log out\n"
                                   "13 - close program \n";

                write(1, textPrint, strlen(textPrint) + 1);

                int size = read(0, textPrint, 3);
                fflush(stdin);
                char input[4];
                readFromBuffer(textPrint, size, input);
                int counter = 0;
                int option = readInt(&counter, input);
                switch (option) {
                    case 1:
                        sendRequest(queueId, 2, "\nList of online Users\n");
                        readShowResponse(queueId);
                        break;
                    case 2:
                        sendRequest(queueId, 3, "\nList of available groups\n");
                        readShowResponse(queueId);
                        break;
                    case 3:
                        sendRequest(queueId, 4, "Type the Group name: ");
                        write(1, "\nList of members of the group:\n", 32);
                        readShowResponse(queueId);
                        break;
                    case 4:
                        sendMessage(clientId, queueId, 0);
                        break;
                    case 5:
                        sendMessage(clientId, queueId, 1);
                        break;
                    case 6:
                        sendRequest(queueId, 5, "Type the Group name: ");
                        break;
                    case 7:
                        sendRequest(queueId, 6, "Type the Group name: ");
                        break;
                    case 8:
                        sendRequest(queueId, 7, "Type the User login: ");
                        break;
                    case 9:
                        sendRequest(queueId, 8, "Type the Group name: ");
                        break;
                    case 10:
                        sendRequest(queueId, 9, "Type the User login: ");
                        break;
                    case 11:
                        sendRequest(queueId, 10, "Type the Group name: ");
                        break;
                    case 12:
                        logOut(&clientId, &queueId);
                        logged = 0;
                        break;
                    case 13:
                        logOut(&clientId, &queueId);
                        exit(0);
                    default:
                        strcpy(textPrint, "Wrong argument. Try once again \n");
                        write(1, textPrint, strlen(textPrint) + 1);
                }
            }
        }
    }
}

void logIn(int *clientId, int *queueId) {
    int loginQueue = -1;
    while (loginQueue == -1) {
        char textBuffer[32];
        sprintf(textBuffer, "Type Login Queue key: ");
        write(1, textBuffer, strlen(textBuffer) + 1);
        read(0, textBuffer, 32);
        int counter = 0;
        loginQueue = readInt(&counter, textBuffer);
        loginQueue = msgget(loginQueue, 0600);
    }

    while (*clientId < 0) {

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

        int success = msgsnd(loginQueue, &logInRequest, loginRequestSize, 0);
        if (success == -1)
        {
            write(1, "Error. Lost connection with server.\n", 37);
            exit(0);
        }

        LogInResponse logInResponse;
        int loginResponseSize = sizeof(logInResponse) - sizeof(long);
        msgrcv(loginQueue, &logInResponse, loginResponseSize, getpid(), 0);
        if (success == -1)
        {
            write(1, "Error. Lost connection with server.\n", 37);
            exit(0);
        }
        *clientId = logInResponse.clientId;
        *queueId = logInResponse.queueId;

        if (*clientId == -4) {
            strcpy(buffer, "There is no account with this login\n");
        } else if (*clientId == -3) {
            strcpy(buffer, "Too many failed login attempts. The user's account is blocked\n");
        } else if (*clientId == -1 || *clientId == -2) {
            sprintf(buffer, "Wrong password. %d times to block this account\n", 3 + *clientId);
        } else {
            strcpy(buffer, "Successfully logged in\n");
        }
        write(1, buffer, strlen(buffer) + 1);
    }
}

void sendRequest(int queueId, int typeOfRequest, char *textPrint) {
    write(1, textPrint, strlen(textPrint) + 1);

    ServerRequest serverRequest = {typeOfRequest, ""};
    int requestSize = sizeof(serverRequest) - sizeof(long);
    char textBuffer[32];
    if (typeOfRequest > 3) {
        int size = read(0, textBuffer, 32);
        readFromBuffer(textBuffer, size, serverRequest.extra);
    }

    int success = msgsnd(queueId, &serverRequest, requestSize, 0);
    if (success == -1)
    {
        write(1,"Error. Lost connection with server.\n\n", 37);
        exit(0);
    }
}

void readShowResponse(int queueId) {
    ShowResponse showResponse;
    int responseSize = sizeof(showResponse) - sizeof(long);
    msgrcv(queueId, &showResponse, responseSize, 14, 0);
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
                msgrcv(queueId, &showResponse, responseSize, 14, 0);
        }
        write(1, textPrint, strlen(textPrint) + 1);
    }
}

void sendMessage(int clientId, int queueId, int typeOfMessage) {
    Message message = {12, clientId, queueId, typeOfMessage, "", "", clientId, ""};
    int messageSize = sizeof(message) - sizeof(long);

    char textBuffer[2048] = "Type the recipient name: ";
    write(1, textBuffer, strlen(textBuffer) + 1);
    int size = read(0, textBuffer, 64);
    readFromBuffer(textBuffer, size, message.nameRecipient);

    strcpy(textBuffer, "Type the text of message: ");
    write(1, textBuffer, strlen(textBuffer) + 1);
    size = read(0, textBuffer, 2048);
    readFromBuffer(textBuffer, size, message.text);

    int success = msgsnd(queueId, &message, messageSize, 0);
    if (success == -1)
    {
        write(1, "Error during sending the message. You were logged out\n",55);
        exit(0);
    }
    ServerRequest serverRequest = {1, ""};
    int requestSize = sizeof(serverRequest) - sizeof(long);
    msgsnd(queueId, &serverRequest, requestSize, 0);
}

void getMessage(int queueId) {
    Message message;
    int messageSize = sizeof(message) - sizeof(long);
    int success = msgrcv(queueId, &message, messageSize, 13, 0);
    if (success == -1) {
        exit(0);
    }
    char printText[160];
    if (message.typeOfMessage == 0)
        sprintf(printText, "\nFrom %s: ", message.nameSender);
    else if (message.typeOfMessage == 1)
        sprintf(printText, "\nIn Group %s from %s: ", message.nameRecipient, message.nameSender);
    else if (message.typeOfMessage == 2)
        strcpy(printText, "\nFrom Server: ");
    else
        exit(0);
    write(1, printText, strlen(printText) + 1);
    write(1, message.text, strlen(message.text) + 1);
    strcpy(printText, "\n");
    write(1, printText, strlen(printText) + 1);
}

void logOut(int *clientId, int *queueId) {
    ServerRequest serverRequest = {11, ""};
    int requestSize = sizeof(serverRequest) - sizeof(long);
    msgsnd(*queueId, &serverRequest, requestSize, 0);
    write(1, "You successfully logged out\n", 29);
    *clientId = -1;
    *queueId = -1;
}
