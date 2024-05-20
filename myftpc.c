#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "myftp.h"

int ac = 0;
char *av[16];

void help(int ac, char *av[], int sock);
void quit(int ac, char *av[], int sock);
void pwd(int ac, char *av[], int sock);
void cd(int ac, char *av[], int sock);
void dir(int ac, char *av[], int sock);
void lpwd(int ac, char *av[], int sock);
void lcd(int ac, char *av[], int sock);
void ldir(int ac, char *av[], int sock);
void get(int ac, char *av[], int sock);
void put(int ac, char *av[], int sock);

struct command_table
{
    char *cmd;
    void (*func)(int, char *[], int, struct sockaddr_in);
} cmd_tbl[] = {
    {"quit", quit},
    {"pwd", pwd},
    {"cd", cd},
    {"dir", dir},
    {"lpwd", lpwd},
    {"lcd", lcd},
    {"ldir", ldir},
    {"get", get},
    {"put", put},
    {"help", help},
    {NULL, NULL}};

enum State
{
    Start = 1,
    Word,
    Make,
    Exit,
};
enum State state;

enum Client_State
{
    Use = 1,
    Close,
};
enum Client_State client_state;

void check_err_msg(struct myftp_msg msg)
{
    switch (msg.type)
    {
    case MYFTP_REP_MSGTYPE_COMM_ERR:
        switch (msg.code)
        {
        case MYFTP_REP_MSGCODE_STRUCT_ERR:
            printf("Command Error: The structure is wrong.\n");
            break;

        case MYFTP_REP_MSGCODE_NOCOMM_ERR:
            printf("Command Error: The command is undefined.\n");
            break;

        case MYFTP_REP_MSGCODE_PROT_ERR:
            printf("Command Error: The protocol error.\n");
            break;

        default:
            printf("Command Error, but the code of the message violates the protocol.\n");
            break;
        }

        break;

    case MYFTP_REP_MSGTYPE_FILE_ERR:

        switch (msg.code)
        {
        case MYFTP_REP_MSGCODE_NO_FILE_ERR:
            printf("File Error: There is no such file.\n");
            break;

        case MYFTP_REP_MSGCODE_NO_RIGHT_ERR:
            printf("File Error: You have no right to access the file.\n");
            break;

        default:
            printf("File Error, but the code of the message violates the protocol.\n");
            break;
        }
        break;

    default:
        // else: unknown error
        printf("Unknown Error: The error is undefined.\n");
        break;
    }
}

void quit(int ac, char *av[], int sock)
{
    // send a quit message to server
    struct myftp_msg msg = {
        .type = MYFTP_COM_MSGTYPE_QUIT,
        .code = 0,
        .data_len = 0,
        .data = {0},
    };

    if (send(sock, &msg, sizeof(msg), 0) < 0)
    {
        perror("quit: send");
        return;
    }

    printf("Send a message: QUIT\n");
    printf("\n");
    client_state = Close;
}

void pwd(int ac, char *av[], int sock)
{
    // send a PWD message to server
    struct myftp_msg msg = {
        .type = MYFTP_COM_MSGTYPE_PWD,
        .code = 0,
        .data_len = 0,
        .data = {0},
    };

    if (send(sock, &msg, sizeof(msg), 0) < 0)
    {
        perror("pwd: send");
        return;
    }

    printf("Send a message: PWD\n");
    printf("\n");

    // receive the message
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    if (select(sock + 1, &readfds, NULL, NULL, NULL) < 0)
    {
        perror("pwd: select");
    }

    if (!FD_ISSET(sock, &readfds))
    {
        printf("readfds is not set.\n");
    }
    else if (FD_ISSET(sock, &readfds))
    {
        struct myftp_msg msg = {0};
        if ((recv(sock, &msg, sizeof(msg), 0)) > 0)
        {
            switch (msg.type)
            {
            case MYFTP_REP_MSGTYPE_COMM_OK:
                switch (msg.code)
                {
                case MYFTP_REP_MSGCODE_COMM_OK:
                    printf("Recieve a reply message: The command is ok\n");
                    char pathname[PATHNAME_SIZE] = {0};

                    if (msg.data_len <= 0)
                    {
                        printf("Error: the length of the data is under 0.\n");
                        break;
                    }

                    strncpy(pathname, msg.data, msg.data_len);
                    printf("Get path: %s\n", pathname);
                    break;

                default:
                    // receive the msg which type is different from COMM_OK
                    printf("Error: Protocol Error\n");
                    break;
                }
                break;
            default:
                check_err_msg(msg);
                break;
            }
        }
        printf("\n");
    }
}

void cd(int ac, char *av[], int sock)
{
    if (av[1] == NULL)
    {
        fprintf(stderr, "Usage: cd <pathname>\n");
        return;
    }

    // send a CWD message to server
    int len = strlen(av[1]);

    struct myftp_msg msg = {
        .type = MYFTP_COM_MSGTYPE_CWD,
        .code = 0,
        .data_len = len,
        .data = {0},
    };
    strncpy(msg.data, av[1], len);

    if (send(sock, &msg, sizeof(msg), 0) < 0)
    {
        perror("cd: send");
        return;
    }

    printf("Send a message: CWD\n");
    printf("\n");

    // receive the message
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    if (select(sock + 1, &readfds, NULL, NULL, NULL) < 0)
    {
        perror("cd: select");
    }

    if (!FD_ISSET(sock, &readfds))
    {
        printf("readfds is not set.\n");
    }
    else if (FD_ISSET(sock, &readfds))
    {
        struct myftp_msg msg = {0};
        if ((recv(sock, &msg, sizeof(msg), 0)) > 0)
        {
            switch (msg.type)
            {
            case MYFTP_REP_MSGTYPE_COMM_OK:
                switch (msg.code)
                {
                case MYFTP_REP_MSGCODE_COMM_OK:
                    printf("Recieve a reply message: The command is ok\n");
                    break;

                default:
                    // receive the msg which type is different from COMM_OK_TO_CLIENT
                    printf("Error: Protocol Error\n");
                    break;
                }
                break;
            default:
                check_err_msg(msg);
                break;
            }
        }
        printf("\n");
    }
}

void dir(int ac, char *av[], int sock)
{
    // send a LIST message to server
    if (av[1] == NULL)
    {
        struct myftp_msg msg = {
            .type = MYFTP_COM_MSGTYPE_LIST,
            .code = 0,
            .data_len = 0,
            .data = {0},
        };

        if (send(sock, &msg, sizeof(msg), 0) < 0)
        {
            perror("dir: send");
            return;
        }

        printf("check: dir, and send (current dir)\n");
    }
    else
    {
        int len = strlen(av[1]);
        struct myftp_msg msg = {
            .type = MYFTP_COM_MSGTYPE_LIST,
            .code = 0,
            .data_len = len,
            .data = {0},
        };
        strncpy(msg.data, av[1], len);
        if (send(sock, &msg, sizeof(msg), 0) < 0)
        {
            perror("dir: send");
            return;
        }
        printf("check: dir, and send(dir: %s)\n", av[1]);
    }

    printf("Send a message: LIST\n");
    printf("\n");

    // receive the message
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    if (select(sock + 1, &readfds, NULL, NULL, NULL) < 0)
    {
        perror("dir: select");
    }

    if (!FD_ISSET(sock, &readfds))
    {
        printf("readfds is not set.\n");
    }
    else if (FD_ISSET(sock, &readfds))
    {
        struct myftp_msg msg = {0};
        if ((recv(sock, &msg, sizeof(msg), 0)) > 0)
        {
            switch (msg.type)
            {
            case MYFTP_REP_MSGTYPE_COMM_OK:
                switch (msg.code)
                {
                case MYFTP_REP_MSGCODE_COMM_OK_TO_CLIENT:
                    printf("Recieve a reply message: The command is ok\n");

                    // data_msg will come after the reply msg
                    int fin = 0;
                    while ((recv(sock, &msg, sizeof(msg), 0)) > 0 && fin == 0)
                    {
                        switch (msg.type)
                        {
                        case MYFTP_DATA_MSGTYPE:
                            switch (msg.code)
                            {
                            case MYFTP_DATA_MSGCODE_NOT_END:
                                printf("%s\n", msg.data);
                                break;

                            case MYFTP_DATA_MSGCODE_END:
                                printf("%s\n", msg.data);
                                fin++;
                                break;

                            default:
                                printf("Error: Protocol Error\n");
                                fin++;
                                break;
                            }
                            break;

                        default:
                            fin++;
                            printf("Error: Protocol Error\n");
                            break;
                        }
                        if (fin)
                        {
                            printf("\n");
                            return;
                        }
                    }
                    break;

                default:
                    // receive the msg which type is different from COMM_OK_TO_CLIENT
                    printf("Error: Protocol Error\n");
                    break;
                }
                break;
            default:
                check_err_msg(msg);
                break;
            }
        }
        printf("\n");
    }
}

// recieve data from the server
// path1 path2: server_path client_path(to save)
// path1      : server_path (when save data in client, it reuse the filename in server)
void get(int ac, char *av[], int sock)
{
    int fd = 0;

    // send a RETR message to server
    if (av[1] == NULL)
    {
        fprintf(stderr, "Usage: get <pathname1> [pathname2]\n");
        return;
    }

    int len = strlen(av[1]);
    struct myftp_msg msg = {
        .type = MYFTP_COM_MSGTYPE_RETR,
        .code = 0,
        .data_len = len,
        .data = {0},
    };
    strncpy(msg.data, av[1], len);
    if (send(sock, &msg, sizeof(msg), 0) < 0)
    {
        perror("get: send");
        return;
    }

    printf("Send a message: RETR\n");
    printf("\n");

    msg.data_len = msg.code = msg.type = 0;
    memset(msg.data, 0, strlen(msg.data));

    // receive the reply message
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    if (select(sock + 1, &readfds, NULL, NULL, NULL) < 0)
    {
        perror("get: select");
    }

    if (!FD_ISSET(sock, &readfds))
    {
        printf("readfds is not set.\n");
    }
    else if (FD_ISSET(sock, &readfds))
    {
        if ((recv(sock, &msg, sizeof(msg), 0)) > 0)
        {
            switch (msg.type)
            {
            case MYFTP_REP_MSGTYPE_COMM_OK:
                switch (msg.code)
                {
                case MYFTP_REP_MSGCODE_COMM_OK_TO_CLIENT:
                    printf("Recieve a reply message: The command is ok\n");

                    // open the file to save
                    if (av[2] != NULL)
                    {
                        if ((fd = open(av[2], O_WRONLY | O_CREAT, 0644)) < 0)
                        {
                            fprintf(stderr, "get: cannot open the file (path: %s)\n", av[2]);
                            return;
                        }
                    }
                    else
                    {
                        // if the name of the file did not specify, make the file whose name is same as the server file
                        if ((fd = open(av[1], O_WRONLY | O_CREAT, 0644)) < 0)
                        {
                            fprintf(stderr, "get: cannot open the file (path: %s)\n", av[1]);
                            return;
                        }
                    }
                }
                break;
            default:
                check_err_msg(msg);
                return;
            }
        }
    }
    msg.data_len = msg.code = msg.type = 0;
    memset(msg.data, 0, strlen(msg.data));

    // data_msg will come after the reply msg
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    if (select(sock + 1, &readfds, NULL, NULL, NULL) < 0)
    {
        perror("get: select");
    }

    if (!FD_ISSET(sock, &readfds))
    {
        printf("readfds is not set.\n");
    }
    else if (FD_ISSET(sock, &readfds))
    {
        while ((recv(sock, &msg, sizeof(msg), 0)) > 0)
        {
            if ((write(fd, msg.data, msg.data_len)) <= 0)
            {
                perror("get: write");
                return;
            }
        }
    }
}

// for store
void send_data_to_srv(int fd, int sock)
{
    // read fd, and write into sock
    char buf[DATA_SIZE] = {0};
    int n = 0;
    struct myftp_msg msg = {0};

    while ((n = read(fd, buf, DATA_SIZE)) > 0)
    {
        printf("Send a data message\n");
        strncpy(msg.data, buf, n);
        msg.type = MYFTP_DATA_MSGTYPE;
        msg.code = MYFTP_DATA_MSGCODE_NOT_END;
        msg.data_len = n;

        if (send(sock, &msg, sizeof(msg), 0) < 0)
        {
            perror("put: send");
            return;
        }
    }

    printf("Send a data message(this is the end)\n");
    msg.type = MYFTP_DATA_MSGTYPE;
    msg.code = MYFTP_DATA_MSGCODE_END;
    msg.data_len = 0;
    memset(msg.data, 0, strlen(msg.data));
    if (send(sock, &msg, sizeof(msg), 0) < 0)
    {
        perror("put: send");
    }
}

// send data to the the server from the client
// path1 path2: client_path  server_path(to save)
// path1      : client_path (when save data in server, it reuse the filename in  client)
void put(int ac, char *av[], int sock)
{
    if (av[1] == NULL)
    {
        fprintf(stderr, "Usage: put <pathname1> [pathname2]\n");
        return;
    }

    int fd = 0;
    if ((fd = open(av[1], O_RDONLY)) < 0)
    {
        fprintf(stderr, "put: cannot open the file (path: %s)\n", av[1]);
        return;
    }

    if (av[2] != NULL)
    {
        int len = strlen(av[2]);
        struct myftp_msg msg = {
            .type = MYFTP_COM_MSGTYPE_STORE,
            .code = 0,
            .data_len = len,
            .data = {0},
        };
        strncpy(msg.data, av[2], len);

        if (send(sock, &msg, sizeof(msg), 0) < 0)
        {
            perror("put: send");
            return;
        }
    }
    else
    {
        int len = strlen(av[1]);
        struct myftp_msg msg = {
            .type = MYFTP_COM_MSGTYPE_STORE,
            .code = 0,
            .data_len = len,
            .data = {0},
        };
        strncpy(msg.data, av[1], len);

        if (send(sock, &msg, sizeof(msg), 0) < 0)
        {
            perror("put: send");
            return;
        }
    }

    printf("Send a message: STORE\n");
    printf("\n");

    // receive the message
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    if (select(sock + 1, &readfds, NULL, NULL, NULL) < 0)
    {
        perror("put: select");
    }

    if (!FD_ISSET(sock, &readfds))
    {
        printf("readfds is not set.\n");
    }
    else if (FD_ISSET(sock, &readfds))
    {
        struct myftp_msg msg = {0};
        if ((recv(sock, &msg, sizeof(msg), 0)) > 0)
        {
            switch (msg.type)
            {
            case MYFTP_REP_MSGTYPE_COMM_OK:
                switch (msg.code)
                {
                case MYFTP_REP_MSGCODE_COMM_OK_TO_SERVER:
                    printf("Recieve a reply message: The command is ok\n");
                    send_data_to_srv(fd, sock);
                    break;

                default:
                    // receive the msg which type is different from COMM_OK_TO_CLIENT
                    printf("Error: Protocol Error\n");
                    break;
                }
                break;
            default:
                check_err_msg(msg);
                break;
            }
        }
        printf("\n");
    }
}

void lpwd(int ac, char *av[], int sock)
{
    // execute pwd in the client
    char pathname[PATHNAME_SIZE] = {0};
    if ((getcwd(pathname, PATHNAME_SIZE)) == NULL)
    {
        perror("lpwd: getcwd");
        return;
    }
    fprintf(stderr, "%s\n", pathname);
}

void lcd(int ac, char *av[], int sock)
{
    // execute cd in the client
    if (av[1] == NULL)
    {
        fprintf(stderr, "Usage: lcd <pathname>\n");
        return;
    }

    if ((chdir(av[1])) < 0)
    {
        perror("lcd: chdir");
        return;
    }
}

void ldir(int ac, char *av[], int sock)
{
    // execute dir command in the client
    DIR *dir;
    char pathname[PATHNAME_SIZE] = {0};

    // open directory
    if (av[1] == NULL)
    {
        // ldir in the current dir
        if ((getcwd(pathname, PATHNAME_SIZE)) == NULL)
        {
            perror("ldir: getcwd");
            return;
        }
        if ((dir = opendir(pathname)) == NULL)
        {
            perror("ldir: opendir");
        }
    }
    else
    {
        // check whther the path exists
        if ((access(av[1], R_OK)) == -1)
        {
            fprintf(stderr, "access: the pathname is wrong. Usage: ldir [pathname]\n");
            return;
        }

        // open the directory
        if ((dir = opendir(av[1])) == NULL)
        {
            fprintf(stderr, "opendir: cannot open the directory.\n");
            return;
        }
    }

    // show the files
    struct dirent *d_ent;
    while ((d_ent = readdir(dir)))
    {
        printf("%s  ", d_ent->d_name);
    }
    printf("\n");

    if (errno)
    {
        perror("readdir");
        return;
    }

    // close the directory
    if (closedir(dir))
    {
        perror("closedir");
        return;
    }
}

void help(int ac, char *av[], int sock)
{
    printf("\n");
    printf("\n");
    printf("\t\t\t\tGeneral Commands Manual\n");
    printf("\n");
    printf("\n");

    printf("NAME\n");
    printf("\thelp\n");
    printf("\n");
    printf("SYNOPSIS\n");
    printf("\thelp\n");
    printf("\n");
    printf("DESCRIPTION\n");
    printf("\tThis command shows the description and synopsis of commands.\n");
    printf("\n");
    printf("\n");
    printf("\n");

    printf("NAME\n");
    printf("\tquit\n");
    printf("\n");
    printf("SYNOPSIS\n");
    printf("\tquit\n");
    printf("\n");
    printf("DESCRIPTION\n");
    printf("\tThis command finishes this software.\n");
    printf("\n");

    printf("NAME\n");
    printf("\tpwd\n");
    printf("\n");
    printf("SYNOPSIS\n");
    printf("\tpwd\n");
    printf("\n");
    printf("DESCRIPTION\n");
    printf("\tThis command shows the current directory in the server.\n");
    printf("\n");
    printf("\n");
    printf("\n");

    printf("NAME\n");
    printf("\tcd\n");
    printf("\n");
    printf("SYNOPSIS\n");
    printf("\tcd pass_name\n");
    printf("\n");
    printf("DESCRIPTION\n");
    printf("\tThis command transfers the current directory in the server.\n");
    printf("\n");
    printf("\n");
    printf("\n");

    printf("NAME\n");
    printf("\tdir\n");
    printf("\n");
    printf("SYNOPSIS\n");
    printf("\tdir [pass_name]\n");
    printf("\n");
    printf("DESCRIPTION\n");
    printf("\tThis command gets the information of files in the server.\n");
    printf("\n");
    printf("\n");
    printf("\n");

    printf("NAME\n");
    printf("\tlpwd\n");
    printf("\n");
    printf("SYNOPSIS\n");
    printf("\tlpwd\n");
    printf("\n");
    printf("DESCRIPTION\n");
    printf("\tThis command shows the current directory in the client.\n");
    printf("\n");
    printf("\n");
    printf("\n");

    printf("NAME\n");
    printf("\tlcd\n");
    printf("\n");
    printf("SYNOPSIS\n");
    printf("\tlcd pass_name\n");
    printf("\n");
    printf("DESCRIPTION\n");
    printf("\tThis command transfers the current directory in the client.\n");
    printf("\n");
    printf("\n");
    printf("\n");

    printf("NAME\n");
    printf("\tldir\n");
    printf("\n");
    printf("SYNOPSIS\n");
    printf("\tldir [pass_name]\n");
    printf("\n");
    printf("DESCRIPTION\n");
    printf("\tThis command gets the information of files in the client.\n");
    printf("\n");
    printf("\n");
    printf("\n");

    printf("NAME\n");
    printf("\tget\n");
    printf("\n");
    printf("SYNOPSIS\n");
    printf("\tget pass_name1 [pass_name2]\n");
    printf("\n");
    printf("DESCRIPTION\n");
    printf("\tThis command forwards the file from server to client.\n");
    printf("\n");
    printf("\n");
    printf("\n");

    printf("NAME\n");
    printf("\tput\n");
    printf("\n");
    printf("SYNOPSIS\n");
    printf("\tput pass_name1 [pass_name2]\n");
    printf("\n");
    printf("DESCRIPTION\n");
    printf("\tThis command forwards the file from client to server.\n");
    printf("\n");
    printf("\n");
    printf("\n");
}

void token(char *line)
{
    int fin = 0;
    int count_space = 0;
    char *cp = line;
    char *p = line;
    ac = 0;
    memset(av, 0, sizeof(av));
    state = Start;

    while (!fin)
    {
        switch (state)
        {
        case Start:
            switch (*cp)
            {
            case ' ':
            case '\t':
                count_space++;
                cp++;
                p++;
                break;
            case '\n':
                state = Exit;
                break;
            default:
                state = Word;
                break;
            }
            break;

        case Word:
            switch (*cp)
            {
            case ' ':
            case '\t':
            case '\n':
                count_space++;
                state = Make;
                break;
            default:
                cp++;
                break;
            }
            break;

        case Make:
            switch (*cp)
            {
            case '\n':
                av[ac] = p;
                *cp = '\0';
                ac++;
                state = Exit;
                break;

            case ' ':
            case '\t':
                count_space++;
                av[ac] = p;
                *cp = '\0';
                cp++;
                p += strlen(av[ac]) + 1;
                ac++;
                state = Start;
                break;
            }
            break;

        case Exit:
            if (count_space == strlen(line))
            {
                if (!av[0])
                {
                    av[0] = NULL;
                }
            }
            fin++;
            break;
        }
    }
}

void command(int sock, struct sockaddr_in server_addr)
{
    char line[MAX_LEN] = {0};
    memset(av, 0, sizeof(av));
    memset(line, 0, sizeof(line));
    printf("myFTP$ ");
    if (fgets(line, MAX_LEN, stdin) == NULL)
    {
        if (ferror(stdin))
        {
            fprintf(stderr, "can't read the line\n");
        }
    }

    // if line is one character and it is '\n', go to the begin of the loop
    if (*line == '\n')
    {
        return;
    }

    token(line);

    if (av[0] == NULL)
    {
        // input is consist of only tab or space.
        return;
    }

    struct command_table *p;
    for (p = cmd_tbl; p->cmd; p++)
    {
        if (strcmp(av[0], p->cmd) == 0)
        {
            (*p->func)(ac, av, sock, server_addr);
            break;
        }
    }
    if (p->cmd == NULL)
    {
        fprintf(stderr, "Unknown command.\n");
    }
}

struct addrinfo hints;
struct addrinfo *info;
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: ./myftpc <the name of server host>\n");
        exit(1);
    }

    // register the server address
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(50021),
        .sin_addr = {.s_addr = htonl(INADDR_ANY)},
    };

    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(argv[1], NULL, &hints, &info))
    {
        perror("getaddrinfo");
        exit(1);
    }
    server_addr.sin_addr.s_addr = ((struct sockaddr_in *)(info->ai_addr))->sin_addr.s_addr;
    // freeaddrinfo(info);

    // make a TCP socket
    int sock;
    if ((sock = (socket(AF_INET, SOCK_STREAM, 0))) < 0)
    {
        perror("socket");
        exit(1);
    }

    // send a requesr of TCP connection
    if ((connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0)
    {
        perror("connect");
        exit(1);
    }

    printf("connect to the server: Success\n");
    client_state = Use;
    for (;;)
    {
        command(sock, server_addr);

        if (client_state == Close)
        {
            close(sock);
            exit(0);
        }
    }
}