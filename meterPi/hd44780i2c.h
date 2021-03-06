/*
//  ===========================================================================

    hd44780i2cPi:

    Raspberry Pi driver for the HD44780 LCD display via the MCP23017
    port expander.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.

//  ===========================================================================

    Compile with:

        gcc -c -fpic -Wall hd44780i2c.c mcp23017.c -lpthread

    Also use the following flags for Raspberry Pi optimisation:

        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

//  ---------------------------------------------------------------------------

    Authors:        D.Faulke    24/12/2015

    Contributors:

//  Information. --------------------------------------------------------------

    Pin layout for Hitachi HD44780 based LCD display.

        +------------------------------------------------------------+
        | Pin | Label | Pi   | Description                           |
        |-----+-------+------+---------------------------------------|
        |   1 |  Vss  | GND  | Ground (0V) for logic.                |
        |   2 |  Vdd  | 5V   | 5V supply for logic.                  |
        |   3 |  Vo   | xV   | Variable V for contrast.              |
        |   4 |  RS   | GPIO | Register Select. 0: command, 1: data. |
        |   5 |  RW   | GND  | R/W. 0: write, 1: read. *Caution*     |
        |   6 |  E    | GPIO | Enable bit.                           |
        |   7 |  DB0  | n/a  | Data bit 0. Not used in 4-bit mode.   |
        |   8 |  DB1  | n/a  | Data bit 1. Not used in 4-bit mode.   |
        |   9 |  DB2  | n/a  | Data bit 2. Not used in 4-bit mode.   |
        |  10 |  DB3  | n/a  | Data bit 3. Not used in 4-bit mode.   |
        |  11 |  DB4  | GPIO | Data bit 4.                           |
        |  12 |  DB5  | GPIO | Data bit 5.                           |
        |  13 |  DB6  | GPIO | Data bit 6.                           |
        |  14 |  DB7  | GPIO | Data bit 7.                           |
        |  15 |  A    | xV   | Voltage for backlight (max 5V).       |
        |  16 |  K    | GND  | Ground (0V) for backlight.            |
        +------------------------------------------------------------+

    LCD register bits:

        +---------------------------------------+   +-------------------+
        |RS |RW |DB7|DB6|DB5|DB4|DB3|DB2|DB1|DB0|   |Key|Effect         |
        |---+---+---+---+---+---+---+---+---+---|   |---+---------------|
        | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 |   |I/D|DDRAM inc/dec. |
        | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | - |   |R/L|Shift R/L.     |
        | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 |I/D| S |   |S  |Shift on.      |
        | 0 | 0 | 0 | 0 | 0 | 0 | 1 | D | C | B |   |DL |4-bit/8-bit.   |
        | 0 | 0 | 0 | 0 | 0 | 1 |S/C|R/L| - | - |   |D  |Display on/off.|
        | 0 | 0 | 0 | 0 | 1 |DL | N | F | - | - |   |N  |1/2 lines.     |
        | 0 | 0 | 0 | 1 |   : CGRAM address :   |   |C  |Cursor on/off. |
        | 0 | 0 | 1 |   :   DDRAM address   :   |   |F  |5x8/5x10 font. |
        | 0 | 1 |BF |   :   Address counter :   |   |B  |Blink on/off.  |
        | 1 | 0 |   :   : Read Data :   :   :   |   |S/C|Display/cursor.|
        | 1 | 1 |   :   : Write Data:   :   :   |   |BF |Busy flag.     |
        +---------------------------------------+   +-------------------+

    DDRAM: Display Data RAM.
    CGRAM: Character Generator RAM.

//  ---------------------------------------------------------------------------

    The MCP23017 is an I2C bus operated 16-bit I/O port expander:

                           +-----------( )-----------+
                           |  Fn  | pin | pin |  Fn  |
                           |------+-----+-----+------|
                           | GPB0 |  01 | 28  | GPA7 |
                           | GPB1 |  02 | 27  | GPA6 |
                           | GPB2 |  03 | 26  | GPA5 |
                           | GPB3 |  04 | 25  | GPA4 |
                           | GPB4 |  05 | 24  | GPA3 |
                           | GPB5 |  06 | 23  | GPA2 |
                           | GPB6 |  07 | 22  | GPA1 |
                           | GPB7 |  08 | 21  | GPA0 |
                           |  VDD |  09 | 20  | INTA |
                           |  VSS |  10 | 19  | INTB |
                           |   NC |  11 | 18  | RST  |
                           |  SCL |  12 | 17  | A2   |
                           |  SDA |  13 | 16  | A1   |
                           |   NC |  14 | 15  | A0   |
                           +-------------------------+

//  ---------------------------------------------------------------------------

    Wiring the HD44780 Display to the MCP23017:

        The MCP23017 has two 8-bit ports (PORTA & PORTB) that can operate in
        8-bit or 16-bit modes. This driver assumes that the HD44780 display is
        attached to the MCP23017 in 8-bit mode with all data pins (DB0-DB7)
        attached to GPIOB. The RS, R/W, and E pins are attached to GPIOA.
        Further displays can share the same GPIOs except the E pin, which must
        have a unique GPIOA pin for each display.

        This driver uses 8-bit mode to allow up to 6 devices to use a single
        MCP23017. If any other devices are connected, they should only operate
        by setting individual pins rather than an 8-bit write to the PORT.

                       GND
                        |    10k
        +-----------+   +---\/\/\--x
        | pin | Fn  |   |     |
        |-----+-----|   |     |   ,----------------------------------,
        |   1 | VSS |---'     |   | ,--------------------------------|-,
        |   2 | VDD |--> 5V   |   | | ,------------------------------|-|-,
        |   3 | Vo  |---------'   | | |                              | | |
        |   4 | RS  |-------------' | | +-----------( )-----------+  | | |
        |   5 | R/W |---------------' | |  Fn  | pin | pin |  Fn  |  | | |
        |   6 | E   |-----------------' |------+-----+-----+------|  | | |
        |   7 | DB0 |------------------>| GPB0 |  01 | 28  | GPA7 |<-' | |
        |   8 | DB1 |------------------>| GPB1 |  02 | 27  | GPA6 |<---' |
        |   9 | DB2 |------------------>| GPB2 |  03 | 26  | GPA5 |<-----'
        |  10 | DB3 |------------------>| GPB3 |  04 | 25  | GPA4 |
        |  11 | DB4 |------------------>| GPB4 |  05 | 24  | GPA3 |
        |  12 | DB5 |------------------>| GPB5 |  06 | 23  | GPA2 |
        |  13 | DB6 |------------------>| GPB6 |  07 | 22  | GPA1 |
        |  14 | DB7 |------------------>| GPB7 |  08 | 21  | GPA0 |
        |  15 | A   |--+----------------|  VDD |  09 | 20  | INTA |
        |  16 | K   |--|----+-----------|  VSS |  10 | 19  | INTB |
        +-----------+  |    |           |   NC |  11 | 18  | RST  |-----> +5V
                       |    |      ,----|  SCL |  12 | 17  | A2   |---,
                       |    |      |  ,-|  SDA |  13 | 16  | A1   |---+-> GND
                       |    |      |  | |   NC |  14 | 15  | A0   |---'
                       v    v      |  | +-------------------------+
                      +5V  GND     |  |
                                 {-+  +-}
                                 {  <<  } Logic level shifter *
                                 {-+  +-}   (bi-directional)
                                   |  |
                                   v  v
                                SCL1  SDA1

    Notes:  Vo is connected to the wiper of a 10k trim pot to adjust the
            contrast. A similar (perhaps 5k) pot could be used to adjust the
            backlight but connecting to 3.3V works OK instead. These displays
            are commonly sold with a single 10k pot.

            * The HD44780 operates slightly faster at 5V but 3.3V works fine.
            The MCP23017 expander can operate at both levels. The Pi's I2C
            pins have pull-up resistors that should protect against 5V levels
            but use a logic level shifter if there is any doubt.
*/

#ifndef HD44780I2C_H
#define HD44780I2C_H

//  Macros. -------------------------------------------------------------------

// Constants. Change these according to needs.
#define BITS_BYTE          8 // Number of bits in a byte.
#define BITS_NIBBLE        4 // Number of bits in a nibble.
#define PINS_DATA          4 // Number of data pins used.
#define HD44780_MAX        6 // Max number of displays (single MCP23017).
#define TEXT_MAX_LENGTH  512 // Arbitrary length limit for text string.
#define FRAMES_MAX         2 // Maximum animation frames.

// These should be replaced by command line options.
#define DISPLAY_COLUMNS   16 // No of LCD display characters.
#define DISPLAY_ROWS       2 // No of LCD display lines.
#define DISPLAY_NUM        1 // Number of displays.
#define DISPLAY_ROWS_MAX   4 // Max known number of rows for this type of LCD.

// Modes
#define MODE_COMMAND       0 // Enable command mode for RS pin.
#define MODE_DATA          1 // Enable character mode for RS pin.

// Clear and reset.
#define DISPLAY_CLEAR   0x01 // Clears DDRAM and sets address counter to start.
#define DISPLAY_HOME    0x02 // Sets DDRAM address counter to start.

// Character entry modes.
// ENTRY_BASE = command, decrement DDRAM counter, display shift off.
#define ENTRY_BASE      0x04 // OR this with the options below:
#define ENTRY_COUNTER   0x02 // Increment DDRAM counter (cursor position).
#define ENTRY_SHIFT     0x01 // Display shift on.

// Screen and cursor commands.
// DISPLAY_BASE = command, display off, underline cursor off, block cursor off.
#define DISPLAY_BASE    0x08 // OR this with the options below:
#define DISPLAY_ON      0x04 // Display on.
#define DISPLAY_CURSOR  0x02 // Cursor on.
#define DISPLAY_BLINK   0x01 // Block cursor on.

// Screen and cursor movement.
// MOVE_BASE = command, move cursor left.
#define MOVE_BASE       0x10 // OR this with the options below:
#define MOVE_DISPLAY    0x08 // Move screen.
#define MOVE_DIRECTION  0x04 // Move screen/cursor right.

// LCD function modes.
// FUNCTION_BASE = command, 4-bit mode, 1 line, 5x8 font.
#define FUNCTION_BASE   0x20 // OR this with the options below:
#define FUNCTION_DATA   0x10 // 8-bit (byte) mode.
#define FUNCTION_LINES  0x08 // Use 2 display lines.
#define FUNCTION_FONT   0x04 // 5x10 font.

// LCD character generator and display memory addresses.
#define ADDRESS_CGRAM   0x40 // Character generator start address.
#define ADDRESS_DDRAM   0x80 // Display data start address.
#define ADDRESS_ROW_0   0x00 // Row 1 start address.
#define ADDRESS_ROW_1   0x40 // Row 2 start address.
#define ADDRESS_ROW_2   0x14 // Row 3 start address.
#define ADDRESS_ROW_3   0x54 // Row 4 start address.


//  Mutex. --------------------------------------------------------------------

pthread_mutex_t displayBusy; // Locks further writes to display until finished.


//  Data structures. ----------------------------------------------------------

struct hd44780
{
    uint8_t rs;    // MCP23017 GPIOA pin address for HD44780 RS pin.
    uint8_t rw;    // MCP23017 GPIOA pin address for HD44780 R/W pin.
    uint8_t en;    // MCP23017 GPIOA pin address for HD44780 E pin.
};

struct hd44780 *hd44780[HD44780_MAX];

struct text
{
    struct  mcp23017 *mcp23017;
    struct  hd44780  *hd44780;
    uint8_t row;     // Display row.
    uint8_t col;     // Display column.
    char    *buffer; // Display text.
};

struct calendar
{
    struct  mcp23017 *mcp23017;  // MCP23017 instance.
    struct  hd44780  *hd44780;   // HD44780 instance.
    struct  timeval  delay;      // Delay between updates.
    uint8_t row;                 // Display row (y).
    uint8_t col;                 // Display col (x).
    uint8_t length;              // Length of formatting string.
    uint8_t frames;              // Actual number of animation frames.
    char    *format[FRAMES_MAX]; // format strings. Use for animating.
};
/*
        format[n] is a string containing <time.h> formatting codes.
        Some common codes are:
            %a  Abbreviated weekday name.
            %A  Full weekday name.
            %d  Day of the month.
            %b  Abbreviated month name.
            %B  Full month name.
            %m  Month number.
            %y  Abbreviated year.
            %Y  Full year.
            %H  Hour in 24h format.
            %I  Hour in 12h format.
            %M  Minute.
            %S  Second.
            %p  AM/PM.
*/

struct ticker
{
    struct   mcp23017 *mcp23017;    // MCP23017 instance.
    struct   hd44780  *hd44780;     // HD44780 instance.
    struct   timeval  delay;        // Delay between updates.
    char     text[TEXT_MAX_LENGTH]; // Display text.
    uint16_t length;                // Text length.
    uint16_t padding;               // Text padding between end and start.
    uint8_t  row;                   // Display row.
    int16_t  increment;             // Size and direction of tick movement.
};
/*
    .increment = Number and direction of characters to rotate.
                 +ve: rotate left.
                 -ve: rotate right.
    .length + .padding must be < TEXT_MAX_LENGTH.
*/

//  HD44780 display functions. ------------------------------------------------

//  ---------------------------------------------------------------------------
//  Toggles E (enable) bit in byte mode without changing other bits.
//  ---------------------------------------------------------------------------
void hd44780Enable( struct mcp23017 *mcp23017, struct hd44780 *hd44780 );

//  ---------------------------------------------------------------------------
//  Writes a command or data byte (according to mode) to HD44780 via MCP23017.
//  ---------------------------------------------------------------------------
int8_t hd44780WriteByte( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                         uint8_t data, bool mode );

//  ---------------------------------------------------------------------------
//  Writes a data string to LCD.
//  ---------------------------------------------------------------------------
int8_t hd44780WriteString( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                           char *string, uint8_t len );

//  ---------------------------------------------------------------------------
//  Moves cursor to row, position.
//  ---------------------------------------------------------------------------
/*
    All displays, regardless of size, have the same start address for each
    row due to common architecture. Moving from the end of a line to the start
    of the next is not contiguous memory.
*/
int8_t hd44780Goto( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                    uint8_t row, uint8_t pos );

//  Display init and mode functions. ------------------------------------------

//  ---------------------------------------------------------------------------
//  Clears display.
//  ---------------------------------------------------------------------------
int8_t hd44780Clear( struct mcp23017 *mcp23017, struct hd44780 *hd44780 );

//  ---------------------------------------------------------------------------
//  Clears memory and returns cursor/screen to original position.
//  ---------------------------------------------------------------------------
int8_t hd44780Home( struct mcp23017 *mcp23017, struct hd44780 *hd44780 );

//  ---------------------------------------------------------------------------
//  Initialises display. Must be called before any other LCD functions.
//  ---------------------------------------------------------------------------
/*
    Currently only supports 4-bit mode initialisation.
    Software initialisation is achieved by setting 8-bit mode and writing a
    sequence of EN toggles with fixed delays between each command.
        Initial delay after Vcc rises to 2.7V
        15mS@5V, 40mS@3V (minimum).
        Set 8-bit mode (command 0x3).
        4.1mS (minimum).
        Set 8-bit mode (command 0x3).
        100uS (minimum).
        Set 8-bit mode (command 0x3).
        100uS (minimum).
        Set function mode. Cannot be changed after this unless re-initialised.
        37uS (minimum).
        Display off.
        Display clear.
        Set entry mode.
*/
int8_t hd44780Init( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                     bool data,    bool lines,  bool font,
                     bool display, bool cursor, bool blink,
                     bool counter, bool shift,
                     bool mode,    bool direction );
/*
    data      = 0: 4-bit mode.
    data      = 1: 8-bit mode.
    lines     = 0: 1 display line.
    lines     = 1: 2 display lines.
    font      = 0: 5x10 font (uses 2 lines).
    font      = 1: 5x8 font.
    counter   = 0: Decrement DDRAM counter after data write (cursor moves L)
    counter   = 1: Increment DDRAM counter after data write (cursor moves R)
    shift     = 0: Do not shift display after data write.
    shift     = 1: Shift display after data write.
    display   = 0: Display off.
    display   = 1: Display on.
    cursor    = 0: Cursor off.
    cursor    = 1: Cursor on.
    blink     = 0: Blink (block cursor) on.
    blink     = 1: Blink (block cursor) off.
    mode      = 0: Shift cursor.
    mode      = 1: Shift display.
    direction = 0: Left.
    direction = 1: Right.
*/

//  Mode settings. ------------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Sets entry mode.
//  ---------------------------------------------------------------------------
/*
    counter = 0: Decrement DDRAM counter after data write (cursor moves L)
    counter = 1: Increment DDRAM counter after data write (cursor moves R)
    shift =   0: Do not shift display after data write.
    shift =   1: Shift display after data write.
*/
int8_t hd44780EntryMode( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                         bool counter, bool shift );

//  ---------------------------------------------------------------------------
//  Sets display mode.
//  ---------------------------------------------------------------------------
/*
    display = 0: Display off.
    display = 1: Display on.
    cursor  = 0: Cursor off.
    cursor  = 1: Cursor on.
    blink   = 0: Blink (block cursor) on.
    blink   = 0: Blink (block cursor) off.
*/
int8_t hd44780DisplayMode( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                           bool display, bool cursor, bool blink );

//  ---------------------------------------------------------------------------
//  Shifts cursor or display.
//  ---------------------------------------------------------------------------
/*
    mode      = 0: Shift cursor.
    mode      = 1: Shift display.
    direction = 0: Left.
    direction = 1: Right.
*/
int8_t hd44780MoveMode( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                        bool mode, bool direction );

//  Custom characters and animation. ------------------------------------------
/*
    Default characters actually have an extra row at the bottom, reserved
    for the cursor. It is therefore possible to define 8 5x8 characters.
*/

#define CUSTOM_SIZE  8 // Size of char (rows) for custom chars (5x8).
#define CUSTOM_MAX   8 // Max number of custom chars allowed.

struct customChars
{
    uint8_t num; // Number of custom chars (max 8).
    uint8_t data[CUSTOM_MAX][CUSTOM_SIZE];
};

//  ---------------------------------------------------------------------------
//  Loads custom characters into CGRAM.
//  ---------------------------------------------------------------------------
/*
    Set command to point to start of CGRAM and load data line by line. CGRAM
    pointer is auto-incremented. Set command to point to start of DDRAM to
    finish.
*/
int8_t hd44780LoadCustom( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                          const uint8_t newChar[CUSTOM_MAX][CUSTOM_SIZE] );


#endif
