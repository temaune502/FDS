#define FDS_IMPL
#include "fds.h"
#define FDS_EXT_SOCKET_IMPL
#include "fds_ext_socket.h"

void lamp_on(FdsSocket sock)
{
    uint8_t cmd[] = { 0x71, 0x23, 0x0F, 0xA3 };
    fds_socket_send_raw(sock, cmd, sizeof(cmd));
}

void lamp_off(FdsSocket sock)
{
    FdsBytesBuilder bytes = fds_bb_create(4);
    fds_bb_append_byte(&bytes, 0x71);
    fds_bb_append_byte(&bytes, 0x24);
    fds_bb_append_byte(&bytes, 0x0F);
    fds_bb_append_byte(&bytes, 0xA4);
    fds_socket_send_raw(sock, bytes.data, bytes.size);
    fds_bb_destroy(&bytes);
}

void set_rgb(FdsSocket sock, uint8_t r, uint8_t g, uint8_t b)
{
    FdsBytesBuilder bytes = fds_bb_create(4);
    fds_bb_append_byte(&bytes, 0x31);
    // fds_bb_append_byte(&bytes, 0xFF);
    // fds_bb_append_byte(&bytes, 0x00);
    // fds_bb_append_byte(&bytes, 0x00);
    fds_bb_append_byte(&bytes, r);
    fds_bb_append_byte(&bytes, g);
    fds_bb_append_byte(&bytes, b);
    fds_bb_append_byte(&bytes, 0x00);
    fds_bb_append_byte(&bytes, 0x0F);
    fds_bb_append_byte(&bytes, (0x31 + r + g + b + 0x00 + 0x0F) & 0xFF);
    fds_socket_send_raw(sock, bytes.data, bytes.size);
    fds_bb_destroy(&bytes);
}


int main()
{
    fds_net_init();

    
    // fds_bb_write_u32(&bytes, 0x71 0x23 0x0F 0xA3);

    FdsSocket sock = fds_socket_connect("192.168.0.100", 5577);
    
    // lamp_on(sock);
    // // set_rgb(sock, 255,255,200);
    // FdsBytesBuilder bytes = fds_bb_create(8);
    // fds_bb_append_byte(&bytes, 0x31); // Command

    // fds_bb_append_byte(&bytes, 0x00); // Red
    // fds_bb_append_byte(&bytes, 0x00); // Green
    // fds_bb_append_byte(&bytes, 0x00); // Blue

    // fds_bb_append_byte(&bytes, 0x80); // Warm White
    // fds_bb_append_byte(&bytes, 0x00); // Cool White
    // fds_bb_append_byte(&bytes, 0x0F); // Mode

    // fds_bb_append_byte(&bytes, (0x31+0x23+0x23+0x23+0x0F) & 0xFF);

    // fds_socket_send_raw(sock, bytes.data, bytes.size);
    
    // fds_sleep_ms(2000);

    lamp_off(sock);

    fds_socket_close(&sock);
    return 0;
}