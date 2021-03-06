/*
//  ===========================================================================

    rotencPi:

    Rotary encoder driver for the Raspberry Pi.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>

    Based on state machine algorithm by Michael Kellet.
        -see www.mkesc.co.uk/ise.pdf

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

    Authors:        D.Faulke    12/12/2015

    Contributors:

    Testers:        D.Stivens

    Changelog:

        v0.1    Original version.
        v0.2    Converted to libraries.

    To Do:

        Write GPIO and interrupt routines to replace wiringPi.
        Improve response by splitting interrupts.


//  Description of rotary encoder function. -----------------------------------

    Quadrature encoding:
                                                 +---------------------------+
          :   :   :   :   :   :   :   :   :      |     |  old  |  new  |     |
          :   +-------+   :   +-------+   :      | dir |-------+-------| hex |
          :   |   :   |   :   |   :   |   :      |     | a | b | A | B |     |
    a,A   :   |   :   |   :   |   :   |   :      |-----+---+---+---+---+-----|
      --------+   :   +-------+   :   +-------   | +ve | 0 | 0 | 1 | 0 | 0x2 |
          :   :   :   :   :   :   :   :   :      |     | 1 | 0 | 1 | 1 | 0xb |
          :   :   :   :   :   :   :   :   :      |     | 1 | 1 | 0 | 1 | 0xd |
          +-------+   :   +-------+   :   +---   |     | 0 | 1 | 0 | 0 | 0x4 |
          |   :   |   :   |   :   |   :   |      |-----+---+---+---+---+-----|
    b,B   |   :   |   :   |   :   |   :   |      | -ve | 1 | 1 | 1 | 0 | 0xe |
      ----+   :   +-------+   :   +-------+      |     | 1 | 0 | 0 | 0 | 0x8 |
          :   :   :   :   :   :   :   :   :      |     | 0 | 0 | 0 | 1 | 0x1 |
        1 : 2 : 3 : 4 : 1 : 2 : 3 : 4 : 1 : 2    |     | 0 | 1 | 1 | 1 | 0x7 |
          :   :   :   :   :   :   :   :   :      +---------------------------+

    A & B are current readings and a & b are the previous readings.
    dec is the decimal equivalent of nibble abAB.

//  ---------------------------------------------------------------------------

    The most direct method is to set an interrupt on the rising edfe of pin A
    and measure B. If B is high then the direction is +ve, otherwise -ve.

    SIMPLE_1 - Interrupt on leading edge of A. Sample B (1x resolution).

    Works surprisingly well with mechanical encoders with a small delay after
    reading the encoder direction.

//  ---------------------------------------------------------------------------

    There are a variety of other ways to decode the information but a simple
    state machine method by Michael Kellet, http://www.mkesc.co.uk/ise.pdf,
    is efficient and provides 2 modes.

    SIMPLE_2 - Interrupt on both edges of A. Read A & B (2x resolution).
    SIMPLE_4 - Interrupt on both edges of A and B. Read A & B, (4x).

    The state table contains directions for all possible combinations of abAB.

           +-----------------------------------------------------------+
           | abAB(hex) | 0| 1| 2| 3| 4| 5| 6| 7| 8| 9| a| b| c| d| e| f|
           |-----------+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--|
           | direction | 0|-1|+1| 0|+1| 0| 0|-1|-1| 0| 0|+1| 0|+1|-1| 0|
           +-----------------------------------------------------------+

    Can be a bit jumpy with mechanical encoders due to contact bounce.

//  ---------------------------------------------------------------------------

    An alternative method using a state transition table that helps with
    noisy encoders by ignoring invalid transitions between states has been
    offered by Ben Buxton, http://ww.buxtronix.net, and offers 2 modes with
    different resolutions. Both require reads of A & B.

    HALF - Outputs direction after half and full steps (2x resolution).
    FULL - Outputs direction after full step only (1x resolution).

    Each row is a state. Each cell in that row contains the new state to set
    based on the encoder output, AB. The direction is determined when the
    state reaches 0x10 (+ve) or 0x20 (-ve).

    Half mode transition table - outputs direction after half and full steps.

                +---------------------------------+
                |             | Encoder output AB |
                | Transitions |-------------------|
                |             | 00 | 01 | 10 | 11 |
                |-------------+----+----+----+----|
              ->| Start       | 03 | 02 | 01 | 00 |
                | -ve begin   | 23 | 00 | 01 | 00 |
                | +ve begin   | 13 | 02 | 00 | 00 |
                | Halfway     | 03 | 05 | 04 | 00 |
                | +ve begin   | 03 | 03 | 04 | 10 |-> +ve
                | -ve begin   | 03 | 05 | 03 | 20 |-> -ve
                +---------------------------------+

    Full mode transition table - outputs direction after full step only.

                +---------------------------------+
                |             | Encoder output AB |
                | Transitions |-------------------|
                |             | 00 | 01 | 10 | 11 |
                |-------------+----+----+----+----|
              ->| Start       | 00 | 02 | 04 | 00 |
                | +ve end     | 03 | 00 | 01 | 10 |-> +ve
                | +ve begin   | 03 | 02 | 00 | 00 |
                | +ve next    | 03 | 02 | 01 | 00 |
                | -ve begin   | 06 | 00 | 04 | 00 |
                | -ve end     | 06 | 05 | 00 | 20 |-> -ve
                | -ve next    | 06 | 05 | 04 | 00 |
                +---------------------------------+

*/

//  Macros --------------------------------------------------------------------

#ifndef ROTENCPI_H
#define ROTENCPI_H

// Simple state table.
#define SIMPLE_TABLE_COLS 16
#define SIMPLE_TABLE     { 0,-1, 1, 0, 1, 0, 0,-1,-1, 0, 0, 1, 0, 1,-1, 0 }

// Half step transition table.
#define HALF_TABLE_ROWS  6
#define HALF_TABLE_COLS  4
#define HALF_TABLE {{ 0x03, 0x02, 0x01, 0x00 },\
                    { 0x23, 0x00, 0x01, 0x00 },\
                    { 0x13, 0x02, 0x00, 0x00 },\
                    { 0x03, 0x05, 0x04, 0x00 },\
                    { 0x03, 0x03, 0x04, 0x10 },\
                    { 0x03, 0x05, 0x03, 0x20 }}

// Full step transition table.
#define FULL_TABLE_ROWS  7
#define FULL_TABLE_COLS  4
#define FULL_TABLE {{ 0x00, 0x02, 0x04, 0x00 },\
                    { 0x03, 0x00, 0x01, 0x10 },\
                    { 0x03, 0x02, 0x00, 0x00 },\
                    { 0x03, 0x02, 0x01, 0x00 },\
                    { 0x06, 0x00, 0x04, 0x00 },\
                    { 0x06, 0x05, 0x00, 0x20 },\
                    { 0x06, 0x05, 0x04, 0x00 }}


//  Data structures -----------------------------------------------------------

volatile int8_t encoderDirection;   // Encoder direction.
//volatile int8_t encoderState;       // Encoder state, abAB.
volatile int8_t buttonState;        // Button state, on or off.

// Decoder methods. See description of encoder functions below.
enum decode_t { SIMPLE_1, SIMPLE_2, SIMPLE_4, HALF, FULL };

struct encoderStruct
{
    uint8_t       gpioA; // GPIO for encoder pin A.
    uint8_t       gpioB; // GPIO for encoder pin B.
    uint16_t      delay; // Sensitivity delay (uS).
    enum decode_t mode;  // Simple, half or full quadrature.
}   encoder;

struct buttonStruct
{
    uint8_t gpio;   // GPIO for button pin.
}   button;

/*
    Functions to set direction in encoderDirection variable:
    encoderDirection = +1: +ve direction.
                     =  0: no change determined.
                     = -1: -ve direction.
*/
//  ---------------------------------------------------------------------------
//  Sets direction in encoderDirection according to state of pin B.
//  ---------------------------------------------------------------------------
void setDirectionSimple( void );

//  ---------------------------------------------------------------------------
//  Sets direction in encoderDirection using SIMPLE_TABLE.
//  ---------------------------------------------------------------------------
void setDirectionTable( void );

//  ---------------------------------------------------------------------------
//  Sets direction in encoderDirection using HALF_TABLE.
//  ---------------------------------------------------------------------------
void setDirectionHalf( void );

//  ---------------------------------------------------------------------------
//  Sets direction in encoderDirection using FULL_TABLE.
//  ---------------------------------------------------------------------------
void setDirectionFull( void );

//  ---------------------------------------------------------------------------
//  Returns button state in buttonState. Call by interrupt on GPIO.
//  ---------------------------------------------------------------------------
void setButtonState( void );

//  ---------------------------------------------------------------------------
//  Initialises encoder and button GPIOs.
//  ---------------------------------------------------------------------------
/*
    Send 0xFF for button if no GPIO present.
*/
void encoderInit( uint8_t encoderA, uint8_t encoderB, uint8_t button );

#endif
