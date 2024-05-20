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

#define MAX_CLIENT_NUM 5

void send_undefine_com_err(int sock)
{
    // send a reply message
    struct myftp_msg msg = {
        .type = MYFTP_REP_MSGTYPE_COMM_ERR,
        .code = MYFTP_REP_MSGCODE_NOCOMM_ERR,
        .data_len = 0,
        .data = {0},
    };

    if ((send(sock, &msg, sizeof(msg), 0)) < 0)
    {
        perror("quit: send");
    }

    printf("Send a message: ERROR(Command is not defines)\n");
}

void send_reply_pwd(int sock)
{
    // execute pwd in the server
    char pathname[PATHNAME_SIZE] = {0};
    if ((getcwd(pathname, PATHNAME_SIZE)) == NULL)
    {
        perror("pwd: getcwd");

        // if getcwd fails, send com_err(No right to access)
        struct myftp_msg msg = {
            .type = MYFTP_REP_MSGTYPE_COMM_ERR,
            .code = MYFTP_REP_MSGCODE_NO_RIGHT_ERR,
            .data_len = 0,
            .data = {0},
        };
        if (send(sock, &msg, sizeof(msg), 0) < 0)
        {
            perror("pwd: send");
        }
        printf("Send a message: ERROR(There is no right to access)\n");
    }
    else
    {
        int len = strlen(pathname);
        struct myftp_msg msg = {
            .type = MYFTP_REP_MSGTYPE_COMM_OK,
            .code = MYFTP_REP_MSGCODE_COMM_OK,
            .data_len = len,
            .data = {0},
        };

        strncpy(msg.data, pathname, len);
        if (send(sock, &msg, sizeof(msg), 0) < 0)
        {
            perror("pwd: send");
        }
        printf("Send a message: COMMAND OK\n");
    }
}

void send_reply_cwd(int sock, struct myftp_msg msg)
{
    struct myftp_msg send_msg = {0};
    if (msg.data_len == 0)
    {
        // Error: Protocol error
        send_msg.type = MYFTP_REP_MSGTYPE_COMM_ERR;
        send_msg.code = MYFTP_REP_MSGCODE_PROT_ERR;

        printf("Send a message: ERROR(Protocol Error)\n");
    }
    else
    {
        char pathname[PATHNAME_SIZE] = {0};
        int len = strlen(msg.data);
        strncpy(pathname, msg.data, len);

        if ((chdir(pathname)) < 0)
        {
            perror("cwd: chdir");
            send_msg.type = MYFTP_REP_MSGTYPE_FILE_ERR;
            send_msg.code = MYFTP_REP_MSGCODE_NO_FILE_ERR;

            printf("Send a message: FILE_ERR\n");
        }
        else
        {
            send_msg.type = MYFTP_REP_MSGTYPE_COMM_OK;
            send_msg.code = MYFTP_REP_MSGCODE_COMM_OK;

            int len = strlen(pathname);
            strncpy(send_msg.data, pathname, len);
            printf("Send a message: COMMAND OK\n");
        }
    }
    if (send(sock, &send_msg, sizeof(send_msg), 0) < 0)
    {
        perror("cwd: send");
    }
}

DIR *check_server_dir(int sock, struct myftp_msg msg, int data_code)
{
    // return error or not
    DIR *dir;
    char pathname[PATHNAME_SIZE] = {0};
    struct myftp_msg send_msg = {0};
    int error = 0;

    if (msg.data_len == 0)
    {
        if ((getcwd(pathname, PATHNAME_SIZE)) == NULL)
        {
            perror("dir: getcwd");

            // if getcwd fails, send error message and finish
            send_msg.type = MYFTP_REP_MSGTYPE_UNKWN_ERR;
            send_msg.code = MYFTP_REP_MSGCODE_NO_DEF_ERR;

            printf("Send a message: UNDEFINED_ERR\n");
            error++;
        }
        if ((dir = opendir(pathname)) == NULL)
        {
            perror("dir: opendir");
            send_msg.type = MYFTP_REP_MSGTYPE_FILE_ERR;
            send_msg.code = MYFTP_REP_MSGCODE_NO_RIGHT_ERR;
            printf("Send a message: FILE_ERR(There is no right to access)\n");
            error++;
        }
    }
    else
    {
        // check whther the path exists
        if ((access(msg.data, R_OK)) == -1)
        {
            fprintf(stderr, "access: the pathname is wrong. Usage: ldir [pathname]\n");
            send_msg.type = MYFTP_REP_MSGTYPE_FILE_ERR;
            send_msg.code = MYFTP_REP_MSGCODE_NO_FILE_ERR;

            printf("Send a message: FILE_ERR(There is no such file)\n");
            error++;
        }
        strncpy(pathname, msg.data, strlen(msg.data));
    }

    if ((dir = opendir(pathname)) == NULL)
    {
        fprintf(stderr, "opendir: cannot open the directory.\n");
        send_msg.type = MYFTP_REP_MSGTYPE_FILE_ERR;
        send_msg.code = MYFTP_REP_MSGCODE_NO_RIGHT_ERR;

        printf("Send a message: FILE_ERR(There is no right to access)\n");
        error++;
    }
    else
    {
        printf("Send a message: COMMAND OK\n");
        send_msg.type = MYFTP_REP_MSGTYPE_COMM_OK;
        send_msg.code = data_code;
    }

    // send the reply message
    if (send(sock, &send_msg, sizeof(send_msg), 0) < 0)
    {
        perror("dir: send");
    }

    if (error)
        return NULL;

    return dir;
}

void send_reply_list(int sock, struct myftp_msg msg)
{
    DIR *dir = check_server_dir(sock, msg, MYFTP_REP_MSGCODE_COMM_OK_TO_CLIENT);

    if (dir == NULL)
        return;

    struct dirent *d_ent;
    struct myftp_msg send_msg = {0};

    while ((d_ent = readdir(dir)))
    {
        int add_len = strlen(d_ent->d_name);
        int data_len = strlen(send_msg.data);
        char space[] = "  ";
        int space_len = strlen(space);

        if ((data_len + add_len + space_len) <= DATA_SIZE)
        {
            strncat(send_msg.data, d_ent->d_name, strlen(d_ent->d_name));
            strncat(send_msg.data, space, strlen(space));
        }
        else
        {
            printf("Send a data message\n");
            send_msg.type = MYFTP_DATA_MSGTYPE;
            send_msg.code = MYFTP_DATA_MSGCODE_NOT_END;
            send_msg.data_len = strlen(send_msg.data);

            if (send(sock, &send_msg, sizeof(send_msg), 0) < 0)
            {
                perror("list: send");
                return;
            }
            memset(send_msg.data, 0, sizeof(send_msg));
            send_msg.data_len = 0;
            strncpy(send_msg.data, d_ent->d_name, strlen(d_ent->d_name));
        }
    }
    printf("Send a data message(this is the end of the message)\n");
    send_msg.type = MYFTP_DATA_MSGTYPE;
    send_msg.code = MYFTP_DATA_MSGCODE_END;
    send_msg.data_len = strlen(send_msg.data);

    if (send(sock, &send_msg, sizeof(send_msg), 0) < 0)
    {
        perror("list: send");
        return;
    }
    memset(send_msg.data, 0, sizeof(send_msg));
    send_msg.data_len = 0;
    send_msg.type = send_msg.code = 0;

    if (errno)
    {
        perror("readdir");
        send_msg.type = MYFTP_REP_MSGTYPE_FILE_ERR;
        send_msg.code = MYFTP_REP_MSGCODE_NO_RIGHT_ERR;

        printf("Send a message: FILE_ERR(There is no right to access)\n");
        if (send(sock, &send_msg, sizeof(send_msg), 0) < 0)
        {
            perror("list: send");
            return;
        }
    }

    // close the directory
    if (closedir(dir))
    {
        perror("list: closedir");
    }
}

void send_data_to_cl(int fd, int sock)
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
            perror("retr: send");
            return;
        }
    }

    printf("Send a data message (this is the end)\n");
    msg.data_len = 0;
    msg.type = MYFTP_DATA_MSGTYPE;
    msg.code = MYFTP_DATA_MSGCODE_END;
    // memset(msg.data, 0, strlen(msg.data));
    if (send(sock, &msg, sizeof(msg), 0) < 0)
    {
        perror("retr: send");
        return;
    }
}

// send data to the client from the server
void send_reply_retr(int sock, struct myftp_msg msg)
{
    char pathname[PATHNAME_SIZE] = {0};
    struct myftp_msg send_msg = {0};
    int error = 0;

    if (msg.data_len == 0)
    {
        // if getcwd fails, send error message and finish
        send_msg.type = MYFTP_REP_MSGTYPE_UNKWN_ERR;
        send_msg.code = MYFTP_REP_MSGCODE_NO_DEF_ERR;

        printf("send_reply_retr: getcwd fails\n");
        printf("Send a message: UNDEFINED_ERR\n");
        error++;
    }
    else
        strncpy(pathname, msg.data, strlen(msg.data));

    // open the file of the server
    int fd = 0;
    if ((fd = open(pathname, O_RDONLY)) < 0)
    {
        fprintf(stderr, "retr: cannot open the file (path: %s)\n", pathname);
        send_msg.type = MYFTP_REP_MSGTYPE_FILE_ERR;
        send_msg.code = MYFTP_REP_MSGCODE_NO_FILE_ERR;

        printf("send_reply_retr: cannot open the file\n");
        printf("Send a message: FILE_ERR (NO_FILE_ERR)\n");
        error++;
    }

    if (error == 0)
    {
        printf("Send a message: COMMAND OK\n");
        send_msg.type = MYFTP_REP_MSGTYPE_COMM_OK;
        send_msg.code = MYFTP_REP_MSGCODE_COMM_OK_TO_CLIENT;
    }

    if (send(sock, &send_msg, sizeof(send_msg), 0) < 0)
    {
        perror("retr: send");
        return;
    }

    if (error)
        return;

    memset(send_msg.data, 0, strlen(send_msg.data));
    send_msg.data_len = 0;
    send_msg.type = send_msg.code = 0;

    send_data_to_cl(fd, sock);
}

// recieve data from the client
void send_reply_store(int sock, struct myftp_msg msg)
{
    struct myftp_msg send_msg = {0};
    int error = 0;

    if (msg.data_len == 0)
    {
        printf("Send a message: COMMAND ERR (PROTOCOL_ERROR)\n");
        send_msg.type = MYFTP_REP_MSGTYPE_COMM_ERR;
        send_msg.code = MYFTP_REP_MSGCODE_PROT_ERR;
        error++;
    }

    int fd = 0;
    if ((fd = open(msg.data, O_WRONLY | O_CREAT, 0644)) < 0)
    {
        fprintf(stderr, "store: cannot open the file (path: %s)\n", msg.data);
        error++;
    }

    if (error == 0)
    {
        printf("Send a message: COMMAND OK\n");
        send_msg.type = MYFTP_REP_MSGTYPE_COMM_OK;
        send_msg.code = MYFTP_REP_MSGCODE_COMM_OK_TO_SERVER;
    }

    if (send(sock, &send_msg, sizeof(send_msg), 0) < 0)
    {
        perror("store: send");
        return;
    }
    if (error)
        return;

    // recieve data from the client
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    if (select(sock + 1, &readfds, NULL, NULL, NULL) < 0)
    {
        perror("store: select");
        return;
    }

    if (!FD_ISSET(sock, &readfds))
    {
        printf("readfds is not set.\n");
    }
    else if (FD_ISSET(sock, &readfds))
    {
        // server: recieve data from the client
        memset(msg.data, 0, strlen(msg.data));
        msg.code = msg.type = msg.data_len = 0;

        while ((recv(sock, &msg, sizeof(msg), 0)) > 0)
        {
            switch (msg.type)
            {
            case MYFTP_DATA_MSGTYPE:
                if (msg.code == MYFTP_DATA_MSGCODE_NOT_END)
                {
                    printf("Recieve a data message\n");
                    if ((write(fd, msg.data, msg.data_len)) <= 0)
                    {
                        perror("store: write");
                        return;
                    }
                }
                else if (msg.code == MYFTP_DATA_MSGCODE_END)
                {
                    printf("Recieve a data message (this is the end)\n");
                    return;
                }
                else
                {
                    printf("Protocol error\n");
                    return;
                }
                break;
            default:
                fprintf(stderr, "Protocol error\n");
                return;
            }
        }
    }
}

int main(int argc, char **argv)
{
    if (argc > 2)
    {
        fprintf(stderr, "Usage: ./myftpd [<the current dir>]\n");
        exit(1);
    }
    else if (argc == 2)
    {
        // argv[1] is the current dir
        if ((chdir(argv[1])) < 0)
        {
            perror("main: chdir");
            exit(1);
        }
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(50021),
        .sin_addr = {.s_addr = htonl(INADDR_ANY)},
    };

    int sock = 0;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("main: socket");
        exit(1);
    }

    if ((bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0)
    {
        perror("main: bind");
        exit(1);
    }

    if (listen(sock, MAX_CLIENT_NUM))
    {
        perror("main: listen");
        exit(1);
    }

    struct sockaddr_in client_addr;
    socklen_t sock_len = sizeof(client_addr);

    int sock_new = 0;
    printf("wait the request of the connection...\n");
    if ((sock_new = accept(sock, (struct sockaddr *)&client_addr, &sock_len)) < 0)
    {
        perror("main: accept");
        exit(1);
    }

    printf("accept the client. the number of clients\n");
    printf("\n");

    int pid = 0;

    for (;;)
    {
        if ((pid = fork()) > 0)
        {
            printf("wait the request of the connection...\n");
            sock_new = 0;
            if ((sock_new = accept(sock, (struct sockaddr *)&client_addr, &sock_len)) < 0)
            {
                perror("main: accept");
            }

            printf("accept the client. the number of clients\n");
            printf("\n");
        }
        else if (pid == 0)
        {
            for (;;)
            {
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(sock_new, &readfds);
                if (select(sock_new + 1, &readfds, NULL, NULL, NULL) < 0)
                {
                    perror("main: select");
                }

                if (!FD_ISSET(sock_new, &readfds))
                {
                    printf("readfds is not set.\n");
                }
                else if (FD_ISSET(sock_new, &readfds))
                {
                    struct myftp_msg msg = {0};
                    if ((recv(sock_new, &msg, sizeof(msg), 0)) > 0)
                    {
                        switch (msg.type)
                        {
                        case MYFTP_COM_MSGTYPE_QUIT:
                            printf("Receive a message: QUIT\n");
                            printf("quit connection with this client.\n");
                            printf("\n");
                            close(sock_new);
                            exit(0);
                            break;

                        case MYFTP_COM_MSGTYPE_PWD:
                            printf("Receive a message: PWD\n");
                            send_reply_pwd(sock_new);
                            break;

                        case MYFTP_COM_MSGTYPE_CWD:
                            printf("Receive a message: CWD\n");
                            send_reply_cwd(sock_new, msg);
                            break;

                        case MYFTP_COM_MSGTYPE_LIST:
                            printf("Receive a message: LIST\n");
                            send_reply_list(sock_new, msg);
                            break;

                        case MYFTP_COM_MSGTYPE_RETR:
                            printf("Receive a message: RETR\n");
                            send_reply_retr(sock_new, msg);
                            break;

                        case MYFTP_COM_MSGTYPE_STORE:
                            printf("Receive a message: STORE\n");
                            send_reply_store(sock_new, msg);
                            break;

                        case MYFTP_DATA_MSGTYPE:

                            if (msg.code == MYFTP_DATA_MSGCODE_END)
                            {
                                printf("Recieve data from the client\n");
                            }
                            break;
                        default:
                            // send the err: the command is not defined
                            send_undefine_com_err(sock_new);
                            printf("quit connection with this client.\n");
                            printf("\n");
                            exit(0);
                            break;
                        }
                    }
                }
            }
        }
        else
        {
            perror("fork");
        }
    }
}
