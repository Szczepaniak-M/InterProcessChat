#ifndef STRUCT_H
#define STRUCT_H

typedef struct User{
    int id;
    char login[32];
    char password[32];
    int loginAttempts;
    int blockedUsers[512];
    int blockedUsersNumber;
    int blockedGroups[512];
    int blockedGroupsNumber;
} User;

typedef struct OnlineUser{
    int id;
    int queueId;
} OnlineUser;

typedef struct Group{
    int id;
    char name[32];
    int membersNumber;
    int members[512];
} Group;

typedef struct ServerRequest{
    long typeOfRequest;
    char extra[64];
} ServerRequest;

typedef struct LogInRequest{
    long recipient;
    char login[32];
    char password[32];
    long sender;
} LogInRequest;

typedef struct LogInResponse{
    long typeOfRequest;
    int clientId;
    int queueId;
} LogInResponse;

typedef struct ShowResponse{
    long typeOfRequest;
    int listLength;
    char list[32];
} ShowResponse;

typedef struct Message{
    long typeOfRequest;
    int recipientId;
    int queueId;
    int typeOfMessage;
    char nameRecipient[32];
    char text[2048];
    int senderId;
    char nameSender[32];
} Message;

void readFromBuffer(char *buffer, int size, char *destination) {
    strncpy(destination, buffer, size);
    destination[size - 1] = '\0';
}

int readString(int counter, char *line, char *destination) {
    int counterStart = counter;
    while (line[counter] != ' ' && line[counter] != '\n' && line[counter] != '\0') {
        counter++;
    }
    strncpy(destination, line + counterStart, counter - counterStart);
    destination[counter - counterStart] = '\0';
    return ++counter;
}

int readInt(int *counter, const char *line) {
    int id = line[(*counter)++] - 48;
    while (line[*counter] >= '0' && line[*counter] <= '9') {
        id = id * 10 + line[(*counter)++] - 48;
    }

    if(line[*counter] == ' ')
        (*counter)++;

    return id;
}



#endif
