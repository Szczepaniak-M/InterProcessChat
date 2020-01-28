#ifndef STRUCT_H
#define STRUCT_H

typedef struct User{
    int id;
    char login[33];
    char password[33];
} User;

typedef struct Group{
    int id;
    char name[33];
    int membersNumber;
    long members[512];
} Group;

typedef struct ServerRequest{
    long idClient;
    int typeOfRequest;
    char extra[65];
} ServerRequest;

typedef struct LogInRequest{
    long recipient;
    char login[33];
    char password[33];
    long sender;
} LogInRequest;

typedef struct LogInResponse{
    long recipient;
    long idClient;
} LogInResponse;

typedef struct ShowResponse{
    long recipient;
    long listLength;
    char list[33];
} ShowResponse;

typedef struct Message{
    long idRecipient;
    char typeOfMessage;
    char nameRecipient[33];
    char text[2049];
    long idSender;
    char nameSender[33];
} Message;

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
