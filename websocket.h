#pragma once

#ifdef _WIN64
#define WS_SIZE_T unsigned __int64
#elif defined(_WIN32)
#define WS_SIZE_T unsigned int
#else
#include <stdint.h>
#define WS_SIZE_T size_t
#endif

enum WSOpcode
{
    WS_OPCODE_CONTINUE = 0x0,
    WS_OPCODE_TEXT = 0x1,
    WS_OPCODE_BINARY = 0x2,

    WS_OPCODE_CLOSE = 0x8,
    WS_OPCODE_PING = 0x9,
    WS_OPCODE_PONG = 0xA,
};

enum WSResult
{
    WS_OK,
    WS_SIZE,
    WS_INVALID,
    WS_USER = 16,
};

struct WSHeaderByte1
{
    unsigned char fin : 1;
    unsigned char rsv1 : 1;
    unsigned char rsv2 : 1;
    unsigned char rsv3 : 1;
    unsigned char opcode : 4;
};
struct WSHeaderByte2
{
    unsigned char mask : 1;
    unsigned char len : 7;
};

typedef enum WSResult (*WSStreamCallback)(struct WSStream* stream, const void* payload, WS_SIZE_T sz);

struct WSStream
{
    WS_SIZE_T struct_size;
    WSStreamCallback cb;

    unsigned long long cur_offset;
    unsigned long long expected_len;
    unsigned int mask;
    struct WSHeaderByte1 byte1;
    struct WSHeaderByte2 byte2;

    char m_parser_state;
    char m_intsize;
};

enum WSResult WS_stream_init(struct WSStream* stream, WS_SIZE_T sz, WSStreamCallback cb);
enum WSResult WS_stream_data(struct WSStream* stream, const void* data, WS_SIZE_T data_sz);
enum WSResult WS_stream_reset(struct WSStream* stream);

// @returns 0 on failure, bytes written on success
int WS_write_header_byte2_masked(void* buf, WS_SIZE_T buf_sz, unsigned long long length, unsigned int mask);

// @returns 0 on failure, bytes written on success
int WS_write_header_byte2_unmasked(void* buf, WS_SIZE_T buf_sz, unsigned long long length);
