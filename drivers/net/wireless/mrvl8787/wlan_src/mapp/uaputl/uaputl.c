/** @file  uaputl.c
 *
 *  @brief Program to send AP commands to the driver/firmware of the uAP
 *         driver.
 *
 *   Usage: uaputl.exe [-option params] 
 *   or		uaputl.exe [command] [params]
 * 
 *  Copyright (C) 2008-2011, Marvell International Ltd.
 *  All Rights Reserved
 */
/****************************************************************************
Change log:
    03/01/08: Initial creation
****************************************************************************/

/****************************************************************************
        Header files
****************************************************************************/
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdio.h>
#include <getopt.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "uaputl.h"
#include "uapcmd.h"

/****************************************************************************
        Definitions
****************************************************************************/
/** Default debug level */
int debug_level = MSG_NONE;

/** Enable or disable debug outputs */
#define DEBUG   1

/** Convert character to integer */
#define CHAR2INT(x) (((x) >= 'A') ? ((x) - 'A' + 10) : ((x) - '0'))

/****************************************************************************
        Global variables
****************************************************************************/
/** Device name */
static char dev_name[IFNAMSIZ + 1];
/** Option for cmd */
struct option cmd_options[] = {
    {"help", 0, 0, 'h'},
    {0, 0, 0, 0}
};

/****************************************************************************
        Local functions
****************************************************************************/
/**
 *    @brief Convert char to hex integer
 *   
 *    @param chr          Char
 *    @return             Hex integer
 */
unsigned char
hexc2bin(char chr)
{
    if (chr >= '0' && chr <= '9')
        chr -= '0';
    else if (chr >= 'A' && chr <= 'F')
        chr -= ('A' - 10);
    else if (chr >= 'a' && chr <= 'f')
        chr -= ('a' - 10);

    return chr;
}

/** 
 *  @brief Check protocol is valid or not
 *
 *  @param protocol    	     Protocol
 *
 *  @return         UAP_SUCCESS or UAP_FAILURE
 */
int
is_protocol_valid(int protocol)
{
    int ret = UAP_FAILURE;
    switch (protocol) {
    case PROTOCOL_NO_SECURITY:
    case PROTOCOL_STATIC_WEP:
    case PROTOCOL_WPA:
    case PROTOCOL_WPA2:
    case PROTOCOL_WPA2_MIXED:
        ret = UAP_SUCCESS;
        break;
    default:
        printf("ERR: Invalid Protocol: %d\n", protocol);
        break;
    }
    return ret;
}

/**
 *  @brief Function to check valid rate
 *
 *                  
 *  @param  rate    Rate to verify
 *
 *  return 	    UAP_SUCCESS or UAP_FAILURE	
 **/
int
is_rate_valid(int rate)
{
    int ret = UAP_SUCCESS;
    switch (rate) {
    case 2:
    case 4:
    case 11:
    case 22:
    case 12:
    case 18:
    case 24:
    case 48:
    case 72:
    case 96:
    case 108:
    case 36:
        break;
    default:
        ret = UAP_FAILURE;
        break;
    }
    return ret;
}

/**
 *  @brief  Detects duplicates rate in array of strings
 *          Note that 0x82 and 0x2 are same for rate
 *
 *  @param  argc    Number of elements
 *  @param  argv    Array of strings
 *  @return UAP_FAILURE or UAP_SUCCESS
 */
inline int
has_dup_rate(int argc, char *argv[])
{
    int i, j;
    /* Check for duplicate */
    for (i = 0; i < (argc - 1); i++) {
        for (j = i + 1; j < argc; j++) {
            if ((A2HEXDECIMAL(argv[i]) & ~BASIC_RATE_SET_BIT) ==
                (A2HEXDECIMAL(argv[j]) & ~BASIC_RATE_SET_BIT)) {
                return UAP_FAILURE;
            }
        }
    }
    return UAP_SUCCESS;
}

/**
 *  @brief Check for mandatory rates
 *
 *
 * 2, 4, 11, 22 must be present 
 *
 * 6 12 and 24 must be present for ofdm
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_FAILURE or UAP_SUCCESS
 *
 */
int
check_mandatory_rates(int argc, char **argv)
{
    int i;
    int tmp;
    t_u32 rate_bitmap = 0;
    int cck_enable = 0;
    int ofdm_enable = 0;
#define BITMAP_RATE_1M         0x01
#define BITMAP_RATE_2M         0x02
#define BITMAP_RATE_5_5M       0x04
#define BITMAP_RATE_11M        0x8
#define B_RATE_MANDATORY       0x0f
#define BITMAP_RATE_6M         0x10
#define BITMAP_RATE_12M        0x20
#define BITMAP_RATE_24M        0x40
#define G_RATE_MANDATORY        0x70
    for (i = 0; i < argc; i++) {
        tmp = (A2HEXDECIMAL(argv[i]) & ~BASIC_RATE_SET_BIT);
        switch (tmp) {
        case 2:
            cck_enable = 1;
            rate_bitmap |= BITMAP_RATE_1M;
            break;
        case 4:
            cck_enable = 1;
            rate_bitmap |= BITMAP_RATE_2M;
            break;
        case 11:
            cck_enable = 1;
            rate_bitmap |= BITMAP_RATE_5_5M;
            break;
        case 22:
            cck_enable = 1;
            rate_bitmap |= BITMAP_RATE_11M;
            break;
        case 12:
            ofdm_enable = 1;
            rate_bitmap |= BITMAP_RATE_6M;
            break;
        case 24:
            ofdm_enable = 1;
            rate_bitmap |= BITMAP_RATE_12M;
            break;
        case 48:
            ofdm_enable = 1;
            rate_bitmap |= BITMAP_RATE_24M;
            break;
        case 18:
        case 36:
        case 72:
        case 96:
        case 108:
            ofdm_enable = 1;
            break;
        }
    }
    if ((rate_bitmap & B_RATE_MANDATORY) != B_RATE_MANDATORY) {
        if (cck_enable) {
            printf("Basic Rates 2, 4, 11 and 22 (500K units) \n"
                   "must be present in basic or non-basic rates\n");
            return UAP_FAILURE;
        }
    }
    if (ofdm_enable && ((rate_bitmap & G_RATE_MANDATORY) != G_RATE_MANDATORY)) {
        printf("OFDM Rates 12, 24 and 48 ( 500Kb units)\n"
               "must be present in basic or non-basic rates\n");
        return UAP_FAILURE;
    }
    return UAP_SUCCESS;
}

/**
 *  @brief  Detects duplicates channel in array of strings
 *          
 *  @param  argc    Number of elements
 *  @param  argv    Array of strings
 *  @return UAP_FAILURE or UAP_SUCCESS
 */
inline int
has_dup_channel(int argc, char *argv[])
{
    int i, j;
    /* Check for duplicate */
    for (i = 0; i < (argc - 1); i++) {
        for (j = i + 1; j < argc; j++) {
            if (atoi(argv[i]) == atoi(argv[j])) {
                return UAP_FAILURE;
            }
        }
    }
    return UAP_SUCCESS;
}

/** 
 *    @brief Convert string to hex integer
 *  
 *    @param s            A pointer string buffer
 *    @return             Hex integer
 */
unsigned int
a2hex(char *s)
{
    unsigned int val = 0;
    if (!strncasecmp("0x", s, 2)) {
        s += 2;
    }
    while (*s && isxdigit(*s)) {
        val = (val << 4) + hexc2bin(*s++);
    }
    return val;
}

/** 
 *  @brief Dump hex data
 *
 *  @param prompt	A pointer prompt buffer
 *  @param p		A pointer to data buffer
 *  @param len		The len of data buffer
 *  @param delim	Delim char
 *  @return            	None
 */
void
hexdump_data(char *prompt, void *p, int len, char delim)
{
    int i;
    unsigned char *s = p;

    if (prompt) {
        printf("%s: len=%d\n", prompt, (int) len);
    }
    for (i = 0; i < len; i++) {
        if (i != len - 1)
            printf("%02x%c", *s++, delim);
        else
            printf("%02x\n", *s);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n");
}

#if DEBUG
/** 
 * @brief           Conditional printf
 *
 * @param level     Severity level of the message
 * @param fmt       Printf format string, followed by optional arguments
 */
void
uap_printf(int level, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (level <= debug_level) {
        vprintf(fmt, ap);
    }
    va_end(ap);
}

/** 
 *  @brief Dump hex data
 *
 *  @param prompt	A pointer prompt buffer
 *  @param p		A pointer to data buffer
 *  @param len		The len of data buffer
 *  @param delim	Delim char
 *  @return            	None
 */
void
hexdump(char *prompt, void *p, int len, char delim)
{
    if (debug_level < MSG_ALL)
        return;
    hexdump_data(prompt, p, len, delim);
}
#endif

/** 
 *  @brief      Hex to number 
 *
 *  @param c    Hex value
 *  @return     Integer value or -1
 */
int
hex2num(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}

/**
 *  @brief Show usage information for the sys_info command
 *
 *  $return         N/A
 */
void
print_sys_info_usage(void)
{
    printf("\nUsage : sys_info\n");
    return;
}

/**
 *  @brief Parse domain file for country information
 *
 *  @param country  Country name
 *  @param band    Band Info. 0x01 : B band, 0x02 : G band, 0x04 : A band.
 *  @param sub_bands Band information 
 *  @return number of band/ UAP_FAILURE 
 */
t_u8
parse_domain_file(char *country, int band, ieeetypes_subband_set_t * sub_bands)
{
    FILE *fp;
    char str[64];
    char domain_name[40];
    int cflag = 0;
    int dflag = 0;
    int found = 0;
    int skip = 0;
    int j = -1, reset_j = 0;
    t_u8 no_of_sub_band = 0;

    fp = fopen("config/80211d_domain.conf", "r");
    if (fp == NULL) {
        printf("File opening Error\n");
        return UAP_FAILURE;
    }

    /** 
     * Search specific domain name
     */
    while (!feof(fp)) {
        fscanf(fp, "%s", str);
        if (cflag) {
            strcpy(domain_name, str);
            cflag = 0;
        }
        if (!strcmp(str, "COUNTRY:")) {
            /** Store next string to domain_name */
            cflag = 1;
        }

        if (!strcmp(str, country)) {
            /** Country is matched ;)*/
            found = 1;
            break;
        }
    }

    if (!found) {
        printf("No match found for Country = %s in the 80211d_domain.conf \n",
               country);
        fclose(fp);
        found = 0;
        return UAP_FAILURE;
    }

    /**
     * Search domain specific information
     */
    while (!feof(fp)) {
        fscanf(fp, "%s", str);

        if (feof(fp)
            ) {
            break;
        }

        if (dflag && !strcmp(str, "DOMAIN:")) {

            if ((band & BAND_A) == 0)
                break;

            /* parse next domain */
            cflag = 0;
            dflag = 0;
            j = -1;
            reset_j = 0;
        }
        if (dflag) {
            j++;
            if (strchr(str, ','))
                reset_j = 1;

            strcpy(str, strtok(str, ", "));

            if (str == NULL) {
                if (reset_j) {
                    j = -1;
                    reset_j = 0;
                }
                continue;
            }

            if (IS_HEX_OR_DIGIT(str) == UAP_FAILURE) {
                printf("ERR: Only Number values are allowed\n");
                fclose(fp);
                return UAP_FAILURE;
            }

            switch (j) {
            case 0:
                sub_bands[no_of_sub_band].first_chan = (t_u8) A2HEXDECIMAL(str);
                break;
            case 1:
                sub_bands[no_of_sub_band].no_of_chan = (t_u8) A2HEXDECIMAL(str);
                break;
            case 2:
                sub_bands[no_of_sub_band++].max_tx_pwr =
                    (t_u8) A2HEXDECIMAL(str);
                break;
            default:
                printf("ERR: Incorrect 80211d_domain.conf file\n");
                fclose(fp);
                return UAP_FAILURE;
            }

            if (reset_j) {
                j = -1;
                reset_j = 0;
            }
        }

        if (cflag && !strcmp(str, domain_name)) {
            /* Followed will be the band details */
            cflag = 0;
            if (band & (BAND_B | BAND_G) || skip)
                dflag = 1;
            else
                skip = 1;
        }
        if (!dflag && !strcmp(str, "DOMAIN:")) {
            cflag = 1;
        }
    }
    fclose(fp);
    return (no_of_sub_band);

}

/** 
 *
 *  @brief Set/Get SNMP MIB
 *
 *  @param action 0-GET 1-SET
 *  @param oid    Oid
 *  @param size   Size of oid value
 *  @param oid_buf  Oid value
 *  @return UAP_FAILURE or UAP_SUCCESS
 *
 */
int
sg_snmp_mib(t_u16 action, t_u16 oid, t_u16 size, t_u8 * oid_buf)
{
    apcmdbuf_snmp_mib *cmd_buf = NULL;
    tlvbuf_header *tlv = NULL;
    int ret = UAP_FAILURE;
    t_u8 *buf = NULL;
    t_u16 buf_len;
    t_u16 cmd_len;
    int i;

    buf_len = sizeof(apcmdbuf_snmp_mib) + sizeof(tlvbuf_header) + size;
    buf = (t_u8 *) malloc(buf_len);
    if (!buf) {
        printf("ERR:Cannot allocate buffer from command!\n");
        return ret;
    }
    memset(buf, 0, buf_len);

    /* Locate Headers */
    cmd_buf = (apcmdbuf_snmp_mib *) buf;
    tlv = (tlvbuf_header *) (buf + sizeof(apcmdbuf_snmp_mib));
    cmd_buf->size = buf_len - BUF_HEADER_SIZE;
    cmd_buf->result = 0;
    cmd_buf->seq_num = 0;
    cmd_buf->cmd_code = HostCmd_SNMP_MIB;

    tlv->type = uap_cpu_to_le16(oid);
    tlv->len = uap_cpu_to_le16(size);
    for (i = 0; action && (i < size); i++) {
        tlv->data[i] = oid_buf[i];
    }

    cmd_buf->action = uap_cpu_to_le16(action);
    cmd_len = buf_len;
    ret = uap_ioctl((t_u8 *) cmd_buf, &cmd_len, buf_len);
    if (ret == UAP_SUCCESS) {
        if (cmd_buf->result == CMD_SUCCESS) {
            if (!action) {
                /** Relocate the headers */
                tlv =
                    (tlvbuf_header *) ((t_u8 *) cmd_buf +
                                       sizeof(apcmdbuf_snmp_mib));
                for (i = 0; i < MIN(uap_le16_to_cpu(tlv->len), size); i++) {
                    oid_buf[i] = tlv->data[i];
                }
            }
            ret = UAP_SUCCESS;
        } else {
            printf("ERR:Command Response incorrect!\n");
            ret = UAP_FAILURE;
        }
    } else {
        printf("ERR:Command sending failed!\n");
    }
    free(buf);
    return ret;
}

/** 
 *  @brief Creates a sys_info request and sends to the driver
 *
 *  Usage: "sys_info"
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_sys_info(int argc, char *argv[])
{
    apcmdbuf_sys_info_request *cmd_buf = NULL;
    apcmdbuf_sys_info_response *response_buf = NULL;
    t_u8 *buf = NULL;
    t_u16 cmd_len;
    t_u16 buf_len;
    int ret = UAP_SUCCESS;
    int opt;

    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_sys_info_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    /* Check arguments */
    if (argc != 0) {
        printf("ERR:Too many arguments.\n");
        print_sys_info_usage();
        return UAP_FAILURE;
    }

    buf_len =
        (sizeof(apcmdbuf_sys_info_request) >=
         sizeof(apcmdbuf_sys_info_response)) ? sizeof(apcmdbuf_sys_info_request)
        : sizeof(apcmdbuf_sys_info_response);

    /* Alloc buf for command */
    buf = (t_u8 *) malloc(buf_len);

    if (!buf) {
        printf("ERR:Cannot allocate buffer from command!\n");
        return UAP_FAILURE;
    }
    memset(buf, 0, buf_len);

    /* Locate headers */
    cmd_len = sizeof(apcmdbuf_sys_info_request);
    cmd_buf = (apcmdbuf_sys_info_request *) buf;
    response_buf = (apcmdbuf_sys_info_response *) buf;

    /* Fill the command buffer */
    cmd_buf->cmd_code = APCMD_SYS_INFO;
    cmd_buf->size = cmd_len;
    cmd_buf->seq_num = 0;
    cmd_buf->result = 0;

    /* Send the command */
    ret = uap_ioctl((t_u8 *) cmd_buf, &cmd_len, buf_len);

    /* Process response */
    if (ret == UAP_SUCCESS) {
        /* Verify response */
        if (response_buf->cmd_code != (APCMD_SYS_INFO | APCMD_RESP_CHECK)) {
            printf("ERR:Corrupted response!\n");
            free(buf);
            return UAP_FAILURE;
        }
        /* Print response */
        if (response_buf->result == CMD_SUCCESS) {
            printf("System information = %s\n", response_buf->sys_info);
        } else {
            printf("ERR:Could not retrieve system information!\n");
            ret = UAP_FAILURE;
        }
    } else {
        printf("ERR:Command sending failed!\n");
    }

    free(buf);
    return ret;
}

/**
 *  @brief Show usage information for deepsleep command
 *
 *  $return         N/A
 */
void
print_deepsleep_usage(void)
{
    printf("\nUsage : deepsleep [MODE][IDLE_TIME]");
    printf("\nOptions: MODE : 0 - Disable auto deep sleep mode");
    printf("\n                1 - Enable auto deep sleep mode");
    printf
        ("\n         IDLE_TIME: Idle time in milliseconds, default value is 100 ms");
    printf("\n         empty - get auto deep sleep mode\n");
    return;
}

/** 
 *  @brief Creates deepsleep  request and send to driver
 *
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_deepsleep(int argc, char *argv[])
{
    int opt;
    deep_sleep_para param;
    struct ifreq ifr;
    t_s32 sockfd;
    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_deepsleep_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    memset(&param, 0, sizeof(param));
    /* Check arguments */
    if (argc > 2) {
        printf("ERR:wrong arguments. Only support 2 arguments\n");
        print_deepsleep_usage();
        return UAP_FAILURE;
    }
    param.subcmd = UAP_DEEP_SLEEP;
    if (argc) {
        if (argc >= 1) {
            if ((IS_HEX_OR_DIGIT(argv[0]) == UAP_FAILURE) ||
                ((atoi(argv[0]) < 0) || (atoi(argv[0]) > 1))) {
                printf("ERR: Only Number values are allowed\n");
                print_deepsleep_usage();
                return UAP_FAILURE;
            }
        }
        param.action = 1;
        param.deep_sleep = (t_u16) A2HEXDECIMAL(argv[0]);
        param.idle_time = DEEP_SLEEP_IDLE_TIME;
        if (argc == 2) {
            if (IS_HEX_OR_DIGIT(argv[0]) == UAP_FAILURE) {
                printf("ERR: Only Number values are allowed\n");
                print_deepsleep_usage();
                return UAP_FAILURE;
            }
            param.idle_time = (t_u16) A2HEXDECIMAL(argv[1]);
        }
    } else {
        param.action = 0;
    }
    /* Open socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("ERR:Cannot open socket\n");
        return UAP_FAILURE;
    }
    /* Initialize the ifr structure */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_ifrn.ifrn_name, dev_name, strlen(dev_name));
    ifr.ifr_ifru.ifru_data = (void *) &param;
    /* Perform ioctl */
    errno = 0;
    if (ioctl(sockfd, UAP_IOCTL_CMD, &ifr)) {
        perror("");
        printf("ERR:deep sleep failed\n");
        close(sockfd);
        return UAP_FAILURE;
    }
    if (!argc) {
        printf("deep sleep mode: %s\n",
               (param.deep_sleep == 1) ? "enabled" : "disabled");
    }
    /* Close socket */
    close(sockfd);
    return UAP_SUCCESS;
}

/** 
 *  @brief Process host_cmd
 *
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         N/A
 */
int
apcmd_hostcmd(int argc, char *argv[])
{
    apcmdbuf *hdr;
    t_u8 *buffer = NULL;
    int ret = UAP_SUCCESS;
    t_u16 cmd_len;
    t_s8 cmdname[256];

    if (argc <= 2) {
        printf("Error: invalid no of arguments\n");
        printf("Syntax: ./uaputl hostcmd <hostcmd.conf> <cmdname>\n");
        return UAP_FAILURE;
    }

    buffer = (t_u8 *) malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
    if (!buffer) {
        printf("ERR:Cannot allocate buffer for command!\n");
        return UAP_FAILURE;
    }

    memset(buffer, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
    sprintf(cmdname, "%s", argv[2]);
    ret = prepare_host_cmd_buffer(argv[1], cmdname, buffer);
    if (ret == UAP_FAILURE)
        goto _exit_;

    /* Locate headers */
    hdr = (apcmdbuf *) buffer;
    cmd_len = hdr->size + BUF_HEADER_SIZE;

    /* Send the command */
    ret = uap_ioctl((t_u8 *) buffer, &cmd_len, MRVDRV_SIZE_OF_CMD_BUFFER);

    /* Process response */
    if (ret == UAP_SUCCESS) {
        hdr->cmd_code &= HostCmd_CMD_ID_MASK;
        if (!hdr->result) {
            printf
                ("UAPHOSTCMD: CmdCode=%#04x, Size=%#04x, SeqNum=%#04x, Result=%#04x\n",
                 hdr->cmd_code, hdr->size, hdr->seq_num, hdr->result);
            hexdump_data("payload", (void *) (buffer + APCMDHEADERLEN),
                         hdr->size - (APCMDHEADERLEN - BUF_HEADER_SIZE), ' ');
        } else
            printf
                ("UAPHOSTCMD failed: CmdCode=%#04x, Size=%#04x, SeqNum=%#04x, Result=%#04x\n",
                 hdr->cmd_code, hdr->size, hdr->seq_num, hdr->result);
    } else
        printf("ERR:Command sending failed!\n");

  _exit_:
    if (buffer)
        free(buffer);
    return ret;
}

/**
 *  @brief Show usage information for addbapara command
 *
 *  $return         N/A
 */
void
print_addbapara_usage(void)
{
    printf("\nUsage : addbapara [timeout txwinsize rxwinsize]");
    printf("\nOptions: timeout : 0 - Disable");
    printf("\n                   1 - 65535 : Block Ack Timeout in TU");
    printf("\n         txwinsize: Buffer size for ADDBA request");
    printf("\n         rxwinsize: Buffer size for ADDBA response");
    printf("\n         empty - get ADDBA parameters\n");
    return;
}

/** 
 *  @brief Creates addbaparam request and send to driver
 *
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_addbapara(int argc, char *argv[])
{
    int opt;
    addba_param param;
    struct ifreq ifr;
    t_s32 sockfd;
    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_addbapara_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    memset(&param, 0, sizeof(param));
    /* Check arguments */
    if ((argc != 0) && (argc != 3)) {
        printf("ERR:wrong arguments. Only support 0 or 3 arguments\n");
        print_addbapara_usage();
        return UAP_FAILURE;
    }
    param.subcmd = UAP_ADDBA_PARA;
    if (argc) {
        if ((IS_HEX_OR_DIGIT(argv[0]) == UAP_FAILURE) ||
            (IS_HEX_OR_DIGIT(argv[1]) == UAP_FAILURE) ||
            (IS_HEX_OR_DIGIT(argv[2]) == UAP_FAILURE)) {
            printf("ERR: Only Number values are allowed\n");
            print_addbapara_usage();
            return UAP_FAILURE;
        }
        param.action = 1;
        param.timeout = (t_u32) A2HEXDECIMAL(argv[0]);
        if (param.timeout > DEFAULT_BLOCK_ACK_TIMEOUT) {
            printf("ERR: Block Ack timeout should be in range [1-65535]\n");
            print_addbapara_usage();
            return UAP_FAILURE;
        }
        param.txwinsize = (t_u32) A2HEXDECIMAL(argv[1]);
        param.rxwinsize = (t_u32) A2HEXDECIMAL(argv[2]);
        if (param.txwinsize > MAX_TXRX_WINDOW_SIZE ||
            param.rxwinsize > MAX_TXRX_WINDOW_SIZE) {
            printf("ERR: Tx/Rx window size should not be greater than 1023\n");
            print_addbapara_usage();
            return UAP_FAILURE;
        }
    } else {
        param.action = 0;
    }
    /* Open socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("ERR:Cannot open socket\n");
        return UAP_FAILURE;
    }
    /* Initialize the ifr structure */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_ifrn.ifrn_name, dev_name, strlen(dev_name));
    ifr.ifr_ifru.ifru_data = (void *) &param;
    /* Perform ioctl */
    errno = 0;
    if (ioctl(sockfd, UAP_IOCTL_CMD, &ifr)) {
        perror("");
        printf("ERR:ADDBA PARA failed\n");
        close(sockfd);
        return UAP_FAILURE;
    }
    if (!argc) {
        printf("ADDBA parameters:\n");
        printf("\ttimeout=%d\n", (int) param.timeout);
        printf("\ttxwinsize=%d\n", (int) param.txwinsize);
        printf("\trxwinsize=%d\n", (int) param.rxwinsize);
    }
    /* Close socket */
    close(sockfd);
    return UAP_SUCCESS;
}

/**
 *  @brief Show usage information for aggrpriotbl command
 *
 *  $return         N/A
 */
void
print_aggrpriotbl_usage(void)
{
    printf("\nUsage : aggrpriotbl <m0> <n0> <m1> <n1> ... <m7> <n7>");
    printf("\nOptions: <mx> : 0 - 7, 0xff to disable AMPDU aggregation.");
    printf("\n         <nx> : 0 - 7, 0xff to disable AMSDU aggregation.");
    printf("\n         empty - get the priority table\n");
    return;
}

/** 
 *  @brief Creates aggrpriotbl request and send to driver
 *
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_aggrpriotbl(int argc, char *argv[])
{
    int opt;
    aggr_prio_tbl prio_tbl;
    struct ifreq ifr;
    t_s32 sockfd;
    t_u8 value;
    int i;
    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_aggrpriotbl_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    memset(&prio_tbl, 0, sizeof(prio_tbl));
    /* Check arguments */
    if ((argc != 0) && (argc != 16)) {
        printf("ERR:wrong arguments. Only support 0 or 16 arguments\n");
        print_aggrpriotbl_usage();
        return UAP_FAILURE;
    }
    prio_tbl.subcmd = UAP_AGGR_PRIOTBL;
    if (argc) {
        for (i = 0; i < argc; i++) {
            if ((IS_HEX_OR_DIGIT(argv[i]) == UAP_FAILURE)) {
                printf("ERR: Only Number values are allowed\n");
                print_aggrpriotbl_usage();
                return UAP_FAILURE;
            }
            value = (t_u8) A2HEXDECIMAL(argv[i]);
            if ((value > 7) && (value != 0xff)) {
                printf
                    ("ERR: Invalid priority, Valid value 0-7, 0xff to disable aggregation.\n");
                print_aggrpriotbl_usage();
                return UAP_FAILURE;
            }
        }
        prio_tbl.action = 1;
        for (i = 0; i < MAX_NUM_TID; i++) {
            prio_tbl.ampdu[i] = (t_u8) A2HEXDECIMAL(argv[i * 2]);
            prio_tbl.amsdu[i] = (t_u8) A2HEXDECIMAL(argv[i * 2 + 1]);
        }
    } else {
        prio_tbl.action = 0;
    }
    /* Open socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("ERR:Cannot open socket\n");
        return UAP_FAILURE;
    }
    /* Initialize the ifr structure */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_ifrn.ifrn_name, dev_name, strlen(dev_name));
    ifr.ifr_ifru.ifru_data = (void *) &prio_tbl;
    /* Perform ioctl */
    errno = 0;
    if (ioctl(sockfd, UAP_IOCTL_CMD, &ifr)) {
        perror("");
        printf("ERR: priority table failed\n");
        close(sockfd);
        return UAP_FAILURE;
    }
    if (!argc) {
        printf("AMPDU/AMSDU priority table:");
        for (i = 0; i < MAX_NUM_TID; i++) {
            printf(" %d %d", prio_tbl.ampdu[i], prio_tbl.amsdu[i]);
        }
        printf("\n");
    }
    /* Close socket */
    close(sockfd);
    return UAP_SUCCESS;
}

/**
 *  @brief Show usage information for addbareject command
 *
 *  $return         N/A
 */
void
print_addba_reject_usage(void)
{
    printf("\nUsage : addbareject <m0> <m1> ... <m7>");
    printf("\nOptions: <mx> : 1 enables rejection of ADDBA request for TidX.");
    printf("\n                0 would accept any ADDBAs for TidX.");
    printf("\n         empty - get the addbareject table\n");
    return;
}

/** 
 *  @brief Creates addbareject request and send to driver
 *
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_addbareject(int argc, char *argv[])
{
    int opt;
    addba_reject_para param;
    struct ifreq ifr;
    t_s32 sockfd;
    int i;
    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_addba_reject_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    memset(&param, 0, sizeof(param));
    /* Check arguments */
    if ((argc != 0) && (argc != 8)) {
        printf("ERR:wrong arguments. Only support 0 or 8 arguments\n");
        print_addba_reject_usage();
        return UAP_FAILURE;
    }
    param.subcmd = UAP_ADDBA_REJECT;
    if (argc) {
        for (i = 0; i < argc; i++) {
            if ((ISDIGIT(argv[i]) == UAP_FAILURE) || (atoi(argv[i]) < 0) ||
                (atoi(argv[i]) > 1)) {
                printf("ERR: Only allow 0 or 1\n");
                print_addba_reject_usage();
                return UAP_FAILURE;
            }
        }
        param.action = 1;
        for (i = 0; i < MAX_NUM_TID; i++) {
            param.addba_reject[i] = (t_u8) atoi(argv[i]);
        }
    } else {
        param.action = 0;
    }
    /* Open socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("ERR:Cannot open socket\n");
        return UAP_FAILURE;
    }
    /* Initialize the ifr structure */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_ifrn.ifrn_name, dev_name, strlen(dev_name));
    ifr.ifr_ifru.ifru_data = (void *) &param;
    /* Perform ioctl */
    errno = 0;
    if (ioctl(sockfd, UAP_IOCTL_CMD, &ifr)) {
        perror("");
        printf("ERR: addba reject table failed\n");
        close(sockfd);
        return UAP_FAILURE;
    }
    if (!argc) {
        printf("addba reject table: ");
        for (i = 0; i < MAX_NUM_TID; i++) {
            printf("%d ", param.addba_reject[i]);
        }
        printf("\n");
    }
    /* Close socket */
    close(sockfd);
    return UAP_SUCCESS;
}

/**
 *  @brief Show usage information for hscfg command
 *
 *  $return         N/A
 */
void
print_hscfg_usage(void)
{
    printf("\nUsage : hscfg [condition [[GPIO# [gap]]]]");
    printf("\nOptions: condition : bit 0 = 1   -- broadcast data");
    printf("\n                     bit 1 = 1   -- unicast data");
    printf("\n                     bit 2 = 1   -- mac event");
    printf("\n                     bit 3 = 1   -- multicast packet");
    printf
        ("\n         GPIO: the pin number (e.g. 0-7) of GPIO used to wakeup the host");
    printf
        ("\n               or 0xff interface (e.g. SDIO) used to wakeup the host");
    printf
        ("\n         gap: time between wakeup signal and wakeup event (in milliseconds)");
    printf
        ("\n              or 0xff for special setting when GPIO is used to wakeup host");
    printf("\n         empty - get current host sleep parameters\n");
    return;
}

/** 
 *  @brief Creates host sleep parameter request and send to driver
 *
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_hscfg(int argc, char *argv[])
{
    int opt;
    ds_hs_cfg hscfg;
    struct ifreq ifr;
    t_s32 sockfd;

    if ((argc == 2) && strstr(argv[1], "-1"))
        strcpy(argv[1], "0xffff");

    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_hscfg_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    memset(&hscfg, 0, sizeof(hscfg));
    /* Check arguments */
    if (argc > 3) {
        printf("ERR:wrong arguments.\n");
        print_hscfg_usage();
        return UAP_FAILURE;
    }

    if (argc) {
        hscfg.flags |= HS_CFG_FLAG_SET | HS_CFG_FLAG_CONDITION;
        hscfg.conditions = (t_u32) A2HEXDECIMAL(argv[0]);
        if (hscfg.conditions >= 0xffff)
            hscfg.conditions = HS_CFG_CANCEL;
        if ((hscfg.conditions != HS_CFG_CANCEL) &&
            (hscfg.conditions & ~HS_CFG_CONDITION_MASK)) {
            printf("ERR:Illegal conditions 0x%lx\n", hscfg.conditions);
            print_hscfg_usage();
            return UAP_FAILURE;
        }
        if (argc > 1) {
            hscfg.flags |= HS_CFG_FLAG_GPIO;
            hscfg.gpio = (t_u32) A2HEXDECIMAL(argv[1]);
            if (hscfg.gpio > 255) {
                printf("ERR:Illegal gpio 0x%lx\n", hscfg.gpio);
                print_hscfg_usage();
                return UAP_FAILURE;
            }
        }
        if (argc > 2) {
            hscfg.flags |= HS_CFG_FLAG_GAP;
            hscfg.gap = (t_u32) A2HEXDECIMAL(argv[2]);
        }
    } else {
        hscfg.flags = HS_CFG_FLAG_GET;
    }
    /* Open socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("ERR:Cannot open socket\n");
        return UAP_FAILURE;
    }
    /* Initialize the ifr structure */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_ifrn.ifrn_name, dev_name, strlen(dev_name));
    ifr.ifr_ifru.ifru_data = (void *) &hscfg;
    /* Perform ioctl */
    errno = 0;
    if (ioctl(sockfd, UAP_HS_CFG, &ifr)) {
        perror("");
        printf("ERR:UAP_HS_CFG failed\n");
        close(sockfd);
        return UAP_FAILURE;
    }
    if (!argc) {
        printf("Host sleep parameters:\n");
        printf("\tconditions=%d\n", (int) hscfg.conditions);
        printf("\tGPIO=%d\n", (int) hscfg.gpio);
        printf("\tgap=%d\n", (int) hscfg.gap);
    }
    /* Close socket */
    close(sockfd);
    return UAP_SUCCESS;
}

/**
 *  @brief Show usage information for the powermode command
 *
 *  $return         N/A
 */
void
print_power_mode_usage(void)
{
    printf
        ("\nUsage : powermode [MODE] [SLEEP_PARAM=1 CTRL MIN_SLEEP MAX_SLEEP]");
    printf("\n                  [INACT_PARAM=2 INACTTO MIN_AWAKE MAX_AWAKE]");
    printf("\nOptions: MODE :     0 - disable power mode");
    printf("\n                    1 - periodic DTIM power save mode");
    printf("\n                    2 - inactivity based power save mode");
    printf("\n   SLEEP_PARAM:");
    printf("\n        CTRL:  0 - disable protection frame Tx before PS");
    printf("\n               1 - enable protection frame Tx before PS");
    printf("\n        MIN_SLEEP: Minimum sleep duration in microseconds");
    printf("\n        MAX_SLEEP: Maximum sleep duration in miroseconds");
    printf("\n   INACT_PARAM: (only for inactivity based power save mode)");
    printf("\n          INACTTO: Inactivity timeout in miroseconds");
    printf("\n        MIN_AWAKE: Minimum awake duration in microseconds");
    printf("\n        MAX_AWAKE: Maximum awake duration in microseconds");
    printf("\n          empty - get current power mode\n");
    return;
}

/** 
 *  @brief Set/get power mode 
 *
 *  @param pm      A pointer to ps_mgmt structure
 *  @param flag    flag for query 
 *  @return        UAP_SUCCESS/UAP_FAILURE
 */
int
send_power_mode_ioctl(ps_mgmt * pm, int flag)
{
    struct ifreq ifr;
    t_s32 sockfd;
    t_u32 result = 0;
    /* Open socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("ERR:Cannot open socket\n");
        return UAP_FAILURE;
    }
    /* Initialize the ifr structure */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_ifrn.ifrn_name, dev_name, strlen(dev_name));
    ifr.ifr_ifru.ifru_data = (void *) pm;
    /* Perform ioctl */
    errno = 0;
    if (ioctl(sockfd, UAP_POWER_MODE, &ifr)) {
        memcpy((void *) &result, (void *) pm, sizeof(result));
        if (result == 1) {
            printf("ERR:Power mode needs to be disabled before modifying it\n");
        } else {
            perror("");
            printf("ERR:UAP_POWER_MODE is not supported by %s\n", dev_name);
        }
        close(sockfd);
        return UAP_FAILURE;
    }
    if (flag) {
        /* Close socket */
        close(sockfd);
        return UAP_SUCCESS;
    }
    switch (pm->ps_mode) {
    case 0:
        printf("power mode = Disabled\n");
        break;
    case 1:
        printf("power mode = Periodic DTIM PS\n");
        break;
    case 2:
        printf("power mode = Inactivity based PS \n");
        break;
    }
    if (pm->flags & PS_FLAG_SLEEP_PARAM) {
        printf("Sleep param:\n");
        printf("\tctrl_bitmap=%d\n", (int) pm->sleep_param.ctrl_bitmap);
        printf("\tmin_sleep=%d us\n", (int) pm->sleep_param.min_sleep);
        printf("\tmax_sleep=%d us\n", (int) pm->sleep_param.max_sleep);
    }
    if (pm->flags & PS_FLAG_INACT_SLEEP_PARAM) {
        printf("Inactivity sleep param:\n");
        printf("\tinactivity_to=%d us\n", (int) pm->inact_param.inactivity_to);
        printf("\tmin_awake=%d us\n", (int) pm->inact_param.min_awake);
        printf("\tmax_awake=%d us\n", (int) pm->inact_param.max_awake);
    }
    /* Close socket */
    close(sockfd);
    return UAP_SUCCESS;
}

/** 
 *  @brief Creates power mode request and send to driver
 *   and sends to the driver
 *
 *   Usage: "Usage : powermode [MODE]"
 *
 *   Options: MODE :     0 - disable power mode
 *                       1 - enable power mode
 *            		 empty - get current power mode                         
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_power_mode(int argc, char *argv[])
{
    int opt;
    ps_mgmt pm;
    int type = 0;
    int ret = UAP_SUCCESS;
    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_power_mode_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    memset(&pm, 0, sizeof(ps_mgmt));
    /* Check arguments */
    if ((argc > 9) ||
        ((argc != 0) && (argc != 1) && (argc != 5) && (argc != 9))) {
        printf("ERR:wrong arguments.\n");
        print_power_mode_usage();
        return UAP_FAILURE;
    }

    if (argc) {
        if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
            (atoi(argv[0]) > 2)) {
            printf
                ("ERR:Illegal power mode %s. Must be either '0' '1' or '2'.\n",
                 argv[0]);
            print_power_mode_usage();
            return UAP_FAILURE;
        }
        pm.flags = PS_FLAG_PS_MODE;
        pm.ps_mode = atoi(argv[0]);
        if ((pm.ps_mode == PS_MODE_DISABLE) && (argc > 1)) {
            printf("ERR: Illegal parameter for disable power mode\n");
            print_power_mode_usage();
            return UAP_FAILURE;
        }
        if (argc >= 5) {
            if ((ISDIGIT(argv[1]) == 0) || (atoi(argv[1]) < 1) ||
                (atoi(argv[1]) > 2)) {
                printf
                    ("ERR:Illegal parameter type %s. Must be either '1' or '2'.\n",
                     argv[1]);
                print_power_mode_usage();
                return UAP_FAILURE;
            }
            type = atoi(argv[1]);
            if (type == SLEEP_PARAMETER) {
                if ((ISDIGIT(argv[2]) == 0) || (atoi(argv[2]) < 0) ||
                    (atoi(argv[2]) > 1)) {
                    printf
                        ("ERR:Illegal ctrl bitmap = %s. Must be either '0' or '1'.\n",
                         argv[2]);
                    print_power_mode_usage();
                    return UAP_FAILURE;
                }
                pm.flags |= PS_FLAG_SLEEP_PARAM;
                pm.sleep_param.ctrl_bitmap = atoi(argv[2]);
                if ((ISDIGIT(argv[3]) == 0) || (ISDIGIT(argv[4]) == 0)) {
                    printf("ERR:Illegal parameter\n");
                    print_power_mode_usage();
                    return UAP_FAILURE;
                }
                pm.sleep_param.min_sleep = atoi(argv[3]);
                pm.sleep_param.max_sleep = atoi(argv[4]);
                if (pm.sleep_param.min_sleep > pm.sleep_param.max_sleep) {
                    printf
                        ("ERR: MIN_SLEEP value should be less than or equal to MAX_SLEEP\n");
                    return UAP_FAILURE;
                }
                if (pm.sleep_param.min_sleep < PS_SLEEP_PARAM_MIN ||
                    ((pm.sleep_param.max_sleep > PS_SLEEP_PARAM_MAX) &&
                     pm.sleep_param.ctrl_bitmap)) {
                    printf
                        ("ERR: Incorrect value of sleep period. Please check README\n");
                    return UAP_FAILURE;
                }
            } else {
                if ((ISDIGIT(argv[2]) == 0) || (ISDIGIT(argv[3]) == 0) ||
                    (ISDIGIT(argv[4]) == 0)) {
                    printf("ERR:Illegal parameter\n");
                    print_power_mode_usage();
                    return UAP_FAILURE;
                }
                pm.flags |= PS_FLAG_INACT_SLEEP_PARAM;
                pm.inact_param.inactivity_to = atoi(argv[2]);
                pm.inact_param.min_awake = atoi(argv[3]);
                pm.inact_param.max_awake = atoi(argv[4]);
                if (pm.inact_param.min_awake > pm.inact_param.max_awake) {
                    printf
                        ("ERR: MIN_AWAKE value should be less than or equal to MAX_AWAKE\n");
                    return UAP_FAILURE;
                }
                if (pm.inact_param.min_awake < PS_AWAKE_PERIOD_MIN) {
                    printf("ERR: Incorrect value of MIN_AWAKE period.\n");
                    return UAP_FAILURE;
                }
            }
        }
        if (argc == 9) {
            if ((ISDIGIT(argv[5]) == 0) || (atoi(argv[5]) < 1) ||
                (atoi(argv[5]) > 2)) {
                printf
                    ("ERR:Illegal parameter type %s. Must be either '1' or '2'.\n",
                     argv[5]);
                print_power_mode_usage();
                return UAP_FAILURE;
            }
            if (type == atoi(argv[5])) {
                printf("ERR: Duplicate parameter type %s.\n", argv[5]);
                print_power_mode_usage();
                return UAP_FAILURE;
            }
            type = atoi(argv[5]);
            if (type == SLEEP_PARAMETER) {
                if ((ISDIGIT(argv[6]) == 0) || (atoi(argv[6]) < 0) ||
                    (atoi(argv[6]) > 1)) {
                    printf
                        ("ERR:Illegal ctrl bitmap = %s. Must be either '0' or '1'.\n",
                         argv[6]);
                    print_power_mode_usage();
                    return UAP_FAILURE;
                }
                pm.flags |= PS_FLAG_SLEEP_PARAM;
                pm.sleep_param.ctrl_bitmap = atoi(argv[6]);
                if ((ISDIGIT(argv[7]) == 0) || (ISDIGIT(argv[8]) == 0)) {
                    printf("ERR:Illegal parameter\n");
                    print_power_mode_usage();
                    return UAP_FAILURE;
                }
                pm.sleep_param.min_sleep = atoi(argv[7]);
                pm.sleep_param.max_sleep = atoi(argv[8]);
                if (pm.sleep_param.min_sleep > pm.sleep_param.max_sleep) {
                    printf
                        ("ERR: MIN_SLEEP value should be less than or equal to MAX_SLEEP\n");
                    return UAP_FAILURE;
                }
                if (pm.sleep_param.min_sleep < PS_SLEEP_PARAM_MIN ||
                    ((pm.sleep_param.max_sleep > PS_SLEEP_PARAM_MAX) &&
                     pm.sleep_param.ctrl_bitmap)) {
                    printf
                        ("ERR: Incorrect value of sleep period. Please check README\n");
                    return UAP_FAILURE;
                }
            } else {
                if ((ISDIGIT(argv[6]) == 0) || (ISDIGIT(argv[7]) == 0) ||
                    (ISDIGIT(argv[8]) == 0)) {
                    printf("ERR:Illegal parameter\n");
                    print_power_mode_usage();
                    return UAP_FAILURE;
                }
                pm.flags |= PS_FLAG_INACT_SLEEP_PARAM;
                pm.inact_param.inactivity_to = atoi(argv[6]);
                pm.inact_param.min_awake = atoi(argv[7]);
                pm.inact_param.max_awake = atoi(argv[8]);
                if (pm.inact_param.min_awake > pm.inact_param.max_awake) {
                    printf
                        ("ERR: MIN_AWAKE value should be less than or equal to MAX_AWAKE\n");
                    return UAP_FAILURE;
                }
                if (pm.inact_param.min_awake < PS_AWAKE_PERIOD_MIN) {
                    printf("ERR: Incorrect value of MIN_AWAKE period.\n");
                    return UAP_FAILURE;
                }
            }
        }
    }
    ret = send_power_mode_ioctl(&pm, 0);
    return ret;
}

/**
 *  @brief Show usage information for the powermode command
 *
 *  $return         N/A
 */
void
print_pscfg_usage(void)
{
    printf
        ("\nUsage : pscfg [MODE] [CTRL INACTTO MIN_SLEEP MAX_SLEEP MIN_AWAKE MAX_AWAKE]");
    printf("\nOptions: MODE :     0 - disable power mode");
    printf("\n                    1 - periodic DTIM power save mode");
    printf("\n                    2 - inactivity based power save mode");
    printf("\n   PS PARAMS:");
    printf("\n        CTRL:  0 - disable protection frame Tx before PS");
    printf("\n               1 - enable protection frame Tx before PS");
    printf("\n        INACTTO: Inactivity timeout in miroseconds");
    printf("\n        MIN_SLEEP: Minimum sleep duration in microseconds");
    printf("\n        MAX_SLEEP: Maximum sleep duration in miroseconds");
    printf("\n        MIN_AWAKE: Minimum awake duration in microseconds");
    printf("\n        MAX_AWAKE: Maximum awake duration in microseconds");
    printf
        ("\n        MIN_AWAKE,MAX_AWAKE only valid for inactivity based power save mode");
    printf("\n        empty - get current power mode\n");
    return;
}

/** 
 *  @brief Creates power mode request and send to driver
 *   and sends to the driver
 *
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_pscfg(int argc, char *argv[])
{
    int opt;
    ps_mgmt pm;
    int ret = UAP_SUCCESS;
    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_pscfg_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    memset(&pm, 0, sizeof(ps_mgmt));
    /* Check arguments */
    if ((argc > 7) ||
        ((argc != 0) && (argc != 1) && (argc != 5) && (argc != 7))) {
        printf("ERR:wrong arguments.\n");
        print_pscfg_usage();
        return UAP_FAILURE;
    }

    if (argc) {
        if (send_power_mode_ioctl(&pm, 1) == UAP_FAILURE)
            return UAP_FAILURE;
        if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
            (atoi(argv[0]) > 2)) {
            printf
                ("ERR:Illegal power mode %s. Must be either '0' '1' or '2'.\n",
                 argv[0]);
            print_pscfg_usage();
            return UAP_FAILURE;
        }
        pm.flags = PS_FLAG_PS_MODE;
        pm.ps_mode = atoi(argv[0]);
        if ((pm.ps_mode == PS_MODE_DISABLE) && (argc > 1)) {
            printf("ERR: Illegal parameter for disable power mode\n");
            print_pscfg_usage();
            return UAP_FAILURE;
        }
        if ((pm.ps_mode != PS_MODE_INACTIVITY) && (argc > 5)) {
            printf
                ("ERR:Min awake period and Max awake period are valid only for inactivity based power save mode\n");
            print_pscfg_usage();
            return UAP_FAILURE;
        }
        if (argc >= 5) {
            if ((ISDIGIT(argv[1]) == 0) || (atoi(argv[1]) < 0) ||
                (atoi(argv[1]) > 1)) {
                printf
                    ("ERR:Illegal ctrl bitmap = %s. Must be either '0' or '1'.\n",
                     argv[1]);
                print_pscfg_usage();
                return UAP_FAILURE;
            }
            pm.flags |= PS_FLAG_SLEEP_PARAM | PS_FLAG_INACT_SLEEP_PARAM;
            pm.sleep_param.ctrl_bitmap = atoi(argv[1]);
            if ((ISDIGIT(argv[2]) == 0) || (ISDIGIT(argv[3]) == 0) ||
                (ISDIGIT(argv[4]) == 0)) {
                printf("ERR:Illegal parameter\n");
                print_pscfg_usage();
                return UAP_FAILURE;
            }
            pm.inact_param.inactivity_to = atoi(argv[2]);
            pm.sleep_param.min_sleep = atoi(argv[3]);
            pm.sleep_param.max_sleep = atoi(argv[4]);
            if (pm.sleep_param.min_sleep > pm.sleep_param.max_sleep) {
                printf
                    ("ERR: MIN_SLEEP value should be less than or equal to MAX_SLEEP\n");
                return UAP_FAILURE;
            }
            if (pm.sleep_param.min_sleep < PS_SLEEP_PARAM_MIN ||
                ((pm.sleep_param.max_sleep > PS_SLEEP_PARAM_MAX) &&
                 pm.sleep_param.ctrl_bitmap)) {
                printf
                    ("ERR: Incorrect value of sleep period. Please check README\n");
                return UAP_FAILURE;
            }
            if (argc == 7) {
                if ((ISDIGIT(argv[5]) == 0) || (ISDIGIT(argv[6]) == 0)) {
                    printf("ERR:Illegal parameter\n");
                    print_pscfg_usage();
                    return UAP_FAILURE;
                }
                pm.inact_param.min_awake = atoi(argv[5]);
                pm.inact_param.max_awake = atoi(argv[6]);
                if (pm.inact_param.min_awake > pm.inact_param.max_awake) {
                    printf
                        ("ERR: MIN_AWAKE value should be less than or equal to MAX_AWAKE\n");
                    return UAP_FAILURE;
                }
                if (pm.inact_param.min_awake < PS_AWAKE_PERIOD_MIN) {
                    printf("ERR: Incorrect value of MIN_AWAKE period.\n");
                    return UAP_FAILURE;
                }
            }
        }
    }
    ret = send_power_mode_ioctl(&pm, 0);
    return ret;
}

/** 
 *  @brief start/stop/reset bss 
 *
 *  @param mode     bss control mode     
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
send_bss_ctl_ioctl(int mode)
{
    struct ifreq ifr;
    t_s32 sockfd;
    t_u32 data = (t_u32) mode;
    /* Open socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("ERR:Cannot open socket\n");
        return UAP_FAILURE;
    }
    /* Initialize the ifr structure */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_ifrn.ifrn_name, dev_name, strlen(dev_name));
    ifr.ifr_ifru.ifru_data = (void *) &data;
    /* Perform ioctl */
    errno = 0;
    if (ioctl(sockfd, UAP_BSS_CTRL, &ifr)) {
        printf("ERR:UAP_BSS_CTRL fail, result=%d\n", (int) data);
        switch (mode) {
        case UAP_BSS_START:
            if (data == BSS_FAILURE_START_INVAL)
                printf("ERR:Could not start BSS! Invalid BSS parameters.\n");
            else
                printf("ERR:Could not start BSS!\n");
            break;
        case UAP_BSS_STOP:
            printf("ERR:Could not stop BSS!\n");
            break;
        case UAP_BSS_RESET:
            printf("ERR:Could not reset system!\n");
            break;
        }
        close(sockfd);
        return UAP_FAILURE;
    }

    switch (mode) {
    case UAP_BSS_START:
        printf("BSS start successful!\n");
        break;
    case UAP_BSS_STOP:
        printf("BSS stop successful!\n");
        break;
    case UAP_BSS_RESET:
        printf("System reset successful!\n");
        break;
    }

    /* Close socket */
    close(sockfd);
    return UAP_SUCCESS;
}

/**
 *  @brief Show usage information for the sys_reset command
 *
 *  $return         N/A
 */
void
print_sys_reset_usage(void)
{
    printf("\nUsage : sys_reset\n");
    return;
}

/** 
 *  @brief Creates a sys_reset request and sends to the driver
 *
 *  Usage: "sys_reset"
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_sys_reset(int argc, char *argv[])
{
    int opt;
    int ret = UAP_SUCCESS;
    ps_mgmt pm;
    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_sys_reset_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    /* Check arguments */
    if (argc != 0) {
        printf("ERR:Too many arguments.\n");
        print_sys_reset_usage();
        return UAP_FAILURE;
    }
    memset(&pm, 0, sizeof(ps_mgmt));
    pm.flags = PS_FLAG_PS_MODE;
    pm.ps_mode = PS_MODE_DISABLE;
    if (send_power_mode_ioctl(&pm, 0) == UAP_FAILURE)
        return UAP_FAILURE;
    ret = send_bss_ctl_ioctl(UAP_BSS_RESET);
    return ret;
}

/**
 *  @brief Show usage information for the bss_start command
 *
 *  $return         N/A
 */
void
print_bss_start_usage(void)
{
    printf("\nUsage : bss_start\n");
    return;
}

/** 
 *  @brief Creates a BSS start request and sends to the driver
 *
 *   Usage: "bss_start"
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_bss_start(int argc, char *argv[])
{
    int opt;
    apcmdbuf_sys_configure *cmd_buf = NULL;
    t_u8 *buf = NULL;
    t_u16 cmd_len;
    t_u16 buf_len;
    int ret = UAP_SUCCESS;

    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_bss_start_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    /* Check arguments */
    if (argc != 0) {
        printf("ERR:Too many arguments.\n");
        print_bss_start_usage();
        return UAP_FAILURE;
    }

    /** Query AP's setting */
    buf_len = MRVDRV_SIZE_OF_CMD_BUFFER;

    /* Alloc buf for command */
    buf = (t_u8 *) malloc(buf_len);

    if (!buf) {
        printf("ERR:Cannot allocate buffer from command!\n");
        return UAP_FAILURE;
    }
    memset(buf, 0, buf_len);

    /* Locate headers */
    cmd_len = sizeof(apcmdbuf_sys_configure);
    cmd_buf = (apcmdbuf_sys_configure *) buf;

    /* Fill the command buffer */
    cmd_buf->cmd_code = APCMD_SYS_CONFIGURE;
    cmd_buf->size = cmd_len - BUF_HEADER_SIZE;
    cmd_buf->seq_num = 0;
    cmd_buf->result = 0;

    /* Send the command */
    ret = uap_ioctl((t_u8 *) cmd_buf, &cmd_len, buf_len);

    /* Process response */
    if (ret == UAP_SUCCESS) {
        /* Verify response */
        if (cmd_buf->cmd_code != (APCMD_SYS_CONFIGURE | APCMD_RESP_CHECK)) {
            printf("ERR:Corrupted response!\n");
            ret = UAP_FAILURE;
            goto done;
        }
        /* Check response */
        if (cmd_buf->result == CMD_SUCCESS) {
            ret = check_sys_config(buf + sizeof(apcmdbuf_sys_configure),
                                   cmd_buf->size -
                                   sizeof(apcmdbuf_sys_configure) +
                                   BUF_HEADER_SIZE);
        } else {
            printf("ERR:Could not retrieve system configure\n");
            ret = UAP_FAILURE;
            goto done;
        }
    } else {
        printf("ERR:Command sending failed!\n");
    }

    if (ret == UAP_FAILURE) {
        printf("ERR: Wrong system configuration!\n");
        goto done;
    }
    ret = send_bss_ctl_ioctl(UAP_BSS_START);

  done:
    if (buf)
        free(buf);
    return ret;
}

/**
 *  @brief Show usage information for the bss_stop command
 *
 *  $return         N/A
 */
void
print_bss_stop_usage(void)
{
    printf("\nUsage : bss_stop\n");
    return;
}

/** 
 *  @brief Creates a BSS stop request and sends to the driver
 *
 *   Usage: "bss_stop"
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_bss_stop(int argc, char *argv[])
{
    int opt;
    int ret = UAP_SUCCESS;

    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_bss_stop_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;             /* Check arguments */

    if (argc != 0) {
        printf("ERR:Too many arguments.\n");
        print_bss_stop_usage();
        return UAP_FAILURE;
    }
    ret = send_bss_ctl_ioctl(UAP_BSS_STOP);
    return ret;
}

/**
 *  @brief Show usage information for the sta_list command
 *
 *  $return         N/A
 */
void
print_sta_list_usage(void)
{
    printf("\nUsage : sta_list\n");
    return;
}

/** 
 *  @brief Creates a STA list request and sends to the driver
 *
 *   Usage: "sta_list"
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_sta_list(int argc, char *argv[])
{
    struct ifreq ifr;
    t_s32 sockfd;
    sta_list list;
    int i = 0;
    int opt;
    int rssi = 0;

    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_sta_list_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    /* Check arguments */
    if (argc != 0) {
        printf("ERR:Too many arguments.\n");
        print_sta_list_usage();
        return UAP_FAILURE;
    }
    memset(&list, 0, sizeof(sta_list));

    /* Open socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("ERR:Cannot open socket\n");
        return UAP_FAILURE;
    }
    /* Initialize the ifr structure */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_ifrn.ifrn_name, dev_name, strlen(dev_name));
    ifr.ifr_ifru.ifru_data = (void *) &list;
    /* Perform ioctl */
    errno = 0;
    if (ioctl(sockfd, UAP_GET_STA_LIST, &ifr)) {
        perror("");
        printf("ERR:UAP_GET_STA_LIST is not supported by %s\n", dev_name);
        close(sockfd);
        return UAP_FAILURE;
    }
    printf("Number of STA = %d\n\n", list.sta_count);

    for (i = 0; i < list.sta_count; i++) {
        printf("STA %d information:\n", i + 1);
        printf("=====================\n");
        printf("MAC Address: ");
        print_mac(list.info[i].mac_address);
        printf("\nPower mfg status: %s\n",
               (list.info[i].power_mfg_status == 0) ? "active" : "power save");

        /** On some platform, s8 is same as unsigned char*/
        rssi = (int) list.info[i].rssi;
        if (rssi > 0x7f)
            rssi = -(256 - rssi);
        printf("Rssi : %d dBm\n\n", rssi);
    }
    /* Close socket */
    close(sockfd);
    return UAP_SUCCESS;
}

/**
 *  @brief Show usage information for the sta_deauth command
 *
 *  $return         N/A
 */
void
print_sta_deauth_usage(void)
{
    printf("\nUsage : sta_deauth <STA_MAC_ADDRESS>\n");
    return;
}

/** 
 *  @brief Creates a STA deauth request and sends to the driver
 *
 *   Usage: "sta_deauth <STA_MAC_ADDRESS>"
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_sta_deauth(int argc, char *argv[])
{
    APCMDBUF_STA_DEAUTH *cmd_buf = NULL;
    t_u8 *buffer = NULL;
    t_u16 cmd_len;
    int ret = UAP_SUCCESS;
    int opt;

    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_sta_deauth_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    /* Check arguments */
    if (argc != 1) {
        printf("ERR:wrong arguments! Must provide STA_MAC_ADDRESS.\n");
        print_sta_deauth_usage();
        return UAP_FAILURE;
    }

    /* Initialize the command length */
    cmd_len = sizeof(APCMDBUF_STA_DEAUTH);

    /* Initialize the command buffer */
    buffer = (t_u8 *) malloc(cmd_len);
    if (!buffer) {
        printf("ERR:Cannot allocate buffer for command!\n");
        return UAP_FAILURE;
    }
    memset(buffer, 0, cmd_len);

    /* Locate headers */
    cmd_buf = (APCMDBUF_STA_DEAUTH *) buffer;

    /* Fill the command buffer */
    cmd_buf->cmd_code = APCMD_STA_DEAUTH;
    cmd_buf->size = cmd_len - BUF_HEADER_SIZE;
    cmd_buf->seq_num = 0;
    cmd_buf->result = 0;
    if ((ret = mac2raw(argv[0], cmd_buf->sta_mac_address)) != UAP_SUCCESS) {
        printf("ERR: %s Address\n", ret == UAP_FAILURE ? "Invalid MAC" :
               ret == UAP_RET_MAC_BROADCAST ? "Broadcast" : "Multicast");
        free(buffer);
        return UAP_FAILURE;
    }

    /* Send the command */
    ret = uap_ioctl((t_u8 *) cmd_buf, &cmd_len, cmd_len);

    /* Process response */
    if (ret == UAP_SUCCESS) {
        /* Verify response */
        if (cmd_buf->cmd_code != (APCMD_STA_DEAUTH | APCMD_RESP_CHECK)) {
            printf("ERR:Corrupted response!\n");
            free(buffer);
            return UAP_FAILURE;
        }

        /* Print response */
        if (cmd_buf->result == CMD_SUCCESS) {
            printf("Deauthentication successful!\n");
        } else {
            printf("ERR:Deauthentication unsuccessful!\n");
            ret = UAP_FAILURE;
        }
    } else {
        printf("ERR:Command sending failed!\n");
    }
    if (buffer)
        free(buffer);
    return ret;
}

/**
 *  @brief Show usage information for the sta_deauth_ext command
 *
 *  $return         N/A
 */
void
print_sta_deauth_ext_usage(void)
{
    printf("\nUsage : sta_deauth <STA_MAC_ADDRESS> <REASCON_CODE>\n");
    return;
}

/** 
 *  @brief Creates a STA deauth request and sends to the driver
 *
 *   Usage: "sta_deauth <STA_MAC_ADDRESS><REASON_CODE>"
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_sta_deauth_ext(int argc, char *argv[])
{
    int ret = UAP_SUCCESS;
    int opt;
    deauth_param param;
    struct ifreq ifr;
    t_s32 sockfd;
    t_u32 result = 0;

    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_sta_deauth_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    /* Check arguments */
    if (argc != 2) {
        printf
            ("ERR:wrong arguments! Must provide STA_MAC_ADDRESS, REASON_CODE.\n");
        print_sta_deauth_ext_usage();
        return UAP_FAILURE;
    }
    memset(&param, 0, sizeof(deauth_param));

    if ((ret = mac2raw(argv[0], param.mac_addr)) != UAP_SUCCESS) {
        printf("ERR: %s Address\n", ret == UAP_FAILURE ? "Invalid MAC" :
               ret == UAP_RET_MAC_BROADCAST ? "Broadcast" : "Multicast");
        return UAP_FAILURE;
    }

    if ((IS_HEX_OR_DIGIT(argv[1]) == UAP_FAILURE) ||
        (atoi(argv[1]) > MAX_DEAUTH_REASON_CODE)) {
        printf("ERR: Invalid input for reason code\n");
        return UAP_FAILURE;
    }
    param.reason_code = (t_u16) A2HEXDECIMAL(argv[1]);
    /* Open socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("ERR:Cannot open socket\n");
        return UAP_FAILURE;
    }
    /* Initialize the ifr structure */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_ifrn.ifrn_name, dev_name, strlen(dev_name));
    ifr.ifr_ifru.ifru_data = (void *) &param;
    /* Perform ioctl */
    errno = 0;
    if (ioctl(sockfd, UAP_STA_DEAUTH, &ifr)) {
        memcpy((void *) &result, (void *) &param, sizeof(result));
        if (result == 1)
            printf("ERR:UAP_STA_DEAUTH fail\n");
        else
            perror("");
        close(sockfd);
        return UAP_FAILURE;
    }
    printf("Station deauth successful\n");
    /* Close socket */
    close(sockfd);
    return UAP_SUCCESS;
}

/**
 *  @brief Show usage information for the sys_config command
 *
 *  $return         N/A
 */
void
print_sys_config_usage(void)
{
    printf("\nUsage : sys_config [CONFIG_FILE]\n");
    printf
        ("\nIf CONFIG_FILE is provided, a 'set' is performed, else a 'get' is performed.\n");
    printf("CONFIG_FILE is file contain all the Micro AP settings.\n");
    return;
}

/**
 *  @brief Show usage information for the rdeeprom command
 *
 *  $return         N/A
 */
void
print_apcmd_read_eeprom_usage(void)
{
    printf("\nUsage: rdeeprom <offset> <bytecount>\n");
    printf("    offset    : 0,4,8,..., multiple of 4\n");
    printf("    bytecount : 4-20, multiple of 4\n");
    return;
}

/**
 *  @brief Show protocol tlv 
 *
 *  @param protocol Protocol number
 *  
 *  $return         N/A
 */
void
print_protocol(t_u16 protocol)
{
    switch (protocol) {
    case 0:
    case PROTOCOL_NO_SECURITY:
        printf("PROTOCOL = No security\n");
        break;
    case PROTOCOL_STATIC_WEP:
        printf("PROTOCOL = Static WEP\n");
        break;
    case PROTOCOL_WPA:
        printf("PROTOCOL = WPA \n");
        break;
    case PROTOCOL_WPA2:
        printf("PROTOCOL = WPA2 \n");
        break;
    case PROTOCOL_WPA | PROTOCOL_WPA2:
        printf("PROTOCOL = WPA/WPA2 \n");
        break;
    default:
        printf("Unknown PROTOCOL: 0x%x \n", protocol);
        break;
    }
}

/**
 *  @brief Show wep tlv 
 *
 *  @param tlv     Pointer to wep tlv
 *  
 *  $return         N/A
 */
void
print_wep_key(tlvbuf_wep_key * tlv)
{
    int i;
    t_u16 tlv_len;

    tlv_len = *(t_u8 *) & tlv->length;
    tlv_len |= (*((t_u8 *) & tlv->length + 1) << 8);

    if (tlv_len <= 2) {
        printf("wrong wep_key tlv: length=%d\n", tlv_len);
        return;
    }
    printf("WEP KEY_%d = ", tlv->key_index);
    for (i = 0; i < tlv_len - 2; i++)
        printf("%02x ", tlv->key[i]);
    if (tlv->is_default)
        printf("\nDefault WEP Key = %d\n", tlv->key_index);
    else
        printf("\n");
}

/** 
 *  @brief Parses a command line
 *
 *  @param line     The line to parse
 *  @param args     Pointer to the argument buffer to be filled in
 *  @return         Number of arguments in the line or EOF
 */
static int
parse_line(char *line, char *args[])
{
    int arg_num = 0;
    int is_start = 0;
    int is_quote = 0;
    int length = 0;
    int i = 0;

    arg_num = 0;
    length = strlen(line);
    /* Process line */

    /* Find number of arguments */
    is_start = 0;
    is_quote = 0;
    for (i = 0; i < length; i++) {
        /* Ignore leading spaces */
        if (is_start == 0) {
            if (line[i] == ' ') {
                continue;
            } else if (line[i] == '\t') {
                continue;
            } else if (line[i] == '\n') {
                break;
            } else {
                is_start = 1;
                args[arg_num] = &line[i];
                arg_num++;
            }
        }
        if (is_start == 1) {
            /* Ignore comments */
            if (line[i] == '#') {
                if (is_quote == 0) {
                    line[i] = '\0';
                    arg_num--;
                }
                break;
            }
            /* Separate by '=' */
            if (line[i] == '=') {
                line[i] = '\0';
                is_start = 0;
                continue;
            }
            /* Separate by ',' */
            if (line[i] == ',') {
                line[i] = '\0';
                is_start = 0;
                continue;
            }
            /* Change ',' to ' ', but not inside quotes */
            if ((line[i] == ',') && (is_quote == 0)) {
                line[i] = ' ';
                continue;
            }
        }
        /* Remove newlines */
        if (line[i] == '\n') {
            line[i] = '\0';
        }
        /* Check for quotes */
        if (line[i] == '"') {
            is_quote = (is_quote == 1) ? 0 : 1;
            continue;
        }
        if (((line[i] == ' ') || (line[i] == '\t')) && (is_quote == 0)) {
            line[i] = '\0';
            is_start = 0;
            continue;
        }
    }
    return arg_num;
}

/** 
 *  @brief      Parse function for a configuration line  
 *
 *  @param s        Storage buffer for data
 *  @param size     Maximum size of data
 *  @param stream   File stream pointer
 *  @param line     Pointer to current line within the file
 *  @param _pos     Output string or NULL
 *  @return     String or NULL
 */
static char *
config_get_line(char *s, int size, FILE * stream, int *line, char **_pos)
{
    char *pos, *end, *sstart;
    while (fgets(s, size, stream)) {
        (*line)++;
        s[size - 1] = '\0';
        pos = s;
        /* Skip white space from the beginning of line. */
        while (*pos == ' ' || *pos == '\t' || *pos == '\r')
            pos++;
        /* Skip comment lines and empty lines */
        if (*pos == '#' || *pos == '\n' || *pos == '\0')
            continue;
        /* 
         * Remove # comments unless they are within a double quoted
         * string.
         */
        sstart = strchr(pos, '"');
        if (sstart)
            sstart = strrchr(sstart + 1, '"');
        if (!sstart)
            sstart = pos;
        end = strchr(sstart, '#');
        if (end)
            *end-- = '\0';
        else
            end = pos + strlen(pos) - 1;
        /* Remove trailing white space. */
        while (end > pos &&
               (*end == '\n' || *end == ' ' || *end == '\t' || *end == '\r'))
            *end-- = '\0';
        if (*pos == '\0')
            continue;
        if (_pos)
            *_pos = pos;
        return pos;
    }

    if (_pos)
        *_pos = NULL;
    return NULL;
}

/** 
 *  @brief Read the profile and sends to the driver
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS or UAP_FAILURE
 */
int
apcmd_sys_config_profile(int argc, char *argv[])
{
    FILE *config_file = NULL;
    char *line = NULL;
    int li = 0;
    char *pos = NULL;
    int arg_num = 0;
    char *args[30];
    int i;
    int is_ap_config = 0;
    int is_ap_mac_filter = 0;
    apcmdbuf_sys_configure *cmd_buf = NULL;
    HTCap_t htcap;
    int enable_11n = -1;
    t_u16 tlv_offset_11n = 0;
    t_u8 *buffer = NULL;
    t_u16 cmd_len = 0;
    t_u16 tlv_len = 0;
    int keyindex = -1;
    int protocol = -1;
    int pwkcipher_wpa = -1;
    int pwkcipher_wpa2 = -1;
    int gwkcipher = -1;
    tlvbuf_sta_mac_addr_filter *filter_tlv = NULL;
    int filter_mac_count = -1;
    int tx_data_rate = -1;
    int mcbc_data_rate = -1;
    t_u8 rate[MAX_RATES];
    int found = 0;
    char country_80211d[4];
    t_u8 state_80211d;
    int flag_80211d = 0;
    int chan_mode = 0;
    int band = 0;
    int ret = UAP_SUCCESS;

    memset(rate, 0, MAX_RATES);
    /* Check if file exists */
    config_file = fopen(argv[0], "r");
    if (config_file == NULL) {
        printf("\nERR:Config file can not open.\n");
        return UAP_FAILURE;
    }
    line = (char *) malloc(MAX_CONFIG_LINE);
    if (!line) {
        printf("ERR:Cannot allocate memory for line\n");
        ret = UAP_FAILURE;
        goto done;
    }
    memset(line, 0, MAX_CONFIG_LINE);

    /* Parse file and process */
    while (config_get_line(line, MAX_CONFIG_LINE, config_file, &li, &pos)) {
#if DEBUG
        uap_printf(MSG_DEBUG, "DBG:Received config line (%d) = %s\n", li, line);
#endif
        arg_num = parse_line(line, args);
#if DEBUG
        uap_printf(MSG_DEBUG, "DBG:Number of arguments = %d\n", arg_num);
        for (i = 0; i < arg_num; i++) {
            uap_printf(MSG_DEBUG, "\tDBG:Argument %d. %s\n", i + 1, args[i]);
        }
#endif
        /* Check for end of AP configurations */
        if (is_ap_config == 1) {
            if (strcmp(args[0], "}") == 0) {
                is_ap_config = 0;
                if (tx_data_rate != -1) {
                    if ((!rate[0]) && (tx_data_rate) &&
                        (is_tx_rate_valid((t_u8) tx_data_rate) !=
                         UAP_SUCCESS)) {
                        printf("ERR: Invalid Tx Data Rate \n");
                        ret = UAP_FAILURE;
                        goto done;
                    }
                    if (rate[0] && tx_data_rate) {
                        for (i = 0; rate[i] != 0; i++) {
                            if ((rate[i] & ~BASIC_RATE_SET_BIT) == tx_data_rate) {
                                found = 1;
                                break;
                            }
                        }
                        if (!found) {
                            printf("ERR: Invalid Tx Data Rate \n");
                            ret = UAP_FAILURE;
                            goto done;
                        }
                    }

                    /* Append a new TLV */
                    tlvbuf_tx_data_rate *tlv = NULL;
                    tlv_len = sizeof(tlvbuf_tx_data_rate);
                    buffer = realloc(buffer, cmd_len + tlv_len);
                    if (!buffer) {
                        printf("ERR:Cannot append tx data rate TLV!\n");
                        ret = UAP_FAILURE;
                        goto done;
                    }
                    cmd_buf = (apcmdbuf_sys_configure *) buffer;
                    tlv = (tlvbuf_tx_data_rate *) (buffer + cmd_len);
                    cmd_len += tlv_len;
                    /* Set TLV fields */
                    tlv->tag = MRVL_TX_DATA_RATE_TLV_ID;
                    tlv->length = 2;
                    tlv->tx_data_rate = tx_data_rate;
                    endian_convert_tlv_header_out(tlv);
                    tlv->tx_data_rate = uap_cpu_to_le16(tlv->tx_data_rate);
                }
                if (mcbc_data_rate != -1) {
                    if ((!rate[0]) && (mcbc_data_rate) &&
                        (is_mcbc_rate_valid((t_u8) mcbc_data_rate) !=
                         UAP_SUCCESS)) {
                        printf("ERR: Invalid Tx Data Rate \n");
                        ret = UAP_FAILURE;
                        goto done;
                    }
                    if (rate[0] && mcbc_data_rate) {
                        for (i = 0; rate[i] != 0; i++) {
                            if (rate[i] & BASIC_RATE_SET_BIT) {
                                if ((rate[i] & ~BASIC_RATE_SET_BIT) ==
                                    mcbc_data_rate) {
                                    found = 1;
                                    break;
                                }
                            }
                        }
                        if (!found) {
                            printf("ERR: Invalid MCBC Data Rate \n");
                            ret = UAP_FAILURE;
                            goto done;
                        }
                    }

                    /* Append a new TLV */
                    tlvbuf_mcbc_data_rate *tlv = NULL;
                    tlv_len = sizeof(tlvbuf_mcbc_data_rate);
                    buffer = realloc(buffer, cmd_len + tlv_len);
                    if (!buffer) {
                        printf("ERR:Cannot append tx data rate TLV!\n");
                        ret = UAP_FAILURE;
                        goto done;
                    }
                    cmd_buf = (apcmdbuf_sys_configure *) buffer;
                    tlv = (tlvbuf_mcbc_data_rate *) (buffer + cmd_len);
                    cmd_len += tlv_len;
                    /* Set TLV fields */
                    tlv->tag = MRVL_MCBC_DATA_RATE_TLV_ID;
                    tlv->length = 2;
                    tlv->mcbc_datarate = mcbc_data_rate;
                    endian_convert_tlv_header_out(tlv);
                    tlv->mcbc_datarate = uap_cpu_to_le16(tlv->mcbc_datarate);
                }
                if ((protocol == PROTOCOL_WPA2_MIXED) &&
                    ((pwkcipher_wpa < 0) || (pwkcipher_wpa2 < 0))) {
                    printf
                        ("ERR:Both PwkCipherWPA and PwkCipherWPA2 should be defined for Mixed mode.\n");
                    ret = UAP_FAILURE;
                    goto done;
                }

                if (((pwkcipher_wpa >= 0) || (pwkcipher_wpa2 >= 0)) &&
                    (gwkcipher >= 0)) {
                    if ((protocol == PROTOCOL_WPA) ||
                        (protocol == PROTOCOL_WPA2_MIXED)) {
                        if (enable_11n != -1) {
                            if (is_cipher_valid_with_11n
                                (pwkcipher_wpa, gwkcipher, protocol,
                                 enable_11n) != UAP_SUCCESS) {
                                printf
                                    ("ERR:Wrong group cipher and WPA pairwise cipher combination!\n");
                                ret = UAP_FAILURE;
                                goto done;
                            }
                        } else
                            if (is_cipher_valid_with_proto
                                (pwkcipher_wpa, gwkcipher,
                                 protocol) != UAP_SUCCESS) {
                            printf
                                ("ERR:Wrong group cipher and WPA pairwise cipher combination!\n");
                            ret = UAP_FAILURE;
                            goto done;
                        }
                    }
                    if ((protocol == PROTOCOL_WPA2) ||
                        (protocol == PROTOCOL_WPA2_MIXED)) {
                        if (enable_11n != -1) {
                            if (is_cipher_valid_with_11n
                                (pwkcipher_wpa2, gwkcipher, protocol,
                                 enable_11n) != UAP_SUCCESS) {
                                printf
                                    ("ERR:Wrong group cipher and WPA2 pairwise cipher combination!\n");
                                ret = UAP_FAILURE;
                                goto done;
                            }
                        } else
                            if (is_cipher_valid_with_proto
                                (pwkcipher_wpa2, gwkcipher,
                                 protocol) != UAP_SUCCESS) {
                            printf
                                ("ERR:Wrong group cipher and WPA2 pairwise cipher combination!\n");
                            ret = UAP_FAILURE;
                            goto done;
                        }
                    }
                }

                if (pwkcipher_wpa >= 0) {
                    tlvbuf_pwk_cipher *tlv = NULL;
                    /* Append a new TLV */
                    tlv_len = sizeof(tlvbuf_pwk_cipher);
                    buffer = realloc(buffer, cmd_len + tlv_len);
                    if (!buffer) {
                        printf("ERR:Cannot append cipher TLV!\n");
                        ret = UAP_FAILURE;
                        goto done;
                    }
                    cmd_buf = (apcmdbuf_sys_configure *) buffer;
                    tlv = (tlvbuf_pwk_cipher *) (buffer + cmd_len);
                    memset(tlv, 0, tlv_len);
                    cmd_len += tlv_len;
                    /* Set TLV fields */
                    tlv->tag = MRVL_CIPHER_PWK_TLV_ID;
                    tlv->length = sizeof(tlvbuf_pwk_cipher) - TLVHEADER_LEN;
                    tlv->pairwise_cipher = pwkcipher_wpa;
                    tlv->protocol = PROTOCOL_WPA;
                    endian_convert_tlv_header_out(tlv);
                    tlv->protocol = uap_cpu_to_le16(tlv->protocol);
                }

                if (pwkcipher_wpa2 >= 0) {
                    tlvbuf_pwk_cipher *tlv = NULL;
                    /* Append a new TLV */
                    tlv_len = sizeof(tlvbuf_pwk_cipher);
                    buffer = realloc(buffer, cmd_len + tlv_len);
                    if (!buffer) {
                        printf("ERR:Cannot append cipher TLV!\n");
                        ret = UAP_FAILURE;
                        goto done;
                    }
                    cmd_buf = (apcmdbuf_sys_configure *) buffer;
                    tlv = (tlvbuf_pwk_cipher *) (buffer + cmd_len);
                    memset(tlv, 0, tlv_len);
                    cmd_len += tlv_len;
                    /* Set TLV fields */
                    tlv->tag = MRVL_CIPHER_PWK_TLV_ID;
                    tlv->length = sizeof(tlvbuf_pwk_cipher) - TLVHEADER_LEN;
                    tlv->pairwise_cipher = pwkcipher_wpa2;
                    tlv->protocol = PROTOCOL_WPA2;
                    endian_convert_tlv_header_out(tlv);
                    tlv->protocol = uap_cpu_to_le16(tlv->protocol);
                }

                if (gwkcipher >= 0) {
                    tlvbuf_gwk_cipher *tlv = NULL;
                    /* Append a new TLV */
                    tlv_len = sizeof(tlvbuf_gwk_cipher);
                    buffer = realloc(buffer, cmd_len + tlv_len);
                    if (!buffer) {
                        printf("ERR:Cannot append cipher TLV!\n");
                        ret = UAP_FAILURE;
                        goto done;
                    }
                    cmd_buf = (apcmdbuf_sys_configure *) buffer;
                    tlv = (tlvbuf_gwk_cipher *) (buffer + cmd_len);
                    memset(tlv, 0, tlv_len);
                    cmd_len += tlv_len;
                    /* Set TLV fields */
                    tlv->tag = MRVL_CIPHER_GWK_TLV_ID;
                    tlv->length = sizeof(tlvbuf_gwk_cipher) - TLVHEADER_LEN;
                    tlv->group_cipher = gwkcipher;
                    endian_convert_tlv_header_out(tlv);
                }
                cmd_buf->size = cmd_len;
                /* Send collective command */
                if (uap_ioctl((t_u8 *) cmd_buf, &cmd_len, cmd_len) ==
                    UAP_SUCCESS) {
                    if (cmd_buf->result != CMD_SUCCESS) {
                        printf("ERR: Failed to set the configuration!\n");
                        ret = UAP_FAILURE;
                        goto done;
                    }
                } else {
                    printf("ERR: Command sending failed!\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
                cmd_len = 0;
                if (buffer) {
                    free(buffer);
                    buffer = NULL;
                }
                continue;
            }
        }

        /* Check for beginning of AP configurations */
        if (strcmp(args[0], "ap_config") == 0) {
            is_ap_config = 1;
            cmd_len = sizeof(apcmdbuf_sys_configure);
            if (buffer) {
                free(buffer);
                buffer = NULL;
            }
            buffer = (t_u8 *) malloc(cmd_len);
            if (!buffer) {
                printf("ERR:Cannot allocate memory!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            cmd_buf->cmd_code = APCMD_SYS_CONFIGURE;
            cmd_buf->size = cmd_len;
            cmd_buf->seq_num = 0;
            cmd_buf->result = 0;
            cmd_buf->action = ACTION_SET;
            continue;
        }

        /* Check for end of AP MAC address filter configurations */
        if (is_ap_mac_filter == 1) {
            if (strcmp(args[0], "}") == 0) {
                is_ap_mac_filter = 0;
                if (filter_tlv->count != filter_mac_count) {
                    printf
                        ("ERR:Number of MAC address provided does not match 'Count'\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
                if (filter_tlv->count) {
                    filter_tlv->length = (filter_tlv->count * ETH_ALEN) + 2;
                    cmd_len -=
                        (MAX_MAC_ONESHOT_FILTER - filter_mac_count) * ETH_ALEN;
                } else {
                    filter_tlv->length =
                        (MAX_MAC_ONESHOT_FILTER * ETH_ALEN) + 2;
                    memset(filter_tlv->mac_address, 0,
                           MAX_MAC_ONESHOT_FILTER * ETH_ALEN);
                }
                cmd_buf->size = cmd_len;
                endian_convert_tlv_header_out(filter_tlv);
                if (uap_ioctl((t_u8 *) cmd_buf, &cmd_len, cmd_len) ==
                    UAP_SUCCESS) {
                    if (cmd_buf->result != CMD_SUCCESS) {
                        printf("ERR: Failed to set the configuration!\n");
                        ret = UAP_FAILURE;
                        goto done;
                    }
                } else {
                    printf("ERR: Command sending failed!\n");
                    ret = UAP_FAILURE;
                    goto done;
                }

                cmd_len = 0;
                if (buffer) {
                    free(buffer);
                    buffer = NULL;
                }
                continue;
            }
        }

        if (flag_80211d && (strcmp(args[0], "11d_enable") == 0)) {
            if (IS_HEX_OR_DIGIT(args[1]) == UAP_FAILURE) {
                printf("ERR: valid input for state are 0 or 1\n");
                ret = UAP_FAILURE;
                goto done;
            }
            state_80211d = (t_u8) A2HEXDECIMAL(args[1]);

            if ((state_80211d != 0) && (state_80211d != 1)) {
                printf("ERR: valid input for state are 0 or 1 \n");
                ret = UAP_FAILURE;
                goto done;
            }
            if (sg_snmp_mib
                (ACTION_SET, OID_80211D_ENABLE, sizeof(state_80211d),
                 &state_80211d)
                == UAP_FAILURE) {
                ret = UAP_FAILURE;
                goto done;
            }
        }

        if (strcmp(args[0], "country") == 0) {
            apcmdbuf_cfg_80211d *cmd_buf = NULL;
            ieeetypes_subband_set_t sub_bands[MAX_SUB_BANDS];
            t_u8 no_of_sub_band = 0;
            t_u16 buf_len;
            t_u16 cmdlen;
            t_u8 *buf = NULL;

            if ((strlen(args[1]) > 3) || (strlen(args[1]) == 0)) {
                printf("In-correct country input\n");
                ret = UAP_FAILURE;
                goto done;
            }
            strcpy(country_80211d, args[1]);
            for (i = 0; (unsigned int) i < strlen(country_80211d); i++) {
                if ((country_80211d[i] < 'A') || (country_80211d[i] > 'z')) {
                    printf("Invalid Country Code\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
                if (country_80211d[i] > 'Z')
                    country_80211d[i] = country_80211d[i] - 'a' + 'A';
            }
            no_of_sub_band = parse_domain_file(country_80211d, band, sub_bands);
            if (no_of_sub_band == UAP_FAILURE) {
                printf("Parsing Failed\n");
                ret = UAP_FAILURE;
                goto done;
            }
            buf_len = sizeof(apcmdbuf_cfg_80211d);
            buf_len += no_of_sub_band * sizeof(ieeetypes_subband_set_t);
            buf = (t_u8 *) malloc(buf_len);
            if (!buf) {
                printf("ERR:Cannot allocate buffer from command!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            memset(buf, 0, buf_len);
            cmd_buf = (apcmdbuf_cfg_80211d *) buf;
            cmdlen = buf_len;
            cmd_buf->size = cmdlen - BUF_HEADER_SIZE;
            cmd_buf->result = 0;
            cmd_buf->seq_num = 0;
            cmd_buf->action = uap_cpu_to_le16(ACTION_SET);
            cmd_buf->cmd_code = HostCmd_CMD_802_11D_DOMAIN_INFO;
            cmd_buf->domain.tag = uap_cpu_to_le16(TLV_TYPE_DOMAIN);
            cmd_buf->domain.length = uap_cpu_to_le16(sizeof(domain_param_t)
                                                     - BUF_HEADER_SIZE
                                                     +
                                                     (no_of_sub_band *
                                                      sizeof
                                                      (ieeetypes_subband_set_t)));

            memset(cmd_buf->domain.country_code, ' ',
                   sizeof(cmd_buf->domain.country_code));
            memcpy(cmd_buf->domain.country_code, country_80211d,
                   strlen(country_80211d));
            memcpy(cmd_buf->domain.subband, sub_bands,
                   no_of_sub_band * sizeof(ieeetypes_subband_set_t));

            /* Send the command */
            if (uap_ioctl((t_u8 *) cmd_buf, &cmdlen, cmdlen) == UAP_SUCCESS) {
                if (cmd_buf->result != CMD_SUCCESS) {
                    printf("ERR: Failed to set the configuration!\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
            } else {
                printf("ERR: Command sending failed!\n");
                ret = UAP_FAILURE;
                goto done;
            }

            if (buf)
                free(buf);
        }

        /* Check for beginning of AP MAC address filter configurations */
        if (strcmp(args[0], "ap_mac_filter") == 0) {
            is_ap_mac_filter = 1;
            cmd_len =
                sizeof(apcmdbuf_sys_configure) +
                sizeof(tlvbuf_sta_mac_addr_filter) +
                (MAX_MAC_ONESHOT_FILTER * ETH_ALEN);
            if (buffer) {
                free(buffer);
                buffer = NULL;
            }
            buffer = (t_u8 *) malloc(cmd_len);
            if (!buffer) {
                printf("ERR:Cannot allocate memory!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            cmd_buf->cmd_code = APCMD_SYS_CONFIGURE;
            cmd_buf->size = cmd_len;
            cmd_buf->seq_num = 0;
            cmd_buf->result = 0;
            cmd_buf->action = ACTION_SET;
            filter_tlv =
                (tlvbuf_sta_mac_addr_filter *) (buffer +
                                                sizeof(apcmdbuf_sys_configure));
            filter_tlv->tag = MRVL_STA_MAC_ADDR_FILTER_TLV_ID;
            filter_tlv->length = 2;
            filter_tlv->count = 0;
            filter_mac_count = 0;
            continue;
        }
        if ((strcmp(args[0], "FilterMode") == 0) && is_ap_mac_filter) {
            if ((ISDIGIT(args[1]) == 0) || (atoi(args[1]) < 0) ||
                (atoi(args[1]) > 2)) {
                printf
                    ("ERR:Illegal FilterMode paramter %d. Must be either '0', '1', or '2'.\n",
                     atoi(args[1]));
                ret = UAP_FAILURE;
                goto done;
            }
            filter_tlv->filter_mode = atoi(args[1]);
            continue;
        }
        if ((strcmp(args[0], "Count") == 0) && is_ap_mac_filter) {
            filter_tlv->count = atoi(args[1]);
            if ((ISDIGIT(args[1]) == 0) ||
                (filter_tlv->count > MAX_MAC_ONESHOT_FILTER)) {
                printf("ERR: Illegal Count parameter.\n");
                ret = UAP_FAILURE;
                goto done;
            }
        }
        if ((strncmp(args[0], "mac_", 4) == 0) && is_ap_mac_filter) {
            if (filter_mac_count < MAX_MAC_ONESHOT_FILTER) {
                if (mac2raw
                    (args[1],
                     &filter_tlv->mac_address[filter_mac_count * ETH_ALEN]) !=
                    UAP_SUCCESS) {
                    printf("ERR: Invalid MAC address %s \n", args[1]);
                    ret = UAP_FAILURE;
                    goto done;
                }
                filter_mac_count++;
            } else {
                printf
                    ("ERR: Filter table can not have more than %d MAC addresses\n",
                     MAX_MAC_ONESHOT_FILTER);
                ret = UAP_FAILURE;
                goto done;
            }
        }

        if (strcmp(args[0], "SSID") == 0) {
            if (arg_num == 1) {
                printf("ERR:SSID field is blank!\n");
                ret = UAP_FAILURE;
                goto done;
            } else {
                tlvbuf_ssid *tlv = NULL;
                if (args[1][0] == '"') {
                    args[1]++;
                }
                if (args[1][strlen(args[1]) - 1] == '"') {
                    args[1][strlen(args[1]) - 1] = '\0';
                }
                if ((strlen(args[1]) > MAX_SSID_LENGTH) ||
                    (strlen(args[1]) == 0)) {
                    printf("ERR:SSID length out of range (%d to %d).\n",
                           MIN_SSID_LENGTH, MAX_SSID_LENGTH);
                    ret = UAP_FAILURE;
                    goto done;
                }
                /* Append a new TLV */
                tlv_len = sizeof(tlvbuf_ssid) + strlen(args[1]);
                buffer = realloc(buffer, cmd_len + tlv_len);
                if (!buffer) {
                    printf("ERR:Cannot realloc SSID TLV!\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
                cmd_buf = (apcmdbuf_sys_configure *) buffer;
                tlv = (tlvbuf_ssid *) (buffer + cmd_len);
                cmd_len += tlv_len;
                /* Set TLV fields */
                tlv->tag = MRVL_SSID_TLV_ID;
                tlv->length = strlen(args[1]);
                memcpy(tlv->ssid, args[1], tlv->length);
                endian_convert_tlv_header_out(tlv);
            }
        }
        if (strcmp(args[0], "BeaconPeriod") == 0) {
            if (is_input_valid(BEACONPERIOD, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                goto done;
            }
            tlvbuf_beacon_period *tlv = NULL;
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_beacon_period);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot realloc beacon period TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_beacon_period *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_BEACON_PERIOD_TLV_ID;
            tlv->length = 2;
            tlv->beacon_period_ms = (t_u16) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
            tlv->beacon_period_ms = uap_cpu_to_le16(tlv->beacon_period_ms);
        }
        if (strcmp(args[0], "ChanList") == 0) {
            if (is_input_valid(SCANCHANNELS, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }

            tlvbuf_channel_list *tlv = NULL;
            channel_list *pchan_list = NULL;
            /* Append a new TLV */
            tlv_len =
                sizeof(tlvbuf_channel_list) +
                ((arg_num - 1) * sizeof(channel_list));
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append channel list TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_channel_list *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_CHANNELLIST_TLV_ID;
            tlv->length = sizeof(channel_list) * (arg_num - 1);
            pchan_list = (channel_list *) tlv->chan_list;
            for (i = 0; i < (arg_num - 1); i++) {
                pchan_list->chan_number = (t_u8) atoi(args[i + 1]);
                pchan_list->band_config_type = 0;
                pchan_list++;
            }
            endian_convert_tlv_header_out(tlv);
        }
        if (strcmp(args[0], "Channel") == 0) {
            if (is_input_valid(CHANNEL, arg_num - 1, args + 1) != UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_channel_config *tlv = NULL;
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_channel_config);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append channel TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_channel_config *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_CHANNELCONFIG_TLV_ID;
            tlv->length = 2;
            tlv->chan_number = (t_u8) atoi(args[1]);
            if (tlv->chan_number > MAX_CHANNELS_BG)
                band = BAND_A;
            else
                band = BAND_B | BAND_G;
            if ((arg_num - 1) == 2) {
                chan_mode = atoi(args[2]);
                tlv->band_config_type = 0;
                if (chan_mode & BITMAP_ACS_MODE)
                    tlv->band_config_type |= BAND_CONFIG_ACS_MODE;
                if (chan_mode & BITMAP_CHANNEL_ABOVE)
                    tlv->band_config_type |= SECOND_CHANNEL_ABOVE;
                if (chan_mode & BITMAP_CHANNEL_BELOW)
                    tlv->band_config_type |= SECOND_CHANNEL_BELOW;
            } else
                tlv->band_config_type = 0;
            if (tlv->chan_number > MAX_CHANNELS_BG)
                tlv->band_config_type |= BAND_CONFIG_5GHZ;
            endian_convert_tlv_header_out(tlv);
        }
        if (strcmp(args[0], "AP_MAC") == 0) {
            int ret;
            tlvbuf_ap_mac_address *tlv = NULL;
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_ap_mac_address);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append ap_mac TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_ap_mac_address *) (buffer + cmd_len);
            cmd_len += tlv_len;
            cmd_buf->action = ACTION_SET;
            tlv->tag = MRVL_AP_MAC_ADDRESS_TLV_ID;
            tlv->length = ETH_ALEN;
            if ((ret = mac2raw(args[1], tlv->ap_mac_addr)) != UAP_SUCCESS) {
                printf("ERR: %s Address \n",
                       ret == UAP_FAILURE ? "Invalid MAC" : ret ==
                       UAP_RET_MAC_BROADCAST ? "Broadcast" : "Multicast");
                ret = UAP_FAILURE;
                goto done;
            }
            endian_convert_tlv_header_out(tlv);
        }

        if (strcmp(args[0], "RxAntenna") == 0) {
            if ((ISDIGIT(args[1]) != UAP_SUCCESS) || (atoi(args[1]) < 0) ||
                (atoi(args[1]) > 1)) {
                printf("ERR: Invalid Antenna value\n");
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_antenna_ctl *tlv = NULL;
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_antenna_ctl);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append channel TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_antenna_ctl *) (buffer + cmd_len);
            cmd_len += tlv_len;
            cmd_buf->action = ACTION_SET;
            tlv->tag = MRVL_ANTENNA_CTL_TLV_ID;
            tlv->length = 2;
            tlv->which_antenna = 0;
            tlv->antenna_mode = atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
        }

        if (strcmp(args[0], "TxAntenna") == 0) {
            if ((ISDIGIT(args[1]) != UAP_SUCCESS) || (atoi(args[1]) < 0) ||
                (atoi(args[1]) > 1)) {
                printf("ERR: Invalid Antenna value\n");
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_antenna_ctl *tlv = NULL;
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_antenna_ctl);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append channel TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_antenna_ctl *) (buffer + cmd_len);
            cmd_len += tlv_len;
            cmd_buf->action = ACTION_SET;
            tlv->tag = MRVL_ANTENNA_CTL_TLV_ID;
            tlv->length = 2;
            tlv->which_antenna = 1;
            tlv->antenna_mode = atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
        }
        if (strcmp(args[0], "Rate") == 0) {
            if (is_input_valid(RATE, arg_num - 1, args + 1) != UAP_SUCCESS) {
                printf("ERR: Invalid Rate input\n");
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_rates *tlv = NULL;
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_rates) + arg_num - 1;
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append rates TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_rates *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_RATES_TLV_ID;
            tlv->length = arg_num - 1;
            for (i = 0; i < tlv->length; i++) {
                rate[i] = tlv->operational_rates[i] =
                    (t_u8) A2HEXDECIMAL(args[i + 1]);
            }
            endian_convert_tlv_header_out(tlv);
        }
        if (strcmp(args[0], "TxPowerLevel") == 0) {
            if (is_input_valid(TXPOWER, arg_num - 1, args + 1) != UAP_SUCCESS) {
                printf("ERR:Invalid TxPowerLevel \n");
                ret = UAP_FAILURE;
                goto done;
            } else {
                tlvbuf_tx_power *tlv = NULL;
                /* Append a new TLV */
                tlv_len = sizeof(tlvbuf_tx_power);
                buffer = realloc(buffer, cmd_len + tlv_len);
                if (!buffer) {
                    printf("ERR:Cannot append tx power level TLV!\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
                cmd_buf = (apcmdbuf_sys_configure *) buffer;
                tlv = (tlvbuf_tx_power *) (buffer + cmd_len);
                cmd_len += tlv_len;
                /* Set TLV fields */
                tlv->tag = MRVL_TX_POWER_TLV_ID;
                tlv->length = 1;
                tlv->tx_power_dbm = (t_u8) atoi(args[1]);
                endian_convert_tlv_header_out(tlv);
            }
        }
        if (strcmp(args[0], "BroadcastSSID") == 0) {
            if (is_input_valid(BROADCASTSSID, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                goto done;
            }
            tlvbuf_bcast_ssid_ctl *tlv = NULL;
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_bcast_ssid_ctl);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append SSID broadcast control TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_bcast_ssid_ctl *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_BCAST_SSID_CTL_TLV_ID;
            tlv->length = 1;
            tlv->bcast_ssid_ctl = (t_u8) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
        }
        if (strcmp(args[0], "RTSThreshold") == 0) {
            if (is_input_valid(RTSTHRESH, arg_num - 1, args + 1) != UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_rts_threshold *tlv = NULL;
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_rts_threshold);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append RTS threshold TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_rts_threshold *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_RTS_THRESHOLD_TLV_ID;
            tlv->length = 2;
            tlv->rts_threshold = (t_u16) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
            tlv->rts_threshold = uap_cpu_to_le16(tlv->rts_threshold);
        }
        if (strcmp(args[0], "FragThreshold") == 0) {
            if (is_input_valid(FRAGTHRESH, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_frag_threshold *tlv = NULL;
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_frag_threshold);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append Fragmentation threshold TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_frag_threshold *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_FRAG_THRESHOLD_TLV_ID;
            tlv->length = 2;
            tlv->frag_threshold = (t_u16) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
            tlv->frag_threshold = uap_cpu_to_le16(tlv->frag_threshold);
        }
        if (strcmp(args[0], "DTIMPeriod") == 0) {
            if (is_input_valid(DTIMPERIOD, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_dtim_period *tlv = NULL;
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_dtim_period);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append DTIM period TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_dtim_period *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_DTIM_PERIOD_TLV_ID;
            tlv->length = 1;
            tlv->dtim_period = (t_u8) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
        }
        if (strcmp(args[0], "RadioControl") == 0) {
            if (is_input_valid(RADIOCONTROL, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_radio_ctl *tlv = NULL;
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_radio_ctl);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append radio control TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_radio_ctl *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_RADIO_CTL_TLV_ID;
            tlv->length = 1;
            tlv->radio_ctl = (t_u8) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
        }
        if (strcmp(args[0], "RSNReplayProtection") == 0) {
            if (is_input_valid(RSNREPLAYPROT, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_rsn_replay_prot *tlv = NULL;
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_rsn_replay_prot);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append RSN replay protection TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_rsn_replay_prot *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_RSN_REPLAY_PROT_TLV_ID;
            tlv->length = 1;
            tlv->rsn_replay_prot = (t_u8) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
        }
        if (strcmp(args[0], "TxDataRate") == 0) {
            if (is_input_valid(TXDATARATE, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            tx_data_rate = (t_u16) A2HEXDECIMAL(args[1]);
        }
        if (strcmp(args[0], "MCBCdataRate") == 0) {
            if (is_input_valid(MCBCDATARATE, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            mcbc_data_rate = (t_u16) A2HEXDECIMAL(args[1]);
        }
        if (strcmp(args[0], "PktFwdCtl") == 0) {
            if (is_input_valid(PKTFWD, arg_num - 1, args + 1) != UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_pkt_fwd_ctl *tlv = NULL;
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_pkt_fwd_ctl);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append packet forwarding control TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_pkt_fwd_ctl *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_PKT_FWD_CTL_TLV_ID;
            tlv->length = 1;
            tlv->pkt_fwd_ctl = (t_u8) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
        }
        if (strcmp(args[0], "StaAgeoutTimer") == 0) {
            if (is_input_valid(STAAGEOUTTIMER, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_sta_ageout_timer *tlv = NULL;
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_sta_ageout_timer);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append STA ageout timer TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_sta_ageout_timer *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_STA_AGEOUT_TIMER_TLV_ID;
            tlv->length = 4;
            tlv->sta_ageout_timer_ms = (t_u32) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
            tlv->sta_ageout_timer_ms =
                uap_cpu_to_le32(tlv->sta_ageout_timer_ms);
        }
        if (strcmp(args[0], "PSStaAgeoutTimer") == 0) {
            if (is_input_valid(PSSTAAGEOUTTIMER, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_ps_sta_ageout_timer *tlv = NULL;
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_ps_sta_ageout_timer);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append PS STA ageout timer TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_ps_sta_ageout_timer *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_PS_STA_AGEOUT_TIMER_TLV_ID;
            tlv->length = 4;
            tlv->ps_sta_ageout_timer_ms = (t_u32) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
            tlv->ps_sta_ageout_timer_ms =
                uap_cpu_to_le32(tlv->ps_sta_ageout_timer_ms);
        }
        if (strcmp(args[0], "AuthMode") == 0) {
            if (is_input_valid(AUTHMODE, arg_num - 1, args + 1) != UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_auth_mode *tlv = NULL;
            if ((atoi(args[1]) < 0) || (atoi(args[1]) > 1)) {
                printf
                    ("ERR:Illegal AuthMode parameter. Must be either '0' or '1'.\n");
                ret = UAP_FAILURE;
                goto done;
            }
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_auth_mode);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append auth mode TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_auth_mode *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_AUTH_TLV_ID;
            tlv->length = 1;
            tlv->auth_mode = (t_u8) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
        }
        if (strcmp(args[0], "KeyIndex") == 0) {
            if (arg_num == 1) {
                printf("KeyIndex is blank!\n");
                ret = UAP_FAILURE;
                goto done;
            } else {
                if (ISDIGIT(args[1]) == 0) {
                    printf
                        ("ERR:Illegal KeyIndex parameter. Must be either '0', '1', '2', or '3'.\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
                keyindex = atoi(args[1]);
                if ((keyindex < 0) || (keyindex > 3)) {
                    printf
                        ("ERR:Illegal KeyIndex parameter. Must be either '0', '1', '2', or '3'.\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
            }
        }
        if (strncmp(args[0], "Key_", 4) == 0) {
            if (arg_num == 1) {
                printf("ERR:%s is blank!\n", args[0]);
                ret = UAP_FAILURE;
                goto done;
            } else {
                tlvbuf_wep_key *tlv = NULL;
                int key_len = 0;
                if (args[1][0] == '"') {
                    if ((strlen(args[1]) != 2) && (strlen(args[1]) != 7) &&
                        (strlen(args[1]) != 15)) {
                        printf("ERR:Wrong key length!\n");
                        ret = UAP_FAILURE;
                        goto done;
                    }
                    key_len = strlen(args[1]) - 2;
                } else {
                    if ((strlen(args[1]) != 0) && (strlen(args[1]) != 10) &&
                        (strlen(args[1]) != 26)) {
                        printf("ERR:Wrong key length!\n");
                        ret = UAP_FAILURE;
                        goto done;
                    }
                    if (UAP_FAILURE == ishexstring(args[1])) {
                        printf
                            ("ERR:Only hex digits are allowed when key length is 10 or 26\n");
                        ret = UAP_FAILURE;
                        goto done;
                    }
                    key_len = strlen(args[1]) / 2;
                }
                memset(&htcap, 0, sizeof(htcap));
                if (UAP_SUCCESS == get_sys_cfg_11n(&htcap)) {
                    if (htcap.supported_mcs_set[0]) {
                        printf
                            ("ERR: WEP cannot be used when AP operates in 802.11n mode.\n");
                        ret = UAP_FAILURE;
                        goto done;
                    }
                }
                /* Append a new TLV */
                tlv_len = sizeof(tlvbuf_wep_key) + key_len;
                buffer = realloc(buffer, cmd_len + tlv_len);
                if (!buffer) {
                    printf("ERR:Cannot append WEP key configurations TLV!\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
                cmd_buf = (apcmdbuf_sys_configure *) buffer;
                tlv = (tlvbuf_wep_key *) (buffer + cmd_len);
                cmd_len += tlv_len;
                /* Set TLV fields */
                tlv->tag = MRVL_WEP_KEY_TLV_ID;
                tlv->length = key_len + 2;
                if (strcmp(args[0], "Key_0") == 0) {
                    tlv->key_index = 0;
                } else if (strcmp(args[0], "Key_1") == 0) {
                    tlv->key_index = 1;
                } else if (strcmp(args[0], "Key_2") == 0) {
                    tlv->key_index = 2;
                } else if (strcmp(args[0], "Key_3") == 0) {
                    tlv->key_index = 3;
                }
                if (keyindex == tlv->key_index) {
                    tlv->is_default = 1;
                } else {
                    tlv->is_default = 0;
                }
                if (args[1][0] == '"') {
                    memcpy(tlv->key, &args[1][1], strlen(args[1]) - 2);
                } else {
                    string2raw(args[1], tlv->key);
                }
                endian_convert_tlv_header_out(tlv);
            }
        }
        if (strcmp(args[0], "PSK") == 0) {
            if (arg_num == 1) {
                printf("ERR:PSK is blank!\n");
                ret = UAP_FAILURE;
                goto done;
            } else {
                tlvbuf_wpa_passphrase *tlv = NULL;
                if (args[1][0] == '"') {
                    args[1]++;
                }
                if (args[1][strlen(args[1]) - 1] == '"') {
                    args[1][strlen(args[1]) - 1] = '\0';
                }
                tlv_len = sizeof(tlvbuf_wpa_passphrase) + strlen(args[1]);
                if (strlen(args[1]) > MAX_WPA_PASSPHRASE_LENGTH) {
                    printf("ERR:PSK too long.\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
                if (strlen(args[1]) < MIN_WPA_PASSPHRASE_LENGTH) {
                    printf("ERR:PSK too short.\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
                if (strlen(args[1]) == MAX_WPA_PASSPHRASE_LENGTH) {
                    if (UAP_FAILURE == ishexstring(args[1])) {
                        printf
                            ("ERR:Only hex digits are allowed when passphrase's length is 64\n");
                        ret = UAP_FAILURE;
                        goto done;
                    }
                }
                /* Append a new TLV */
                buffer = realloc(buffer, cmd_len + tlv_len);
                if (!buffer) {
                    printf("ERR:Cannot append WPA passphrase TLV!\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
                cmd_buf = (apcmdbuf_sys_configure *) buffer;
                tlv = (tlvbuf_wpa_passphrase *) (buffer + cmd_len);
                cmd_len += tlv_len;
                /* Set TLV fields */
                tlv->tag = MRVL_WPA_PASSPHRASE_TLV_ID;
                tlv->length = strlen(args[1]);
                memcpy(tlv->passphrase, args[1], tlv->length);
                endian_convert_tlv_header_out(tlv);
            }
        }
        if (strcmp(args[0], "Protocol") == 0) {
            if (is_input_valid(PROTOCOL, arg_num - 1, args + 1) != UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            protocol = (t_u16) atoi(args[1]);
            tlvbuf_protocol *tlv = NULL;
            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_protocol);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append protocol TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_protocol *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_PROTOCOL_TLV_ID;
            tlv->length = 2;
            tlv->protocol = protocol;
            endian_convert_tlv_header_out(tlv);
            tlv->protocol = uap_cpu_to_le16(tlv->protocol);
            if (atoi(args[1]) & (PROTOCOL_WPA | PROTOCOL_WPA2)) {
                tlvbuf_akmp *tlv = NULL;
                /* Append a new TLV */
                tlv_len = sizeof(tlvbuf_akmp);
                buffer = realloc(buffer, cmd_len + tlv_len);
                if (!buffer) {
                    printf("ERR:Cannot append AKMP TLV!\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
                cmd_buf = (apcmdbuf_sys_configure *) buffer;
                tlv = (tlvbuf_akmp *) (buffer + cmd_len);
                cmd_len += tlv_len;
                /* Set TLV fields */
                tlv->tag = MRVL_AKMP_TLV_ID;
                tlv->length = 2;
                tlv->key_mgmt = KEY_MGMT_PSK;
                endian_convert_tlv_header_out(tlv);
                tlv->key_mgmt = uap_cpu_to_le16(tlv->key_mgmt);
            }
        }

        if ((strcmp(args[0], "PairwiseCipher") == 0) ||
            (strcmp(args[0], "GroupCipher") == 0)) {
            printf("ERR:PairwiseCipher and GroupCipher are not supported.\n"
                   "    Please configure pairwise cipher using parameters PwkCipherWPA or PwkCipherWPA2\n"
                   "    and group cipher using GwkCipher in the config file.\n");
            ret = UAP_FAILURE;
            goto done;
        }

        if ((protocol == PROTOCOL_NO_SECURITY) ||
            (protocol == PROTOCOL_STATIC_WEP)) {
            if ((strcmp(args[0], "PwkCipherWPA") == 0) ||
                (strcmp(args[0], "PwkCipherWPA2") == 0)
                || (strcmp(args[0], "GwkCipher") == 0)) {
                printf
                    ("ERR:Pairwise cipher and group cipher should not be defined for Open and WEP mode.\n");
                ret = UAP_FAILURE;
                goto done;
            }
        }

        if (strcmp(args[0], "PwkCipherWPA") == 0) {
            if (arg_num == 1) {
                printf("ERR:PwkCipherWPA is blank!\n");
                ret = UAP_FAILURE;
                goto done;
            } else {
                if (ISDIGIT(args[1]) == 0) {
                    printf
                        ("ERR:Illegal PwkCipherWPA parameter. Must be either bit '2' or '3'.\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
                if (atoi(args[1]) & ~CIPHER_BITMAP) {
                    printf
                        ("ERR:Illegal PwkCipherWPA parameter. Must be either bit '2' or '3'.\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
                pwkcipher_wpa = atoi(args[1]);
                if (enable_11n && protocol != PROTOCOL_WPA2_MIXED) {
                    memset(&htcap, 0, sizeof(htcap));
                    if (UAP_SUCCESS == get_sys_cfg_11n(&htcap)) {
                        if (htcap.supported_mcs_set[0] &&
                            (atoi(args[1]) == CIPHER_TKIP)) {
                            printf
                                ("ERR: WPA/TKIP cannot be used when AP operates in 802.11n mode.\n");
                            ret = UAP_FAILURE;
                            goto done;
                        }
                    }
                }
            }
        }

        if (strcmp(args[0], "PwkCipherWPA2") == 0) {
            if (arg_num == 1) {
                printf("ERR:PwkCipherWPA2 is blank!\n");
                ret = UAP_FAILURE;
                goto done;
            } else {
                if (ISDIGIT(args[1]) == 0) {
                    printf
                        ("ERR:Illegal PwkCipherWPA2 parameter. Must be either bit '2' or '3'.\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
                if (atoi(args[1]) & ~CIPHER_BITMAP) {
                    printf
                        ("ERR:Illegal PwkCipherWPA2 parameter. Must be either bit '2' or '3'.\n");
                    ret = UAP_FAILURE;
                    goto done;
                }
                pwkcipher_wpa2 = atoi(args[1]);
                if (enable_11n && protocol != PROTOCOL_WPA2_MIXED) {
                    memset(&htcap, 0, sizeof(htcap));
                    if (UAP_SUCCESS == get_sys_cfg_11n(&htcap)) {
                        if (htcap.supported_mcs_set[0] &&
                            (atoi(args[1]) == CIPHER_TKIP)) {
                            printf
                                ("ERR: WPA/TKIP cannot be used when AP operates in 802.11n mode.\n");
                            ret = UAP_FAILURE;
                            goto done;
                        }
                    }
                }
            }
        }
        if (strcmp(args[0], "GwkCipher") == 0) {
            if (is_input_valid(GWK_CIPHER, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            gwkcipher = atoi(args[1]);
        }
        if (strcmp(args[0], "GroupRekeyTime") == 0) {
            if (is_input_valid(GROUPREKEYTIMER, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_group_rekey_timer *tlv = NULL;

            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_group_rekey_timer);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append group rekey timer TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_group_rekey_timer *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_GRP_REKEY_TIME_TLV_ID;
            tlv->length = 4;
            tlv->group_rekey_time_sec = (t_u32) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
            tlv->group_rekey_time_sec =
                uap_cpu_to_le32(tlv->group_rekey_time_sec);
        }
        if (strcmp(args[0], "MaxStaNum") == 0) {
            if (is_input_valid(MAXSTANUM, arg_num - 1, args + 1) != UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_max_sta_num *tlv = NULL;

            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_max_sta_num);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot realloc max station number TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_max_sta_num *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_MAX_STA_CNT_TLV_ID;
            tlv->length = 2;
            tlv->max_sta_num = (t_u16) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
            tlv->max_sta_num = uap_cpu_to_le16(tlv->max_sta_num);
        }
        if (strcmp(args[0], "Retrylimit") == 0) {
            if (is_input_valid(RETRYLIMIT, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_retry_limit *tlv = NULL;

            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_retry_limit);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot realloc retry limit TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_retry_limit *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_RETRY_LIMIT_TLV_ID;
            tlv->length = 1;
            tlv->retry_limit = (t_u8) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
        }
        if (strcmp(args[0], "PairwiseUpdateTimeout") == 0) {
            if ((ISDIGIT(args[1]) == 0) || (atoi(args[1]) < 0)) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_eapol_pwk_hsk_timeout *tlv = NULL;

            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_eapol_pwk_hsk_timeout);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append pairwise update timeout TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_eapol_pwk_hsk_timeout *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_EAPOL_PWK_HSK_TIMEOUT_TLV_ID;
            tlv->length = 4;
            tlv->pairwise_update_timeout = (t_u32) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
            tlv->pairwise_update_timeout =
                uap_cpu_to_le32(tlv->pairwise_update_timeout);
        }
        if (strcmp(args[0], "PairwiseHandshakeRetries") == 0) {
            if ((ISDIGIT(args[1]) == 0) || (atoi(args[1]) < 0)) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_eapol_pwk_hsk_retries *tlv = NULL;

            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_eapol_pwk_hsk_retries);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append pairwise handshake retries TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_eapol_pwk_hsk_retries *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_EAPOL_PWK_HSK_RETRIES_TLV_ID;
            tlv->length = 4;
            tlv->pwk_retries = (t_u32) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
            tlv->pwk_retries = uap_cpu_to_le32(tlv->pwk_retries);
        }
        if (strcmp(args[0], "GroupwiseUpdateTimeout") == 0) {
            if ((ISDIGIT(args[1]) == 0) || (atoi(args[1]) < 0)) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_eapol_gwk_hsk_timeout *tlv = NULL;

            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_eapol_gwk_hsk_timeout);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append groupwise update timeout TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_eapol_gwk_hsk_timeout *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_EAPOL_GWK_HSK_TIMEOUT_TLV_ID;
            tlv->length = 4;
            tlv->groupwise_update_timeout = (t_u32) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
            tlv->groupwise_update_timeout =
                uap_cpu_to_le32(tlv->groupwise_update_timeout);
        }
        if (strcmp(args[0], "GroupwiseHandshakeRetries") == 0) {
            if ((ISDIGIT(args[1]) == 0) || (atoi(args[1]) < 0)) {
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_eapol_gwk_hsk_retries *tlv = NULL;

            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_eapol_gwk_hsk_retries);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append groupwise handshake retries TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_eapol_gwk_hsk_retries *) (buffer + cmd_len);
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = MRVL_EAPOL_GWK_HSK_RETRIES_TLV_ID;
            tlv->length = 4;
            tlv->gwk_retries = (t_u32) atoi(args[1]);
            endian_convert_tlv_header_out(tlv);
            tlv->gwk_retries = uap_cpu_to_le32(tlv->gwk_retries);
        }

        if (strcmp(args[0], "Enable11n") == 0) {
            if ((ISDIGIT(args[1]) != UAP_SUCCESS) || (atoi(args[1]) < 0) ||
                (atoi(args[1]) > 1)) {
                printf("ERR: Invalid Enable11n value\n");
                ret = UAP_FAILURE;
                goto done;
            }
            tlvbuf_htcap_t *tlv = NULL;
            enable_11n = atoi(args[1]);

            memset(&htcap, 0, sizeof(htcap));
            if (UAP_SUCCESS != get_sys_cfg_11n(&htcap)) {
                printf("ERR: Reading current 11n configuration.\n");
                ret = UAP_FAILURE;
                goto done;
            }

            /* Append a new TLV */
            tlv_len = sizeof(tlvbuf_htcap_t);
            buffer = realloc(buffer, cmd_len + tlv_len);
            if (!buffer) {
                printf("ERR:Cannot append HT Cap TLV!\n");
                ret = UAP_FAILURE;
                goto done;
            }
            cmd_buf = (apcmdbuf_sys_configure *) buffer;
            tlv = (tlvbuf_htcap_t *) (buffer + cmd_len);
            tlv_offset_11n = cmd_len;
            cmd_len += tlv_len;
            /* Set TLV fields */
            tlv->tag = HT_CAPABILITY_TLV_ID;
            tlv->length = sizeof(HTCap_t);
            memcpy(&tlv->ht_cap, &htcap, sizeof(HTCap_t));
            if (enable_11n == 1) {
                /* enable mcs rate */
                tlv->ht_cap.supported_mcs_set[0] = DEFAULT_MCS_SET_0;
                tlv->ht_cap.supported_mcs_set[4] = DEFAULT_MCS_SET_4;
            } else {
                /* disable mcs rate */
                tlv->ht_cap.supported_mcs_set[0] = 0;
                tlv->ht_cap.supported_mcs_set[4] = 0;
            }
            endian_convert_tlv_header_out(tlv);
        }
        if (strcmp(args[0], "HTCapInfo") == 0) {
            if (enable_11n <= 0) {
                printf
                    ("ERR: Enable11n parameter should be set before HTCapInfo.\n");
                ret = UAP_FAILURE;
                goto done;
            }
            if ((IS_HEX_OR_DIGIT(args[1]) == UAP_FAILURE) ||
                ((((t_u16) A2HEXDECIMAL(args[1])) & (~HT_CAP_CONFIG_MASK)) !=
                 HT_CAP_CHECK_MASK)) {
                printf("ERR: Invalid HTCapInfo value\n");
                ret = UAP_FAILURE;
                goto done;
            }

            /* Find HT tlv pointer in buffer and set HTCapInfo */
            tlvbuf_htcap_t *tlv = NULL;
            tlv = (tlvbuf_htcap_t *) (buffer + tlv_offset_11n);
            tlv->ht_cap.ht_cap_info =
                DEFAULT_HT_CAP_VALUE & ~HT_CAP_CONFIG_MASK;
            tlv->ht_cap.ht_cap_info |=
                (t_u16) A2HEXDECIMAL(args[1]) & HT_CAP_CONFIG_MASK;
            tlv->ht_cap.ht_cap_info = uap_cpu_to_le16(tlv->ht_cap.ht_cap_info);
        }
        if (strcmp(args[0], "AMPDU") == 0) {
            if (enable_11n <= 0) {
                printf
                    ("ERR: Enable11n parameter should be set before AMPDU.\n");
                ret = UAP_FAILURE;
                goto done;
            }
            if ((IS_HEX_OR_DIGIT(args[1]) == UAP_FAILURE) ||
                ((A2HEXDECIMAL(args[1])) > AMPDU_CONFIG_MASK)) {
                printf("ERR: Invalid AMPDU value\n");
                ret = UAP_FAILURE;
                goto done;
            }

            /* Find HT tlv pointer in buffer and set AMPDU */
            tlvbuf_htcap_t *tlv = NULL;
            tlv = (tlvbuf_htcap_t *) (buffer + tlv_offset_11n);
            tlv->ht_cap.ampdu_param =
                (t_u8) A2HEXDECIMAL(args[1]) & AMPDU_CONFIG_MASK;
        }
#if DEBUG
        if (cmd_len != 0) {
            hexdump("Command Buffer", (void *) cmd_buf, cmd_len, ' ');
        }
#endif
    }
  done:
    fclose(config_file);
    if (buffer)
        free(buffer);
    if (line)
        free(line);
    return ret;
}

/**
 *  @brief Get band from current channel  
 *  @param         band
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
static int
get_band(int *band)
{
    apcmdbuf_sys_configure *cmd_buf = NULL;
    tlvbuf_channel_config *tlv = NULL;
    t_u8 *buffer = NULL;
    t_u16 cmd_len;
    int ret = UAP_SUCCESS;

    /* Initialize the command length */
    cmd_len = sizeof(apcmdbuf_sys_configure) + sizeof(tlvbuf_channel_config);

    /* Initialize the command buffer */
    buffer = (t_u8 *) malloc(cmd_len);

    if (!buffer) {
        printf("ERR:Cannot allocate buffer for command!\n");
        ret = UAP_FAILURE;
        goto done;
    }
    memset(buffer, 0, cmd_len);

    /* Locate headers */
    cmd_buf = (apcmdbuf_sys_configure *) buffer;
    tlv = (tlvbuf_channel_config *) (buffer + sizeof(apcmdbuf_sys_configure));

    /* Fill the command buffer */
    cmd_buf->cmd_code = APCMD_SYS_CONFIGURE;
    cmd_buf->size = cmd_len;
    cmd_buf->seq_num = 0;
    cmd_buf->result = 0;
    tlv->tag = MRVL_CHANNELCONFIG_TLV_ID;
    tlv->length = 2;
    cmd_buf->action = ACTION_GET;

    endian_convert_tlv_header_out(tlv);
    /* Send the command */
    ret = uap_ioctl((t_u8 *) cmd_buf, &cmd_len, cmd_len);
    endian_convert_tlv_header_in(tlv);

    if (ret == UAP_SUCCESS) {
        /* Verify response */
        if ((cmd_buf->cmd_code != (APCMD_SYS_CONFIGURE | APCMD_RESP_CHECK)) ||
            (tlv->tag != MRVL_CHANNELCONFIG_TLV_ID)) {
            printf("ERR:Corrupted response! cmd_code=%x, Tlv->tag=%x\n",
                   cmd_buf->cmd_code, tlv->tag);
            free(buffer);
            ret = UAP_FAILURE;
            goto done;
        }
        if (cmd_buf->result == CMD_SUCCESS) {
            printf("Channel = %d\n", tlv->chan_number);
            if (tlv->chan_number > MAX_CHANNELS_BG)
                *band = BAND_A;
            else
                *band = BAND_B | BAND_G;
        } else {
            printf("ERR:Could not get band!\n");
            ret = UAP_FAILURE;
            goto done;
        }
    } else {
        printf("ERR:Command sending failed!\n");
        ret = UAP_FAILURE;
        goto done;
    }
  done:
    if (buffer)
        free(buffer);
    return ret;
}

/**
 *  @brief Show usage information for the cfg_80211d command
 *
 *  $return         N/A
 */
void
print_apcmd_cfg_80211d_usage(void)
{
    printf("\nUsage: cfg_80211d <state 0/1> <country Country_code> \n");
    return;
}

/**
 *  @brief Show usage information for the uap_stats command
 *
 *  $return         N/A
 */
void
print_apcmd_uap_stats(void)
{
    printf("Usage: uap_stats \n");
    return;
}

/**
 *  SNMP MIB OIDs Table
 */
static oids_table snmp_oids[] = {
    {0x0b, 4, "dot11LocalTKIPMICFailures"},
    {0x0c, 4, "dot11CCMPDecryptErrors"},
    {0x0d, 4, "dot11WEPUndecryptableCount"},
    {0x0e, 4, "dot11WEPICVErrorCount"},
    {0x0f, 4, "dot11DecryptFailureCount"},
    {0x12, 4, "dot11FailedCount"},
    {0x13, 4, "dot11RetryCount"},
    {0x14, 4, "dot11MultipleRetryCount"},
    {0x15, 4, "dot11FrameDuplicateCount"},
    {0x16, 4, "dot11RTSSuccessCount"},
    {0x17, 4, "dot11RTSFailureCount"},
    {0x18, 4, "dot11ACKFailureCount"},
    {0x19, 4, "dot11ReceivedFragmentCount"},
    {0x1a, 4, "dot11MulticastReceivedFrameCount"},
    {0x1b, 4, "dot11FCSErrorCount"},
    {0x1c, 4, "dot11TransmittedFrameCount"},
    {0x1d, 4, "dot11RSNATKIPCounterMeasuresInvoked"},
    {0x1e, 4, "dot11RSNA4WayHandshakeFailures"},
    {0x1f, 4, "dot11MulticastTransmittedFrameCount"}
};

/** 
 *  @brief Get uAP stats
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_uap_stats(int argc, char *argv[])
{
    t_u8 no_of_oids = sizeof(snmp_oids) / sizeof(snmp_oids[0]);
    t_u16 i, j;
    int size;
    apcmdbuf_snmp_mib *cmd_buf = NULL;
    t_u8 *buf = NULL;
    tlvbuf_header *tlv = NULL;
    t_u16 cmd_len = 0;
    t_u16 buf_len = 0;
    int opt;
    t_u16 oid_size, byte2 = 0;
    t_u32 byte4 = 0;
    int ret = UAP_SUCCESS;

    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_apcmd_uap_stats();
            return UAP_SUCCESS;
        }
    }

    argc -= optind;
    argv += optind;
    if (argc) {
        printf("Error: Invalid Input\n");
        print_apcmd_uap_stats();
        return UAP_FAILURE;
    }

    /**  Command Header */
    buf_len += sizeof(apcmdbuf_snmp_mib);

    for (i = 0; i < no_of_oids; i++) {
        /** 
         * Size of Oid + Oid_value + Oid_size 
         */
        buf_len += snmp_oids[i].len + sizeof(tlvbuf_header);
    }
    buf = (t_u8 *) malloc(buf_len);
    if (!buf) {
        printf("ERR:Cannot allocate buffer from command!\n");
        return UAP_FAILURE;
    }
    memset(buf, 0, buf_len);

    /* Locate Headers */
    cmd_buf = (apcmdbuf_snmp_mib *) buf;
    cmd_buf->size = buf_len - BUF_HEADER_SIZE;
    cmd_buf->result = 0;
    cmd_buf->seq_num = 0;
    cmd_buf->cmd_code = HostCmd_SNMP_MIB;
    cmd_buf->action = uap_cpu_to_le16(ACTION_GET);

    tlv = (tlvbuf_header *) ((t_u8 *) cmd_buf + sizeof(apcmdbuf_snmp_mib));
    /* Add oid, oid_size and oid_value for each OID */
    for (i = 0; i < no_of_oids; i++) {
        /** Copy Index as Oid */
        tlv->type = uap_cpu_to_le16(snmp_oids[i].type);
        /** Copy its size */
        tlv->len = uap_cpu_to_le16(snmp_oids[i].len);
        /** Next TLV */
        tlv = (tlvbuf_header *) & (tlv->data[snmp_oids[i].len]);
    }
    cmd_len = buf_len;
    ret = uap_ioctl((t_u8 *) cmd_buf, &cmd_len, buf_len);
    if (ret == UAP_SUCCESS) {
        if (cmd_buf->result == CMD_SUCCESS) {
            tlv =
                (tlvbuf_header *) ((t_u8 *) cmd_buf +
                                   sizeof(apcmdbuf_snmp_mib));

            size =
                cmd_buf->size - (sizeof(apcmdbuf_snmp_mib) - BUF_HEADER_SIZE);

            while ((unsigned int) size >= sizeof(tlvbuf_header)) {
                tlv->type = uap_le16_to_cpu(tlv->type);
                for (i = 0; i < no_of_oids; i++) {
                    if (snmp_oids[i].type == tlv->type) {
                        printf("%s: ", snmp_oids[i].name);
                        break;
                    }
                }
                oid_size = uap_le16_to_cpu(tlv->len);
                switch (oid_size) {
                case 1:
                    printf("%d", (unsigned int) tlv->data[0]);
                    break;
                case 2:
                    memcpy(&byte2, &tlv->data[0], sizeof(oid_size));
                    printf("%d", (unsigned int) uap_le16_to_cpu(byte2));
                    break;
                case 4:
                    memcpy(&byte4, &tlv->data[0], sizeof(oid_size));
                    printf("%ld", (unsigned long) uap_le32_to_cpu(byte4));
                    break;
                default:
                    for (j = 0; j < oid_size; j++) {
                        printf("%d ", (t_u8) tlv->data[j]);
                    }
                    break;
                }
                /** Next TLV */
                tlv = (tlvbuf_header *) & (tlv->data[oid_size]);
                size -= (sizeof(tlvbuf_header) + oid_size);
                size = (size > 0) ? size : 0;
                printf("\n");
            }

        } else {
            printf("ERR:Command Response incorrect!\n");
            ret = UAP_FAILURE;
        }
    } else {
        printf("ERR:Command sending failed!\n");
    }
    free(buf);
    return ret;
}

/**
 *  @brief parser for sys_cfg_80211d input 
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @param output   Stores indexes for "state, country"
 *                  arguments
 *
 *  @return         NA
 *
 */
void
parse_input_80211d(int argc, char **argv, int output[2][2])
{
    int i, j, k = 0;
    char *keywords[2] = { "state", "country" };

    for (i = 0; i < 2; i++)
        output[i][0] = -1;

    for (i = 0; i < argc; i++) {
        for (j = 0; j < 2; j++) {
            if (strcmp(argv[i], keywords[j]) == 0) {
                output[j][1] = output[j][0] = i;
                k = j;
                break;
            }
        }
        output[k][1] += 1;
    }
}

/**
 *  @brief Set/Get 802.11D country information 
 *
 *  Usage: cfg_80211d state country_code 
 *  
 *  State 0 or 1
 *  
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_cfg_80211d(int argc, char *argv[])
{
    apcmdbuf_cfg_80211d *cmd_buf = NULL;
    ieeetypes_subband_set_t *subband = NULL;
    t_u8 *buf = NULL;
    t_u16 cmd_len;
    t_u16 buf_len;
    int output[2][2];
    int ret = UAP_SUCCESS;
    int opt;
    int i, j;
    t_u8 state = 0;
    char country[4] = { ' ', ' ', 0, 0 };
    t_u8 sflag = 0, cflag = 0;
    ieeetypes_subband_set_t sub_bands[MAX_SUB_BANDS];
    t_u8 no_of_sub_band = 0;
    int band;

    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_apcmd_cfg_80211d_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    if (argc) {
        /** SET */
        parse_input_80211d(argc, argv, output);

        /** State */
        if ((output[0][0] != -1) && (output[0][1] > output[0][0])) {
            if ((output[0][1] - output[0][0]) != 2) {
                printf("ERR: Invalid state inputs\n");
                print_apcmd_cfg_80211d_usage();
                return UAP_FAILURE;
            }

            if (IS_HEX_OR_DIGIT(argv[output[0][0] + 1]) == UAP_FAILURE) {
                printf("ERR: valid input for state are 0 or 1\n");
                print_apcmd_cfg_80211d_usage();
                return UAP_FAILURE;
            }
            state = (t_u8) A2HEXDECIMAL(argv[output[0][0] + 1]);

            if ((state != 0) && (state != 1)) {
                printf("ERR: valid input for state are 0 or 1 \n");
                print_apcmd_cfg_80211d_usage();
                return UAP_FAILURE;
            }
            sflag = 1;
        }

        /** Country */
        if ((output[1][0] != -1) && (output[1][1] > output[1][0])) {
            if ((output[1][1] - output[1][0]) > 2) {
                printf("ERR: Invalid country inputs\n");
                print_apcmd_cfg_80211d_usage();
                return UAP_FAILURE;
            }
            if ((strlen(argv[output[1][0] + 1]) > 3) ||
                (strlen(argv[output[1][0] + 1]) == 0)) {
                print_apcmd_cfg_80211d_usage();
                return UAP_FAILURE;
            }
            /* Only 2 characters of country code are copied here as
               indoor/outdoor conditions are not handled in domain file */
            strncpy(country, argv[output[1][0] + 1], 2);

            for (i = 0; (unsigned int) i < strlen(country); i++) {
                if ((country[i] < 'A') || (country[i] > 'z')) {
                    printf("Invalid Country Code\n");
                    print_apcmd_cfg_80211d_usage();
                    return UAP_FAILURE;
                }
                if (country[i] > 'Z')
                    country[i] = country[i] - 'a' + 'A';
            }

            cflag = 1;
            if (!get_band(&band)) {
                printf("ERR:couldn't get band from channel!\n");
                return UAP_FAILURE;
            }
           /** Get domain information from the file */
            no_of_sub_band = parse_domain_file(country, band, sub_bands);
            if (no_of_sub_band == UAP_FAILURE) {
                printf("Parsing Failed\n");
                return UAP_FAILURE;
            }
        }
    }

    if (argc && !cflag && !sflag) {
        printf("ERR: Invalid input\n");
        print_apcmd_cfg_80211d_usage();
        return UAP_FAILURE;
    }

    if (sflag && !cflag) {
        /**
         * Update MIB only and return
         */
        if (sg_snmp_mib(ACTION_SET, OID_80211D_ENABLE, sizeof(state), &state) ==
            UAP_SUCCESS) {
            printf("802.11d %sd \n", state ? "enable" : "disable");
        }
        return UAP_SUCCESS;
    }

    buf_len = sizeof(apcmdbuf_cfg_80211d);

    if (cflag) {
        buf_len += no_of_sub_band * sizeof(ieeetypes_subband_set_t);
    } else { /** Get */
        buf_len += MAX_SUB_BANDS * sizeof(ieeetypes_subband_set_t);
    }

    buf = (t_u8 *) malloc(buf_len);
    if (!buf) {
        printf("ERR:Cannot allocate buffer from command!\n");
        return UAP_FAILURE;
    }
    memset(buf, 0, buf_len);
    /* Locate headers */
    cmd_buf = (apcmdbuf_cfg_80211d *) buf;
    cmd_len = argc ? buf_len :
                 /** Set */
        (sizeof(apcmdbuf_cfg_80211d) - sizeof(domain_param_t)); /** Get */

    cmd_buf->size = cmd_len - BUF_HEADER_SIZE;
    cmd_buf->result = 0;
    cmd_buf->seq_num = 0;
    cmd_buf->action = argc ? ACTION_SET : ACTION_GET;
    cmd_buf->action = uap_cpu_to_le16(cmd_buf->action);
    cmd_buf->cmd_code = HostCmd_CMD_802_11D_DOMAIN_INFO;

    if (cflag) {
        /* third character of country code is copied here as indoor/outdoor
           condition is not supported in domain file */
        strcpy(country, argv[output[1][0] + 1]);
        for (i = 0; (unsigned int) i < strlen(country); i++) {
            if ((country[i] < 'A') || (country[i] > 'z')) {
                printf("Invalid Country Code\n");
                print_apcmd_cfg_80211d_usage();
                return UAP_FAILURE;
            }
            if (country[i] > 'Z')
                country[i] = country[i] - 'a' + 'A';
        }
        cmd_buf->domain.tag = uap_cpu_to_le16(TLV_TYPE_DOMAIN);
        cmd_buf->domain.length = uap_cpu_to_le16(sizeof(domain_param_t)
                                                 - BUF_HEADER_SIZE
                                                 +
                                                 (no_of_sub_band *
                                                  sizeof
                                                  (ieeetypes_subband_set_t)));

        memset(cmd_buf->domain.country_code, ' ',
               sizeof(cmd_buf->domain.country_code));
        memcpy(cmd_buf->domain.country_code, country, strlen(country));
        memcpy(cmd_buf->domain.subband, sub_bands,
               no_of_sub_band * sizeof(ieeetypes_subband_set_t));
    }

    /* Send the command */
    ret = uap_ioctl((t_u8 *) cmd_buf, &cmd_len, buf_len);
    if (ret == UAP_SUCCESS) {
        if (cmd_buf->result == CMD_SUCCESS) {
            if (argc) {
                printf("Set executed successfully\n");
                if (sflag) {
                    if (sg_snmp_mib
                        (ACTION_SET, OID_80211D_ENABLE, sizeof(state),
                         &state) == UAP_SUCCESS) {
                        printf("802.11d %sd \n", state ? "enable" : "disable");
                    }
                }
            } else {
                j = uap_le16_to_cpu(cmd_buf->domain.length);
                if (sg_snmp_mib
                    (ACTION_GET, OID_80211D_ENABLE, sizeof(state), &state)
                    == UAP_SUCCESS) {
                    printf("State = %sd\n", state ? "enable" : "disable");
                }

                if (cmd_buf->domain.country_code[0] ||
                    cmd_buf->domain.country_code[1] ||
                    cmd_buf->domain.country_code[2]) {
                    printf("Country string = %c%c%c",
                           cmd_buf->domain.country_code[0],
                           cmd_buf->domain.country_code[1],
                           cmd_buf->domain.country_code[2]);
                    j -= sizeof(cmd_buf->domain.country_code);
                    subband =
                        (ieeetypes_subband_set_t *) cmd_buf->domain.subband;
                    printf("\nSub-band info=");
                    printf("\t(1st, #chan, MAX-power) \n");
                    for (i = 0; i < (j / 3); i++) {
                        printf("\t\t(%d, \t%d, \t%d dbm)\n",
                               subband->first_chan, subband->no_of_chan,
                               subband->max_tx_pwr);
                        subband++;
                    }
                }
            }
        } else {
            printf("ERR:Command Response incorrect!\n");
            if (argc)
                printf("11d info is allowed to set only before bss start.\n");
            ret = UAP_FAILURE;
        }
    } else {
        printf("ERR:Command sending failed!\n");
    }
    free(buf);
    return ret;
}

/** 
 *  @brief Creates a sys_config request and sends to the driver
 *
 *  Usage: "Usage : sys_config [CONFIG_FILE]"
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_sys_config(int argc, char *argv[])
{
    apcmdbuf_sys_configure *cmd_buf = NULL;
    t_u8 *buf = NULL;
    t_u16 cmd_len;
    t_u16 buf_len;
    int ret = UAP_SUCCESS;
    int opt;
    char **argv_dummy = NULL;
    ps_mgmt pm;

    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_sys_config_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    /* Check arguments */
    if (argc > 1) {
        printf("ERR:Too many arguments.\n");
        print_sys_config_usage();
        return UAP_FAILURE;
    }
    if (argc == 1) {
        /* Read profile and send command to firmware */
        ret = apcmd_sys_config_profile(argc, argv);
        return ret;
    }

    /** Query AP's setting */
    buf_len = MRVDRV_SIZE_OF_CMD_BUFFER;

    /* Alloc buf for command */
    buf = (t_u8 *) malloc(buf_len);

    if (!buf) {
        printf("ERR:Cannot allocate buffer from command!\n");
        return UAP_FAILURE;
    }
    memset(buf, 0, buf_len);

    /* Locate headers */
    cmd_len = sizeof(apcmdbuf_sys_configure);
    cmd_buf = (apcmdbuf_sys_configure *) buf;

    /* Fill the command buffer */
    cmd_buf->cmd_code = APCMD_SYS_CONFIGURE;
    cmd_buf->size = cmd_len - BUF_HEADER_SIZE;
    cmd_buf->seq_num = 0;
    cmd_buf->result = 0;

    /* Send the command */
    ret = uap_ioctl((t_u8 *) cmd_buf, &cmd_len, buf_len);

    /* Process response */
    if (ret == UAP_SUCCESS) {
        /* Verify response */
        if (cmd_buf->cmd_code != (APCMD_SYS_CONFIGURE | APCMD_RESP_CHECK)) {
            printf("ERR:Corrupted response!\n");
            free(buf);
            return UAP_FAILURE;
        }
        /* Print response */
        if (cmd_buf->result == CMD_SUCCESS) {
            printf("AP settings:\n");
            print_tlv(buf + sizeof(apcmdbuf_sys_configure),
                      cmd_buf->size - sizeof(apcmdbuf_sys_configure) +
                      BUF_HEADER_SIZE);
            if (apcmd_addbapara(1, argv_dummy) == UAP_FAILURE) {
                printf("Couldn't get ADDBA parameters\n");
                return UAP_FAILURE;
            }
            if (apcmd_aggrpriotbl(1, argv_dummy) == UAP_FAILURE) {
                printf("Couldn't get AMSDU/AMPDU priority table\n");
                return UAP_FAILURE;
            }
            if (apcmd_addbareject(1, argv_dummy) == UAP_FAILURE) {
                printf("Couldn't get ADDBA reject table\n");
                return UAP_FAILURE;
            }
            printf("\n802.11D setting:\n");
            if (apcmd_cfg_80211d(1, argv_dummy) == UAP_FAILURE) {
                return UAP_FAILURE;
            }
        } else {
            printf("ERR:Could not retrieve system configure\n");
        }
    } else {
        printf("ERR:Command sending failed!\n");
    }
    free(buf);
    memset(&pm, 0, sizeof(ps_mgmt));
    ret = send_power_mode_ioctl(&pm, 0);
    return ret;
}

/** 
 *  @brief Checks current system configuration
 *
 *  @param buf     pointer to TLV buffer
 *  @param len     TLV buffer length
 *  @return        UAP_SUCCESS/UAP_FAILURE
 */
int
check_sys_config(t_u8 * buf, t_u16 len)
{
    tlvbuf_header *pcurrent_tlv = (tlvbuf_header *) buf;
    int tlv_buf_left = len;
    t_u16 tlv_type;
    t_u16 tlv_len;
    tlvbuf_channel_config *channel_tlv = NULL;
    tlvbuf_channel_list *chnlist_tlv = NULL;
    tlvbuf_pwk_cipher *pwk_cipher_tlv = NULL;
    tlvbuf_gwk_cipher *gwk_cipher_tlv = NULL;
    tlvbuf_auth_mode *auth_tlv = NULL;
    tlvbuf_protocol *protocol_tlv = NULL;
    tlvbuf_wpa_passphrase *passphrase_tlv = NULL;
    tlvbuf_tx_data_rate *tx_data_rate_tlv = NULL;
    tlvbuf_mcbc_data_rate *mcbc_data_rate_tlv = NULL;
    t_u16 protocol = 0;
    t_u16 tx_data_rate = 0;
    t_u16 mcbc_data_rate = 0;
    t_u8 acs_mode_enabled = 0;
    t_u8 state_80211d = 0;
    tlvbuf_htcap_t *ht_cap_tlv = NULL;
    t_u16 enable_40Mhz = 0;
    t_u8 secondary_ch_set = 0;
    HTCap_t htcap;
    channel_list *pchan_list;
    tlvbuf_tx_power *txpower_tlv = NULL;
    int i = 0, ret = UAP_SUCCESS;
    int pairwise_cipher_wpa = -1;
    int pairwise_cipher_wpa2 = -1;
    int group_cipher = -1;
    int chan_list_len = 0;

    ret =
        sg_snmp_mib(ACTION_GET, OID_80211D_ENABLE, sizeof(state_80211d),
                    &state_80211d);

    while (tlv_buf_left >= (int) sizeof(tlvbuf_header) && (ret != UAP_FAILURE)) {
        tlv_type = *(t_u8 *) & pcurrent_tlv->type;
        tlv_type |= (*((t_u8 *) & pcurrent_tlv->type + 1) << 8);
        tlv_len = *(t_u8 *) & pcurrent_tlv->len;
        tlv_len |= (*((t_u8 *) & pcurrent_tlv->len + 1) << 8);
        if ((sizeof(tlvbuf_header) + tlv_len) > (unsigned int) tlv_buf_left) {
            printf("wrong tlv: tlv_len=%d, tlv_buf_left=%d\n", tlv_len,
                   tlv_buf_left);
            break;
        }
        switch (tlv_type) {
        case MRVL_CHANNELCONFIG_TLV_ID:
            channel_tlv = (tlvbuf_channel_config *) pcurrent_tlv;
            /* Block channels 12, 13, 14 if 11d is not enabled or country code
               is not JP */
            if ((!state_80211d) && (channel_tlv->chan_number <= MAX_CHANNELS_BG)
                && (channel_tlv->chan_number > DEFAULT_MAX_VALID_CHANNLEL_BG)) {
                printf("ERR: Invalid channel in 2.4GHz band!\n");
                return UAP_FAILURE;
            }
            if (!(channel_tlv->band_config_type & BAND_CONFIG_ACS_MODE)) {
                if (state_80211d) {
                    if (check_channel_validity_11d(channel_tlv->chan_number) ==
                        UAP_FAILURE)
                        return UAP_FAILURE;
                }
            } else {
                acs_mode_enabled = 1;
            }

            channel_tlv->band_config_type &= 0x30;
            secondary_ch_set = channel_tlv->band_config_type;
            break;
        case MRVL_CHANNELLIST_TLV_ID:
            chnlist_tlv = (tlvbuf_channel_list *) pcurrent_tlv;
            pchan_list = (channel_list *) & (chnlist_tlv->chan_list);
            if (tlv_len % sizeof(channel_list)) {
                printf("ERR:Invalid Scan channel list TLV!\n");
                return UAP_FAILURE;
            }
            chan_list_len = tlv_len / sizeof(channel_list);

            for (i = 0; i < chan_list_len; i++) {
                /* Block channels 12, 13, 14 if 11d is not enabled or country
                   code is not JP */
                if ((!state_80211d) &&
                    (pchan_list->chan_number <= MAX_CHANNELS_BG)
                    && (pchan_list->chan_number >
                        DEFAULT_MAX_VALID_CHANNLEL_BG)) {
                    printf("ERR: Invalid scan channel in 2.4 GHz band!\n");
                    return UAP_FAILURE;
                }
                pchan_list++;
            }
            break;
        case MRVL_TX_POWER_TLV_ID:
            txpower_tlv = (tlvbuf_tx_power *) pcurrent_tlv;
            if (state_80211d) {
                if (check_tx_pwr_validity_11d(txpower_tlv->tx_power_dbm) ==
                    UAP_FAILURE)
                    return UAP_FAILURE;
            }
            break;
        case MRVL_CIPHER_PWK_TLV_ID:
            pwk_cipher_tlv = (tlvbuf_pwk_cipher *) pcurrent_tlv;
            pwk_cipher_tlv->protocol =
                uap_le16_to_cpu(pwk_cipher_tlv->protocol);
            if (pwk_cipher_tlv->protocol == PROTOCOL_WPA)
                pairwise_cipher_wpa = pwk_cipher_tlv->pairwise_cipher;
            else if (pwk_cipher_tlv->protocol == PROTOCOL_WPA2)
                pairwise_cipher_wpa2 = pwk_cipher_tlv->pairwise_cipher;
            break;
        case MRVL_CIPHER_GWK_TLV_ID:
            gwk_cipher_tlv = (tlvbuf_gwk_cipher *) pcurrent_tlv;
            group_cipher = gwk_cipher_tlv->group_cipher;
            break;
        case MRVL_AUTH_TLV_ID:
            auth_tlv = (tlvbuf_auth_mode *) pcurrent_tlv;
            break;
        case MRVL_PROTOCOL_TLV_ID:
            protocol_tlv = (tlvbuf_protocol *) pcurrent_tlv;
            protocol = uap_le16_to_cpu(protocol_tlv->protocol);
            break;
        case MRVL_WPA_PASSPHRASE_TLV_ID:
            passphrase_tlv = (tlvbuf_wpa_passphrase *) pcurrent_tlv;
            break;
        case HT_CAPABILITY_TLV_ID:
            ht_cap_tlv = (tlvbuf_htcap_t *) pcurrent_tlv;
            ht_cap_tlv->ht_cap.ht_cap_info =
                uap_le16_to_cpu(ht_cap_tlv->ht_cap.ht_cap_info);
            if (ht_cap_tlv->ht_cap.supported_mcs_set[0])
                enable_40Mhz =
                    IS_11N_40MHZ_ENABLED(ht_cap_tlv->ht_cap.ht_cap_info);
            break;
        case MRVL_TX_DATA_RATE_TLV_ID:
            tx_data_rate_tlv = (tlvbuf_tx_data_rate *) pcurrent_tlv;
            tx_data_rate = uap_le16_to_cpu(tx_data_rate_tlv->tx_data_rate);
            if (tx_data_rate &&
                (is_tx_rate_valid((t_u8) tx_data_rate) != UAP_SUCCESS)) {
                printf("ERR: Invalid Tx Data Rate \n");
                return UAP_FAILURE;
            }
            break;
        case MRVL_MCBC_DATA_RATE_TLV_ID:
            mcbc_data_rate_tlv = (tlvbuf_mcbc_data_rate *) pcurrent_tlv;
            mcbc_data_rate = uap_le16_to_cpu(mcbc_data_rate_tlv->mcbc_datarate);
            if (mcbc_data_rate &&
                (is_mcbc_rate_valid((t_u8) mcbc_data_rate) != UAP_SUCCESS)) {
                printf("ERR: Invalid MCBC Data Rate \n");
                return UAP_FAILURE;
            }
            break;
        }
        tlv_buf_left -= (sizeof(tlvbuf_header) + tlv_len);
        pcurrent_tlv = (tlvbuf_header *) (pcurrent_tlv->data + tlv_len);
    }
    if ((auth_tlv->auth_mode == 1) && (protocol != PROTOCOL_STATIC_WEP)) {
        printf
            ("ERR:Shared key authentication is not allowed for Open/WPA/WPA2/Mixed mode\n");
        return UAP_FAILURE;
    }
    if (((protocol == PROTOCOL_WPA) || (protocol == PROTOCOL_WPA2)
         || (protocol == PROTOCOL_WPA2_MIXED)) && !(passphrase_tlv->length)) {
        printf("ERR:Passphrase must be set for WPA/WPA2/Mixed mode\n");
        return UAP_FAILURE;
    }
    if ((protocol == PROTOCOL_WPA) || (protocol == PROTOCOL_WPA2_MIXED)) {
        if (is_cipher_valid(pairwise_cipher_wpa, group_cipher) != UAP_SUCCESS) {
            printf
                ("ERR:Wrong group cipher and WPA pairwise cipher combination!\n");
            return UAP_FAILURE;
        }
    }
    if ((protocol == PROTOCOL_WPA2) || (protocol == PROTOCOL_WPA2_MIXED)) {
        if (is_cipher_valid(pairwise_cipher_wpa2, group_cipher) != UAP_SUCCESS) {
            printf
                ("ERR:Wrong group cipher and WPA2 pairwise cipher combination!\n");
            return UAP_FAILURE;
        }
    }
    /* For protocol = Mixed, 11n enabled, only allow TKIP cipher for WPA
       protocol, not for WPA2 */
    if ((protocol == PROTOCOL_WPA2_MIXED) &&
        (pairwise_cipher_wpa2 == CIPHER_TKIP)) {
        memset(&htcap, 0, sizeof(htcap));
        if (UAP_SUCCESS == get_sys_cfg_11n(&htcap)) {
            if (htcap.supported_mcs_set[0]) {
                printf
                    ("ERR: WPA2 pairwise cipher cannot be TKIP when AP operates in 802.11n Mixed mode.\n");
                return UAP_FAILURE;
            }
        }
    }
    if (state_80211d && acs_mode_enabled) {
        pchan_list = (channel_list *) & (chnlist_tlv->chan_list);
        for (i = 0; i < chan_list_len; i++) {
            if (check_channel_validity_11d(pchan_list->chan_number) ==
                UAP_FAILURE)
                return UAP_FAILURE;
            pchan_list++;
        }
    }
    if (!acs_mode_enabled && enable_40Mhz && !secondary_ch_set) {
        printf("ERR:Secondary channel should be set when 40Mhz is enabled!\n");
        return UAP_FAILURE;
    }
    return ret;
}

/** 
 *  @brief Send read/write command along with register details to the driver
 *  @param reg      Reg type
 *  @param offset   Pointer to register offset string
 *  @param strvalue Pointer to value string
 *  @return         UAP_SUCCESS or UAP_FAILURE
 */
int
apcmd_regrdwr_process(int reg, t_s8 * offset, t_s8 * strvalue)
{
    apcmdbuf_reg_rdwr *cmd_buf = NULL;
    t_u8 *buf = NULL;
    t_u16 cmd_len;
    t_u16 buf_len;
    int ret = UAP_SUCCESS;
    t_s8 *whichreg;

    buf_len = sizeof(apcmdbuf_reg_rdwr);

    /* Alloc buf for command */
    buf = (t_u8 *) malloc(buf_len);

    if (!buf) {
        printf("ERR:Cannot allocate buffer from command!\n");
        return UAP_FAILURE;
    }
    memset(buf, 0, buf_len);

    /* Locate headers */
    cmd_len = sizeof(apcmdbuf_reg_rdwr);
    cmd_buf = (apcmdbuf_reg_rdwr *) buf;

    /* Fill the command buffer */
    cmd_buf->size = cmd_len - BUF_HEADER_SIZE;
    cmd_buf->seq_num = 0;
    cmd_buf->result = 0;

    switch (reg) {
    case CMD_MAC:
        whichreg = "MAC";
        cmd_buf->cmd_code = HostCmd_CMD_MAC_REG_ACCESS;
        break;
    case CMD_BBP:
        whichreg = "BBP";
        cmd_buf->cmd_code = HostCmd_CMD_BBP_REG_ACCESS;
        break;
    case CMD_RF:
        cmd_buf->cmd_code = HostCmd_CMD_RF_REG_ACCESS;
        whichreg = "RF";
        break;
    default:
        printf("Invalid register set specified.\n");
        free(buf);
        return UAP_FAILURE;
    }
    if (strvalue) {
        cmd_buf->action = 1;    // WRITE
    } else {
        cmd_buf->action = 0;    // READ
    }
    cmd_buf->action = uap_cpu_to_le16(cmd_buf->action);
    cmd_buf->offset = A2HEXDECIMAL(offset);
    cmd_buf->offset = uap_cpu_to_le16(cmd_buf->offset);
    if (strvalue) {
        cmd_buf->value = A2HEXDECIMAL(strvalue);
        cmd_buf->value = uap_cpu_to_le32(cmd_buf->value);
    }

    /* Send the command */
    ret = uap_ioctl((t_u8 *) cmd_buf, &cmd_len, buf_len);

    /* Process response */
    if (ret == UAP_SUCCESS) {
        if (cmd_buf->result == CMD_SUCCESS) {
            printf("Successfully executed the command\n");
            printf("%s[0x%04hx] = 0x%08lx\n",
                   whichreg, uap_le16_to_cpu(cmd_buf->offset),
                   uap_le32_to_cpu(cmd_buf->value));
        } else {
            printf("ERR:Command sending failed!\n");
            free(buf);
            return UAP_FAILURE;
        }
    } else {
        printf("ERR:Command sending failed!\n");
        free(buf);
        return UAP_FAILURE;
    }

    free(buf);
    return UAP_SUCCESS;
}

/**
 *  @brief Send read command for EEPROM 
 *
 *  Usage: "Usage : rdeeprom <offset> <byteCount>"
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_read_eeprom(int argc, char *argv[])
{
    apcmdbuf_eeprom_access *cmd_buf = NULL;
    t_u8 *buf = NULL;
    t_u16 cmd_len;
    t_u16 buf_len;
    t_u16 byte_count, offset;
    int ret = UAP_SUCCESS;
    int opt;
    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_apcmd_read_eeprom_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    /* Check arguments */
    if (!argc || (argc && is_input_valid(RDEEPROM, argc, argv) != UAP_SUCCESS)) {
        print_apcmd_read_eeprom_usage();
        return UAP_FAILURE;
    }
    offset = A2HEXDECIMAL(argv[0]);
    byte_count = A2HEXDECIMAL(argv[1]);

    buf_len = sizeof(apcmdbuf_eeprom_access) + MAX_EEPROM_LEN;
    buf = (t_u8 *) malloc(buf_len);
    if (!buf) {
        printf("ERR:Cannot allocate buffer from command!\n");
        return UAP_FAILURE;
    }
    memset(buf, 0, buf_len);

    /* Locate headers */
    cmd_buf = (apcmdbuf_eeprom_access *) buf;
    cmd_len = sizeof(apcmdbuf_eeprom_access);

    cmd_buf->size = sizeof(apcmdbuf_eeprom_access) - BUF_HEADER_SIZE;
    cmd_buf->result = 0;
    cmd_buf->seq_num = 0;
    cmd_buf->action = 0;

    cmd_buf->cmd_code = HostCmd_EEPROM_ACCESS;
    cmd_buf->offset = uap_cpu_to_le16(offset);
    cmd_buf->byte_count = uap_cpu_to_le16(byte_count);

    /* Send the command */
    ret = uap_ioctl((t_u8 *) cmd_buf, &cmd_len, buf_len);

    /* Process response */
    if (ret == UAP_SUCCESS) {
        if (cmd_buf->result == CMD_SUCCESS) {
            printf("Successfully executed the command\n");
            byte_count = uap_le16_to_cpu(cmd_buf->byte_count);
            offset = uap_le16_to_cpu(cmd_buf->offset);
            hexdump_data("EEPROM", (void *) cmd_buf->value, byte_count, ' ');
        } else {
            printf("ERR:Command Response incorrect!\n");
            ret = UAP_FAILURE;
        }
    } else {
        printf("ERR:Command sending failed!\n");
    }

    free(buf);
    return ret;
}

/**
 *  @brief Show usage information for the regrdwr command
 *  command
 *
 *  $return         N/A
 */
void
print_regrdwr_usage(void)
{
    printf("\nUsage : uaputl.exe regrdwr <TYPE> <OFFSET> [value]\n");
    printf("\nTYPE Options: 1     - read/write MAC register");
    printf("\n              2     - read/write BBP register");
    printf("\n              3     - read/write RF register");
    printf("\n");
    return;

}

/** 
 *  @brief Provides interface to perform read/write operations on regsiters
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_regrdwr(int argc, char *argv[])
{
    int opt;
    int ret = UAP_SUCCESS;
    t_s32 reg;
    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_regrdwr_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    /* Check arguments */
    if ((argc < 2) || (argc > 3)) {
        printf("ERR:wrong arguments.\n");
        print_regrdwr_usage();
        return UAP_FAILURE;
    }
    if ((atoi(argv[0]) != 1) && (atoi(argv[0]) != 2) && (atoi(argv[0]) != 3)) {
        printf("ERR:Illegal register type %s. Must be either '1','2' or '3'.\n",
               argv[0]);
        print_regrdwr_usage();
        return UAP_FAILURE;
    }
    reg = atoi(argv[0]);
    ret = apcmd_regrdwr_process(reg, argv[1], argc > 2 ? argv[2] : NULL);
    return ret;
}

/**
 *    @brief Show usage information for the memaccess command
 *    command
 *    
 *    $return         N/A
 */
void
print_memaccess_usage(void)
{
    printf("\nUsage : uaputl.exe memaccess <ADDRESS> [value]\n");
    printf("\nRead/Write memory location");
    printf("\nADDRESS: Address of the memory location to be read/written");
    printf("\nValue  : Value to be written at that address\n");
    return;
}

/** 
 *  @brief Provides interface to perform read/write memory location
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_memaccess(int argc, char *argv[])
{
    int opt;
    apcmdbuf_mem_access *cmd_buf = NULL;
    t_u8 *buf = NULL;
    t_u16 cmd_len;
    t_u16 buf_len;
    int ret = UAP_SUCCESS;
    t_s8 *address = NULL;
    t_s8 *value = NULL;

    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_memaccess_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    /* Check arguments */
    if ((argc < 1) || (argc > 2)) {
        printf("ERR:wrong arguments.\n");
        print_memaccess_usage();
        return UAP_FAILURE;
    }

    address = argv[0];
    if (argc == 2)
        value = argv[1];

    buf_len = sizeof(apcmdbuf_mem_access);

    /* Alloc buf for command */
    buf = (t_u8 *) malloc(buf_len);

    if (!buf) {
        printf("ERR:Cannot allocate buffer from command!\n");
        return UAP_FAILURE;
    }
    memset(buf, 0, buf_len);
    /* Locate headers */
    cmd_len = sizeof(apcmdbuf_mem_access);
    cmd_buf = (apcmdbuf_mem_access *) buf;

    /* Fill the command buffer */
    cmd_buf->size = cmd_len - BUF_HEADER_SIZE;
    cmd_buf->seq_num = 0;
    cmd_buf->result = 0;
    cmd_buf->cmd_code = HostCmd_CMD_MEM_ACCESS;

    if (value)
        cmd_buf->action = 1;    // WRITE
    else
        cmd_buf->action = 0;    // READ

    cmd_buf->action = uap_cpu_to_le16(cmd_buf->action);
    cmd_buf->address = A2HEXDECIMAL(address);
    cmd_buf->address = uap_cpu_to_le32(cmd_buf->address);

    if (value) {
        cmd_buf->value = A2HEXDECIMAL(value);
        cmd_buf->value = uap_cpu_to_le32(cmd_buf->value);
    }

    /* Send the command */
    ret = uap_ioctl((t_u8 *) cmd_buf, &cmd_len, buf_len);

    /* Process response */
    if (ret == UAP_SUCCESS) {
        if (cmd_buf->result == CMD_SUCCESS) {
            printf("Successfully executed the command\n");
            printf("[0x%04lx] = 0x%08lx\n",
                   uap_le32_to_cpu(cmd_buf->address),
                   uap_le32_to_cpu(cmd_buf->value));
        } else {
            printf("ERR:Command sending failed!\n");
            free(buf);
            return UAP_FAILURE;
        }
    } else {
        printf("ERR:Command sending failed!\n");
        free(buf);
        return UAP_FAILURE;
    }
    free(buf);
    return UAP_SUCCESS;
}

/**
 *    @brief Show usage information for sys_debug command
 *    command
 *    
 *    $return         N/A
 */
void
print_sys_debug_usage(void)
{
    printf("\nUsage : uaputl.exe sys_debug <subcmd> [parameter]\n");
    printf("\nSet/Get debug parameter");
    printf("\nsubcmd: used to set/get debug parameters or set user scan");
    printf("\nparameter  :  parameters for specific subcmd");
    printf("\n		If no [parameter] are given, it return");
    printf("\n		debug parameters for selected subcmd");
    printf("\n\n");
    return;
}

/** 
 *  @brief Creates a sys_debug request and sends to the driver
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_sys_debug(int argc, char *argv[])
{
    apcmdbuf_sys_debug *cmd_buf = NULL;
    t_u8 *buffer = NULL;
    t_u16 cmd_len;
    t_u16 buf_len;
    t_u32 subcmd = 0;
    int ret = UAP_SUCCESS;
    int opt;
    t_s8 *value = NULL;
    t_u32 parameter = 0;
    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_sys_debug_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;
    /* Check arguments */
    if ((argc == 0) || (argc > 2)) {
        printf("ERR:wrong arguments.\n");
        print_sys_debug_usage();
        return UAP_FAILURE;
    } else {
        if (argc == 2) {
            value = argv[1];
            parameter = A2HEXDECIMAL(value);
        }
    }
    subcmd = atoi(argv[0]);
    /* Initialize the command length */
    if (subcmd == DEBUG_SUBCOMMAND_CHANNEL_SCAN) {
        buf_len =
            sizeof(apcmdbuf_sys_debug) +
            MAX_CHANNELS * sizeof(channel_scan_entry_t);
        cmd_len = sizeof(apcmdbuf_sys_debug) - sizeof(debug_config_t);
    } else {
        cmd_len = sizeof(apcmdbuf_sys_debug);
        buf_len = cmd_len;
    }

    /* Initialize the command buffer */
    buffer = (t_u8 *) malloc(buf_len);
    if (!buffer) {
        printf("ERR:Cannot allocate buffer for command!\n");
        return UAP_FAILURE;
    }
    memset(buffer, 0, buf_len);

    /* Locate headers */
    cmd_buf = (apcmdbuf_sys_debug *) buffer;

    /* Fill the command buffer */
    cmd_buf->cmd_code = APCMD_SYS_DEBUG;
    cmd_buf->size = cmd_len - BUF_HEADER_SIZE;
    cmd_buf->seq_num = 0;
    cmd_buf->result = 0;
    cmd_buf->subcmd = subcmd;
    if (subcmd == DEBUG_SUBCOMMAND_CHANNEL_SCAN) {
        cmd_buf->action = ACTION_SET;
    } else {
        if (argc == 1) {
            cmd_buf->action = ACTION_GET;
        } else {
            cmd_buf->action = ACTION_SET;
            if (subcmd == DEBUG_SUBCOMMAND_GMODE) {
                cmd_buf->debug_config.global_debug_mode = (t_u8) parameter;
            } else if (subcmd == DEBUG_SUBCOMMAND_MAJOREVTMASK) {
                cmd_buf->debug_config.debug_major_id_mask = parameter;
                cmd_buf->debug_config.debug_major_id_mask =
                    uap_cpu_to_le32(cmd_buf->debug_config.debug_major_id_mask);
            } else {
                cmd_buf->debug_config.value = uap_cpu_to_le32(parameter);
            }
        }
    }
    cmd_buf->action = uap_cpu_to_le16(cmd_buf->action);
    cmd_buf->subcmd = uap_cpu_to_le32(cmd_buf->subcmd);

    /* Send the command */
    ret = uap_ioctl((t_u8 *) cmd_buf, &cmd_len, buf_len);

    /* Process response */
    if (ret == UAP_SUCCESS) {
        /* Verify response */
        if (cmd_buf->cmd_code != (APCMD_SYS_DEBUG | APCMD_RESP_CHECK)) {
            printf("ERR:Corrupted response! cmd_code=%x\n", cmd_buf->cmd_code);
            free(buffer);
            return UAP_FAILURE;
        }
        /* Print response */
        if (cmd_buf->result == CMD_SUCCESS) {
            if (subcmd == DEBUG_SUBCOMMAND_CHANNEL_SCAN) {
                int i = 0;
                channel_scan_entry_t *cst = NULL;
                if (cmd_buf->size <
                    (sizeof(apcmdbuf_sys_debug) - BUF_HEADER_SIZE)) {
                    printf
                        ("ERR: Invalid command response size, cmd_buf->size = %x\n",
                         cmd_buf->size);
                    free(buffer);
                    return UAP_FAILURE;
                }
                for (i = 0; i < cmd_buf->debug_config.cs_entry.num_channels;
                     i++) {
                    if (i == 0) {
                        printf
                            ("\n------------------------------------------------------");
                        printf("\nChan\tNumAPs\tCCA_Count\tDuration\tWeight");
                        printf
                            ("\n------------------------------------------------------");
                        cst = cmd_buf->debug_config.cs_entry.cst;
                    }
                    printf("\n%d\t%d\t%ld\t\t%ld\t\t%ld", cst->chan_num,
                           cst->num_of_aps,
                           cst->cca_count, cst->duration, cst->channel_weight);
                    cst++;
                }
                printf
                    ("\n------------------------------------------------------\n");
            } else {
                if (argc == 1) {
                    if (subcmd == DEBUG_SUBCOMMAND_GMODE) {
                        printf("globalDebugmode=%d\n",
                               cmd_buf->debug_config.global_debug_mode);
                    } else if (subcmd == DEBUG_SUBCOMMAND_MAJOREVTMASK) {
                        printf("MajorId mask=0x%08lx\n",
                               uap_le32_to_cpu(cmd_buf->
                                               debug_config.debug_major_id_mask));
                    } else {
                        printf("Value = %ld\n", uap_le32_to_cpu
                               (cmd_buf->debug_config.value));
                    }
                } else {
                    printf("set debug parameter successful\n");
                }
            }
        } else {
            if (argc == 1) {
                printf("ERR:Could not get debug parameter!\n");
            } else {
                printf("ERR:Could not set debug parameter!\n");
            }
            ret = UAP_FAILURE;
        }
    } else {
        printf("ERR:Command sending failed!\n");
    }
    if (buffer)
        free(buffer);
    return ret;
}

/**
 *  @brief Show usage information for the bss_config command
 *
 *  $return         N/A
 */
void
print_bss_config_usage(void)
{
    printf("\nUsage : bss_config [CONFIG_FILE]\n");
    printf
        ("\nIf CONFIG_FILE is provided, a 'set' is performed, else a 'get' is performed.\n");
    printf("CONFIG_FILE is file containing all the BSS settings.\n");
    return;
}

/** 
 *  @brief Read the BSS profile and populate structure
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @param bss      Pointer to BSS configuration buffer
 *  @return         UAP_SUCCESS or UAP_FAILURE
 */
int
parse_bss_config(int argc, char *argv[], bss_config_t * bss)
{
    FILE *config_file = NULL;
    char *line = NULL;
    int li = 0;
    char *pos = NULL;
    int arg_num = 0;
    char *args[30];
    int i;
    int is_ap_config = 0;
    int is_ap_mac_filter = 0;
    int keyindex = -1;
    int pwkcipher_wpa = -1;
    int pwkcipher_wpa2 = -1;
    int gwkcipher = -1;
    int protocol = -1;
    int tx_data_rate = -1;
    int mcbc_data_rate = -1;
    int num_rates = 0;
    int found = 0;
    int filter_mac_count = -1;
    int retval = UAP_SUCCESS;
    int chan_mode = 0;
    HTCap_t htcap;
    int enable_11n = -1;

    /* Check if file exists */
    config_file = fopen(argv[0], "r");
    if (config_file == NULL) {
        printf("\nERR:Config file can not open.\n");
        return UAP_FAILURE;
    }
    line = (char *) malloc(MAX_CONFIG_LINE);
    if (!line) {
        printf("ERR:Cannot allocate memory for line\n");
        retval = UAP_FAILURE;
        goto done;
    }
    memset(line, 0, MAX_CONFIG_LINE);

    /* Parse file and process */
    while (config_get_line(line, MAX_CONFIG_LINE, config_file, &li, &pos)) {
#if DEBUG
        uap_printf(MSG_DEBUG, "DBG:Received config line (%d) = %s\n", li, line);
#endif
        arg_num = parse_line(line, args);
#if DEBUG
        uap_printf(MSG_DEBUG, "DBG:Number of arguments = %d\n", arg_num);
        for (i = 0; i < arg_num; i++) {
            uap_printf(MSG_DEBUG, "\tDBG:Argument %d. %s\n", i + 1, args[i]);
        }
#endif
        /* Check for end of AP configurations */
        if (is_ap_config == 1) {
            if (strcmp(args[0], "}") == 0) {
                is_ap_config = 0;
                if (tx_data_rate != -1) {
                    if ((!bss->rates[0]) && (tx_data_rate) &&
                        (is_tx_rate_valid((t_u8) tx_data_rate) !=
                         UAP_SUCCESS)) {
                        printf("ERR: Invalid Tx Data Rate \n");
                        retval = UAP_FAILURE;
                        goto done;
                    }
                    if (bss->rates[0] && tx_data_rate) {
                        for (i = 0; bss->rates[i] != 0; i++) {
                            if ((bss->rates[i] & ~BASIC_RATE_SET_BIT) ==
                                tx_data_rate) {
                                found = 1;
                                break;
                            }
                        }
                        if (!found) {
                            printf("ERR: Invalid Tx Data Rate \n");
                            retval = UAP_FAILURE;
                            goto done;
                        }
                    }

                    /* Set Tx data rate field */
                    bss->tx_data_rate = tx_data_rate;
                }
                if (mcbc_data_rate != -1) {
                    if ((!bss->rates[0]) && (mcbc_data_rate) &&
                        (is_mcbc_rate_valid((t_u8) mcbc_data_rate) !=
                         UAP_SUCCESS)) {
                        printf("ERR: Invalid Tx Data Rate \n");
                        retval = UAP_FAILURE;
                        goto done;
                    }
                    if (bss->rates[0] && mcbc_data_rate) {
                        for (i = 0; bss->rates[i] != 0; i++) {
                            if (bss->rates[i] & BASIC_RATE_SET_BIT) {
                                if ((bss->rates[i] & ~BASIC_RATE_SET_BIT) ==
                                    mcbc_data_rate) {
                                    found = 1;
                                    break;
                                }
                            }
                        }
                        if (!found) {
                            printf("ERR: Invalid MCBC Data Rate \n");
                            retval = UAP_FAILURE;
                            goto done;
                        }
                    }

                    /* Set MCBC data rate field */
                    bss->mcbc_data_rate = mcbc_data_rate;
                }

                if ((protocol == PROTOCOL_WPA2_MIXED) &&
                    ((pwkcipher_wpa < 0) || (pwkcipher_wpa2 < 0))) {
                    printf
                        ("ERR:Both PwkCipherWPA and PwkCipherWPA2 should be defined for Mixed mode.\n");
                    goto done;
                }

                if (((pwkcipher_wpa >= 0) || (pwkcipher_wpa2 >= 0)) &&
                    (gwkcipher >= 0)) {
                    if ((protocol == PROTOCOL_WPA) ||
                        (protocol == PROTOCOL_WPA2_MIXED)) {
                        if (enable_11n != -1) {
                            if (is_cipher_valid_with_11n
                                (pwkcipher_wpa, gwkcipher, protocol,
                                 enable_11n) != UAP_SUCCESS) {
                                printf
                                    ("ERR:Wrong group cipher and WPA pairwise cipher combination!\n");
                                retval = UAP_FAILURE;
                                goto done;
                            }
                        } else
                            if (is_cipher_valid_with_proto
                                (pwkcipher_wpa, gwkcipher,
                                 protocol) != UAP_SUCCESS) {
                            printf
                                ("ERR:Wrong group cipher and WPA pairwise cipher combination!\n");
                            retval = UAP_FAILURE;
                            goto done;
                        }
                    }
                    if ((protocol == PROTOCOL_WPA2) ||
                        (protocol == PROTOCOL_WPA2_MIXED)) {
                        if (enable_11n != -1) {
                            if (is_cipher_valid_with_11n
                                (pwkcipher_wpa2, gwkcipher, protocol,
                                 enable_11n) != UAP_SUCCESS) {
                                printf
                                    ("ERR:Wrong group cipher and WPA2 pairwise cipher combination!\n");
                                retval = UAP_FAILURE;
                                goto done;
                            }
                        } else
                            if (is_cipher_valid_with_proto
                                (pwkcipher_wpa2, gwkcipher,
                                 protocol) != UAP_SUCCESS) {
                            printf
                                ("ERR:Wrong group cipher and WPA2 pairwise cipher combination!\n");
                            retval = UAP_FAILURE;
                            goto done;
                        }
                    }
                    /* Set pairwise and group cipher fields */
                    bss->wpa_cfg.pairwise_cipher_wpa = pwkcipher_wpa;
                    bss->wpa_cfg.pairwise_cipher_wpa2 = pwkcipher_wpa2;
                    bss->wpa_cfg.group_cipher = gwkcipher;
                }
                continue;
            }
        }

        /* Check for beginning of AP configurations */
        if (strcmp(args[0], "ap_config") == 0) {
            is_ap_config = 1;
            continue;
        }

        /* Check for end of AP MAC address filter configurations */
        if (is_ap_mac_filter == 1) {
            if (strcmp(args[0], "}") == 0) {
                is_ap_mac_filter = 0;
                if (bss->filter.mac_count != filter_mac_count) {
                    printf
                        ("ERR:Number of MAC address provided does not match 'Count'\n");
                    retval = UAP_FAILURE;
                    goto done;
                }
                if (bss->filter.filter_mode && (bss->filter.mac_count == 0)) {
                    printf
                        ("ERR:Filter list can not be empty for %s Filter mode\n",
                         (bss->filter.filter_mode ==
                          1) ? "'Allow'" : "'Block'");
                    retval = UAP_FAILURE;
                    goto done;
                }
                continue;
            }
        }

        /* Check for beginning of AP MAC address filter configurations */
        if (strcmp(args[0], "ap_mac_filter") == 0) {
            is_ap_mac_filter = 1;
            bss->filter.mac_count = 0;
            filter_mac_count = 0;
            continue;
        }
        if ((strcmp(args[0], "FilterMode") == 0) && is_ap_mac_filter) {
            if ((ISDIGIT(args[1]) == 0) || (atoi(args[1]) < 0) ||
                (atoi(args[1]) > 2)) {
                printf
                    ("ERR:Illegal FilterMode paramter %d. Must be either '0', '1', or '2'.\n",
                     atoi(args[1]));
                retval = UAP_FAILURE;
                goto done;
            }
            bss->filter.filter_mode = atoi(args[1]);
            continue;
        }
        if ((strcmp(args[0], "Count") == 0) && is_ap_mac_filter) {
            bss->filter.mac_count = atoi(args[1]);
            if ((ISDIGIT(args[1]) == 0) ||
                (bss->filter.mac_count > MAX_MAC_ONESHOT_FILTER)) {
                printf("ERR: Illegal Count parameter.\n");
                retval = UAP_FAILURE;
                goto done;
            }
        }
        if ((strncmp(args[0], "mac_", 4) == 0) && is_ap_mac_filter) {
            if (filter_mac_count < MAX_MAC_ONESHOT_FILTER) {
                if (mac2raw
                    (args[1],
                     bss->filter.mac_list[filter_mac_count]) != UAP_SUCCESS) {
                    printf("ERR: Invalid MAC address %s \n", args[1]);
                    retval = UAP_FAILURE;
                    goto done;
                }
                filter_mac_count++;
            } else {
                printf
                    ("ERR: Filter table can not have more than %d MAC addresses\n",
                     MAX_MAC_ONESHOT_FILTER);
                retval = UAP_FAILURE;
                goto done;
            }
        }

        if (strcmp(args[0], "SSID") == 0) {
            if (arg_num == 1) {
                printf("ERR:SSID field is blank!\n");
                retval = UAP_FAILURE;
                goto done;
            } else {
                if (args[1][0] == '"') {
                    args[1]++;
                }
                if (args[1][strlen(args[1]) - 1] == '"') {
                    args[1][strlen(args[1]) - 1] = '\0';
                }
                if ((strlen(args[1]) > MAX_SSID_LENGTH) ||
                    (strlen(args[1]) == 0)) {
                    printf("ERR:SSID length out of range (%d to %d).\n",
                           MIN_SSID_LENGTH, MAX_SSID_LENGTH);
                    retval = UAP_FAILURE;
                    goto done;
                }
                /* Set SSID field */
                bss->ssid.ssid_len = strlen(args[1]);
                memcpy(bss->ssid.ssid, args[1], bss->ssid.ssid_len);
            }
        }
        if (strcmp(args[0], "BeaconPeriod") == 0) {
            if (is_input_valid(BEACONPERIOD, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set beacon period field */
            bss->beacon_period = (t_u16) atoi(args[1]);
        }
        if (strcmp(args[0], "ChanList") == 0) {
            if (is_input_valid(SCANCHANNELS, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }

            /* Set channel list field */
            if ((arg_num - 1) < MAX_CHANNELS) {
                bss->chan_list.num_of_chan = arg_num - 1;
            } else {
                bss->chan_list.num_of_chan = MAX_CHANNELS;
            }
            for (i = 0; (unsigned int) i < bss->chan_list.num_of_chan; i++) {
                bss->chan_list.chan[i] = (t_u16) atoi(args[i + 1]);
            }
        }
        if (strcmp(args[0], "Channel") == 0) {
            if (is_input_valid(CHANNEL, arg_num - 1, args + 1) != UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set channel field */
            bss->channel = (t_u8) atoi(args[1]);
            if ((arg_num - 1) == 2) {
                chan_mode = atoi(args[2]);
                bss->band_cfg = 0;
                if (chan_mode & BITMAP_ACS_MODE)
                    bss->band_cfg |= BAND_CONFIG_ACS_MODE;
                if (chan_mode & BITMAP_CHANNEL_ABOVE)
                    bss->band_cfg |= SECOND_CHANNEL_ABOVE;
                if (chan_mode & BITMAP_CHANNEL_BELOW)
                    bss->band_cfg |= SECOND_CHANNEL_BELOW;
            } else
                bss->band_cfg = 0;
            if (bss->channel > MAX_CHANNELS_BG)
                bss->band_cfg |= BAND_CONFIG_5GHZ;
        }
        if (strcmp(args[0], "AP_MAC") == 0) {
            int ret;
            if ((ret = mac2raw(args[1], bss->mac_addr)) != UAP_SUCCESS) {
                printf("ERR: %s Address \n",
                       ret == UAP_FAILURE ? "Invalid MAC" : ret ==
                       UAP_RET_MAC_BROADCAST ? "Broadcast" : "Multicast");
                retval = UAP_FAILURE;
                goto done;
            }
        }

        if (strcmp(args[0], "RxAntenna") == 0) {
            if ((ISDIGIT(args[1]) != UAP_SUCCESS) || (atoi(args[1]) < 0) ||
                (atoi(args[1]) > 1)) {
                printf("ERR: Invalid Antenna value\n");
                retval = UAP_FAILURE;
                goto done;
            }
            bss->rx_antenna = atoi(args[1]);
        }

        if (strcmp(args[0], "TxAntenna") == 0) {
            if ((ISDIGIT(args[1]) != UAP_SUCCESS) || (atoi(args[1]) < 0) ||
                (atoi(args[1]) > 1)) {
                printf("ERR: Invalid Antenna value\n");
                retval = UAP_FAILURE;
                goto done;
            }
            bss->tx_antenna = atoi(args[1]);
        }
        if (strcmp(args[0], "Rate") == 0) {
            if (is_input_valid(RATE, arg_num - 1, args + 1) != UAP_SUCCESS) {
                printf("ERR: Invalid Rate input\n");
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set rates field */
            if ((arg_num - 1) < MAX_DATA_RATES) {
                num_rates = arg_num - 1;
            } else {
                num_rates = MAX_DATA_RATES;
            }
            for (i = 0; i < num_rates; i++) {
                bss->rates[i] = (t_u8) A2HEXDECIMAL(args[i + 1]);
            }
        }
        if (strcmp(args[0], "TxPowerLevel") == 0) {
            if (is_input_valid(TXPOWER, arg_num - 1, args + 1) != UAP_SUCCESS) {
                printf("ERR:Invalid TxPowerLevel \n");
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set Tx power level field */
            bss->tx_power_level = (t_u8) atoi(args[1]);
        }
        if (strcmp(args[0], "BroadcastSSID") == 0) {
            if (is_input_valid(BROADCASTSSID, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set broadcast SSID control field */
            bss->bcast_ssid_ctl = (t_u8) atoi(args[1]);
        }
        if (strcmp(args[0], "RTSThreshold") == 0) {
            if (is_input_valid(RTSTHRESH, arg_num - 1, args + 1) != UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set RTS threshold field */
            bss->rts_threshold = (t_u16) atoi(args[1]);
        }
        if (strcmp(args[0], "FragThreshold") == 0) {
            if (is_input_valid(FRAGTHRESH, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set Frag threshold field */
            bss->frag_threshold = (t_u16) atoi(args[1]);
        }
        if (strcmp(args[0], "DTIMPeriod") == 0) {
            if (is_input_valid(DTIMPERIOD, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set DTIM period field */
            bss->dtim_period = (t_u8) atoi(args[1]);
        }
        if (strcmp(args[0], "RSNReplayProtection") == 0) {
            if (is_input_valid(RSNREPLAYPROT, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set RSN replay protection field */
            bss->wpa_cfg.rsn_protection = (t_u8) atoi(args[1]);
        }
        if (strcmp(args[0], "PairwiseUpdateTimeout") == 0) {
            if ((ISDIGIT(args[1]) == 0) || (atoi(args[1]) < 0)) {
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set Pairwise Update Timeout field */
            bss->pairwise_update_timeout = (t_u32) atoi(args[1]);
        }
        if (strcmp(args[0], "PairwiseHandshakeRetries") == 0) {
            if ((ISDIGIT(args[1]) == 0) || (atoi(args[1]) < 0)) {
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set Pairwise Hanshake Retries */
            bss->pwk_retries = (t_u32) atoi(args[1]);
        }
        if (strcmp(args[0], "GroupwiseUpdateTimeout") == 0) {
            if ((ISDIGIT(args[1]) == 0) || (atoi(args[1]) < 0)) {
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set Groupwise Update Timeout field */
            bss->groupwise_update_timeout = (t_u32) atoi(args[1]);
        }
        if (strcmp(args[0], "GroupwiseHandshakeRetries") == 0) {
            if ((ISDIGIT(args[1]) == 0) || (atoi(args[1]) < 0)) {
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set Groupwise Hanshake Retries */
            bss->gwk_retries = (t_u32) atoi(args[1]);
        }
        if (strcmp(args[0], "RadioControl") == 0) {
            if (is_input_valid(RADIOCONTROL, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }
            bss->radio_ctl = (t_u8) atoi(args[1]);
        }
        if (strcmp(args[0], "TxDataRate") == 0) {
            if (is_input_valid(TXDATARATE, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }
            tx_data_rate = (t_u16) A2HEXDECIMAL(args[1]);
        }
        if (strcmp(args[0], "MCBCdataRate") == 0) {
            if (is_input_valid(MCBCDATARATE, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }
            mcbc_data_rate = (t_u16) A2HEXDECIMAL(args[1]);
        }
        if (strcmp(args[0], "PktFwdCtl") == 0) {
            if (is_input_valid(PKTFWD, arg_num - 1, args + 1) != UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set packet forward control field */
            bss->pkt_forward_ctl = (t_u8) atoi(args[1]);
        }
        if (strcmp(args[0], "StaAgeoutTimer") == 0) {
            if (is_input_valid(STAAGEOUTTIMER, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set STA ageout timer field */
            bss->sta_ageout_timer = (t_u32) atoi(args[1]);
        }
        if (strcmp(args[0], "PSStaAgeoutTimer") == 0) {
            if (is_input_valid(PSSTAAGEOUTTIMER, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set PS STA ageout timer field */
            bss->ps_sta_ageout_timer = (t_u32) atoi(args[1]);
        }
        if (strcmp(args[0], "AuthMode") == 0) {
            if (is_input_valid(AUTHMODE, arg_num - 1, args + 1) != UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }
            if ((atoi(args[1]) < 0) || (atoi(args[1]) > 1)) {
                printf
                    ("ERR:Illegal AuthMode parameter. Must be either '0' or '1'.\n");
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set auth mode field */
            bss->auth_mode = (t_u16) atoi(args[1]);
        }
        if (strcmp(args[0], "KeyIndex") == 0) {
            if (arg_num == 1) {
                printf("KeyIndex is blank!\n");
                retval = UAP_FAILURE;
                goto done;
            } else {
                if (ISDIGIT(args[1]) == 0) {
                    printf
                        ("ERR:Illegal KeyIndex parameter. Must be either '0', '1', '2', or '3'.\n");
                    retval = UAP_FAILURE;
                    goto done;
                }
                keyindex = atoi(args[1]);
                if ((keyindex < 0) || (keyindex > 3)) {
                    printf
                        ("ERR:Illegal KeyIndex parameter. Must be either '0', '1', '2', or '3'.\n");
                    retval = UAP_FAILURE;
                    goto done;
                }
                switch (keyindex) {
                case 0:
                    bss->wep_cfg.key0.is_default = 1;
                    break;
                case 1:
                    bss->wep_cfg.key1.is_default = 1;
                    break;
                case 2:
                    bss->wep_cfg.key2.is_default = 1;
                    break;
                case 3:
                    bss->wep_cfg.key3.is_default = 1;
                    break;
                default:
                    break;
                }
            }
        }
        if (strncmp(args[0], "Key_", 4) == 0) {
            if (arg_num == 1) {
                printf("ERR:%s is blank!\n", args[0]);
                retval = UAP_FAILURE;
                goto done;
            } else {
                int key_len = 0;
                if (args[1][0] == '"') {
                    if ((strlen(args[1]) != 2) && (strlen(args[1]) != 7) &&
                        (strlen(args[1]) != 15)) {
                        printf("ERR:Wrong key length!\n");
                        retval = UAP_FAILURE;
                        goto done;
                    }
                    key_len = strlen(args[1]) - 2;
                } else {
                    if ((strlen(args[1]) != 0) && (strlen(args[1]) != 10) &&
                        (strlen(args[1]) != 26)) {
                        printf("ERR:Wrong key length!\n");
                        retval = UAP_FAILURE;
                        goto done;
                    }
                    if (UAP_FAILURE == ishexstring(args[1])) {
                        printf
                            ("ERR:Only hex digits are allowed when key length is 10 or 26\n");
                        retval = UAP_FAILURE;
                        goto done;
                    }
                    key_len = strlen(args[1]) / 2;
                }
                memset(&htcap, 0, sizeof(htcap));
                if (UAP_SUCCESS == get_sys_cfg_11n(&htcap)) {
                    if (htcap.supported_mcs_set[0]) {
                        printf
                            ("ERR: WEP cannot be used when AP operates in 802.11n mode.\n");
                        goto done;
                    }
                }
                /* Set WEP key fields */
                if (strcmp(args[0], "Key_0") == 0) {
                    bss->wep_cfg.key0.key_index = 0;
                    bss->wep_cfg.key0.length = key_len;
                    if (args[1][0] == '"') {
                        memcpy(bss->wep_cfg.key0.key, &args[1][1],
                               strlen(args[1]) - 2);
                    } else {
                        string2raw(args[1], bss->wep_cfg.key0.key);
                    }
                } else if (strcmp(args[0], "Key_1") == 0) {
                    bss->wep_cfg.key1.key_index = 1;
                    bss->wep_cfg.key1.length = key_len;
                    if (args[1][0] == '"') {
                        memcpy(bss->wep_cfg.key1.key, &args[1][1],
                               strlen(args[1]) - 2);
                    } else {
                        string2raw(args[1], bss->wep_cfg.key1.key);
                    }
                } else if (strcmp(args[0], "Key_2") == 0) {
                    bss->wep_cfg.key2.key_index = 2;
                    bss->wep_cfg.key2.length = key_len;
                    if (args[1][0] == '"') {
                        memcpy(bss->wep_cfg.key2.key, &args[1][1],
                               strlen(args[1]) - 2);
                    } else {
                        string2raw(args[1], bss->wep_cfg.key2.key);
                    }
                } else if (strcmp(args[0], "Key_3") == 0) {
                    bss->wep_cfg.key3.key_index = 3;
                    bss->wep_cfg.key3.length = key_len;
                    if (args[1][0] == '"') {
                        memcpy(bss->wep_cfg.key3.key, &args[1][1],
                               strlen(args[1]) - 2);
                    } else {
                        string2raw(args[1], bss->wep_cfg.key3.key);
                    }
                }
            }
        }
        if (strcmp(args[0], "PSK") == 0) {
            if (arg_num == 1) {
                printf("ERR:PSK is blank!\n");
                retval = UAP_FAILURE;
                goto done;
            } else {
                if (args[1][0] == '"') {
                    args[1]++;
                }
                if (args[1][strlen(args[1]) - 1] == '"') {
                    args[1][strlen(args[1]) - 1] = '\0';
                }
                if (strlen(args[1]) > MAX_WPA_PASSPHRASE_LENGTH) {
                    printf("ERR:PSK too long.\n");
                    retval = UAP_FAILURE;
                    goto done;
                }
                if (strlen(args[1]) < MIN_WPA_PASSPHRASE_LENGTH) {
                    printf("ERR:PSK too short.\n");
                    retval = UAP_FAILURE;
                    goto done;
                }
                if (strlen(args[1]) == MAX_WPA_PASSPHRASE_LENGTH) {
                    if (UAP_FAILURE == ishexstring(args[1])) {
                        printf
                            ("ERR:Only hex digits are allowed when passphrase's length is 64\n");
                        retval = UAP_FAILURE;
                        goto done;
                    }
                }
                /* Set WPA passphrase field */
                bss->wpa_cfg.length = strlen(args[1]);
                memcpy(bss->wpa_cfg.passphrase, args[1], bss->wpa_cfg.length);
            }
        }
        if (strcmp(args[0], "Protocol") == 0) {
            if (is_input_valid(PROTOCOL, arg_num - 1, args + 1) != UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }
            /* Set protocol field */
            protocol = (t_u16) atoi(args[1]);
            bss->protocol = protocol;

            if (atoi(args[1]) & (PROTOCOL_WPA | PROTOCOL_WPA2)) {
                /* Set key management field */
                bss->key_mgmt = KEY_MGMT_PSK;
            }
        }
        if ((strcmp(args[0], "PairwiseCipher") == 0) ||
            (strcmp(args[0], "GroupCipher") == 0)) {
            printf("ERR:PairwiseCipher and GroupCipher are not supported.\n"
                   "    Please configure pairwise cipher using parameters PwkCipherWPA or PwkCipherWPA2\n"
                   "    and group cipher using GwkCipher in the config file.\n");
            goto done;
        }

        if ((protocol == PROTOCOL_NO_SECURITY) ||
            (protocol == PROTOCOL_STATIC_WEP)) {
            if ((strcmp(args[0], "PwkCipherWPA") == 0) ||
                (strcmp(args[0], "PwkCipherWPA2") == 0)
                || (strcmp(args[0], "GwkCipher") == 0)) {
                printf
                    ("ERR:Pairwise cipher and group cipher should not be defined for Open and WEP mode.\n");
                goto done;
            }
        }

        if (strcmp(args[0], "PwkCipherWPA") == 0) {
            if (arg_num == 1) {
                printf("ERR:PwkCipherWPA is blank!\n");
                goto done;
            } else {
                if (ISDIGIT(args[1]) == 0) {
                    printf
                        ("ERR:Illegal PwkCipherWPA parameter. Must be either bit '2' or '3'.\n");
                    goto done;
                }
                if (atoi(args[1]) & ~CIPHER_BITMAP) {
                    printf
                        ("ERR:Illegal PwkCipherWPA parameter. Must be either bit '2' or '3'.\n");
                    goto done;
                }
                pwkcipher_wpa = atoi(args[1]);
                if (enable_11n && protocol != PROTOCOL_WPA2_MIXED) {
                    memset(&htcap, 0, sizeof(htcap));
                    if (UAP_SUCCESS == get_sys_cfg_11n(&htcap)) {
                        if (htcap.supported_mcs_set[0] &&
                            (atoi(args[1]) == CIPHER_TKIP)) {
                            printf
                                ("ERR: WPA/TKIP cannot be used when AP operates in 802.11n mode.\n");
                            return UAP_FAILURE;
                        }
                    }
                }
            }
        }
        if (strcmp(args[0], "PwkCipherWPA2") == 0) {
            if (arg_num == 1) {
                printf("ERR:PwkCipherWPA2 is blank!\n");
                goto done;
            } else {
                if (ISDIGIT(args[1]) == 0) {
                    printf
                        ("ERR:Illegal PwkCipherWPA2 parameter. Must be either bit '2' or '3'.\n");
                    goto done;
                }
                if (atoi(args[1]) & ~CIPHER_BITMAP) {
                    printf
                        ("ERR:Illegal PwkCipherWPA2 parameter. Must be either bit '2' or '3'.\n");
                    goto done;
                }
                pwkcipher_wpa2 = atoi(args[1]);
                if (enable_11n && protocol != PROTOCOL_WPA2_MIXED) {
                    memset(&htcap, 0, sizeof(htcap));
                    if (UAP_SUCCESS == get_sys_cfg_11n(&htcap)) {
                        if (htcap.supported_mcs_set[0] &&
                            (atoi(args[1]) == CIPHER_TKIP)) {
                            printf
                                ("ERR: WPA/TKIP cannot be used when AP operates in 802.11n mode.\n");
                            return UAP_FAILURE;
                        }
                    }
                }
            }
        }
        if (strcmp(args[0], "GwkCipher") == 0) {
            if (is_input_valid(GWK_CIPHER, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                goto done;
            }
            gwkcipher = atoi(args[1]);
        }
        if (strcmp(args[0], "GroupRekeyTime") == 0) {
            if (is_input_valid(GROUPREKEYTIMER, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }

            /* Set group rekey time field */
            bss->wpa_cfg.gk_rekey_time = (t_u32) atoi(args[1]);
        }
        if (strcmp(args[0], "MaxStaNum") == 0) {
            if (is_input_valid(MAXSTANUM, arg_num - 1, args + 1) != UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }

            /* Set max STA number field */
            bss->max_sta_count = (t_u16) atoi(args[1]);
        }
        if (strcmp(args[0], "Retrylimit") == 0) {
            if (is_input_valid(RETRYLIMIT, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }

            /* Set retry limit field */
            bss->retry_limit = (t_u16) atoi(args[1]);
        }
        if (strcmp(args[0], "PreambleType") == 0) {
            if (is_input_valid(PREAMBLETYPE, arg_num - 1, args + 1) !=
                UAP_SUCCESS) {
                retval = UAP_FAILURE;
                goto done;
            }

            /* Set preamble type field */
            bss->preamble_type = (t_u8) atoi(args[1]);
        }
        if (strcmp(args[0], "Enable11n") == 0) {
            if ((ISDIGIT(args[1]) != UAP_SUCCESS) || (atoi(args[1]) < 0) ||
                (atoi(args[1]) > 1)) {
                printf("ERR: Invalid Enable11n value\n");
                goto done;
            }
            enable_11n = atoi(args[1]);

            memset(&htcap, 0, sizeof(htcap));
            if (UAP_SUCCESS != get_sys_cfg_11n(&htcap)) {
                printf("ERR: Reading current 11n configuration.\n");
                goto done;
            }
            bss->ht_cap_info = htcap.ht_cap_info;
            bss->ampdu_param = htcap.ampdu_param;
            memcpy(bss->supported_mcs_set, htcap.supported_mcs_set, 16);
            bss->ht_ext_cap = htcap.ht_ext_cap;
            bss->tx_bf_cap = htcap.tx_bf_cap;
            bss->asel = htcap.asel;
            if (enable_11n == 1) {
                /* enable mcs rate */
                bss->supported_mcs_set[0] = DEFAULT_MCS_SET_0;
                bss->supported_mcs_set[4] = DEFAULT_MCS_SET_4;
            } else {
                /* disable mcs rate */
                bss->supported_mcs_set[0] = 0;
                bss->supported_mcs_set[4] = 0;
            }
        }
        if (strcmp(args[0], "HTCapInfo") == 0) {
            if (enable_11n <= 0) {
                printf
                    ("ERR: Enable11n parameter should be set before HTCapInfo.\n");
                goto done;
            }
            if ((IS_HEX_OR_DIGIT(args[1]) == UAP_FAILURE) ||
                ((((t_u16) A2HEXDECIMAL(args[1])) & (~HT_CAP_CONFIG_MASK)) !=
                 HT_CAP_CHECK_MASK)) {
                printf("ERR: Invalid HTCapInfo value\n");
                goto done;
            }
            bss->ht_cap_info = DEFAULT_HT_CAP_VALUE & ~HT_CAP_CONFIG_MASK;
            bss->ht_cap_info |=
                (t_u16) A2HEXDECIMAL(args[1]) & HT_CAP_CONFIG_MASK;
            bss->ht_cap_info = uap_cpu_to_le16(bss->ht_cap_info);
        }
        if (strcmp(args[0], "AMPDU") == 0) {
            if (enable_11n <= 0) {
                printf
                    ("ERR: Enable11n parameter should be set before AMPDU.\n");
                goto done;
            }
            if ((IS_HEX_OR_DIGIT(args[1]) == UAP_FAILURE) ||
                ((A2HEXDECIMAL(args[1])) > AMPDU_CONFIG_MASK)) {
                printf("ERR: Invalid AMPDU value\n");
                goto done;
            }
            /* Find HT tlv pointer in buffer and set AMPDU */
            bss->ampdu_param = (t_u8) A2HEXDECIMAL(args[1]) & AMPDU_CONFIG_MASK;
        }
    }
  done:
    fclose(config_file);
    if (line)
        free(line);
    return retval;
}

/**
 *  @brief Show all the BSS configuration in the buffer
 *
 *  @param buf     Pointer to BSS configuration buffer
 *  
 *  $return         N/A
 */
void
print_bss_config(bss_config_t * buf)
{
    int i = 0;
    int flag = 0;

    if (!buf) {
        printf("ERR:Empty BSS config!\n");
        return;
    }

    /* Print AP MAC address */
    printf("AP MAC address = ");
    print_mac(buf->mac_addr);
    printf("\n");

    /* Print SSID */
    if (buf->ssid.ssid_len) {
        printf("SSID = %s\n", buf->ssid.ssid);
    }

    /* Print broadcast SSID control */
    printf("SSID broadcast = %s\n",
           (buf->bcast_ssid_ctl == 1) ? "enabled" : "disabled");

    /* Print DTIM period */
    printf("DTIM period = %d\n", buf->dtim_period);

    /* Print beacon period */
    printf("Beacon period = %d\n", buf->beacon_period);

    /* Print rates */
    printf("Basic Rates =");
    for (i = 0; i < MAX_DATA_RATES; i++) {
        if (!buf->rates[i])
            break;
        if (buf->rates[i] > (BASIC_RATE_SET_BIT - 1)) {
            flag = flag ? : 1;
            printf(" 0x%x", buf->rates[i]);
        }
    }
    printf("%s\nNon-Basic Rates =", flag ? "" : " ( none ) ");
    for (flag = 0, i = 0; i < MAX_DATA_RATES; i++) {
        if (!buf->rates[i])
            break;
        if (buf->rates[i] < BASIC_RATE_SET_BIT) {
            flag = flag ? : 1;
            printf(" 0x%x", buf->rates[i]);
        }
    }
    printf("%s\n", flag ? "" : " ( none ) ");

    /* Print Tx data rate */
    printf("Tx data rate = ");
    if (buf->tx_data_rate == 0)
        printf("auto\n");
    else
        printf("0x%x\n", buf->tx_data_rate);

    /* Print MCBC data rate */
    printf("MCBC data rate = ");
    if (buf->mcbc_data_rate == 0)
        printf("auto\n");
    else
        printf("0x%x\n", buf->mcbc_data_rate);

    /* Print Tx power level */
    printf("Tx power = %d dBm\n", buf->tx_power_level);

    /* Print Tx antenna */
    printf("Tx antenna = %s\n", (buf->tx_antenna) ? "B" : "A");

    /* Print Rx antenna */
    printf("Rx antenna = %s\n", (buf->rx_antenna) ? "B" : "A");

    /* Print Radio control */
    printf("Radio = %s\n", (buf->radio_ctl == 0) ? "on" : "off");

    /* Print packet forward control */
    printf("Firmware = %s\n", (buf->pkt_forward_ctl == 0) ?
           "forwards all packets to the host" : "handles intra-BSS packets");

    /* Print maximum STA count */
    printf("Max Station Number = %d\n", buf->max_sta_count);

    /* Print MAC filter */
    if (buf->filter.filter_mode == 0) {
        printf("Filter Mode = Filter table is disabled\n");
    } else {
        if (buf->filter.filter_mode == 1) {
            printf
                ("Filter Mode = Allow MAC addresses specified in the allowed list\n");
        } else if (buf->filter.filter_mode == 2) {
            printf
                ("Filter Mode = Block MAC addresses specified in the banned list\n");
        } else {
            printf("Filter Mode = Unknown\n");
        }
        for (i = 0; i < buf->filter.mac_count; i++) {
            printf("MAC_%d = ", i);
            print_mac(buf->filter.mac_list[i]);
            printf("\n");
        }
    }

    /* Print STA ageout timer */
    printf("STA ageout timer = %ld\n", buf->sta_ageout_timer);

    /* Print PS STA ageout timer */
    printf("PS STA ageout timer = %ld\n", buf->ps_sta_ageout_timer);

    /* Print RTS threshold */
    printf("RTS threshold = %d\n", buf->rts_threshold);

    /* Print Fragmentation threshold */
    printf("Fragmentation threshold = %d\n", buf->frag_threshold);

    /* Print retry limit */
    printf("Retry Limit = %d\n", buf->retry_limit);

    /* Print preamble type */
    printf("Preamble type = %s\n", (buf->preamble_type == 0) ?
           "auto" : ((buf->preamble_type == 1) ? "short" : "long"));

    /* Print channel */
    printf("Channel = %d\n", buf->channel);
    printf("Channel Select Mode = %s\n",
           (buf->band_cfg & BAND_CONFIG_ACS_MODE) ? "ACS" : "Manual");
    buf->band_cfg &= 0x30;
    if (buf->band_cfg == 0)
        printf("no secondary channel\n");
    else if (buf->band_cfg == SECOND_CHANNEL_ABOVE)
        printf("secondary channel is above primary channel\n");
    else if (buf->band_cfg == SECOND_CHANNEL_BELOW)
        printf("secondary channel is below primary channel\n");

    /* Print channel list */
    printf("Channels List = ");
    for (i = 0; (unsigned int) i < buf->chan_list.num_of_chan; i++) {
        printf("%d ", buf->chan_list.chan[i]);
    }
    printf("\n");

    /* Print auth mode */
    switch (buf->auth_mode) {
    case 0:
        printf("AUTHMODE = Open authentication\n");
        break;
    case 1:
        printf("AUTHMODE = Shared key authentication\n");
        break;
    case 2:
        printf("AUTHMODE = Auto (open and shared key)\n");
        break;
    default:
        printf("ERR: Invalid authmode=%d\n", buf->auth_mode);
        break;
    }

    /* Print protocol */
    switch (buf->protocol) {
    case 0:
    case PROTOCOL_NO_SECURITY:
        printf("PROTOCOL = No security\n");
        break;
    case PROTOCOL_STATIC_WEP:
        printf("PROTOCOL = Static WEP\n");
        break;
    case PROTOCOL_WPA:
        printf("PROTOCOL = WPA \n");
        break;
    case PROTOCOL_WPA2:
        printf("PROTOCOL = WPA2 \n");
        break;
    case PROTOCOL_WPA | PROTOCOL_WPA2:
        printf("PROTOCOL = WPA/WPA2 \n");
        break;
    default:
        printf("Unknown PROTOCOL: 0x%x \n", buf->protocol);
        break;
    }

    /* Print key management */
    if (buf->key_mgmt == KEY_MGMT_PSK)
        printf("KeyMgmt = PSK\n");
    else
        printf("KeyMgmt = NONE\n");

    /* Print WEP configurations */
    if (buf->wep_cfg.key0.length) {
        printf("WEP KEY_0 = ");
        for (i = 0; i < buf->wep_cfg.key0.length; i++) {
            printf("%02x ", buf->wep_cfg.key0.key[i]);
        }
        (buf->wep_cfg.key0.
         is_default) ? (printf("<Default>\n")) : (printf("\n"));
    } else {
        printf("WEP KEY_0 = NONE\n");
    }
    if (buf->wep_cfg.key1.length) {
        printf("WEP KEY_1 = ");
        for (i = 0; i < buf->wep_cfg.key1.length; i++) {
            printf("%02x ", buf->wep_cfg.key1.key[i]);
        }
        (buf->wep_cfg.key1.
         is_default) ? (printf("<Default>\n")) : (printf("\n"));
    } else {
        printf("WEP KEY_1 = NONE\n");
    }
    if (buf->wep_cfg.key2.length) {
        printf("WEP KEY_2 = ");
        for (i = 0; i < buf->wep_cfg.key2.length; i++) {
            printf("%02x ", buf->wep_cfg.key2.key[i]);
        }
        (buf->wep_cfg.key2.
         is_default) ? (printf("<Default>\n")) : (printf("\n"));
    } else {
        printf("WEP KEY_2 = NONE\n");
    }
    if (buf->wep_cfg.key3.length) {
        printf("WEP KEY_3 = ");
        for (i = 0; i < buf->wep_cfg.key3.length; i++) {
            printf("%02x ", buf->wep_cfg.key3.key[i]);
        }
        (buf->wep_cfg.key3.
         is_default) ? (printf("<Default>\n")) : (printf("\n"));
    } else {
        printf("WEP KEY_3 = NONE\n");
    }

    /* Print WPA configurations */
    if (buf->protocol & PROTOCOL_WPA) {
        switch (buf->wpa_cfg.pairwise_cipher_wpa) {
        case CIPHER_TKIP:
            printf("PwkCipherWPA = TKIP\n");
            break;
        case CIPHER_AES_CCMP:
            printf("PwkCipherWPA = AES CCMP\n");
            break;
        case CIPHER_TKIP | CIPHER_AES_CCMP:
            printf("PwkCipherWPA = TKIP + AES CCMP\n");
            break;
        case CIPHER_NONE:
            printf("PwkCipherWPA =  None\n");
            break;
        default:
            printf("Unknown PwkCipherWPA 0x%x\n",
                   buf->wpa_cfg.pairwise_cipher_wpa);
            break;
        }
    }
    if (buf->protocol & PROTOCOL_WPA2) {
        switch (buf->wpa_cfg.pairwise_cipher_wpa2) {
        case CIPHER_TKIP:
            printf("PwkCipherWPA2 = TKIP\n");
            break;
        case CIPHER_AES_CCMP:
            printf("PwkCipherWPA2 = AES CCMP\n");
            break;
        case CIPHER_TKIP | CIPHER_AES_CCMP:
            printf("PwkCipherWPA2 = TKIP + AES CCMP\n");
            break;
        case CIPHER_NONE:
            printf("PwkCipherWPA2 =  None\n");
            break;
        default:
            printf("Unknown PwkCipherWPA2 0x%x\n",
                   buf->wpa_cfg.pairwise_cipher_wpa2);
            break;
        }
    }
    switch (buf->wpa_cfg.group_cipher) {
    case CIPHER_TKIP:
        printf("GroupCipher = TKIP\n");
        break;
    case CIPHER_AES_CCMP:
        printf("GroupCipher = AES CCMP\n");
        break;
    case CIPHER_NONE:
        printf("GroupCipher = None\n");
        break;
    default:
        printf("Unknown Group cipher 0x%x\n", buf->wpa_cfg.group_cipher);
        break;
    }
    printf("RSN replay protection = %s\n",
           (buf->wpa_cfg.rsn_protection) ? "enabled" : "disabled");
    printf("Pairwise Handshake timeout = %ld\n", buf->pairwise_update_timeout);
    printf("Pairwise Handshake Retries = %ld\n", buf->pwk_retries);
    printf("Groupwise Handshake timeout = %ld\n",
           buf->groupwise_update_timeout);
    printf("Groupwise Handshake Retries = %ld\n", buf->gwk_retries);
    if (buf->wpa_cfg.length > 0) {
        printf("WPA passphrase = ");
        for (i = 0; (unsigned int) i < buf->wpa_cfg.length; i++)
            printf("%c", buf->wpa_cfg.passphrase[i]);
        printf("\n");
    } else {
        printf("WPA passphrase = None\n");
    }
    if (buf->wpa_cfg.gk_rekey_time == 0)
        printf("Group re-key time = disabled\n");
    else
        printf("Group re-key time = %ld second\n", buf->wpa_cfg.gk_rekey_time);
    return;
}

/** 
 *  @brief Send command to Read the BSS profile
 *
 *  @param buf      Pointer to bss command buffer for get
 *  @return         UAP_SUCCESS or UAP_FAILURE
 */
static int
get_bss_config(t_u8 * buf)
{
    apcmdbuf_bss_configure *cmd_buf = NULL;
    t_s32 sockfd;
    struct ifreq ifr;

    cmd_buf = (apcmdbuf_bss_configure *) buf;
    cmd_buf->action = ACTION_GET;

    /* Open socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("ERR:Cannot open socket\n");
        return UAP_FAILURE;
    }
#if DEBUG
    /* Dump request buffer */
    hexdump("Get Request buffer", (void *) buf, sizeof(apcmdbuf_bss_configure)
            + sizeof(bss_config_t), ' ');
#endif
    /* Initialize the ifr structure */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_ifrn.ifrn_name, dev_name, strlen(dev_name));
    ifr.ifr_ifru.ifru_data = (void *) cmd_buf;
    if (ioctl(sockfd, UAP_BSS_CONFIG, &ifr)) {
        printf("ERR:UAP_BSS_CONFIG is not supported by %s\n", dev_name);
        close(sockfd);
        return UAP_FAILURE;
    }
#if DEBUG
    /* Dump request buffer */
    hexdump("Get Response buffer", (void *) buf, sizeof(apcmdbuf_bss_configure)
            + sizeof(bss_config_t), ' ');
#endif
    close(sockfd);
    return UAP_SUCCESS;
}

/** 
 *  @brief Creates a bss_config request and sends to the driver
 *
 *  Usage: "Usage : bss_config [CONFIG_FILE]"
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_bss_config(int argc, char *argv[])
{
    apcmdbuf_bss_configure *cmd_buf = NULL;
    bss_config_t *bss = NULL;
    t_u8 *buf = NULL;
    t_u16 cmd_len;
    t_u16 buf_len;
    int ret = UAP_SUCCESS;
    int opt;
    t_s32 sockfd;
    struct ifreq ifr;

    while ((opt = getopt_long(argc, argv, "+", cmd_options, NULL)) != -1) {
        switch (opt) {
        default:
            print_bss_config_usage();
            return UAP_SUCCESS;
        }
    }
    argc -= optind;
    argv += optind;

    /* Check arguments */
    if (argc > 1) {
        printf("ERR:Too many arguments.\n");
        print_bss_config_usage();
        return UAP_FAILURE;
    }

    /* Query BSS settings */

    /* Alloc buf for command */
    buf_len = sizeof(apcmdbuf_bss_configure) + sizeof(bss_config_t);
    buf = (t_u8 *) malloc(buf_len);
    if (!buf) {
        printf("ERR:Cannot allocate buffer from command!\n");
        return UAP_FAILURE;
    }
    memset((char *) buf, 0, buf_len);

    /* Locate headers */
    cmd_len = sizeof(apcmdbuf_bss_configure);
    cmd_buf = (apcmdbuf_bss_configure *) buf;
    bss = (bss_config_t *) (buf + cmd_len);

    /* Get all parametes first */
    if (get_bss_config(buf) == UAP_FAILURE) {
        printf("ERR:Reading current parameters\n");
        free(buf);
        return UAP_FAILURE;
    }

    if (argc == 1) {
        /* Parse config file and populate structure */
        ret = parse_bss_config(argc, argv, bss);
        if (ret == UAP_FAILURE) {
            free(buf);
            return ret;
        }
        cmd_len += sizeof(bss_config_t);
        cmd_buf->action = ACTION_SET;

        /* Send the command */
        /* Open socket */
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            printf("ERR:Cannot open socket\n");
            free(buf);
            return UAP_FAILURE;
        }

        /* Initialize the ifr structure */
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_ifrn.ifrn_name, dev_name, strlen(dev_name));
        ifr.ifr_ifru.ifru_data = (void *) cmd_buf;
#if DEBUG
        /* Dump request buffer */
        hexdump("Request buffer", (void *) buf, buf_len, ' ');
#endif
        if (ioctl(sockfd, UAP_BSS_CONFIG, &ifr)) {
            perror("");
            printf("ERR:UAP_BSS_CONFIG is not supported by %s\n", dev_name);
            close(sockfd);
            free(buf);
            return UAP_FAILURE;
        }
#if DEBUG
        /* Dump respond buffer */
        hexdump("Respond buffer", (void *) buf, buf_len, ' ');
#endif
        close(sockfd);

    } else {
        /* Print response */
        printf("BSS settings:\n");
        print_bss_config(bss);
    }

    free(buf);
    return ret;
}

/**
 *  @brief Show usage information for the sys_cfg_custom_ie
 *   command
 *
 *  $return         N/A
 */
void
print_sys_cfg_custom_ie_usage(void)
{
    printf("\nUsage : sys_cfg_custom_ie [INDEX] [MASK] [IEBuffer]");
    printf("\n         empty - Get all IE settings\n");
    printf("\n         INDEX:  0 - Get/Set IE index 0 setting");
    printf("\n                 1 - Get/Set IE index 1 setting");
    printf("\n                 2 - Get/Set IE index 2 setting");
    printf("\n                 3 - Get/Set IE index 3 setting");
    printf("\n                 .                             ");
    printf("\n                 .                             ");
    printf("\n                 .                             ");
    printf("\n                -1 - Append/Delete IE automatically");
    printf
        ("\n                     Delete will delete the IE from the matching IE buffer");
    printf
        ("\n                     Append will append the IE to the buffer with the same mask");
    printf
        ("\n         MASK :  Management subtype mask value as per bit defintions");
    printf("\n              :  Bit 0 - Association request.");
    printf("\n              :  Bit 1 - Association response.");
    printf("\n              :  Bit 2 - Reassociation request.");
    printf("\n              :  Bit 3 - Reassociation response.");
    printf("\n              :  Bit 4 - Probe request.");
    printf("\n              :  Bit 5 - Probe response.");
    printf("\n              :  Bit 8 - Beacon.");
    printf("\n         MASK :  MASK = 0 to clear the mask and the IE buffer");
    printf("\n         IEBuffer :  IE Buffer in hex (max 256 bytes)\n\n");
    return;
}

/** custom IE, auto mask value */
#define	UAP_CUSTOM_IE_AUTO_MASK	0xffff

/**
 *  @brief Creates a sys_cfg request for custom IE settings
 *   and sends to the driver
 *
 *   Usage: "sys_cfg_custom_ie [INDEX] [MASK] [IEBuffer]"
 *
 *   Options: INDEX :      0 - Get/Set IE index 0 setting
 *                         1 - Get/Set IE index 1 setting
 *                         2 - Get/Set IE index 2 setting
 *                         3 - Get/Set IE index 3 setting
 *                         .
 *                         .
 *                         .
 *            MASK  :      Management subtype mask value
 *            IEBuffer:      IE Buffer in hex
 *            empty - Get all IE settings
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
apcmd_sys_cfg_custom_ie(int argc, char *argv[])
{
    tlvbuf_custom_ie *tlv = NULL;
    custom_ie *ie_ptr = NULL;
    t_u8 *buffer = NULL;
    t_u16 buf_len = 0;
    t_u16 mgmt_subtype_mask = 0;
    int ie_buf_len = 0, ie_len = 0;
    struct ifreq ifr;
    t_s32 sockfd;

    /* Check arguments */
    if (argc > 4) {
        printf("ERR:Too many arguments.\n");
        print_sys_cfg_custom_ie_usage();
        return UAP_FAILURE;
    }

    /* Error checks and initialize the command length */
    if (argc >= 2) {
        if (((IS_HEX_OR_DIGIT(argv[1]) == UAP_FAILURE) && (atoi(argv[1]) != -1))
            || (atoi(argv[1]) < -1)) {
            printf("ERR:Illegal index %s\n", argv[1]);
            print_sys_cfg_custom_ie_usage();
            return UAP_FAILURE;
        }
    }
    switch (argc) {
    case 1:
        buf_len = MRVDRV_SIZE_OF_CMD_BUFFER;
        break;
    case 2:
        if (atoi(argv[1]) < 0) {
            printf
                ("ERR:Illegal index %s. Must be either greater than or equal to 0 for Get Operation \n",
                 argv[1]);
            print_sys_cfg_custom_ie_usage();
            return UAP_FAILURE;
        }
        buf_len = MRVDRV_SIZE_OF_CMD_BUFFER;
        break;
    case 3:
        if (UAP_FAILURE == ishexstring(argv[2]) || A2HEXDECIMAL(argv[2]) != 0) {
            printf("ERR: Mask value should be 0 to clear IEBuffers.\n");
            print_sys_cfg_custom_ie_usage();
            return UAP_FAILURE;
        }
        if (atoi(argv[1]) == -1) {
            printf("ERR: Buffer should be provided for automatic deletion.\n");
            print_sys_cfg_custom_ie_usage();
            return UAP_FAILURE;
        }
        buf_len = sizeof(tlvbuf_custom_ie) + sizeof(custom_ie);
        break;
    case 4:
        /* This is to check negative numbers and special symbols */
        if (UAP_FAILURE == IS_HEX_OR_DIGIT(argv[2])) {
            printf("ERR:Mask value must be 0 or hex digits\n");
            print_sys_cfg_custom_ie_usage();
            return UAP_FAILURE;
        }
        /* If above check is passed and mask is not hex, then it must be 0 */
        if ((ISDIGIT(argv[2]) == UAP_SUCCESS) && atoi(argv[2])) {
            printf("ERR:Mask value must be 0 or hex digits\n ");
            print_sys_cfg_custom_ie_usage();
            return UAP_FAILURE;
        }
        if (UAP_FAILURE == ishexstring(argv[3])) {
            printf("ERR:Only hex digits are allowed\n");
            print_sys_cfg_custom_ie_usage();
            return UAP_FAILURE;
        }
        ie_buf_len = strlen(argv[3]);
        if (!strncasecmp("0x", argv[3], 2)) {
            ie_len = (ie_buf_len - 2 + 1) / 2;
            argv[3] += 2;
        } else
            ie_len = (ie_buf_len + 1) / 2;
        if (ie_len > MAX_IE_BUFFER_LEN) {
            printf("ERR:Incorrect IE length %d\n", ie_buf_len);
            print_sys_cfg_custom_ie_usage();
            return UAP_FAILURE;
        }
        mgmt_subtype_mask = (t_u16) A2HEXDECIMAL(argv[2]);
        buf_len = sizeof(tlvbuf_custom_ie) + sizeof(custom_ie) + ie_len;
        break;
    }

    /* Initialize the command buffer */
    buffer = (t_u8 *) malloc(buf_len);
    if (!buffer) {
        printf("ERR:Cannot allocate buffer for command!\n");
        return UAP_FAILURE;
    }
    memset(buffer, 0, buf_len);
    tlv = (tlvbuf_custom_ie *) buffer;
    tlv->tag = MRVL_MGMT_IE_LIST_TLV_ID;
    if (argc == 1 || argc == 2) {
        if (argc == 1)
            tlv->length = 0;
        else {
            tlv->length = sizeof(t_u16);
            ie_ptr = (custom_ie *) (tlv->ie_data);
            ie_ptr->ie_index = (t_u16) (atoi(argv[1]));
        }
    } else {
        /* Locate headers */
        ie_ptr = (custom_ie *) (tlv->ie_data);
        /* Set TLV fields */
        tlv->length = sizeof(custom_ie) + ie_len;
        ie_ptr->ie_index = atoi(argv[1]);
        ie_ptr->mgmt_subtype_mask = mgmt_subtype_mask;
        ie_ptr->ie_length = ie_len;
        if (argc == 4)
            string2raw(argv[3], ie_ptr->ie_buffer);
    }
    /* Open socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("ERR:Cannot open socket\n");
        if (buffer)
            free(buffer);
        return UAP_FAILURE;
    }
    /* Initialize the ifr structure */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_ifrn.ifrn_name, dev_name, strlen(dev_name));
    ifr.ifr_ifru.ifru_data = (void *) buffer;
    /* Perform ioctl */
    if (ioctl(sockfd, UAP_CUSTOM_IE, &ifr)) {
        perror("ioctl[UAP_CUSTOM_IE]");
        printf("ERR:Command sending failed!\n");
        close(sockfd);
        if (buffer)
            free(buffer);
        return UAP_FAILURE;
    }
    /* Print response */
    if (argc > 2) {
        printf("custom IE setting successful\n");
    } else {
        printf("Querying custom IE successful\n");
        tlv = (tlvbuf_custom_ie *) buffer;
        ie_len = tlv->length;
        ie_ptr = (custom_ie *) (tlv->ie_data);
        if (tlv->tag == MRVL_MGMT_IE_LIST_TLV_ID) {
            while ((unsigned int) ie_len >= sizeof(custom_ie)) {
                printf("Index [%d]\n", ie_ptr->ie_index);
                if (ie_ptr->ie_length)
                    printf("Management Subtype Mask = 0x%02x\n",
                           ie_ptr->mgmt_subtype_mask == 0 ?
                           UAP_CUSTOM_IE_AUTO_MASK : ie_ptr->mgmt_subtype_mask);
                else
                    printf("Management Subtype Mask = 0x%02x\n",
                           ie_ptr->mgmt_subtype_mask);
                hexdump_data("IE Buffer", (void *) ie_ptr->ie_buffer,
                             (ie_ptr->ie_length), ' ');
                ie_len -= sizeof(custom_ie) + ie_ptr->ie_length;
                ie_ptr = (custom_ie *) ((t_u8 *) ie_ptr + sizeof(custom_ie) +
                                        ie_ptr->ie_length);
            }
        }
    }
    if (buffer)
        free(buffer);

    close(sockfd);
    return UAP_SUCCESS;
}

/** Structure of command table*/
typedef struct
{
    /** Command name */
    char *cmd;
    /** Command function pointer */
    int (*func) (int argc, char *argv[]);
    /** Command usuage */
    char *help;
} command_table;

/** AP command table */
static command_table ap_command[] = {
    {"sys_config", apcmd_sys_config, "\tSet/get uAP's profile"},
    {"sys_info", apcmd_sys_info, "\tDisplay system info"},
    {"sys_reset", apcmd_sys_reset, "\tReset uAP"},
    {"bss_start", apcmd_bss_start, "\tStart the BSS"},
    {"bss_stop", apcmd_bss_stop, "\tStop the BSS"},
    {"sta_deauth", apcmd_sta_deauth, "\tDeauth client"},
    {"sta_list", apcmd_sta_list, "\tDisplay list of clients"},
    {"sys_cfg_ap_mac_address", apcmd_sys_cfg_ap_mac_address,
     "Set/get uAP mac address"},
    {"sys_cfg_ssid", apcmd_sys_cfg_ssid, "\tSet/get uAP ssid"},
    {"sys_cfg_beacon_period", apcmd_sys_cfg_beacon_period,
     "Set/get uAP beacon period"},
    {"sys_cfg_dtim_period", apcmd_sys_cfg_dtim_period,
     "Set/get uAP dtim period"},
    {"sys_cfg_bss_status", apcmd_sys_cfg_bss_status, "Get BSS status"},
    {"sys_cfg_channel", apcmd_sys_cfg_channel, "\tSet/get uAP radio channel"},
    {"sys_cfg_scan_channels", apcmd_sys_cfg_scan_channels,
     "Set/get uAP radio channel list"},
    {"sys_cfg_rates", apcmd_sys_cfg_rates, "\tSet/get uAP rates"},
    {"sys_cfg_rates_ext", apcmd_sys_cfg_rates_ext, "\tSet/get uAP rates"},
    {"sys_cfg_tx_power", apcmd_sys_cfg_tx_power, "Set/get uAP tx power"},
    {"sys_cfg_bcast_ssid_ctl", apcmd_sys_cfg_bcast_ssid_ctl,
     "Set/get uAP broadcast ssid"},
    {"sys_cfg_preamble_ctl", apcmd_sys_cfg_preamble_ctl, "Get uAP preamble"},
    {"sys_cfg_antenna_ctl", apcmd_sys_cfg_antenna_ctl,
     "Set/get uAP tx/rx antenna"},
    {"sys_cfg_rts_threshold", apcmd_sys_cfg_rts_threshold,
     "Set/get uAP rts threshold"},
    {"sys_cfg_frag_threshold", apcmd_sys_cfg_frag_threshold,
     "Set/get uAP frag threshold"},
    {"sys_cfg_radio_ctl", apcmd_sys_cfg_radio_ctl, "Set/get uAP radio on/off"},
    {"sys_cfg_tx_data_rate", apcmd_sys_cfg_tx_data_rate, "Set/get uAP tx rate"},
    {"sys_cfg_mcbc_data_rate", apcmd_sys_cfg_mcbc_data_rate,
     "Set/get uAP MCBC rate"},
    {"sys_cfg_rsn_replay_prot", apcmd_sys_cfg_rsn_replay_prot,
     "Set/get RSN replay protection"},
    {"sys_cfg_pkt_fwd_ctl", apcmd_sys_cfg_pkt_fwd_ctl,
     "Set/get uAP packet forwarding"},
    {"sys_cfg_sta_ageout_timer", apcmd_sys_cfg_sta_ageout_timer,
     "Set/get station ageout timer"},
    {"sys_cfg_ps_sta_ageout_timer", apcmd_sys_cfg_ps_sta_ageout_timer,
     "Set/get PS station ageout timer"},
    {"sys_cfg_auth", apcmd_sys_cfg_auth, "\tSet/get uAP authentication mode"},
    {"sys_cfg_protocol", apcmd_sys_cfg_protocol,
     "Set/get uAP security protocol"},
    {"sys_cfg_wep_key", apcmd_sys_cfg_wep_key, "\tSet/get uAP wep key"},
    {"sys_cfg_cipher", apcmd_sys_cfg_cipher, "\tSet/get uAP WPA/WPA2 cipher"},
    {"sys_cfg_pwk_cipher", apcmd_sys_cfg_pwk_cipher,
     "\tSet/get uAP WPA/WPA2 pairwise cipher"},
    {"sys_cfg_gwk_cipher", apcmd_sys_cfg_gwk_cipher,
     "\tSet/get uAP WPA/WPA2 group cipher"},
    {"sys_cfg_wpa_passphrase", apcmd_sys_cfg_wpa_passphrase,
     "Set/get uAP WPA or WPA2 passphrase"},
    {"sys_cfg_group_rekey_timer", apcmd_sys_cfg_group_rekey_timer,
     "Set/get uAP group re-key time"},
    {"sys_cfg_max_sta_num", apcmd_sys_cfg_max_sta_num,
     "Set/get uAP max station number"},
    {"sys_cfg_retry_limit", apcmd_sys_cfg_retry_limit,
     "Set/get uAP retry limit number"},
    {"sys_cfg_eapol_pwk_hsk", apcmd_sys_cfg_eapol_pwk_hsk,
     "Set/getuAP pairwise Handshake timeout value and retries"},
    {"sys_cfg_eapol_gwk_hsk", apcmd_sys_cfg_eapol_gwk_hsk,
     "Set/getuAP groupwise Handshake timeout value and retries"},
    {"sys_cfg_custom_ie", apcmd_sys_cfg_custom_ie,
     "\tSet/get custom IE configuration"},
    {"sta_filter_table", apcmd_sta_filter_table, "Set/get uAP mac filter"},
    {"regrdwr", apcmd_regrdwr, "\t\tRead/Write register command"},
    {"memaccess", apcmd_memaccess, "\tRead/Write to a memory address command"},
    {"rdeeprom", apcmd_read_eeprom, "\tRead EEPROM "},
    {"cfg_data", apcmd_cfg_data,
     "\tGet/Set configuration file from/to firmware"},
    {"sys_debug", apcmd_sys_debug, "\tSet/Get debug parameter"},
    {"sys_cfg_80211d", apcmd_cfg_80211d, "\tSet/Get 802.11D info"},
    {"uap_stats", apcmd_uap_stats, "\tGet uAP stats"},
    {"powermode", apcmd_power_mode, "\tSet/get uAP power mode"},
    {"pscfg", apcmd_pscfg, "\t\tSet/get uAP power mode"},
    {"bss_config", apcmd_bss_config, "\tSet/get BSS configuration"},
    {"sta_deauth_ext", apcmd_sta_deauth_ext, "\tDeauth client"},
    {"hscfg", apcmd_hscfg, "\t\tSet/get uAP host sleep parameters."},
    {"addbapara", apcmd_addbapara, "\tSet/get uAP ADDBA parameters."},
    {"aggrpriotbl", apcmd_aggrpriotbl,
     "\tSet/get uAP priority table for AMPDU/AMSDU."},
    {"addbareject", apcmd_addbareject, "\tSet/get uAP addbareject table."},
    {"sys_cfg_11n", apcmd_sys_cfg_11n, "\tSet/get uAP 802.11n parameters."},
    {"sys_cfg_wmm", apcmd_sys_cfg_wmm, "\tSet/get uAP wmm parameters."},
    {"deepsleep", apcmd_deepsleep, "\tSet/get deepsleep mode."},
    {"hostcmd", apcmd_hostcmd, "\tSet/get hostcmd"},
    {NULL, NULL, 0}
};

/** 
 *  @brief Prints usage information of uaputl
 *
 *  @return          N/A
 */
static void
print_tool_usage(void)
{
    int i;
    printf("uaputl.exe - uAP utility ver %s\n", UAP_VERSION);
    printf("Usage:\n"
           "\tuaputl.exe [options] <command> [command parameters]\n");
    printf("Options:\n"
           "\t--help\tDisplay help\n"
           "\t-v\tDisplay version\n"
           "\t-i <interface>\n" "\t-d <debug_level=0|1|2>\n");
    printf("Commands:\n");
    for (i = 0; ap_command[i].cmd; i++)
        printf("\t%-4s\t\t%s\n", ap_command[i].cmd, ap_command[i].help);
    printf("\n"
           "For more information on the usage of each command use:\n"
           "\tuaputl.exe <command> --help\n");
}

/****************************************************************************
        Global functions
****************************************************************************/
/** Option parameter*/
static struct option ap_options[] = {
    {"help", 0, NULL, 'h'},
    {"interface", 1, NULL, 'i'},
    {"debug", 1, NULL, 'd'},
    {"version", 0, NULL, 'v'},
    {NULL, 0, NULL, '\0'}
};

/**
 *    @brief isdigit for String.
 *   
 *    @param x            Char string
 *    @return             UAP_FAILURE for non-digit.
 *                        UAP_SUCCESS for digit
 */
inline int
ISDIGIT(char *x)
{
    unsigned int i;
    for (i = 0; i < strlen(x); i++)
        if (isdigit(x[i]) == 0)
            return UAP_FAILURE;
    return UAP_SUCCESS;
}

/**
 * @brief Checks if given channel in 'a' band is valid or not.
 *
 * @param channel   Channel number
 * @return          UAP_SUCCESS or UAP_FAILURE
 */
int
is_valid_a_band_channel(int channel)
{
    int ret = UAP_SUCCESS;
    switch (channel) {
    case 16:
    case 34:
    case 36:
    case 38:
    case 40:
    case 42:
    case 44:
    case 46:
    case 48:
    case 52:
    case 56:
    case 60:
    case 64:
    case 100:
    case 104:
    case 108:
    case 112:
    case 116:
    case 120:
    case 124:
    case 128:
    case 132:
    case 136:
    case 140:
    case 149:
    case 153:
    case 157:
    case 161:
    case 165:
        break;
    default:
        ret = UAP_FAILURE;
        break;
    }
    return ret;
}

/** 
 *  @brief Checkes a particular input for validatation.
 *
 *  @param cmd      Type of input
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         UAP_SUCCESS or UAP_FAILURE
 */
int
is_input_valid(valid_inputs cmd, int argc, char *argv[])
{
    int i;
    int ch;
    int ret = UAP_SUCCESS;
    if (argc == 0)
        return UAP_FAILURE;
    switch (cmd) {
    case RDEEPROM:
        if (argc != 2) {
            printf(" ERR: Argument count mismatch\n");
            ret = UAP_FAILURE;
        } else {
            if ((ISDIGIT(argv[0]) == 0) || (ISDIGIT(argv[1]) == 0) ||
                (A2HEXDECIMAL(argv[0]) & 0x03) ||
                ((int) (A2HEXDECIMAL(argv[0])) < 0) ||
                (A2HEXDECIMAL(argv[1]) & 0x03) ||
                (A2HEXDECIMAL(argv[1]) < 4) || (A2HEXDECIMAL(argv[1]) > 20)) {
                printf(" ERR: Invalid inputs for Read EEPROM\n");
                ret = UAP_FAILURE;
            }
        }
        break;
    case SCANCHANNELS:
        if (argc > MAX_CHANNELS) {
            printf("ERR: Invalid List of Channels\n");
            ret = UAP_FAILURE;
        } else {
            for (i = 0; i < argc; i++) {
                if ((ISDIGIT(argv[i]) == 0) || (atoi(argv[i]) < 1) ||
                    (atoi(argv[i]) > MAX_CHANNELS)) {
                    printf("ERR: Channel must be in the range of 1 to %d\n",
                           MAX_CHANNELS);
                    ret = UAP_FAILURE;
                    break;
                }
                if ((atoi(argv[i]) > MAX_CHANNELS_BG) &&
                    !(is_valid_a_band_channel(atoi(argv[i])))) {
                    printf("ERR: Invalid Channel in 'a' band!\n");
                    ret = UAP_FAILURE;
                    break;
                }
            }
            if ((ret != UAP_FAILURE) &&
                (has_dup_channel(argc, argv) != UAP_SUCCESS)) {
                printf("ERR: Duplicate channel values entered\n");
                ret = UAP_FAILURE;
            }
        }
        break;
    case TXPOWER:
        if ((argc > 1) || (ISDIGIT(argv[0]) == 0)) {
            printf("ERR:Invalid Transmit power\n");
            ret = UAP_FAILURE;
        } else {
            if ((atoi(argv[0]) < MIN_TX_POWER) ||
                (atoi(argv[0]) > MAX_TX_POWER)) {
                printf("ERR: TX Powar must be in the rage of %d to %d. \n",
                       MIN_TX_POWER, MAX_TX_POWER);
                ret = UAP_FAILURE;
            }
        }
        break;
    case PROTOCOL:
        if ((argc > 1) || (ISDIGIT(argv[0]) == 0)) {
            printf("ERR:Invalid Protocol\n");
            ret = UAP_FAILURE;
        } else
            ret = is_protocol_valid(atoi(argv[0]));
        break;
    case CHANNEL:
        if ((argc != 1) && (argc != 2)) {
            printf("ERR: Incorrect arguments for channel.\n");
            ret = UAP_FAILURE;
        } else {
            if (argc == 2) {
                if ((ISDIGIT(argv[1]) == 0) ||
                    (atoi(argv[1]) & ~CHANNEL_MODE_MASK)) {
                    printf("ERR: Invalid Mode\n");
                    ret = UAP_FAILURE;
                }
                if ((atoi(argv[1]) & BITMAP_ACS_MODE) && (atoi(argv[0]) != 0)) {
                    printf("ERR: Channel must be 0 for ACS; MODE = 1.\n");
                    ret = UAP_FAILURE;
                }
                if ((atoi(argv[1]) & BITMAP_CHANNEL_ABOVE) &&
                    (atoi(argv[1]) & BITMAP_CHANNEL_BELOW)) {
                    printf
                        ("ERR: secondary channel above and below both are enabled\n");
                    ret = UAP_FAILURE;
                }
            }
            if ((argc == 1) || (!(atoi(argv[1]) & BITMAP_ACS_MODE))) {
                if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 1) ||
                    (atoi(argv[0]) > MAX_CHANNELS)) {
                    printf("ERR: Channel must be in the range of 1 to %d\n",
                           MAX_CHANNELS);
                    ret = UAP_FAILURE;
                }
                if ((atoi(argv[0]) > MAX_CHANNELS_BG) &&
                    !(is_valid_a_band_channel(atoi(argv[0])))) {
                    printf("ERR: Invalid Channel in 'a' band!\n");
                    ret = UAP_FAILURE;
                }
                ch = atoi(argv[0]);
                if (ch <= MAX_CHANNELS_BG) {
                    if ((argc == 2) && (atoi(argv[1]) & BITMAP_CHANNEL_ABOVE) &&
                        (atoi(argv[0]) > MAX_CHANNEL_ABOVE)) {
                        printf
                            ("ERR: only allow channel 1-7 for secondary channel above\n");
                        ret = UAP_FAILURE;
                    }
                    if ((argc == 2) && (atoi(argv[1]) & BITMAP_CHANNEL_BELOW) &&
                        (atoi(argv[0]) < MIN_CHANNEL_BELOW)) {
                        printf
                            ("ERR: only allow channel 5-11 for secondary channel below\n");
                        ret = UAP_FAILURE;
                    }
                } else {
                    if (argc == 2) {
                        if (atoi(argv[1]) & BITMAP_CHANNEL_BELOW) {
                            if ((ch < 40) || ((ch > 64) && (ch < 104)) ||
                                ((ch > 116) && (ch < 140)) || ((ch > 140) &&
                                                               (ch < 153))) {
                                printf
                                    ("ERR: Secondary channel can be set below primary only for channels"
                                     " 40-64, 104-116, 140, 153-165.\n");
                                ret = UAP_FAILURE;
                            }
                        }
                        if (atoi(argv[1]) & BITMAP_CHANNEL_ABOVE) {
                            if (((ch > 60) && (ch < 100)) ||
                                ((ch > 112) && (ch < 136)) || ((ch > 136) &&
                                                               (ch < 149)) ||
                                (ch > 161)) {
                                printf
                                    ("ERR: Secondary channel can be set above primary only for channels"
                                     " 36-60, 100-112, 136, 149-161.\n");
                                ret = UAP_FAILURE;
                            }
                        }
                    }
                }
            }
        }
        break;
    case RATE:
        if (argc > MAX_RATES) {
            printf("ERR: Incorrect number of RATES arguments.\n");
            ret = UAP_FAILURE;
        } else {
            for (i = 0; i < argc; i++) {
                if ((IS_HEX_OR_DIGIT(argv[i]) == UAP_FAILURE) ||
                    (is_rate_valid(A2HEXDECIMAL(argv[i]) & ~BASIC_RATE_SET_BIT)
                     != UAP_SUCCESS)) {
                    printf("ERR:Unsupported rate.\n");
                    ret = UAP_FAILURE;
                    break;
                }
            }
            if ((ret != UAP_FAILURE) &&
                (has_dup_rate(argc, argv) != UAP_SUCCESS)) {
                printf("ERR: Duplicate rate values entered\n");
                ret = UAP_FAILURE;
            }
            if (check_mandatory_rates(argc, argv) != UAP_SUCCESS) {
                ret = UAP_FAILURE;
            }
        }
        break;
    case BROADCASTSSID:
        if (argc != 1) {
            printf("ERR:wrong BROADCASTSSID arguments.\n");
            ret = UAP_FAILURE;
        } else {
            if ((ISDIGIT(argv[0]) == 0) ||
                ((atoi(argv[0]) != 0) && (atoi(argv[0]) != 1))) {
                printf
                    ("ERR:Illegal parameter %s for BROADCASTSSID. Must be either '0' or '1'.\n",
                     argv[0]);
                ret = UAP_FAILURE;
            }
        }
        break;
    case RTSTHRESH:
        if (argc != 1) {
            printf("ERR:Incorrect number of arguments for RTSTHRESHOLD\n");
            ret = UAP_FAILURE;
        } else if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
                   (atoi(argv[0]) > MAX_RTS_THRESHOLD)) {
            printf
                ("ERR:Illegal RTSTHRESHOLD %s. The value must between 0 and %d\n",
                 argv[0], MAX_RTS_THRESHOLD);
            ret = UAP_FAILURE;
        }
        break;
    case FRAGTHRESH:
        if (argc != 1) {
            printf("ERR:Incorrect number of arguments for FRAGTHRESH\n");
            ret = UAP_FAILURE;
        } else if ((ISDIGIT(argv[0]) == 0) ||
                   (atoi(argv[0]) < MIN_FRAG_THRESHOLD) ||
                   (atoi(argv[0]) > MAX_FRAG_THRESHOLD)) {
            printf
                ("ERR:Illegal FRAGTHRESH %s. The value must between %d and %d\n",
                 argv[0], MIN_FRAG_THRESHOLD, MAX_FRAG_THRESHOLD);
            ret = UAP_FAILURE;
        }
        break;
    case DTIMPERIOD:
        if (argc != 1) {
            printf("ERR:Incorrect number of arguments for DTIMPERIOD\n");
            ret = UAP_FAILURE;
        } else if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 1) ||
                   (atoi(argv[0]) > MAX_DTIM_PERIOD)) {
            printf("ERR: DTIMPERIOD Value must be in range of 1 to %d\n",
                   MAX_DTIM_PERIOD);
            ret = UAP_FAILURE;
        }
        break;
    case RSNREPLAYPROT:
        if (argc != 1) {
            printf("ERR:Incorrect number of arguments for RSNREPLAYPROT\n");
            ret = UAP_FAILURE;
        } else {
            if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
                (atoi(argv[0]) > 1)) {
                printf
                    ("ERR:Illegal RSNREPLAYPROT parameter %s. Must be either '0' or '1'.\n",
                     argv[0]);
                ret = UAP_FAILURE;
            }
        }
        break;
    case RADIOCONTROL:
        if (argc != 1) {
            printf("ERR:Incorrect number of arguments for RADIOCONTROL\n");
            ret = UAP_FAILURE;
        } else {
            if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
                (atoi(argv[0]) > 1)) {
                printf
                    ("ERR:Illegal RADIOCONTROL parameter %s. Must be either '0' or '1'.\n",
                     argv[0]);
                ret = UAP_FAILURE;
            }
        }
        break;
    case MCBCDATARATE:
    case TXDATARATE:
        if (argc != 1) {
            printf("ERR:Incorrect number of arguments for DATARATE\n");
            ret = UAP_FAILURE;
        } else {
            if (IS_HEX_OR_DIGIT(argv[0]) == UAP_FAILURE) {
                printf("ERR: invalid data rate\n");
                ret = UAP_FAILURE;
            } else if ((A2HEXDECIMAL(argv[0]) != 0) &&
                       (is_rate_valid
                        (A2HEXDECIMAL(argv[0]) & ~BASIC_RATE_SET_BIT) !=
                        UAP_SUCCESS)) {
                printf("ERR: invalid data rate\n");
                ret = UAP_FAILURE;
            }
        }
        break;
    case PKTFWD:
        if (argc != 1) {
            printf("ERR:Incorrect number of arguments for PKTFWD.\n");
            ret = UAP_FAILURE;
        } else if ((ISDIGIT(argv[0]) == 0) ||
                   ((atoi(argv[0]) != 0) && (atoi(argv[0]) != 1))) {
            printf
                ("ERR:Illegal PKTFWD parameter %s. Must be either '0' or '1'.\n",
                 argv[0]);
            ret = UAP_FAILURE;
        }
        break;
    case STAAGEOUTTIMER:
        if (argc != 1) {
            printf("ERR:Incorrect number of arguments for STAAGEOUTTIMER.\n");
            ret = UAP_FAILURE;
        } else {
            if ((ISDIGIT(argv[0]) == 0) || ((atoi(argv[0]) != 0) &&
                                            ((atoi(argv[0]) <
                                              MIN_STAGE_OUT_TIME) ||
                                             (atoi(argv[0]) >
                                              MAX_STAGE_OUT_TIME)))) {
                printf
                    ("ERR:Illegal STAAGEOUTTIMER %s. Must be between %d and %d.\n",
                     argv[0], MIN_STAGE_OUT_TIME, MAX_STAGE_OUT_TIME);
                ret = UAP_FAILURE;
            }
        }
        break;
    case PSSTAAGEOUTTIMER:
        if (argc != 1) {
            printf("ERR:Incorrect number of arguments for PSSTAAGEOUTTIMER.\n");
            ret = UAP_FAILURE;
        } else {
            if ((ISDIGIT(argv[0]) == 0) || ((atoi(argv[0]) != 0) &&
                                            ((atoi(argv[0]) <
                                              MIN_STAGE_OUT_TIME) ||
                                             (atoi(argv[0]) >
                                              MAX_STAGE_OUT_TIME)))) {
                printf
                    ("ERR:Illegal PSSTAAGEOUTTIMER %s. Must be between %d and %d.\n",
                     argv[0], MIN_STAGE_OUT_TIME, MAX_STAGE_OUT_TIME);
                ret = UAP_FAILURE;
            }
        }
        break;
    case AUTHMODE:
        if (argc != 1) {
            printf("ERR:Incorrect number of arguments for AUTHMODE\n");
            ret = UAP_FAILURE;
        } else {
            if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
                (atoi(argv[0]) > 1)) {
                printf
                    ("ERR:Illegal AUTHMODE parameter %s. Must be either '0', or '1'.\n",
                     argv[0]);
                ret = UAP_FAILURE;
            }
        }
        break;
    case GROUPREKEYTIMER:
        if (argc != 1) {
            printf("ERR:Incorrect number of arguments for GROUPREKEYTIMER.\n");
            ret = UAP_FAILURE;
        } else {
            if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < 0) ||
                (atoi(argv[0]) > MAX_GRP_TIMER)) {
                printf("ERR: GROUPREKEYTIMER range is [0:%d] (0 for disable)\n",
                       MAX_GRP_TIMER);
                ret = UAP_FAILURE;
            }
        }
        break;
    case MAXSTANUM:
        if (argc != 1) {
            printf("ERR:Incorrect number of arguments for MAXSTANUM\n");
            ret = UAP_FAILURE;
        } else {
            if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) > MAX_STA_COUNT) ||
                (atoi(argv[0]) <= 0)) {
                printf("ERR:STA_NUM must be in the range of [1:%d] %s.\n",
                       MAX_STA_COUNT, argv[0]);
                ret = UAP_FAILURE;
            }
        }
        break;
    case BEACONPERIOD:
        if (argc != 1) {
            printf("ERR:Incorrect number of argument for BEACONPERIOD.\n");
            ret = UAP_FAILURE;
        } else {
            if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) < MIN_BEACON_PERIOD)
                || (atoi(argv[0]) > MAX_BEACON_PERIOD)) {
                printf("ERR: BEACONPERIOD must be in range of %d to %d.\n",
                       MIN_BEACON_PERIOD, MAX_BEACON_PERIOD);
                ret = UAP_FAILURE;
            }
        }
        break;
    case RETRYLIMIT:
        if (argc != 1) {
            printf("ERR:Incorrect number of arguments for RETRY LIMIT\n");
            ret = UAP_FAILURE;
        } else {
            if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) > MAX_RETRY_LIMIT) ||
                (atoi(argv[0]) < 0)) {
                printf
                    ("ERR:RETRY_LIMIT must be in the range of [0:%d]. The  input was %s.\n",
                     MAX_RETRY_LIMIT, argv[0]);
                ret = UAP_FAILURE;
            }
        }
        break;
    case EAPOL_PWK_HSK:
        if (argc != 2) {
            printf("ERR:Incorrect number of EAPOL_PWK_HSK arguments.\n");
            ret = UAP_FAILURE;
        } else {
            if ((ISDIGIT(argv[0]) == 0) || (ISDIGIT(argv[1]) == 0) ||
                (atoi(argv[0]) < 0) || (atoi(argv[1]) < 0)) {
                printf
                    ("ERR:Illegal parameters for EAPOL_PWK_HSK. Must be digits greater than equal to zero.\n");
            }
        }
        break;
    case EAPOL_GWK_HSK:
        if (argc != 2) {
            printf("ERR:Incorrect number of EAPOL_GWK_HSK arguments.\n");
            ret = UAP_FAILURE;
        } else {
            if ((ISDIGIT(argv[0]) == 0) || (ISDIGIT(argv[1]) == 0) ||
                (atoi(argv[0]) < 0) || (atoi(argv[1]) < 0)) {
                printf
                    ("ERR:Illegal parameters for EAPOL_GWK_HSK. Must be digits greater than equal to zero.\n");
            }
        }
        break;
    case PREAMBLETYPE:
        if (argc != 1) {
            printf("ERR:Incorrect number of arguments for PREAMBLE TYPE\n");
            ret = UAP_FAILURE;
        } else {
            if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) > MAX_PREAMBLE_TYPE)
                || (atoi(argv[0]) < 0)) {
                printf
                    ("ERR:PREAMBLE TYPE must be in the range of [0:%d]. The  input was %s.\n",
                     MAX_PREAMBLE_TYPE, argv[0]);
                ret = UAP_FAILURE;
            }
        }
        break;

    case PWK_CIPHER:
        if ((argc != 1) && (argc != 2)) {
            printf("ERR:Incorrect number of arguments for pwk_cipher.\n");
            ret = UAP_FAILURE;
        } else {
            if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) & ~PROTOCOL_BITMAP)) {
                printf("Invalid Protocol paramter.\n");
                ret = UAP_FAILURE;
            }
            if (argc == 2) {
                if ((ISDIGIT(argv[1]) == 0) || (atoi(argv[1]) & ~CIPHER_BITMAP)) {
                    printf("Invalid pairwise cipher.\n");
                    ret = UAP_FAILURE;
                }
            }
        }
        break;
    case GWK_CIPHER:
        if (argc != 1) {
            printf("ERR:Incorrect number of arguments for gwk_cipher.\n");
            ret = UAP_FAILURE;
        } else {
            if ((ISDIGIT(argv[0]) == 0) || (atoi(argv[0]) & ~CIPHER_BITMAP) ||
                (atoi(argv[0]) == AES_CCMP_TKIP)) {
                printf("Invalid group cipher.\n");
                ret = UAP_FAILURE;
            }
        }
        break;
    default:
        ret = UAP_FAILURE;
        break;
    }
    return ret;
}

/** 
 *  @brief Converts colon separated MAC address to hex value
 *
 *  @param mac      A pointer to the colon separated MAC string
 *  @param raw      A pointer to the hex data buffer
 *  @return         UAP_SUCCESS or UAP_FAILURE
 *                  UAP_RET_MAC_BROADCAST  - if broadcast mac
 *                  UAP_RET_MAC_MULTICAST - if multicast mac
 */
int
mac2raw(char *mac, t_u8 * raw)
{
    unsigned int temp_raw[ETH_ALEN];
    int num_tokens = 0;
    int i;
    if (strlen(mac) != ((2 * ETH_ALEN) + (ETH_ALEN - 1))) {
        return UAP_FAILURE;
    }
    num_tokens = sscanf(mac, "%2x:%2x:%2x:%2x:%2x:%2x",
                        temp_raw + 0, temp_raw + 1, temp_raw + 2, temp_raw + 3,
                        temp_raw + 4, temp_raw + 5);
    if (num_tokens != ETH_ALEN) {
        return UAP_FAILURE;
    }
    for (i = 0; i < num_tokens; i++)
        raw[i] = (t_u8) temp_raw[i];

    if (memcmp(raw, "\xff\xff\xff\xff\xff\xff", ETH_ALEN) == 0) {
        return UAP_RET_MAC_BROADCAST;
    } else if (raw[0] & 0x01) {
        return UAP_RET_MAC_MULTICAST;
    }
    return UAP_SUCCESS;
}

/** 
 *  @brief Converts a string to hex value
 *
 *  @param str      A pointer to the string
 *  @param raw      A pointer to the raw data buffer
 *  @return         Number of bytes read
 */
int
string2raw(char *str, unsigned char *raw)
{
    int len = (strlen(str) + 1) / 2;

    do {
        if (!isxdigit(*str)) {
            return -1;
        }
        *str = toupper(*str);
        *raw = CHAR2INT(*str) << 4;
        ++str;
        *str = toupper(*str);
        if (*str == '\0')
            break;
        *raw |= CHAR2INT(*str);
        ++raw;
    } while (*++str != '\0');
    return len;
}

/** 
 *  @brief Prints a MAC address in colon separated form from hex data
 *
 *  @param raw      A pointer to the hex data buffer
 *  @return         N/A
 */
void
print_mac(t_u8 * raw)
{
    printf("%02x:%02x:%02x:%02x:%02x:%02x", (unsigned int) raw[0],
           (unsigned int) raw[1], (unsigned int) raw[2], (unsigned int) raw[3],
           (unsigned int) raw[4], (unsigned int) raw[5]);
    return;
}

/** 
 *  @brief 		Check hex string
 *  
 *  @param hex		A pointer to hex string
 *  @return      	UAP_SUCCESS or UAP_FAILURE
 */
int
ishexstring(void *hex)
{
    int i, a;
    char *p = hex;
    int len = strlen(p);
    if (!strncasecmp("0x", p, 2)) {
        p += 2;
        len -= 2;
    }
    for (i = 0; i < len; i++) {
        a = hex2num(*p);
        if (a < 0)
            return UAP_FAILURE;
        p++;
    }
    return UAP_SUCCESS;
}

/**
 *  @brief Show auth tlv 
 *
 *  @param tlv     Pointer to auth tlv
 *  
 *  $return         N/A
 */
void
print_auth(tlvbuf_auth_mode * tlv)
{
    switch (tlv->auth_mode) {
    case 0:
        printf("AUTHMODE = Open authentication\n");
        break;
    case 1:
        printf("AUTHMODE = Shared key authentication\n");
        break;
    case 2:
        printf("AUTHMODE = Auto (open and shared key)\n");
        break;
    default:
        printf("ERR: Invalid authmode=%d\n", tlv->auth_mode);
        break;
    }
}

/**
 *
 *  @brief Show cipher tlv 
 *
 *  @param tlv     Pointer to cipher tlv
 *  
 *  $return         N/A
 */
void
print_cipher(tlvbuf_cipher * tlv)
{
    switch (tlv->pairwise_cipher) {
    case CIPHER_TKIP:
        printf("PairwiseCipher = TKIP\n");
        break;
    case CIPHER_AES_CCMP:
        printf("PairwiseCipher = AES CCMP\n");
        break;
    case CIPHER_TKIP | CIPHER_AES_CCMP:
        printf("PairwiseCipher = TKIP + AES CCMP\n");
        break;
    case CIPHER_NONE:
        printf("PairwiseCipher =  None\n");
        break;
    default:
        printf("Unknown Pairwise cipher 0x%x\n", tlv->pairwise_cipher);
        break;
    }
    switch (tlv->group_cipher) {
    case CIPHER_TKIP:
        printf("GroupCipher = TKIP\n");
        break;
    case CIPHER_AES_CCMP:
        printf("GroupCipher = AES CCMP\n");
        break;
    case CIPHER_NONE:
        printf("GroupCipher = None\n");
        break;
    default:
        printf("Unknown Group cipher 0x%x\n", tlv->group_cipher);
        break;
    }
}

/**
 *  @brief Show pairwise cipher tlv 
 *
 *  @param tlv     Pointer to pairwise cipher tlv
 *  
 *  $return         N/A
 */
void
print_pwk_cipher(tlvbuf_pwk_cipher * tlv)
{
    switch (tlv->protocol) {
    case PROTOCOL_WPA:
        printf("Protocol WPA  : ");
        break;
    case PROTOCOL_WPA2:
        printf("Protocol WPA2 : ");
        break;
    default:
        printf("Unknown Protocol 0x%x\n", tlv->protocol);
        break;
    }

    switch (tlv->pairwise_cipher) {
    case CIPHER_TKIP:
        printf("PairwiseCipher = TKIP\n");
        break;
    case CIPHER_AES_CCMP:
        printf("PairwiseCipher = AES CCMP\n");
        break;
    case CIPHER_TKIP | CIPHER_AES_CCMP:
        printf("PairwiseCipher = TKIP + AES CCMP\n");
        break;
    case CIPHER_NONE:
        printf("PairwiseCipher =  None\n");
        break;
    default:
        printf("Unknown Pairwise cipher 0x%x\n", tlv->pairwise_cipher);
        break;
    }
}

/**
 *  @brief Show group cipher tlv 
 *
 *  @param tlv     Pointer to group cipher tlv
 *  
 *  $return         N/A
 */
void
print_gwk_cipher(tlvbuf_gwk_cipher * tlv)
{
    switch (tlv->group_cipher) {
    case CIPHER_TKIP:
        printf("GroupCipher = TKIP\n");
        break;
    case CIPHER_AES_CCMP:
        printf("GroupCipher = AES CCMP\n");
        break;
    case CIPHER_NONE:
        printf("GroupCipher = None\n");
        break;
    default:
        printf("Unknown Group cipher 0x%x\n", tlv->group_cipher);
        break;
    }
}

/**
 *  @brief Show mac filter tlv 
 *
 *  @param tlv     Pointer to filter tlv
 *  
 *  $return         N/A
 */
void
print_mac_filter(tlvbuf_sta_mac_addr_filter * tlv)
{
    int i;
    switch (tlv->filter_mode) {
    case 0:
        printf("Filter Mode = Filter table is disabled\n");
        return;
    case 1:
        if (!tlv->count) {
            printf("No mac address is allowed to connect\n");
        } else {
            printf
                ("Filter Mode = Allow mac address specified in the allowed list\n");
        }
        break;
    case 2:
        if (!tlv->count) {
            printf("No mac address is blocked\n");
        } else {
            printf
                ("Filter Mode = Block MAC addresses specified in the banned list\n");
        }
        break;
    }
    for (i = 0; i < tlv->count; i++) {
        printf("MAC_%d = ", i);
        print_mac(&tlv->mac_address[i * ETH_ALEN]);
        printf("\n");
    }
}

/**
 *  @brief Show rate tlv 
 *
 *  @param tlv      Pointer to rate tlv
 *  
 *  $return         N/A
 */
void
print_rate(tlvbuf_rates * tlv)
{
    int flag = 0;
    int i;
    t_u16 tlv_len;

    tlv_len = *(t_u8 *) & tlv->length;
    tlv_len |= (*((t_u8 *) & tlv->length + 1) << 8);

    printf("Basic Rates =");
    for (i = 0; i < tlv_len; i++) {
        if (tlv->operational_rates[i] > (BASIC_RATE_SET_BIT - 1)) {
            flag = flag ? : 1;
            printf(" 0x%x", tlv->operational_rates[i]);
        }
    }
    printf("%s\nNon-Basic Rates =", flag ? "" : " ( none ) ");
    for (flag = 0, i = 0; i < tlv_len; i++) {
        if (tlv->operational_rates[i] < BASIC_RATE_SET_BIT) {
            flag = flag ? : 1;
            printf(" 0x%x", tlv->operational_rates[i]);
        }
    }
    printf("%s\n", flag ? "" : " ( none ) ");
}

/**
 *  @brief Show all the tlv in the buf
 *
 *  @param buf     Pointer to tlv buffer
 *  @param len     Tlv buffer len
 *  
 *  $return         N/A
 */
void
print_tlv(t_u8 * buf, t_u16 len)
{
    tlvbuf_header *pcurrent_tlv = (tlvbuf_header *) buf;
    int tlv_buf_left = len;
    t_u16 tlv_type;
    t_u16 tlv_len;
    t_u16 tlv_val_16;
    t_u32 tlv_val_32;
    t_u8 ssid[33];
    int i = 0;
    tlvbuf_ap_mac_address *mac_tlv;
    tlvbuf_ssid *ssid_tlv;
    tlvbuf_beacon_period *beacon_tlv;
    tlvbuf_dtim_period *dtim_tlv;
    tlvbuf_rates *rates_tlv;
    tlvbuf_tx_power *txpower_tlv;
    tlvbuf_bcast_ssid_ctl *bcast_tlv;
    tlvbuf_preamble_ctl *preamble_tlv;
    tlvbuf_bss_status *bss_status_tlv;
    tlvbuf_antenna_ctl *antenna_tlv;
    tlvbuf_rts_threshold *rts_tlv;
    tlvbuf_radio_ctl *radio_tlv;
    tlvbuf_tx_data_rate *txrate_tlv;
    tlvbuf_mcbc_data_rate *mcbcrate_tlv;
    tlvbuf_pkt_fwd_ctl *pkt_fwd_tlv;
    tlvbuf_sta_ageout_timer *ageout_tlv;
    tlvbuf_ps_sta_ageout_timer *ps_ageout_tlv;
    tlvbuf_auth_mode *auth_tlv;
    tlvbuf_protocol *proto_tlv;
    tlvbuf_akmp *akmp_tlv;
    tlvbuf_cipher *cipher_tlv;
    tlvbuf_pwk_cipher *pwk_cipher_tlv;
    tlvbuf_gwk_cipher *gwk_cipher_tlv;
    tlvbuf_group_rekey_timer *rekey_tlv;
    tlvbuf_wpa_passphrase *psk_tlv;
    tlvbuf_wep_key *wep_tlv;
    tlvbuf_frag_threshold *frag_tlv;
    tlvbuf_sta_mac_addr_filter *filter_tlv;
    tlvbuf_max_sta_num *max_sta_tlv;
    tlvbuf_retry_limit *retry_limit_tlv;
    tlvbuf_eapol_pwk_hsk_timeout *pwk_timeout_tlv;
    tlvbuf_eapol_pwk_hsk_retries *pwk_retries_tlv;
    tlvbuf_eapol_gwk_hsk_timeout *gwk_timeout_tlv;
    tlvbuf_eapol_gwk_hsk_retries *gwk_retries_tlv;
    tlvbuf_channel_config *channel_tlv;
    tlvbuf_channel_list *chnlist_tlv;
    channel_list *pchan_list;
    t_u16 custom_ie_len;
    tlvbuf_rsn_replay_prot *replay_prot_tlv;
    tlvbuf_custom_ie *custom_ie_tlv;
    custom_ie *custom_ie_ptr;
    tlvbuf_wmm_para_t *wmm_para_tlv;
    tlvbuf_htcap_t *ht_cap_tlv;

#ifdef DEBUG
    uap_printf(MSG_DEBUG, "tlv total len=%d\n", len);
#endif
    while (tlv_buf_left >= (int) sizeof(tlvbuf_header)) {
        tlv_type = *(t_u8 *) & pcurrent_tlv->type;
        tlv_type |= (*((t_u8 *) & pcurrent_tlv->type + 1) << 8);
        tlv_len = *(t_u8 *) & pcurrent_tlv->len;
        tlv_len |= (*((t_u8 *) & pcurrent_tlv->len + 1) << 8);
        if ((sizeof(tlvbuf_header) + tlv_len) > (unsigned int) tlv_buf_left) {
            printf("wrong tlv: tlv_len=%d, tlv_buf_left=%d\n", tlv_len,
                   tlv_buf_left);
            break;
        }
        switch (tlv_type) {
        case MRVL_AP_MAC_ADDRESS_TLV_ID:
            mac_tlv = (tlvbuf_ap_mac_address *) pcurrent_tlv;
            printf("AP MAC address = ");
            print_mac(mac_tlv->ap_mac_addr);
            printf("\n");
            break;
        case MRVL_SSID_TLV_ID:
            memset(ssid, 0, sizeof(ssid));
            ssid_tlv = (tlvbuf_ssid *) pcurrent_tlv;
            memcpy(ssid, ssid_tlv->ssid, tlv_len);
            printf("SSID = %s\n", ssid);
            break;
        case MRVL_BEACON_PERIOD_TLV_ID:
            beacon_tlv = (tlvbuf_beacon_period *) pcurrent_tlv;
            tlv_val_16 = *(t_u8 *) & beacon_tlv->beacon_period_ms;
            tlv_val_16 |= (*((t_u8 *) & beacon_tlv->beacon_period_ms + 1) << 8);
            printf("Beacon period = %d\n", tlv_val_16);
            break;
        case MRVL_DTIM_PERIOD_TLV_ID:
            dtim_tlv = (tlvbuf_dtim_period *) pcurrent_tlv;
            printf("DTIM period = %d\n", dtim_tlv->dtim_period);
            break;
        case MRVL_CHANNELCONFIG_TLV_ID:
            channel_tlv = (tlvbuf_channel_config *) pcurrent_tlv;
            printf("Channel = %d\n", channel_tlv->chan_number);
            printf("Channel Select Mode = %s\n",
                   (channel_tlv->
                    band_config_type & BAND_CONFIG_ACS_MODE) ? "ACS" :
                   "Manual");
            channel_tlv->band_config_type &= 0x30;
            if (channel_tlv->band_config_type == 0)
                printf("no secondary channel\n");
            else if (channel_tlv->band_config_type == SECOND_CHANNEL_ABOVE)
                printf("secondary channel is above primary channel\n");
            else if (channel_tlv->band_config_type == SECOND_CHANNEL_BELOW)
                printf("secondary channel is below primary channel\n");
            break;
        case MRVL_CHANNELLIST_TLV_ID:
            chnlist_tlv = (tlvbuf_channel_list *) pcurrent_tlv;
            printf("Channels List = ");
            pchan_list = (channel_list *) & (chnlist_tlv->chan_list);
            if (tlv_len % sizeof(channel_list)) {
                break;
            }
            for (i = 0; (unsigned int) i < (tlv_len / sizeof(channel_list));
                 i++) {
                printf("%d ", pchan_list->chan_number);
                pchan_list++;
            }
            printf("\n");
            break;
        case MRVL_RSN_REPLAY_PROT_TLV_ID:
            replay_prot_tlv = (tlvbuf_rsn_replay_prot *) pcurrent_tlv;
            printf("RSN replay protection = %s\n",
                   replay_prot_tlv->rsn_replay_prot ? "enabled" : "disabled");
            break;
        case MRVL_RATES_TLV_ID:
            rates_tlv = (tlvbuf_rates *) pcurrent_tlv;
            print_rate(rates_tlv);
            break;
        case MRVL_TX_POWER_TLV_ID:
            txpower_tlv = (tlvbuf_tx_power *) pcurrent_tlv;
            printf("Tx power = %d dBm\n", txpower_tlv->tx_power_dbm);
            break;
        case MRVL_BCAST_SSID_CTL_TLV_ID:
            bcast_tlv = (tlvbuf_bcast_ssid_ctl *) pcurrent_tlv;
            printf("SSID broadcast = %s\n",
                   (bcast_tlv->bcast_ssid_ctl == 1) ? "enabled" : "disabled");
            break;
        case MRVL_PREAMBLE_CTL_TLV_ID:
            preamble_tlv = (tlvbuf_preamble_ctl *) pcurrent_tlv;
            printf("Preamble type = %s\n", (preamble_tlv->preamble_type == 0) ?
                   "auto" : ((preamble_tlv->preamble_type == 1) ? "short" :
                             "long"));
            break;
        case MRVL_BSS_STATUS_TLV_ID:
            bss_status_tlv = (tlvbuf_bss_status *) pcurrent_tlv;
            printf("BSS status = %s\n",
                   (bss_status_tlv->bss_status == 0) ? "stopped" : "started");
            break;
        case MRVL_ANTENNA_CTL_TLV_ID:
            antenna_tlv = (tlvbuf_antenna_ctl *) pcurrent_tlv;
            printf("%s antenna = %s\n", (antenna_tlv->which_antenna == 0) ?
                   "Rx" : "Tx", (antenna_tlv->antenna_mode == 0) ? "A" : "B");
            break;
        case MRVL_RTS_THRESHOLD_TLV_ID:
            rts_tlv = (tlvbuf_rts_threshold *) pcurrent_tlv;
            tlv_val_16 = *(t_u8 *) & rts_tlv->rts_threshold;
            tlv_val_16 |= (*((t_u8 *) & rts_tlv->rts_threshold + 1) << 8);
            printf("RTS threshold = %d\n", tlv_val_16);
            break;
        case MRVL_FRAG_THRESHOLD_TLV_ID:
            frag_tlv = (tlvbuf_frag_threshold *) pcurrent_tlv;
            tlv_val_16 = *(t_u8 *) & frag_tlv->frag_threshold;
            tlv_val_16 |= (*((t_u8 *) & frag_tlv->frag_threshold + 1) << 8);
            printf("Fragmentation threshold = %d\n", tlv_val_16);
            break;
        case MRVL_RADIO_CTL_TLV_ID:
            radio_tlv = (tlvbuf_radio_ctl *) pcurrent_tlv;
            printf("Radio = %s\n", (radio_tlv->radio_ctl == 0) ? "on" : "off");
            break;
        case MRVL_TX_DATA_RATE_TLV_ID:
            txrate_tlv = (tlvbuf_tx_data_rate *) pcurrent_tlv;
            tlv_val_16 = *(t_u8 *) & txrate_tlv->tx_data_rate;
            tlv_val_16 |= (*((t_u8 *) & txrate_tlv->tx_data_rate + 1) << 8);
            if (txrate_tlv->tx_data_rate == 0)
                printf("Tx data rate = auto\n");
            else
                printf("Tx data rate = 0x%x\n", tlv_val_16);
            break;
        case MRVL_MCBC_DATA_RATE_TLV_ID:
            mcbcrate_tlv = (tlvbuf_mcbc_data_rate *) pcurrent_tlv;
            tlv_val_16 = *(t_u8 *) & mcbcrate_tlv->mcbc_datarate;
            tlv_val_16 |= (*((t_u8 *) & mcbcrate_tlv->mcbc_datarate + 1) << 8);
            if (mcbcrate_tlv->mcbc_datarate == 0)
                printf("MCBC data rate = auto\n");
            else
                printf("MCBC data rate = 0x%x\n", tlv_val_16);
            break;
        case MRVL_PKT_FWD_CTL_TLV_ID:
            pkt_fwd_tlv = (tlvbuf_pkt_fwd_ctl *) pcurrent_tlv;
            printf("Firmware = %s\n", (pkt_fwd_tlv->pkt_fwd_ctl == 0) ?
                   "forwards all packets to the host" :
                   "handles intra-BSS packets");
            break;
        case MRVL_STA_AGEOUT_TIMER_TLV_ID:
            ageout_tlv = (tlvbuf_sta_ageout_timer *) pcurrent_tlv;
            tlv_val_32 = *(t_u8 *) & ageout_tlv->sta_ageout_timer_ms;
            tlv_val_32 |=
                (*((t_u8 *) & ageout_tlv->sta_ageout_timer_ms + 1) << 8);
            tlv_val_32 |=
                (*((t_u8 *) & ageout_tlv->sta_ageout_timer_ms + 2) << 16);
            tlv_val_32 |=
                (*((t_u8 *) & ageout_tlv->sta_ageout_timer_ms + 3) << 24);
            printf("STA ageout timer = %d\n", (int) tlv_val_32);
            break;
        case MRVL_PS_STA_AGEOUT_TIMER_TLV_ID:
            ps_ageout_tlv = (tlvbuf_ps_sta_ageout_timer *) pcurrent_tlv;
            tlv_val_32 = *(t_u8 *) & ps_ageout_tlv->ps_sta_ageout_timer_ms;
            tlv_val_32 |=
                (*((t_u8 *) & ps_ageout_tlv->ps_sta_ageout_timer_ms + 1) << 8);
            tlv_val_32 |=
                (*((t_u8 *) & ps_ageout_tlv->ps_sta_ageout_timer_ms + 2) << 16);
            tlv_val_32 |=
                (*((t_u8 *) & ps_ageout_tlv->ps_sta_ageout_timer_ms + 3) << 24);
            printf("PS STA ageout timer = %d\n", (int) tlv_val_32);
            break;
        case MRVL_AUTH_TLV_ID:
            auth_tlv = (tlvbuf_auth_mode *) pcurrent_tlv;
            print_auth(auth_tlv);
            break;
        case MRVL_PROTOCOL_TLV_ID:
            proto_tlv = (tlvbuf_protocol *) pcurrent_tlv;
            tlv_val_16 = *(t_u8 *) & proto_tlv->protocol;
            tlv_val_16 |= (*((t_u8 *) & proto_tlv->protocol + 1) << 8);
            print_protocol(tlv_val_16);
            break;
        case MRVL_AKMP_TLV_ID:
            akmp_tlv = (tlvbuf_akmp *) pcurrent_tlv;
            tlv_val_16 = *(t_u8 *) & akmp_tlv->key_mgmt;
            tlv_val_16 |= (*((t_u8 *) & akmp_tlv->key_mgmt + 1) << 8);
            if (tlv_val_16 == KEY_MGMT_PSK)
                printf("KeyMgmt = PSK\n");
            else
                printf("KeyMgmt = NONE\n");
            break;
        case MRVL_CIPHER_TLV_ID:
            cipher_tlv = (tlvbuf_cipher *) pcurrent_tlv;
            print_cipher(cipher_tlv);
            break;
        case MRVL_CIPHER_PWK_TLV_ID:
            pwk_cipher_tlv = (tlvbuf_pwk_cipher *) pcurrent_tlv;
            pwk_cipher_tlv->protocol =
                uap_le16_to_cpu(pwk_cipher_tlv->protocol);
            print_pwk_cipher(pwk_cipher_tlv);
            break;
        case MRVL_CIPHER_GWK_TLV_ID:
            gwk_cipher_tlv = (tlvbuf_gwk_cipher *) pcurrent_tlv;
            print_gwk_cipher(gwk_cipher_tlv);
            break;
        case MRVL_GRP_REKEY_TIME_TLV_ID:
            rekey_tlv = (tlvbuf_group_rekey_timer *) pcurrent_tlv;
            tlv_val_32 = *(t_u8 *) & rekey_tlv->group_rekey_time_sec;
            tlv_val_32 |=
                (*((t_u8 *) & rekey_tlv->group_rekey_time_sec + 1) << 8);
            tlv_val_32 |=
                (*((t_u8 *) & rekey_tlv->group_rekey_time_sec + 2) << 16);
            tlv_val_32 |=
                (*((t_u8 *) & rekey_tlv->group_rekey_time_sec + 3) << 24);
            if (tlv_val_32 == 0)
                printf("Group re-key time = disabled\n");
            else
                printf("Group re-key time = %ld second\n", tlv_val_32);
            break;
        case MRVL_WPA_PASSPHRASE_TLV_ID:
            psk_tlv = (tlvbuf_wpa_passphrase *) pcurrent_tlv;
            if (tlv_len > 0) {
                printf("WPA passphrase = ");
                for (i = 0; i < tlv_len; i++)
                    printf("%c", psk_tlv->passphrase[i]);
                printf("\n");
            } else
                printf("WPA passphrase = None\n");
            break;
        case MRVL_WEP_KEY_TLV_ID:
            wep_tlv = (tlvbuf_wep_key *) pcurrent_tlv;
            print_wep_key(wep_tlv);
            break;
        case MRVL_STA_MAC_ADDR_FILTER_TLV_ID:
            filter_tlv = (tlvbuf_sta_mac_addr_filter *) pcurrent_tlv;
            print_mac_filter(filter_tlv);
            break;
        case MRVL_MAX_STA_CNT_TLV_ID:
            max_sta_tlv = (tlvbuf_max_sta_num *) pcurrent_tlv;
            tlv_val_16 = *(t_u8 *) & max_sta_tlv->max_sta_num;
            tlv_val_16 |= (*((t_u8 *) & max_sta_tlv->max_sta_num + 1) << 8);
            printf("Max Station Number = %d\n", tlv_val_16);
            break;
        case MRVL_RETRY_LIMIT_TLV_ID:
            retry_limit_tlv = (tlvbuf_retry_limit *) pcurrent_tlv;
            printf("Retry Limit = %d\n", retry_limit_tlv->retry_limit);
            break;
        case MRVL_EAPOL_PWK_HSK_TIMEOUT_TLV_ID:
            pwk_timeout_tlv = (tlvbuf_eapol_pwk_hsk_timeout *) pcurrent_tlv;
            pwk_timeout_tlv->pairwise_update_timeout =
                uap_le32_to_cpu(pwk_timeout_tlv->pairwise_update_timeout);
            printf("Pairwise handshake timeout = %ld\n",
                   pwk_timeout_tlv->pairwise_update_timeout);
            break;
        case MRVL_EAPOL_PWK_HSK_RETRIES_TLV_ID:
            pwk_retries_tlv = (tlvbuf_eapol_pwk_hsk_retries *) pcurrent_tlv;
            pwk_retries_tlv->pwk_retries =
                uap_le32_to_cpu(pwk_retries_tlv->pwk_retries);
            printf("Pairwise handshake retries = %ld\n",
                   pwk_retries_tlv->pwk_retries);
            break;
        case MRVL_EAPOL_GWK_HSK_TIMEOUT_TLV_ID:
            gwk_timeout_tlv = (tlvbuf_eapol_gwk_hsk_timeout *) pcurrent_tlv;
            gwk_timeout_tlv->groupwise_update_timeout =
                uap_le32_to_cpu(gwk_timeout_tlv->groupwise_update_timeout);
            printf("Groupwise handshake timeout = %ld\n",
                   gwk_timeout_tlv->groupwise_update_timeout);
            break;
        case MRVL_EAPOL_GWK_HSK_RETRIES_TLV_ID:
            gwk_retries_tlv = (tlvbuf_eapol_gwk_hsk_retries *) pcurrent_tlv;
            gwk_retries_tlv->gwk_retries =
                uap_le32_to_cpu(gwk_retries_tlv->gwk_retries);
            printf("Groupwise handshake retries = %ld\n",
                   gwk_retries_tlv->gwk_retries);
            break;
        case MRVL_MGMT_IE_LIST_TLV_ID:
            custom_ie_tlv = (tlvbuf_custom_ie *) pcurrent_tlv;
            custom_ie_len = tlv_len;
            custom_ie_ptr = (custom_ie *) (custom_ie_tlv->ie_data);
            while (custom_ie_len >= sizeof(custom_ie)) {
                custom_ie_ptr->ie_index =
                    uap_le16_to_cpu(custom_ie_ptr->ie_index);
                custom_ie_ptr->ie_length =
                    uap_le16_to_cpu(custom_ie_ptr->ie_length);
                custom_ie_ptr->mgmt_subtype_mask =
                    uap_le16_to_cpu(custom_ie_ptr->mgmt_subtype_mask);
                printf("Index [%d]\n", custom_ie_ptr->ie_index);
                if (custom_ie_ptr->ie_length)
                    printf("Management Subtype Mask = 0x%02x\n",
                           custom_ie_ptr->mgmt_subtype_mask == 0 ?
                           UAP_CUSTOM_IE_AUTO_MASK :
                           custom_ie_ptr->mgmt_subtype_mask);
                else
                    printf("Management Subtype Mask = 0x%02x\n",
                           custom_ie_ptr->mgmt_subtype_mask);
                hexdump_data("IE Buffer", (void *) custom_ie_ptr->ie_buffer,
                             (custom_ie_ptr->ie_length), ' ');
                custom_ie_len -= sizeof(custom_ie) + custom_ie_ptr->ie_length;
                custom_ie_ptr =
                    (custom_ie *) ((t_u8 *) custom_ie_ptr + sizeof(custom_ie) +
                                   custom_ie_ptr->ie_length);
            }
            break;

        case HT_CAPABILITY_TLV_ID:
            printf("\nHT Capability Info: \n");
            ht_cap_tlv = (tlvbuf_htcap_t *) pcurrent_tlv;
            if (!ht_cap_tlv->ht_cap.supported_mcs_set[0]) {
                printf("802.11n is disabled\n");
            } else {
                printf("802.11n is enabled\n");
                printf("ht_cap_info=0x%x, ampdu_param=0x%x, tx_bf_cap=%#lx\n",
                       uap_le16_to_cpu(ht_cap_tlv->ht_cap.ht_cap_info),
                       ht_cap_tlv->ht_cap.ampdu_param,
                       uap_le32_to_cpu(ht_cap_tlv->ht_cap.tx_bf_cap));
            }
            break;
        case VENDOR_SPECIFIC_IE_TLV_ID:
            wmm_para_tlv = (tlvbuf_wmm_para_t *) pcurrent_tlv;
            printf("wmm parameters:\n");
            printf("\tqos_info = 0x%x\n", wmm_para_tlv->wmm_para.qos_info);
            printf("\tBE: AIFSN=%d, CW_MAX=%d CW_MIN=%d, TXOP=%d\n",
                   wmm_para_tlv->wmm_para.ac_params[AC_BE].aci_aifsn.aifsn,
                   wmm_para_tlv->wmm_para.ac_params[AC_BE].ecw.ecw_max,
                   wmm_para_tlv->wmm_para.ac_params[AC_BE].ecw.ecw_min,
                   uap_le16_to_cpu(wmm_para_tlv->wmm_para.ac_params[AC_BE].
                                   tx_op_limit));
            printf("\tBK: AIFSN=%d, CW_MAX=%d CW_MIN=%d, TXOP=%d\n",
                   wmm_para_tlv->wmm_para.ac_params[AC_BK].aci_aifsn.aifsn,
                   wmm_para_tlv->wmm_para.ac_params[AC_BK].ecw.ecw_max,
                   wmm_para_tlv->wmm_para.ac_params[AC_BK].ecw.ecw_min,
                   uap_le16_to_cpu(wmm_para_tlv->wmm_para.ac_params[AC_BK].
                                   tx_op_limit));
            printf("\tVI: AIFSN=%d, CW_MAX=%d CW_MIN=%d, TXOP=%d\n",
                   wmm_para_tlv->wmm_para.ac_params[AC_VI].aci_aifsn.aifsn,
                   wmm_para_tlv->wmm_para.ac_params[AC_VI].ecw.ecw_max,
                   wmm_para_tlv->wmm_para.ac_params[AC_VI].ecw.ecw_min,
                   uap_le16_to_cpu(wmm_para_tlv->wmm_para.ac_params[AC_VI].
                                   tx_op_limit));
            printf("\tVO: AIFSN=%d, CW_MAX=%d CW_MIN=%d, TXOP=%d\n",
                   wmm_para_tlv->wmm_para.ac_params[AC_VO].aci_aifsn.aifsn,
                   wmm_para_tlv->wmm_para.ac_params[AC_VO].ecw.ecw_max,
                   wmm_para_tlv->wmm_para.ac_params[AC_VO].ecw.ecw_min,
                   uap_le16_to_cpu(wmm_para_tlv->wmm_para.ac_params[AC_VO].
                                   tx_op_limit));
            break;
        default:
            break;
        }
        tlv_buf_left -= (sizeof(tlvbuf_header) + tlv_len);
        pcurrent_tlv = (tlvbuf_header *) (pcurrent_tlv->data + tlv_len);
    }
    return;
}

/** 
 *  @brief Performs the ioctl operation to send the command to
 *  the driver.
 *
 *  @param cmd        	 Pointer to the command buffer
 *  @param size          Pointer to the command size. This value is
 *                       overwritten by the function with the size of the
 *                       received response.
 *  @param buf_size 	 Size of the allocated command buffer
 *  @return              UAP_SUCCESS or UAP_FAILURE
 */
int
uap_ioctl(t_u8 * cmd, t_u16 * size, t_u16 buf_size)
{
    struct ifreq ifr;
    apcmdbuf *header = NULL;
    t_s32 sockfd;

    if (buf_size < *size) {
        printf("buf_size should not less than cmd buffer size\n");
        return UAP_FAILURE;
    }

    /* Open socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("ERR:Cannot open socket\n");
        return UAP_FAILURE;
    }
    *(t_u32 *) cmd = buf_size - BUF_HEADER_SIZE;

    /* Initialize the ifr structure */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_ifrn.ifrn_name, dev_name, strlen(dev_name));
    ifr.ifr_ifru.ifru_data = (void *) cmd;
    header = (apcmdbuf *) cmd;
    header->size = *size - BUF_HEADER_SIZE;
    if (header->cmd_code == APCMD_SYS_CONFIGURE) {
        apcmdbuf_sys_configure *sys_cfg;
        sys_cfg = (apcmdbuf_sys_configure *) cmd;
        sys_cfg->action = uap_cpu_to_le16(sys_cfg->action);
    }
    endian_convert_request_header(header);
#if DEBUG
    /* Dump request buffer */
    hexdump("Request buffer", (void *) cmd, *size, ' ');
#endif
    /* Perform ioctl */
    errno = 0;
    if (ioctl(sockfd, UAPHOSTCMD, &ifr)) {
        perror("");
        printf("ERR:UAPHOSTCMD is not supported by %s\n", dev_name);
        close(sockfd);
        return UAP_FAILURE;
    }
    endian_convert_response_header(header);
    header->cmd_code &= HostCmd_CMD_ID_MASK;
    header->cmd_code |= APCMD_RESP_CHECK;
    *size = header->size;

    /* Validate response size */
    if (*size > (buf_size - BUF_HEADER_SIZE)) {
        printf
            ("ERR:Response size (%d) greater than buffer size (%d)! Aborting!\n",
             *size, buf_size);
        close(sockfd);
        return UAP_FAILURE;
    }
#if DEBUG
    /* Dump respond buffer */
    hexdump("Respond buffer", (void *) header, header->size + BUF_HEADER_SIZE,
            ' ');
#endif

    /* Close socket */
    close(sockfd);
    return UAP_SUCCESS;
}

/**
 *  @brief  Get protocol from the firmware
 *
 *  @param  proto   A pointer to protocol var
 *  @return         UAP_SUCCESS/UAP_FAILURE
 */
int
get_sys_cfg_protocol(t_u16 * proto)
{
    apcmdbuf_sys_configure *cmd_buf = NULL;
    tlvbuf_protocol *tlv = NULL;
    t_u8 *buffer = NULL;
    t_u16 cmd_len;
    int ret = UAP_FAILURE;

    cmd_len = sizeof(apcmdbuf_sys_configure) + sizeof(tlvbuf_protocol);
    /* Initialize the command buffer */
    buffer = (t_u8 *) malloc(cmd_len);
    if (!buffer) {
        printf("ERR:Cannot allocate buffer for command!\n");
        return ret;
    }
    memset(buffer, 0, cmd_len);
    /* Locate headers */
    cmd_buf = (apcmdbuf_sys_configure *) buffer;
    tlv = (tlvbuf_protocol *) (buffer + sizeof(apcmdbuf_sys_configure));
    /* Fill the command buffer */
    cmd_buf->cmd_code = APCMD_SYS_CONFIGURE;
    cmd_buf->size = cmd_len;
    cmd_buf->seq_num = 0;
    cmd_buf->result = 0;
    cmd_buf->action = ACTION_GET;
    tlv->tag = MRVL_PROTOCOL_TLV_ID;
    tlv->length = 2;
    endian_convert_tlv_header_out(tlv);
    /* Send the command */
    ret = uap_ioctl((t_u8 *) cmd_buf, &cmd_len, cmd_len);
    endian_convert_tlv_header_in(tlv);

    /* Process response */
    if (ret == UAP_SUCCESS) {
        /* Verify response */
        if (cmd_buf->cmd_code != (APCMD_SYS_CONFIGURE | APCMD_RESP_CHECK)) {
            printf("ERR:Corrupted response! cmd_code=%x, Tlv->tag=%x\n",
                   cmd_buf->cmd_code, tlv->tag);
            free(buffer);
            return UAP_FAILURE;
        }

        if (cmd_buf->result == CMD_SUCCESS) {
            tlv->protocol = uap_le16_to_cpu(tlv->protocol);
            memcpy(proto, &tlv->protocol, sizeof(tlv->protocol));
        } else {
            ret = UAP_FAILURE;
        }
    }
    if (buffer)
        free(buffer);
    return ret;
}

/** 
 *  @brief Check cipher is valid or not
 *
 *  @param pairwisecipher    Pairwise cipher
 *  @param groupcipher       Group cipher
 *  @param protocol          Protocol
 *  @param enable_11n        11n enabled or not.
 *  @return         UAP_SUCCESS or UAP_FAILURE
 */
int
is_cipher_valid_with_11n(int pairwisecipher, int groupcipher,
                         int protocol, int enable_11n)
{
    if ((pairwisecipher == CIPHER_NONE) && (groupcipher == CIPHER_NONE))
        return UAP_SUCCESS;
    if (pairwisecipher == CIPHER_TKIP) {
        /* Ok to have TKIP in mixed mode */
        if (enable_11n && protocol != PROTOCOL_WPA2_MIXED) {
            printf
                ("ERR: WPA/TKIP cannot be used when AP operates in 802.11n mode.\n");
            return UAP_FAILURE;
        }
    }
    if ((pairwisecipher == CIPHER_TKIP) && (groupcipher == CIPHER_TKIP))
        return UAP_SUCCESS;
    if ((pairwisecipher == CIPHER_AES_CCMP) && (groupcipher == CIPHER_AES_CCMP))
        return UAP_SUCCESS;
    if ((pairwisecipher == CIPHER_AES_CCMP) && (groupcipher == CIPHER_TKIP))
        return UAP_SUCCESS;
    if ((pairwisecipher == CIPHER_BITMAP) && (groupcipher == CIPHER_TKIP))
        return UAP_SUCCESS;
    return UAP_FAILURE;
}

/** 
 *  @brief Check cipher is valid or not based on proto
 *
 *  @param pairwisecipher    Pairwise cipher
 *  @param groupcipher       Group cipher
 *  @param protocol          Protocol
 *  @return         UAP_SUCCESS or UAP_FAILURE
 */
int
is_cipher_valid_with_proto(int pairwisecipher, int groupcipher, int protocol)
{
    HTCap_t htcap;

    if ((pairwisecipher == CIPHER_NONE) && (groupcipher == CIPHER_NONE))
        return UAP_SUCCESS;
    if (pairwisecipher == CIPHER_TKIP) {
        /* Ok to have TKIP in mixed mode */
        if (protocol != PROTOCOL_WPA2_MIXED) {
            memset(&htcap, 0, sizeof(htcap));
            if (UAP_SUCCESS == get_sys_cfg_11n(&htcap)) {
                if (htcap.supported_mcs_set[0]) {
                    printf
                        ("ERR: WPA/TKIP cannot be used when AP operates in 802.11n mode.\n");
                    return UAP_FAILURE;
                }
            }
        }
    }
    if ((pairwisecipher == CIPHER_TKIP) && (groupcipher == CIPHER_TKIP))
        return UAP_SUCCESS;
    if ((pairwisecipher == CIPHER_AES_CCMP) && (groupcipher == CIPHER_AES_CCMP))
        return UAP_SUCCESS;
    if ((pairwisecipher == CIPHER_AES_CCMP) && (groupcipher == CIPHER_TKIP))
        return UAP_SUCCESS;
    if ((pairwisecipher == CIPHER_BITMAP) && (groupcipher == CIPHER_TKIP))
        return UAP_SUCCESS;
    return UAP_FAILURE;
}

/** 
 *  @brief Check cipher is valid or not
 *
 *  @param pairwisecipher    Pairwise cipher
 *  @param groupcipher       Group cipher
 *  @return         UAP_SUCCESS or UAP_FAILURE
 */
int
is_cipher_valid(int pairwisecipher, int groupcipher)
{
    HTCap_t htcap;
    t_u16 proto = 0;

    if ((pairwisecipher == CIPHER_NONE) && (groupcipher == CIPHER_NONE))
        return UAP_SUCCESS;
    if (pairwisecipher == CIPHER_TKIP) {
        if (UAP_SUCCESS == get_sys_cfg_protocol(&proto)) {
            /* Ok to have TKIP in mixed mode */
            if (proto != PROTOCOL_WPA2_MIXED) {
                memset(&htcap, 0, sizeof(htcap));
                if (UAP_SUCCESS == get_sys_cfg_11n(&htcap)) {
                    if (htcap.supported_mcs_set[0]) {
                        printf
                            ("ERR: WPA/TKIP cannot be used when AP operates in 802.11n mode.\n");
                        return UAP_FAILURE;
                    }
                }
            }
        }
    }
    if ((pairwisecipher == CIPHER_TKIP) && (groupcipher == CIPHER_TKIP))
        return UAP_SUCCESS;
    if ((pairwisecipher == CIPHER_AES_CCMP) && (groupcipher == CIPHER_AES_CCMP))
        return UAP_SUCCESS;
    if ((pairwisecipher == CIPHER_AES_CCMP) && (groupcipher == CIPHER_TKIP))
        return UAP_SUCCESS;
    if ((pairwisecipher == CIPHER_BITMAP) && (groupcipher == CIPHER_TKIP))
        return UAP_SUCCESS;
    return UAP_FAILURE;
}

/** 
 *  @brief The main function
 *
 *  @param argc     Number of arguments
 *  @param argv     Pointer to the arguments
 *  @return         0 or 1
 */
int
main(int argc, char *argv[])
{
    int opt, i;
    int ret = 0;
    memset(dev_name, 0, sizeof(dev_name));
    strcpy(dev_name, DEFAULT_DEV_NAME);

    /* Parse arguments */
    while ((opt = getopt_long(argc, argv, "+hi:d:v", ap_options, NULL)) != -1) {
        switch (opt) {
        case 'i':
            if (strlen(optarg) < IFNAMSIZ) {
                memset(dev_name, 0, sizeof(dev_name));
                strncpy(dev_name, optarg, strlen(optarg));
            }
            printf("dev_name:%s\n", dev_name);
            break;
        case 'v':
            printf("uaputl.exe - uAP utility ver %s\n", UAP_VERSION);
            exit(0);
        case 'd':
            debug_level = strtoul(optarg, NULL, 10);
            uap_printf(MSG_DEBUG, "debug_level=%x\n", debug_level);
            break;
        case 'h':
        default:
            print_tool_usage();
            exit(0);
        }
    }

    argc -= optind;
    argv += optind;
    optind = 0;

    if (argc < 1) {
        print_tool_usage();
        exit(1);
    }

    /* Process command */
    for (i = 0; ap_command[i].cmd; i++) {
        if (strncmp(ap_command[i].cmd, argv[0], strlen(ap_command[i].cmd)))
            continue;
        if (strlen(ap_command[i].cmd) != strlen(argv[0]))
            continue;
        ret = ap_command[i].func(argc, argv);
        break;
    }
    if (!ap_command[i].cmd) {
        printf("ERR: %s is not supported\n", argv[0]);
        exit(1);
    }
    if (ret == UAP_FAILURE)
        return -1;
    else
        return 0;
}
