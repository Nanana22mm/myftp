#include <inttypes.h>
#include <netinet/in.h>

// Command Message: type(do not use code)
#define MYFTP_COM_MSGTYPE_QUIT 0x01
#define MYFTP_COM_MSGTYPE_PWD 0x02
#define MYFTP_COM_MSGTYPE_CWD 0x03
#define MYFTP_COM_MSGTYPE_LIST 0x04
#define MYFTP_COM_MSGTYPE_RETR 0x05
#define MYFTP_COM_MSGTYPE_STORE 0x06

// Reply Message: type and code
/* Type */
#define MYFTP_REP_MSGTYPE_COMM_OK 0x10
#define MYFTP_REP_MSGTYPE_COMM_ERR 0x011
#define MYFTP_REP_MSGTYPE_FILE_ERR 0x012
#define MYFTP_REP_MSGTYPE_UNKWN_ERR 0x013
/* Code */
#define MYFTP_REP_MSGCODE_COMM_OK 0x00
#define MYFTP_REP_MSGCODE_COMM_OK_TO_CLIENT 0x01
#define MYFTP_REP_MSGCODE_COMM_OK_TO_SERVER 0x02
#define MYFTP_REP_MSGCODE_STRUCT_ERR 0x01
#define MYFTP_REP_MSGCODE_NOCOMM_ERR 0x02
#define MYFTP_REP_MSGCODE_PROT_ERR 0x03
#define MYFTP_REP_MSGCODE_NO_FILE_ERR 0x00
#define MYFTP_REP_MSGCODE_NO_RIGHT_ERR 0x01
#define MYFTP_REP_MSGCODE_NO_DEF_ERR 0x05

// Data Message: type and code
/* Type */
#define MYFTP_DATA_MSGTYPE 0x20
/* Code */
#define MYFTP_DATA_MSGCODE_END 0x00
#define MYFTP_DATA_MSGCODE_NOT_END 0x01

#define IPADDR_LEN 20
#define MAX_LEN 512
#define PATHNAME_SIZE 256
#define DATA_SIZE 1024

struct __attribute__((__packed__)) myftp_msg
{
    uint8_t type;
    uint8_t code;
    uint16_t data_len;
    char data[DATA_SIZE];
};

extern struct myftp_msg *pkt_d; /* header + data */
extern struct myftp_msg *pkt_h; /* header + only */
