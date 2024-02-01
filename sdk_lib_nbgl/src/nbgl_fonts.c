
/**
 * @file nbgl_fonts.c
 * Implementation of fonts array
 */

/*********************
 *      INCLUDES
 *********************/
#include <string.h>
#include "nbgl_debug.h"
#include "nbgl_draw.h"
#include "nbgl_fonts.h"
#include "os_helpers.h"
#include "os_pic.h"
#if defined(HAVE_LANGUAGE_PACK)
#include "ux_loc.h"
#endif  // defined(HAVE_LANGUAGE_PACK)

/*********************
 *      DEFINES
 *********************/
#define PIC_FONT(x)       ((nbgl_font_t const *) PIC(x))
#define BAGL_FONT_ID_MASK 0x0FFF

#define IS_WORD_DELIM(c) (c == ' ' || c == '\n' || c == '\b' || c == '\f' || c == '-' || c == '_')

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/
#ifdef HAVE_UNICODE_SUPPORT
static nbgl_unicode_ctx_t unicodeCtx = {0};
#endif  // HAVE_UNICODE_SUPPORT

/**********************
 *      VARIABLES
 **********************/

#if defined(HAVE_LANGUAGE_PACK)
extern const LANGUAGE_PACK *language_pack;
#endif  // defined(HAVE_LANGUAGE_PACK)

#if defined(BOLOS_OS_UPGRADER_APP)
#ifdef SCREEN_SIZE_WALLET
#ifdef TARGET_STAX
#include "nbgl_font_inter_regular_24.inc"
#include "nbgl_font_inter_semibold_24.inc"
#include "nbgl_font_inter_medium_32.inc"
#include "nbgl_font_inter_regular_24_1bpp.inc"
#include "nbgl_font_inter_semibold_24_1bpp.inc"
#include "nbgl_font_inter_medium_32_1bpp.inc"
#endif  // TARGET_STAX
#else   // SCREEN_SIZE_WALLET
#include "nbgl_font_open_sans_extrabold_11.inc"
#include "nbgl_font_open_sans_regular_11.inc"
#include "nbgl_font_open_sans_light_16.inc"
#endif  // SCREEN_SIZE_WALLET

const nbgl_font_t *const C_nbgl_fonts[] = {

#include "nbgl_font_rom_struct.inc"

};
const unsigned int C_nbgl_fonts_count = sizeof(C_nbgl_fonts) / sizeof(C_nbgl_fonts[0]);

#endif

#if (defined(HAVE_BOLOS) && !defined(BOLOS_OS_UPGRADER_APP))
#if !defined(HAVE_LANGUAGE_PACK)
const nbgl_font_unicode_t *const C_nbgl_fonts_unicode[] = {

#include "nbgl_font_unicode_rom_struct.inc"

};

// All Unicode fonts MUST have the same number of characters!
const unsigned int C_unicode_characters_count
    = (sizeof(charactersOPEN_SANS_REGULAR_11PX_UNICODE)
       / sizeof(charactersOPEN_SANS_REGULAR_11PX_UNICODE[0]));

#endif  //! defined(HAVE_LANGUAGE_PACK)
#endif  // HAVE_BOLOS

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief return the non-unicode font corresponding to the given font ID
 *
 * @param fontId font ID
 * @return the found font or NULL
 */
#if defined(BOLOS_OS_UPGRADER_APP)
const nbgl_font_t *nbgl_getFont(nbgl_font_id_e fontId)
{
    unsigned int i = C_nbgl_fonts_count;
    fontId &= BAGL_FONT_ID_MASK;

    while (i--) {
        // font id match this entry (non indexed array)
        if (PIC_FONT(C_nbgl_fonts[i])->font_id == fontId) {
            return PIC_FONT(C_nbgl_fonts[i]);
        }
    }

    // id not found
    return NULL;
}
#else
const nbgl_font_t *nbgl_getFont(nbgl_font_id_e fontId)
{
    return nbgl_font_getFont(fontId);
}
#endif  // BOLOS_OS_UPGRADER_APP
/**
 * @brief Get the coming unicode value on the given UTF-8 string. If the value is a simple ASCII
 * character, is_unicode is set to false.
 *
 * @param text (in/out) text to get character from. Updated after pop to the next UTF-8 char
 * @param textLen (in/out) remaining length in given text (before '\n' or '\0')
 * @param is_unicode (out) set to true if it's a real unicode (not ASCII)
 * @return unicode (or ascii-7) value of the found character
 */
uint32_t nbgl_popUnicodeChar(const uint8_t **text, uint16_t *textLen, bool *is_unicode)
{
    const uint8_t *txt      = *text;
    uint8_t        cur_char = *txt++;
    uint32_t       unicode;

    *is_unicode = true;
    // Handle UTF-8 decoding (RFC3629): (https://www.ietf.org/rfc/rfc3629.txt
    // Char. number range  |        UTF-8 octet sequence
    // (hexadecimal)    |              (binary)
    // --------------------+---------------------------------------------
    // 0000 0000-0000 007F | 0xxxxxxx
    // 0000 0080-0000 07FF | 110xxxxx 10xxxxxx
    // 0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
    // 0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

    // 4 bytes UTF-8, Unicode 0x1000 to 0x1FFFF
    if ((cur_char >= 0xF0) && (*textLen >= 4)) {
        unicode = (cur_char & 0x07) << 18;
        unicode |= (*txt++ & 0x3F) << 12;
        unicode |= (*txt++ & 0x3F) << 6;
        unicode |= (*txt++ & 0x3F);

        // 3 bytes, from 0x800 to 0xFFFF
    }
    else if ((cur_char >= 0xE0) && (*textLen >= 3)) {
        unicode = (cur_char & 0x0F) << 12;
        unicode |= (*txt++ & 0x3F) << 6;
        unicode |= (*txt++ & 0x3F);

        // 2 bytes UTF-8, Unicode 0x80 to 0x7FF
        // (0xC0 & 0xC1 are unused and can be used to store something else)
    }
    else if ((cur_char >= 0xC2) && (*textLen >= 2)) {
        unicode = (cur_char & 0x1F) << 6;
        unicode |= (*txt++ & 0x3F);
    }
    else {
        *is_unicode = false;
        unicode     = cur_char;
    }
    *textLen = *textLen - (txt - *text);
    *text    = txt;
    return unicode;
}

/**
 * @brief return the width in pixels of the given char (unicode or not).
 *
 * @param fontId font ID
 * @param unicode character
 * @param is_unicode if true, is unicode
 * @return the width in pixels of the char
 */
static uint8_t getCharWidth(const nbgl_font_t *font, uint32_t unicode, bool is_unicode)
{
    if (is_unicode) {
#ifdef HAVE_UNICODE_SUPPORT
        const nbgl_font_unicode_character_t *unicodeCharacter
            = nbgl_getUnicodeFontCharacter(unicode);
        if (!unicodeCharacter) {
            return 0;
        }
        return unicodeCharacter->width - font->char_kerning;
#else   // HAVE_UNICODE_SUPPORT
        return 0;
#endif  // HAVE_UNICODE_SUPPORT
    }
    else {
        const nbgl_font_character_t *character;  // non-unicode char
        if ((unicode < font->first_char) || (unicode > font->last_char)) {
            return 0;
        }
        character
            = (const nbgl_font_character_t *) PIC(&font->characters[unicode - font->first_char]);
        return character->width - font->char_kerning;
    }
}

/**
 * @brief return the max width in pixels of the given text until the first terminator (\n if
 * breakOnLineEnd, or \0) is encountered, or maxLen bytes have been parsed.
 *
 * @param fontId font ID
 * @param text text in UTF8
 * @param breakOnLineEnd if true, \n is considered as an end of string
 * @param maxLen max number of bytes to parse in text
 * @return the width in pixels of the text
 */
static uint16_t getTextWidth(nbgl_font_id_e fontId,
                             const char    *text,
                             bool           breakOnLineEnd,
                             uint16_t       maxLen)
{
    uint16_t           line_width = 0;
    uint16_t           max_width  = 0;
    const nbgl_font_t *font       = nbgl_getFont(fontId);
    uint16_t           textLen    = MIN(strlen(text), maxLen);

#ifdef HAVE_UNICODE_SUPPORT
    nbgl_getUnicodeFont(fontId);
#endif  // HAVE_UNICODE_SUPPORT
    // end loop text len is NULL (max reached)
    while (textLen) {
        uint8_t  char_width;
        uint32_t unicode;
        bool     is_unicode;

        unicode = nbgl_popUnicodeChar((const uint8_t **) &text, &textLen, &is_unicode);
        if (unicode == '\n') {
            if (breakOnLineEnd) {
                break;
            }
            // reset line width for next line
            line_width = 0;
            continue;
        }
        // if \b, switch fontId
        else if (unicode == '\b') {
            if (fontId == BAGL_FONT_OPEN_SANS_REGULAR_11px_1bpp) {  // switch to bold
                fontId = BAGL_FONT_OPEN_SANS_EXTRABOLD_11px_1bpp;
#ifdef HAVE_UNICODE_SUPPORT
                nbgl_getUnicodeFont(fontId);
#endif  // HAVE_UNICODE_SUPPORT
                font = nbgl_getFont(fontId);
            }
            else if (fontId == BAGL_FONT_OPEN_SANS_EXTRABOLD_11px_1bpp) {  // switch to regular
                fontId = BAGL_FONT_OPEN_SANS_REGULAR_11px_1bpp;
#ifdef HAVE_UNICODE_SUPPORT
                nbgl_getUnicodeFont(fontId);
#endif  // HAVE_UNICODE_SUPPORT
                font = nbgl_getFont(fontId);
            }
            continue;
        }
        char_width = getCharWidth(font, unicode, is_unicode);

        line_width += char_width;
        // memorize max line width if greater than current
        if (line_width > max_width) {
            max_width = line_width;
        }
    }
    return max_width;
}

/**
 * @brief return the max width in pixels of the given text until the first \n or \0 is encountered
 *
 * @param fontId font ID
 * @param text text in UTF8
 * @return the width in pixels of the text
 */
uint16_t nbgl_getSingleLineTextWidth(nbgl_font_id_e fontId, const char *text)
{
    return getTextWidth(fontId, text, true, 0xFFFF);
}

/**
 * @brief return the max width in pixels of the given text until the first \n or \0 is encountered,
 * or maxLen characters have been parsed.
 *
 * @param fontId font ID
 * @param text text in UTF8
 * @param maxLen max number of bytes to parse
 * @return the width in pixels of the text
 */
uint16_t nbgl_getSingleLineTextWidthInLen(nbgl_font_id_e fontId, const char *text, uint16_t maxLen)
{
    return getTextWidth(fontId, text, true, maxLen);
}

/**
 * @brief return the max width in pixels of the given text (can be multiline)
 *
 * @param fontId font ID
 * @param text text in UTF8
 * @return the width in pixels of the text
 */
uint16_t nbgl_getTextWidth(nbgl_font_id_e fontId, const char *text)
{
    return getTextWidth(fontId, text, false, 0xFFFF);
}

/**
 * @brief return the width in pixels of the given UTF-8 character
 *
 * @param fontId font ID
 * @param text UTF-8 character
 * @return the width in pixels of the character
 */
uint8_t nbgl_getCharWidth(nbgl_font_id_e fontId, const char *text)
{
    const nbgl_font_t *font = nbgl_getFont(fontId);
    uint32_t           unicode;
    bool               is_unicode;
    uint16_t           textLen = 4;  // max len for a char

#ifdef HAVE_UNICODE_SUPPORT
    nbgl_getUnicodeFont(fontId);
#endif  // HAVE_UNICODE_SUPPORT

    unicode = nbgl_popUnicodeChar((const uint8_t **) &text, &textLen, &is_unicode);

    return getCharWidth(font, unicode, is_unicode);
}

/**
 * @brief return the height in pixels of the font with the given font ID
 *
 * @param fontId font ID
 * @return the height in pixels
 */
uint8_t nbgl_getFontHeight(nbgl_font_id_e fontId)
{
    const nbgl_font_t *font = nbgl_getFont(fontId);
    return font->height;
}

/**
 * @brief return the height in pixels of the line of font with the given font ID
 *
 * @param fontId font ID
 * @return the height in pixels
 */
uint8_t nbgl_getFontLineHeight(nbgl_font_id_e fontId)
{
    const nbgl_font_t *font = nbgl_getFont(fontId);
    return font->line_height;
}

/**
 * @brief return the number of lines in the given text, according to the found '\n's
 *
 * @param text text to get the number of lines from
 * @return the number of lines in the given text
 */
uint16_t nbgl_getTextNbLines(const char *text)
{
    uint16_t nbLines = 1;
    while (*text) {
        if (*text == '\n') {
            nbLines++;
        }
        text++;
    }
    return nbLines;
}

/**
 * @brief return the height of the given multiline text, with the given font.
 *
 * @param fontId font ID
 * @param text text to get the height from
 * @return the height in pixels
 */
uint16_t nbgl_getTextHeight(nbgl_font_id_e fontId, const char *text)
{
    const nbgl_font_t *font = nbgl_getFont(fontId);
    return (nbgl_getTextNbLines(text) * font->line_height);
}

/**
 * @brief return the number of bytes of the given text, excluding final '\n' or '\0'
 * @note '\n' and '\0' are considered as end of string
 *
 * @param text text to get the number of bytes from
 * @return the number of bytes in the given text
 */
uint16_t nbgl_getTextLength(const char *text)
{
    const char *origText = text;
    while (*text) {
        if (*text == '\f') {
            break;
        }
        else if (*text == '\n') {
            break;
        }
        text++;
    }
    return text - origText;
}

/**
 * @brief compute the max width of the first line of the given text fitting in maxWidth
 *
 * @param fontId font ID
 * @param text input UTF-8 string, possibly multi-line (but only first line, before \n, is used)
 * @param maxWidth maximum width in bytes, if text is greater than that the parsing is escaped
 * @param len (output) consumed bytes in text fitting in maxWidth
 * @param width (output) set to maximum width in pixels in text fitting in maxWidth
 * @param wrapping if true, lines are split on separators like spaces, \n...
 *
 * @return true if maxWidth is reached, false otherwise
 *
 */
void nbgl_getTextMaxLenAndWidth(nbgl_font_id_e fontId,
                                const char    *text,
                                uint16_t       maxWidth,
                                uint16_t      *len,
                                uint16_t      *width,
                                bool           wrapping)
{
    const nbgl_font_t *font               = nbgl_getFont(fontId);
    uint16_t           textLen            = nbgl_getTextLength(text);
    uint32_t           lenAtLastDelimiter = 0, widthAtlastDelimiter = 0;

#ifdef HAVE_UNICODE_SUPPORT
    nbgl_getUnicodeFont(fontId);
#endif  // HAVE_UNICODE_SUPPORT

    *width = 0;
    *len   = 0;
    while (textLen) {
        uint8_t  char_width;
        uint32_t unicode;
        bool     is_unicode;
        uint16_t curTextLen = textLen;

        unicode = nbgl_popUnicodeChar((const uint8_t **) &text, &textLen, &is_unicode);
        // if \f, exit loop
        if (unicode == '\f') {
            *len += curTextLen - textLen;
            break;
        }
        // if \n, exit
        else if (unicode == '\n') {
            *len += curTextLen - textLen;
            return;
        }
        // if \b, switch fontId
        else if (unicode == '\b') {
            if (fontId == BAGL_FONT_OPEN_SANS_REGULAR_11px_1bpp) {  // switch to bold
                fontId = BAGL_FONT_OPEN_SANS_EXTRABOLD_11px_1bpp;
#ifdef HAVE_UNICODE_SUPPORT
                nbgl_getUnicodeFont(fontId);
#endif  // HAVE_UNICODE_SUPPORT
                font = nbgl_getFont(fontId);
            }
            else if (fontId == BAGL_FONT_OPEN_SANS_EXTRABOLD_11px_1bpp) {  // switch to regular
                fontId = BAGL_FONT_OPEN_SANS_REGULAR_11px_1bpp;
#ifdef HAVE_UNICODE_SUPPORT
                nbgl_getUnicodeFont(fontId);
#endif  // HAVE_UNICODE_SUPPORT
                font = nbgl_getFont(fontId);
            }
            *len += curTextLen - textLen;
            continue;
        }

        char_width = getCharWidth(font, unicode, is_unicode);
        if (char_width == 0) {
            continue;
        }

        // memorize cursors at last found sepator, when wrapping
        if ((wrapping == true) && IS_WORD_DELIM(unicode)) {
            lenAtLastDelimiter   = *len;
            widthAtlastDelimiter = *width;
        }
        if ((*width + char_width) > maxWidth) {
            if ((wrapping == true) && (widthAtlastDelimiter > 0)) {
                *len   = lenAtLastDelimiter + 1;
                *width = widthAtlastDelimiter;
            }
            return;
        }
        *len += curTextLen - textLen;
        *width = *width + char_width;
    }
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
    const nbgl_font_t *font               = nbgl_getFont(fontId);
    uint16_t           textLen            = strlen(text);
    uint16_t           width              = 0;
    const char        *lastDelimiter      = NULL;
    uint32_t           lenAtLastDelimiter = 0;
    const char        *origText           = text;
    const char        *previousText;

#ifdef HAVE_UNICODE_SUPPORT
    nbgl_getUnicodeFont(fontId);
#endif  // HAVE_UNICODE_SUPPORT

    while ((textLen) && (maxNbLines > 0)) {
        uint8_t  char_width;
        uint32_t unicode;
        bool     is_unicode;

        previousText = text;
        unicode      = nbgl_popUnicodeChar((const uint8_t **) &text, &textLen, &is_unicode);
        // memorize cursors at last found delimiter
        if ((wrapping == true) && (IS_WORD_DELIM(unicode))) {
            lastDelimiter      = text;
            lenAtLastDelimiter = textLen;
        }

        // if \n, reset width
        if (unicode == '\n') {
            maxNbLines--;
            width = 0;
            continue;
        }
        // if \f, exit
        else if (unicode == '\f') {
            maxNbLines = 0;
            continue;
        }
        // if \b, switch fontId
        else if (unicode == '\b') {
            if (fontId == BAGL_FONT_OPEN_SANS_REGULAR_11px_1bpp) {  // switch to bold
                fontId = BAGL_FONT_OPEN_SANS_EXTRABOLD_11px_1bpp;
#ifdef HAVE_UNICODE_SUPPORT
                nbgl_getUnicodeFont(fontId);
#endif  // HAVE_UNICODE_SUPPORT
                font = nbgl_getFont(fontId);
            }
            else if (fontId == BAGL_FONT_OPEN_SANS_EXTRABOLD_11px_1bpp) {  // switch to regular
                fontId = BAGL_FONT_OPEN_SANS_REGULAR_11px_1bpp;
#ifdef HAVE_UNICODE_SUPPORT
                nbgl_getUnicodeFont(fontId);
#endif  // HAVE_UNICODE_SUPPORT
                font = nbgl_getFont(fontId);
            }
            continue;
        }

        char_width = getCharWidth(font, unicode, is_unicode);
        if (char_width == 0) {
            continue;
        }

        if ((width + char_width) > maxWidth) {
            if ((wrapping == true) && (lastDelimiter != NULL)) {
                text          = lastDelimiter;
                lastDelimiter = NULL;
                textLen       = lenAtLastDelimiter;
            }
            else {
                textLen += text - previousText;
                text = previousText;
            }
            width = 0;
            maxNbLines--;
            if (maxNbLines == 0) {
                break;
            }
            continue;
        }
        width += char_width;
    }
    *len = text - origText;
    return (maxNbLines == 0);
}

/**
 * @brief compute the len and width of the given text fitting in the maxWidth, starting from end of
 * text
 * @note works only with ASCII string
 *
 * @param fontId font ID
 * @param text input ascii string, possibly multi-line (but only first line is handled)
 * @param maxWidth maximum width in bytes, if text is greater than that the parsing is escaped
 * @param len (output) consumed bytes in text fitting in maxWidth
 * @param width (output) set to maximum width in pixels in text fitting in maxWidth
 *
 * @return true if maxWidth is reached, false otherwise
 *
 */
bool nbgl_getTextMaxLenAndWidthFromEnd(nbgl_font_id_e fontId,
                                       const char    *text,
                                       uint16_t       maxWidth,
                                       uint16_t      *len,
                                       uint16_t      *width)
{
    const nbgl_font_t *font    = nbgl_getFont(fontId);
    uint16_t           textLen = nbgl_getTextLength(text);

    *width = 0;
    *len   = 0;
    while (textLen) {
        const nbgl_font_character_t *character;
        uint8_t                      char_width;
        char                         cur_char;

        textLen--;
        cur_char = text[textLen];
        // if \n, exit
        if (cur_char == '\n') {
            *len = *len + 1;
            continue;
        }

        // skip not printable char
        if ((cur_char < font->first_char) || (cur_char > font->last_char)) {
            continue;
        }
        character
            = (const nbgl_font_character_t *) PIC(&font->characters[cur_char - font->first_char]);
        char_width = character->width;

        if ((*width + char_width) > maxWidth) {
            return true;
        }
        *len   = *len + 1;
        *width = *width + char_width;
    }
    return false;
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
    const nbgl_font_t *font               = nbgl_getFont(fontId);
    uint16_t           width              = 0;
    uint16_t           nbLines            = 1;
    uint16_t           textLen            = strlen(text);
    const char        *lastDelimiter      = NULL;
    uint32_t           lenAtLastDelimiter = 0;
    const char        *prevText           = NULL;

#ifdef HAVE_UNICODE_SUPPORT
    nbgl_getUnicodeFont(fontId);
#endif  // HAVE_UNICODE_SUPPORT
    // end loop when a '\0' is uncountered
    while (textLen) {
        uint8_t  char_width;
        uint32_t unicode;
        bool     is_unicode;

        // memorize the last char
        prevText = text;
        unicode  = nbgl_popUnicodeChar((const uint8_t **) &text, &textLen, &is_unicode);

        // memorize cursors at last found space
        if ((wrapping == true) && (IS_WORD_DELIM(unicode))) {
            lastDelimiter      = prevText;
            lenAtLastDelimiter = textLen;
        }
        // if \f, exit loop
        if (unicode == '\f') {
            break;
        }
        // if \n, increment the number of lines
        else if (unicode == '\n') {
            nbLines++;
            width         = 0;
            lastDelimiter = NULL;
            continue;
        }
        // if \b, switch fontId
        else if (unicode == '\b') {
            if (fontId == BAGL_FONT_OPEN_SANS_REGULAR_11px_1bpp) {  // switch to bold
                fontId = BAGL_FONT_OPEN_SANS_EXTRABOLD_11px_1bpp;
#ifdef HAVE_UNICODE_SUPPORT
                nbgl_getUnicodeFont(fontId);
#endif  // HAVE_UNICODE_SUPPORT
                font = nbgl_getFont(fontId);
            }
            else if (fontId == BAGL_FONT_OPEN_SANS_EXTRABOLD_11px_1bpp) {  // switch to regular
                fontId = BAGL_FONT_OPEN_SANS_REGULAR_11px_1bpp;
#ifdef HAVE_UNICODE_SUPPORT
                nbgl_getUnicodeFont(fontId);
#endif  // HAVE_UNICODE_SUPPORT
                font = nbgl_getFont(fontId);
            }
            continue;
        }

        char_width = getCharWidth(font, unicode, is_unicode);
        if (char_width == 0) {
            continue;
        }

        // if about to reach max len, increment the number of lines
        if ((width + char_width) > maxWidth) {
            if ((wrapping == true) && (lastDelimiter != NULL)) {
                text          = lastDelimiter + 1;
                lastDelimiter = NULL;
                textLen       = lenAtLastDelimiter;
                width         = 0;
            }
            else {
                width = char_width;
            }
            nbLines++;
        }
        else {
            width += char_width;
        }
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
    const nbgl_font_t *font               = nbgl_getFont(fontId);
    uint16_t           width              = 0;
    uint16_t           nbLines            = 0;
    uint8_t            nbPages            = 1;
    uint16_t           textLen            = strlen(text);
    const char        *lastDelimiter      = NULL;
    uint32_t           lenAtLastDelimiter = 0;
    const char        *prevText           = NULL;

#ifdef HAVE_UNICODE_SUPPORT
    nbgl_getUnicodeFont(fontId);
#endif  // HAVE_UNICODE_SUPPORT
    // end loop when a '\0' is uncountered
    while (textLen) {
        uint8_t  char_width;
        uint32_t unicode;
        bool     is_unicode;

        // memorize the last char
        prevText = text;
        unicode  = nbgl_popUnicodeChar((const uint8_t **) &text, &textLen, &is_unicode);

        // memorize cursors at last found space
        if (IS_WORD_DELIM(unicode)) {
            lastDelimiter      = prevText;
            lenAtLastDelimiter = textLen;
        }
        // if \f, updates page number
        if (unicode == '\f') {
            nbPages++;
            nbLines = 0;
            width   = 0;
            continue;
        }
        // if \n, increment the number of lines
        else if (unicode == '\n') {
            nbLines++;
            if (nbLines == nbLinesPerPage) {
                nbPages++;
                nbLines = 0;
            }

            width         = 0;
            lastDelimiter = NULL;
            continue;
        }
        // if \b, switch fontId
        else if (unicode == '\b') {
            if (fontId == BAGL_FONT_OPEN_SANS_REGULAR_11px_1bpp) {  // switch to bold
                fontId = BAGL_FONT_OPEN_SANS_EXTRABOLD_11px_1bpp;
#ifdef HAVE_UNICODE_SUPPORT
                nbgl_getUnicodeFont(fontId);
#endif  // HAVE_UNICODE_SUPPORT
                font = nbgl_getFont(fontId);
            }
            else if (fontId == BAGL_FONT_OPEN_SANS_EXTRABOLD_11px_1bpp) {  // switch to regular
                fontId = BAGL_FONT_OPEN_SANS_REGULAR_11px_1bpp;
#ifdef HAVE_UNICODE_SUPPORT
                nbgl_getUnicodeFont(fontId);
#endif  // HAVE_UNICODE_SUPPORT
                font = nbgl_getFont(fontId);
            }
            continue;
        }

        char_width = getCharWidth(font, unicode, is_unicode);
        if (char_width == 0) {
            continue;
        }

        // if about to reach max len, increment the number of lines/pages
        if ((width + char_width) > maxWidth) {
            if (lastDelimiter != NULL) {
                text          = lastDelimiter + 1;
                lastDelimiter = NULL;
                textLen       = lenAtLastDelimiter;
                width         = 0;
            }
            else {
                width = char_width;
            }
            nbLines++;
            if (nbLines == nbLinesPerPage) {
                nbPages++;
                nbLines = 0;
            }
        }
        else {
            width += char_width;
        }
    }
    return nbPages;
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

/**
 * @brief Modifies the given text to wrap it on the given max width (in pixels), in the given
 * nbLines If possible,
 *
 * @param fontId font ID
 * @param text (input/output) UTF-8 string, possibly multi-line
 * @param maxWidth maximum width in pixels
 * @param nbLines (input) If the text doesn't fit in this number of lines, the last chars will be
 * replaced by ...
 *
 */
void nbgl_textWrapOnNbLines(nbgl_font_id_e fontId, char *text, uint16_t maxWidth, uint8_t nbLines)
{
    const nbgl_font_t *font               = nbgl_getFont(fontId);
    uint16_t           textLen            = nbgl_getTextLength(text);
    uint16_t           width              = 0;
    uint8_t            currentNbLines     = 1;
    char              *lastDelimiter      = NULL;
    uint32_t           lenAtLastDelimiter = 0;
    char              *prevText           = NULL;

#ifdef HAVE_UNICODE_SUPPORT
    nbgl_getUnicodeFont(fontId);
#endif

    while (*text) {
        uint8_t  char_width;
        uint32_t unicode;
        bool     is_unicode;
        char    *prevPrevText;

        // memorize the two last chars
        prevPrevText = prevText;
        prevText     = text;
        unicode      = nbgl_popUnicodeChar((const uint8_t **) &text, &textLen, &is_unicode);
        // if \n, reset width
        if (unicode == '\n') {
            width = 0;
            currentNbLines++;
            lastDelimiter = NULL;
            continue;
        }

        char_width = getCharWidth(font, unicode, is_unicode);
        if (char_width == 0) {
            continue;
        }

        // memorize cursors at last found space
        if (IS_WORD_DELIM(unicode)) {
            lastDelimiter      = prevText;
            lenAtLastDelimiter = textLen + 1;
        }
        // if the width is about to overpass maxWidth, do something
        if ((width + char_width) > maxWidth) {
            // if the max number of lines has not been reached, try to wrap on last space
            // encountered
            if (currentNbLines < nbLines) {
                currentNbLines++;
                // replace last found space by a \n
                if (lastDelimiter != NULL) {
                    *lastDelimiter++ = '\n';
                    text             = lastDelimiter;
                    lastDelimiter    = NULL;
                    textLen          = lenAtLastDelimiter;
                }
                else {
                    textLen += text - prevText;
                    text = prevText;
                }
                // reset width for next line
                width = 0;
            }
            else {
                // replace the 2 last chars by '...' (should be same width)
                if (prevPrevText != NULL) {
                    *prevPrevText++ = '.';
                    *prevPrevText++ = '.';
                    *prevPrevText++ = '.';
                    *prevPrevText   = '\0';
                }
                return;
            }
        }
        width += char_width;
    }
}

#ifdef HAVE_UNICODE_SUPPORT
/**
 * @brief Get the font entry for the given font id (sparse font array support)
 *
 * @param font_id font ID (from @ref nbgl_font_id_e)

 * @return the found unicode context structure or NULL if not found
 */
nbgl_unicode_ctx_t *nbgl_getUnicodeFont(nbgl_font_id_e fontId)
{
    if ((unicodeCtx.font != NULL) && (unicodeCtx.font->font_id == fontId)) {
        return &unicodeCtx;
    }
#if defined(HAVE_LANGUAGE_PACK)
    // Be sure we need to change font
    const uint8_t             *ptr  = (const uint8_t *) language_pack;
    const nbgl_font_unicode_t *font = (const void *) (ptr + language_pack->fonts_offset);
    unicodeCtx.characters
        = (const nbgl_font_unicode_character_t *) (ptr + language_pack->characters_offset);
    unicodeCtx.bitmap = (const uint8_t *) (ptr + language_pack->bitmaps_offset);

    for (uint32_t i = 0; i < language_pack->nb_fonts; i++) {
        if (font->font_id == fontId) {
            // Update all other global variables
            unicodeCtx.font = font;
            return &unicodeCtx;
        }
        // Compute next Bitmap offset
        unicodeCtx.bitmap += font->bitmap_len;

        // Update all pointers for next font
        unicodeCtx.characters += language_pack->nb_characters;
        font++;
    }
#endif  // defined(HAVE_LANGUAGE_PACK)
    // id not found
    return NULL;
}

/**
 * @brief Get the unicode character object matching the given unicode (a unicode character is
 encoded on max of 3 chars)
 * in the current language
 *
 * @param unicode the unicode value of the character

 * @return the found character or NULL if not found
 */
const nbgl_font_unicode_character_t *nbgl_getUnicodeFontCharacter(uint32_t unicode)
{
#if defined(HAVE_LANGUAGE_PACK)
    const nbgl_font_unicode_character_t *characters
        = (const nbgl_font_unicode_character_t *) PIC(unicodeCtx.characters);
    uint32_t n = language_pack->nb_characters;
    if (characters == NULL) {
        return NULL;
    }
    // For the moment, let just parse the full array, but at the end let use
    // binary search as data are sorted by unicode value !
    for (unsigned i = 0; i < n - 1; i++, characters++) {
        if (characters->char_unicode == unicode) {
            // Compute & store the number of bytes used to display this character
            unicodeCtx.unicode_character_byte_count
                = (characters + 1)->bitmap_offset - characters->bitmap_offset;
            return characters;
        }
    }
    // By default, let's use the last Unicode character, which should be the
    // 0x00FFFD one, used to replace unrecognized or unrepresentable character.

    // Compute & store the number of bytes used to display this character
    unicodeCtx.unicode_character_byte_count
        = unicodeCtx.font->bitmap_len - characters->bitmap_offset;
    return characters;
#else   // defined(HAVE_LANGUAGE_PACK)
    UNUSED(unicode);
    // id not found
    return NULL;
#endif  // defined(HAVE_LANGUAGE_PACK)
}

/**
 * @brief Get the bitmap byte count of the latest used unicode character.
 * (the one returned by nbgl_getUnicodeFontCharacter)
 *
 * @return bitmap byte count of that character (0 for empty ones, ie space)
 */
uint32_t nbgl_getUnicodeFontCharacterByteCount(void)
{
#ifdef HAVE_LANGUAGE_PACK
    return unicodeCtx.unicode_character_byte_count;
#else   // defined(HAVE_LANGUAGE_PACK)
    return 0;
#endif  // HAVE_LANGUAGE_PACK
}

#ifdef HAVE_LANGUAGE_PACK
/**
 * @brief Function to be called when a change on the current language pack is notified by the OS,
 * to be sure that the current unicodeCtx variable will be set again at next @ref
 * nbgl_getUnicodeFont() call
 *
 */
void nbgl_refreshUnicodeFont(void)
{
    unicodeCtx.font       = NULL;
    unicodeCtx.characters = NULL;
}
#endif  // HAVE_LANGUAGE_PACK

#endif  // HAVE_UNICODE_SUPPORT
