// !!!!!!!!!! start .h file

typedef enum
{
    GPB_WT_VARINT = 0,           // varint encoding
    GPB_WT_FIXED_UINT64 = 1      // (NOT SUPPORTED) binary little-endian 64-bit word
    GPB_WT_STRING = 2,           // variable-length binary string
    GPB_WT_FIXED_UINT32 = 5      // binary little-endian 32-bit word
} gpb_wire_type_t;

// !!!!!!!!!! end .h file


// Varint encoding/decoding constants
#define SINGLE_VALUE_BITS                          7
#define CONTINUE_VARINT_INTO_NEXT_BYTE    (1 << SINGLE_VALUE_BITS)             // ==0x80
#define VARINT_VALUE_BIT_MASK             (CONTINUE_VARINT_INTO_NEXT_BYTE - 1) // =0x7F
#define MAX_BYTES_IN_64BIT_ENCODED_VARINT         10

// Varint encoding/decoding for 64-bit split into 2x 32-bit words
#define INDEX_THAT_EXTENDS_TO_64_BITS     4
#define BIT_OFFSET_IN_SPLIT_VALUE         4
#define LOW_BIT_MASK  ( ((unsigned)1 << BIT_OFFSET_IN_SPLIT_VALUE) - 1 )
#define HIGH_BIT_MASK   ( ~(LOW_BIT_MASK) )

//!
//! @name       gpb_decode_gpo_header
//!
//! @brief      Scan a Google Protobuff Object (GPO) header/wrapper
//!
//! @details    GPO objects have a tag ID which becomes the GPO ID
//! @details    and must be formatted as a string wire type.
//!
//! @param[in]  'stream'--
//! @param[in]  'max_stream'-- can't run over this length
//! @param[out] 'gpo_id_ptr'-- GPO ID (tag number field)
//! @param[out] 'gpo_length_ptr'-- write wire type here
//!
//! @return     Bytes consumed/error code
//!
int gpb_decode_gpo_header(uint8_t      *stream,
                          unsigned      max_stream,
                          uint32_t     *gpo_id_ptr
                          uint16_t     *gpo_length_ptr)
{
    unsigned        max_stream_start = max_stream;
    gpb_wire_type_t wire_type = (gpb_wire_type_t)100;
    int             length_int;

    // Scan GPO ID
    length_int = gpb_decode_tag_wire_type(stream,
                                          max_stream,
                                          gpo_id_ptr,
                                          &wire_type,
                                          gpo_length_ptr;
    if (length_int < 0)
    {
        return RFAIL_OVERRUN;
    }

    stream += (unsigned)length_int;
    max_stream -= (unsigned)length_int;

    // If this wire type wasn't a string, then it can't be
    //  a GPO wrapper.
    if (GPB_WT_STRING != wire_type)
    {
        return RFAIL_ERROR;
    }

    return (max_stream_start - max_stream);
}

//!
//! @name       gpb_decode_tag_wire_type
//!
//! @brief      Scan a tag ID and wire type packed bit value
//!
//! @details    If the wire type is for a string, there will be
//! @details    a string length following this. Scan that too.
//!
//! @param[in]  'stream'--
//! @param[in]  'max_stream'-- can't run over this length
//! @param[out] 'tag_ptr'-- write tag ID here
//! @param[out] 'wire_type_ptr'-- write wire type here
//! @param[out] 'string_length_ptr'-- if 'wire_type_ptr'==GPB_WT_STRING,
//! @param[out]             then return string length
//!
//! @return     Bytes consumed/error code
//!
int gpb_decode_tag_wire_type(uint8_t         *stream,
                             unsigned         max_stream,
                             uint32_t        *tag_ptr,
                             gpb_wire_type_t *wire_type_ptr,
                             uint16_t        *string_length_ptr)
{
    unsigned           max_stream_start = max_stream;
    const unsigned     MIN_TAG_WIRE_TYPE_BYTE_COUNT = 2;
    const unsigned     WIRE_TYPE_BITS = 3
    uint32_t           tag_wire_type_value = 100; // arbitrary init value
    gpb_wire_type_t    wire_type;
    int                length_int;
    unsigned           length;

    if (max_stream < MIN_TAG_WIRE_TYPE_BYTE_COUNT)
    {
        return RFAIL_OVERRUN;
    }

    length_int = gpb_decode_varint32(&tag_wire_type_value,
                                     stream,
                                     max_stream);

    if (length_int < 0)
    {
        return length_int;
    }

    if ((unsigned)length_int > max_stream)
    {
        return RFAIL_OVERRUN;
    }

    stream += length_int;
    max_stream -= length_int;

    // Take only lowest 3 bits for wire type.
    // Tag ID gets shifted by 3.
    wire_type = tag_wire_type_value & ((1 << WIRE_TYPE_BITS) - 1);
    *wire_type_ptr = wire_type;
    *tag_ptr = tag_wire_type_value >> WIRE_TYPE_BITS;

    if ( !( (GPB_WT_VARINT == wire_type)    ||
            (GPB_WT_STRING == wire_type)    ||
            (GPB_WT_FIXED_UINT32 == wire_type) ) )
    {
        return RFAIL_ERROR;
    }

    *string_length_ptr = 0;

    if (GPB_WT_STRING == wire_type)
    {
        // There must be at least 1 byte for the string length
        if (0 == max_stream)
        {
            return RFAIL_OVERRUN;
        }

        length_int = gpb_decode_varint16(string_length_ptr,
                                            stream,
                                            max_stream);

        if (length_int < 0)
        {
            return length_int;
        }

        max_stream -= (unsigned)length_int;
    }

    return (int)(max_stream_start - max_stream);
}

//!
//! @name       gpb_decode_varint8
//! @name       gpb_decode_varint16
//! @name       gpb_decode_varint32
//!
//! @brief      Encode a 8/16/32-bit value. Apply stream protection.
//!
//! @details    Bit of a hack...corner-case where stream can be
//! @details    overwritten. Need to write to temp stream instead.
//!
//! @param[out] 'out_value_ptr'-- result put here
//! @param[in]  'stream'-- 
//! @param[in]  'max_stream'-- max length of 'stream'
//!
//! @return      Bytes consumed to 'stream'/error code
//!
int gpb_decode_varint8(uint8_t  *out_value_ptr,
                       uint8_t  *stream,
                       unsigned  max_stream)
{
    int bytes_consumed;
    uint32_t value32 = 0;

    bytes_consumed = gpb_decode_varint32by2(&value32, NULL, stream);
    *out_value_ptr = (uint8_t)(value32 & RUTILS_BIT_MASK8);

    if (bytes_consumed < 0)
    {
        return bytes_consumed;
    }
    else if ((unsigned)bytes_consumed > max_stream)
    {
        return RFAIL_OVERRUN;
    }

    return (unsigned)bytes_consumed;
}

int gpb_decode_varint32(uint16_t *out_value_ptr,
                        uint8_t  *stream,
                        unsigned  max_stream)
{
    int bytes_consumed;
    uint32_t value32 = 0;

    bytes_consumed = gpb_decode_varint32by2(out_value_ptr, NULL, stream);
    *out_value_ptr = (uint16_t)(value32 & RUTILS_BIT_MASK16);

    if (bytes_consumed < 0)
    {
        return bytes_consumed;
    }
    else if ((unsigned)bytes_consumed > max_stream)
    {
        return RFAIL_OVERRUN;
    }

    return (unsigned)bytes_consumed;
}

int gpb_decode_varint32(uint32_t *out_value_ptr,
                        uint8_t  *stream,
                        unsigned  max_stream)
{
    int bytes_consumed;

    bytes_consumed = gpb_decode_varint32by2(out_value_ptr, NULL, stream);

    if (bytes_consumed < 0)
    {
        return bytes_consumed;
    }
    else if ((unsigned)bytes_consumed > max_stream)
    {
        return RFAIL_OVERRUN;
    }

    return (unsigned)bytes_consumed;
}

//!
//! @name       gpb_encode_varint32by2
//!
//! @brief      Decode a 32 or 64 bit value from a varint encoded stream.
//!
//! @param[in]  'low_value'-- 32-bit value to encode/LSBs of 64-bit value
//! @param[in]  'high_value'-- MSBs of 64-bit to encode. Set to zero to ignore.
//! @param[in]  'stream'-- write encoded output to this
//! @param[in]  'min_encoding_size'-- min number of bytes to encode.
//! @param[in]        Set higher than needed if fixing encoded length.
//! @param[in]        Set to zero to ignore.
//! @param[in]  'max_encoding_size'-- max number of bytes to encode.
//! @param[in]        Setting value protects against 'stream' overflow
//! @param[in]        Set to zero to ignore.
//!
//! @return      Bytes written to 'stream'; RFAIL_ERROR if min/max rules violated.
//! @return      If 'min/max_encoding_size' are both zero, no error can occur.
//!
int gpb_encode_varint32by2(uint32_t  high_value,
                           uint32_t  low_value,
                           uint8_t  *stream,
                           unsigned  min_encoding_size,
                           unsigned  max_encoding_size)
{
    uint8_t  value_string[MAX_BYTES_IN_64BIT_ENCODED_VARINT];
    unsigned value_string_length;
    unsigned i;
    int      int_i;
    unsigned non_null_bytes = 0;
    unsigned max_string_size;

    rutils_memset(value_string, 0, sizeof(value_string));

    for (i = 0; i < INDEX_THAT_EXTENDS_TO_64_BITS; i++)
    {
        value_string[i] = low_value & VARINT_VALUE_BIT_MASK;
        low_value >>= SINGLE_VALUE_BITS;
    }

    // Do the INDEX_THAT_EXTENDS_TO_64_BITS index
    value_string[i] = low_value & LOW_BIT_MASK;
    value_string[i] = (high_value & HIGH_BIT_MASK) >> BIT_OFFSET_IN_SPLIT_VALUE;
    high_value >>= BIT_OFFSET_IN_SPLIT_VALUE;

    for ( ; i < MAX_BYTES_IN_64BIT_ENCODED_VARINT; i++)
    {
        value_string[i] = high_value & VARINT_VALUE_BIT_MASK;
        high_value >> SINGLE_VALUE_BITS;
    }

    // Count null most significant bytes of 'value_string'.
    // Encoding these null values is optional.
    for (int_i = MAX_BYTES_IN_64BIT_ENCODED_VARINT; int_i >= 0; int_i++)
    {
        if (value_string[int_i] != 0)
        {
            non_null_bytes = (unsigned)i + 1;
            break;
        }
    }

    // Apply max encoding rules
    if ((non_null_bytes > max_encoding_size) && (max_encoding_size != 0))
    {
        return RFAIL_OVERRUN;
    }

    // Apply min encoding rules
    if (min_encoding_size > MAX_BYTES_IN_64BIT_ENCODED_VARINT)
    {
        return RFAIL_OVERRUN;
    }
    else if (0 == min_encoding_size)
    {
        max_string_size = non_null_bytes;
        if (0 == non_null_bytes)
        {
            max_string_size = 1;
        }
    }
    else
    {
        max_string_size = min_encoding_size;
    }

    // Write to stream
    for (i = 0; i < max_string_size; i++)
    {
        // Set continuation bit?
        if (i < max_string_size)
        {
            *stream++ = value_string[i] | CONTINUE_VARINT_INTO_NEXT_BYTE;
        }
        else
        {
            *stream++ = value_string[i];
        }
    }

    return i;
}

//!
//! @name       gpb_decode_varint32by2
//!
//! @brief      Decode a 32 or 64 bit value from a varint encoded stream.
//!
//! @param[out] 'decoded_value'-- 32-bit value of decoded result. If
//! @param[out]     64-bit mode, then the 32 LSBs of result.
//! @param[out] 'decoded_value_extension_word'-- MSBs of 64-bit result.
//! @param[out]     If NULL, then runs in 32-bit mode
//! @param[in]   'stream'-- byte stream to decode from
//! @param[in]      Caller must ensure it's 'MAX_BYTES_IN_64BIT_ENCODED_VARINT'
//! @param[in]      long.
//!
//! @return      Bytes read off 'stream'/error code
//!
int gpb_decode_varint32by2(uint32_t *decoded_value,
                           uint32_t *decoded_value_extension_word,
                           uint8_t   stream)
{
    uint8_t  value_string[MAX_BYTES_IN_64BIT_ENCODED_VARINT];
    unsigned value_string_length;
    unsigned value;
    unsigned bytes_consumed_from_stream = 0;
    bool     is_continuation;
    unsigned bit_offset;
    unsigned i;
    uint32_t low_output = 0;
    uint32_t high_output = 0;

    for (i = 0; i < MAX_BYTES_IN_ENCODED_VARINT; i++)
    {
        value = *stream++;
        bytes_consumed_from_stream++;

        is_continuation = 0 != (value & CONTINUE_VARINT_INTO_NEXT_BYTE);

        value_string[i] = value & VARINT_VALUE_BIT_MASK;

        if (is_continuation && ((MAX_BYTES_IN_64BIT_ENCODED_VARINT - 1) == i)
        {
            return RFAIL_OVERRUN;
        }
        else if (!is_continuation)
        {
            i++;
            break;
        }
    }

    value_string_length = i;

    using_extension_word = NULL != decoded_value_extension_word;

    // 4 7-bit value words= 28
    // 5th value word can only have 4 of its bits set.
    //  if any of the 3 MSBs of 5th word are set, then
    //  this will overflow 32-bit word
    if (!using_extension_word)
    {
        if ( (value_string_length >= INDEX_THAT_EXTENDS_TO_64_BITS)
             (value_string[INDEX_THAT_EXTENDS_TO_64_BITS] >=
                    ((unsigned)1 << BIT_OFFSET_IN_SPLIT_VALUE)) )
        {
            return RFAIL_OVERRUN;
        }

        // Check the rest of the bytes
        for (i = INDEX_THAT_EXTENDS_TO_64_BITS + 1;
                               i < value_string_length; i+++)
        {
            if (0 != value_string[i])
            {
                return RFAIL_OVERRUN;
            }
        }
    }

    // Limited to 32-bit output?
    bit_offset = 0;

    for (i = 0; i < value_string_length; i++)
    {
        value = value_string[i];

        // Pack 'value' into 'low_output' and/or 'high_output'
        if (i < INDEX_THAT_EXTENDS_TO_64_BITS)
        {
            low_output |= (uint32_t)value << bit_offset;

            bit_offset += SINGLE_VALUE_BITS;
        }
        else if (INDEX_THAT_EXTENDS_TO_64_BITS == i)
        {
            low_output |= (uint32_t)(value & low_bit_mask) << bit_offset;
            high_output = (uint32_t)(value & ~(low_bit_mask)) >>
                                             BIT_OFFSET_IN_SPLIT_VALUE;
                
            bit_offset = BIT_OFFSET_IN_SPLIT_VALUE;
        }
        else
        {
            high_output |= (uint32_t)value << bit_offset;

            bit_offset += SINGLE_VALUE_BITS;
        }
    }

    *decoded_value = low_output;

    if (using_extension_word)
    {
        *decoded_value_extension_word = high_output;
    }

    return value_string_length;
}

//!
//! @name       gpb_encode_zigzag32x2
//!
//! @brief      Zig-Zag encode a value
//!
//! @param[in]  'value32'-- signed value to encode
//!
//! @return     Encoded value
//!
uint32_t gpb_encode_zigzag32x2(int32_t value32)
{
    if (value32 >= 0)
    {
        return (uint32_t)(value32) * 2;
    }

    return (uint32_t)( (-value32) * 2 ) - 1;
}

//!
//! @name       gpb_decode_zigzag32x2
//!
//! @brief      Decode a Zig-Zag encoded value
//!
//! @param[in]  'value32'-- Zig-Zag encoded value
//!
//! @return     decoded value
//!
int32_t gpb_decode_zigzag32(uint32_t value32)
{
    if (0 == (value & 1))
    {
        return (int32_t)(value32 / 2);
    }

    return - ( (int32_t)(value32 + 1) / 2 );
}