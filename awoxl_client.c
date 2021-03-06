#include "awoxl_client.h"
#include "awoxl_protocol.h"

#include <bluetooth/l2cap.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h> /* close */

// from bluez
static int l2cap_bind(int sock, const bdaddr_t *src, uint8_t src_type,
                uint16_t psm, uint16_t cid/*, GError **err*/)
{
    struct sockaddr_l2 addr;

    memset(&addr, 0, sizeof(addr));
    addr.l2_family = AF_BLUETOOTH;
    bacpy(&addr.l2_bdaddr, src);

    if (cid)
        addr.l2_cid = htobs(cid);
    else
        addr.l2_psm = htobs(psm);

    addr.l2_bdaddr_type = src_type;

    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        int error = -errno;
        // ERROR_FAILED(err, "l2cap_bind", errno);
        return error;
    }

    return 0;
}

// from bluez
static int l2cap_connect(int sock, const bdaddr_t *dst, uint8_t dst_type,
                        uint16_t psm, uint16_t cid)
{
    int err;
    struct sockaddr_l2 addr;

    memset(&addr, 0, sizeof(addr));
    addr.l2_family = AF_BLUETOOTH;
    bacpy(&addr.l2_bdaddr, dst);
    if (cid)
        addr.l2_cid = htobs(cid);
    else
        addr.l2_psm = htobs(psm);

    addr.l2_bdaddr_type = dst_type;

    err = connect(sock, (struct sockaddr *) &addr, sizeof(addr));
    if (err < 0 && !(errno == EAGAIN || errno == EINPROGRESS))
        return -errno;

    return 0;
}

int awoxl_connect(bdaddr_t dst) {
    int cid = 4, psm = 0;
    bdaddr_t src;
    for (int i = 0; i < 6; i++)
        src.b[i] = 0;

    int sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    int err = l2cap_bind(sock, &src, BDADDR_LE_PUBLIC, psm, cid);
    if (err)
        return -1;
    err = l2cap_connect(sock, &dst, BDADDR_LE_PUBLIC, psm, cid);
    if (err)
        return -1;
    return sock;
}

int awoxl_disconnect(int sock) {
    return close(sock);
}

int awoxl_send_command(int sock, unsigned char* command, int len) {
    unsigned char* buffer = (unsigned char*) malloc(len + 3);
    buffer[0] = 0x12;
    buffer[1] = 0x1D;
    buffer[2] = 0x00;
    memcpy(buffer + 3, command, len);
    int sent = send(sock, buffer, len + 3, 0);
    free(buffer);
    return sent == len + 3 ? 0 : -1;
}

int awoxl_on(int sock) {
    int len;
    unsigned char* buffer;
    
    len = awoxl_protocol_on(&buffer);
    int err = awoxl_send_command(sock, buffer, len);
    free(buffer);
    return err;
}

int awoxl_off(int sock) {
    int len;
    unsigned char* buffer;
    
    len = awoxl_protocol_off(&buffer);
    int err = awoxl_send_command(sock, buffer, len);
    free(buffer);
    return err;
}

int awoxl_onoff(int sock, int on) {
    return on ? awoxl_on(sock) : awoxl_off(sock);
}

int awoxl_brightness(int sock, unsigned char brightness) {
    int len;
    unsigned char* buffer;
    
    len = awoxl_protocol_brightness(&buffer, brightness);
    int err = awoxl_send_command(sock, buffer, len);
    free(buffer);
    return err;
}

int awoxl_white(int sock, unsigned char temperature) {
    int err;
    int len;
    unsigned char* buffer;

    // TODO maybe we can even customize the white light a bit more?    
    len = awoxl_protocol_rgb(&buffer, 0xFF, 0xDE, 0x00, 1);
    err = awoxl_send_command(sock, buffer, len);
    free(buffer);
    if (err != 0)
        return err;
    // TODO have to wait a little bit, maybe even less delay is possible?
    usleep(50*1000);

    len = awoxl_protocol_white(&buffer, temperature);
    err = awoxl_send_command(sock, buffer, len);
    free(buffer);
    return err;
}

int awoxl_rgb(int sock, unsigned char r, unsigned char g, unsigned char b) {
    int len;
    unsigned char* buffer;
    
    len = awoxl_protocol_rgb(&buffer, r, g, b, 0);
    int err = awoxl_send_command(sock, buffer, len);
    free(buffer);
    return err;
}

