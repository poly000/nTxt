#include <os.h>
#include "output.h"
#include "navigation.h"
#include "charmap_8x12.h"

static scr_type_t buffer_type;
static unsigned screen_width;
static unsigned screen_height;
static int screen_bytes_size;


static inline void setPixelBuf_color(void* scrbuf, unsigned x, unsigned y, uint16_t color) {
    if (x < screen_width && y < screen_height) {
        uint16_t* p = (uint16_t * )(scrbuf + 2*screen_width*y + 2*x);
        *p = color;
    }
}

static inline void setPixelBuf_grey(void* scrbuf, unsigned x, unsigned y, uint8_t color) {
    if (x < screen_width && y < screen_height) {
        uint8_t* p = (uint8_t*) scrbuf + screen_width/2*y + x/2;
        if (x % 2)
            *p = (*p & 0xF0) | color;
        else
            *p = (*p & 0x0F) | (color << 4);
    }
}

static inline void setPixelBuf(void* scrbuf, unsigned x, unsigned y, uint16_t color) {
    if (buffer_type == SCR_320x240_4)
        setPixelBuf_grey(scrbuf, x, y, color);
    else
        setPixelBuf_color(scrbuf, x, y, color);
}

void putChar(void* scrbuf, unsigned x, unsigned y, uint8_t chr) {
    int i, j, pixelOn;
    for (i = 0; i < CHAR_HEIGHT; i++) {
        for (j = 0; j < CHAR_WIDTH; j++) {
            pixelOn = charMap_ascii[chr][i] << j;
            pixelOn = pixelOn & 0b10000000;
            if (pixelOn) {
                setPixelBuf(scrbuf, x + j, y + i, 0);
            }
        }
    }
}

void putCharColor(void* scrbuf, unsigned x, unsigned y, uint8_t chr, uint16_t chrcolor, uint16_t bgcolor) {
    int i, j, pixelOn;
    for (i = 0; i < CHAR_HEIGHT; i++) {
        for (j = 0; j < CHAR_WIDTH; j++) {
            pixelOn = charMap_ascii[chr][i] << j;
            pixelOn = pixelOn & 0x80;
            setPixelBuf(scrbuf, x + j, y + i, pixelOn ? chrcolor : bgcolor);
        }
    }
}

void dispString(void* scrbuf, unsigned x, unsigned y, const char* message) {
    int l = strlen(message);
    int i;
    unsigned line = 0, col = 0;
    for (i = 0; i < l; i++) {
        if (message[i] == '\n') {
            col = 0;
            ++line;
        }
        else if (message[i] == '\t')
            col += TAB_WIDTH - (col % TAB_WIDTH);
        else {
            putChar(scrbuf, x + col * CHAR_WIDTH, y + line * CHAR_HEIGHT, message[i]);
            ++col;
        }
        if (x + col * CHAR_WIDTH >= screen_width) {
            col = 0;
            ++line;
        }
        if (y + line * CHAR_HEIGHT >= screen_height)
            break;
    }
}

void dispStringColor(void* scrbuf, unsigned x, unsigned y, const char* message, uint16_t chrcolor, uint16_t bgcolor) {
    const char* p;
    unsigned line = 0, col = 0;
    for (p = message; *p; ++p) {
        if (*p == '\n') {
            putCharColor(scrbuf, x + col * CHAR_WIDTH, y + line * CHAR_HEIGHT, ' ', chrcolor, bgcolor);  // disp colored blank
            col = 0;
            ++line;
        }
        else if (*p == '\t') {
            while (TAB_WIDTH - col % TAB_WIDTH) {
                putCharColor(scrbuf, x + col * CHAR_WIDTH, y + line * CHAR_HEIGHT, ' ', chrcolor, bgcolor);
                col++;
            }
        }
        else {
            putCharColor(scrbuf, x + col * CHAR_WIDTH, y + line * CHAR_HEIGHT, *p, chrcolor, bgcolor);
            ++col;
        }
        if (x + col * CHAR_WIDTH >= screen_width) {
            col = 0;
            ++line;
        }
        if (y + line * CHAR_HEIGHT >= screen_height)
            break;
    }
}

void dispStringWithSelection(void* scrbuf, unsigned x, unsigned y, char* message, int selectionstart, int selectionend) {
    int l = strlen(message);
    int i;
    unsigned line = 0, col = 0;
    for (i = 0; i < l; i++) {
        if (message[i] == '\n') {
            if (i >= selectionstart && i < selectionend)
                putCharColor(scrbuf, x + col * CHAR_WIDTH, y + line * CHAR_HEIGHT, ' ', 0, STR_SELECTION_COLOR); //disp colored blank
            col = 0;
            ++line;
        }
        else if (message[i] == '\t') {
            if (i >= selectionstart && i < selectionend) {
                do {
                    putCharColor(scrbuf, x + col * CHAR_WIDTH, y + line * CHAR_HEIGHT, ' ', 0, STR_SELECTION_COLOR);
                    col++;
                } while (col % TAB_WIDTH);
            }
            else
                col += TAB_WIDTH - (col % TAB_WIDTH);
        }
        else {
            if (i >= selectionstart && i < selectionend)
                putCharColor(scrbuf, x + col * CHAR_WIDTH, y + line * CHAR_HEIGHT, message[i], WHITE_COLOR, STR_SELECTION_COLOR);
            else
                putChar(scrbuf, x + col * CHAR_WIDTH, y + line * CHAR_HEIGHT, message[i]);
            ++col;
        }

        if (x + col * CHAR_WIDTH >= screen_width) {
            col = 0;
            ++line;
        }
        if (y + line * CHAR_HEIGHT >= screen_height)
            break;
    }
}

void dispStringWithSelection_nosoftbreak(void* scrbuf, const char* textbuffer, int pos, int cursorscreencol, int selectionstart, int selectionend) {
    const unsigned NUM_LINES = screen_height / CHAR_HEIGHT;
    const unsigned NUM_COLS = screen_width / CHAR_WIDTH;
    char* linelist[NUM_LINES];
    int poslist[NUM_LINES];  // contains the pos of the first char of the respective line
    memset(&linelist, 0, sizeof(char*) * NUM_LINES);
    unsigned i;
    const char* p = textbuffer;
    int len = 0;
    for (i = 0; i < NUM_LINES; i++) {
        if (strchr(p, '\n') != NULL)
            len = strchr(p, '\n') - p;
        else
            len = strrchr(p, '\0') - p;
        poslist[i] = p - textbuffer;
        linelist[i] = malloc(len + 1);
        if (linelist[i] == NULL) {
            printf("malloc failed\n");
            while (i--)
                free(linelist[i]);
            return;
        }
        strncpy(linelist[i], p, len);
        linelist[i][len] = '\0';
        p += len;
        if (*p == '\0')
            break;
        p++;
    }
    int coloffset = getw_nosoftbreak(textbuffer, pos) - cursorscreencol;
    for (i = 0; i < NUM_LINES && linelist[i] != 0; i++) {
        int linepos = gotow_nosoftbreak(linelist[i], 0, coloffset);
        int xoff = getw_nosoftbreak(linelist[i], linepos) - coloffset;
        unsigned w = 0;
        for (; linelist[i][linepos] != '\0' && w < NUM_COLS; linepos++) {
            if (linelist[i][linepos] == '\t') {
                if (poslist[i] + linepos >= selectionstart && poslist[i] + linepos < selectionend) {
                    do {
                        putCharColor(scrbuf, (xoff + w) * CHAR_WIDTH, i * CHAR_HEIGHT, ' ', WHITE_COLOR, STR_SELECTION_COLOR);
                        w++;
                    } while ((w + coloffset + xoff) % TAB_WIDTH);
                }
                else
                    w += TAB_WIDTH - (w + coloffset + xoff) % TAB_WIDTH;
            }
            else {
                if (poslist[i] + linepos >= selectionstart && poslist[i] + linepos < selectionend)
                    putCharColor(scrbuf, (xoff + w) * CHAR_WIDTH, i * CHAR_HEIGHT, linelist[i][linepos], WHITE_COLOR, STR_SELECTION_COLOR);
                else
                    putChar(scrbuf, (xoff + w) * CHAR_WIDTH, i * CHAR_HEIGHT, linelist[i][linepos]);
                w++;
            }
        }
        if (linelist[i + 1] != 0 && poslist[i + 1] - 1 >= selectionstart && poslist[i + 1] - 1 < selectionend)  // if newline char is selected, print a selected space
            putCharColor(scrbuf, (xoff + w) * CHAR_WIDTH, i * CHAR_HEIGHT, ' ', WHITE_COLOR, STR_SELECTION_COLOR);
        free(linelist[i]);
    }
}

void dispCursor(void* scrbuf, int offset, char* displinep) {  // offset = where is cursor relative to displinep
    unsigned row = 0;
    unsigned col = 0;
    int i;
    for (i = 0; i < offset; i++) {
        switch (displinep[i]) {
            case '\n':
                row++;
                col = 0;
                break;
            case '\t':
                col += TAB_WIDTH - (col % TAB_WIDTH);
                break;
            default:
                col++;
        }
        if (col >= screen_width / CHAR_WIDTH) {
            row++;
            col = 0;
        }
    }
    dispVertLine(scrbuf, col * CHAR_WIDTH, row * CHAR_HEIGHT, CHAR_HEIGHT, 0x0);
}

void dispCursor_nosoftbreak(void* scrbuf, int offset, char* displinep, int cursorscreencol) {
    char restore = displinep[offset];
    displinep[offset] = '\0';
    int row = countnewl(displinep);
    displinep[offset] = restore;
    dispVertLine(scrbuf, cursorscreencol * CHAR_WIDTH, row * CHAR_HEIGHT, CHAR_HEIGHT, 0x0);
}

void dispHorizLine(void* scrbuf, int x, int y, int l, int color) {
    while (l--)
        setPixelBuf(scrbuf, x++, y, color);
}

void dispVertLine(void* scrbuf, int x, int y, int l, int color) {
    while (l--)
        setPixelBuf(scrbuf, x, y++, color);
}

void dispRect(void* scrbuf, int x, int y, int w, int h, int color) {
    dispHorizLine(scrbuf, x, y, w, color);
    dispHorizLine(scrbuf, x, y + h - 1, w, color);
    dispVertLine(scrbuf, x, y + 1, h - 2, color);
    dispVertLine(scrbuf, x + w - 1, y + 1, h - 2, color);
}

void filledRect(void* scrbuf, int x, int y, int w, int h, int color) {
    while (h--)
        dispHorizLine(scrbuf, x, y++, w, color);
}

void clearScreen(void* scrbuf) {
    memset(scrbuf, 0xFF, screen_bytes_size);
}


void ntxt_show_msgbox(const char* title, const char* msg) {
    lcd_init(SCR_TYPE_INVALID);
    show_msgbox(title, msg);
    lcd_init(buffer_type);
}

unsigned ntxt_show_msgbox_2b(const char* title, const char* msg, const char* button1, const char* button2) {
    lcd_init(SCR_TYPE_INVALID);
    unsigned res = show_msgbox_2b(title, msg, button1, button2);
    lcd_init(buffer_type);
    return res;
}

unsigned ntxt_show_msgbox_3b(const char* title, const char* msg, const char* button1, const char* button2, const char* button3) {
    lcd_init(SCR_TYPE_INVALID);
    unsigned res = show_msgbox_3b(title, msg, button1, button2, button3);
    lcd_init(buffer_type);
    return res;
}

int ntxt_show_msg_user_input(const char* title, const char* msg, char* defaultvalue, char** value_ref) {
    lcd_init(SCR_TYPE_INVALID);
    int res = show_msg_user_input(title, msg, defaultvalue, value_ref);
    lcd_init(buffer_type);
    return res;
}


void showBuffer(void* scrbuf) {
    lcd_blit(scrbuf, buffer_type);
}

void* initScrbuf() {
    screen_width = 320;
    screen_height = 240;
    if (has_colors) {
        buffer_type = SCR_320x240_565;
        screen_bytes_size = screen_height * screen_width * 2;
    }
    else {
        buffer_type = SCR_320x240_4;
        screen_bytes_size = screen_height * screen_width / 2;
    }
    if (!lcd_init(buffer_type)) {
        printf("lcd_init failed\n");
        return NULL;
    }
    void* scrbuf = malloc(screen_bytes_size);
    if (scrbuf == NULL)
        printf("Failed to allocate %i bytes for scrbuf\n", screen_bytes_size);
    return scrbuf;
}

void freeScrbuf(void* scrbuf) {
    free(scrbuf);
    lcd_init(SCR_TYPE_INVALID);
}
