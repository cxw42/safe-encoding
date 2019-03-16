#include <gtest/gtest.h>
#include <safe85/safe85.h>

// #define KSLogger_LocalLevel TRACE
#include "kslogger.h"

static const int g_bytes_per_group  = 4;
static const int g_chunks_per_group = 5;


// ----------
// Assertions
// ----------

void assert_encode_decode(std::string expected_encoded, std::vector<uint8_t> expected_decoded)
{
    int64_t expected_encoded_length = expected_encoded.size();
    int64_t actual_encoded_length = safe85_get_encoded_length(expected_decoded.size(), false);
    ASSERT_EQ(expected_encoded_length, actual_encoded_length);

    std::vector<uint8_t> encode_buffer(1000);
    int64_t actual_encode_used_bytes = safe85_encode(expected_decoded.data(),
                                                     expected_decoded.size(),
                                                     encode_buffer.data(),
                                                     encode_buffer.size());
    ASSERT_EQ(expected_encoded_length, actual_encode_used_bytes);
    std::string actual_encoded(encode_buffer.begin(), encode_buffer.begin() + actual_encode_used_bytes);
    ASSERT_EQ(actual_encoded_length, actual_encode_used_bytes);
    ASSERT_EQ(expected_encoded, actual_encoded);

    int64_t expected_decoded_length = expected_decoded.size();
    int64_t actual_decoded_length = safe85_get_decoded_length(expected_encoded.size());
    ASSERT_EQ(expected_decoded_length, actual_decoded_length);

    std::vector<uint8_t> decode_buffer(1000);
    int64_t expected_decode_used_bytes = expected_decoded.size();
    int64_t actual_decode_used_bytes = safe85_decode((uint8_t*)expected_encoded.data(),
                                                     expected_encoded.size(),
                                                     decode_buffer.data(),
                                                     decode_buffer.size());
    ASSERT_EQ(expected_decode_used_bytes, actual_decode_used_bytes);
    std::vector<uint8_t> actual_decoded(decode_buffer.begin(), decode_buffer.begin() + actual_decode_used_bytes);
    ASSERT_EQ(expected_decoded, actual_decoded);
}

void assert_encode_decode_with_length(std::string expected_encoded, std::vector<uint8_t> expected_decoded)
{
    std::vector<uint8_t> encode_buffer(1000);
    int64_t actual_encode_used_bytes = safe85l_encode(expected_decoded.data(),
                                                      expected_decoded.size(),
                                                      encode_buffer.data(),
                                                      encode_buffer.size());
    ASSERT_GT(actual_encode_used_bytes, 0);
    std::string actual_encoded(encode_buffer.begin(), encode_buffer.begin() + actual_encode_used_bytes);
    ASSERT_EQ(expected_encoded, actual_encoded);

    std::vector<uint8_t> decode_buffer(1000);
    int64_t actual_decode_used_bytes = safe85l_decode((uint8_t*)expected_encoded.data(),
                                                      expected_encoded.size(),
                                                      decode_buffer.data(),
                                                      decode_buffer.size());
    ASSERT_GT(actual_decode_used_bytes, 0);
    std::vector<uint8_t> actual_decoded(decode_buffer.begin(), decode_buffer.begin() + actual_decode_used_bytes);
    ASSERT_EQ(expected_decoded, actual_decoded);
}

void assert_decode_with_length_status(std::string expected_encoded, int64_t force_length, int64_t expected_status)
{
    if(force_length < 0)
    {
        force_length = expected_encoded.size();
    }
    std::vector<uint8_t> decode_buffer(1000);
    int64_t actual_decode_used_bytes = safe85l_decode((uint8_t*)expected_encoded.data(),
                                                      force_length,
                                                      decode_buffer.data(),
                                                      decode_buffer.size());
    ASSERT_EQ(expected_status, actual_decode_used_bytes);
}

std::vector<uint8_t> make_bytes(int length, int start_value)
{
    std::vector<uint8_t> vec;
    for(int i = 0; i < length; i++)
    {
        vec.push_back((uint8_t)((start_value+i) & 0xff));
    }
    return vec;
}

void assert_chunked_encode_src_packeted(int length)
{
    std::vector<uint8_t> data = make_bytes(length, length);
    std::vector<uint8_t> encode_buffer(length * 2);
    std::vector<uint8_t> decode_buffer(length);

    for(int packet_size=length-1; packet_size >= g_bytes_per_group; packet_size--)
    {
        KSLOG_DEBUG("packet size %d", packet_size);
        safe85_status status = SAFE85_STATUS_OK;
        const uint8_t* e_src = data.data();
        const uint8_t* e_src_end = data.data() + data.size();
        uint8_t* e_dst = (uint8_t*)encode_buffer.data();
        uint8_t* e_dst_end = (uint8_t*)encode_buffer.data() + encode_buffer.size();
        const uint8_t* d_src = (uint8_t*)encode_buffer.data();
        uint8_t* d_dst = decode_buffer.data();
        uint8_t* d_dst_end = decode_buffer.data() + decode_buffer.size();

        while(e_src < e_src_end)
        {
            bool is_end = false;
            safe85_stream_state stream_state = SAFE85_STREAM_STATE_NONE;
            int encode_byte_count = packet_size;
            if(encode_byte_count >= e_src_end - e_src)
            {
                encode_byte_count = e_src_end - e_src;
                stream_state = SAFE85_SRC_IS_AT_END_OF_STREAM;
                is_end = true;
            }
            status = safe85_encode_feed(&e_src,
                                        encode_byte_count,
                                        &e_dst,
                                        e_dst_end - e_dst,
                                        is_end);
            ASSERT_EQ(SAFE85_STATUS_OK, status);
            status = safe85_decode_feed(&d_src,
                                        e_dst - d_src,
                                        &d_dst,
                                        d_dst_end - d_dst,
                                        stream_state);
            ASSERT_TRUE(status == SAFE85_STATUS_OK || status == SAFE85_STATUS_PARTIALLY_COMPLETE);
        }
        ASSERT_EQ(SAFE85_STATUS_OK, status);
        ASSERT_EQ(data, decode_buffer);
    }
}

void assert_chunked_encode_dst_packeted(int length)
{
    std::vector<uint8_t> data = make_bytes(length, length);
    std::vector<uint8_t> encode_buffer(length * 2);
    std::vector<uint8_t> decode_buffer(length);

    for(int packet_size=length-1; packet_size >= g_chunks_per_group; packet_size--)
    {
        KSLOG_DEBUG("packet size %d", packet_size);
        safe85_status status = SAFE85_STATUS_OK;
        const uint8_t* e_src = data.data();
        const uint8_t* e_src_end = data.data() + data.size();
        uint8_t* e_dst = (uint8_t*)encode_buffer.data();
        const uint8_t* d_src = (uint8_t*)encode_buffer.data();
        uint8_t* d_dst = decode_buffer.data();
        uint8_t* d_dst_end = decode_buffer.data() + decode_buffer.size();

        while(e_src < e_src_end)
        {
            bool is_end = e_src + packet_size >= e_src_end;
            safe85_stream_state stream_state = (safe85_stream_state)(is_end ? SAFE85_SRC_IS_AT_END_OF_STREAM : SAFE85_STREAM_STATE_NONE);
            status = safe85_encode_feed(&e_src,
                                        e_src_end - e_src,
                                        &e_dst,
                                        packet_size,
                                        is_end);
            ASSERT_TRUE(status == SAFE85_STATUS_OK || status == SAFE85_STATUS_PARTIALLY_COMPLETE);
            status = safe85_decode_feed(&d_src,
                                        e_dst - d_src,
                                        &d_dst,
                                        d_dst_end - d_dst,
                                        stream_state);
            ASSERT_TRUE(status == SAFE85_STATUS_OK || status == SAFE85_STATUS_PARTIALLY_COMPLETE);
        }
        ASSERT_EQ(SAFE85_STATUS_OK, status);
        ASSERT_EQ(data, decode_buffer);
    }
}

void assert_chunked_decode_dst_packeted(int length)
{
    KSLOG_DEBUG("Length %d", length);
    std::vector<uint8_t> data = make_bytes(length, length);
    std::vector<uint8_t> encode_buffer(length * 2);
    std::vector<uint8_t> decode_buffer(length);
    int64_t encoded_length = safe85_encode(data.data(), data.size(), encode_buffer.data(), encode_buffer.size());
    ASSERT_GT(encoded_length, 0);
    const uint8_t* encoded_end = (uint8_t*)encode_buffer.data() + encoded_length;


    for(int packet_size=length-1; packet_size >= g_chunks_per_group; packet_size--)
    {
        KSLOG_DEBUG("packet size %d", packet_size);
        safe85_status status = SAFE85_STATUS_OK;
        const uint8_t* d_src = (uint8_t*)encode_buffer.data();
        uint8_t* d_dst = decode_buffer.data();
        uint8_t* d_dst_end = decode_buffer.data() + decode_buffer.size();

        while(d_src < encoded_end)
        {
            const uint8_t* d_src_end = d_src + packet_size;
            safe85_stream_state stream_state = SAFE85_STREAM_STATE_NONE;
            if(d_src_end >= encoded_end)
            {
                d_src_end = encoded_end;
                stream_state = (safe85_stream_state)(SAFE85_EXPECT_DST_STREAM_TO_END | SAFE85_DST_IS_AT_END_OF_STREAM);
            }
            KSLOG_DEBUG("Feed %d chars into %d bytes with state %d", d_src_end - d_src, d_dst_end - d_dst, stream_state);
            status = safe85_decode_feed(&d_src,
                                        d_src_end - d_src,
                                        &d_dst,
                                        d_dst_end - d_dst,
                                        stream_state);
            ASSERT_TRUE(status == SAFE85_STATUS_OK || status == SAFE85_STATUS_PARTIALLY_COMPLETE);
        }
        ASSERT_EQ(data, decode_buffer);
    }
}

void assert_decode(std::string expected_encoded, std::vector<uint8_t> expected_decoded)
{
    int64_t decoded_length = safe85_get_decoded_length(expected_encoded.size());
    ASSERT_GE(decoded_length, 0);
    std::vector<uint8_t> decode_buffer(decoded_length);
    int64_t actual_decode_used_bytes = safe85_decode((uint8_t*)expected_encoded.data(),
                                                     expected_encoded.size(),
                                                     decode_buffer.data(),
                                                     decode_buffer.size());
    ASSERT_GE(actual_decode_used_bytes, 1);
    std::vector<uint8_t> actual_decoded(decode_buffer.begin(), decode_buffer.begin() + actual_decode_used_bytes);
    ASSERT_EQ(expected_decoded, actual_decoded);
}

void assert_decode_status(int buffer_size, std::string encoded, int expected_status_code)
{
    std::vector<uint8_t> decode_buffer(buffer_size);
    int64_t actual_status_code = safe85_decode((uint8_t*)encoded.data(),
                                               encoded.size(),
                                               decode_buffer.data(),
                                               decode_buffer.size());
    ASSERT_EQ(expected_status_code, actual_status_code);
}

void assert_encode_length(int64_t length, std::string expected_encoded)
{
    std::vector<uint8_t> encode_buffer(100);
    int64_t bytes_written = safe85_write_length_field(length, encode_buffer.data(), encode_buffer.size());
    ASSERT_GT(bytes_written, 0);
    std::string actual_encoded(encode_buffer.begin(), encode_buffer.begin() + bytes_written);
    ASSERT_EQ(expected_encoded, actual_encoded);
}

void assert_encode_decode_length(int64_t start_length, int64_t end_length)
{
    for(int64_t i = start_length; i <= end_length; i++)
    {
        int64_t length = i;
        std::vector<uint8_t> encode_buffer(100);
        int64_t bytes_written = safe85_write_length_field(length, encode_buffer.data(), encode_buffer.size());
        ASSERT_GT(bytes_written, 0);
        int64_t actual_length = 0;
        int64_t bytes_read = safe85_read_length_field((uint8_t*)encode_buffer.data(), bytes_written, &actual_length);
        ASSERT_GT(bytes_read, 0);
        ASSERT_EQ(length, actual_length);
    }
}

void assert_encode_length_status(int64_t length, int buffer_size, int64_t expected_status)
{
    std::vector<uint8_t> encode_buffer(buffer_size);
    int64_t actual_status = safe85_write_length_field(length, encode_buffer.data(), encode_buffer.size());
    ASSERT_EQ(expected_status, actual_status);
}

void assert_decode_length(std::string encoded, int64_t expected_length, int64_t expected_status)
{
    int64_t actual_length;
    int64_t actual_status = safe85_read_length_field((uint8_t*)encoded.data(), encoded.size(), &actual_length);
    ASSERT_EQ(expected_status, actual_status);
    if(expected_status >= 0)
    {
        ASSERT_EQ(expected_length, actual_length);
    }
}



// --------------------
// Common Test Patterns
// --------------------

#define TEST_DECODE(NAME, ENCODED, ...) \
TEST(Decode, NAME) { assert_decode(ENCODED, __VA_ARGS__); }

#define TEST_ENCODE_DECODE(NAME, ENCODED, ...) \
TEST(EncodeDecode, NAME) { assert_encode_decode(ENCODED, __VA_ARGS__); }

#define TEST_ENCODE_DECODE_WITH_LENGTH(NAME, ENCODED, ...) \
TEST(EncodeDecodeWithLength, NAME) { assert_encode_decode_with_length(ENCODED, __VA_ARGS__); }

#define TEST_DECODE_ERROR(NAME, BUFFER_SIZE, ENCODED, EXPECTED_STATUS_CODE) \
TEST(DecodeError, NAME) { assert_decode_status(BUFFER_SIZE, ENCODED, EXPECTED_STATUS_CODE); }

#define TEST_ENCODE_LENGTH(NAME, LENGTH, ENCODED) \
TEST(EncodeLength, NAME) { assert_encode_length(LENGTH, ENCODED); }

#define TEST_ENCODE_LENGTH_STATUS(NAME, LENGTH, BUFFER_SIZE, STATUS) \
TEST(EncodeLengthStatus, NAME) { assert_encode_length_status(LENGTH, BUFFER_SIZE, STATUS); }

#define TEST_DECODE_LENGTH(NAME, ENCODED, EXPECTED_LENGTH, EXPECTED_STATUS) \
TEST(DecodeLength, NAME) { assert_decode_length(ENCODED, EXPECTED_LENGTH, EXPECTED_STATUS); }

#define TEST_ENCODE_DECODE_LENGTH(NAME, START_LENGTH, END_LENGTH) \
TEST(EncodeDecodeLength, NAME) { assert_encode_decode_length(START_LENGTH, END_LENGTH); }

#define TEST_DECODE_WITH_LENGTH_STATUS(NAME, ENCODED, FORCE_LENGTH, EXPECTED_STATUS) \
TEST(DecodeLength, NAME) { assert_decode_with_length_status(ENCODED, FORCE_LENGTH, EXPECTED_STATUS); }


// -----
// Tests
// -----

TEST(Library, version)
{
    const char* expected = "1.0.0";
    const char* actual = safe85_version();
    ASSERT_STREQ(expected, actual);
}

TEST_ENCODE_DECODE(_1_byte,  "%q",      {0xf1})
TEST_ENCODE_DECODE(_2_bytes, "$aF",     {0x2e, 0x99})
TEST_ENCODE_DECODE(_3_bytes, "Bq|Q",    {0xf2, 0x34, 0x56})
TEST_ENCODE_DECODE(_4_bytes, "@{632",  {0x4a, 0x88, 0xbc, 0xd1})
TEST_ENCODE_DECODE(_5_bytes, "|-Ps^$g", {0xff, 0x71, 0xdd, 0x3a, 0x92})

TEST_ENCODE_DECODE_WITH_LENGTH(_1_byte,  "$%q",      {0xf1})
TEST_ENCODE_DECODE_WITH_LENGTH(_2_bytes, "%$aF",     {0x2e, 0x99})
TEST_ENCODE_DECODE_WITH_LENGTH(_3_bytes, "(Bq|Q",    {0xf2, 0x34, 0x56})
TEST_ENCODE_DECODE_WITH_LENGTH(_4_bytes, ")@{632",   {0x4a, 0x88, 0xbc, 0xd1})
TEST_ENCODE_DECODE_WITH_LENGTH(_5_bytes, "*|-Ps^$g", {0xff, 0x71, 0xdd, 0x3a, 0x92})

TEST_DECODE_ERROR(dst_buffer_too_short_4, 4, "|-Ps^$g", SAFE85_ERROR_NOT_ENOUGH_ROOM)
TEST_DECODE_ERROR(dst_buffer_too_short_3, 3, "|-Ps^$g", SAFE85_ERROR_NOT_ENOUGH_ROOM)
TEST_DECODE_ERROR(dst_buffer_too_short_2, 2, "|-Ps^$g", SAFE85_ERROR_NOT_ENOUGH_ROOM)
TEST_DECODE_ERROR(dst_buffer_too_short_1, 1, "|-Ps^$g", SAFE85_ERROR_NOT_ENOUGH_ROOM)

TEST_DECODE_ERROR(invalid_0, 100, "#-Ps^$g", SAFE85_ERROR_INVALID_SOURCE_DATA)
TEST_DECODE_ERROR(invalid_1, 100, "|#Ps^$g", SAFE85_ERROR_INVALID_SOURCE_DATA)
TEST_DECODE_ERROR(invalid_2, 100, "|-#s^$g", SAFE85_ERROR_INVALID_SOURCE_DATA)
TEST_DECODE_ERROR(invalid_3, 100, "|-P#^$g", SAFE85_ERROR_INVALID_SOURCE_DATA)
TEST_DECODE_ERROR(invalid_4, 100, "|-Ps#$g", SAFE85_ERROR_INVALID_SOURCE_DATA)
TEST_DECODE_ERROR(invalid_5, 100, "|-Ps^#g", SAFE85_ERROR_INVALID_SOURCE_DATA)
TEST_DECODE_ERROR(invalid_6, 100, "|-Ps^$#", SAFE85_ERROR_INVALID_SOURCE_DATA)

TEST_DECODE(space_0, " |-Ps^$g", {0xff, 0x71, 0xdd, 0x3a, 0x92})
TEST_DECODE(space_1, "| -Ps^$g", {0xff, 0x71, 0xdd, 0x3a, 0x92})
TEST_DECODE(space_2, "|- Ps^$g", {0xff, 0x71, 0xdd, 0x3a, 0x92})
TEST_DECODE(space_3, "|-P s^$g", {0xff, 0x71, 0xdd, 0x3a, 0x92})
TEST_DECODE(space_4, "|-Ps ^$g", {0xff, 0x71, 0xdd, 0x3a, 0x92})
TEST_DECODE(space_5, "|-Ps^ $g", {0xff, 0x71, 0xdd, 0x3a, 0x92})
TEST_DECODE(space_6, "|-Ps^$ g", {0xff, 0x71, 0xdd, 0x3a, 0x92})
TEST_DECODE(space_7, "|-Ps^$g ", {0xff, 0x71, 0xdd, 0x3a, 0x92})

TEST_DECODE(lots_of_whitespace, "|\t\t-\r\n\n P   s^\t \t\t$g", {0xff, 0x71, 0xdd, 0x3a, 0x92})

TEST(Packetized, encode_dst_packeted)
{
    assert_chunked_encode_dst_packeted(163);
    assert_chunked_encode_dst_packeted(10);
    assert_chunked_encode_dst_packeted(200);
}

TEST(Packetized, encode_src_packeted)
{
    assert_chunked_encode_src_packeted(131);
    assert_chunked_encode_src_packeted(15);
    assert_chunked_encode_src_packeted(230);
}

TEST(Packetized, decode_dst_packeted)
{
    assert_chunked_decode_dst_packeted(102);
    assert_chunked_decode_dst_packeted(20);
    assert_chunked_decode_dst_packeted(250);
}

TEST_ENCODE_LENGTH(_0, 0, "!")
TEST_ENCODE_LENGTH(_1, 1, "$")
TEST_ENCODE_LENGTH(_10, 10, "0")
TEST_ENCODE_LENGTH(_31, 31, "H")
TEST_ENCODE_LENGTH(_32, 32, "J!")
TEST_ENCODE_LENGTH(_33, 33, "J$")
TEST_ENCODE_LENGTH(_1023, 1023, "iH")
TEST_ENCODE_LENGTH(_1024, 1024, "JI!")
TEST_ENCODE_LENGTH(_1025, 1025, "JI$")
TEST_ENCODE_LENGTH(_32767, 32767, "iiH")
TEST_ENCODE_LENGTH(_32768, 32768, "JII!")
TEST_ENCODE_LENGTH(_32769, 32769, "JII$")
TEST_ENCODE_LENGTH(_1048575, 1048575, "iiiH")
TEST_ENCODE_LENGTH(_1048576, 1048576, "JIII!")
TEST_ENCODE_LENGTH(_1048577, 1048577, "JIII$")

TEST_ENCODE_DECODE_LENGTH(_0_2000, 0, 2000)
TEST_ENCODE_DECODE_LENGTH(_32000_33000, 32000, 33000)

TEST_ENCODE_LENGTH_STATUS(_1_length_0,   1, 0, SAFE85_ERROR_NOT_ENOUGH_ROOM)
TEST_ENCODE_LENGTH_STATUS(_1_length_1,   1, 1, 1)
TEST_ENCODE_LENGTH_STATUS(_32_length_1, 32, 1, SAFE85_ERROR_NOT_ENOUGH_ROOM)
TEST_ENCODE_LENGTH_STATUS(_32_length_2, 32, 2, 2)

TEST_DECODE_LENGTH(_0,        "!",      0, 1)
TEST_DECODE_LENGTH(_31,       "H",     31, 1)
TEST_DECODE_LENGTH(_32_bad,   "J",     32, SAFE85_ERROR_UNTERMINATED_LENGTH_FIELD)
TEST_DECODE_LENGTH(_32,       "J!",    32, 2)
TEST_DECODE_LENGTH(_1024_bad, "JI",  1024, SAFE85_ERROR_UNTERMINATED_LENGTH_FIELD)
TEST_DECODE_LENGTH(_1024,     "JI!", 1024, 3)

TEST_DECODE_WITH_LENGTH_STATUS(_continues_beyond_end, "$$aF", -1, 1)
TEST_DECODE_WITH_LENGTH_STATUS(_longer_than_end, "9*|-Ps^$g", -1, SAFE85_ERROR_TRUNCATED_DATA)
TEST_DECODE_WITH_LENGTH_STATUS(_no_data,  "$", -1, SAFE85_ERROR_TRUNCATED_DATA)
TEST_DECODE_WITH_LENGTH_STATUS(_invalid, "/%q", -1, SAFE85_ERROR_INVALID_SOURCE_DATA)
TEST_DECODE_WITH_LENGTH_STATUS(_whitespace, " J $ 0 j a=a;60mK0lIG[I*8|Mh70U!_X!`XYRvJ]as!-_%W", -1, 33)

TEST(Length, invalid)
{
    std::vector<uint8_t> encoded_data(100);
    std::vector<uint8_t> decoded_data(100);

    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_get_decoded_length(-1));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_decode(encoded_data.data(), -1, decoded_data.data(), 1));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_decode(encoded_data.data(), 1, decoded_data.data(), -1));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85l_decode(encoded_data.data(), -1, decoded_data.data(), 1));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85l_decode(encoded_data.data(), 1, decoded_data.data(), -1));

    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_get_encoded_length(-1, false));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_get_encoded_length(-1, true));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_encode(decoded_data.data(), -1, encoded_data.data(), 1));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_encode(decoded_data.data(), 1, encoded_data.data(), -1));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85l_encode(decoded_data.data(), -1, encoded_data.data(), 1));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85l_encode(decoded_data.data(), 1, encoded_data.data(), -1));

    int64_t length = 0;
    uint8_t* encoded_ptr = (uint8_t*)encoded_data.data();
    const uint8_t* const_encoded_ptr = encoded_ptr;
    uint8_t* decoded_ptr = decoded_data.data();
    const uint8_t* const_decoded_ptr = decoded_ptr;

    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_read_length_field((uint8_t*)encoded_data.data(), -1, &length));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_decode_feed(&const_encoded_ptr, -1, &decoded_ptr, 1, SAFE85_STREAM_STATE_NONE));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_decode_feed(&const_encoded_ptr, 1, &decoded_ptr, -1, SAFE85_STREAM_STATE_NONE));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_decode_feed(&const_encoded_ptr, -1, &decoded_ptr, 1, SAFE85_SRC_IS_AT_END_OF_STREAM));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_decode_feed(&const_encoded_ptr, 1, &decoded_ptr, -1, SAFE85_SRC_IS_AT_END_OF_STREAM));

    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_write_length_field(1, (uint8_t*)encoded_data.data(), -1));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_encode_feed(&const_decoded_ptr, -1, &encoded_ptr, 1, false));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_encode_feed(&const_decoded_ptr, 1, &encoded_ptr, -1, false));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_encode_feed(&const_decoded_ptr, -1, &encoded_ptr, 1, true));
    ASSERT_EQ(SAFE85_ERROR_INVALID_LENGTH, safe85_encode_feed(&const_decoded_ptr, 1, &encoded_ptr, -1, true));
}


// Specification Examples:

TEST_ENCODE_DECODE(example_1, "8F2{*RVCLI8LDzZ!3e", {0x39, 0x12, 0x82, 0xe1, 0x81, 0x39, 0xd9, 0x8b, 0x39, 0x4c, 0x63, 0x9d, 0x04, 0x8c})
TEST_ENCODE_DECODE(example_2, "szEXiyl.1C!Tc1o-w;X", {0xe6, 0x12, 0xa6, 0x9f, 0xf8, 0x38, 0x6d, 0x7b, 0x01, 0x99, 0x3e, 0x6c, 0x53, 0x7b, 0x60})
TEST_ENCODE_DECODE(example_3, "0stg*0r4~*MKP6zkj.X1", {0x21, 0xd1, 0x7d, 0x3f, 0x21, 0xc1, 0x88, 0x99, 0x71, 0x45, 0x96, 0xad, 0xcc, 0x96, 0x79, 0xd8})
TEST_DECODE_LENGTH(example_1,    "$",      1, 1)
TEST_DECODE_LENGTH(example_31,   "H",     31, 1)
TEST_DECODE_LENGTH(example_32,   "J!",    32, 2)
TEST_DECODE_LENGTH(example_2000, "Jh6", 2000, 3)
TEST_ENCODE_DECODE_WITH_LENGTH(example_w_length, "J$0ja=a;60mK0lIG[I*8|Mh70U!_X!`XYRvJ]as!-_%W", {0x21, 0x7b, 0x01, 0x99, 0x3e, 0xd1, 0x7d, 0x3f, 0x21, 0x8b, 0x39, 0x4c, 0x63, 0xc1, 0x88, 0x21, 0xc1, 0x88, 0x99, 0x71, 0xa6, 0x9f, 0xf8, 0x45, 0x96, 0xe1, 0x81, 0x39, 0xad, 0xcc, 0x96, 0x79, 0xd8})


// README Examples:

static void my_receive_decoded_data_function(std::vector<uint8_t>& data)
{
    (void)data;
}
static void my_receive_encoded_data_function(std::string& data)
{
    (void)data;
}

TEST(Example, decoding)
{
    std::string my_source_data = "8F2{*RVCLI8LDzZ!3e";

    int64_t decoded_length = safe85_get_decoded_length(my_source_data.size());
    std::vector<uint8_t> decode_buffer(decoded_length);

    int64_t used_bytes = safe85_decode((uint8_t*)my_source_data.data(),
                                       my_source_data.size(),
                                       decode_buffer.data(),
                                       decode_buffer.size());
    if(used_bytes < 0)
    {
        // TODO: used_bytes is an error code.
        ASSERT_TRUE(false);
    }
    std::vector<uint8_t> decoded_data(decode_buffer.begin(), decode_buffer.begin() + used_bytes);
    my_receive_decoded_data_function(decoded_data);
}

TEST(Example, decoding_with_length)
{
    std::string my_source_data = "48F2{*RVCLI8LDzZ!3e";

    int64_t decoded_length = safe85_get_decoded_length(my_source_data.size());
    std::vector<uint8_t> decode_buffer(decoded_length);

    int64_t used_bytes = safe85l_decode((uint8_t*)my_source_data.data(),
                                        my_source_data.size(),
                                        decode_buffer.data(),
                                        decode_buffer.size());
    if(used_bytes < 0)
    {
        // TODO: used_bytes is an error code.
        ASSERT_TRUE(false);
    }
    std::vector<uint8_t> decoded_data(decode_buffer.begin(), decode_buffer.begin() + used_bytes);
    my_receive_decoded_data_function(decoded_data);
}

TEST(Example, encoding)
{
    std::vector<uint8_t> my_source_data({0x39, 0x12, 0x82, 0xe1, 0x81, 0x39, 0xd9, 0x8b, 0x39, 0x4c, 0x63, 0x9d, 0x04, 0x8c});

    bool should_include_length = false;
    int64_t encoded_length = safe85_get_encoded_length(my_source_data.size(), should_include_length);
    std::vector<uint8_t> encode_buffer(encoded_length);

    int64_t used_bytes = safe85_encode(my_source_data.data(),
                                       my_source_data.size(),
                                       encode_buffer.data(),
                                       encode_buffer.size());
    if(used_bytes < 0)
    {
        // TODO: used_bytes is an error code.
        ASSERT_TRUE(false);
    }
    std::string encoded_data(encode_buffer.begin(), encode_buffer.begin() + used_bytes);
    my_receive_encoded_data_function(encoded_data);
}

TEST(Example, encoding_with_length)
{
    std::vector<uint8_t> my_source_data({0x39, 0x12, 0x82, 0xe1, 0x81, 0x39, 0xd9, 0x8b, 0x39, 0x4c, 0x63, 0x9d, 0x04, 0x8c});

    bool should_include_length = true;
    int64_t encoded_length = safe85_get_encoded_length(my_source_data.size(), should_include_length);
    std::vector<uint8_t> encode_buffer(encoded_length);

    int64_t used_bytes = safe85l_encode(my_source_data.data(),
                                        my_source_data.size(),
                                        encode_buffer.data(),
                                        encode_buffer.size());
    if(used_bytes < 0)
    {
        // TODO: used_bytes is an error code.
        ASSERT_TRUE(false);
    }
    std::string encoded_data(encode_buffer.begin(), encode_buffer.begin() + used_bytes);
    my_receive_encoded_data_function(encoded_data);
}
