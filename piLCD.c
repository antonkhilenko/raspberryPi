// ****************************************************************************
// ****************************************************************************
/*
    piLCD:

    LCD control app for the Raspberry Pi.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>
        - based on Python script lcd_16x2.py 2015 by Matt Hawkins.
        - see http://www.raspberrypi-spy.co.uk

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
*/
// ****************************************************************************
// ****************************************************************************

#define Version "Version 0.1"

//  Compilation:
//
//  Compile with gcc piLCD.c -o piLCD -lwiringPi
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//  Authors:        D.Faulke    08/11/2015  This program.
//
//  Contributors:
//
//  Changelog:
//
//  v0.1 Original version.
//

//  To Do:
//      Add routine to check validity of GPIOs.
//      Improve error trapping and return codes for all functions.
//      Write GPIO and interrupt routines to replace wiringPi.
//      Remove all global variables.
//

#include <stdio.h>
#include <string.h>
#include <argp.h>
#include <wiringPi.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>

// ============================================================================
//  Information.
// ============================================================================
/*
    Pin layout for Hitachi HD44780 based 16x2 LCD.

    +-----+-------+------+---------------------------------------+
    | Pin | Label | Pi   | Description                           |
    +-----+-------+------+---------------------------------------+
    |   1 |  Vss  | GND  | Ground (0V) for logic.                |
    |   2 |  Vdd  | 5V   | 5V supply for logic.                  |
    |   3 |  Vo   | xV   | Variable V for contrast.              |
    |   4 |  RS   | GPIO | Register Select. 0: command, 1: data. |
    |   5 |  RW   | GND  | R/W. 0: write, 1: read. *Caution*     |
    |   6 |  E    | GPIO | Enable bit.                           |
    |   7 |  DB0  | n/a  | Data bit 0. Not used in 4-bit mode.   |
    |   8 |  DB1  | n/a  | Data bit 1. Not used in 4-bit mode.   |
    |   9 |  DB2  | n/a  | Data bot 2. Not used in 4-bit mode.   |
    |  10 |  DB3  | n/a  | Data bit 3. Not used in 4-bit mode.   |
    |  11 |  DB4  | GPIO | Data bit 4.                           |
    |  12 |  DB5  | GPIO | Data bit 5.                           |
    |  13 |  DB6  | GPIO | Data bit 6.                           |
    |  14 |  DB7  | GPIO | Data bit 7.                           |
    |  15 |  A    | xV   | Voltage for backlight (max 5V).       |
    |  16 |  K    | GND  | Ground (0V) for backlight.            |
    +-----+-------+------+---------------------------------------+

    Note: Setting pin 5 (R/W) to 1 (read) while connected to a GPIO
          will likely damage the Pi unless V is reduced or grounded.
*/

/*
    HD44780 command codes
        - see https://en.wikipedia.org/wiki/Hitachi_HD44780_LCD_controller

    Key:
    +-----+----------------------+
    | Key | Effect               |
    +-----+----------------------+
    | D/I | Cursor pos L/R       |
    | L/R | Shift display L/R.   |
    | S   | Auto shift off/on.   |
    | DL  | Nibble/byte mode.    |
    | D   | Display off/on.      |
    | N   | 1/2 lines.           |
    | C   | Cursor off/on.       |
    | F   | 5x7/5x10 dots.       |
    | B   | Cursor blink off/on. |
    | C/S | Move cursor/display. |
    | BF  | Busy flag.           |
    +-----+----------------------+

    DDRAM: Display Data RAM.
    CGRAM: Character Generator RAM.

    LCD register bits:
    +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
    | RS  | RW  | D7  | D6  | D5  | D4  | D3  | D2  | D1  | D0  |
    +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
    |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  1  |
    |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  1  |  -  |
    |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  1  | D/I |  S  |
    |  0  |  0  |  0  |  0  |  0  |  0  |  1  |  D  |  C  |  B  |
    |  0  |  0  |  0  |  0  |  0  |  1  | C/S | L/R |  -  |  -  |
    |  0  |  0  |  0  |  0  |  1  | DL  |  N  |  F  |  -  |  -  |
    |  0  |  0  |  0  |  1  |     :   CGRAM address :     :     |
    |  0  |  0  |  1  |     :     : DDRAM address   :     :     |
    |  0  |  1  | BF  |     :     : Address counter :     :     |
    |  1  |  0  |     :     :    Read Data    :     :     :     |
    |  1  |  1  |     :     :    Write Data   :     :     :     |
    +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
*/
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Useful LCD commands and constants.
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Constants
#define BITS_BYTE    8 // Number of bits in a byte.
#define BITS_NIBBLE  4 // Number of bits in a nibble.
#define PINS_DATA    4 // Number of LCD data pins being used.
#define LCD_WIDTH   16 // No of LCD display characters.
#define LCD_LINES    2 // No of LCD display lines.

// Modes
#define MODE_CMD     0 // Enable command mode for RS pin.
#define MODE_CHAR    1 // Enable character mode for RS pin.

// Clear and reset.
#define MODE_CLR  0x01 // Clear LCD screen.
#define MODE_HOME 0x02 // Screen and cursor home.
#define MODE_INIT 0x30 // Initialise.

// Character entry modes.
#define MODE_ENTR 0x04 // OR this with the options below:
#define ENTR_INCR 0x02 // Cursor increment. Default is decrement,
#define ENTR_SHFT 0x01 // Auto shift. Default is off.

// Screen and cursor commands.
#define MODE_DISP 0x08 // OR this with the options below:
#define DISP_ON   0x04 // Display on. Default is off.
#define CURS_ON   0x02 // Cursor on. Default is off.
#define BLNK_ON   0x01 // Blink on. Default is off.

// Screen and cursor movement.
#define MODE_MOVE 0x10 // OR this with the options below:
#define MOVE_DISP 0x08 // Move screen. Default is cursor.
#define MOVE_RGHT 0x04 // Move screen/cursor right. Default is left.

// LCD function modes.
#define MODE_LCD  0x20 // OR this with the options below:
#define LCD_DATA  0x10 // 8 bit (byte) mode. Default is 4 bit (nibble) mode.
#define LCD_LINE  0x08 // Use 2 display lines. Default is 1 line.
#define LCD_FONT  0x04 // 5x10 font. Default is 5x7 font.

// LCD character generator and display addresses.
#define CHAR_ADDR 0x40 // Character generator start address.
#define DISP_ADDR 0x80 // Display data start address.

#define GOTO_1    0x80 // Move cursor to start of line 1.
#define GOTO_2    0xC0 // Move cursor to start of line 2


// ============================================================================
//  Data structures.
// ============================================================================

/*
    Note: char is the smallest integer size (usually 8 bit) and is used to
          keep the memory footprint as low as possible.
*/

// ----------------------------------------------------------------------------
// Data structure for GPIOs and setting LCD mode.
// ----------------------------------------------------------------------------
struct gpioStruct
{
    unsigned char rs;            // GPIO pin for LCD RS pin.
    unsigned char en;            // GPIO pin for LCD Enable pin.
    unsigned char rw;            // GPIO pin for R/W mode. Not used.
    unsigned char db[PINS_DATA]; // GPIO pins for LCD data pins.
} gpio =
{
    .rs    = 7,   // Pin 26.
    .en    = 8,   // Pin 24.
    .rw    = 11,  // Pin 23
    .db[0] = 25,  // Pin 22.
    .db[1] = 24,  // Pin 18.
    .db[2] = 23,  // Pin 16.
    .db[3] = 18   // Pin 12.
//    .db[0] = 18,  // Pin 12.
//    .db[1] = 23,  // Pin 16.
//    .db[2] = 24,  // Pin 18.
//    .db[3] = 25   // Pin 22.
};

struct modeStruct
{
    // MODE_DISP
    unsigned char display   :1; // 0 = display off, 1 = display on.
    unsigned char cursor    :1; // 0 = cursor off, 1 = cursor on.
    unsigned char blink     :1; // 0 = blink off, 1 = blink on.
    // MODE_LCD
    unsigned char data      :1; // 0 = nibble mode, 1 = byte mode.
    unsigned char lines     :1; // 0 = 1 line, 1 = 2 lines.
    unsigned char font      :1; // 0 = 5x7, 1 = 5x10.
    // MODE_MOVE
    unsigned char movedisp  :1; // 0 = move cursor, 1 = move screen.
    unsigned char direction :1; // 0 = left, 1 = right.
    //MODE_ENTR
    unsigned char increment :1; // 0 = decrement, 1 = increment.
    unsigned char shift     :1; // 0 = auto shift off, 1 = auto shift on.
    unsigned char           :6; // Padding to make structure 16 bits.
} mode =
{
    .display   = 1,
    .cursor    = 1,
    .blink     = 0,
    .data      = 0,
    .lines     = 1,
    .font      = 1,
    .movedisp  = 0,
    .direction = 1,
    .increment = 1,
    .shift     = 0
};

// ****************************************************************************
//  LCD functions.
// ****************************************************************************

// ----------------------------------------------------------------------------
//  Toggles Enable bit to allow writing.
// ----------------------------------------------------------------------------
static char toggleEnable( void )
{
    digitalWrite( gpio.en, 1 );
    delayMicroseconds ( 50 );
    digitalWrite( gpio.en, 0 );
    delayMicroseconds( 50 );
}


// ----------------------------------------------------------------------------
//  Returns string equivalent of byte.
// ----------------------------------------------------------------------------
static void printByte( unsigned char byte )
{
    unsigned char i;
    for ( i = 0; i < BITS_BYTE; i++ )
    {
        printf( "%i", ( byte & 1 ));
        byte >>= 1;
    }
    return;
};


// ----------------------------------------------------------------------------
//  Returns string equivalent of nibble.
// ----------------------------------------------------------------------------
static void printNibble( unsigned char nibble )
{
    unsigned char i;
    for ( i = 0; i < BITS_NIBBLE; i++ )
    {
        printf( "%i", ( nibble & 1 ));
        nibble >>= 1;
    }
    return;
};


// ----------------------------------------------------------------------------
//  Writes byte value of a char to LCD in nibbles.
// ----------------------------------------------------------------------------
static char writeNibble( unsigned char nibble )
{
    unsigned char i;

    // Write nibble to GPIOs
    for ( i = 0; i < BITS_NIBBLE; i++ )
    {
        digitalWrite( gpio.db[i], ( nibble & 1 ));
        nibble >>= 1;
    }
    // Toggle enable bit to send nibble.
    toggleEnable();

    return 0;
};


// ----------------------------------------------------------------------------
//  Writes byte value of a command to LCD in nibbles.
// ----------------------------------------------------------------------------
static char writeCmd( unsigned char data )
{
    unsigned char nibble;
    unsigned char i;

    // Set to command mode.
    digitalWrite( gpio.rs, 0 );

    printf( "Command = 0x%02x, binary = ", data );
    printByte( data ); printf( ".\n" );
    printf( "Nibbles = " );
    // High nibble.

    nibble = data & 0x0F;
    writeNibble( nibble );
    printNibble( nibble );
    printf ( "," );

    // Low nibble.
    nibble = ( data >> BITS_NIBBLE ) & 0x0F;
    writeNibble( nibble );
    printNibble( nibble );
    printf( ".\n" );

    delay ( 5 );
    return 0;
};


// ----------------------------------------------------------------------------
//  Writes byte value of a command to LCD in nibbles.
// ----------------------------------------------------------------------------
static char writeChar( unsigned char data )
{
    unsigned char i;
    unsigned char nibble;

    // Set to character mode.
    digitalWrite( gpio.rs, 1 );

    printf( "Char = %c, binary = ", data );
    printByte( data ); printf( ".\n" );
    printf( "Nibbles = " );
    // High nibble.
    nibble = data & 0x0F;
    writeNibble( nibble );
    printf ( "," );

    // Low nibble.
    nibble = ( data >> BITS_NIBBLE ) & 0x0F;
    writeNibble( nibble );
    printf( ".\n" );

    return 0;
};


// ----------------------------------------------------------------------------
//  Writes a string to LCD.
// ----------------------------------------------------------------------------
static char writeString( char *string, unsigned char line  )
{
    unsigned int i;

    if (( line < 1 ) || ( line > 2 )) return -1;
    if ( line == 1 ) writeCmd( GOTO_1 );
    else
    if ( line == 1 ) writeCmd( GOTO_2 );

    for ( i = 0; i < strlen( string ); i++ )
        writeChar( string[i] );

    return 0;
};


// ----------------------------------------------------------------------------
//  Clears LCD screen.
// ----------------------------------------------------------------------------
static char clearScreen( void )
{
    writeCmd( MODE_CLR );
    return 0;
}


// ----------------------------------------------------------------------------
//  Initialise LCD.
// ----------------------------------------------------------------------------
static char initLCD( void )
{
/*
    LCD has to be initialised in 8-bit mode by writing a fixed sequence
    of initialise commands with delays betwen each command.
*/
    delay( 30 );
    writeCmd( MODE_INIT );
    delay( 35 );
    writeCmd( MODE_INIT );
    delay( 35 );
    writeCmd( MODE_INIT );
    delay( 35 );
    return 0;
};


// ----------------------------------------------------------------------------
//  set default LCD mode.
// ----------------------------------------------------------------------------
static char setMode( void )
{

    unsigned char lcd;
    unsigned char display;
    unsigned char entry;
    unsigned char move;

    lcd = MODE_LCD & ( mode.data & LCD_DATA )
                   & ( mode.lines & LCD_LINE )
                   & ( mode.font & LCD_FONT );

    writeCmd( lcd );
    delay( 35 );

    writeCmd( MODE_DISP ); // Turn off display, cursor and blink.

    entry = MODE_ENTR & ( mode.increment & ENTR_INCR )
                      & ( mode.shift & ENTR_SHFT );

    move = MODE_MOVE & ( mode.movedisp & MOVE_DISP )
                     & ( mode.direction & MOVE_RGHT );

    display = MODE_DISP & ( mode.display & DISP_ON )
                        & ( mode.cursor & CURS_ON )
                        & ( mode.blink & BLNK_ON );

    writeCmd( display );
    delay( 35 );
    writeCmd( entry );
    delay( 35 );
    writeCmd( move );
    delay( 35 );
    writeCmd( MODE_CLR );
    delay( 35 );

    return 0;
};


// ----------------------------------------------------------------------------
//  Initialises GPIOs.
// ----------------------------------------------------------------------------
static char initGPIO( void )
{
    unsigned char i;
    wiringPiSetupGpio();

    // Set all GPIO pins to 0.
    digitalWrite( gpio.rs, 0 );
    digitalWrite( gpio.en, 0 );
    for ( i = 0; i < PINS_DATA; i++ )
        digitalWrite( gpio.db[i], 0 );

    // Set LCD pin modes.
    pinMode( gpio.rs, OUTPUT );
    pinMode( gpio.en, OUTPUT );
    // Data pins.
    for ( i = 0; i < PINS_DATA; i++ )
        pinMode( gpio.db[i], OUTPUT );

    return 0;
}


// ****************************************************************************
//  Command line option functions.
// ****************************************************************************

// ============================================================================
//  argp documentation.
// ============================================================================
const char *argp_program_version = Version;
const char *argp_program_bug_address = "darren@alidaf.co.uk";
static const char doc[] = "Raspberry Pi LCD control.";
static const char args_doc[] = "piLCD <options>";


// ============================================================================
//  Command line argument definitions.
// ============================================================================
static struct argp_option options[] =
{
    { 0, 0, 0, 0, "Switches:" },
    { "rs", 'r', "<int>", 0, "GPIO for RS (instruction code)" },
    { "en", 'e', "<int>", 0, "GPIO for EN (chip enable)" },
    { 0, 0, 0, 0, "Data pins:" },
    { "db4", 'a', "<int>", 0, "GPIO for data bit 4." },
    { "db5", 'b', "<int>", 0, "GPIO for data bit 5." },
    { "db6", 'c', "<int>", 0, "GPIO for data bit 6." },
    { "db7", 'd', "<int>", 0, "GPIO for data bit 7." },
    { 0 }
};


// ============================================================================
//  Command line argument parser.
// ============================================================================
static int parse_opt( int param, char *arg, struct argp_state *state )
{
    char *str, *token;
    const char delimiter[] = ",";
    struct gpioStruct *gpio = state->input;

    switch ( param )
    {
        case 'r' :
            gpio->rs = atoi( arg );
            break;
        case 'e' :
            gpio->en = atoi( arg );
            break;
        case 'a' :
            gpio->db[0] = atoi( arg );
            break;
        case 'b' :
            gpio->db[1] = atoi( arg );
            break;
        case 'c' :
            gpio->db[2] = atoi( arg );
            break;
        case 'd' :
            gpio->db[3] = atoi( arg );
            break;
    }
    return 0;
};


// ============================================================================
//  argp parser parameter structure.
// ============================================================================
static struct argp argp = { options, parse_opt, args_doc, doc };


// ============================================================================
//  Main section.
// ============================================================================
char main( int argc, char *argv[] )
{

    // ------------------------------------------------------------------------
    //  Get command line arguments and check within bounds.
    // ------------------------------------------------------------------------
    argp_parse( &argp, argc, argv, 0, 0, &gpio );
    // Need to check validity of pins.


    // ------------------------------------------------------------------------
    //  Initialise wiringPi and LCD.
    // ------------------------------------------------------------------------
    initGPIO();
    initLCD();
    setMode();

    clearScreen();

//    writeString( "abcdefghijklmnopqrstuvwxyz", 1 );
//    writeString( "0123456789", 2 );

//    unsigned char data = 0xA5;
//    unsigned char nibble;
//    unsigned char i;

//    printf( "\n0x%x = %s.\n", data, getStringByte( data ));
//    printf( "Nibbles = " );
//    nibble = ( data & 0x0F );
//    for ( i = 0; i < 4; i++ )
//    {
//        printf( "%i", ( nibble & 1 ));
//        nibble >>= 1;
//    }
//    printf( "," );
//    nibble = ( data >> 4 ) & 0x0F;
//    for ( i = 0; i < 4; i++ )
//    {
//        printf( "%i", ( nibble & 1 ));
//        nibble >>= 1;
//    }
//    printf( ".\n" );

    return 0;
}
