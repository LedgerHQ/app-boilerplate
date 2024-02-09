
/**
 * @file nbgl_fonts.c
 * Implementation of fonts array
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "nbgl_fonts.h"

#define PAGING_FORMAT_NB 1

// We implement a light mecanism in order to be able to retrieve the width of
// nano S characters, in the two possible fonts:
// - BAGL_FONT_OPEN_SANS_EXTRABOLD_11px,
// - BAGL_FONT_OPEN_SANS_REGULAR_11px.
#define NANOS_FIRST_CHAR 0x20
#define NANOS_LAST_CHAR  0x7F

// OPEN_SANS_REGULAR_11PX << 4 | OPEN_SANS_EXTRABOLD_11PX
const char nanos_characters_width[96] = {
    3 << 4 | 3,   /* code 0020 */
    3 << 4 | 3,   /* code 0021 */
    4 << 4 | 6,   /* code 0022 */
    7 << 4 | 7,   /* code 0023 */
    6 << 4 | 6,   /* code 0024 */
    9 << 4 | 10,  /* code 0025 */
    8 << 4 | 9,   /* code 0026 */
    2 << 4 | 3,   /* code 0027 */
    3 << 4 | 4,   /* code 0028 */
    3 << 4 | 4,   /* code 0029 */
    6 << 4 | 6,   /* code 002A */
    6 << 4 | 6,   /* code 002B */
    3 << 4 | 3,   /* code 002C */
    4 << 4 | 4,   /* code 002D */
    3 << 4 | 3,   /* code 002E */
    4 << 4 | 5,   /* code 002F */
    6 << 4 | 8,   /* code 0030 */
    6 << 4 | 6,   /* code 0031 */
    6 << 4 | 7,   /* code 0032 */
    6 << 4 | 7,   /* code 0033 */
    8 << 4 | 8,   /* code 0034 */
    6 << 4 | 6,   /* code 0035 */
    6 << 4 | 8,   /* code 0036 */
    6 << 4 | 7,   /* code 0037 */
    6 << 4 | 8,   /* code 0038 */
    6 << 4 | 8,   /* code 0039 */
    3 << 4 | 3,   /* code 003A */
    3 << 4 | 3,   /* code 003B */
    6 << 4 | 5,   /* code 003C */
    6 << 4 | 6,   /* code 003D */
    6 << 4 | 5,   /* code 003E */
    5 << 4 | 6,   /* code 003F */
    10 << 4 | 10, /* code 0040 */
    7 << 4 | 8,   /* code 0041 */
    7 << 4 | 7,   /* code 0042 */
    7 << 4 | 7,   /* code 0043 */
    8 << 4 | 8,   /* code 0044 */
    6 << 4 | 6,   /* code 0045 */
    6 << 4 | 6,   /* code 0046 */
    8 << 4 | 8,   /* code 0047 */
    8 << 4 | 8,   /* code 0048 */
    3 << 4 | 4,   /* code 0049 */
    4 << 4 | 5,   /* code 004A */
    7 << 4 | 8,   /* code 004B */
    6 << 4 | 6,   /* code 004C */
    10 << 4 | 11, /* code 004D */
    8 << 4 | 9,   /* code 004E */
    9 << 4 | 9,   /* code 004F */
    7 << 4 | 7,   /* code 0050 */
    9 << 4 | 9,   /* code 0051 */
    7 << 4 | 8,   /* code 0052 */
    6 << 4 | 6,   /* code 0053 */
    7 << 4 | 6,   /* code 0054 */
    8 << 4 | 8,   /* code 0055 */
    7 << 4 | 6,   /* code 0056 */
    10 << 4 | 11, /* code 0057 */
    6 << 4 | 8,   /* code 0058 */
    6 << 4 | 7,   /* code 0059 */
    6 << 4 | 7,   /* code 005A */
    4 << 4 | 5,   /* code 005B */
    4 << 4 | 5,   /* code 005C */
    4 << 4 | 5,   /* code 005D */
    6 << 4 | 7,   /* code 005E */
    5 << 4 | 6,   /* code 005F */
    6 << 4 | 7,   /* code 0060 */
    6 << 4 | 7,   /* code 0061 */
    7 << 4 | 7,   /* code 0062 */
    5 << 4 | 6,   /* code 0063 */
    7 << 4 | 7,   /* code 0064 */
    6 << 4 | 7,   /* code 0065 */
    5 << 4 | 6,   /* code 0066 */
    6 << 4 | 7,   /* code 0067 */
    7 << 4 | 7,   /* code 0068 */
    3 << 4 | 4,   /* code 0069 */
    4 << 4 | 5,   /* code 006A */
    6 << 4 | 7,   /* code 006B */
    3 << 4 | 4,   /* code 006C */
    10 << 4 | 10, /* code 006D */
    7 << 4 | 7,   /* code 006E */
    7 << 4 | 7,   /* code 006F */
    7 << 4 | 7,   /* code 0070 */
    7 << 4 | 7,   /* code 0071 */
    4 << 4 | 5,   /* code 0072 */
    5 << 4 | 6,   /* code 0073 */
    4 << 4 | 5,   /* code 0074 */
    7 << 4 | 7,   /* code 0075 */
    6 << 4 | 7,   /* code 0076 */
    9 << 4 | 10,  /* code 0077 */
    6 << 4 | 7,   /* code 0078 */
    6 << 4 | 7,   /* code 0079 */
    5 << 4 | 6,   /* code 007A */
    4 << 4 | 5,   /* code 007B */
    6 << 4 | 6,   /* code 007C */
    4 << 4 | 5,   /* code 007D */
    6 << 4 | 6,   /* code 007E */
    7 << 4 | 6,   /* code 007F */
};

// This function is used to retrieve the length of a string (expressed in bytes) delimited with a
// boundary width (expressed in pixels).
uint8_t se_get_cropped_length(const char *text,
                              uint8_t     text_length,
                              uint32_t    width_limit_in_pixels,
                              uint8_t     text_format)
{
    char     current_char;
    uint8_t  length;
    uint32_t current_width_in_pixels = 0;

    for (length = 0; length < text_length; length++) {
        current_char = text[length];

        if ((text_format & PAGING_FORMAT_NB) == PAGING_FORMAT_NB) {
            // Bold.
            current_width_in_pixels
                += nanos_characters_width[current_char - NANOS_FIRST_CHAR] & 0x0F;
        }
        else {
            // Regular.
            current_width_in_pixels
                += (nanos_characters_width[current_char - NANOS_FIRST_CHAR] >> 0x04) & 0x0F;
        }

        // We stop the processing when we reached the limit.
        if (current_width_in_pixels > width_limit_in_pixels) {
            break;
        }
    }

    return length;
}

// This function is used to retrieve the width of a line of text.
static uint32_t se_compute_line_width_light(const char *text,
                                            uint8_t     text_length,
                                            uint8_t     text_format)
{
    char     current_char;
    uint32_t line_width = 0;

    // We parse the characters of the input text on all the input length.
    while (text_length--) {
        current_char = *text;

        if (current_char < NANOS_FIRST_CHAR || current_char > NANOS_LAST_CHAR) {
            if (current_char == '\n' || current_char == '\r') {
                break;
            }
        }
        else {
            // We retrieve the character width, and the paging format indicates whether we are
            // processing bold characters or not.
            if ((text_format & PAGING_FORMAT_NB) == PAGING_FORMAT_NB) {
                // Bold.
                line_width += nanos_characters_width[current_char - NANOS_FIRST_CHAR] & 0x0F;
            }
            else {
                // Regular.
                line_width
                    += (nanos_characters_width[current_char - NANOS_FIRST_CHAR] >> 0x04) & 0x0F;
            }
        }
        text++;
    }
    return line_width;
}

/**
 * @brief compute the len of the given text (in bytes) fitting in the given maximum nb lines, with
 * the given maximum width
 *
 * @param fontId font ID
 * @param text input UTF-8 string, possibly multi-line
 * @param maxWidth maximum width in bytes, if text is greater than that the parsing is escaped
 * @param maxNbLines maximum number of lines, if text is greater than that the parsing is escaped
 * @param len (output) consumed bytes in text fitting in maxWidth
 * @param wrapping if true, lines are split on separators like spaces, \n...
 *
 * @return true if maxNbLines is reached, false otherwise
 *
 */
bool nbgl_getTextMaxLenInNbLines(nbgl_font_id_e fontId,
                                 const char    *text,
                                 uint16_t       maxWidth,
                                 uint16_t       maxNbLines,
                                 uint16_t      *len,
                                 bool           wrapping)
{
    uint16_t           textLen            = strlen(text);
    const char        *origText           = text;

    while ((textLen) && (maxNbLines > 0)) {

        uint8_t line_len = se_get_cropped_length(text, textLen, maxWidth, 0);

        text += line_len;
        textLen -= line_len;
        maxNbLines--;
    }
    *len = text - origText;
    return (maxNbLines == 0);
}

/**
 * @brief compute the number of lines of the given text fitting in the given maxWidth
 *
 * @param fontId font ID
 * @param text UTF-8 text to get the number of lines from
 * @param maxWidth maximum width in which the text must fit
 * @param wrapping if true, lines are split on separators like spaces, \n...
 * @return the number of lines in the given text
 */
uint16_t nbgl_getTextNbLinesInWidth(nbgl_font_id_e fontId,
                                    const char    *text,
                                    uint16_t       maxWidth,
                                    bool           wrapping)
{
    uint16_t           textLen            = strlen(text);
    const char        *origText           = text;
    uint16_t nbLines = 0;

    while (textLen) {
        uint8_t  char_width;

        uint8_t line_len = se_get_cropped_length(text, textLen, maxWidth, 0);

        text += line_len;
        textLen -= line_len;
        nbLines++;
    }
    return nbLines;
}

/**
 * @brief compute the number of pages of nbLinesPerPage lines per page of the given text fitting in
 * the given maxWidth
 *
 * @param fontId font ID
 * @param text UTF-8 text to get the number of pages from
 * @param nbLinesPerPage number of lines in a page
 * @param maxWidth maximum width in which the text must fit
 * @return the number of pages in the given text
 */
uint8_t nbgl_getTextNbPagesInWidth(nbgl_font_id_e fontId,
                                   const char    *text,
                                   uint8_t        nbLinesPerPage,
                                   uint16_t       maxWidth)
{
    return (nbgl_getTextNbLinesInWidth(fontId, text, maxWidth, false) + nbLinesPerPage - 1 )/ nbLinesPerPage;
}

/**
 * @brief return the height of the given multiline text, with the given font.
 *
 * @param fontId font ID
 * @param text text to get the height from
 * @param maxWidth maximum width in which the text must fit
 * @param wrapping if true, lines are split on separators like spaces, \n...
 * @return the height in pixels
 */
uint16_t nbgl_getTextHeightInWidth(nbgl_font_id_e fontId,
                                   const char    *text,
                                   uint16_t       maxWidth,
                                   bool           wrapping)
{
    const nbgl_font_t *font = nbgl_getFont(fontId);
    return (nbgl_getTextNbLinesInWidth(fontId, text, maxWidth, wrapping) * font->line_height);
}
