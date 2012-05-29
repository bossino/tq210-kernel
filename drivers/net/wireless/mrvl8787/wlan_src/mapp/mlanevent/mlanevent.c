/** @file  mlanevent.c
 *
 *  @brief Program to receive events from the driver/firmware of the uAP
 *         driver.
 *
 *   Usage: mlanevent.exe [-option]
 *
 *  Copyright (C) 2008-2009, Marvell International Ltd.
 *  All Rights Reserved
 */
/****************************************************************************
Change log:
    03/18/08: Initial creation
****************************************************************************/

/****************************************************************************
        Header files
****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <getopt.h>

#include <sys/socket.h>
#include <linux/netlink.h>
#include "mlanevent.h"

/****************************************************************************
        Definitions
****************************************************************************/
/** Enable or disable debug outputs */
#define DEBUG   0

/****************************************************************************
        Global variables
****************************************************************************/
/** Termination flag */
int terminate_flag = 0;

/****************************************************************************
        Local functions
****************************************************************************/
/**
 *  @brief Signal handler
 *
 *  @param sig      Received signal number
 *  @return         N/A
 */
void
sig_handler(int sig)
{
    printf("Stopping application.\n");
#if DEBUG
    printf("Process ID of process killed = %d\n", getpid());
#endif
    terminate_flag = 1;
}

/**
 *  @brief Dump hex data
 *
 *  @param p        A pointer to data buffer
 *  @param len      The len of data buffer
 *  @param delim    Deliminator character
 *  @return         Hex integer
 */
static void
hexdump(void *p, t_s32 len, t_s8 delim)
{
    t_s32 i;
    t_u8 *s = p;
    for (i = 0; i < len; i++) {
        if (i != len - 1)
            printf("%02x%c", *s++, delim);
        else
            printf("%02x\n", *s);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
}

/**
 *  @brief Prints a MAC address in colon separated form from raw data
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
 *  @brief Print usage information
 *
 *  @return         N/A
 */
void
print_usage(void)
{
    printf("\n");
    printf("Usage : mlanevent.exe [-v] [-h]\n");
    printf("    -v               : Print version information\n");
    printf("    -h               : Print help information\n");
    printf("\n");
}

/**
 *  @brief Parse and print STA deauthentication event data
 *
 *  @param buffer   Pointer to received event buffer
 *  @param size     Length of the received event data
 *  @return         N/A
 */
void
print_event_sta_deauth(t_u8 * buffer, t_u16 size)
{
    eventbuf_sta_deauth *event_body = NULL;

    if (size < sizeof(eventbuf_sta_deauth)) {
        printf("ERR:Event buffer too small!\n");
        return;
    }
    event_body = (eventbuf_sta_deauth *) buffer;
    event_body->reason_code = uap_le16_to_cpu(event_body->reason_code);
    printf("EVENT: STA_DEAUTH\n");
    printf("Deauthenticated STA MAC: ");
    print_mac(event_body->sta_mac_address);
    printf("\nReason: ");
    switch (event_body->reason_code) {
    case 1:
        printf("Client station leaving the network\n");
        break;
    case 2:
        printf("Client station aged out\n");
        break;
    case 3:
        printf("Client station deauthenticated by user's request\n");
        break;
    case 4:
        printf("Client station authentication failure\n");
        break;
    case 5:
        printf("Client station association failure\n");
        break;
    case 6:
        printf("Client mac address is blocked by ACL filter\n");
        break;
    case 7:
        printf("Client station table is full\n");
        break;
    case 8:
        printf("Client 4-way handshake timeout\n");
        break;
    case 9:
        printf("Client group key handshake timeout\n");
        break;
    default:
        printf("Unspecified\n");
        break;
    }
    return;
}

/** 
 *  @brief Prints mgmt frame
 *
 *  @param mgmt_tlv A pointer to mgmt_tlv
 *  @param tlv_len  Length of tlv payload
 *  @return         N/A
 */
void
print_mgmt_frame(MrvlIETypes_MgmtFrameSet_t * mgmt_tlv, int tlv_len)
{
    IEEEtypes_AssocRqst_t *assoc_req = NULL;
    IEEEtypes_ReAssocRqst_t *reassoc_req = NULL;
    IEEEtypes_AssocRsp_t *assoc_resp = NULL;
    t_u16 frm_ctl = 0;
    printf("\nMgmt Frame:\n");
    memcpy(&frm_ctl, &mgmt_tlv->frame_control, sizeof(t_u16));
    printf("FrameControl: 0x%x\n", frm_ctl);
    if (mgmt_tlv->frame_control.type != 0) {
        printf("Frame type=%d subtype=%d:\n", mgmt_tlv->frame_control.type,
               mgmt_tlv->frame_control.sub_type);
        hexdump(mgmt_tlv->frame_contents, tlv_len - sizeof(t_u16), ' ');
        return;
    }
    switch (mgmt_tlv->frame_control.sub_type) {
    case SUBTYPE_ASSOC_REQUEST:
        printf("Assoc Request:\n");
        assoc_req = (IEEEtypes_AssocRqst_t *) mgmt_tlv->frame_contents;
        printf("CapInfo: 0x%x  ListenInterval: 0x%x \n",
               uap_le16_to_cpu(assoc_req->cap_info),
               uap_le16_to_cpu(assoc_req->listen_interval));
        printf("AssocReqIE:\n");
        hexdump(assoc_req->ie_buffer, tlv_len - sizeof(IEEEtypes_AssocRqst_t)
                - sizeof(IEEEtypes_FrameCtl_t), ' ');
        break;
    case SUBTYPE_REASSOC_REQUEST:
        printf("ReAssoc Request:\n");
        reassoc_req = (IEEEtypes_ReAssocRqst_t *) mgmt_tlv->frame_contents;
        printf("CapInfo: 0x%x  ListenInterval: 0x%x \n",
               uap_le16_to_cpu(reassoc_req->cap_info),
               uap_le16_to_cpu(reassoc_req->listen_interval));
        printf("Current AP address: ");
        print_mac(reassoc_req->current_ap_addr);
        printf("\nReAssocReqIE:\n");
        hexdump(reassoc_req->ie_buffer,
                tlv_len - sizeof(IEEEtypes_ReAssocRqst_t)
                - sizeof(IEEEtypes_FrameCtl_t), ' ');
        break;
    case SUBTYPE_ASSOC_RESPONSE:
    case SUBTYPE_REASSOC_RESPONSE:
        if (mgmt_tlv->frame_control.sub_type == SUBTYPE_ASSOC_RESPONSE)
            printf("Assoc Response:\n");
        else
            printf("ReAssoc Response:\n");
        assoc_resp = (IEEEtypes_AssocRsp_t *) mgmt_tlv->frame_contents;
        printf("CapInfo: 0x%x  StatusCode: %d  AID: 0x%x \n",
               uap_le16_to_cpu(assoc_resp->cap_info),
               (int) (uap_le16_to_cpu(assoc_resp->status_code)),
               uap_le16_to_cpu(assoc_resp->aid) & 0x3fff);
        break;
    default:
        printf("Frame subtype = %d:\n", mgmt_tlv->frame_control.sub_type);
        hexdump(mgmt_tlv->frame_contents, tlv_len - sizeof(t_u16), ' ');
        break;
    }
    return;
}

/**
 *  @brief Parse and print RSN connect event data
 *
 *  @param buffer   Pointer to received buffer
 *  @param size     Length of the received event data
 *  @return         N/A
 */
void
print_event_rsn_connect(t_u8 * buffer, t_u16 size)
{
    int tlv_buf_left = size;
    t_u16 tlv_type, tlv_len;
    tlvbuf_header *tlv = NULL;
    eventbuf_rsn_connect *event_body = NULL;
    if (size < sizeof(eventbuf_rsn_connect)) {
        printf("ERR:Event buffer too small!\n");
        return;
    }
    event_body = (eventbuf_rsn_connect *) buffer;
    printf("EVENT: RSN_CONNECT\n");
    printf("Station MAC: ");
    print_mac(event_body->sta_mac_address);
    printf("\n");
    tlv_buf_left = size - sizeof(eventbuf_rsn_connect);
    if (tlv_buf_left < (int) sizeof(tlvbuf_header))
        return;
    tlv = (tlvbuf_header *) (buffer + sizeof(eventbuf_rsn_connect));

    while (tlv_buf_left >= (int) sizeof(tlvbuf_header)) {
        tlv_type = uap_le16_to_cpu(tlv->type);
        tlv_len = uap_le16_to_cpu(tlv->len);
        if ((sizeof(tlvbuf_header) + tlv_len) > (unsigned int) tlv_buf_left) {
            printf("wrong tlv: tlvLen=%d, tlvBufLeft=%d\n", tlv_len,
                   tlv_buf_left);
            break;
        }
        switch (tlv_type) {
        case IEEE_WPA_IE:
            printf("WPA IE:\n");
            hexdump((t_u8 *) tlv + sizeof(tlvbuf_header), tlv_len, ' ');
            break;
        case IEEE_RSN_IE:
            printf("RSN IE:\n");
            hexdump((t_u8 *) tlv + sizeof(tlvbuf_header), tlv_len, ' ');
            break;
        default:
            printf("unknown tlv: %d\n", tlv_type);
            break;
        }
        tlv_buf_left -= (sizeof(tlvbuf_header) + tlv_len);
        tlv =
            (tlvbuf_header *) ((t_u8 *) tlv + tlv_len + sizeof(tlvbuf_header));
    }
    return;
}

/**
 *  @brief Parse and print STA associate event data
 *
 *  @param buffer   Pointer to received buffer
 *  @param size     Length of the received event data
 *  @return         N/A
 */
void
print_event_sta_assoc(t_u8 * buffer, t_u16 size)
{
    int tlv_buf_left = size;
    t_u16 tlv_type, tlv_len;
    tlvbuf_header *tlv = NULL;
    MrvlIEtypes_WapiInfoSet_t *wapi_tlv = NULL;
    MrvlIETypes_MgmtFrameSet_t *mgmt_tlv = NULL;
    eventbuf_sta_assoc *event_body = NULL;
    if (size < sizeof(eventbuf_sta_assoc)) {
        printf("ERR:Event buffer too small!\n");
        return;
    }
    event_body = (eventbuf_sta_assoc *) buffer;
    printf("EVENT: STA_ASSOCIATE\n");
    printf("Associated STA MAC: ");
    print_mac(event_body->sta_mac_address);
    printf("\n");
    tlv_buf_left = size - sizeof(eventbuf_sta_assoc);
    if (tlv_buf_left < (int) sizeof(tlvbuf_header))
        return;
    tlv = (tlvbuf_header *) (buffer + sizeof(eventbuf_sta_assoc));

    while (tlv_buf_left >= (int) sizeof(tlvbuf_header)) {
        tlv_type = uap_le16_to_cpu(tlv->type);
        tlv_len = uap_le16_to_cpu(tlv->len);
        if ((sizeof(tlvbuf_header) + tlv_len) > (unsigned int) tlv_buf_left) {
            printf("wrong tlv: tlvLen=%d, tlvBufLeft=%d\n", tlv_len,
                   tlv_buf_left);
            break;
        }
        switch (tlv_type) {
        case MRVL_WAPI_INFO_TLV_ID:
            wapi_tlv = (MrvlIEtypes_WapiInfoSet_t *) tlv;
            printf("WAPI Multicast PN:\n");
            hexdump(wapi_tlv->multicast_PN, tlv_len, ' ');
            break;
        case MRVL_MGMT_FRAME_TLV_ID:
            mgmt_tlv = (MrvlIETypes_MgmtFrameSet_t *) tlv;
            print_mgmt_frame(mgmt_tlv, tlv_len);
            break;
        default:
            printf("unknown tlv: %d\n", tlv_type);
            break;
        }
        tlv_buf_left -= (sizeof(tlvbuf_header) + tlv_len);
        tlv =
            (tlvbuf_header *) ((t_u8 *) tlv + tlv_len + sizeof(tlvbuf_header));
    }
    return;
}

/**
 *  @brief Parse and print BSS start event data
 *
 *  @param buffer   Pointer to received buffer
 *  @param size     Length of the received event data
 *  @return         N/A
 */
void
print_event_bss_start(t_u8 * buffer, t_u16 size)
{
    eventbuf_bss_start *event_body = NULL;

    if (size < sizeof(eventbuf_bss_start)) {
        printf("ERR:Event buffer too small!\n");
        return;
    }
    event_body = (eventbuf_bss_start *) buffer;
    printf("EVENT: BSS_START ");
    printf("BSS MAC: ");
    print_mac(event_body->ap_mac_address);
    printf("\n");
    return;
}

/**
 *  @brief Prints station reject state
 *
 *  @param state    Fail state
 *  @return         N/A
 */
void
print_reject_state(t_u8 state)
{
    switch (state) {
    case REJECT_STATE_FAIL_EAPOL_2:
        printf("Reject state: FAIL_EAPOL_2\n");
        break;
    case REJECT_STATE_FAIL_EAPOL_4:
        printf("Reject state: FAIL_EAPOL_4:\n");
        break;
    case REJECT_STATE_FAIL_EAPOL_GROUP_2:
        printf("Reject state: FAIL_EAPOL_GROUP_2\n");
        break;
    default:
        printf("ERR: unknown reject state %d\n", state);
        break;
    }
    return;
}

/**
 *  @brief Prints station reject reason
 *
 *  @param reason   Reason code
 *  @return         N/A
 */
void
print_reject_reason(t_u16 reason)
{
    switch (reason) {
    case IEEEtypes_REASON_INVALID_IE:
        printf("Reject reason: Invalid IE\n");
        break;
    case IEEEtypes_REASON_MIC_FAILURE:
        printf("Reject reason: Mic Failure\n");
        break;
    default:
        printf("Reject reason: %d\n", reason);
        break;
    }
    return;
}

/**
 *  @brief Prints EAPOL state
 *
 *  @param state    Eapol state
 *  @return         N/A
 */
void
print_eapol_state(t_u8 state)
{
    switch (state) {
    case EAPOL_START:
        printf("Eapol state: EAPOL_START\n");
        break;
    case EAPOL_WAIT_PWK2:
        printf("Eapol state: EAPOL_WAIT_PWK2\n");
        break;
    case EAPOL_WAIT_PWK4:
        printf("Eapol state: EAPOL_WAIT_PWK4\n");
        break;
    case EAPOL_WAIT_GTK2:
        printf("Eapol state: EAPOL_WAIT_GTK2\n");
        break;
    case EAPOL_END:
        printf("Eapol state: EAPOL_END\n");
        break;
    default:
        printf("ERR: unknow eapol state%d\n", state);
        break;
    }
    return;
}

/**
 *  @brief Parse and print debug event data
 *
 *  @param buffer   Pointer to received buffer
 *  @param size     Length of the received event data
 *  @return         N/A
 */
void
print_event_debug(t_u8 * buffer, t_u16 size)
{
    eventbuf_debug *event_body = NULL;
    if (size < sizeof(eventbuf_debug)) {
        printf("ERR:Event buffer too small!\n");
        return;
    }
    event_body = (eventbuf_debug *) buffer;
    printf("Debug Event Type: %s\n",
           (event_body->debug_type == 0) ? "EVENT" : "INFO");
    printf("%s log:\n",
           (uap_le32_to_cpu(event_body->debug_id_major) ==
            DEBUG_ID_MAJ_AUTHENTICATOR) ? "Authenticator" : "Assoc_agent");
    if (uap_le32_to_cpu(event_body->debug_id_major) ==
        DEBUG_ID_MAJ_AUTHENTICATOR) {
        switch (uap_le32_to_cpu(event_body->debug_id_minor)) {
        case DEBUG_MAJ_AUTH_MIN_PWK1:
            printf("EAPOL Key message 1 (PWK):\n");
            hexdump((t_u8 *) & event_body->info.eapol_pwkmsg,
                    sizeof(eapol_keymsg_debug_t), ' ');
            break;
        case DEBUG_MAJ_AUTH_MIN_PWK2:
            printf("EAPOL Key message 2 (PWK):\n");
            hexdump((t_u8 *) & event_body->info.eapol_pwkmsg,
                    sizeof(eapol_keymsg_debug_t), ' ');
            break;
        case DEBUG_MAJ_AUTH_MIN_PWK3:
            printf("EAPOL Key message 3 (PWK):\n");
            hexdump((t_u8 *) & event_body->info.eapol_pwkmsg,
                    sizeof(eapol_keymsg_debug_t), ' ');
            break;
        case DEBUG_MAJ_AUTH_MIN_PWK4:
            printf("EAPOL Key message 4: (PWK)\n");
            hexdump((t_u8 *) & event_body->info.eapol_pwkmsg,
                    sizeof(eapol_keymsg_debug_t), ' ');
            break;
        case DEBUG_MAJ_AUTH_MIN_GWK1:
            printf("EAPOL Key message 1: (GWK)\n");
            hexdump((t_u8 *) & event_body->info.eapol_pwkmsg,
                    sizeof(eapol_keymsg_debug_t), ' ');
            break;
        case DEBUG_MAJ_AUTH_MIN_GWK2:
            printf("EAPOL Key message 2: (GWK)\n");
            hexdump((t_u8 *) & event_body->info.eapol_pwkmsg,
                    sizeof(eapol_keymsg_debug_t), ' ');
            break;
        case DEBUG_MAJ_AUTH_MIN_STA_REJ:
            printf("Reject STA MAC: ");
            print_mac(event_body->info.sta_reject.sta_mac_addr);
            printf("\n");
            print_reject_state(event_body->info.sta_reject.reject_state);
            print_reject_reason(uap_le16_to_cpu
                                (event_body->info.sta_reject.reject_reason));
            break;
        case DEBUG_MAJ_AUTH_MIN_EAPOL_TR:
            printf("STA MAC: ");
            print_mac(event_body->info.eapol_state.sta_mac_addr);
            printf("\n");
            print_eapol_state(event_body->info.eapol_state.eapol_state);
            break;
        default:
            printf("ERR: unknow debug_id_minor: %d\n",
                   (int) uap_le32_to_cpu(event_body->debug_id_minor));
            hexdump(buffer, size, ' ');
            return;
        }
    } else if (uap_le32_to_cpu(event_body->debug_id_major) ==
               DEBUG_ID_MAJ_ASSOC_AGENT) {
        switch (uap_le32_to_cpu(event_body->debug_id_minor)) {
        case DEBUG_ID_MAJ_ASSOC_MIN_WPA_IE:
            printf("STA MAC: ");
            print_mac(event_body->info.wpaie.sta_mac_addr);
            printf("\n");
            printf("wpa ie:\n");
            hexdump(event_body->info.wpaie.wpa_ie, MAX_WPA_IE_LEN, ' ');
            break;
        case DEBUG_ID_MAJ_ASSOC_MIN_STA_REJ:
            printf("Reject STA MAC: ");
            print_mac(event_body->info.sta_reject.sta_mac_addr);
            printf("\n");
            print_reject_state(event_body->info.sta_reject.reject_state);
            print_reject_reason(uap_le16_to_cpu
                                (event_body->info.sta_reject.reject_reason));
            break;
        default:
            printf("ERR: unknow debug_id_minor: %d\n",
                   (int) uap_le32_to_cpu(event_body->debug_id_minor));
            hexdump(buffer, size, ' ');
            return;
        }
    }
    return;
}

/**
 *  @brief Parse and print received event information
 *
 *  @param event    Pointer to received event
 *  @param size     Length of the received event
 *  @return         N/A
 */
void
print_event(event_header * event, t_u16 size)
{
    t_u32 event_id = event->event_id;
    switch (event_id) {
    case MICRO_AP_EV_ID_STA_DEAUTH:
        print_event_sta_deauth(event->event_data, size - EVENT_ID_LEN);
        break;
    case MICRO_AP_EV_ID_STA_ASSOC:
        print_event_sta_assoc(event->event_data, size - EVENT_ID_LEN);
        break;
    case MICRO_AP_EV_ID_BSS_START:
        print_event_bss_start(event->event_data, size - EVENT_ID_LEN);
        break;
    case MICRO_AP_EV_ID_DEBUG:
        print_event_debug(event->event_data, size - EVENT_ID_LEN);
        break;
    case MICRO_AP_EV_BSS_IDLE:
        printf("EVENT: BSS_IDLE\n");
        break;
    case MICRO_AP_EV_BSS_ACTIVE:
        printf("EVENT: BSS_ACTIVE\n");
        break;
    case MICRO_AP_EV_RSN_CONNECT:
        print_event_rsn_connect(event->event_data, size - EVENT_ID_LEN);
        break;
    case UAP_EVENT_ID_DRV_HS_ACTIVATED:
        printf("EVENT: HS_ACTIVATED\n");
        break;
    case UAP_EVENT_ID_DRV_HS_DEACTIVATED:
        printf("EVENT: HS_DEACTIVATED\n");
        break;
    case UAP_EVENT_ID_HS_WAKEUP:
        printf("EVENT: HS_WAKEUP\n");
        break;
    case UAP_EVENT_HOST_SLEEP_AWAKE:
        break;
    case MICRO_AP_EV_WMM_STATUS_CHANGE:
        printf("EVENT: WMM_STATUS_CHANGE\n");
        break;
    default:
        printf("ERR:Undefined event type (%X). Dumping event buffer:\n",
               (unsigned int) event_id);
        hexdump((void *) event, size, ' ');
        break;
    }
    return;
}

/**
 *  @brief Read event data from netlink socket
 *
 *  @param sk_fd    Netlink socket handler
 *  @param buffer   Pointer to the data buffer
 *  @param nlh      Pointer to netlink message header
 *  @param msg      Pointer to message header
 *  @return         Number of bytes read or MLAN_EVENT_FAILURE
 */
int
read_event_netlink_socket(int sk_fd, unsigned char *buffer,
                          struct nlmsghdr *nlh, struct msghdr *msg)
{
    int count = -1;
    count = recvmsg(sk_fd, msg, 0);
#if DEBUG
    printf("DBG:Waiting for message from NETLINK.\n");
#endif
    if (count < 0) {
        printf("ERR:NETLINK read failed!\n");
        terminate_flag++;
        return MLAN_EVENT_FAILURE;
    }
#if DEBUG
    printf("DBG:Received message payload (%d)\n", count);
#endif
    if (count > NLMSG_SPACE(NL_MAX_PAYLOAD)) {
        printf("ERR:Buffer overflow!\n");
        return MLAN_EVENT_FAILURE;
    }
    memset(buffer, 0, NL_MAX_PAYLOAD);
    memcpy(buffer, NLMSG_DATA(nlh), count - NLMSG_HDRLEN);
#if DEBUG
    hexdump(buffer, count - NLMSG_HDRLEN, ' ');
#endif
    return count - NLMSG_HDRLEN;
}

/**
 *  @brief Configure and read event data from netlink socket
 *
 *  @param sk_fd    Netlink socket handler
 *  @param buffer   Pointer to the data buffer
 *  @param timeout  Socket listen timeout value
 *  @param nlh      Pointer to netlink message header
 *  @param msg      Pointer to message header
 *  @return         Number of bytes read or MLAN_EVENT_FAILURE
 */
int
read_event(int sk_fd, unsigned char *buffer, int timeout, struct nlmsghdr *nlh,
           struct msghdr *msg)
{
    struct timeval tv;
    fd_set rfds;
    int ret = MLAN_EVENT_FAILURE;

    /* Setup read fds */
    FD_ZERO(&rfds);
    FD_SET(sk_fd, &rfds);

    /* Initialize timeout value */
    if (timeout != 0)
        tv.tv_sec = timeout;
    else
        tv.tv_sec = UAP_RECV_WAIT_DEFAULT;
    tv.tv_usec = 0;

    /* Wait for reply */
    ret = select(sk_fd + 1, &rfds, NULL, NULL, &tv);
    if (ret == -1) {
        /* Error */
        terminate_flag++;
        return MLAN_EVENT_FAILURE;
    } else if (!ret) {
        /* Timeout. Try again */
        return MLAN_EVENT_FAILURE;
    }
    if (!FD_ISSET(sk_fd, &rfds)) {
        /* Unexpected error. Try again */
        return MLAN_EVENT_FAILURE;
    }

    /* Success */
    ret = read_event_netlink_socket(sk_fd, buffer, nlh, msg);
    return ret;
}

/* Command line options */
static const struct option long_opts[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {NULL, 0, NULL, 0}
};

/**
 *  @brief Determine the netlink number
 *
 *  @return         Netlink number to use
 */
static int
get_netlink_num(void)
{
    FILE *fp;
    int netlink_num = NETLINK_MARVELL;
    char str[64];
    char *srch = "netlink_num";

    /* Try to open /proc/mwlan/config */
    fp = fopen("/proc/mwlan/config", "r");
    if (fp) {
        while (!feof(fp)) {
            fgets(str, sizeof(str), fp);
            if (strncmp(str, srch, strlen(srch)) == 0) {
                netlink_num = atoi(str + strlen(srch) + 1);
                break;
            }
        }
        fclose(fp);
    }

    printf("Netlink number = %d\n", netlink_num);
    return netlink_num;
}

/****************************************************************************
        Global functions
****************************************************************************/
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
    int opt;
    int nl_sk = 0;
    struct nlmsghdr *nlh = NULL;
    struct sockaddr_nl src_addr, dest_addr;
    struct msghdr msg;
    struct iovec iov;
    unsigned char *buffer = NULL;
    struct timeval current_time;
    struct tm *timeinfo;
    int num_events = 0;
    event_header *event = NULL;
    int ret = MLAN_EVENT_FAILURE;
    int netlink_num = 0;

    /* Check command line options */
    while ((opt = getopt_long(argc, argv, "hvt", long_opts, NULL)) > 0) {
        switch (opt) {
        case 'h':
            print_usage();
            return 0;
        case 'v':
            printf("mlanevent version : %s\n", MLAN_EVENT_VERSION);
            return 0;
            break;
        default:
            print_usage();
            return 1;
        }
    }
    if (optind < argc) {
        fputs("Too many arguments.\n", stderr);
        print_usage();
        return 1;
    }

    netlink_num = get_netlink_num();

    /* Open netlink socket */
    nl_sk = socket(PF_NETLINK, SOCK_RAW, netlink_num);
    if (nl_sk < 0) {
        printf("ERR:Could not open netlink socket.\n");
        ret = MLAN_EVENT_FAILURE;
        goto done;
    }

    /* Set source address */
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid(); /* Our PID */
    src_addr.nl_groups = NL_MULTICAST_GROUP;

    /* Bind socket with source address */
    if (bind(nl_sk, (struct sockaddr *) &src_addr, sizeof(src_addr)) < 0) {
        printf("ERR:Could not bind socket!\n");
        ret = MLAN_EVENT_FAILURE;
        goto done;
    }

    /* Set destination address */
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;       /* Kernel */
    dest_addr.nl_groups = NL_MULTICAST_GROUP;

    /* Initialize netlink header */
    nlh = (struct nlmsghdr *) malloc(NLMSG_SPACE(NL_MAX_PAYLOAD));
    if (!nlh) {
        printf("ERR: Could not alloc buffer\n");
        ret = MLAN_EVENT_FAILURE;
        goto done;
    }
    memset(nlh, 0, NLMSG_SPACE(NL_MAX_PAYLOAD));

    /* Initialize I/O vector */
    iov.iov_base = (void *) nlh;
    iov.iov_len = NLMSG_SPACE(NL_MAX_PAYLOAD);

    /* Initialize message header */
    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_name = (void *) &dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    /* Initialize receive buffer */
    buffer = malloc(NL_MAX_PAYLOAD);
    if (!buffer) {
        printf("ERR: Could not alloc buffer\n");
        ret = MLAN_EVENT_FAILURE;
        goto done;
    }
    memset(buffer, 0, sizeof(buffer));

    gettimeofday(&current_time, NULL);

    printf("\n");
    printf("**********************************************\n");
    if ((timeinfo = localtime(&(current_time.tv_sec))))
        printf("mlanevent start time : %s", asctime(timeinfo));
    printf("                      %u usecs\n",
           (unsigned int) current_time.tv_usec);
    printf("**********************************************\n");

    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGALRM, sig_handler);
    while (1) {
        if (terminate_flag) {
            printf("Stopping!\n");
            break;
        }
        ret = read_event(nl_sk, buffer, 0, nlh, &msg);

        /* No result. Loop again */
        if (ret == MLAN_EVENT_FAILURE) {
            continue;
        }
        if (ret == 0) {
            /* Zero bytes received */
            printf("ERR:Received zero bytes!\n");
            continue;
        }
        num_events++;
        gettimeofday(&current_time, NULL);
        printf("\n");
        printf("============================================\n");
        printf("Received event");
        if ((timeinfo = localtime(&(current_time.tv_sec))))
            printf(": %s", asctime(timeinfo));
        printf("                     %u usecs\n",
               (unsigned int) current_time.tv_usec);
        printf("============================================\n");
        event = (event_header *) buffer;
#if DEBUG
        printf("DBG:Received buffer =\n");
        hexdump(buffer, ret, ' ');
#endif
        print_event(event, ret);
        fflush(stdout);
    }
    gettimeofday(&current_time, NULL);
    printf("\n");
    printf("*********************************************\n");
    if ((timeinfo = localtime(&(current_time.tv_sec))))
        printf("mlanevent end time  : %s", asctime(timeinfo));
    printf("                     %u usecs\n",
           (unsigned int) current_time.tv_usec);
    printf("Total events       : %u\n", num_events);
    printf("*********************************************\n");
  done:
    if (buffer)
        free(buffer);
    if (nl_sk)
        close(nl_sk);
    if (nlh)
        free(nlh);
    return 0;
}
