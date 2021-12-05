#include "websocket.h"

enum WSParserState
{
    WS_PARSE_START = 0,
    WS_PARSE_BYTE2,
    WS_PARSE_LEN,
    WS_PARSE_MASK,
    WS_PARSE_PAYLOAD,
    WS_PARSE_ERROR,
};

enum WSResult WS_stream_init(struct WSStream* self, WS_SIZE_T sz, WSStreamCallback cb)
{
    if (sz != sizeof(struct WSStream))
    {
        return WS_SIZE;
    }

    self->struct_size = sz;
    self->cb = cb;
    return WS_stream_reset(self);
}

enum WSResult WS_stream_reset(struct WSStream* self)
{
    self->expected_len = 0;
    self->cur_offset = 0;
    self->mask = 0;
    self->m_parser_state = WS_PARSE_START;
    return WS_OK;
}

enum WSResult WS_stream_data(struct WSStream* const self, const void* const data, WS_SIZE_T const data_sz)
{
    const unsigned char* const chdata = data;
    WS_SIZE_T cur = 0;

#define LABEL_CASE(STATE)                                                                                              \
    STATE:                                                                                                             \
    if (cur == data_sz)                                                                                                \
    {                                                                                                                  \
        self->m_parser_state = STATE;                                                                                  \
        return WS_OK;                                                                                                  \
    }                                                                                                                  \
    case STATE:

    while (cur < data_sz)
    {
        switch (self->m_parser_state)
        {
            LABEL_CASE(WS_PARSE_START);
            *(unsigned char*)(&self->byte1) = chdata[cur++];
            if (self->byte1.rsv1 | self->byte1.rsv2 | self->byte1.rsv3)
            {
                // nonzero reserved bits
                goto WS_PARSE_ERROR;
            }
            if (self->byte1.opcode & 0x4 || (self->byte1.opcode & 0x3) == 0x3)
            {
                // unknown opcode
                goto WS_PARSE_ERROR;
            }

            LABEL_CASE(WS_PARSE_BYTE2);
            {
                *(unsigned char*)(&self->byte2) = chdata[cur++];
                if (self->byte2.len == 126)
                {
                    self->m_intsize = 2;
                    goto WS_PARSE_LEN;
                }
                else if (self->byte2.len == 127)
                {
                    self->m_intsize = 8;
                    goto WS_PARSE_LEN;
                }
                else
                {
                    self->m_intsize = 0;
                    self->expected_len = self->byte2.len;
                    goto WS_PARSE_HAVE_LEN;
                }
            }

            LABEL_CASE(WS_PARSE_LEN);
            self->expected_len <<= 8;
            self->expected_len += chdata[cur++];
            if (--self->m_intsize > 0) goto WS_PARSE_LEN;

        WS_PARSE_HAVE_LEN:
            if (!self->byte2.mask) goto WS_PARSE_PAYLOAD;
            self->m_intsize = 4;

            LABEL_CASE(WS_PARSE_MASK);
            self->mask <<= 8;
            self->mask += chdata[cur++];
            if (--self->m_intsize > 0) goto WS_PARSE_MASK;

            LABEL_CASE(WS_PARSE_PAYLOAD);
            {
                const WS_SIZE_T data_rem = data_sz - cur;
                const WS_SIZE_T exp_rem = self->expected_len - self->cur_offset;
                if (exp_rem > data_rem)
                {
                    // incomplete frame
                    const enum WSResult res = self->cb(self, chdata + cur, data_rem);
                    self->cur_offset += data_rem;
                    if (res != WS_OK) self->m_parser_state = WS_PARSE_ERROR;
                    return res;
                }
                else
                {
                    // complete frame
                    const enum WSResult res = self->cb(self, chdata + cur, exp_rem);
                    if (res != WS_OK)
                    {
                        self->m_parser_state = WS_PARSE_ERROR;
                        return res;
                    }
                    cur += exp_rem;
                    WS_stream_reset(self);
                    goto WS_PARSE_START;
                }
            }

        WS_PARSE_ERROR:
            self->m_parser_state = WS_PARSE_ERROR;
            case WS_PARSE_ERROR: return WS_INVALID;
        }
    }
#undef LABEL_AND_RET
    return WS_OK;
}

static void WS_htn16(char* const dst, const unsigned short v)
{
    dst[0] = (v & 0xFF00) >> 8;
    dst[1] = (v & 0x00FF);
}

static void WS_htn32(char* const dst, const unsigned int v)
{
    WS_htn16(dst, (v & 0xFFFF0000U) >> 16);
    WS_htn16(dst + 2, v & 0x0000FFFFU);
}

static void WS_htn64(char* const dst, const unsigned long long v)
{
    WS_htn32(dst, (v & 0xFFFFFFFF00000000ULL) >> 32);
    WS_htn32(dst + 4, v & 0x00000000FFFFFFFF);
}

static int WS_write_header_byte2_impl(char* const chbuf, WS_SIZE_T buf_sz, unsigned long long length, int mask)
{
    if (length < 126)
    {
        if (buf_sz < 1) return 0;
        const struct WSHeaderByte2 byte2 = {
            .mask = mask,
            .len = length,
        };
        chbuf[0] = *(char*)&byte2;
        return 1;
    }
    else if (length <= 0xFFFF)
    {
        // 16-bit length
        if (buf_sz < 3) return 0;
        const struct WSHeaderByte2 byte2 = {
            .mask = mask,
            .len = 126,
        };
        chbuf[0] = *(char*)&byte2;
        WS_htn16(chbuf + 1, length);
        return 3;
    }
    else
    {
        // 64-bit length
        if (buf_sz < 9) return 0;
        const struct WSHeaderByte2 byte2 = {
            .mask = mask,
            .len = 127,
        };
        chbuf[0] = *(char*)&byte2;
        WS_htn64(chbuf + 1, length);
        return 9;
    }
}

int WS_write_header_byte2_masked(void* buf, WS_SIZE_T buf_sz, unsigned long long length, unsigned int mask)
{
    if (buf_sz < 5) return 0;
    const int at = WS_write_header_byte2_impl(buf, buf_sz - 4, length, 1);
    if (!at) return 0;
    WS_htn32((char*)buf + at, mask);
    return at + 4;
}

int WS_write_header_byte2_unmasked(void* buf, WS_SIZE_T buf_sz, unsigned long long length)
{
    return WS_write_header_byte2_impl(buf, buf_sz, length, 0);
}
