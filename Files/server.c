#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include "structures.h"

int memoryId[4];
int semaphores;
int loginQueue;

void readData(User *users, Group *groups);

void readUser(char *line, User *users, int *usersNumber);

void readGroup(char *line, Group *groups, int *groupsNumber);

OnlineUser logIn(int loginQueue, User *users);

void newClient(OnlineUser newUser, int semaphore, OnlineUser *onlineUsers, int *onlineUsersNumber);

void clientProcess(int queueId, int clientId, int semaphore, User *users, OnlineUser *onlineUsers,
                   int *onlineUsersNumber, Group *groups);

void getMessage(int queueId, int semaphore, User *users, OnlineUser *onlineUsers,
                int *onlineUsersNumber, Group *groups);

void sendMessage(Message message);

void sendUserMessage(Message message, int semaphore, User *users, OnlineUser *onlineUsers,
                     int *onlineUsersNumber, Group *groups);

void sendGroupMessage(Message message, int semaphore, User *users, OnlineUser *onlineUsers,
                      int *onlineUsersNumber, Group *groups);

void checkAndSend(Message *message, int semaphore, User *users, OnlineUser *onlineUsers,
                  const int *onlineUsersNumber, Group *groups);

void showOnlineUser(int queueId, int clientId, int semaphore, User *users,
                    OnlineUser *onlineUsers, const int *onlineUsersNumber);

void showMembers(int queueId, int clientId, int semaphore, char *extra, User *users, Group *groups);

void showAvailableGroups(int queueId, int clientId, Group *groups);

void joinGroup(int queueId, int clientId, int semaphore, char *extra, User *users, Group *groups);

void leaveGroup(int queueId, int clientId, int semaphore, char *extra, User *users, Group *groups);

void blockUser(int queueId, int clientId, int semaphore, char *extra, User *users);

void blockGroup(int queueId, int clientId, int semaphore, char *extra, User *users, Group *groups);

void unblockUser(int queueId, int clientId, int semaphore, char *extra, User *users);

void unblockGroup(int queueId, int clientId, int semaphore, char *extra, User *users, Group *groups);

void logOut(int queueId, int clientId, int semaphore, OnlineUser *onlineUsers, int *onlineUsersNumber);

void mySignal();

char *findUserLoginById(int id, User *users);

int findUserIndexById(int id, User *users);

int findUserIndexByName(char *name, User *users);

int findGroupIndexByName(char *name, Group *groups);

int main() {
    signal(SIGINT, mySignal);
    loginQueue = -1;
    while (loginQueue == -1) {
        char textBuffer[32];
        sprintf(textBuffer, "Type Login Queue key: ");
        write(1, textBuffer, strlen(textBuffer) + 1);
        read(0, textBuffer, 32);
        int counter = 0;
        loginQueue = readInt(&counter, textBuffer);
        loginQueue = msgget(loginQueue, 0600 | IPC_CREAT | IPC_EXCL);
    }
    memoryId[0] = shmget(IPC_PRIVATE, sizeof(User) * 512, 0600);
    memoryId[1] = shmget(IPC_PRIVATE, sizeof(OnlineUser) * 512, 0600);
    memoryId[2] = shmget(IPC_PRIVATE, sizeof(int), 0600);
    memoryId[3] = shmget(IPC_PRIVATE, sizeof(Group) * 512, 0600);
    User *users = shmat(memoryId[0], 0, 0);
    OnlineUser *onlineUsers = shmat(memoryId[1], 0, 0);
    int *onlineUsersNumber = shmat(memoryId[2], 0, 0);
    Group *groups = shmat(memoryId[3], 0, 0);

    semaphores = semget(IPC_PRIVATE, 4, 0600);
    for (int i = 0; i < 4; i++) {
        struct sembuf sem = {i, 1, 0};
        semop(semaphores, &sem, 1);
    }
    readData(users, groups);

    OnlineUser newUser;
    while (1) {
        newUser = logIn(loginQueue, users);
        if (newUser.id != -1) {
            int processId = fork();
            if (processId) {
                newClient(newUser, semaphores, onlineUsers, onlineUsersNumber);
            } else {
                signal(SIGINT, SIG_DFL);
                clientProcess(newUser.queueId, newUser.id, semaphores, users, onlineUsers, onlineUsersNumber, groups);
            }
        }
    }
}

void readData(User *users, Group *groups) {
    char ch;
    char *line;
    int usersNumber = 0;
    int groupsNumber = 0;

    int file_id = open("Files/config.txt", O_RDONLY);
    while (read(file_id, &ch, 1)) {
        int counter = 2;
        while (read(file_id, &ch, 1) && ch != '\n') {
            counter++;
        }
        lseek(file_id, -counter, SEEK_CUR);
        line = malloc(sizeof(char) * counter);

        read(file_id, line, counter);
        if (line[0] == 'u')
            readUser(line, users, &usersNumber);
        else
            readGroup(line, groups, &groupsNumber);
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

OnlineUser logIn(int loginQueue, User *users) {
    LogInRequest loginRequest;
    int requestSize = sizeof(loginRequest) - sizeof(long);
    msgrcv(loginQueue, &loginRequest, requestSize, 1, 0);
    LogInResponse logInResponse = {loginRequest.sender, -4, -1};
    int responseSize = sizeof(logInResponse) - sizeof(long);
    int success = 0;
    OnlineUser newUser = {-1, -1};

    for (int i = 0; i < 512; i++) {
        if (strcmp(loginRequest.login, users[i].login) == 0) {
            struct sembuf semP = {0, -1, 0};
            semop(semaphores, &semP, 1);

            if (users[i].loginAttempts == -3) {
                logInResponse.clientId = users[i].loginAttempts;
            } else if (strcmp(loginRequest.password, users[i].password) == 0) {
                logInResponse.clientId = users[i].id;
                logInResponse.queueId = msgget(IPC_PRIVATE, 0600);
                newUser.id = logInResponse.clientId;
                newUser.queueId = logInResponse.queueId;
                users[i].loginAttempts = 0;
                success = 1;
            } else {
                users[i].loginAttempts--;
                logInResponse.clientId = users[i].loginAttempts;
            }

            struct sembuf semV = {0, 1, 0};
            semop(semaphores, &semV, 1);
            break;
        }
    }
    msgsnd(loginQueue, &logInResponse, responseSize, 0);
    char textPrint[100];
    if (success) {
        sprintf(textPrint, "User with ID %d successfully logged in\n", logInResponse.clientId);
        write(1, textPrint, strlen(textPrint) + 1);
    } else if (logInResponse.clientId == -3) {
        strcpy(textPrint, "Unsuccessfully logging. User blocked\n");
        write(1, textPrint, strlen(textPrint) + 1);
    } else {
        strcpy(textPrint, "Unsuccessfully logging\n");
        write(1, textPrint, strlen(textPrint) + 1);
    }
    return newUser;
}

void newClient(OnlineUser newUser, int semaphore, OnlineUser *onlineUsers, int *onlineUsersNumber) {
    struct sembuf semP[2] = {{1, -1, 0},
                             {2, -1, 0}};
    semop(semaphore, semP, 2);

    onlineUsers[*onlineUsersNumber] = newUser;
    (*onlineUsersNumber)++;

    struct sembuf semV[2] = {{1, 1, 0},
                             {2, 1, 0}};
    semop(semaphore, semV, 2);
}

void
clientProcess(int queueId, int clientId, int semaphore, User *users, OnlineUser *onlineUsers,
              int *onlineUsersNumber, Group *groups) {
    ServerRequest serverRequest;
    int requestSize = sizeof(serverRequest) - sizeof(long);
    while (1) {
        msgrcv(queueId, &serverRequest, requestSize, -11, 0);
        switch (serverRequest.typeOfRequest) {
            case 1:
                getMessage(queueId, semaphore, users, onlineUsers, onlineUsersNumber, groups);
                break;
            case 2:
                showOnlineUser(queueId, clientId, semaphore, users, onlineUsers, onlineUsersNumber);
                break;
            case 3:
                showAvailableGroups(queueId, clientId, groups);
                break;
            case 4:
                showMembers(queueId, clientId, semaphore, serverRequest.extra, users, groups);
                break;
            case 5:
                joinGroup(queueId, clientId, semaphore, serverRequest.extra, users, groups);
                break;
            case 6:
                leaveGroup(queueId, clientId, semaphore, serverRequest.extra, users, groups);
                break;
            case 7:
                blockUser(queueId, clientId, semaphore, serverRequest.extra, users);
                break;
            case 8:
                blockGroup(queueId, clientId, semaphore, serverRequest.extra, users, groups);
                break;
            case 9:
                unblockUser(queueId, clientId, semaphore, serverRequest.extra, users);
                break;
            case 10:
                unblockGroup(queueId, clientId, semaphore, serverRequest.extra, users, groups);
                break;
            case 11:
                logOut(queueId, clientId, semaphore, onlineUsers, onlineUsersNumber);
                break;
        }
    }
}

void getMessage(int queueId, int semaphore, User *users, OnlineUser *onlineUsers,
                int *onlineUsersNumber, Group *groups) {
    Message message;
    int messageSize = sizeof(message) - sizeof(long);
    msgrcv(queueId, &message, messageSize, 12, 0);
    char textPrint[100];
    sprintf(textPrint, "Server got message from User with ID %d \n", message.senderId);
    write(1, textPrint, strlen(textPrint) + 1);
    strcpy(message.nameSender, findUserLoginById(message.senderId, users));
    if (message.typeOfMessage == 0)
        sendUserMessage(message, semaphore, users, onlineUsers, onlineUsersNumber, groups);
    else
        sendGroupMessage(message, semaphore, users, onlineUsers, onlineUsersNumber, groups);
}

void sendUserMessage(Message message, int semaphore, User *users, OnlineUser *onlineUsers,
                     int *onlineUsersNumber, Group *groups) {
    for (int i = 0; i < 512; i++) {
        if (strcmp(message.nameRecipient, users[i].login) == 0) {
            message.recipientId = users[i].id;
            checkAndSend(&message, semaphore, users, onlineUsers, onlineUsersNumber, groups);
            return;
        }
    }
    message.typeOfMessage = 2;
    char textPrint[256];
    sprintf(textPrint, "Server didn't send message to User %s, because recipient does not exist\n",
            message.nameRecipient);
    strcpy(message.text, textPrint);
    sendMessage(message);
    sprintf(textPrint, "Server didn't send message from User %s to User %s, because recipient does not exist\n",
            message.nameSender, message.nameRecipient);
    write(1, textPrint, strlen(textPrint) + 1);
}

void sendGroupMessage(Message message, int semaphore, User *users, OnlineUser *onlineUsers,
                      int *onlineUsersNumber, Group *groups) {
    message.recipientId = message.senderId;
    char copyText[2048];
    for (int i = 0; i < 512; i++) {
        if (strcmp(message.nameRecipient, groups[i].name) == 0) {
            struct sembuf semP = {3, -1, 0};
            semop(semaphore, &semP, 1);
            strcpy(copyText, message.text);
            for (int j = 0; j < groups[i].membersNumber; j++) {
                strcpy(message.text, copyText);
                if (groups[i].members[j] != message.senderId) {
                    message.recipientId = groups[i].members[j];
                    checkAndSend(&message, semaphore, users, onlineUsers, onlineUsersNumber, groups);
                }
            }
            struct sembuf semV = {3, 1, 0};
            semop(semaphore, &semV, 1);
            return;
        }
    }
    char textPrint[256];
    message.typeOfMessage = 2;
    sprintf(textPrint, "Server didn't send message to Group %s, because recipient doesn't exist \n",
            message.nameRecipient);
    strcpy(message.text, textPrint);
    sendMessage(message);
    sprintf(textPrint, "Server didn't send message from User %s to Group %s, because recipient doesn't exist\n",
            message.nameSender, message.nameRecipient);
    write(1, textPrint, strlen(textPrint) + 1);
}

void checkAndSend(Message *message, int semaphore, User *users, OnlineUser *onlineUsers,
                  const int *onlineUsersNumber, Group *groups) {
    char textPrint[256];
    int senderQueue = message->queueId;
    int recipientType = message->typeOfMessage;
    int check = 0;
    if (message->typeOfMessage == 1)
        check++;
    struct sembuf semP[2] = {{1, -1, 0},
                             {2, -1, 0}};
    semop(semaphore, semP, 2);

    for (int i = 0; i < *onlineUsersNumber; i++) {
        if (message->recipientId == onlineUsers[i].id) {
            message->queueId = onlineUsers[i].queueId;
            check += 2;
            break;
        }
    }

    if (check > 1) {
        struct sembuf semPU = {0, -1, 0};
        semop(semaphore, &semPU, 1);
        int recipientIndex = findUserIndexById(message->recipientId, users);
        if (message->typeOfMessage == 0) {
            for (int i = 0; i < users[recipientIndex].blockedUsersNumber; i++) {
                if (users[recipientIndex].blockedUsers[i] == message->senderId) {
                    check -= 4;
                    break;
                }
            }
        } else if (message->typeOfMessage == 1) {
            int indexGroup = findGroupIndexByName(message->nameRecipient, groups);
            int groupId = groups[indexGroup].id;
            for (int i = 0; i < users[recipientIndex].blockedGroupsNumber; i++) {
                if (users[recipientIndex].blockedGroups[i] == groupId) {
                    check -= 4;
                    break;
                }
            }
        }
        check += 4;
        struct sembuf semVU = {0, 1, 0};
        semop(semaphore, &semVU, 1);
    }

    if (check == 0) {
        message->typeOfMessage = 2;
        sprintf(textPrint, "Server didn't send message to User %s, because the User is offline",
                message->nameRecipient);
        strcpy(message->text, textPrint);
        sprintf(textPrint, "Server didn't send message from User %s to User %s, because the User is offline\n",
                message->nameSender, message->nameRecipient);
        write(1, textPrint, strlen(textPrint) + 1);
    } else if (check == 1) {
        message->typeOfMessage = 2;
        sprintf(textPrint, "Server didn't send message to User %s in Group %s, because the User is offline",
                findUserLoginById(message->recipientId, users), message->nameRecipient);
        strcpy(message->text, textPrint);
        sprintf(textPrint,
                "Server didn't send message from User %s to User %s in Group %s, because the User is offline\n",
                message->nameSender, findUserLoginById(message->recipientId, users), message->nameRecipient);
        write(1, textPrint, strlen(textPrint) + 1);
    } else if (check == 2) {
        message->typeOfMessage = 2;
        message->queueId = senderQueue;
        sprintf(textPrint, "Server didn't send message to User %s, because the User blocked you",
                message->nameRecipient);
        strcpy(message->text, textPrint);
        sprintf(textPrint,
                "Server didn't send message from User %s to User %s, because the Sender is blocked by Recipient\n",
                message->nameSender, message->nameRecipient);
        write(1, textPrint, strlen(textPrint) + 1);
    } else if (check == 3) {
        message->typeOfMessage = 2;
        message->queueId = senderQueue;
        sprintf(textPrint,
                "Server didn't send message to User %s in Group %s, because the Group is blocked by Recipient",
                findUserLoginById(message->recipientId, users), message->nameRecipient);
        strcpy(message->text, textPrint);
        sprintf(textPrint,
                "Server didn't send message from User %s to User %s in Group %s, because the Group is blocked by Recipient\n",
                message->nameSender, findUserLoginById(message->recipientId, users), message->nameRecipient);
        write(1, textPrint, strlen(textPrint) + 1);
    } else if (check == 6) {
        sprintf(textPrint,
                "Server send message from User %s to User %s\n",
                message->nameSender, message->nameRecipient);
        write(1, textPrint, strlen(textPrint) + 1);
    } else if (check == 7) {
        sprintf(textPrint,
                "Server send message from User %s to User %s in Group %s\n",
                message->nameSender, findUserLoginById(message->recipientId, users), message->nameRecipient);
        write(1, textPrint, strlen(textPrint) + 1);
    }
    sendMessage(*message);
    message->queueId = senderQueue;
    message->typeOfMessage = recipientType;
    struct sembuf semV[2] = {{1, 1, 0},
                             {2, 1, 0}};
    semop(semaphore, semV, 2);
}

void sendMessage(Message message) {
    int messageSize = sizeof(message) - sizeof(long);
    message.typeOfRequest++;
    msgsnd(message.queueId, &message, messageSize, 0);
}

void showOnlineUser(int queueId, int clientId, int semaphore, User *users,
                    OnlineUser *onlineUsers, const int *onlineUsersNumber) {
    ShowResponse showResponse = {14, *onlineUsersNumber, ""};
    int responseSize = sizeof(showResponse) - sizeof(long);

    struct sembuf semP[2] = {{1, -1, 0},
                             {2, -1, 0}};
    semop(semaphore, semP, 2);

    for (int i = 0; i < *onlineUsersNumber; i++) {
        char *name = findUserLoginById(onlineUsers[i].id, users);
        strcpy(showResponse.list, name);
        showResponse.listLength = *onlineUsersNumber;
        msgsnd(queueId, &showResponse, responseSize, 0);
    }

    struct sembuf semV[2] = {{1, 1, 0},
                             {2, 1, 0}};
    semop(semaphore, semV, 2);

    char textPrint[100];
    sprintf(textPrint, "User with ID %d got list of Online Users\n", clientId);
    write(1, textPrint, strlen(textPrint) + 1);
}

void showMembers(int queueId, int clientId, int semaphore, char *extra, User *users, Group *groups) {
    ShowResponse showResponse = {14, -1, ""};
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
        struct sembuf semP = {3, -1, 0};
        semop(semaphore, &semP, 1);

        showResponse.listLength = groups[idx].membersNumber;
        for (int i = 0; i < groups[idx].membersNumber; i++) {
            char *name = findUserLoginById(groups[idx].members[i], users);
            strcpy(showResponse.list, name);
            msgsnd(queueId, &showResponse, responseSize, 0);
        }

        struct sembuf semV = {3, 1, 0};
        semop(semaphore, &semV, 1);

        sprintf(textPrint, "User with ID %d successfully got the list of members of group %s\n", clientId, extra);
        write(1, textPrint, strlen(textPrint) + 1);
    } else {
        sprintf(textPrint, "User with ID %d did not get the list of members of group %s\n", clientId, extra);
        write(1, textPrint, strlen(textPrint) + 1);
        msgsnd(queueId, &showResponse, responseSize, 0);
    }
}

void showAvailableGroups(int queueId, int clientId, Group *groups) {
    ShowResponse showResponse = {14, 0, ""};
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
            msgsnd(queueId, &showResponse, responseSize, 0);
        }
    }
    char textPrint[100];
    sprintf(textPrint, "User with ID %d got list of Available Groups\n", clientId);
    write(1, textPrint, strlen(textPrint) + 1);
}

void joinGroup(int queueId, int clientId, int semaphore, char *extra, User *users, Group *groups) {
    int index = findGroupIndexByName(extra, groups);
    if (index != -1) {
        struct sembuf semP = {3, -1, 0};
        semop(semaphore, &semP, 1);

        for (int j = 0; j < groups[index].membersNumber; j++) {
            if (groups[index].members[j] == clientId) {
                index = -2;
                break;
            }
        }
        if (index != -2)
            groups[index].members[groups[index].membersNumber++] = clientId;

        struct sembuf semV = {3, 1, 0};
        semop(semaphore, &semV, 1);
    }

    Message message = {13, clientId, queueId, 2, "", "", 1, "server"};
    int messageSize = sizeof(message) - sizeof(long);
    char textPrint[100];
    if (index == -1) {
        sprintf(message.text, "You cannot join to Group %s, because it doesn't exist\n", extra);
        sprintf(textPrint, "User %s cannot join to Group %s, because it doesn't exist\n",
                findUserLoginById(clientId, users), extra);
    } else if (index == -2) {
        sprintf(message.text, "You cannot join to Group %s, because you are already a member of the group\n", extra);
        sprintf(textPrint, "User %s cannot join to Group %s, because he/she is already a member of the group\n",
                findUserLoginById(clientId, users), extra);
    } else {
        sprintf(message.text, "You have joined to Group %s\n", extra);
        sprintf(textPrint, "User %s have joined to Group %s\n", findUserLoginById(clientId, users), extra);
    }
    write(1, textPrint, strlen(textPrint) + 1);
    msgsnd(queueId, &message, messageSize, 0);
}

void leaveGroup(int queueId, int clientId, int semaphore, char *extra, User *users, Group *groups) {
    int index = findGroupIndexByName(extra, groups);
    int success = 0;
    if (index != -1) {
        struct sembuf semP = {3, -1, 0};
        semop(semaphore, &semP, 1);

        for (int i = 0; i < groups[index].membersNumber; i++) {
            if (success) {
                if (i < 511)
                    groups[index].members[i] = groups[index].members[i + 1];
                else
                    groups[index].members[i] = -1;
            } else {
                if (groups[index].members[i] == clientId) {
                    success = 1;
                    if (i < 511)
                        groups[index].members[i] = groups[index].members[i + 1];
                    else
                        groups[index].members[i] = -1;
                }
            }
        }
        if (success)
            groups[index].membersNumber--;
        struct sembuf semV = {3, 1, 0};
        semop(semaphore, &semV, 1);
    }
    Message message = {13, clientId, queueId, 2, "", "", 1, "server"};
    int messageSize = sizeof(message) - sizeof(long);
    char textPrint[128];
    if (index != -1 && success) {
        sprintf(message.text, "You have left Group %s\n", extra);
        sprintf(textPrint, "User %s have left Group %s\n", findUserLoginById(clientId, users), extra);
    } else if (index != -1) {
        sprintf(message.text, "You cannot left Group %s, because you aren't a member of the group\n", extra);
        sprintf(textPrint, "User %s cannot left Group %s, because he/she isn't a member of the group\n",
                findUserLoginById(clientId, users), extra);
    } else {
        sprintf(message.text, "You cannot left Group %s, because the group does not exist\n", extra);
        sprintf(textPrint, "User %s cannot left Group %s, because the group doesn't exist\n",
                findUserLoginById(clientId, users), extra);
    }
    write(1, textPrint, strlen(textPrint) + 1);
    msgsnd(queueId, &message, messageSize, 0);
}

void blockUser(int queueId, int clientId, int semaphore, char *extra, User *users) {
    int clientIndex = findUserIndexById(clientId, users);
    int blockIndex = findUserIndexByName(extra, users);
    if (blockIndex != -1) {
        struct sembuf semP = {0, -1, 0};
        semop(semaphore, &semP, 1);

        for (int j = 0; j < users[clientIndex].blockedUsersNumber; j++) {
            if (users[clientIndex].blockedUsers[j] == users[blockIndex].id) {
                blockIndex = -2;
                break;
            }
        }
        if (users[blockIndex].id == clientId)
            blockIndex = -3;

        if (blockIndex >= 0)
            users[clientIndex].blockedUsers[users[clientIndex].blockedUsersNumber++] = users[blockIndex].id;

        struct sembuf semV = {0, 1, 0};
        semop(semaphore, &semV, 1);
    }

    Message message = {13, clientId, queueId, 2, "", "", 1, "server"};
    int messageSize = sizeof(message) - sizeof(long);
    char textPrint[128];
    if (blockIndex == -1) {
        sprintf(message.text, "You cannot block User %s, because he/she doesn't exist\n", extra);
        sprintf(textPrint, "User %s cannot block User %s, because he/she doesn't exist\n",
                findUserLoginById(clientId, users), extra);
    } else if (blockIndex == -2) {
        sprintf(message.text, "You cannot block User %s, because he/she is already blocked\n", extra);
        sprintf(textPrint, "User %s cannot block User %s, because he/she is already blocked\n",
                findUserLoginById(clientId, users), extra);
    } else if (blockIndex == -3) {
        sprintf(message.text, "You cannot block yourself\n");
        sprintf(textPrint, "User %s cannot block himself/herself\n", extra);
    } else {
        sprintf(message.text, "You have blocked User %s\n", extra);
        sprintf(textPrint, "User %s have blocked User %s\n", findUserLoginById(clientId, users), extra);
    }
    write(1, textPrint, strlen(textPrint) + 1);
    msgsnd(queueId, &message, messageSize, 0);
}

void blockGroup(int queueId, int clientId, int semaphore, char *extra, User *users, Group *groups) {
    int clientIndex = findUserIndexById(clientId, users);
    int blockIndex = findGroupIndexByName(extra, groups);
    if (blockIndex != -1) {
        struct sembuf semP = {0, -1, 0};
        semop(semaphore, &semP, 1);

        for (int j = 0; j < users[clientIndex].blockedGroupsNumber; j++) {
            if (users[clientIndex].blockedGroups[j] == users[blockIndex].id) {
                blockIndex = -2;
                break;
            }
        }
        if (blockIndex != -2)
            users[clientIndex].blockedGroups[users[clientIndex].blockedGroupsNumber++] = groups[blockIndex].id;

        struct sembuf semV = {0, 1, 0};
        semop(semaphore, &semV, 1);
    }
    Message message = {13, clientId, queueId, 2, "", "", 1, "server"};
    int messageSize = sizeof(message) - sizeof(long);
    char textPrint[128];
    if (blockIndex == -1) {
        sprintf(message.text, "You cannot block Group %s, because it doesn't exist\n", extra);
        sprintf(textPrint, "User %s cannot block Group %s, because it doesn't exist\n",
                findUserLoginById(clientId, users), extra);
    } else if (blockIndex == -2) {
        sprintf(message.text, "You cannot block Group %s, because it is already blocked\n", extra);
        sprintf(textPrint, "User %s cannot block Group %s, because it is already blocked\n",
                findUserLoginById(clientId, users), extra);
    } else {
        sprintf(message.text, "You have blocked Group %s\n", extra);
        sprintf(textPrint, "User %s have blocked Group %s\n", findUserLoginById(clientId, users), extra);
    }
    write(1, textPrint, strlen(textPrint) + 1);
    msgsnd(queueId, &message, messageSize, 0);
}

void unblockUser(int queueId, int clientId, int semaphore, char *extra, User *users) {
    int clientIndex = findUserIndexById(clientId, users);
    int blockIndex = findUserIndexByName(extra, users);
    int success = 0;
    if (blockIndex != -1) {
        struct sembuf semP = {0, -1, 0};
        semop(semaphore, &semP, 1);
        for (int i = 0; i < users[clientIndex].blockedUsersNumber; i++) {
            if (success) {
                if (i < 511)
                    users[clientIndex].blockedUsers[i] = users[clientIndex].blockedUsers[i + 1];
                else
                    users[clientIndex].blockedUsers[i] = -1;
            } else {
                if (users[clientIndex].blockedUsers[i] == users[blockIndex].id) {
                    success = 1;
                    if (i < 511)
                        users[clientIndex].blockedUsers[i] = users[clientIndex].blockedUsers[i + 1];
                    else
                        users[clientIndex].blockedUsers[i] = -1;
                }
            }
        }
        if (success)
            users[clientIndex].blockedUsersNumber--;
        struct sembuf semV = {0, 1, 0};
        semop(semaphore, &semV, 1);
    }
    Message message = {13, clientId, queueId, 2, "", "", 1, "server"};
    int messageSize = sizeof(message) - sizeof(long);
    char textPrint[100];
    if (blockIndex >= 0 && success) {
        sprintf(message.text, "You have unblocked User %s\n", extra);
        sprintf(textPrint, "User %s have unblocked User %s\n", findUserLoginById(clientId, users), extra);
    } else if (blockIndex >= 0) {
        sprintf(message.text, "You cannot unblock User %s, because you didn't block the user\n", extra);
        sprintf(textPrint, "User %s cannot unblock User: %s, because he/she didn't block the user\n",
                findUserLoginById(clientId, users), extra);
    } else {
        sprintf(message.text, "You cannot unblock User %s, because the user doesn't exist\n", extra);
        sprintf(textPrint, "User %s cannot unblock User: %s, because the user doesn't exist\n",
                findUserLoginById(clientId, users), extra);
    }
    write(1, textPrint, strlen(textPrint) + 1);
    msgsnd(queueId, &message, messageSize, 0);
}

void unblockGroup(int queueId, int clientId, int semaphore, char *extra, User *users, Group *groups) {
    int clientIndex = findUserIndexById(clientId, users);
    int blockIndex = findGroupIndexByName(extra, groups);
    int success = 0;
    if (blockIndex != -1) {
        struct sembuf semP = {0, -1, 0};
        semop(semaphore, &semP, 1);
        for (int i = 0; i < users[clientIndex].blockedGroupsNumber; i++) {
            if (success) {
                if (i < 511)
                    users[clientIndex].blockedGroups[i] = users[clientIndex].blockedGroups[i + 1];
                else
                    users[clientIndex].blockedGroups[i] = -1;
            } else {
                if (users[clientIndex].blockedGroups[i] == groups[blockIndex].id) {
                    success = 1;
                    if (i < 511)
                        users[clientIndex].blockedGroups[i] = users[clientIndex].blockedGroups[i + 1];
                    else
                        users[clientIndex].blockedGroups[i] = -1;
                }
            }
        }
        if (success)
            users[clientIndex].blockedGroupsNumber--;
        struct sembuf semV = {0, 1, 0};
        semop(semaphore, &semV, 1);
    }
    Message message = {13, clientId, queueId, 2, "", "", 1, "server"};
    int messageSize = sizeof(message) - sizeof(long);
    char textPrint[100];
    if (blockIndex >= 0 && success) {
        sprintf(message.text, "You have unblocked Group %s\n", extra);
        sprintf(textPrint, "User %s have unblocked Group %s\n", findUserLoginById(clientId, users), extra);
    } else if (blockIndex >= 0) {
        sprintf(message.text, "You cannot unblock Group %s, because you didn't block the group\n", extra);
        sprintf(textPrint, "User %s cannot unblock Group %s, because he/she didn't block the group\n",
                findUserLoginById(clientId, users), extra);
    } else {
        sprintf(message.text, "You cannot unblock Group %s, because the group doesn't exist\n", extra);
        sprintf(textPrint, "User %s cannot unblock Group %s, because the group doesn't exist\n",
                findUserLoginById(clientId, users), extra);
    }
    write(1, textPrint, strlen(textPrint) + 1);
    msgsnd(queueId, &message, messageSize, 0);
}

void logOut(int queueId, int clientId, int semaphore, OnlineUser *onlineUsers, int *onlineUsersNumber) {
    struct sembuf semP[2] = {{1, -1, 0},
                             {2, -1, 0}};
    semop(semaphore, semP, 2);
    int found = 0;
    for (int i = 0; i < 512; i++) {
        if (found) {
            if (i < 511)
                onlineUsers[i] = onlineUsers[i + 1];
            else
                onlineUsers[i].id = -1;
        } else {
            if (onlineUsers[i].id == clientId) {
                found = 1;
                if (i < 511)
                    onlineUsers[i] = onlineUsers[i + 1];
                else
                    onlineUsers[i].id = -1;
            }
        }
    }
    (*onlineUsersNumber)--;
    struct sembuf semV[2] = {{1, 1, 0},
                             {2, 1, 0}};
    semop(semaphore, semV, 2);
    Message kill = {13, clientId, queueId, 3, "", "", 1, "server"};
    int killSize = sizeof(kill) - sizeof(long);
    msgsnd(queueId, &kill, killSize, 0);
    char textPrint[100];
    sprintf(textPrint, "User with ID %d successfully logged out\n", clientId);
    write(1, textPrint, strlen(textPrint) + 1);
    msgctl(queueId, IPC_RMID, 0);
    exit(0);
}

void mySignal() {
    shmctl(memoryId[3], IPC_RMID, 0);
    shmctl(memoryId[0], IPC_RMID, 0);
    struct sembuf semP[4] = {{0, -1, 0},
                             {1, -1, 0},
                             {2, -1, 0},
                             {3, -1, 0}};
    semop(semaphores, semP, 4);

    int *OnlineUsersNumber = shmat(memoryId[2], 0, 0);
    OnlineUser *OnlineUsers = shmat(memoryId[1], 0, 0);
    for (int i = 0; i < *OnlineUsersNumber; i++) {
        msgctl(OnlineUsers[i].queueId, IPC_RMID, 0);
    }
    shmctl(memoryId[2], IPC_RMID, 0);
    shmctl(memoryId[1], IPC_RMID, 0);
    semctl(semaphores, IPC_RMID, 0);
    msgctl(loginQueue, IPC_RMID, 0);
    exit(0);
}

char *findUserLoginById(int id, User *users) {
    for (int i = 0; i < 512; i++) {
        if (users[i].id == id)
            return users[i].login;
    }
    return "";
}

int findUserIndexById(int id, User *users) {
    for (int i = 0; i < 512; i++) {
        if (users[i].id == id)
            return i;
    }
    return -1;
}

int findUserIndexByName(char *name, User *users) {
    for (int i = 0; i < 512; i++) {
        if (strcmp(users[i].login, name) == 0)
            return i;
    }
    return -1;
}

int findGroupIndexByName(char *name, Group *groups) {
    for (int i = 0; i < 512; i++) {
        if (strcmp(groups[i].name, name) == 0)
            return i;
    }
    return -1;
}
