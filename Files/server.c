#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include "structures.h"

void readData(User *users, Group *groups, int *userNumber, int *groupsNumber);

void readUser(char *line, User *users, int *usersNumber);

void readGroup(char *line, Group *groups, int *groupsNumber);

void logIn(User *users, long *onlineUsers, int *onlineUsersNumber);

void logOut(long clientID, long *onlineUsers, int *onlineUsersNumber);

void showOnlineUser(long clientID, User *users, long *onlineUsers, int onlineUsersNumber);

void showMembers(long clientID, char *extra, Group *groups, User *users);

void showAvailableGroups(long clientID, Group *groups);

char *findUserById(long id, User *users);

void joinGroup(long clientID, char *extra, Group *groups, User *users);

void leaveGroup(long clientID, char *extra, Group *groups, User *users);

void getMessage(Group *groups, User *users);

void sendMessage(Message message, User *users);

void sendUserMessage(Message message, User *users);

void sendGroupMessage(Message message, Group *groups, User *users);

void checkBlock();

void mySignal();

int main() {
    signal(SIGINT, mySignal);
    User users[512];
    int usersNumber = 0;
    long onlineUsers[512];
    int onlineUsersNumber = 0;
    Group groups[512];
    int groupsNumber = 0;
    readData(users, groups, &usersNumber, &groupsNumber);
    ServerRequest serverRequest;
    int requestSize = sizeof(serverRequest) - sizeof(long);
    int requestQueue = msgget(1024, 0666 | IPC_CREAT);
    while (1) {
        msgrcv(requestQueue, &serverRequest, requestSize, 0, 0);
        switch (serverRequest.typeOfRequest) {
            case 1:
                logIn(users, onlineUsers, &onlineUsersNumber);
                break;
            case 2:
                logOut(serverRequest.idClient, onlineUsers, &onlineUsersNumber);
                break;
            case 3:
                showOnlineUser(serverRequest.idClient, users, onlineUsers, onlineUsersNumber);
                break;
            case 4:
                showMembers(serverRequest.idClient, serverRequest.extra, groups, users);
                break;
            case 5:
                showAvailableGroups(serverRequest.idClient, groups);
                break;
            case 6:
                joinGroup(serverRequest.idClient, serverRequest.extra, groups, users);
                break;
            case 7:
                leaveGroup(serverRequest.idClient, serverRequest.extra, groups, users);
                break;
            case 8:
                getMessage(groups, users);
                break;
        }
    }
    return 0;
}

void readData(User *users, Group *groups, int *userNumber, int *groupsNumber) {
    char ch;
    char *line;
    int file_id = open("config.txt", O_RDONLY);
    while (read(file_id, &ch, 1)) {
        int counter = 2;
        while (read(file_id, &ch, 1) && ch != '\n') {
            counter++;
        }
        lseek(file_id, -counter, SEEK_CUR);
        line = malloc(sizeof(char) * counter);

        read(file_id, line, counter);
        if (line[0] == 'u')
            readUser(line, users, userNumber);
        else
            readGroup(line, groups, groupsNumber);
        free(line);
    }

    close(file_id);
}

void readUser(char *line, User *users, int *usersNumber) {
    int counter = 2;
    int id = readInt(&counter, line);
    users[*usersNumber].id = id;

    counter = readString(counter, line, users[*usersNumber].login);
    readString(counter, line, users[*usersNumber].password);
    (*usersNumber)++;
}

void readGroup(char *line, Group *groups, int *groupsNumber) {
    int counter = 2;
    int id = readInt(&counter, line);
    groups[*groupsNumber].id = id;
    counter = readString(counter, line, groups[*groupsNumber].name);

    int counterMembers = 0;
    while (line[counter] != '\n')
        groups[*groupsNumber].members[counterMembers++] = readInt(&counter, line);

    groups[*groupsNumber].membersNumber = counterMembers;
    (*groupsNumber)++;
}

void logIn(User *users, long *onlineUsers, int *onlineUsersNumber) {
    int loginQueue = msgget(1026, 0666 | IPC_CREAT);
    LogInRequest loginRequest;
    int requestSize = sizeof(loginRequest) - sizeof(long);
    msgrcv(loginQueue, &loginRequest, requestSize, 1, 0);
    LogInResponse logInResponse = {loginRequest.sender, -1};
    int responseSize = sizeof(logInResponse) - sizeof(long);
    int success = 0;
    for (int i = 0; i < 512; i++) {
        if (strcmp(loginRequest.login, users[i].login) == 0) {

            if (strcmp(loginRequest.password, users[i].password) == 0) {
                logInResponse.idClient = users[i].id;
                onlineUsers[(*onlineUsersNumber)++] = logInResponse.idClient;
                success = 1;
            }
            break;
        }
    }
    msgsnd(loginQueue, &logInResponse, responseSize, 0);

    char textPrint[100];
    if (success) {
        sprintf(textPrint, "User with ID %ld successfully logged in\n", logInResponse.idClient);
        write(1, textPrint, strlen(textPrint) + 1);
    } else {
        strcpy(textPrint, "Unsuccessfully logging\n");
        write(1, textPrint, strlen(textPrint) + 1);
    }
}

void logOut(long clientId, long *onlineUsers, int *onlineUsersNumber) {
    int found = 0;
    for (int i = 0; i < 512; i++) {
        if (found) {
            if (i < 511)
                onlineUsers[i] = onlineUsers[i + 1];
            else
                onlineUsers[i] = -1;
        } else {
            if (onlineUsers[i] == clientId) {
                found = 1;
                if (i < 511)
                    onlineUsers[i] = onlineUsers[i + 1];
                else
                    onlineUsers[i] = -1;
            }
        }
    }
    int messageQueue = msgget(1025, 0666 | IPC_CREAT);
    Message kill = {clientId, 'k', "", "", 1, "server"};
    int killSize = sizeof(kill) - sizeof(long);
    msgsnd(messageQueue, &kill, killSize, 0);
    (*onlineUsersNumber)--;
    char textPrint[100];
    sprintf(textPrint, "User with ID %ld successfully logged out\n", clientId);
    write(1, textPrint, strlen(textPrint) + 1);
}

void showOnlineUser(long clientId, User *users, long *onlineUsers, int onlineUsersNumber) {
    int showQueue = msgget(1027, 0666 | IPC_CREAT);
    ShowResponse showResponse = {clientId, onlineUsersNumber, ""};
    int responseSize = sizeof(showResponse) - sizeof(long);
    for (int i = 0; i < onlineUsersNumber; i++) {
        char *name = findUserById(onlineUsers[i], users);
        strcpy(showResponse.list, name);
        showResponse.listLength = onlineUsersNumber;
        msgsnd(showQueue, &showResponse, responseSize, 0);
    }
    char textPrint[100];
    sprintf(textPrint, "User with ID %ld got list of Online Users\n", clientId);
    write(1, textPrint, strlen(textPrint) + 1);
}

void showMembers(long clientId, char *extra, Group *groups, User *users) {
    int showQueue = msgget(1027, 0666 | IPC_CREAT);
    ShowResponse showResponse = {clientId, -1, ""};
    int responseSize = sizeof(showResponse) - sizeof(long);
    int idx = -1;

    for (int i = 0; i < 512; i++) {
        if (strcmp(extra, groups[i].name) == 0) {
            idx = i;
            break;
        }
    }

    char textPrint[100];
    if (idx > -1) {
        showResponse.listLength = groups[idx].membersNumber;
        for (int i = 0; i < groups[idx].membersNumber; i++) {
            char *name = findUserById(groups[idx].members[i], users);
            strcpy(showResponse.list, name);
            msgsnd(showQueue, &showResponse, responseSize, 0);
        }
        sprintf(textPrint, "User with ID %ld successfully got the list of members of group %s\n", clientId, extra);
        write(1, textPrint, strlen(textPrint) + 1);
    } else {
        sprintf(textPrint, "User with ID %ld did not get the list of members of group %s\n", clientId, extra);
        write(1, textPrint, strlen(textPrint) + 1);
        msgsnd(showQueue, &showResponse, responseSize, 0);
    }
}

void showAvailableGroups(long clientId, Group *groups) {
    int showQueue = msgget(1027, 0666 | IPC_CREAT);
    ShowResponse showResponse = {clientId, 0, ""};
    int responseSize = sizeof(showResponse) - sizeof(long);
    int counter = 0;
    int idx[512];
    for (int i = 0; i < 512; i++) {
        if (strcmp(groups[i].name, "") != 0) {
            idx[counter] = i;
            counter++;
        }
    }
    showResponse.listLength = counter;
    for (int i = 0; i < counter; i++) {
        if (strcmp(groups[i].name, "") != 0) {
            strcpy(showResponse.list, groups[idx[i]].name);
            msgsnd(showQueue, &showResponse, responseSize, 0);
        }
    }
    char textPrint[100];
    sprintf(textPrint, "User with ID %ld got list of Available Groups\n", clientId);
    write(1, textPrint, strlen(textPrint) + 1);
}

char *findUserById(long id, User *users) {
    for (int i = 0; i < 512; i++) {
        if (users[i].id == id)
            return users[i].login;
    }
    return "";
}

void joinGroup(long clientId, char *extra, Group *groups, User *users) {
    int success = 0;
    for (int i = 0; i < 512; i++) {
        if (strcmp(extra, groups[i].name) == 0) {
            groups[i].members[groups[i].membersNumber++] = clientId;
            success = 1;
            break;
        }
    }
    int messageQueue = msgget(1025, 0666 | IPC_CREAT);
    Message message = {clientId, 's', "", "", 1, "server"};
    int messageSize = sizeof(message) - sizeof(long);
    char textPrint[100];
    if (success) {
        sprintf(message.text, "You have joined to Group: %s\n", extra);
        sprintf(textPrint, "User %s have joined to Group: %s\n", findUserById(clientId, users), extra);
    } else {
        sprintf(message.text, "You cannot join to Group: %s\n", extra);
        sprintf(textPrint, "User %s cannot joined to Group: %s\n", findUserById(clientId, users), extra);
    }
    write(1, textPrint, strlen(textPrint) + 1);
    msgsnd(messageQueue, &message, messageSize, 0);
}

void leaveGroup(long clientId, char *extra, Group *groups, User *users) {
    int idx = -1;
    int success = 0;
    for (int i = 0; i < 512; i++) {
        if (strcmp(extra, groups[i].name) == 0) {
            idx = i;
            break;
        }
    }
    for (int i = 0; i < groups[idx].membersNumber; i++) {
        if (success) {
            if (i < 511)
                groups[idx].members[i] = groups[idx].members[i + 1];
            else
                groups[idx].members[i] = -1;
        } else {
            if (groups[idx].members[i] == clientId) {
                success = 1;
                if (i < 511)
                    groups[idx].members[i] = groups[idx].members[i + 1];
                else
                    groups[idx].members[i]= -1;
            }
        }
    }
    int messageQueue = msgget(1025, 0666 | IPC_CREAT);
    Message message = {clientId, 's', "", "", 1, "server"};
    int messageSize = sizeof(message) - sizeof(long);
    char textPrint[100];
    if (idx >= 0 && success) {
        sprintf(message.text, "You have left  Group %s\n", extra);
        sprintf(textPrint, "User %s have left to Group: %s\n", findUserById(clientId, users), extra);
    } else if (idx >= 0) {
        sprintf(message.text, "You cannot left to Group %s, because you are not member of that group\n", extra);
        sprintf(textPrint, "User %s cannot left to Group: %s, because you are not member of that group\n",
                findUserById(clientId, users), extra);
    } else {
        sprintf(message.text, "You cannot left to Group %s, because that group does not exist\n", extra);
        sprintf(textPrint, "User %s cannot left to Group: %s, because that group does not exist\n",
                findUserById(clientId, users), extra);
    }
    write(1, textPrint, strlen(textPrint) + 1);
    msgsnd(messageQueue, &message, messageSize, 0);
}


void getMessage(Group *groups, User *users) {
    int messageQueue = msgget(1025, 0666 | IPC_CREAT);
    Message message;
    int messageSize = sizeof(message) - sizeof(long);
    msgrcv(messageQueue, &message, messageSize, 1, 0);
    char textPrint[100];
    sprintf(textPrint, "Server get message from User with ID %ld \n", message.idSender);
    write(1, textPrint, strlen(textPrint) + 1);
    strcpy(message.nameSender, findUserById(message.idSender, users));
    if (message.typeOfMessage == 'u')
        sendUserMessage(message, users);
    else
        sendGroupMessage(message, groups, users);
}

void sendMessage(Message message, User *users) {
    int messageQueue = msgget(1025, 0666 | IPC_CREAT);
    int messageSize = sizeof(message) - sizeof(long);
    msgsnd(messageQueue, &message, messageSize, 0);
    char textPrint[256];
    if (message.idRecipient == message.idSender)
        sprintf(textPrint, "Server did not send message from User %s to %s, because recipient does not exist\n",
                message.nameSender, message.nameRecipient);
    else if (message.typeOfMessage == 'u')
        sprintf(textPrint, "Server send message from User %s to User %s \n", message.nameSender,
                message.nameRecipient);
    else if (message.typeOfMessage == 'g')
        sprintf(textPrint, "Server send message from User %s User to User %s in Group %s \n", message.nameSender,
                findUserById(message.idRecipient, users), message.nameRecipient);
    write(1, textPrint, strlen(textPrint) + 1);
}

void sendUserMessage(Message message, User *users) {
    char textPrint[128];
    message.idRecipient = message.idSender;
    for (int i = 0; i < 512; i++) {
        if (strcmp(message.nameRecipient, users[i].login) == 0) {
            message.idRecipient = users[i].id;
            break;
        }
    }
    if (message.idSender == message.idRecipient) {
        sprintf(textPrint, "Server did not find user with name: %s \n", message.nameRecipient);
        strcpy(message.text, textPrint);
    }
    sendMessage(message, users);
}

void sendGroupMessage(Message message, Group *groups, User *users) {
    message.idRecipient = message.idSender;
    for (int i = 0; i < 512; i++) {
        if (strcmp(message.nameRecipient, groups[i].name) == 0) {
            for (int j = 0; j < groups[i].membersNumber; j++) {
                if (groups[i].members[j] != message.idSender) {
                    message.idRecipient = groups[i].members[j];
                    sendMessage(message, users);
                }
            }
            break;
        }
    }
    if (message.idSender == message.idRecipient) {
        char textPrint[128];
        sprintf(textPrint, "Server did not find group with name: %s \n", message.nameRecipient);
        strcpy(message.text, textPrint);
        sendMessage(message, users);
    }
}
void checkBlock() {

}

void mySignal() {
    int queue = msgget(1024, 0666 | IPC_CREAT);
    msgctl(queue, IPC_RMID, 0);
    queue = msgget(1025, 0666 | IPC_CREAT);
    msgctl(queue, IPC_RMID, 0);
    queue = msgget(1026, 0666 | IPC_CREAT);
    msgctl(queue, IPC_RMID, 0);
    queue = msgget(1027, 0666 | IPC_CREAT);
    msgctl(queue, IPC_RMID, 0);
    exit(0);
}
