/*
Copyright (c) 2018, Bernie Woodland
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

//! @file     rcoap-protocol.c
//! @authors  Bernie Woodland
//! @date     27Mar18
//!
//! @brief    CoAP protocol (RFC 7252) Encoding and Decoding
//!


// for deserialization
#define VERSION_FROM_BYTE(x)       ( (x) >> 6 )
#define TYPE_FROM_BYTE(x)          ( (rcoap_type_t)(((x) >> 4) & 0x3) )
#define TOKEN_LENGTH_FROM_BYTE(x)  ( (x) & 0xF )

// for serialization
#define VERSION_TYPE_TOKEN_LENGTH_TO_BYTE(version, type, token_length)          \
        ( ((version & 0x3) << 6) | ((type & 0x3) << 4) | (token_length & 0x0F) )
#define OPTION_BIT_OFFSET           4
#define EXTENDED_STRING_MAX_SIZE    5

#define MIN_HEADER_LENGTH_BYTES     4

#define PAYLOAD_MARKER           0xFF

#define OPTION_DELTA_FROM_BYTE(x)  ( ((x) >> 4) & 0x0F )
#define OPTION_LENGTH_FROM_BYTE(x) ( (x) & 0x0F )

#define FOLLOWON_UINT8                   13
#define FOLLOWON_UINT16                  14
#define ADDON_UINT8                      13
#define ADDON_UINT16                    269

#define DELTA_SPECIAL_CONSTRUCT_UINT8    13
#define DELTA_SPECIAL_CONSTRUCT_UINT16   14
#define DELTA_ADJUSTMENT_UINT8           13
#define DELTA_ADJUSTMENT_UINT8          269



//!
//! @name       uri_path_mark_slashes
//!
//! @brief      Build list of offsets into string where slash char
//! @brief      exists.
//!
//! @param[in]  'uri_path'-- null-terminated string to parse
//! @param[in]      ex: "/path1/path2/path3"
//! @param[in]  'slash_offset'-- array of offset values. Offsets
//! @param[in]      of zero is first char in 'uri_path'.
//! @param[in]  'max_slash_offset'-- length of 'slash_offset' array
//!
//! @return     number of values filled into 'slash_offset'
//!
static unsigned uri_path_mark_slashes(const char *uri_path,
                                      uint8_t    *slash_offset,
                                      unsigned    max_slash_offset)
{
    unsigned total_length;
    unsigned i;
    unsigned num_slashes = 0;

    total_length = rutils_strlen(uri_path);

    if (0 == total_length)
    {
        return 0;
    }

    // Loop through all chars in 'uri_path' string
    for (i = 0; i < total_length; i++)
    {
        if ('/' == uri_path[i])
        {
            // sanity check for 'slash_offset' overflow
            if (num_slashes == max_slash_offset)
            {
                return num_slashes;
            }

            slash_offset[num_slashes++] = i;
        }
    }

    return num_slashes;
}

//!
//! @name       uri_path_mark_subpaths
//!
//! @brief      Build list of subpaths in 'uri_path' string
//!
//! @details    Each subpath in 'uri_path' is delimited by a
//! @details    slash character. Find subpath start indices and
//! @details    subpath lengths.
//! @details    If there are two consecutive slash chars in
//! @details    'uri_path', no subpath between them.
//! @details    Leading and trailing slashes ignored.
//! @details        Ex:
//! @details      /pathA//pathB/pathC has 3 subpaths:
//! @details         "pathA", "pathB", pathC"
//!
//! @param[in]  'uri_path'-- null-terminated string to parse
//! @param[in]      ex: "/path1/path2/path3"
//! @param[out] 'subpath_offset'-- array of offsets where subpaths begin.
//! @param[out]     Subpath begins on first char after slash, not slash.
//! @param[out] 'subpath_length'-- array of length values: length for
//! @param[out]     each subpath.
//! @param[in]  'max_subpath'-- max length of 'subpath_offset'/'subpath_length'
//! @param[in]             arrays.
//!
//! @return     number of values filled into 'subpath_offset'/'subpath_length'
//!
static unsigned uri_path_mark_subpaths(const char    *uri_path,
                                       uint8_t       *subpath_offset,
                                       uint8_t       *subpath_length,
                                       unsigned       max_subpath)
{
    uint8_t  slash_offset[RCOAP_MAX_URI_PATH+5];   // +5 to allow for some extras
    unsigned num_slashes = 0;
    unsigned this_slash;
    unsigned last_slash;
    unsigned num_subpaths = 0;
    unsigned length;
    unsigned i;
    unsigned last_slash_offset;
    bool     has_trailing_slash;
    unsigned trailing_length;

    length = rutils_strlen(uri_path);
    if (0 == length)
    {
        return 0;
    }

    num_slashes = rcoap_uri_path_mark_slashes(uri_path,
                                              slash_offset,
                                              sizeof(slash_offset));

    last_slash = 0;

    // Walk through all slashes:
    // find length up to slash
    for (i = 0; i < num_slashes; i++)
    {
        this_slash = slash_offset[i];

        // Filter out consecutive slashes.
        // Also filters out slash at offset zero.
        if (this_slash > last_slash + 1)
        {
            if (num_subpaths == max_subpath)
            {
                return num_subpaths;
            }

            subpath_offset[num_subpaths] = last_slash;
            subpath_length[num_subpaths] = this_slash - last_slash;
            num_subpaths++;
         }
    }

    // Set 'last_slash_offset' to last slash in 'uri_path' 
    // If there is no slash, set it to zero.
    if (num_slashes > 0)
    {
        last_slash_offset = slash_offset[num_slashes - 1];
    }
    else
    {
        last_slash_offset = 0;
    }

    // Check the next char after the last slash.
    // If it is a '\0', then the last char in the string was a slash
    //has_trailing_slash = uri_path[last_slash_offset + 1] != '\0';
    //   .... equivalent to:
    has_trailing_slash = uri_path[length - 1] == '/';

    // Do case of text after slash to end of 'uri_path'.
    // Includes case of no slashes at all.
    if (!has_trailing_slash)
    {
        trailing_length = rutils_strlen(&uri_path[last_slash_offset + 1]);

        if (trailing_length > 0)
        {
            if (num_subpaths == max_subpath)
            {
                return num_subpaths;
            }

            subpath_offset[num_subpaths] = last_slash_offset + 1;
            subpath_length[num_subpaths] = trailing_length;
            num_subpaths++;
        }
    }

    return num_subpaths;
}            



//!
//! @name       rcoap_serialize_header
//!
//! @brief      Convert CoAP header and URI Path option to a
//! @brief      binary stream.
//!
//! @param[in]  'stream'-- location to start writing/writing bytes
//! @param[in]  'max_stream'-- Do-not-go-over length of 'stream'
//! @param[in]  'header'-- CoAP header based on this
//! @param[in]  'full_uri_path'-- string with uri path option.
//! @param[in]       Ex: "/top/mid/low"
//! @param[in]     "top", "mid", "low" are sub-paths. These will get
//! @param[in]     expanded into their own URI Path option. This is
//! @param[in]     per the RFC.
//! @param[in]  'will_have_payload'-- caller will append payload,
//! @param[in]     therefore, this call must add payload marker.
//!
//! @return     number of bytes written to 'stream'/serialized length.
//! @return      If error, error code returned (always < zero)
//!
int rcoap_serialize_header(uint8_t        *stream,
                           unsigned        max_stream,
                           rcoap_header_t *header,
                           const char     *full_uri_path,
                           bool            will_have_payload)
{
    unsigned  start_max_stream;
    char    (*uri_path_fragment)[RCOAP_MAX_URI_PATH];
    uint8_t   uri_path_fragment_length[RCOAP_MAX_URI_PATH];
    unsigned  uri_path_count;
    unsigned  current_uri_path = 0;
    unsigned  i;
    unsigned  current_option;
    unsigned  option_delta;
    unsigned  previous_option;
    unsigned  option_length;
    uint8_t   extended_string[EXTENDED_STRING_MAX_SIZE];

    start_max_stream = max_stream;

    if (max_stream < MIN_HEADER_LENGTH_BYTES)
    {
        return RFAIL_OVERRUN;
    }

    *stream++ = VERSION_TYPE_TOKEN_LENGTH_TO_BYTE(
                   1, header->type, header->token_length);
    max_stream--;

    header->response_code = *stream++;
    max_stream--;

    rutils_word16_to_stream(stream, header->message_id);
    max_stream -= BYTES_PER_WORD16;

    if (max_stream < header->token_length)
    {
        return RFAIL_OVERRUN;
    }

    rutils_memcpy(stream, header->token, header->token_length);
    max_stream -= header->token_length;

    // Calculate uri path fragment counts from full uri path
    uri_path_count = uri_path_mark_subpaths(full_uri_path,
                                            uri_path_fragment,
                                            uri_path_fragment_length,
                                            RCOAP_MAX_URI_PATH);

    previous_option = 0;

    // Write all options.
    // We're only doing URI Path.
    for (i = 0; i < uri_path_count; i++)
    {
        current_option = RCOAP_OPT_URI_PATH;
        option_delta = current_option + previous_option;

        if (option_delta < DELTA_ADJUSTMENT_UINT8)
        {
            extended_string[0] = option_delta << OPTION_BIT_OFFSET;
            extended_string_length = 1;
        }
        else if (option_delta < DELTA_ADJUSTMENT_UINT16)
        {
            extended_string[0] = DELTA_SPECIAL_CONSTRUCT_UINT8 << OPTION_BIT_OFFSET;
            extended_string[1] = (uint8_t)(option_delta - DELTA_ADJUSTMENT_UINT8);
            extended_string_length = 2;
        }
        else
        {
            extended_string[0] = DELTA_SPECIAL_CONSTRUCT_UINT16 << OPTION_BIT_OFFSET;
            rutils_word16_to_stream(&extended_string[1], (uint16_t)(option_delta - DELTA_ADJUSTMENT_UINT16));
            extended_string_length = 3;
        }

        option_length = uri_path_fragment_length[current_uri_path];

        if (option_length < DELTA_ADJUSTMENT_UINT8)
        {
            extended_string[0] |= option_length;
        }
        else if (option_delta < DELTA_ADJUSTMENT_UINT16)
        {
            extended_string[0] |= DELTA_SPECIAL_CONSTRUCT_UINT8;
            extended_string[extended_string_length] = (uint8_t)(option_length - DELTA_ADJUSTMENT_UINT8);
            extended_string_length++;
        }
        else
        {
            extended_string[0] |= DELTA_SPECIAL_CONSTRUCT_UINT16;
            rutils_word16_to_stream(&extended_string[extended_string_length],
                               (uint16_t)(option_length - DELTA_ADJUSTMENT_UINT16));
            extended_string_length += 2;
        }

        if ((extended_string_length > EXTENDED_STRING_MAX_SIZE) ||
            (extended_string_length > max_stream))
        {
            return RFAIL_OVERRUN;
        }

        // Copy in option delta and length block
        rutils_memcpy(stream, extended_string, extended_string_length);
        stream += extended_string_length;
        max_stream -= extended_string_length;

        // Sanity check option text length
        if (extended_string_length > max_stream)
        {
            return RFAIL_OVERRUN;
        }

        // Copy option text
        rutils_memcpy(stream, uri_path_fragment[current_uri_path], option_length);
        stream += option_length;
        max_stream -= option_length;

        // for next pass of loop
        previous_option = current_option;
    }

    // If there's going to be a payload, add a payload marker
    if (will_have_payload)
    {
        if (max_stream == 0)
        {
            return RFAIL_OVERRUN;
        }

        *stream++ = PAYLOAD_MARKER;
        max_stream--;
    }

    return (start_max_stream - max_stream);
}

//!
//! @name       rcoap_deserialize_header
//!
//! @brief      Convert CoAP header and URI Path option to a
//! @brief      binary stream.
//!
//! @param[in]  'stream'-- location to start writing/writing bytes
//! @param[in]  'max_stream'-- Do-not-go-over length of 'stream'
//! @param[out] 'header'-- CoAP header based on this
//! @param[out] 'uri_path'-- URI Path options concatenated into a
//! @param[out]    single URI Path string. Null terminated.
//! @param[in]       Ex: "/top/mid/low"
//! @param[in]  'max_uri_path'-- max length of 'uri_path'
//!
//! @return     number of bytes in CoAP header.
//! @return      If error, error code returned (always < zero)
//!
int rcoap_deserialize_header(uint8_t        *stream,
                             unsigned        max_stream,
                             rcoap_header_t *header,
                             uint8_t        *uri_path,
                             unsigned        max_uri_path)
{
    unsigned start_max_stream;
    unsigned version_type_token_length;
    unsigned option_delta_non_extended;
    unsigned option_length_non_extended;
    unsigned option_delta;
    unsigned non_extended_byte;
    unsigned option_id;
    unsigned cummulative_delta;
    char    (*uri_path_fragment)[RCOAP_MAX_URI_PATH];
    uint8_t  uri_path_fragment_length[RCOAP_MAX_URI_PATH];
    unsigned uri_path_count;
    unsigned uri_path_length;

    start_max_stream = max_stream;

    if (max_stream < MIN_HEADER_LENGTH_BYTES)
    {
        return RFAIL_OVERRUN;
    }

    version_type_token_length = *stream++;

    header->type = TYPE_FROM_BYTE(version_type_token_length);
    header->token_length = TOKEN_LENGTH_FROM_BYTE(version_type_token_length);

    // Sanity checks
    if ( (VERSION_FROM_BYTE(version_type_token_length) != 1) ||
         (header->token_length > RCOAP_MAX_TOKEN_LENGTH)
       )
    {
        return RFAIL_OVERRUN;
    }

    header->response_code = (rcoap_method_code_t ) *stream++;

    header->message_id = rutils_stream_to_word16(stream);
    stream += BYTES_PER_WORD16;

    max_stream -= MIN_HEADER_LENGTH_BYTES;

    if (max_stream < header->token_length)
    {
        return false;
    }

    rutils_memset(header->token, 0, RCOAP_MAX_TOKEN_LENGTH);
    rutils_memcpy(header->token, stream, header->token_length);
    stream += MIN_HEADER_LENGTH_BYTES;
    max_stream -= MIN_HEADER_LENGTH_BYTES;

    cummulative_delta = 0;
    uri_path_count = 0;

    // Step through options
    while ( (max_stream > 0) && (*stream != PAYLOAD_MARKER) )
    {
        non_extended_byte = *stream++;
        max_stream--;

        option_delta_non_extended = OPTION_DELTA_FROM_BYTE(non_extended_byte);
        option_length_non_extended = OPTION_LENGTH_FROM_BYTE(non_extended_byte);

        extended_count = 0;
        if (DELTA_SPECIAL_CONSTRUCT_UINT8 == option_delta_non_extended)
        {
            extended_count++;
        }
        else if (DELTA_SPECIAL_CONSTRUCT_UINT16 == option_delta_non_extended)
        {
            extended_count++;
        }
        if (DELTA_SPECIAL_CONSTRUCT_UINT8 == option_length_non_extended)
        {
            extended_count++;
        }
        else if (DELTA_SPECIAL_CONSTRUCT_UINT16 == option_length_non_extended)
        {
            extended_count++;
        }

        if (max_stream < extended_count)
        {
            return RFAIL_OVERRUN;
        }

        option_delta = option_delta_non_extended;
        if (DELTA_SPECIAL_CONSTRUCT_UINT8 == option_delta_non_extended)
        {
            option_delta = DELTA_ADJUSTMENT_UINT8 + *stream++;
        }
        else if (DELTA_SPECIAL_CONSTRUCT_UINT16 == option_delta_non_extended)
        {
            option_delta = DELTA_ADJUSTMENT_UINT16 +
                                 rutils_stream_to_word16(stream);
            stream += BYTES_PER_WORD16;
        }

        option_id = option_delta + cummulative_delta;

        option_length = option_length_non_extended;
        if (DELTA_SPECIAL_CONSTRUCT_UINT8 == option_length_non_extended)
        {
            option_length = DELTA_ADJUSTMENT_UINT8 + *stream++;
        }
        else if (DELTA_SPECIAL_CONSTRUCT_UINT16 == option_length_non_extended)
        {
            option_length = DELTA_ADJUSTMENT_UINT16 +
                                 rutils_stream_to_word16(stream);
            stream += BYTES_PER_WORD16;
        }

        max_stream -= extended_count;

        if (max_stream < option_length)
        {
            return RFAIL_OVERRUN;
        }

        //  We're only concerned with URI Path
        if (RCOAP_OPT_URI_PATH == option_id)
        {
            // Make sure we haven't exceeded the fragment count
            if (RCOAP_MAX_URI_PATH == uri_path_count)
            {
                return RFAIL_UNSUPPORTED;
            }

            // Save
            uri_path_fragment[uri_path_count] = stream;
            uri_path_fragment_length[uri_path_count] = option_length;
            uri_path_count++;
         }

        stream += option_length;
        max_stream -= option_length;

        cummulative_delta += option_delta;
    }

    // Are there bytes left over? Must be the payload
    if (max_stream > 0)
    {
        if (PAYLOAD_MARKER != stream)
        {
            return RFAIL_ERROR;
        }
    }

    uri_path_length = 0;

    // Concatenate uri path from slash-delimited fragments
    if (uri_path_count > 0)
    {
        // Loop through all fragments
        for (i = 0; i < uri_path_count; i++)
        {
            unsigned fragment_length = uri_path_length[i];

            // Enough room for leading slash + uri path fragment?
            if (fragment_length + 1 > max_uri_path)
            {
                return RFAIL_OVERRUN;
            }

            // pre-pend slash before each fragment
            uri_path[uri_path_length++] = '/';

            // copy fragment code over
            rutils_strcpy(&uri_path[uri_path_length],
                          uri_path_fragment[i], 
                          fragment_length);
            uri_path_length += fragment_length;
            max_uri_path -= fragment_length;
        }
    }

    // Sanity check: enough room for null termination char?
    if (0 == max_uri_path)
    {
        return RFAIL_OVERRUN;
    }

    // Null-terminate 'uri_path'
    uri_path[uri_path_length++] = '\0';

    return (start_max_stream - max_stream);
}