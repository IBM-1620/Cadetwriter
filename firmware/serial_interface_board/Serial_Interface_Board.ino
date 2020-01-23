//======================================================================================================================
//
//  Program filename:   Serial_Interface_Board.ino
//
//  Description:        This file contains the firmware for the Cadetwriter.  A Wheelwriter-based computer terminal
//                      which can be either an IBM 1620 Jr. Console Typewriter, a general-purpose ASCII Terminal, or a
//                      Standalone Typewriter.
//
//  Project:            The IBM 1620 Jr Project.
//
//  Typewriter:         IBM/Lexmark Wheelwriter 1000, type 6781-024.
//
//  Interface boards:   WheelWriter USB Interface Board, v1.6, June 2018.  Retired.
//                      WheelWriter USB Interface Board, v1.7. May 2019.  Retired.
//                      WheelWriter Serial Interface Board, v2.0. July 2019.
//
//  Build environment:  Arduino IDE 1.8.10 (https://www.arduino.cc/en/main/software).
//                      Teensyduino 1.48 (https://www.pjrc.com/teensy/td_download.html).
//                      SlowSoftSerial library (https://github.com/MustBeArt/SlowSoftSerial)
//                      Compile options - Teensy 3.5, USB Serial, 120 MHz, Fastest with LTO.
//
//  Memory:             78448 bytes (14%) of program storage space.
//                      107988 bytes (41%) of dynamic memory for global variables.
//
//  Documentation:      IBM 1620 Jr. Console Typewriter Protocol, version 1.10, 5/24/2019.
//                      IBM 1620 Jr. Console Typewriter Test Program, version 1.10, 5/24/2019.
//
//  Authors:            Dave Babcock, Joe Fredrick
//
//  Copyright:          Copyright(C) 2019 Dave Babcock & Joe Fredrick
//
//  License:            This program is free software: you can redistribute it and/or modify it under the terms of the
//                      GNU General Public License as published by the Free Software Foundation, either version 3 of the
//                      License, or (at your option) any later version.
//
//                      This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
//                      without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
//                      the GNU General Public License for more details.
//
//                      You should have received a copy of the GNU General Public License along with this program. If
//                      not, see <http://www.gnu.org/licenses/> 
//
//  Revision History:   5R1  5/10/2019  Initial release of rewritten firmware.
//                      5R2  5/25/2019  Corrections to standalone typewriter emulation.
//                                      Removed the printing of the emulation banners.
//                                      Replaced ASCII print characters \, `, <, >, ^, and ~ with period graphics.
//                                      Added more error and warning checking.
//                                      Removed IBM 1620 Jr. initialize command code to be replaced with setup function.
//                                      Added ASCII Terminal options - auto LF, width.
//                                      Added IBM 1620 Jr. options - slash zero, bold.
//                      5R3  6/10/2019  Added IBM 1620 Jr. & ASCII Terminal software flow control.
//                                      Replaced ASCII print characters { and } with period graphics.
//                                      Added interactive setup functions for IBM 1620 Jr. & ASCII Terminal.
//                                      Added settings for RS-232 serial, but not support for it yet.
//                      5R4  8/30/2019  Renamed firmware & variables to reflect expanded capability.
//                                      Added support for v2.0 Serial Interface Board.
//                                      Added support for RS-232 serial.
//                                      Added margin, tab, uppercase, and emulation settings to EEPROM.
//                                      Added column offset setting to support column 1 period graphics.
//                                      Updated period graphic characters to correctly print in column 1.
//                                      Added reset all settings to factory defaults operation.
//                                      Expanded end-of-line handling on input and output.
//                                      Added option to ignore escape sequences.
//                                      Made MarRel, LMar, RMar, TSet, TClr control functions only.
//                                      Disabled unusable baud rates.
//                                      Cleaned up the printing of some special ASCII characters.
//                                      Added developer function to calibrate print string timings.
//                      5R5  11/8/2019  Added support for semi-automatic paper loading.
//                                      Increased time for ISR delay to deal with overlapping column scans.
//                                      Adjusted timing of unshifted, shifted, and code characters.
//                      5R6 11/12/2019  Corrected set of available baud rates.
//                      5R7 12/29/2019  Removed some unneeded timing dependencies.
//                      5R7ptw (TBD)    Adopted SlowSoftSerial library to allow slow baud rates.
//                      5R7ptw2 (TBD)   Restored handling of Enter to accept old baud rate.
//                                      Fixed handling of serial mode setting in EEPROM (refactored).
//
//  Future ideas:       1. Support other Wheelwriter models.
//
//                      2. Extend the developer feature to include keyboard bounce measurements, board tests, change
//                         typewriter settings (Bold, Caps, Center, Word & Continuous Underlining, Language, Line
//                         Spacings, Right Flush, Decimal Alignment, Indent, Spell Check, and PowerWise), etc.
//
//                      3. Implement a subset of ANSI Escape Sequences that make sense for a printing terminal and
//                         ignore the others.
//
//                      4. Add other terminal emulations such as EBCDIC, APL, DecWriter, etc.
//
//                      5. Add a means to use a USB memory stick as a paper tape device to allow the emulation of
//                         terminals such as an ASR-33 and Flexowriter.
//
//                      6. Add pagination support for individual sheets of paper or fan-fold paper.
//
//                      7. Add keyboard key which aborts current printout and flushes print & input buffers.
//
//                      8. Translate common Unicode characters (e.g., curly quotes) to nearest ASCII equivalents.
//
//======================================================================================================================

#include <EEPROM.h>
#include "SlowSoftSerial.h"

//======================================================================================================================
//
//  Principles of Operation
//
//  The Serial Interface Board, plus this firmware, turns a Wheelwriter 1000 electronic typewriter into a printing
//  computer terminal.  Two different computer terminals are currently supported - the IBM 1620 Jr. Console Typewriter
//  and a general-purpose ASCII terminal.  A third implemented emulation turns the device back into an stand-alone
//  Wheelwriter typewriter. There is one terminal emulation open for future expansion.
//
//  Overview - The Wheelwriter typewriter has a main logic board which scans its matrix keyboard and drives the print
//             mechanism.  This adaptation electrically interposes a Serial Interface Board between the logic board and
//             the physical keyboard.  The Serial Interface Board does the actual "reading" of the keyboard and injects
//             "fake" key presses to the logic board.  The interface board also communicates with a computer via a USB
//             or a RS-232 serial connection.  This firmware implements all of the logic to make the device behave like
//             the emulated terminal.
//
//  Conventions - The naming convention of this code is:
//                *  Named constants (#defines) and constant variables (const) are all (or mostly) upper case letters
//                   and numbers (ORANGE_LED_PIN, WW_STR_SPACE).  The exception is when naming keyboard keys (and their
//                   associated print characters and strings), the actual capitalization of the lettering on the key is
//                   used (WW_KEY_n_N_Caps, WW_PRINT_n).
//                *  Enumeration values have a common prefix (ACTION_PRINT, ACTION_SEND, ...).  Macros (#defines) are
//                   used in lieu of enum data types.
//                *  Common global variables are all lower case, complete words, and separated by underscores
//                   (print_buffer).
//                *  Function names are mixed upper / lower case, complete words, separated by underscores, and begin
//                   with an upper case letter (Take_action).  The only exceptions are the setup() and loop() functions
//                   whose names are required by the Arduino executive.
//                *  If a constant, variable, or function is specific to just one emulation, then it includes either
//                   "IBM", "ASCII", "FUTURE", or "STANDALONE" in its name (IBM_KEY_RELEASESTART, ASCII_STR_LESS,
//                   artn_IBM, Setup_FUTURE, Loop_STANDALONE).
//
//                The coding convention is:
//                *  Comments are used to provide clarity, not to state the obvious.
//                *  Code indents are 2 spaces.
//                *  Lines are 120 characters long.
//                *  Named constants, which are grouped together, are used instead of embedded numeric constants
//                   scattered throughout the code.
//
//  Like all Arduino sketches, this code is divided into three components:
//
//  1.  Setup routine - This routine, named setup(), is called once by the executive code at the very beginning of
//                      operation when the processor is first turned on or reset.  There is a single master setup()
//                      routine which performs common initialization for all typewriter emulations.  Based on
//                      which emulation is selected by jumper settings, the common code calls emulation-specific
//                      setup routines for specialized initialization.
//
//  2.  Loop routine - This routine, named loop(), is called repeatedly during operation by the executive code.  It is
//                     important that this routine not internally loop forever as some of the peripheral support is
//                     polled by the executive code during its outer loop.  There is a single master loop() routine
//                     which handles all emulations except the standalone typewriter.
//
//                     This can be considered the main program and is responsible for:
//                     a.  Receiving all characters from the computer via the USB or RS-232 interface.
//                     b.  Sending all characters generated by typing on the keyboard to the computer via the USB or
//                         RS-232 interface.
//                     c.  Processing all received command characters.
//                     d.  Processing all print characters transferred from the ISRs.
//                     e.  Building printer output strings for the ISRs to send to the Wheelwriter logic board.
//
//                     Execution of the loop routine is less time critical.
//
//  3.  Interrupt service routine(s) - These routines are given control when their registered interrupt occurs.  An ISR
//                                     will interrupt the main executing code (i.e. loop()), but is not itself
//                                     interruptable.  If another interrupt is triggered when already in an ISR, the new
//                                     interrupt is queued and its ISR executed when the first ISR completes.  All
//                                     emulations, except the standalone typewriter which has its own ISR, share a
//                                     common ISR which vectors to separate support routines for each "keyboard" column
//                                     scan triggered by the Wheelwriter's logic board.  Note that the same column scan
//                                     lines are used by the Serial Interface Board to both read the real keyboard and
//                                     inject "fake" keyboard key presses to the Wheelwriter logic board for printing.
//
//                                     The common ISR and column support routines are responsible for:
//                                     a.  Reading all keyboard input.
//                                     b.  Asserting "fake" key presses to the Wheelwriter logic board as determined by
//                                         the main program.
//                                     c.  Taking action based on keyboard input and the current typewriter mode.
//                                     d.  Transfering all send and/or print characters to the main program for
//                                         processing.
//
//                                     Execution of the ISRs is highly time critical.
//
//  Emulation selection - This firmware supports 4 different emulations, 3 of which are currently defined & implemented.
//                        The 4th is left open for users to create their own emulation.  The active emulation is
//                        selected by setting jumpers on the Serial Interface Board.
//
//                            J6 J7  Jumper settings.
//                            -------------------------------------------------
//                                   IBM 1620 Jr. Console Typewriter emulation.
//                                X  ASCII Terminal emulation.
//                             X     Reserved for future emulation.
//                             X  X  Standalone Typewriter emulation.
//
//  Synchronization - The Teensy 3.5 contains a single execution core - only one instruction can be executed at a time.
//                    The executive code does not allow the main program (i.e. loop()) to execute while in an ISR.
//                    Therefore, classic semaphores and mutexes cannot be used to guard critical code segments in the
//                    main program and ISRs.  If the main program were to hold a semaphore, an ISR could never acquire
//                    it, deadlocking the program.  This firmware is designed so that all data accesses are carefully
//                    sequenced, or never conflict, or have independent read / write pointers, or are atomic.  All
//                    global, non-constant data is declared volatile, so every read and write accesses memory and are
//                    not cached.
//
//  Column scans - Under normal operating conditions column scans take either 820 microseconds or 3.68 milliseconds.
//                 There are several cases:
//                 *  If no key is pressed within this column, then the scan takes 820 microseconds.
//                 *  If a key if pressed in this column and this is the first scan to see it, then the scan takes 3.68
//                    milliseconds.
//                 *  If a key is pressed in this column, but it has already been seen at least once, then the scan
//                    takes 820 microseconds.
//                 *  If a key in this column had been pressed but has been released since the last scan, then the scan
//                    takes 3.68 milliseconds.
//
//                 There are two exceptions to these column times:
//                 *  If the typewriter's character buffer is full, then the current column scan will be lengthened up
//                    to 2 seconds to allow printing to catch up.
//                 *  If the typewriter's logic board deems that too many vertical carriage movements and/or backspaces
//                    have happened, then it stretches the current scan up to 5.1 seconds.  Exact details of the
//                    conditions that trigger this slowdown are not known.
//
//                 The logic board is always scanning the columns, just the timing of each scan changes.  The order that
//                 columns are scanned in is fixed.  Which column is considered column 1 is arbitrary.  This program
//                 considers the column containing only the left and right shifts as column 1.  This makes the
//                 scheduling of print codes a little easier.
//
//                 *** WARNING ***  Never disable interrupts, even for one statement.  Doing so can result in faulty
//                                  printing.  It is critical that column scan interrupts are serviced immediately so
//                                  that any pending print code for that column can have its row line asserted within
//                                  the 265 microsecond time limit.  If that window is missed, the firmware will still
//                                  assert the row line, but the logic board won't recognize it.
//
//  Keyboard matrix - This table gives the keyboard matrix as processed by the firmware.  The first line in each
//                    box is the unshifted key, the second line is the shifted key, the third row is the "coded"
//                    key.  The 11 keys labelled [Xnn-mmm] represent keys not present on a Wheelwriter 1000 keyboard,
//                    but some or all of which are on other Wheelwriter models.  The nn represents the key's position on
//                    the keyboard [column, row] and mmm gives the scan information [column, row].
//
//                                                                 Column
//                    Row |       1       |       2       |    3     |      4      |  5   |      6       |   7   |
//                    ----+---------------+---------------+----------+-------------+------+--------------+-------+ ...
//                      1 | <left shift>  | <right arrow> |          |   <space>   | Code |              |       |
//                        |               |               |          |             |      |              |       |
//                        |               |     Word      |          | <req space> |      |              |       |
//                    ----+---------------+---------------+----------+-------------+------+--------------+-------+ ...
//                      2 | <right shift> | \          /  |    z     |<load paper> |      |      x       |   c   |
//                        |               |   [X33_022]   |    Z     |             |      |      X       |   C   |
//                        |               | /          \  |          |             |      | <power wise> |  Ctr  |
//                    ----+---------------+---------------+----------+-------------+------+--------------+-------+ ...
//                      3 |               |  \         /  |          | \         / |      |              |       |
//                        |               |   [X32_023]   |          |  [X13_043]  |      |              |       |
//                        |               |  /         \  |          | /         \ |      |              |       |
//                    ----+---------------+---------------+----------+-------------+------+--------------+-------+ ...
//                      4 |               |  Paper Down   |    q     |    L Mar    |      |      w       |   e   |
//                        |               |               |    Q     |             |      |      W       |   E   |
//                        |               |     Micro     |   Impr   |             |      |              |       |
//                    ----+---------------+---------------+----------+-------------+------+--------------+-------+ ...
//                      5 |               | \          /  |    1     | \         / |      |      2       |   3   |
//                        |               |   [X31_025]   |    !     |  [X12_045]  |      |      @       |   #   |
//                        |               | /          \  |   Spell  | /         \ |      |     Add      |  Del  |
//                    ----+---------------+---------------+----------+-------------+------+--------------+-------+ ...
//                      6 |               |   Paper Up    |    +/-   | \         / |      |              |       |
//                        |               |               | <degree> |  [X11_046]  |      |              |       |
//                        |               |     Micro     |          | /         \ |      |              |       |
//                    ----+---------------+---------------+----------+-------------+------+--------------+-------+ ...
//                      7 |               |     Reloc     |    a     | \         / |      |      s       |   d   |
//                        |               |               |    A     |  [X14_047]  |      |      S       |   D   |
//                        |               |  Line Space   |          | /         \ |      |              | Dec T |
//                    ----+---------------+---------------+----------+-------------+------+--------------+-------+ ...
//                      8 |               | <down arrow>  |          |    T Clr    |      |              |       |
//                        |               |               |          |             |      |              |       |
//                        |               |     Line      |          |             |      |              |       |
//                    ----+---------------+---------------+----------+-------------+------+--------------+-------+ ...
//
//
//                                                               Column
//                    Row |   8   |     9      |    10     |   11   |     12      |      13      |    14     |
//                    ----+-------+------------+-----------+--------+-------------+--------------+-----------+
//                      1 |   b   |     n      |           |        |      /      | <left arrow> |    <X     |
//                        |   B   |     N      |           |        |      ?      |              |           |
//                        | Bold  |    Caps    |           |        |             |     Word     |   Word    |
//                    ----+-------+------------+-----------+--------+-------------+--------------+-----------+
//                      2 |   v   |     m      |     ,     |   .    |             |  <up arrow>  |   Lock    |
//                        |   V   |     M      |     ,     |   .    |             |              |           |
//                        |       |            |           |        |             |     Line     |           |
//                    ----+-------+------------+-----------+--------+-------------+--------------+-----------+
//                      3 |   t   |     y      |     ]     |        |     1/2     |              |   R Mar   |
//                        |   T   |     Y      |     [     |        |     1/4     |              |           |
//                        |       |  <1/2 up>  | <super 3> |        |  <super 2>  |              |           |
//                    ----+-------+------------+-----------+--------+-------------+--------------+-----------+
//                      4 |   r   |     u      |     i     |   o    |      p      | \         /  |    Tab    |
//                        |   R   |     U      |     I     |   O    |      P      |  [X22_134]   |           |
//                        | A Rtn |    Cont    |   Word    | R Flsh |             | /         \  |   IndL    |
//                    ----+-------+------------+-----------+--------+-------------+--------------+-----------+
//                      5 |   4   |     7      |     8     |   9    |      0      | \         /  |           |
//                        |   $   |     &      |     *     |   (    |      )      |  [X21_135]   |           |
//                        |  Vol  |            |           |  Stop  |             | /         \  |           |
//                    ----+-------+------------+-----------+--------+-------------+--------------+-----------+
//                      6 |   5   |     6      |     =     |        |      -      |  Backspace   |  Mar Rel  |
//                        |   %   |   <cent>   |     +     |        |      _      |              |           |
//                        |       |            |           |        |             |    Bksp 1    |   RePrt   |
//                    ----+-------+------------+-----------+--------+-------------+--------------+-----------+
//                      7 |   f   |     j      |     k     |   l    |      ;      | \         /  |   T Set   |
//                        |   F   |     J      |     K     |   L    |      :      |  [X23_137]   |           |
//                        |       |            |           |  Lang  |  <section>  | /         \  |           |
//                    ----+-------+------------+-----------+--------+-------------+--------------+-----------+
//                      8 |   g   |     h      |           |        |      '      |    C Rtn     |\         /|
//                        |   G   |     H      |           |        |      "      |              | [X15_148] |
//                        |       | <1/2 down> |           |        | <paragraph> |   Ind Clr    |/         \|
//                    ----+-------+------------+-----------+--------+-------------+--------------+-----------+
//
//  Keyboard input - The common ISR and column support routines determine if a key has been pressed and if so,
//                   processes that key based on the current keyboard state.  All processing data is encoded in key
//                   action tables.  If a key triggers the printing and/or sending of characters, those characters are
//                   transferred to the main program for handling.  This is done to keep ISRs very small & fast and to
//                   avoid synchronization issues.  More details are described below with the ISR routines.
//
//  Printing - Printing is the most complex thing that this code does.  To accomplish it, "fake" key presses (done by
//             asserting row driver lines during the correct column scans) are sent to the typewriter's logic board in
//             the correct sequence with the correct timing.  The logic board thinks it is seeing key presses from the
//             physical keyboard and types the appropriate character or performs the requested operation (such as
//             setting a tab stop).
//
//             To keep the ISR and its support routines small, fast, and running at full printer speed, the complete
//             sequence of column/row print codes to be printed are stored in the print buffer.  When a column scan
//             interrupts, the ISR tests the next element in the print buffer to see if it pertains to the current
//             column.  If it doesn't, then no row drive lines are asserted during this column scan.  If it matches,
//             then the specified row lines are asserted during this column scan and the print buffer is advanced to the
//             next entry.  Note that a column/row print code may direct that no row driver lines are asserted during
//             the designated column scan.  This is done to signal a key release or to synchronize key pressing with
//             printer timing.
//
//             There are two special print codes used in the print buffer - null (value 0x0000) and skip (value 0xffff).
//             The null print code marks the current end of things to print in the print buffer.  The null value
//             effectively stops the ISR from advancing through the print buffer since it doesn't match any of the
//             valid column values.  When seen during any column scan, skip print codes are ignored and the print
//             buffer is advanced to the next print code.
//
//             The main program is responsible for filling the print buffer with print codes.  The characters to print
//             come from two sources - the main program processing input from the USB or RS-232 port via the receive
//             buffer and the ISR processing input from the keyboard via the transfer buffer.  For each character to be
//             printed, a carefully crafted sequence of 3 - 150 print codes, then "pad" null scan print codes, and
//             finally a null print code, is copied to the print buffer after the current end-of-buffer-data null print
//             code.  If there is enough room in the print buffer for all of the print codes of the character to be
//             printed, then the current end-of-buffer-data null will be overwritten with a skip print code by the main
//             program.  This effectively releases the new set of print codes to be seen and output by the ISR during
//             column scans.
//
//             The Teensy-based Serial Interface Board is capable of pumping print codes into the logic board much
//             faster than they can be printed.  The Wheelwriter has a small, 32 character buffer which can fill
//             quickly.  When it is almost full, the typewriter beeps three times.  When it is full, the logic board
//             stretches the current column scan pulse by as much as 2 seconds, stalling "keyboard" input while the
//             print mechanism catches up.  This is a safe thing to do as a stand-alone typewriter, but as a computer
//             terminal it can cause loss of input data from the actual keyboard.  This firmware tries hard to keep from
//             filling the typewriter's buffer by carefully balancing the time it takes to print a character and the
//             total column scan time that it takes to assert the row drive lines for all of the character's print
//             codes.
//
//             There are several types of "characters" that can be printed:
//             *  Standard typewriter characters which are present on the printwheel, such as 'A' or '$'.  These need
//                the smallest set of print codes to print.
//             *  Composite characters which are not present on the printwheel, such as the IBM 1620's record mark or
//                flagged digits.  These characters are synthensized by overprinting several standard characters with
//                appropriate print carriage movements (backspace, roll up/down 1/2 line, etc.).  These need larger sets
//                of print codes to print.
//             *  ASCII characters which are not present on the printwheel, such as the backslash.  These characters are
//                created using "period graphics" - a sequence of micro carriage movements and printing periods.  These
//                need the largest sets of print codes to print.
//             *  Wheelwriter function characters which trigger the logic board to do some special, internal operation
//                such as set tab stop, turn on/off spell checking, etc.  Like standard [printable] characters, these
//                need small sets of print codes to trigger.
//
//             Every print character has a unique data structure which gives all of the information and data needed to
//             print that character.  These data structures contain:
//             *  A spacing code which indicates how this character affects the horizontal position on the line.
//             *  A timing value which is the difference between the time it takes to physically print this character
//                (under normal conditions) and the time it takes to assert all of its print codes.
//             *  A pointer to the string of print codes for this character.
//             The main program queues a character for printing by first copying the character's print codes to the
//             print buffer.  Then it "pads out" the print buffer with as many empty full scan print codes as needed to
//             balance the timing of physical printing and print code scanning as given by an accumulation of the timing
//             value.  Then the current horizontal position is updated based on the character's spacing code.
//
//             The set of print codes for each print character follow these rigorous rules:
//             *  The print codes always assume they start with a column 1 scan.
//             *  The print codes define a number of complete column scans.  That is, the last print code is always in
//                column 14.
//             *  Each print code which actually prints a symbol is asserted for one and only one column scan.
//             *  When a shift (or code) is needed to print a symbol, the shift (or code) key is asserted at least one
//                column scan before and one column scan after the symbol's print code.
//             *  To avoid ghosting of the fake key presses, the left shift key is used with most shifted symbols, but
//                the right shift key is used for the B, N, and ? character.  Note that ghosting cannot be avoided with
//                the Word, Required Space, Bold, Caps, left Word, or delete Word characters, but the typewriter's
//                logic board can deal with these cases.
//
//             Here are the print codes, timing, and scan schedule for printing an 'm':
//               Print codes:  WW_m_M, WW_NULL_9, WW_NULL_14, WW_NULL
//               Timing:  TIME_CHARACTER - (2 * FSCAN_1_CHANGE)
//               Scan schedule:
//                           C1    C2    C3    C4    C5    C6    C7    C8    C9    C10   C11   C12   C13   C14
//                                                                            m
//                                                                          NULL9                         NULL14  NULL
// 
//             Here are the print codes, timing, and scan schedule for printing an 'M':
//               Print codes:  WW_LShift, WW_m_M, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL;
//               Timing:  TIME_CHARACTER - (2 * FSCAN_1_CHANGE + FSCAN_2_CHANGES)
//               Scan schedule:
//                           C1    C2    C3    C4    C5    C6    C7    C8    C9    C10   C11   C12   C13   C14
//                         LShift                                             M
//                         LShift                                          (null9)
//                         NULL1                                                                          NULL14  NULL
//
//             Here are the print codes, timing, and scan schedule for printing the IBM 1620's record mark character:
//               Print codes:  WW_EQUAL_PLUS, WW_NULL_10, WW_Backspace_Bksp1, WW_NULL_13, WW_LShift,
//                             WW_1_EXCLAMATION_Spell, WW_LShift, WW_NULL_1, WW_Backspace_Bksp1, WW_NULL_13,
//                             WW_i_I_Word, WW_NULL_10, WW_NULL_14, WW_NULL
//               Timing: (3 * TIME_CHARACTER + 2 * TIME_HMOVEMENT) - (6 * FSCAN_1_CHANGE + 3 * FSCAN_2_CHANGES))
//               Scan schedule:
//                           C1    C2    C3    C4    C5    C6    C7    C8    C9    C10   C11   C12   C13   C14
//                                                                                  =
//                                                                               NULL10            Backsp
//                                                                                                 NULL13
//                         LShift         !
//                         LShift      (null3)
//                         NULL1                                                                   Backsp
//                                                                                                 NULL13
//                                                                                  i
//                                                                               NULL10                   NULL14  NULL
//
//             Here are the print codes, timing, and scan schedule for printing the ASCII Terminal's '<' character:
//               Print codes:  WW_Code, WW_PaperDown_Micro, WW_Code, WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11,
//                             WW_Backspace_Bksp1, WW_NULL_13, WW_Code, WW_PaperDown_Micro, WW_Code, WW_Code,
//                             WW_PaperDown_Micro, WW_Code, WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11, WW_Backspace_Bksp1,
//                             WW_NULL_13, WW_Code, WW_Backspace_Bksp1, WW_Code, WW_Code, WW_PaperUp_Micro, WW_Code,
//                             WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11, WW_Code, WW_PaperUp_Micro, WW_Code, WW_Code,
//                             WW_PaperUp_Micro, WW_Code, WW_Code, WW_Backspace_Bksp1, WW_Code, WW_Code,
//                             WW_Backspace_Bksp1, WW_Code, WW_Code, WW_Backspace_Bksp1, WW_Code, WW_Code,
//                             WW_Backspace_Bksp1, WW_Code, WW_Code, WW_Backspace_Bksp1, WW_Code, WW_NULL_5,
//                             WW_SPACE_REQSPACE, WW_NULL_4, WW_NULL_14, WW_NULL
//               Timing:  ((3 * TIME_CHARACTER + 7 * TIME_HMOVEMENT + 6 * TIME_VMOVEMENT + 10 * TIME_ADJUST) -
//                         (29 * FSCAN_1_CHANGE + 4 * FSCAN_2_CHANGES + 3 * FSCAN_3_CHANGES))
//               Scan schedule:
//                           C1    C2    C3    C4    C5    C6    C7    C8    C9    C10   C11   C12   C13   C14
//                                                  Code
//                               DMicro             Code
//                                NULL2           (null5)                                 .
//                                                                                     NULL11
//                                                                                                 Backsp
//                                                                                                 NULL13
//                                                  Code
//                               DMicro             Code
//                              (null2)             Code
//                               DMicro             Code
//                                NULL2           (null5)                                 .
//                                                                                     NULL11
//                                                                                                 Backsp
//                                                                                                 NULL13
//                                                  Code                                            Bksp1
//                                                  Code                                          (null13)
//                               UMicro             Code
//                                NULL2           (null5)                                 .
//                                                                                     NULL11
//                                                  Code
//                               UMicro             Code
//                              (null2)             Code
//                               UMicro             Code
//                                                  Code                                            Bksp1
//                                                  Code                                          (null13)
//                                                  Code                                            Bksp1
//                                                  Code                                          (null13)
//                                                  Code                                            Bksp1
//                                                  Code                                          (null13)
//                                                  Code                                            Bksp1
//                                                  Code                                          (null13)
//                                                  Code                                            Bksp1
//                                                  Code
//                                                 NULL5
//                                           SPACE
//                                           NULL4                                                        NULL14  NULL
//
//  Period graphics - Two approaches are currently used to create printed characters that aren't on the Wheelwriter's
//                    printwheel.
//
//                    The first is to carefully overprint several available characters, often with some appropriate
//                    carriage movement.  This is used to create all of the IBM 1620's special characters and some of
//                    the ASCII characters.  For example, the 1620's slashed zero is formed by overprinting a zero with
//                    a slash.  Flagged characters are formed by overprinting the primary character, usually a digit
//                    with a hyphen after rolling the carriage down half a character height.  The goal is to make the
//                    Wheelwriter-based characters look as similar to the real IBM 1620's console typewriter output as
//                    possible.
//
//                    The second method is to create a look-alike character using "period graphics".  There are many
//                    limitations, but for most of the missing ASCII characters, this produces a reasonable result.  For
//                    example, replacement less than, greater than, and caret symbols can be printed using 3 periods.
//                          .     .       .
//                         .       .     . .
//                          .     .
//
//                    A third approach is to substitute an unused, but available character for a needed one.  This was
//                    done at first for some of the missing ASCII characters, but quickly replaced with period graphic
//                    characters.  Substitute characters don't convey the same meaning as the real character.  For
//                    example, using a plus-minus symbol in lieu of a backslash.
//
//                    The first limitation with period graphics is the size of the period itself, which is much larger
//                    than the lines of print characters.  This causes the characters formed by periods look darker and
//                    chunkier.
//
//                    A second limitation is the carriage movements available.  In addition to moving a full character
//                    left or right, the printer can move a half-character up and down and a micro space up, left, and
//                    down, but not right.
//
//                    The space occupied by each printed character breaks down into 6 micro-spaces horizontally and 8
//                    micro-spaces vertically.  Most printed characters occupy a 5x5 area within the 6 x 8 overall
//                    space, as shown in the first diagram below.  For example, the printed 'H' character fills the 5x5
//                    area as shown in the second diagram.  The printed period is natively in the center of the 5x5 area
//                    as in the third diagram.
//
//                            1  2  3  4  5   6             1  2  3  4  5   6             1  2  3  4  5   6
//                          +-------------------+         +-------------------+         +-------------------+
//                        1 |                   |       1 |                   |       1 |                   |
//                          +---------------+   |         +---------------+   |         +---------------+   |
//                        2 | X  X  X  X  X |   |       2 | H           H |   |       2 |               |   |
//                        3 | X  X  X  X  X |   |       3 | H           H |   |       3 |               |   |
//                        4 | X  X  X  X  X |   |       4 | H  H  H  H  H |   |       4 |               |   |
//                        5 | X  X  X  X  X |   |       5 | H           H |   |       5 |               |   |
//                        6 | X  X  X  X  X |   |       6 | H           H |   |       6 |       .       |   |
//                          +---------------+   |         +---------------+   |         +---------------+   |
//                        7 |                   |       7 |                   |       7 |                   |
//                        8 |                   |       8 |                   |       8 |                   |
//                          +-------------------+         +-------------------+         +-------------------+
//
//                    Creating a period graphic character involves combining explicit and implicit carriage movements
//                    with printing periods.  A character always starts in the "home" position and must end in the home
//                    position one character space to the right.  Every time a period is printed, the carriage is
//                    automatically moved one character position to the right and usually has to be moved back to the
//                    character being printed.  For example, here are the period graphic versions of the less than,
//                    backslash, and tilde ASCII characters.
//
//                            1  2  3  4  5   6             1  2  3  4  5   6             1  2  3  4  5   6
//                          +-------------------+         +-------------------+         +-------------------+
//                        1 |                   |       1 |                   |       1 |                   |
//                          +---------------+   |         +---------------+   |         +---------------+   |
//                        2 |               |   |       2 |               |   |       2 |               |   |
//                        3 |          .    |   |       3 | .             |   |       3 |    .          |   |
//                        4 |       .       |   |       4 |    .          |   |       4 | .     .     . |   |
//                        5 |          .    |   |       5 |       .       |   |       5 |          .    |   |
//                        6 |               |   |       6 |          .    |   |       6 |               |   |
//                          +---------------+   |         +---------------+   |         +---------------+   |
//                        7 |                   |       7 |                   |       7 |                   |
//                        8 |                   |       8 |                   |       8 |                   |
//                          +-------------------+         +-------------------+         +-------------------+
//
//                    Here are the print sequences for each of the above characters:
//                      Less - Paper Down Micro, Paper Down Micro, PERIOD, Paper Down Micro, Bksp1, Bksp1, Bksp1, Bksp1,
//                             Bksp1, PERIOD, Backspace, Paper Up Micro, Paper Up Micro, PERIOD, Paper Up Micro, Bksp1
//                      Backslash - Paper Down Micro, PERIOD, Backspace, Bksp1, Paper Down Micro, PERIOD, Backspace,
//                                  Bksp1, Paper Down Micro, PERIOD, Bksp1, Bksp1, Bksp1, Paper Up Micro, Paper Up
//                                  Micro, Paper Up Micro, PERIOD, Bksp1.
//                      Tilde - Paper Down Micro, Paper Down Micro, PERIOD, Backspace, Bksp1, Paper Down Micro, PERIOD,
//                              Backspace, Bksp1, Paper Up Micro, PERIOD, Bksp1, Bksp1, PERIOD, Backspace, Bksp1, Paper
//                              Up Micro, PERIOD, Bksp1.
//                    Note the unusual ordering of printing periods.  Because there is no micro space right, the optimal
//                    order of printing in the micro-columns is:  2, 1, 5, 4, 3.  This ordering requires the fewest
//                    Backspace and Bksp1 operations.
//
//                    There are several special considerations with period graphics.
//                      *  The Wheelwriter logic board sometimes doesn't execute an implicit or explicit space,
//                         backspace, or micro backspace.  It is consistent when it does it, but the reason it does it
//                         isn't known.  If testing a new period graphic incorrectly spaces, then additional spacing
//                         command(s) need be inserted in the print sequence.
//                      *  Sometimes, two adjacent instances of the same character won't space correctly, but the
//                         character will print correctly when surrounded by other characters.  Rearranging the movement
//                         operations can avoid this problem.
//                      *  The period is horizontally in the center of the character "block".  In order to print in
//                         micro-columns 1 and 2, the carriage needs to move to the left.  This is not possible when the
//                         carriage is in the true column 1 position and up against the left stop.  So, the column
//                         offset setting, which is TRUE by default, relocates all columns one print position to the
//                         right.  The affected period graphic characters need appropriate MarRel operations to move
//                         into micro-columns 1 and 2.  This allows period graphic characters printed in column "1"
//                         (physical column 2) to print correctly.  Note that the column marker on the front of the
//                         typewriter will be off by one column, but otherwise everything will work correctly.
//                      *  Period graphic characters take more time to print and have longer print code strings.  This
//                         can cause the logic board's input buffer to fill up and trigger a [very] long column scan,
//                         slowing down printing even more.  Rearranging the order of print codes or inserting null
//                         full scans can avoid this problem.
//                    Even with the special cases, period graphic characters are a better choice than substitute or
//                    overprinted characters.
//
//  Print position - This firmware is responsible for keeping track of the current print position on the line.  This is
//                   because the automatic-return-at-the-right-margin functionality of a computer terminal must be done
//                   by this code - the Wheelwriter's ARtn feature doesn't work as needed.  Also, the Wheelwriter is an
//                   electronic typewriter and all margin setting & clearing and tab setting & clearing functions must
//                   be done by pressing keys, not by mechanical operations.  So, the main program tracks horizontal
//                   movements and defines special typewriter keys to manage the margins and tabs.
//
//  Buffers - The primary communication between the main program and the common ISR (and its column support routines) is
//            via a set of circular data buffers.  This is done for these reasons:
//                *  To keep the interrupt handler small and fast by passing all time-consuming processing off to the
//                   main program.
//                *  To implement synchronization and avoid the need for semaphores.  All buffer data access and control
//                   is done by simple loads and stores.
//
//            Each buffer consists of 4 variables:
//                *_buffer  A data array which contains the circular buffer itself.
//                *_read    An index into the buffer array of the next element to read.
//                *_write   An index into the buffer array of the next element to write (receive, send, and transfer
//                          buffers) or the last element written (print buffer).
//                *_count   A count of the current number of items in the buffer.
// 
//            Each buffer has only one reader (main program or ISR) which uses and updates the *_read pointer and one
//            writer (main program or ISR) which uses and updates the *_write pointer.  Each buffer's shared content
//            counter is updated via an atomic increment/decrement operation.
//
//                Buffer    Writer  Reader   Notes
//                ----------------------------------------------------------------------------------------------
//                command     ISR    main    Used to store setup command characters that the ISR wants the main
//                                           program to process.
//                receive    main    main    Used to temporarily store incoming characters from the computer for
//                                           (slightly) later processing by the main program.
//                send        ISR    main    Used to store characters that the ISR wants the main program to
//                                           send to the computer.
//                transfer    ISR    main    Used to store characters that the ISR wants the main program to
//                                           print.
//                print      main     ISR    Used to store individual column/row print codes (fake key presses)
//                                           that the ISR injects.
//
//            The order of operations on the buffers is critical to maintain their semaphore-free integrity.  When
//            writing, first test that there is space available using the *_count variable, then add the new item to
//            the buffer array at position *_write (or *_write + 1 for the print buffer), then increment the *_write
//            index [handling wrap-around], then increment the *_count variable.  When reading, first test that an item
//            is available using *_count, then read the item from the buffer at position *_read, then increment *_read
//            [handling wrap-around], then decrement the *_count variable.
//
//            For the receive, send, and transfer buffers items are single elements in the buffer array.  The print
//            buffer contains sets of elements (column/row print codes), each representing a single logical print
//            character (print string).  Print strings are added all-or-nothing to the print buffer.
//
//  EEPROM - The Teensy 3.5 contains 4,096 bytes of EEPROM.  The firmware uses it to store emulation settings that are
//           persistent.  The first byte is a "fingerprint" value which is used to identify a virgin EEPROM, so that it
//           can be properly initialized.  Stored data includes:
//               *  Emulation setting (EMULATION_IBM, EMULATION_ASCII, EMULATION_FUTURE, EMULATION_STANDALONE) used by
//                  all emulations.  Initial value = <depends on hardware jumpers>.
//               *  Report errors setting (TRUE, FALSE) used by all emulations.  Initial value = TRUE.
//               *  Report warnings setting (TRUE, FALSE) used by all emulations.  Initial value = FALSE.
//               *  Battery installed setting (TRUE, FALSE) used by all emulations.  Initial value = FALSE
//               *  Left margin setting (1 - 164) used by all emulations.  Initial value = 1;
//               *  Right margin setting (2 - 165) used by all emulations.  Initial value = 80;
//               *  Tab array[166] setting (TRUE, FALSE) used by all emulations.  Initial value:
//                                                                                IBM 1620 Jr. = all FALSE,
//                                                                                ASCII Terminal = TRUE every 8 columns.
//               *  Slashed zeroes setting (TRUE, FALSE) of the IBM 1620 Jr. Console Typewriter.  Initial value = TRUE.
//               *  Print bold input setting (TRUE, FALSE) of the IBM 1620 Jr. Console Typewriter.
//                  Initial value = FALSE.
//               *  Serial setting (USB, RS232) of the ASCII Terminal.  Initial value = USB.
//               *  Duplex setting (FULL, HALF) of the ASCII Terminal.  Initial value = FULL.
//               *  Baud setting (300 - 4608000) of the ASCII Terminal (RS-232 only).  Initial value = 9600.
//               *  Parity setting (NONE, ODD, EVEN) of the ASCII Terminal (USB only).  Initial value = NONE.
//               *  Databits, parity, stopbits setting (7O1, 7E1, 8N1, 8O1, 8E1, 8N2, 8O2, 8E2) of the ASCII Terminal
//                  (RS-232 only).  Initial value = 8N1.
//               *  Software flow control setting (NONE, XON_XOFF) of the ASCII Terminal.  Initial value = XON_XOFF.
//               *  Hardware flow control setting (NONE, RTS_CTS, RTR_CTS) of the ASCII Terminal.  Initial value = TRUE.
//               *  Uppercase only setting (TRUE, FALSE) of the ASCII Terminal.  Initial value = FALSE.
//               *  Auto return setting (TRUE, FALSE) of the ASCII Terminal.  Initial value = TRUE.
//               *  Transmit EOL setting (EOL_CR, EOL_CRLF, EOL_LF, EOL_LFCR) of the ASCII TERMINAL.
//                  Initial value = EOL_CR.
//               *  Receive EOL setting (EOL_CR, EOL_CRLF, EOL_LF, EOL_LFCR) of the ASCII TERMINAL.
//                  Initial value = EOL_CRLF.
//               *  Ignore escape sequences setting (TRUE, FALSE).  Initial value = TRUE.
//               *  Paper width setting (80 - 165) of the ASCII Terminal.  Initial value = 80.
//               *  Column offset setting (TRUE, FALSE) of the ASCII Terminal.  Initial value = TRUE.
//
//  Tabled logic - This program uses "tabled logic" where much of the program's logic is embedded in decision tables and
//                 hybrid FSA [Finite State Automata] tables.  For example, rather than have a set of individual status
//                 variables and code to deal with the keyboard state, shifting, I/O mode, half/duplex, etc. there are
//                 a set of key_action tables, each indexed by the current key press.  Changing state amounts to just
//                 changing one variable which points to the current key_action table.  Each table entry contains which
//                 character to send to the computer (if any), which character to print (if any), and what short,
//                 special action to take (such as switching to a different key_action table).
//
//                 Likewise, each emulation has a serial_action table, indexed by characters received from the computer
//                 via the USB or RS-232 port.  Each entry specifies what short action to take and, if printing is
//                 indicated, the print character to use.
//
//  Inlined functions - This code includes a set of short support routines which are explicitly inlined into the main
//                      code.  By design, most of the arguments to these routines are constants which enables heavy
//                      optimization to the inlined instances.
//
//  Blind operations - In the Wheelwriter typewriter, the connection between the keyboard and the logic board [where the
//                     Serial Interface Board is interposed] is effectively one-way.  That is, key presses can be
//                     asserted to the logic board, but no data can flow out of the logic board.  There is no way that
//                     the keyboard or the Serial Interface Board can query the logic board state or verify operation.
//                     This means that all typewriter operations, triggered by column/row print codes, are done blind.
//
//                     What makes matters worse is that many of the typewriter's options are toggles and there is no way
//                     to definitely set or clear them.  With the typewriter's backup batteries installed, the state of
//                     most options is maintained whether the typewriter is on or off, plugged in or not.  However, if
//                     the batteries are removed or dead, then whether the option states are retained depends on if the
//                     typewriter is plugged in or not, not simply if it is on or off.  This all means that when the
//                     firmware is started, it has no way of knowing the current state of some of the typewriter
//                     settings.
//
//                     On startup the firmware sets, directly or indirectly, as many settings as it can.  For example,
//                     the shift lock setting can always be cleared by virtually pressing and releasing one of the shift
//                     keys.  Power wise mode can always be directly turned off and all tab stops cleared. However, to
//                     set [or possibly reset] the margins, the firmware returns the carriage, asserts margin release,
//                     backspaces 20 positions [in case the left margin had been previously moved], and sets the left
//                     margin.  It then spaces (width - 1) positions to the right [the typewriter never stops at the
//                     right margin, it only beeps when getting close] and sets the right margin.  It is most likely
//                     that the margins were already set to 1 and width, but these startup actions will guarantee it.
//                     Other options (like line spacing) are assumed to be in their "cold start" [no batteries, just
//                     plugged in, and turned on] state.
//
//  Errors and warnings - The Serial Interface Board has limited ability to indicate errors and warnings when they
//                        occur.  Errors are conditions where the firmware cannot operate correctly such as overflowing
//                        the receive buffer.  When enabled, flow control (hardware or software) signals the computer to
//                        stop sending characters, but if it persists (or flow control is disabled), and too many
//                        characters are sent, then the excess ones are discarded.  Warnings are cases where something
//                        is amiss, but doesn't affect correct operation of the firmware, such as the Wheelwriter's
//                        logic board asserting an extra long column scan to catch up printing.  This causes a delay in
//                        printing, but characters aren't lost.
//
//                        When the firmware is started or restarted, all errors and warnings are cleared and the
//                        indicator LEDs are turned off.
//
//                        When an error occurs, the orange LED light on the Teensy board is turned on.  A count is made
//                        in an internal array by error type.  The error counts can be printed, and optionally cleared,
//                        using the setup function of the emulations.
//
//                        When a warning occurs, the blue LED light on the Serial Interface Board is turned on.  A count
//                        is made in an internal array by warning type.  The warning counts can be printed, and
//                        optionally cleared, using the setup function of the emulations.
//
//  Semi-automatic paper loading - The Wheelwriter is able to semi-automatically load paper.  This operation is
//                                 triggered by moving the paper bail all the way forward, holding it until the carriage
//                                 starts moving, then releasing it.  The carriage will move to the center of the line,
//                                 the paper advances approximately 2.5 - 12 inches (depending on where the top-of-form
//                                 is set), and then the carriage returns to the left margin.  This takes 3 - 9 seconds
//                                 and should only be done when nothing is printing.
//
//                                 The paper bail microswitch is wired directly across column 4 and row 2 on the logic
//                                 board's keyboard input.  Because of this, the serial interface board knows nothing
//                                 about the load paper operation to pause printing or reset the current line position.
//                                 There is an optional hardware modification to the serial interface board, documented
//                                 on the Cadetwriter Slack "building" message board, which allows this firmware to
//                                 detect and process the switch closure.
//
//  Setup function - All emulations support a manual setup function.  It is initiated by pressing the Code/Ctrl key and
//                   "Setup" keys on the keyboard.  The "Setup" key is the upper-rightmost key, labelled "Paper Up
//                   Micro" on an unmodified Wheelwriter.  The setup function allows operating parameters of the
//                   typewriter and/or emulation to be changed, the emulator's character set to be printed, and error &
//                   warning counts to printed and optionally reset.  Settings are viewed and changed interactively.
//                   For each setting, the options are presented and the current value is in uppercase.  Typing the
//                   first letter of an option selects it, pressing the carriage return retains the current value.
//                   There are a few exceptions when more than one character must be typed to uniquely identify the
//                   option value, such as the ASCII Terminal's RS-232 baud rate setting which can require two or three
//                   digits be typed.  New settings pertain only to the current session unless explicitly saved.
//
//                   There is also a "restore to factory defaults" option in all emulations.  It is triggered by
//                   pressing either of the Shift keys with the Code/Ctrl key and "Setup" key on the keyboard.
//
//  RS232 serial - The Teensy 3.5 microcontroller includes UART hardware support for 6 RS232 interfaces.  The Serial
//                 Interface Board uses the first RS232 port (UART0) for host computer communication.  There are
//                 limitations to the baud rates supported.  The following table gives the available baud rates and
//                 their error.  Only baud rates with an error of less than 2.5% are usable.  In addition, the MAX3232
//                 serial line driver/receiver cannot handle baud rates greater than 250000.  So, only baud rates 1200
//                 thru 230400 can be used with the hardware UART.
//
//                              Baud    Error    |    Baud    Error    |     Baud    Error
//                            -------------------+---------------------+-------------------
//                                50  2341.42%   |     600   102.59%   |    38400     0.00%
//                                75  1564.61%   |    1200     0.00%   |    57600     0.01%
//                               110  1266.47%   |    1800     0.00%   |    76800     0.00%
//                               134  1120.71%   |    2400     0.00%   |   115200     0.02%
//                               150   815.53%   |    4800     0.00%   |   230400    -0.03%
//                               200   510.36%   |    9600     0.00%   |   460800    -0.03%
//                               300   205.18%   |   19200     0.00%   |   921600     0.16%
//
//                 In order to provide baud rates from 50 to 600, the SlowSoftSerial library is used to bit-bang
//                 the serial data on the same pins.
//
//  Board revisions - This firmware automatically supports all revisions of the Serial Interface Board.
//                    1.6: The first production board.  The basic board has a hardware problem with random characters
//                         being printed and random carriage movements when new firmware is downloaded and at power on.
//                         To correct the problem, two pull-down resistor packs need to be added to the underneath side
//                         of the board.  It directly supports the USB interface and an RS-232 interface via an external
//                         level shifter.  This board is identified by BLUE_LED_PIN == HIGH.  Setting the ROW_ENABLE_PIN
//                         has no effect.  This board has been retired with the release of the v1.7 board.
//                    1.7: The second production board.  It resolves the random characters problem a different way and 
//                         requires no pull-down resistors.  It directly supports the USB interface and an RS-232
//                         interface via an external level shifter.  This board is identified by BLUE_LED_PIN == HIGH.
//                         It requires that the ROW_ENABLE_PIN be set LOW to enable fake key presses to be seen by the
//                         logic board.
//                    2.0: The latest production board.  This board directly supports both the USB and RS-232 serial
//                         interfaces.  It also has Hardware support for a 15th keyboard column scan line.  This board
//                         is identified by BLUE_LED_PIN == LOW.  It requires that the ROW_ENABLE_PIN be set LOW to
//                         enable fake key presses to be seen by the logic board.
// 
//======================================================================================================================



//======================================================================================================================
//
//  Common data and code used by all emulations.
//
//======================================================================================================================

//**********************************************************************************************************************
//
//  Global definitions.
//
//**********************************************************************************************************************

// Firmware version values.
#define VERSION       57
#define VERSION_TEXT  "5R7ptw2"

// Physical I/O port pins.
#define PTC_5   13  // Embedded Teensy 3.5 board orange LED.
#define PTC_9   36  // Logic board row drive enable.  Not used with the v1.6 Serial Interface Boards.
#define PTE_24  33  // Serial Interface Board blue LED.
#define PTE_26  24  // Serial Interface Board configuration jumper J6.
#define PTE_25  34  // Serial Interface Board configuration jumper J7.
#define PTB_16   0  // Serial Interface Board UART RX.
#define PTB_17   1  // Serial Interface Board UART TX.
#define PTC_10  37  // Serial Interface Board serial RTS (v1.6/v1.7) or row drive J14-10 (v2.0).
#define PTC_11  38  // Serial Interface Board serial CTS (v1.6/v1.7) or row drive J14-11 (v2.0).

// Physical keyboard column monitor pins.
#define PTC_0   15  // Column flat flex J13-2.
#define PTC_1   22  // Column flat flex J13-3.
#define PTC_2   23  // Column flat flex J13-4.
#define PTC_3    9  // Column flat flex J13-5.
#define PTC_4   10  // Column flat flex J13-6.
#define PTC_6   11  // Column flat flex J13-7.
#define PTC_7   12  // Column flat flex J13-8.
#define PTC_8   35  // Column flat flex J13-9.
#define PTA_12   3  // Column flat flex J13-10.
#define PTA_13   4  // Column flat flex J13-11.
#define PTA_14  26  // Column flat flex J13-12.
#define PTA_15  27  // Column flat flex J13-13.
#define PTA_16  28  // Column flat flex J13-14.
#define PTA_17  39  // Column flat flex J13-15.

// Physical keyboard row monitor pins.
#define PTD_0   2   // Row flat flex KYBD-8.
#define PTD_1  14   // Row flat flex KYBD-9.
#define PTD_2   7   // Row flat flex KYBD-10.
#define PTD_3   8   // Row flat flex KYBD-11.
#define PTD_4   6   // Row flat flex KYBD-12.
#define PTD_5  20   // Row flat flex KYBD-13.
#define PTD_6  21   // Row flat flex KYBD-14.
#define PTD_7   5   // Row flat flex KYBD-15.

// Physical logic board row drive pins.
#define PTB_0   16  // Logic board row drive J14-8.
#define PTB_1   17  // Logic board row drive J14-9.
#define PTB_2   19  // Logic board row drive J14-10 (v1.6/v1.7) or RTS (v2.0).
#define PTB_3   18  // Logic board row drive J14-11 (v1.6/v1.7) or CTS (v2.0).
#define PTB_10  31  // Logic board row drive J14-12.
#define PTB_11  32  // Logic board row drive J14-13.
#define PTB_18  29  // Logic board row drive J14-14.
#define PTB_19  30  // Logic board row drive J14-15.

// Logical I/O port pins.
#define ORANGE_LED_PIN   PTC_5   // Orange LED on Teensy pin.
#define BLUE_LED_PIN     PTE_24  // Blue LED on Serial Interface Board pin.
#define JUMPER_6_PIN     PTE_26  // Jumper 6 pin.
#define JUMPER_7_PIN     PTE_25  // Jumper 7 pin.
#define ROW_ENABLE_PIN   PTC_9   // Row drive enable pin.
#define UART_RX_PIN      PTB_16  // UART RX pin.
#define UART_TX_PIN      PTB_17  // UART TX pin.
#define SERIAL_RTS_PIN   PTC_10  // Serial RTS pin.
#define SERIAL_RTSX_PIN  PTB_2   // Alternate serial RTS pin for v2.0 Serial Interface Board.
#define SERIAL_CTS_PIN   PTC_11  // Serial CTS pin.
#define SERIAL_CTSX_PIN  PTB_3   // Alternate serial CTS pin for v2.0 Serial Interface Board.

// Logical keyboard column monitor pins.
#define COL_1_PIN   PTC_2   // Column 1 pin.
#define COL_2_PIN   PTA_16  // Column 2 pin.
#define COL_3_PIN   PTC_3   // Column 3 pin.
#define COL_4_PIN   PTC_0   // Column 4 pin.
#define COL_5_PIN   PTA_17  // Column 5 pin.
#define COL_6_PIN   PTC_4   // Column 6 pin.
#define COL_7_PIN   PTC_6   // Column 7 pin.
#define COL_8_PIN   PTC_7   // Column 8 pin.
#define COL_9_PIN   PTC_8   // Column 9 pin.
#define COL_10_PIN  PTA_12  // Column 10 pin.
#define COL_11_PIN  PTA_13  // Column 11 pin.
#define COL_12_PIN  PTA_14  // Column 12 pin.
#define COL_13_PIN  PTA_15  // Column 13 pin.
#define COL_14_PIN  PTC_1   // Column 14 pin.

// Logical keyboard row monitor pins.
#define ROW_IN_1_PIN  PTD_0  // Keyboard row 1 input pin.
#define ROW_IN_2_PIN  PTD_1  // Keyboard row 2 input pin.
#define ROW_IN_3_PIN  PTD_2  // Keyboard row 3 input pin.
#define ROW_IN_4_PIN  PTD_3  // Keyboard row 4 input pin.
#define ROW_IN_5_PIN  PTD_4  // Keyboard row 5 input pin.
#define ROW_IN_6_PIN  PTD_5  // Keyboard row 6 input pin.
#define ROW_IN_7_PIN  PTD_6  // Keyboard row 7 input pin.
#define ROW_IN_8_PIN  PTD_7  // Keyboard row 8 input pin.

// Logical logic board row drive pins.
#define ROW_OUT_NO_PIN  0        // Keyboard row no ouput pin.
#define ROW_OUT_1_PIN   PTB_0    // Keyboard row 1 output pin.
#define ROW_OUT_2_PIN   PTB_1    // Keyboard row 2 output pin.
#define ROW_OUT_3_PIN   PTB_2    // Keyboard row 3 output pin.
#define ROW_OUT_3X_PIN  PTC_10   // Alternate keyboard row 3 output pin for v2.0 Serial Interface Board.
#define ROW_OUT_4_PIN   PTB_3    // Keyboard row 4 output pin.
#define ROW_OUT_4X_PIN  PTC_11   // Alternate keyboard row 4 output pin for v2.0 Serial Interface Board.
#define ROW_OUT_5_PIN   PTB_10   // Keyboard row 5 output pin.
#define ROW_OUT_6_PIN   PTB_11   // Keyboard row 6 output pin.
#define ROW_OUT_7_PIN   PTB_18   // Keyboard row 7 output pin.
#define ROW_OUT_8_PIN   PTB_19   // Keyboard row 8 output pin.

// Wheelwriter column values.
#define WW_COLUMN_NULL  0x00  // Null column.
#define WW_COLUMN_1     0x01  // <left shift>, <right shift>
#define WW_COLUMN_2     0x02  // <right arrow>, <right word>, Paper Down, <down micro>, Paper Up, <up micro>, Reloc,
                              // Line Space, <down arrow>, <down line>
#define WW_COLUMN_3     0x03  // z, Z, q, Q, Impr, 1, !, Spell, +/-, <degree>, a, A
#define WW_COLUMN_4     0x04  // <space>, <required space>, <load paper>, L Mar, T Clr
#define WW_COLUMN_5     0x05  // Code
#define WW_COLUMN_6     0x06  // x, X, <power wise>, w, W, 2, @, Add, s, S
#define WW_COLUMN_7     0x07  // c, C, Ctr, e, E, 3, #, Del, d, D, Dec T
#define WW_COLUMN_8     0x08  // b, B, Bold, v, V, t, T, r, R, A Rtn, 4, $, Vol, 5, %, f, F, g, G
#define WW_COLUMN_9     0x09  // n, N, Caps, m, M, y, Y, <1/2 up>, u, U, Cont, 7, &, 6, <cent>, j, J, h, H, <1/2 down>
#define WW_COLUMN_10    0x0a  // ,, ], [, <superscript 3>, i, I, Word, 8, *, =, +, k, K
#define WW_COLUMN_11    0x0b  // ., o, O, R Flsh, 9, (, Stop, l, L, Lang
#define WW_COLUMN_12    0x0c  // /, ?, <half>, <quarter>, <superscript 2>, p, P, 0, ), -, _, ;, :, <section>, ', ",
                              // <paragraph>
#define WW_COLUMN_13    0x0d  // <left arrow>, <left word>, <up arrow>, <up line>, Backspace, Bksp 1, C Rtn, Ind Clr
#define WW_COLUMN_14    0x0e  // <back X>, <back word>, Lock, R Mar, Tab, IndL, Mar Rel, RePrt, T Set

// Wheelwriter row values.
#define WW_ROW_NULL  0X00  // Null row.
#define WW_ROW_1     0x01  // <left shift>, <right arrow>, <right word>, <space>, <required space>, Code, b, B, Bold, n,
                           // N, Caps, /, ?, <left arrow>, <left word>, <back X>, <back word>
#define WW_ROW_2     0x02  // <right shift>, z, Z, <load paper>, x, X, <power wise>, c, C, Ctr, v, V, m, M, ,, .,
                           // <up arrow>, <up line>, Lock
#define WW_ROW_3     0x03  // t, T, y, Y, <1/2 up>, ], [, <superscript 3>, <half>, <quarter>, <superscript 2>, R Mar
#define WW_ROW_4     0x04  // Paper Down, <down micro>, q, Q, Impr, L Mar, w, W, e, E, r, R, A Rtn, u, U, Cont, i, I,
                           // Word, o, O, R Flsh, p, P, Tab, IndL
#define WW_ROW_5     0x05  // 1, !, Spell, 2, @, Add, 3, #, Del, 4, $, Vol, 7, &, 8, *, 9, (, Stop, 0, )
#define WW_ROW_6     0x06  // Paper Up, <up micro>, +/-, <degree>, 5, %, 6, <cent>, =, +, -, _, Backspace, Bksp 1, Mar
                           // Rel, RePrt
#define WW_ROW_7     0x07  // Reloc, Line Space, a, A, s, S, d, D, Dec T, f, F, j, J, k, K, l, L, Lang, ;, :, <section>,
                           // T Set
#define WW_ROW_8     0x08  // <down arrow>, <down line>, TClr, g, G, h, H, <1/2 down>, ', ", <paragraph>, C Rtn, Ind Clr

// Wheelwriter key values.
#define WW_KEY_LShift                       0  // <left shift> key code.
#define WW_KEY_RShift                       1  // <right shift> key code.
#define WW_KEY_RARROW_Word                  2  // <right arrow>, <right word> key code.
#define WW_KEY_X33_22                       3  // *** not available on Wheelwriter 1000.
#define WW_KEY_X32_23                       4  // *** not available on Wheelwriter 1000.
#define WW_KEY_PaperDown_Micro              5  // Paper Down, <down micro> key code.
#define WW_KEY_X31_25                       6  // *** not available on Wheelwriter 1000.
#define WW_KEY_PaperUp_Micro                7  // Paper Up, <up micro> key code.
#define WW_KEY_Reloc_LineSpace              8  // Reloc, Line Space key code.
#define WW_KEY_DARROW_Line                  9  // <down arrow>, <down line> key code.
#define WW_KEY_z_Z                         10  // z, Z key code.
#define WW_KEY_q_Q_Impr                    11  // q, Q, Impr key code.
#define WW_KEY_1_EXCLAMATION_Spell         12  // 1, !, Spell key code.
#define WW_KEY_PLUSMINUS_DEGREE            13  // +/-, <degree> key code.
#define WW_KEY_a_A                         14  // a, A key code.
#define WW_KEY_SPACE_REQSPACE              15  // <space>, <required space> key code.
#define WW_KEY_LOADPAPER                   16  // <load paper> key code.
#define WW_KEY_X13_43                      17  // *** not available on Wheelwriter 1000.
#define WW_KEY_LMar                        18  // L Mar key code.
#define WW_KEY_X12_45                      19  // *** not available on Wheelwriter 1000.
#define WW_KEY_X11_46                      20  // *** not available on Wheelwriter 1000.
#define WW_KEY_X14_47                      21  // *** not available on Wheelwriter 1000.
#define WW_KEY_TClr                        22  // T Clr key code.
#define WW_KEY_Code                        23  // <code> key code.
#define WW_KEY_x_X_POWERWISE               24  // x, X, <power wise> key code.
#define WW_KEY_w_W                         25  // w, W key code.
#define WW_KEY_2_AT_Add                    26  // 2, @, Add key code.
#define WW_KEY_s_S                         27  // s, S key code.
#define WW_KEY_c_C_Ctr                     28  // c, C, Ctr key code.
#define WW_KEY_e_E                         29  // e, E key code.
#define WW_KEY_3_POUND_Del                 30  // 3, #, Del key code.
#define WW_KEY_d_D_DecT                    31  // d, D, Dec T key code.
#define WW_KEY_b_B_Bold                    32  // b, B, Bold key code.
#define WW_KEY_v_V                         33  // v, V key code.
#define WW_KEY_t_T                         34  // t, T key code.
#define WW_KEY_r_R_ARtn                    35  // r, R, A Rtn key code.
#define WW_KEY_4_DOLLAR_Vol                36  // 4, $, Vol key code.
#define WW_KEY_5_PERCENT                   37  // 5, % key code.
#define WW_KEY_f_F                         38  // f, F key code.
#define WW_KEY_g_G                         39  // g, G key code.
#define WW_KEY_n_N_Caps                    40  // n, N, Caps key code.
#define WW_KEY_m_M                         41  // m, M key code.
#define WW_KEY_y_Y_12UP                    42  // y, Y, <1/2 up> key code.
#define WW_KEY_u_U_Cont                    43  // u, U, Cont key code.
#define WW_KEY_7_AMPERSAND                 44  // 7, & key code.
#define WW_KEY_6_CENT                      45  // 6, <cent> key code.
#define WW_KEY_j_J                         46  // j, J key code.
#define WW_KEY_h_H_12DOWN                  47  // h, H, <1/2 down> key code.
#define WW_KEY_COMMA_COMMA                 48  // ,, , key code.
#define WW_KEY_RBRACKET_LBRACKET_SUPER3    49  // ], [, <superscript 3> key code.
#define WW_KEY_i_I_Word                    50  // i, I, Word key code.
#define WW_KEY_8_ASTERISK                  51  // 8, * key code.
#define WW_KEY_EQUAL_PLUS                  52  // =, + key code.
#define WW_KEY_k_K                         53  // k, K key code.
#define WW_KEY_PERIOD_PERIOD               54  // ., . key code.
#define WW_KEY_o_O_RFlsh                   55  // o, O, R Flsh key code.
#define WW_KEY_9_LPAREN_Stop               56  // 9, (, Stop key code.
#define WW_KEY_l_L_Lang                    57  // l, L, Lang key code.
#define WW_KEY_SLASH_QUESTION              58  // /, ? key code.
#define WW_KEY_HALF_QUARTER_SUPER2         59  // <half>, <quarter>, <superscript 2> key code.
#define WW_KEY_p_P                         60  // p, P key code.
#define WW_KEY_0_RPAREN                    61  // 0, ) key code.
#define WW_KEY_HYPHEN_UNDERSCORE           62  // -, _ key code.
#define WW_KEY_SEMICOLON_COLON_SECTION     63  // ;, :, <section> key code.
#define WW_KEY_APOSTROPHE_QUOTE_PARAGRAPH  64  // ', ", <paragraph> key code.
#define WW_KEY_LARROW_Word                 65  // <left arrow>, <left word> key code.
#define WW_KEY_UARROW_Line                 66  // <up arrow>, <up line> key code.
#define WW_KEY_X22_134                     67  // *** not available on Wheelwriter 1000.
#define WW_KEY_X21_135                     68  // *** not available on Wheelwriter 1000.
#define WW_KEY_Backspace_Bksp1             69  // Backspace, Bksp 1 key code.
#define WW_KEY_X23_137                     70  // *** not available on Wheelwriter 1000.
#define WW_KEY_CRtn_IndClr                 71  // C Rtn, IndClr key code.
#define WW_KEY_BACKX_Word                  72  // <back X>, <back word> key code.
#define WW_KEY_Lock                        73  // Lock key code.
#define WW_KEY_RMar                        74  // R Mar key code.
#define WW_KEY_Tab_IndL                    75  // Tab key code.
#define WW_KEY_MarRel_RePrt                76  // Mar Rel, RePrt key code.
#define WW_KEY_TSet                        77  // T Set key code.
#define WW_KEY_X15_148                     78  // *** not available on Wheelwriter 1000.

#define NUM_WW_KEYS                        79  // Number of keyboard keys.

// Wheelwriter special print codes.
#define WW_NULL      0x00                 // Null print code.
#define WW_NULL_1    (WW_COLUMN_1 << 4)   // Column 1 null print code.
#define WW_NULL_2    (WW_COLUMN_2 << 4)   // Column 2 null print code.
#define WW_NULL_3    (WW_COLUMN_3 << 4)   // Column 3 null print code.
#define WW_NULL_4    (WW_COLUMN_4 << 4)   // Column 4 null print code.
#define WW_NULL_5    (WW_COLUMN_5 << 4)   // Column 5 null print code.
#define WW_NULL_6    (WW_COLUMN_6 << 4)   // Column 6 null print code.
#define WW_NULL_7    (WW_COLUMN_7 << 4)   // Column 7 null print code.
#define WW_NULL_8    (WW_COLUMN_8 << 4)   // Column 8 null print code.
#define WW_NULL_9    (WW_COLUMN_9 << 4)   // Column 9 null print code.
#define WW_NULL_10   (WW_COLUMN_10 << 4)  // Column 10 null print code.
#define WW_NULL_11   (WW_COLUMN_11 << 4)  // Column 11 null print code.
#define WW_NULL_12   (WW_COLUMN_12 << 4)  // Column 12 null print code.
#define WW_NULL_13   (WW_COLUMN_13 << 4)  // Column 13 null print code.
#define WW_NULL_14   (WW_COLUMN_14 << 4)  // Column 14 null print code.
#define WW_CATCH_UP  0xfe                 // Wait for printing to catch up.
#define WW_SKIP      0xff                 // Skip print code.

// Wheelwriter print codes.
#define WW_LShift                      (WW_NULL_1 | WW_ROW_1)   // <left shift> print code.
#define WW_RShift                      (WW_NULL_1 | WW_ROW_2)   // <right shift> print code.
#define WW_RARROW_Word                 (WW_NULL_2 | WW_ROW_1)   // <right arrow>, <right word> print code.
#define WW_PaperDown_Micro             (WW_NULL_2 | WW_ROW_4)   // Paper Down, <down micro> print code.
#define WW_PaperUp_Micro               (WW_NULL_2 | WW_ROW_6)   // Paper Up, <up micro> print code.
#define WW_Reloc_LineSpace             (WW_NULL_2 | WW_ROW_7)   // Reloc, Line Space print code.
#define WW_DARROW_Line                 (WW_NULL_2 | WW_ROW_8)   // <down arrow>, <down line> print code.
#define WW_z_Z                         (WW_NULL_3 | WW_ROW_2)   // z, Z print code.
#define WW_q_Q_Impr                    (WW_NULL_3 | WW_ROW_4)   // q, Q, Impr print code.
#define WW_1_EXCLAMATION_Spell         (WW_NULL_3 | WW_ROW_5)   // 1, !, Spell print code.
#define WW_PLUSMINUS_DEGREE            (WW_NULL_3 | WW_ROW_6)   // +/-, <degree> print code.
#define WW_a_A                         (WW_NULL_3 | WW_ROW_7)   // a, A print code.
#define WW_SPACE_REQSPACE              (WW_NULL_4 | WW_ROW_1)   // <space>, <required space> print code.
#define WW_LOADPAPER                   (WW_NULL_4 | WW_ROW_2)   // <load paper> print code.
#define WW_LMar                        (WW_NULL_4 | WW_ROW_4)   // L Mar print code.
#define WW_TClr                        (WW_NULL_4 | WW_ROW_8)   // T Clr print code.
#define WW_Code                        (WW_NULL_5 | WW_ROW_1)   // Code print code.
#define WW_x_X_POWERWISE               (WW_NULL_6 | WW_ROW_2)   // x, X, <power wise> print code.
#define WW_w_W                         (WW_NULL_6 | WW_ROW_4)   // w, W print code.
#define WW_2_AT_Add                    (WW_NULL_6 | WW_ROW_5)   // 2, @, Add print code.
#define WW_s_S                         (WW_NULL_6 | WW_ROW_7)   // s, S print code.
#define WW_c_C_Ctr                     (WW_NULL_7 | WW_ROW_2)   // c, C, Ctr print code.
#define WW_e_E                         (WW_NULL_7 | WW_ROW_4)   // e, E print code.
#define WW_3_POUND_Del                 (WW_NULL_7 | WW_ROW_5)   // 3, #, Del print code.
#define WW_d_D_DecT                    (WW_NULL_7 | WW_ROW_7)   // d, D, Dec T print code.
#define WW_b_B_Bold                    (WW_NULL_8 | WW_ROW_1)   // b, B, Bold print code.
#define WW_v_V                         (WW_NULL_8 | WW_ROW_2)   // v, V print code.
#define WW_t_T                         (WW_NULL_8 | WW_ROW_3)   // t, T print code.
#define WW_r_R_ARtn                    (WW_NULL_8 | WW_ROW_4)   // r, R, A Rtn print code.
#define WW_4_DOLLAR_Vol                (WW_NULL_8 | WW_ROW_5)   // 4, $, Vol print code.
#define WW_5_PERCENT                   (WW_NULL_8 | WW_ROW_6)   // 5, % print code.
#define WW_f_F                         (WW_NULL_8 | WW_ROW_7)   // f, F print code.
#define WW_g_G                         (WW_NULL_8 | WW_ROW_8)   // g, G print code.
#define WW_n_N_Caps                    (WW_NULL_9 | WW_ROW_1)   // n, N, Caps print code.
#define WW_m_M                         (WW_NULL_9 | WW_ROW_2)   // m, M print code.
#define WW_y_Y_12UP                    (WW_NULL_9 | WW_ROW_3)   // y, Y, <1/2 up> print code.
#define WW_u_U_Cont                    (WW_NULL_9 | WW_ROW_4)   // u, U, Cont print code.
#define WW_7_AMPERSAND                 (WW_NULL_9 | WW_ROW_5)   // 7, & print code.
#define WW_6_CENT                      (WW_NULL_9 | WW_ROW_6)   // 6, <cent> print code.
#define WW_j_J                         (WW_NULL_9 | WW_ROW_7)   // j, J print code.
#define WW_h_H_12DOWN                  (WW_NULL_9 | WW_ROW_8)   // h, H, <1/2 down> print code.
#define WW_COMMA_COMMA                 (WW_NULL_10 | WW_ROW_2)  // ,, , print code.
#define WW_RBRACKET_LBRACKET_SUPER3    (WW_NULL_10 | WW_ROW_3)  // ], [, <superscript 3> print code.
#define WW_i_I_Word                    (WW_NULL_10 | WW_ROW_4)  // i, I, Word print code.
#define WW_8_ASTERISK                  (WW_NULL_10 | WW_ROW_5)  // 8, * print code.
#define WW_EQUAL_PLUS                  (WW_NULL_10 | WW_ROW_6)  // =, + print code.
#define WW_k_K                         (WW_NULL_10 | WW_ROW_7)  // k, K print code.
#define WW_PERIOD_PERIOD               (WW_NULL_11 | WW_ROW_2)  // ., . print code.
#define WW_o_O_RFlsh                   (WW_NULL_11 | WW_ROW_4)  // o, O, R Flsh print code.
#define WW_9_LPAREN_Stop               (WW_NULL_11 | WW_ROW_5)  // 9, (, Stop print code.
#define WW_l_L_Lang                    (WW_NULL_11 | WW_ROW_7)  // l, L, Lang print code.
#define WW_SLASH_QUESTION              (WW_NULL_12 | WW_ROW_1)  // /, ? print code.
#define WW_HALF_QUARTER_SUPER2         (WW_NULL_12 | WW_ROW_3)  // <half>, <quarter>, <superscript 2> print code.
#define WW_p_P                         (WW_NULL_12 | WW_ROW_4)  // p, P print code.
#define WW_0_RPAREN                    (WW_NULL_12 | WW_ROW_5)  // 0, ) print code.
#define WW_HYPHEN_UNDERSCORE           (WW_NULL_12 | WW_ROW_6)  // -, _ print code.
#define WW_SEMICOLON_COLON_SECTION     (WW_NULL_12 | WW_ROW_7)  // :, :, <section> print code.
#define WW_APOSTROPHE_QUOTE_PARAGRAPH  (WW_NULL_12 | WW_ROW_8)  // ', ", <paragraph> print code.
#define WW_LARROW_Word                 (WW_NULL_13 | WW_ROW_1)  // <left arrow>, <left word> print code.
#define WW_UARROW_Line                 (WW_NULL_13 | WW_ROW_2)  // <up arrow>, <up line> print code.
#define WW_Backspace_Bksp1             (WW_NULL_13 | WW_ROW_6)  // Backspace, Bksp 1 print code.
#define WW_CRtn_IndClr                 (WW_NULL_13 | WW_ROW_8)  // C Rtn, Ind Clr print code.
#define WW_BACKX_Word                  (WW_NULL_14 | WW_ROW_1)  // <back X>, <back word> print code.
#define WW_Lock                        (WW_NULL_14 | WW_ROW_2)  // Lock print code.
#define WW_RMar                        (WW_NULL_14 | WW_ROW_3)  // R Mar print code.
#define WW_Tab_IndL                    (WW_NULL_14 | WW_ROW_4)  // Tab print code.
#define WW_MarRel_RePrt                (WW_NULL_14 | WW_ROW_6)  // Mar Rel, RePrt print code.
#define WW_TSet                        (WW_NULL_14 | WW_ROW_7)  // T Set print code.

// Keyboard key offset values.
#define OFFSET_NONE   0                            // Not shifted.
#define OFFSET_SHIFT  (NUM_WW_KEYS)                // Shifted.
#define OFFSET_CODE   (NUM_WW_KEYS + NUM_WW_KEYS)  // Code shifted.

// Keyboard scan timing values (in msec).
#define SHORT_SCAN_DURATION  10UL  // Threshold time for Wheelwriter short scans.
#define LONG_SCAN_DURATION   25UL  // Threshold time for Wheelwriter long scans.

// Keyboard scan timing values (in usec).
#define CSCAN_NO_CHANGE   820                                       // Time for column scan with no key change.
#define CSCAN_CHANGE     3680                                       // Time for column scan with a key change.
#define FSCAN_0_CHANGES  (14 * CSCAN_NO_CHANGE)                     // Time for full scan with 0 key changes.
#define FSCAN_1_CHANGE   (13 * CSCAN_NO_CHANGE +     CSCAN_CHANGE)  // Time for full scan with 1 key change.
#define FSCAN_2_CHANGES  (12 * CSCAN_NO_CHANGE + 2 * CSCAN_CHANGE)  // Time for full scan with 2 key changes.
#define FSCAN_3_CHANGES  (11 * CSCAN_NO_CHANGE + 3 * CSCAN_CHANGE)  // Time for full scan with 3 key changes.

// Keyboard read timing values (in msec).
#define KEY_PRESS_DEBOUNCE     85UL  // Shadow time for debouncing key presses.
#define KEY_RELEASE_DEBOUNCE   15UL  // Shadow time for debouncing key releases.
#define KEY_SPACE_DEBOUNCE    150UL  // Shadow time for debouncing space key presses.
#define KEY_RETURN_DEBOUNCE   150UL  // Shadow time for debouncing return key presses.
#define KEY_REPEAT            100UL  // Repeating keys 10 cps.

// Keyboard actions.
#define ACTION_NONE          0x00      // No action.
#define ACTION_SEND          0x01      // Send a character action.
#define ACTION_PRINT         0x02      // Print a character action.
#define ACTION_COMMAND       0x04      // Process a command character action.
#define ACTION_MASK          0xf8      // Action mask.

#define ACTION_IBM_MODE_0    (1 << 3)  // IBM 1620 Jr. Mode 0 action.
#define ACTION_IBM_MODE_1    (2 << 3)  // IBM 1620 Jr. Mode 1 action.
#define ACTION_IBM_MODE_1F   (3 << 3)  // IBM 1620 Jr. Mode 1 flag action.
#define ACTION_IBM_MODE_2    (4 << 3)  // IBM 1620 Jr. Mode 2 action.
#define ACTION_IBM_MODE_3    (5 << 3)  // IBM 1620 Jr. Mode 3 action.
#define ACTION_IBM_SETUP     (6 << 3)  // IBM 1620 Jr. setup action.

#define ACTION_ASCII_RETURN  (7 << 3)  // ASCII Terminal return action.
#define ACTION_ASCII_SETUP   (8 << 3)  // ASCII Terminal setup action.

#define ACTION_FUTURE_SETUP  (9 << 3)  // Future setup action.

// Serial input actions.
#define CMD_NONE           0  // No action.
#define CMD_PRINT          1  // Print character action.

#define CMD_IBM_MODE_0     2  // Set mode 0 IBM 1620 Jr. action.
#define CMD_IBM_MODE_1     3  // Set mode 1 IBM 1620 Jr. action.
#define CMD_IBM_MODE_2     4  // Set mode 2 IBM 1620 Jr. action.
#define CMD_IBM_MODE_3     5  // Set mode 3 IBM 1620 Jr. action.
#define CMD_IBM_PING       6  // Ping IBM 1620 Jr. action.
#define CMD_IBM_ACK        7  // Ack IBM 1620 Jr. action.
#define CMD_IBM_SLASH      8  // Slash zeroes IBM 1620 Jr. action.
#define CMD_IBM_UNSLASH    9  // Unslash zeroes IBM 1620 Jr. action.
#define CMD_IBM_RESET     10  // Reset IBM 1620 Jr. action.
#define CMD_IBM_PAUSE     11  // Pause IBM 1620 Jr. action.
#define CMD_IBM_RESUME    12  // Resume IBM 1620 Jr. action.

#define CMD_ASCII_CR      13  // CR action.
#define CMD_ASCII_LF      14  // LF action.
#define CMD_ASCII_XON     15  // XON ASCII Terminal action.
#define CMD_ASCII_XOFF    16  // XOFF ASCII Terminal action.

// Type run modes.
#define MODE_INITIALIZING        0  // Typewriter is initializing.
#define MODE_RUNNING             1  // Typewriter is running.
#define MODE_IBM_BEING_SETUP     2  // IBM 1620 Jr. is being setup.
#define MODE_ASCII_BEING_SETUP   3  // ASCII Terminal is being setup.
#define MODE_FUTURE_BEING_SETUP  4  // Future is being setup.

// Print string spacing types.
#define SPACING_NONE       0  // No horizontal movement.
#define SPACING_UNKNOWN    0  // Unknown horizontal movement, treat as none.
#define SPACING_FORWARD    1  // Forward horizontal movement.
#define SPACING_BACKWARD   2  // Backward horizontal movement.
#define SPACING_TAB        3  // Tab horizontal movement.
#define SPACING_RETURN     4  // Return horizontal movement.
#define SPACING_COLUMN1    5  // Column 1 horizontal movement.
#define SPACING_LMAR       6  // No horizontal movement, set left margin.
#define SPACING_RMAR       7  // No horizontal movement, set right margin.
#define SPACING_MARREL     8  // No horizontal movement, margin release.
#define SPACING_TSET       9  // No horizontal movement, set tab.
#define SPACING_TCLR      10  // No horizontal movement, clear tab.
#define SPACING_CLRALL    11  // No horizontal movement, clear all tabs.

// Print string timing values (in usec).
#define TIME_CHARACTER    65000  // Time of one average print character.
#define TIME_HMOVEMENT   125000  // Time of one horizontal movement character.
#define TIME_VMOVEMENT   125000  // Time of one vertical movement character.
#define TIME_RETURN      750000  // Time of one full carriage return.
#define TIME_LOADPAPER  3000000  // Time of one load paper.
#define TIME_ADJUST       10000  // Time adjustment for special cases.

#define POSITIVE(v)  ((v) >= 0 ? (v) : 0)

#define TIMING_NONE       0                               // Residual time for non-printing character.
#define TIMING_UNKNOWN    0                               // Unknown timing, treat as none.
#define TIMING_NOSHIFT    (POSITIVE(TIME_CHARACTER - (2 * FSCAN_1_CHANGE)))
                                                          // Residual time for unshifted print character.
#define TIMING_SHIFT      (POSITIVE(TIME_CHARACTER - (2 * FSCAN_1_CHANGE + FSCAN_2_CHANGES)))
                                                          // Residual time for shifted print character.
#define TIMING_CODE       (POSITIVE(TIME_CHARACTER - (2 * FSCAN_1_CHANGE + FSCAN_2_CHANGES)))
                                                          // Residual time for coded print character.
#define TIMING_HMOVE      (POSITIVE(TIME_HMOVEMENT - (2 * FSCAN_1_CHANGE)))
                                                          // Residual time for single horizontal movement.
#define TIMING_HMOVE2     (POSITIVE(TIME_HMOVEMENT - (2 * FSCAN_1_CHANGE + FSCAN_2_CHANGES)))
                                                          // Residual time for single coded horizontal movement.
#define TIMING_VMOVE      (POSITIVE(TIME_VMOVEMENT - (2 * FSCAN_1_CHANGE)))
                                                          // Residual time for single vertical movement.
#define TIMING_VMOVE2     (POSITIVE(TIME_VMOVEMENT - (2 * FSCAN_1_CHANGE + FSCAN_2_CHANGES)))
                                                          // Residual time for single coded vertical movement.
#define TIMING_TAB        (POSITIVE(2 * TIME_HMOVEMENT))  // Residual time for tab print character, 8 spaces.
#define TIMING_RETURN     (- TIME_RETURN)                 // Residual time for return print character, full line.
#define TIMING_LOADPAPER  (POSITIVE(TIME_LOADPAPER - (2 * FSCAN_1_CHANGE)))
                                                          // Residual time for a load paper.

// ISR values.
#define ISR_DELAY  20  // ISR delay (in usec) before reading column lines.

// Setup values.
#define COLUMN_COMMAND   68  // Column to space to for command response so prompt can be read.
#define COLUMN_RESPONSE  64  // Column to space to for setting responses so prompts can be read.
#define COLUMN_QUESTION  49  // Column to space to for question answers so prompts can be read.
#define COLUMN_FUNCTION  69  // Column to space to for function response so prompt can be read.

// Logic values.
#define TRUE   1  // Boolean true.
#define FALSE  0  // Boolean false.

// Size of data arrays.
#define SIZE_COMMAND_BUFFER     100  // Size of command buffer, in characters.
#define SIZE_RECEIVE_BUFFER    1000  // Size of serial receive buffer, in characters.
#define SIZE_SEND_BUFFER       1000  // Size of serial send buffer, in characters.
#define SIZE_TRANSFER_BUFFER    100  // Size of ISR -> main transfer buffer, in print characters.
#define SIZE_PRINT_BUFFER    100000  // Size of print buffer, in print codes.

// Buffer flow control thresholds.
#define LOWER_THRESHOLD_RECEIVE_BUFFER     50  // Flow control lower threshold of serial receive buffer, in characters.
#define UPPER_THRESHOLD_RECEIVE_BUFFER    750  // Flow control upper threshold of serial receive buffer, in characters.
#define LOWER_THRESHOLD_PRINT_BUFFER     5000  // Flow control lower threshold of print buffer, in print codes.
#define UPPER_THRESHOLD_PRINT_BUFFER    45000  // Flow control upper threshold of print buffer, in print codes.

// EEPROM data locations.
#define EEPROM_FINGERPRINT     0  // EEPROM location of fingerprint byte.                        Used by all emulations.
#define EEPROM_VERSION         1  // EEPROM location of firmware version byte.                   Used by all emulations.
#define EEPROM_ERRORS          2  // EEPROM location of errors setting byte.                     Used by all emulations.
#define EEPROM_WARNINGS        3  // EEPROM location of warnings setting byte.                   Used by all emulations.
#define EEPROM_BATTERY         4  // EEPROM location of battery setting byte.                    Used by all emulations.
#define EEPROM_LMARGIN         5  // EEPROM location of left margin setting byte.                Used by all emulations.
#define EEPROM_RMARGIN         6  // EEPROM location of right margin setting byte.               Used by all emulations.
#define EEPROM_EMULATION       7  // EEPROM location of emulation.                               Used by all emulations.

#define EEPROM_SLASH          10  // EEPROM location of slash zero setting byte.                 Used by IBM 1620 Jr.
#define EEPROM_BOLD           11  // EEPROM location of bold setting byte.                       Used by IBM 1620 Jr.

#define EEPROM_SERIAL         20  // EEPROM location of serial setting byte.                     Used by ASCII Terminal.
#define EEPROM_DUPLEX         21  // EEPROM location of duplex setting byte.                     Used by ASCII Terminal.
#define EEPROM_BAUD           22  // EEPROM location of baud setting byte.                       Used by ASCII Terminal.
#define EEPROM_PARITY         23  // EEPROM location of parity setting byte.                     Used by ASCII Terminal.
#define EEPROM_DPS            24  // EEPROM location of databits, parity, stopbits setting byte. Used by ASCII Terminal.
#define EEPROM_SWFLOW         25  // EEPROM location of software flow control setting byte.      Used by ASCII Terminal.
#define EEPROM_HWFLOW         26  // EEPROM location of hardware flow control setting byte.      Used by ASCII Terminal.
#define EEPROM_AUTORETURN     27  // EEPROM location of auto return setting byte.                Used by ASCII Terminal.
#define EEPROM_TRANSMITEOL    28  // EEPROM location of transmit end-of-line setting byte.       Used by ASCII Terminal.
#define EEPROM_WIDTH          29  // EEPROM location of paper width byte.                        Used by ASCII Terminal.
#define EEPROM_OFFSET         30  // EEPROM location of column offset byte.                      Used by ASCII Terminal.
#define EEPROM_RECEIVEEOL     31  // EEPROM location of receive end-of-line setting byte.        Used by ASCII Terminal.
#define EEPROM_IGNOREESCAPE   32  // EEPROM location of ignore escape setting byte.              Used by ASCII Terminal.
#define EEPROM_UPPERCASE      33  // EEPROM location of uppercase only setting byte.             Used by ASCII Terminal.

#define EEPROM_TABS          100  // EEPROM location of tab table bytes [166].                   Used by all emulations.

#define SIZE_EEPROM         4096  // Size of EEPROM.

// EEPROM fingerprint value.
#define FINGERPRINT  0xdb  // EEPROM fingerprint value.

// Error codes.
#define ERROR_NULL      0  // Null error.
#define ERROR_CB_FULL   1  // Command buffer full error.
#define ERROR_RB_FULL   2  // Receive buffer full error.
#define ERROR_SB_FULL   3  // Send buffer full error.
#define ERROR_TB_FULL   4  // Transfer buffer full error.
#define ERROR_PB_FULL   5  // Print buffer full error.
#define ERROR_BAD_CODE  6  // Bad print code error.

#define NUM_ERRORS      7  // Number of error codes.

// Warning codes.
#define WARNING_NULL             0  // Null warning.
#define WARNING_SHORT_SCAN       1  // Short column scan warning.
#define WARNING_LONG_SCAN        2  // Long column scan warning.
#define WARNING_UNEXPECTED_SCAN  3  // Unexpected column scan warning.

#define NUM_WARNINGS             4  // Number of warning codes.

// Emulation codes.
                                 // J6 J7  Jumper settings.
#define EMULATION_NULL        0  //        - Null emulation.
#define EMULATION_IBM         1  //        - IBM 1620 Jr. Console Typewriter emulation.
#define EMULATION_ASCII       2  //     X  - ASCII Terminal emulation.
#define EMULATION_FUTURE      3  //  X     - Reserved for future emulation.
#define EMULATION_STANDALONE  4  //  X  X  - Standalone Typewriter emulation.

// True or false setting types.
#define SETTING_UNDEFINED  0  // Undefined setting.
#define SETTING_TRUE       1  // True setting.
#define SETTING_FALSE      2  // False setting.

// Serial types.
#define SERIAL_UNDEFINED  0  // Undefined serial setting.
#define SERIAL_USB        1  // USB serial.
#define SERIAL_RS232      2  // RS-232 serial (UART or SlowSoftSerial).

// RS232 types (not stored in EEPROM)
#define RS232_UNDEFINED   0  // Undefined RS232 mode.
#define RS232_UART        1  // Hardware UART.
#define RS232_SLOW        2  // SlowSoftSerial port.

// Duplex types.
#define DUPLEX_UNDEFINED  0  // Undefined duplex setting.
#define DUPLEX_HALF       1  // Half duplex setting.
#define DUPLEX_FULL       2  // Full duplex setting.

// Baud values.
#define BAUD_UNDEFINED   0  // Undefined baud setting.
#define BAUD_50          1  // 50 baud.    Can't use with UART0.
#define BAUD_75          2  // 75 baud.    Can't use with UART0.
#define BAUD_110         3  // 110 baud.   Can't use with UART0.
#define BAUD_134         4  // 134 baud.   Can't use with UART0.
#define BAUD_150         5  // 150 baud.   Can't use with UART0.
#define BAUD_200         6  // 200 baud.   Can't use with UART0.
#define BAUD_300         7  // 300 baud.   Can't use with UART0.
#define BAUD_600         8  // 600 baud.   Can't use with UART0.
#define BAUD_1200        9  // 1200 baud.
#define BAUD_1800       10  // 1800 baud.
#define BAUD_2400       11  // 2400 baud.
#define BAUD_4800       12  // 4800 baud.
#define BAUD_9600       13  // 9600 baud.
#define BAUD_19200      14  // 19200 baud.
#define BAUD_38400      15  // 38400 baud.
#define BAUD_57600      16  // 57600 baud.
#define BAUD_76800      17  // 76800 baud.
#define BAUD_115200     18  // 115200 baud.
#define BAUD_230400     19  // 230400 baud.
#define BAUD_460800     20  // 460800 baud.  Not supported by MAX3232.
#define BAUD_921600     21  // 921600 baud.  Not supported by MAX3232.

#define NUM_BAUDS       22  // Number of baud values.
#define MINIMUM_HARDWARE_BAUD BAUD_1200

// Parity types.
#define PARITY_UNDEFINED  0  // Undefined parity setting.
#define PARITY_NONE       1  // None parity setting.
#define PARITY_ODD        2  // Odd parity setting.
#define PARITY_EVEN       3  // Even parity setting.

// Databits, parity, stopbits values.
#define DPS_UNDEFINED  0  // Undefined databits, parity, stopbits setting.
#define DPS_7O1        1  // 7 databits, odd parity, 1 stopbit setting.
#define DPS_7E1        2  // 7 databits, even parity, 1 stopbit setting.
#define DPS_8N1        3  // 8 databits, no parity, 1 stopbit setting.
#define DPS_8O1        4  // 8 databits, odd parity, 1 stopbit setting.
#define DPS_8E1        5  // 8 databits, even parity, 1 stopbit setting.
#define DPS_8N2        6  // 8 databits, no parity, 2 stopbits setting.
#define DPS_8O2        7  // 8 databits, odd parity, 2 stopbits setting.
#define DPS_8E2        8  // 8 databits, even parity, 2 stopbits setting.

#define NUM_DPSS       9  // Number of dps values.

// Software flow control types.
#define SWFLOW_UNDEFINED  0  // Undefined software flow control setting.
#define SWFLOW_NONE       1  // No software flow control setting.
#define SWFLOW_XON_XOFF   2  // XON/XOFF flow control setting.

// Hardware flow control types.
#define HWFLOW_UNDEFINED  0  // Undefined hardware flow control setting.
#define HWFLOW_NONE       1  // No hardware flow control setting.
#define HWFLOW_RTS_CTS    2  // RTS/CTS flow control setting.
#define HWFLOW_RTR_CTS    3  // RTR/CTS flow control setting.

// End of line types.
#define EOL_UNDEFINED  0  // Undefined end-of-line setting.
#define EOL_CR         1  // CR end-of-line setting.
#define EOL_CRLF       2  // CR LF end-of-line setting.
#define EOL_LF         3  // LF end-of-line setting.
#define EOL_LFCR       4  // LF CR end-of-line setting.

// Paper width values.
#define WIDTH_UNDEFINED    0  // Undefined width setting.
#define WIDTH_MINIMUM     80  // Minimum paper width.
#define WIDTH_MAXIMUM    165  // Maximum paper width.

// Configuration parameters initial values.
#define INITIAL_ERRORS        SETTING_TRUE     // Report errors.
#define INITIAL_WARNINGS      SETTING_FALSE    // Report warnings.
#define INITIAL_BATTERY       SETTING_FALSE    // Battery installed.
#define INITIAL_LMARGIN       1                // Left margin.
#define INITIAL_RMARGIN       80               // Right margin.
#define INITIAL_EMULATION     EMULATION_NULL   // Current emulation.
#define INITIAL_SLASH         SETTING_TRUE     // Print slashed zeroes.
#define INITIAL_BOLD          SETTING_FALSE    // Print bold input.
#define INITIAL_SERIAL        SERIAL_USB       // Serial type.
#define INITIAL_DUPLEX        DUPLEX_FULL      // Duplex.
#define INITIAL_BAUD          BAUD_9600        // Baud rate.
#define INITIAL_PARITY        PARITY_NONE      // Parity.
#define INITIAL_DPS           DPS_8N1          // Databits, parity, stopbits.
#define INITIAL_SWFLOW        SWFLOW_XON_XOFF  // Software flow control.
#define INITIAL_HWFLOW        HWFLOW_NONE      // Hardware flow control.
#define INITIAL_UPPERCASE     SETTING_FALSE    // Uppercase only.
#define INITIAL_AUTORETURN    SETTING_TRUE     // Auto return.
#define INITIAL_TRANSMITEOL   EOL_CR           // Send end-of-line.
#define INITIAL_RECEIVEEOL    EOL_CRLF         // Receive end-of-line.
#define INITIAL_IGNOREESCAPE  SETTING_TRUE     // Ignore escape sequences.
#define INITIAL_WIDTH         80               // Paper width.
#define INITIAL_OFFSET        SETTING_TRUE     // Column offset.

//**********************************************************************************************************************
//
//  Wheelwriter control structures.
//
//**********************************************************************************************************************

// Keyboard key action structure.
struct key_action {
  byte                     action;  // Action to take.
  char                     send;    // Character to send or process as setup command.
  const struct print_info *print;   // Character to print.
};

// Serial input action structure.
struct serial_action {
  int                      action;  // Action to take.
  const struct print_info *print;   // Character to print.
};

// Print string information structure.
struct print_info {
  int                    spacing;  // Spacing type;
  int                    timing;   // Timing value (in usec).
  union {                          // Print codes string.
    const void          *init;     //   Used for static initialization.
    const byte (*string)[1000];    //   Used for referencing.
  };
};


//**********************************************************************************************************************
//
//  Wheelwriter print strings.
//
//**********************************************************************************************************************

// <load paper> print string.
const byte WW_STR_LOADPAPER[]              = {WW_CATCH_UP, WW_LOADPAPER, WW_NULL_4, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_LOADPAPER = {SPACING_RETURN, TIMING_LOADPAPER, &WW_STR_LOADPAPER};

// <space>, <required space>, Tab, C Rtn print strings.
const byte WW_STR_SPACE[]                 = {WW_SPACE_REQSPACE, WW_NULL_4, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_SPACE    = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_SPACE};
const struct print_info WW_PRINT_SPACEX   = {SPACING_NONE, TIMING_NOSHIFT, &WW_STR_SPACE};  // Special space for column offset.

const byte WW_STR_REQSPACE[]              = {WW_Code, WW_SPACE_REQSPACE, WW_Code, WW_NULL_4, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_REQSPACE = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_REQSPACE};

const byte WW_STR_Tab[]                   = {WW_Tab_IndL, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Tab      = {SPACING_TAB, TIMING_TAB, &WW_STR_Tab};

const byte WW_STR_CRtn_IndClr[]           = {WW_CRtn_IndClr, WW_NULL_13, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_CRtn     = {SPACING_RETURN, TIMING_RETURN, &WW_STR_CRtn_IndClr};

// <left shift>, <right shift>, Lock print strings.
const byte WW_STR_LShift[]              = {WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_LShift = {SPACING_NONE, TIMING_NONE, &WW_STR_LShift};

const byte WW_STR_RShift[]              = {WW_RShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_RShift = {SPACING_NONE, TIMING_NONE, &WW_STR_RShift};

const byte WW_STR_Lock[]                = {WW_Lock, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Lock   = {SPACING_NONE, TIMING_NONE, &WW_STR_Lock};

// A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z print strings.
const byte WW_STR_A[]              = {WW_LShift, WW_a_A, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_A = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_A};

const byte WW_STR_B[]              = {WW_RShift, WW_b_B_Bold, WW_RShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_B = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_B};

const byte WW_STR_C[]              = {WW_LShift, WW_c_C_Ctr, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_C = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_C};

const byte WW_STR_D[]              = {WW_LShift, WW_d_D_DecT, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_D = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_D};

const byte WW_STR_E[]              = {WW_LShift, WW_e_E, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_E = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_E};

const byte WW_STR_F[]              = {WW_LShift, WW_f_F, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_F = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_F};

const byte WW_STR_G[]              = {WW_LShift, WW_g_G, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_G = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_G};

const byte WW_STR_H[]              = {WW_LShift, WW_h_H_12DOWN, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_H = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_H};

const byte WW_STR_I[]              = {WW_LShift, WW_i_I_Word, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_I = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_I};

const byte WW_STR_J[]              = {WW_LShift, WW_j_J, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_J = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_J};

const byte WW_STR_K[]              = {WW_LShift, WW_k_K, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_K = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_K};

const byte WW_STR_L[]              = {WW_LShift, WW_l_L_Lang, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_L = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_L};

const byte WW_STR_M[]              = {WW_LShift, WW_m_M, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_M = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_M};

const byte WW_STR_N[]              = {WW_RShift, WW_n_N_Caps, WW_RShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_N = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_N};

const byte WW_STR_O[]              = {WW_LShift, WW_o_O_RFlsh, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_O = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_O};

const byte WW_STR_P[]              = {WW_LShift, WW_p_P, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_P = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_P};

const byte WW_STR_Q[]              = {WW_LShift, WW_q_Q_Impr, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Q = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_Q};

const byte WW_STR_R[]              = {WW_LShift, WW_r_R_ARtn, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_R = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_R};

const byte WW_STR_S[]              = {WW_LShift, WW_s_S, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_S = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_S};

const byte WW_STR_T[]              = {WW_LShift, WW_t_T, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_T = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_T};

const byte WW_STR_U[]              = {WW_LShift, WW_u_U_Cont, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_U = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_U};

const byte WW_STR_V[]              = {WW_LShift, WW_v_V, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_V = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_V};

const byte WW_STR_W[]              = {WW_LShift, WW_w_W, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_W = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_W};

const byte WW_STR_X[]              = {WW_LShift, WW_x_X_POWERWISE, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_X = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_X};

const byte WW_STR_Y[]              = {WW_LShift, WW_y_Y_12UP, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Y = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_Y};

const byte WW_STR_Z[]              = {WW_LShift, WW_z_Z, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL}; 
const struct print_info WW_PRINT_Z = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_Z};

// a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z print strings.
const byte WW_STR_a[]              = {WW_a_A, WW_NULL_3, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_a = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_a};

const byte WW_STR_b[]              = {WW_b_B_Bold, WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_b = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_b};

const byte WW_STR_c[]              = {WW_c_C_Ctr, WW_NULL_7, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_c = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_c};

const byte WW_STR_d[]              = {WW_d_D_DecT, WW_NULL_7, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_d = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_d};

const byte WW_STR_e[]              = {WW_e_E, WW_NULL_7, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_e = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_e};

const byte WW_STR_f[]              = {WW_f_F, WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_f = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_f};

const byte WW_STR_g[]              = {WW_g_G, WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_g = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_g};

const byte WW_STR_h[]              = {WW_h_H_12DOWN, WW_NULL_9, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_h = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_h};

const byte WW_STR_i[]              = {WW_i_I_Word, WW_NULL_10, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_i = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_i};

const byte WW_STR_j[]              = {WW_j_J, WW_NULL_9, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_j = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_j};

const byte WW_STR_k[]              = {WW_k_K, WW_NULL_10, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_k = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_k};

const byte WW_STR_l[]              = {WW_l_L_Lang, WW_NULL_11, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_l = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_l};

const byte WW_STR_m[]              = {WW_m_M, WW_NULL_9, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_m = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_m};

const byte WW_STR_n[]              = {WW_n_N_Caps, WW_NULL_9, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_n = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_n};

const byte WW_STR_o[]              = {WW_o_O_RFlsh, WW_NULL_11, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_o = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_o};

const byte WW_STR_p[]              = {WW_p_P, WW_NULL_12, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_p = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_p};

const byte WW_STR_q[]              = {WW_q_Q_Impr, WW_NULL_3, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_q = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_q};

const byte WW_STR_r[]              = {WW_r_R_ARtn, WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_r = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_r};

const byte WW_STR_s[]              = {WW_s_S, WW_NULL_6, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_s = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_s};

const byte WW_STR_t[]              = {WW_t_T, WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_t = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_t};

const byte WW_STR_u[]              = {WW_u_U_Cont, WW_NULL_9, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_u = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_u};

const byte WW_STR_v[]              = {WW_v_V, WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_v = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_v};

const byte WW_STR_w[]              = {WW_w_W, WW_NULL_6, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_w = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_w};

const byte WW_STR_x[]              = {WW_x_X_POWERWISE, WW_NULL_6, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_x = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_x};

const byte WW_STR_y[]              = {WW_y_Y_12UP, WW_NULL_9, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_y = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_y};

const byte WW_STR_z[]              = {WW_z_Z, WW_NULL_3, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_z = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_z};

// 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 print strings.
const byte WW_STR_0[]              = {WW_0_RPAREN, WW_NULL_12, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_0 = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_0};

const byte WW_STR_1[]              = {WW_1_EXCLAMATION_Spell, WW_NULL_3, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_1 = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_1};

const byte WW_STR_2[]              = {WW_2_AT_Add, WW_NULL_6, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_2 = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_2};

const byte WW_STR_3[]              = {WW_3_POUND_Del, WW_NULL_7, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_3 = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_3};

const byte WW_STR_4[]              = {WW_4_DOLLAR_Vol, WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_4 = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_4};

const byte WW_STR_5[]              = {WW_5_PERCENT, WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_5 = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_5};

const byte WW_STR_6[]              = {WW_6_CENT, WW_NULL_9, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_6 = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_6};

const byte WW_STR_7[]              = {WW_7_AMPERSAND, WW_NULL_9, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_7 = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_7};

const byte WW_STR_8[]              = {WW_8_ASTERISK, WW_NULL_10, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_8 = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_8};

const byte WW_STR_9[]              = {WW_9_LPAREN_Stop, WW_NULL_11, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_9 = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_9};

// QV079 - stress case alphanumeric print string.
const byte WW_STR_QV079[]              = {WW_LShift, WW_q_Q_Impr, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_QV079 = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_QV079};

// !, @, #, $, %, <cent>, &, *, (, ) print strings.
const byte WW_STR_EXCLAMATION[]              = {WW_LShift, WW_1_EXCLAMATION_Spell, WW_LShift, WW_NULL_1, WW_NULL_14,
                                                WW_NULL};
const struct print_info WW_PRINT_EXCLAMATION = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_EXCLAMATION};

const byte WW_STR_AT[]                       = {WW_LShift, WW_2_AT_Add, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_AT          = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_AT};

const byte WW_STR_POUND[]                    = {WW_LShift, WW_3_POUND_Del, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_POUND       = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_POUND};

const byte WW_STR_DOLLAR[]                   = {WW_LShift, WW_4_DOLLAR_Vol, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_DOLLAR      = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_DOLLAR};

const byte WW_STR_PERCENT[]                  = {WW_LShift, WW_5_PERCENT, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_PERCENT     = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_PERCENT};

const byte WW_STR_CENT[]                     = {WW_LShift, WW_6_CENT, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_CENT        = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_CENT};

const byte WW_STR_AMPERSAND[]                = {WW_LShift, WW_7_AMPERSAND, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_AMPERSAND   = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_AMPERSAND};

const byte WW_STR_ASTERISK[]                 = {WW_LShift, WW_8_ASTERISK, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_ASTERISK    = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_ASTERISK};

const byte WW_STR_LPAREN[]                   = {WW_LShift, WW_9_LPAREN_Stop, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_LPAREN      = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_LPAREN};

const byte WW_STR_RPAREN[]                   = {WW_LShift, WW_0_RPAREN, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_RPAREN      = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_RPAREN};

// ., ,, [, ], =, +, /, ?, -, _, :, ;, ', ", <half>, <quarter>, +/-, <degree>, <superscript 2>, <superscript 3>,
// <section>, <paragraph> print strings.
const byte WW_STR_PERIOD[]                  = {WW_PERIOD_PERIOD, WW_NULL_11, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_PERIOD     = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_PERIOD};

const byte WW_STR_COMMA[]                   = {WW_COMMA_COMMA, WW_NULL_10, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_COMMA      = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_COMMA};

const byte WW_STR_LBRACKET[]                = {WW_LShift, WW_RBRACKET_LBRACKET_SUPER3, WW_LShift, WW_NULL_1, WW_NULL_14,
                                               WW_NULL};
const struct print_info WW_PRINT_LBRACKET   = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_LBRACKET};

const byte WW_STR_RBRACKET[]                = {WW_RBRACKET_LBRACKET_SUPER3, WW_NULL_10, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_RBRACKET   = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_RBRACKET};

const byte WW_STR_EQUAL[]                   = {WW_EQUAL_PLUS, WW_NULL_10, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_EQUAL      = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_EQUAL};

const byte WW_STR_PLUS[]                    = {WW_LShift, WW_EQUAL_PLUS, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_PLUS       = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_PLUS};

const byte WW_STR_SLASH[]                   = {WW_SLASH_QUESTION, WW_NULL_12, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_SLASH      = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_SLASH};

const byte WW_STR_QUESTION[]                = {WW_RShift, WW_SLASH_QUESTION, WW_RShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_QUESTION   = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_QUESTION};

const byte WW_STR_HYPHEN[]                  = {WW_HYPHEN_UNDERSCORE, WW_NULL_12, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_HYPHEN     = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_HYPHEN};

const byte WW_STR_UNDERSCORE[]              = {WW_LShift, WW_HYPHEN_UNDERSCORE, WW_LShift, WW_NULL_1, WW_NULL_14,
                                               WW_NULL};
const struct print_info WW_PRINT_UNDERSCORE = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_UNDERSCORE};

const byte WW_STR_COLON[]                   = {WW_LShift, WW_SEMICOLON_COLON_SECTION, WW_LShift, WW_NULL_1, WW_NULL_14,
                                               WW_NULL};
const struct print_info WW_PRINT_COLON      = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_COLON};

const byte WW_STR_SEMICOLON[]               = {WW_SEMICOLON_COLON_SECTION, WW_NULL_12, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_SEMICOLON  = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_SEMICOLON};

const byte WW_STR_APOSTROPHE[]              = {WW_APOSTROPHE_QUOTE_PARAGRAPH, WW_NULL_12, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_APOSTROPHE = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_APOSTROPHE};

const byte WW_STR_QUOTE[]                   = {WW_LShift, WW_APOSTROPHE_QUOTE_PARAGRAPH, WW_LShift, WW_NULL_1,
                                               WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_QUOTE      = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_QUOTE};

const byte WW_STR_HALF[]                    = {WW_HALF_QUARTER_SUPER2, WW_NULL_12, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_HALF       = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_HALF};

const byte WW_STR_QUARTER[]                 = {WW_LShift, WW_HALF_QUARTER_SUPER2, WW_LShift, WW_NULL_1, WW_NULL_14,
                                               WW_NULL};
const struct print_info WW_PRINT_QUARTER    = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_QUARTER};

const byte WW_STR_PLUSMINUS[]               = {WW_PLUSMINUS_DEGREE, WW_NULL_3, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_PLUSMINUS  = {SPACING_FORWARD, TIMING_NOSHIFT, &WW_STR_PLUSMINUS};

const byte WW_STR_DEGREE[]                  = {WW_LShift, WW_PLUSMINUS_DEGREE, WW_LShift, WW_NULL_1, WW_NULL_14,
                                               WW_NULL};
const struct print_info WW_PRINT_DEGREE     = {SPACING_FORWARD, TIMING_SHIFT, &WW_STR_DEGREE};

const byte WW_STR_SUPER2[]                  = {WW_Code, WW_HALF_QUARTER_SUPER2, WW_Code, WW_NULL_5, WW_NULL_14,
                                               WW_NULL};
const struct print_info WW_PRINT_SUPER2     = {SPACING_FORWARD, TIMING_CODE, &WW_STR_SUPER2};

const byte WW_STR_SUPER3[]                  = {WW_Code, WW_RBRACKET_LBRACKET_SUPER3, WW_Code, WW_NULL_5, WW_NULL_14,
                                               WW_NULL};
const struct print_info WW_PRINT_SUPER3     = {SPACING_FORWARD, TIMING_CODE, &WW_STR_SUPER3};

const byte WW_STR_SECTION[]                 = {WW_Code, WW_SEMICOLON_COLON_SECTION, WW_Code, WW_NULL_5, WW_NULL_14,
                                               WW_NULL};
const struct print_info WW_PRINT_SECTION    = {SPACING_FORWARD, TIMING_CODE, &WW_STR_SECTION};

const byte WW_STR_PARAGRAPH[]               = {WW_Code, WW_APOSTROPHE_QUOTE_PARAGRAPH, WW_Code, WW_NULL_5, WW_NULL_14,
                                               WW_NULL};
const struct print_info WW_PRINT_PARAGRAPH  = {SPACING_FORWARD, TIMING_CODE, &WW_STR_PARAGRAPH};

// Bold, Caps, Word, Cont print strings.
const byte WW_STR_Bold[]              = {WW_Code, WW_b_B_Bold, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Bold = {SPACING_NONE, TIMING_NONE, &WW_STR_Bold};

const byte WW_STR_Caps[]              = {WW_Code, WW_n_N_Caps, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Caps = {SPACING_NONE, TIMING_NONE, &WW_STR_Caps};

const byte WW_STR_Word[]              = {WW_Code, WW_i_I_Word, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Word = {SPACING_NONE, TIMING_NONE, &WW_STR_Word};

const byte WW_STR_Cont[]              = {WW_Code, WW_u_U_Cont, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Cont = {SPACING_NONE, TIMING_NONE, &WW_STR_Cont};

// Spell, Add, Del, A Rtn, Lang, Line Space, Impr, Vol, Ctr, Dec T, R Flsh, IndL, Indr Clr, RePrt, Stop print strings.
const byte WW_STR_Spell[]                  = {WW_Code, WW_1_EXCLAMATION_Spell, WW_Code, WW_NULL_3, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Spell     = {SPACING_NONE, TIMING_NONE, &WW_STR_Spell};

const byte WW_STR_Add[]                    = {WW_Code, WW_2_AT_Add, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Add       = {SPACING_NONE, TIMING_NONE, &WW_STR_Add};

const byte WW_STR_Del[]                    = {WW_Code, WW_3_POUND_Del, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Del       = {SPACING_NONE, TIMING_NONE, &WW_STR_Del};

const byte WW_STR_ARtn[]                   = {WW_Code, WW_r_R_ARtn, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_ARtn      = {SPACING_NONE, TIMING_NONE, &WW_STR_ARtn};

const byte WW_STR_Lang[]                   = {WW_Code, WW_l_L_Lang, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Lang      = {SPACING_NONE, TIMING_NONE, &WW_STR_Lang};

const byte WW_STR_LineSpace[]              = {WW_Code, WW_Reloc_LineSpace, WW_Code, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_LineSpace = {SPACING_NONE, TIMING_NONE, &WW_STR_LineSpace};

const byte WW_STR_Impr[]                   = {WW_Code, WW_q_Q_Impr, WW_Code, WW_NULL_3, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Impr      = {SPACING_NONE, TIMING_NONE, &WW_STR_Impr};

const byte WW_STR_Vol[]                    = {WW_Code, WW_4_DOLLAR_Vol, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Vol       = {SPACING_NONE, TIMING_NONE, &WW_STR_Vol};

const byte WW_STR_Ctr[]                    = {WW_Code, WW_c_C_Ctr, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Ctr       = {SPACING_NONE, TIMING_NONE, &WW_STR_Ctr};

const byte WW_STR_DecT[]                   = {WW_Code, WW_d_D_DecT, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_DecT      = {SPACING_NONE, TIMING_NONE, &WW_STR_DecT};

const byte WW_STR_RFlsh[]                  = {WW_Code, WW_o_O_RFlsh, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_RFlsh     = {SPACING_NONE, TIMING_NONE, &WW_STR_RFlsh};

const byte WW_STR_IndL[]                   = {WW_Code, WW_Tab_IndL, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_IndL      = {SPACING_NONE, TIMING_NONE, &WW_STR_IndL};

const byte WW_STR_IndClr[]                 = {WW_Code, WW_CRtn_IndClr, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_IndClr    = {SPACING_NONE, TIMING_NONE, &WW_STR_IndClr};

const byte WW_STR_RePrt[]                  = {WW_Code, WW_MarRel_RePrt, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_RePrt     = {SPACING_NONE, TIMING_NONE, &WW_STR_RePrt};

const byte WW_STR_Stop[]                   = {WW_Code, WW_9_LPAREN_Stop, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Stop      = {SPACING_NONE, TIMING_NONE, &WW_STR_Stop};

// <left arrow>, <right arrow>, <up arrow>, <down arrow>, <left word>, <right word>, <up line>, <down line>, Paper Up,
// Paper Down, <up micro>, <down micro>, <1/2 up>, <1/2 down>, Backspace, Bksp 1, <back x>, <back word>, Reloc print
// strings.
const byte WW_STR_LARROW[]                 = {WW_LARROW_Word, WW_NULL_13, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_LARROW    = {SPACING_BACKWARD, TIMING_HMOVE, &WW_STR_LARROW};

const byte WW_STR_RARROW[]                 = {WW_RARROW_Word, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_RARROW    = {SPACING_FORWARD, TIMING_HMOVE, &WW_STR_RARROW};

const byte WW_STR_UARROW[]                 = {WW_UARROW_Line, WW_NULL_13, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_UARROW    = {SPACING_NONE, TIMING_VMOVE, &WW_STR_UARROW};

const byte WW_STR_DARROW[]                 = {WW_DARROW_Line, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_DARROW    = {SPACING_NONE, TIMING_VMOVE, &WW_STR_DARROW};

const byte WW_STR_LWord[]                  = {WW_Code, WW_LARROW_Word, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_LWord     = {SPACING_UNKNOWN, TIMING_HMOVE2, &WW_STR_LWord};

const byte WW_STR_RWord[]                  = {WW_Code, WW_RARROW_Word, WW_Code, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_RWord     = {SPACING_UNKNOWN, TIMING_HMOVE2, &WW_STR_RWord};

const byte WW_STR_ULine[]                  = {WW_Code, WW_UARROW_Line, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_ULine     = {SPACING_NONE, TIMING_VMOVE2, &WW_STR_ULine};

const byte WW_STR_DLine[]                  = {WW_Code, WW_DARROW_Line, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_DLine     = {SPACING_NONE, TIMING_VMOVE2, &WW_STR_DLine};

const byte WW_STR_PaperUp[]                = {WW_PaperUp_Micro, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_PaperUp   = {SPACING_NONE, TIMING_VMOVE, &WW_STR_PaperUp};

const byte WW_STR_PaperDown[]              = {WW_PaperDown_Micro, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_PaperDown = {SPACING_NONE, TIMING_VMOVE, &WW_STR_PaperDown};

const byte WW_STR_UMicro[]                 = {WW_Code, WW_PaperUp_Micro, WW_Code, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_UMicro    = {SPACING_NONE, TIMING_VMOVE2, &WW_STR_UMicro};

const byte WW_STR_DMicro[]                 = {WW_Code, WW_PaperDown_Micro, WW_Code, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_DMicro    = {SPACING_NONE, TIMING_VMOVE2, &WW_STR_DMicro};

const byte WW_STR_12UP[]                   = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_12UP      = {SPACING_NONE, TIMING_VMOVE2, &WW_STR_12UP};

const byte WW_STR_12DOWN[]                 = {WW_Code, WW_h_H_12DOWN, WW_Code, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_12DOWN    = {SPACING_NONE, TIMING_VMOVE2, &WW_STR_12DOWN};

const byte WW_STR_Backspace[]              = {WW_Backspace_Bksp1, WW_NULL_13, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Backspace = {SPACING_BACKWARD, TIMING_HMOVE, &WW_STR_Backspace};

const byte WW_STR_Bksp1[]                  = {WW_Code, WW_Backspace_Bksp1, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Bksp1     = {SPACING_NONE, TIMING_HMOVE2, &WW_STR_Bksp1};

const byte WW_STR_BACKX[]                  = {WW_BACKX_Word, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_BACKX     = {SPACING_BACKWARD, TIMING_HMOVE, &WW_STR_BACKX};

const byte WW_STR_BWord[]                  = {WW_Code, WW_BACKX_Word, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_BWord     = {SPACING_UNKNOWN, TIMING_UNKNOWN, &WW_STR_BWord};

const byte WW_STR_Reloc[]                  = {WW_Reloc_LineSpace, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Reloc     = {SPACING_UNKNOWN, TIMING_UNKNOWN, &WW_STR_Reloc};

// L Mar, R Mar, Mar Rel, T Set, T Clr, Clr All print strings.
const byte WW_STR_LMar[]                 = {WW_LMar, WW_NULL_4, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_LMar    = {SPACING_LMAR, TIMING_NONE, &WW_STR_LMar};

const byte WW_STR_RMar[]                 = {WW_RMar, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_RMar    = {SPACING_RMAR, TIMING_NONE, &WW_STR_RMar};

const byte WW_STR_MarRel[]               = {WW_MarRel_RePrt, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_MarRel  = {SPACING_MARREL, TIMING_NONE, &WW_STR_MarRel};

const byte WW_STR_TSet[]                 = {WW_TSet, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_TSet    = {SPACING_TSET, TIMING_NONE, &WW_STR_TSet};

const byte WW_STR_TClr[]                 = {WW_TClr, WW_NULL_4, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_TClr    = {SPACING_TCLR, TIMING_NONE, &WW_STR_TClr};

const byte WW_STR_ClrAll[]               = {WW_TClr, WW_CRtn_IndClr, WW_TClr, WW_NULL_4, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_ClrAll  = {SPACING_CLRALL, TIMING_NONE, &WW_STR_ClrAll};
const struct print_info WW_PRINT_ClrAllX = {SPACING_NONE, TIMING_NONE, &WW_STR_ClrAll};
                                                       // Special case for Set_margins_tabs() that doesn't clear tabs[].

// Special function print strings.
const byte WW_STR_POWERWISE_OFF[]              = {WW_Code, WW_x_X_POWERWISE, WW_Code, WW_0_RPAREN, WW_Code, WW_NULL_5,
                                                  WW_NULL_14, WW_NULL}; 
const struct print_info WW_PRINT_POWERWISE_OFF = {SPACING_NONE, TIMING_NONE, &WW_STR_POWERWISE_OFF};

const byte WW_STR_SPELL_CHECK[]                = {WW_Code, WW_1_EXCLAMATION_Spell, WW_Code, WW_NULL_3, WW_NULL_14,
                                                  WW_NULL};
const struct print_info WW_PRINT_SPELL_CHECK   = {SPACING_NONE, TIMING_NONE, &WW_STR_SPELL_CHECK};

const byte WW_STR_BEEP[]                       = {WW_Code, WW_x_X_POWERWISE, WW_Code, WW_9_LPAREN_Stop, WW_Code,
                                                  WW_Code, WW_9_LPAREN_Stop, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_BEEP          = {SPACING_NONE, TIMING_NONE, &WW_STR_BEEP};


//**********************************************************************************************************************
//
//  Common data declarations.
//
//**********************************************************************************************************************

// Serial port for RS232 at slow baud rates.
SlowSoftSerial slow_serial_port (UART_RX_PIN, UART_TX_PIN);
volatile byte rs232_mode = RS232_UNDEFINED;

// Serial Interface Board variables.
volatile int blue_led_on = LOW;
volatile int blue_led_off = HIGH;
volatile byte row_out_3_pin = ROW_OUT_3_PIN;
volatile byte row_out_4_pin = ROW_OUT_4_PIN;
volatile byte serial_rts_pin = SERIAL_RTS_PIN;
volatile byte serial_cts_pin = SERIAL_CTS_PIN;

// Shift state variables.
volatile boolean shift = FALSE;         // Current shift state.
volatile boolean shift_lock = FALSE;    // Current shift lock state.
volatile boolean code = FALSE;          // Current code shift state.
volatile int key_offset = OFFSET_NONE;  // Offset into action table for shifted keys.

// Escape sequence variables.
volatile boolean escaping = FALSE;  // In escape sequence.

// End-of-line variables.
volatile boolean ignore_cr = FALSE;   // Receive ignore cr character.
volatile boolean ignore_lf = FALSE;   // Receive ignore lf character.
volatile boolean pending_cr = FALSE;  // Receive pending cr character.
volatile boolean pending_lf = FALSE;  // Receive pending lf character.

// Scan variables.
volatile int interrupt_column = WW_COLUMN_NULL;        // Current column.
volatile byte column_1_row_pin = ROW_OUT_NO_PIN;       // Latest column 1 row pin.
volatile byte column_5_row_pin = ROW_OUT_NO_PIN;       // Latest column 5 row pin.
volatile unsigned long interrupt_time = 0UL;           // Time of current interrupt.
volatile unsigned long last_scan_time = 0UL;           // Start time of last full scan.
volatile unsigned long last_scan_duration = 0UL;       // Duration of last full scan.
volatile unsigned long last_last_scan_duration = 0UL;  // Duration of last last full scan.

// Print timing variables (in usec).
volatile int residual_time = 0;  // Residual time toward next empty full scan needed.

// Key control variables.
volatile boolean key_pressed_states[NUM_WW_KEYS] = {FALSE};    // Current state of each key.
volatile unsigned long key_repeat_times[NUM_WW_KEYS] = {0UL};  // Time of repeating key press.
volatile unsigned long key_shadow_times[NUM_WW_KEYS] = {0UL};  // End of shadow time for debouncing.

// Error text table.
const char* error_text[NUM_ERRORS] = {NULL,
                                      "Command buffer full errors.",
                                      "Receive buffer full errors.",
                                      "Send buffer full errors.",
                                      "Transfer buffer full errors.",
                                      "Print buffer full errors.",
                                      "Bad print code errors."};

// Warning text table.
const char* warning_text[NUM_WARNINGS] = {NULL,
                                          "Short column scan warnings.",
                                          "Long column scan warnings.",
                                          "Unexpected column scan warnings."};

// Row output pin table.
volatile byte row_out_pins[16] = {ROW_OUT_NO_PIN, ROW_OUT_1_PIN, ROW_OUT_2_PIN, ROW_OUT_3_PIN, ROW_OUT_4_PIN,
                                  ROW_OUT_5_PIN, ROW_OUT_6_PIN, ROW_OUT_7_PIN, ROW_OUT_8_PIN, ROW_OUT_NO_PIN,
                                  ROW_OUT_NO_PIN, ROW_OUT_NO_PIN, ROW_OUT_NO_PIN, ROW_OUT_NO_PIN, ROW_OUT_NO_PIN,
                                  ROW_OUT_NO_PIN};

// Databits parity stopbits tables.
const int data_parity_stops[NUM_DPSS] = {0, SERIAL_7O1, SERIAL_7E1, SERIAL_8N1, SERIAL_8O1, SERIAL_8E1, SERIAL_8N2,
                                         SERIAL_8O2, SERIAL_8E2};
const int data_parity_stops_slow[NUM_DPSS] = {0, SSS_SERIAL_7O1, SSS_SERIAL_7E1, SSS_SERIAL_8N1, SSS_SERIAL_8O1,
                                         SSS_SERIAL_8E1, SSS_SERIAL_8N2, SSS_SERIAL_8O2, SSS_SERIAL_8E2};
const char* data_parity_stops_text[NUM_DPSS] = {NULL, "7o1", "7e1", "8n1", "8o1", "8e1", "8n2", "8o2", "8e2"};

// Baud rate table.  Note: Baud rates below 1200 are not usable with the hardware UART on Teensy 3.5,
//                         and baud rates above 230400 are not supported by MAX3232.
const unsigned long baud_rates[NUM_BAUDS] = {0ul, 50ul, 75ul, 110ul, 134ul, 150ul, 200ul, 300ul, 600ul, 1200ul,
                                             1800ul, 2400ul, 4800ul, 9600ul, 19200ul, 38400ul, 57600ul, 76800ul,
                                             115200ul, 230400ul, 460800ul, 921600ul};

// Parity lookup tables.
const byte odd_parity[128]  = {0x80, 0x01, 0x02, 0x83, 0x04, 0x85, 0x86, 0x07,
                               0x08, 0x89, 0x8a, 0x0b, 0x8c, 0x0d, 0x0e, 0x8f,
                               0x10, 0x91, 0x92, 0x13, 0x94, 0x15, 0x16, 0x97,
                               0x98, 0x19, 0x1a, 0x9b, 0x1c, 0x9d, 0x9e, 0x1f,
                               0x20, 0xa1, 0xa2, 0x23, 0xa4, 0x25, 0x26, 0xa7,
                               0xa8, 0x29, 0x2a, 0xab, 0x2c, 0xad, 0xae, 0x2f,
                               0xb0, 0x31, 0x32, 0xb3, 0x34, 0xb5, 0xb6, 0x37,
                               0x38, 0xb9, 0xba, 0x3b, 0xbc, 0x3d, 0x3e, 0xbf,
                               0x40, 0xc1, 0xc2, 0x43, 0xc4, 0x45, 0x46, 0xc7,
                               0xc8, 0x49, 0x4a, 0xcb, 0x4c, 0xcd, 0xce, 0x4f,
                               0xd0, 0x51, 0x52, 0xd3, 0x54, 0xd5, 0xd6, 0x57,
                               0x58, 0xd9, 0xda, 0x5b, 0xdc, 0x5d, 0x5e, 0xdf,
                               0xe0, 0x61, 0x62, 0xe3, 0x64, 0xe5, 0xe6, 0x67,
                               0x68, 0xe9, 0xea, 0x6b, 0xec, 0x6d, 0x6e, 0xef,
                               0x70, 0xf1, 0xf2, 0x73, 0xf4, 0x75, 0x76, 0xf7,
                               0xf8, 0x79, 0x7a, 0xfb, 0x7c, 0xfd, 0xfe, 0x7f};
const byte even_parity[128] = {0x00, 0x81, 0x82, 0x03, 0x84, 0x05, 0x06, 0x87,
                               0x88, 0x09, 0x0a, 0x8b, 0x0c, 0x8d, 0x8e, 0x0f,
                               0x90, 0x11, 0x12, 0x93, 0x14, 0x95, 0x96, 0x17,
                               0x18, 0x99, 0x9a, 0x1b, 0x9c, 0x1d, 0x1e, 0x9f,
                               0xa0, 0x21, 0x22, 0xa3, 0x24, 0xa5, 0xa6, 0x27,
                               0x28, 0xa9, 0xaa, 0x2b, 0xac, 0x2d, 0x2e, 0xaf,
                               0x30, 0xb1, 0xb2, 0x33, 0xb4, 0x35, 0x36, 0xb7,
                               0xb8, 0x39, 0x3a, 0xbb, 0x3c, 0xbd, 0xbe, 0x3f,
                               0xc0, 0x41, 0x42, 0xc3, 0x44, 0xc5, 0xc6, 0x47,
                               0x48, 0xc9, 0xca, 0x4b, 0xcc, 0x4d, 0x4e, 0xcf,
                               0x50, 0xd1, 0xd2, 0x53, 0xd4, 0x55, 0x56, 0xd7,
                               0xd8, 0x59, 0x5a, 0xdb, 0x5c, 0xdd, 0xde, 0x5f,
                               0x60, 0xe1, 0xe2, 0x63, 0xe4, 0x65, 0x66, 0xe7,
                               0xe8, 0x69, 0x6a, 0xeb, 0x6c, 0xed, 0xee, 0x6f,
                               0xf0, 0x71, 0x72, 0xf3, 0x74, 0xf5, 0xf6, 0x77,
                               0x78, 0xf9, 0xfa, 0x7b, 0xfc, 0x7d, 0x7e, 0xff};

// Print code validation table.
const boolean pc_validation[256] = {
        TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  FALSE, TRUE,  FALSE, TRUE,  TRUE,  TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  FALSE, TRUE,  FALSE, TRUE,  TRUE,  FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  FALSE, TRUE,  FALSE, TRUE,  TRUE,  FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  FALSE, TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  FALSE, TRUE,  FALSE, TRUE,  TRUE,  FALSE, TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  TRUE,  FALSE, TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  TRUE,  TRUE,  FALSE, TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  FALSE, TRUE,  TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,  TRUE};

// Escape termination table.
const boolean terminate_escape[128] = {
        FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE, 
        TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  FALSE, FALSE, TRUE,  TRUE,  TRUE, 
        TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE, 
        TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  FALSE};

// Key action variables.
volatile const struct key_action (*key_actions)[3 * NUM_WW_KEYS] = NULL;       // Current key action table.
volatile const struct key_action (*key_actions_save)[3 * NUM_WW_KEYS] = NULL;  // Save current key action table.

// Serial action variables.
volatile const struct serial_action (*serial_actions)[128] = NULL;  // Current serial action table.

// Run mode.
volatile byte run_mode = MODE_INITIALIZING;  // Current typewriter run mode.

// Error & warning variables.
volatile int total_errors = 0;              // Total count of errors.
volatile int error_counts[NUM_ERRORS];      // Count of errors by error code.
volatile int total_warnings = 0;            // Total count of warnings.
volatile int warning_counts[NUM_WARNINGS];  // Count of warnings by warning code.

// Print variables.
volatile int current_column = 1;                           // Current typewriter print column.
volatile const struct print_info *previous_string = NULL;  // Previous print string.

// Flow control variables.
volatile boolean flow_in_on = FALSE;   // Input flow control turned on.
volatile boolean flow_out_on = FALSE;  // Output flow control turned on.
volatile byte flow_on = 0x00;          // Turn on flow control character.
volatile byte flow_off = 0x00;         // Turn off flow control character.

// Command buffer variables (write: ISR, read: main).
volatile int cb_read = 0;   // Index of current read position of command buffer.
volatile int cb_write = 0;  // Index of current write position of command buffer.
volatile int cb_count = 0;  // Count of characters in command buffer.
volatile byte command_buffer[SIZE_COMMAND_BUFFER] = {0};  // Circular command buffer.

// Receive buffer variables (write: main, read: main).
volatile int rb_read = 0;   // Index of current read position of receive buffer.
volatile int rb_write = 0;  // Index of current write position of receive buffer.
volatile int rb_count = 0;  // Count of characters in receive buffer.
volatile byte receive_buffer[SIZE_RECEIVE_BUFFER] = {0};  // Circular receive buffer.

// Send buffer variables (write: ISR, read: main).
volatile int sb_read = 0;   // Index of current read position of send buffer.
volatile int sb_write = 0;  // Index of current write position of send buffer.
volatile int sb_count = 0;  // Count of of characters in send buffer.
volatile byte send_buffer[SIZE_SEND_BUFFER] = {0};  // Circular send buffer.

// Transfer buffer variables (write: ISR, read: main).
volatile int tb_read = 0;   // Index of current read position of print buffer.
volatile int tb_write = 0;  // Index of current write position of print buffer.
volatile int tb_count = 0;  // Count of of characters in print buffer.
volatile const struct print_info *transfer_buffer[SIZE_TRANSFER_BUFFER] = {NULL};  // Circular transfer buffer.

// Print buffer variables (write: main, read: ISR).
volatile int pb_read = 0;   // Index of current read position of print buffer.
volatile int pb_write = 0;  // Index of current write position of print buffer.
volatile int pb_count = 0;  // Count of of characters in print buffer.
volatile byte print_buffer[SIZE_PRINT_BUFFER] = {0};  // Circular print buffer.

// Configuration parameters stored in EEPROM.
volatile byte errors = INITIAL_ERRORS;              // Report errors.               Used by all emulations.
volatile byte warnings = INITIAL_WARNINGS;          // Report warnings.             Used by all emulations.
volatile byte battery = INITIAL_BATTERY;            // Battery installed.           Used by all emulations.
volatile byte lmargin = INITIAL_LMARGIN;            // Left margin.                 Used by all emulations.
volatile byte rmargin = INITIAL_RMARGIN;            // Right margin.                Used by all emulations.
volatile byte emulation = INITIAL_EMULATION;        // Current emulation.           Used by all emulations.

volatile byte slash = INITIAL_SLASH;                // Print slashed zeroes.        Used by IBM 1620 Jr.
volatile byte bold = INITIAL_BOLD;                  // Print bold input.            Used by IBM 1620 Jr.

volatile byte serial = INITIAL_SERIAL;              // Serial type.                 Used by ASCII Terminal.
volatile byte duplex = INITIAL_DUPLEX;              // Duplex.                      Used by ASCII Terminal.
volatile byte baud = INITIAL_BAUD;                  // Baud rate.                   Used by ASCII Terminal (RS-232).
volatile byte parity = INITIAL_PARITY;              // Parity.                      Used by ASCII Terminal (USB).
volatile byte dps = INITIAL_DPS;                    // Databits, parity, stopbits.  Used by ASCII Terminal (RS-232).
volatile byte swflow = INITIAL_SWFLOW;              // Software flow control.       Used by ASCII Terminal.
volatile byte hwflow = INITIAL_HWFLOW;              // Hardware flow control.       Used by ASCII Terminal (RS-232).
volatile byte uppercase = INITIAL_UPPERCASE;        // Uppercase only.              Used by ASCII Terminal.
volatile byte autoreturn = INITIAL_AUTORETURN;      // Auto return.                 Used by ASCII Terminal.
volatile byte transmiteol = INITIAL_TRANSMITEOL;    // Send end-of-line.            Used by ASCII Terminal.
volatile byte receiveeol = INITIAL_RECEIVEEOL;      // Receive end-of-line.         Used by ASCII Terminal.
volatile byte ignoreescape = INITIAL_IGNOREESCAPE;  // Ignore escape sequences.     Used by ASCII Terminal.
volatile byte width = INITIAL_WIDTH;                // Paper width.                 Used by ASCII Terminal.
volatile byte offset = INITIAL_OFFSET;              // Column offset.               Used by ASCII Terminal.
volatile byte tabs[166] = {SETTING_UNDEFINED};      // Tab settings.                Used by ASCII Terminal.


//======================================================================================================================
//
//  IBM 1620 Jr. Console Typewriter emulation.
//
//======================================================================================================================

//**********************************************************************************************************************
//
//  IBM 1620 Jr. Console Typewriter defines.
//
//**********************************************************************************************************************

// IBM 1620 Jr. name strings.
#define IBM_NAME     "IBM 1620 Jr."
#define IBM_VERSION  IBM_NAME " (v" VERSION_TEXT ")"

// IBM 1620 Jr. key values.
#define IBM_KEY_LSHIFT           WW_KEY_LShift                      // <left shift> key code.
#define IBM_KEY_RSHIFT           WW_KEY_RShift                      // <right shift> key code.
#define IBM_KEY_RARROW           WW_KEY_RARROW_Word                 // <right arrow> key code.
#define IBM_KEY_DARROW           WW_KEY_DARROW_Line                 // <down arrow> key code.
#define IBM_KEY_Z                WW_KEY_z_Z                         // Z key code.
#define IBM_KEY_Q                WW_KEY_q_Q_Impr                    // Q key code.
#define IBM_KEY_RELEASESTART     WW_KEY_PLUSMINUS_DEGREE            // <release start> key code.
#define IBM_KEY_A                WW_KEY_a_A                         // A key code.
#define IBM_KEY_SPACE            WW_KEY_SPACE_REQSPACE              // <space> key code.
#define IBM_KEY_LOADPAPER        WW_KEY_LOADPAPER                   // <load paper> key code.
#define IBM_KEY_LMAR             WW_KEY_LMar                        // <left margin> key code.
#define IBM_KEY_TCLR             WW_KEY_TClr                        // <tab clear> key code.
#define IBM_KEY_X                WW_KEY_x_X_POWERWISE               // X key code.
#define IBM_KEY_W                WW_KEY_w_W                         // W key code.
#define IBM_KEY_S                WW_KEY_s_S                         // S key code.
#define IBM_KEY_C                WW_KEY_c_C_Ctr                     // C key code.
#define IBM_KEY_E                WW_KEY_e_E                         // E key code.
#define IBM_KEY_D                WW_KEY_d_D_DecT                    // D key code.
#define IBM_KEY_B                WW_KEY_b_B_Bold                    // B key code.
#define IBM_KEY_V                WW_KEY_v_V                         // V key code.
#define IBM_KEY_T                WW_KEY_t_T                         // T key code.
#define IBM_KEY_R                WW_KEY_r_R_ARtn                    // R key code.
#define IBM_KEY_AT               WW_KEY_4_DOLLAR_Vol                // @ key code.
#define IBM_KEY_LPAREN           WW_KEY_5_PERCENT                   // ( key code.
#define IBM_KEY_F                WW_KEY_f_F                         // F key code.
#define IBM_KEY_G                WW_KEY_g_G                         // G key code.
#define IBM_KEY_N                WW_KEY_n_N_Caps                    // N key code.
#define IBM_KEY_M_7              WW_KEY_m_M                         // M, 7 key code.
#define IBM_KEY_Y                WW_KEY_y_Y_12UP                    // Y key code.
#define IBM_KEY_U_1              WW_KEY_u_U_Cont                    // U, 1 key code.
#define IBM_KEY_FLAG             WW_KEY_7_AMPERSAND                 // <flag> key code.
#define IBM_KEY_RPAREN           WW_KEY_6_CENT                      // ) key code.
#define IBM_KEY_J_4              WW_KEY_j_J                         // J, 4 key code.
#define IBM_KEY_H                WW_KEY_h_H_12DOWN                  // H key code.
#define IBM_KEY_COMMA_8          WW_KEY_COMMA_COMMA                 // ,, 8 key code.
#define IBM_KEY_I_2              WW_KEY_i_I_Word                    // I, 2 key code.
#define IBM_KEY_EQUAL            WW_KEY_8_ASTERISK                  // = key code.
#define IBM_KEY_GMARK            WW_KEY_EQUAL_PLUS                  // <group mark> key code.
#define IBM_KEY_K_5              WW_KEY_k_K                         // K, 5 key code.
#define IBM_KEY_PERIOD_9         WW_KEY_PERIOD_PERIOD               // ., 9 key code.
#define IBM_KEY_O_3              WW_KEY_o_O_RFlsh                   // O, 3 key code.
#define IBM_KEY_0                WW_KEY_9_LPAREN_Stop               // 0 key code.
#define IBM_KEY_L_6              WW_KEY_l_L_Lang                    // L, 6 key code.
#define IBM_KEY_SLASH            WW_KEY_SLASH_QUESTION              // / key code.
#define IBM_KEY_HYPHEN           WW_KEY_HALF_QUARTER_SUPER2         // - key code.
#define IBM_KEY_P                WW_KEY_p_P                         // P key code.
#define IBM_KEY_ASTERISK_PERIOD  WW_KEY_0_RPAREN                    // *, . key code.
#define IBM_KEY_RMARK            WW_KEY_HYPHEN_UNDERSCORE           // <record mark> key code.
#define IBM_KEY_PLUS             WW_KEY_SEMICOLON_COLON_SECTION     // + key code.
#define IBM_KEY_DOLLAR           WW_KEY_APOSTROPHE_QUOTE_PARAGRAPH  // $ key code.
#define IBM_KEY_LARROW           WW_KEY_LARROW_Word                 // <left arrow> key code.
#define IBM_KEY_UARROW           WW_KEY_UARROW_Line                 // <up arrow> key code.
#define IBM_KEY_BACKSPACE        WW_KEY_Backspace_Bksp1             // <backspace> key code.
#define IBM_KEY_RETURN           WW_KEY_CRtn_IndClr                 // <return> key code.
#define IBM_KEY_LOCK             WW_KEY_Lock                        // <shift lock> key code.
#define IBM_KEY_RMAR             WW_KEY_RMar                        // <right margin> key code.
#define IBM_KEY_TAB              WW_KEY_Tab_IndL                    // <tab> key code.
#define IBM_KEY_MARREL           WW_KEY_MarRel_RePrt                // <margin release> key code.
#define IBM_KEY_TSET             WW_KEY_TSet                        // <tab set> key code.

// IBM 1620 Jr. print string timing values (in usec).
#define TIMING_IBM_FLAG           (POSITIVE((1 * TIME_CHARACTER + 1 * TIME_HMOVEMENT + 2 * TIME_VMOVEMENT) - \
                                            (2 * FSCAN_1_CHANGE + 3 * FSCAN_2_CHANGES + 0 * FSCAN_3_CHANGES) + \
                                            (0 * TIME_ADJUST)))
                                    // Residual time for flag print character.

#define TIMING_IBM_SLASH_0        (POSITIVE((2 * TIME_CHARACTER + 1 * TIME_HMOVEMENT + 0 * TIME_VMOVEMENT) - \
                                            (4 * FSCAN_1_CHANGE + 1 * FSCAN_2_CHANGES + 0 * FSCAN_3_CHANGES) + \
                                            (17 * TIME_ADJUST)))
                                    // Residual time for slashed zero print character.

#define TIMING_IBM_FLAG_SLASH_0   (POSITIVE((3 * TIME_CHARACTER + 2 * TIME_HMOVEMENT + 2 * TIME_VMOVEMENT) - \
                                            (6 * FSCAN_1_CHANGE + 4 * FSCAN_2_CHANGES + 0 * FSCAN_3_CHANGES) + \
                                            (54 * TIME_ADJUST)))
                                    // Residual time for flagged slashed zero print character.

#define TIMING_IBM_FLAG_DIGIT     (POSITIVE((2 * TIME_CHARACTER + 1 * TIME_HMOVEMENT + 2 * TIME_VMOVEMENT) - \
                                            (4 * FSCAN_1_CHANGE + 3 * FSCAN_2_CHANGES + 0 * FSCAN_3_CHANGES) + \
                                            (19 * TIME_ADJUST)))
                                    // Residual time for flagged digit print character.

#define TIMING_IBM_FLAG_NUMBLANK  (POSITIVE((2 * TIME_CHARACTER + 1 * TIME_HMOVEMENT + 2 * TIME_VMOVEMENT) - \
                                            (4 * FSCAN_1_CHANGE + 4 * FSCAN_2_CHANGES + 0 * FSCAN_3_CHANGES) + \
                                            (-4 * TIME_ADJUST)))
                                    // Residual time for flagged numeric blank print character.

#define TIMING_IBM_RMARK          (POSITIVE((3 * TIME_CHARACTER + 2 * TIME_HMOVEMENT + 0 * TIME_VMOVEMENT) - \
                                            (6 * FSCAN_1_CHANGE + 3 * FSCAN_2_CHANGES + 0 * FSCAN_3_CHANGES) + \
                                            (-10 * TIME_ADJUST)))
                                    // Residual time for record mark print character.

#define TIMING_IBM_FLAG_RMARK     (POSITIVE((4 * TIME_CHARACTER + 3 * TIME_HMOVEMENT + 2 * TIME_VMOVEMENT) - \
                                            (8 * FSCAN_1_CHANGE + 6 * FSCAN_2_CHANGES + 0 * FSCAN_3_CHANGES) + \
                                            (-4 * TIME_ADJUST)))
                                    // Residual time for flagged record mark print character.

#define TIMING_IBM_GMARK          (POSITIVE((4 * TIME_CHARACTER + 3 * TIME_HMOVEMENT + 6 * TIME_VMOVEMENT) - \
                                            (18 * FSCAN_1_CHANGE + 5 * FSCAN_2_CHANGES + 2 * FSCAN_3_CHANGES) + \
                                            (12 * TIME_ADJUST)))
                                    // Residual time for group mark print character.

#define TIMING_IBM_FLAG_GMARK     (POSITIVE((5 * TIME_CHARACTER + 4 * TIME_HMOVEMENT + 8 * TIME_VMOVEMENT) - \
                                            (20 * FSCAN_1_CHANGE + 8 * FSCAN_2_CHANGES + 2 * FSCAN_3_CHANGES) + \
                                            (26 * TIME_ADJUST)))
                                    // Residual time for flagged group mark print character.

#define TIMING_IBM_RELEASESTART   (POSITIVE((2 * TIME_CHARACTER + 4 * TIME_HMOVEMENT + 5 * TIME_VMOVEMENT) - \
                                            (20 * FSCAN_1_CHANGE + 4 * FSCAN_2_CHANGES + 1 * FSCAN_3_CHANGES) + \
                                            (-33 * TIME_ADJUST)))
                                    // Residual time for release start print character.

#define TIMING_IBM_INVALID        (POSITIVE((3 * TIME_CHARACTER + 2 * TIME_HMOVEMENT + 0 * TIME_VMOVEMENT) - \
                                            (6 * FSCAN_1_CHANGE + 4 * FSCAN_2_CHANGES + 0 * FSCAN_3_CHANGES) + \
                                            (-5 * TIME_ADJUST)))
                                     // Residual time for invalid print character.

// IBM 1620 Jr. command characters.
#define CHAR_IBM_MODE_0   'a'  // Mode 0 command character.
#define CHAR_IBM_MODE_1   'b'  // Mode 1 command character.
#define CHAR_IBM_MODE_2   'c'  // Mode 2 command character.
#define CHAR_IBM_MODE_3   'd'  // Mode 3 command character.
#define CHAR_IBM_PING     'g'  // Ping command character.
#define CHAR_IBM_ACK      'h'  // Ack command character.
#define CHAR_IBM_SLASH    's'  // Slash command character.
#define CHAR_IBM_UNSLASH  'u'  // Unslash command character.
#define CHAR_IBM_RESET    'z'  // Reset command character.

#define CHAR_IBM_PAUSE    '%'  // Pause command character.
#define CHAR_IBM_RESUME   '&'  // Resume command character.


//**********************************************************************************************************************
//
//  IBM 1620 Jr. print strings.
//
//**********************************************************************************************************************

// <flag>, <slash 0>, <flag slash 0>, <flag 0>, <flag 1>, <flag 2>, <flag 3>, <flag 4>, <flag 5>, <flag 6>, <flag 7>,
// <flag 8>, <slash 9> print strings.
const byte IBM_STR_FLAG[]                      = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_5, WW_HYPHEN_UNDERSCORE,
                                                  WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13, WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_FLAG         = {SPACING_NONE, TIMING_IBM_FLAG, &IBM_STR_FLAG};

const byte IBM_STR_SLASH_0[]                   = {WW_0_RPAREN, WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13,
                                                  WW_SLASH_QUESTION, WW_NULL_12, WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_SLASH_0      = {SPACING_FORWARD, TIMING_IBM_SLASH_0, &IBM_STR_SLASH_0};

const byte IBM_STR_FLAG_SLASH_0[]              = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_5, WW_HYPHEN_UNDERSCORE,
                                                  WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13, WW_0_RPAREN, WW_NULL_12,
                                                  WW_Backspace_Bksp1, WW_NULL_13, WW_SLASH_QUESTION, WW_NULL_12,
                                                  WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_FLAG_SLASH_0 = {SPACING_FORWARD, TIMING_IBM_FLAG_SLASH_0, &IBM_STR_FLAG_SLASH_0};

const byte IBM_STR_FLAG_0[]                    = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_5, WW_HYPHEN_UNDERSCORE,
                                                  WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13, WW_0_RPAREN, WW_NULL_12,
                                                  WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_FLAG_0       = {SPACING_FORWARD, TIMING_IBM_FLAG_DIGIT, &IBM_STR_FLAG_0};

const byte IBM_STR_FLAG_1[]                    = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_5, WW_HYPHEN_UNDERSCORE,
                                                  WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13, WW_1_EXCLAMATION_Spell,
                                                  WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_FLAG_1       = {SPACING_FORWARD, TIMING_IBM_FLAG_DIGIT, &IBM_STR_FLAG_1};

const byte IBM_STR_FLAG_2[]                    = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_5, WW_HYPHEN_UNDERSCORE,
                                                  WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13, WW_2_AT_Add, WW_NULL_6,
                                                  WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_FLAG_2       = {SPACING_FORWARD, TIMING_IBM_FLAG_DIGIT, &IBM_STR_FLAG_2};

const byte IBM_STR_FLAG_3[]                    = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_5, WW_HYPHEN_UNDERSCORE,
                                                  WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13, WW_3_POUND_Del, WW_NULL_7,
                                                  WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_FLAG_3       = {SPACING_FORWARD, TIMING_IBM_FLAG_DIGIT, &IBM_STR_FLAG_3};

const byte IBM_STR_FLAG_4[]                    = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_5, WW_HYPHEN_UNDERSCORE,
                                                  WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13, WW_4_DOLLAR_Vol,
                                                  WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_FLAG_4       = {SPACING_FORWARD, TIMING_IBM_FLAG_DIGIT, &IBM_STR_FLAG_4};

const byte IBM_STR_FLAG_5[]                    = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_5, WW_HYPHEN_UNDERSCORE,
                                                  WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13, WW_5_PERCENT, WW_NULL_8,
                                                  WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_FLAG_5       = {SPACING_FORWARD, TIMING_IBM_FLAG_DIGIT, &IBM_STR_FLAG_5};

const byte IBM_STR_FLAG_6[]                    = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_5, WW_HYPHEN_UNDERSCORE,
                                                  WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13, WW_6_CENT, WW_NULL_9,
                                                  WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_FLAG_6       = {SPACING_FORWARD, TIMING_IBM_FLAG_DIGIT, &IBM_STR_FLAG_6};

const byte IBM_STR_FLAG_7[]                    = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_5, WW_HYPHEN_UNDERSCORE,
                                                  WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13, WW_7_AMPERSAND, WW_NULL_9,
                                                  WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_FLAG_7       = {SPACING_FORWARD, TIMING_IBM_FLAG_DIGIT, &IBM_STR_FLAG_7};

const byte IBM_STR_FLAG_8[]                    = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_5, WW_HYPHEN_UNDERSCORE,
                                                  WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13, WW_8_ASTERISK, WW_NULL_10,
                                                  WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_FLAG_8       = {SPACING_FORWARD, TIMING_IBM_FLAG_DIGIT, &IBM_STR_FLAG_8};

const byte IBM_STR_FLAG_9[]                    = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_5, WW_HYPHEN_UNDERSCORE,
                                                  WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13, WW_9_LPAREN_Stop,
                                                  WW_NULL_11, WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_FLAG_9       = {SPACING_FORWARD, TIMING_IBM_FLAG_DIGIT, &IBM_STR_FLAG_9};

// <flag numblank> print strings.
const byte IBM_STR_FLAG_NUMBLANK[]              = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_5, WW_HYPHEN_UNDERSCORE,
                                                   WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13, WW_LShift, WW_2_AT_Add,
                                                   WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_FLAG_NUMBLANK = {SPACING_FORWARD, TIMING_IBM_FLAG_NUMBLANK, &IBM_STR_FLAG_NUMBLANK};

// <rmark>, <flag rmark>, <gmark>, <flag gmark> print strings.
const byte IBM_STR_RMARK[]                   = {WW_EQUAL_PLUS, WW_NULL_10, WW_Backspace_Bksp1, WW_NULL_13, WW_LShift,
                                                WW_1_EXCLAMATION_Spell, WW_LShift, WW_NULL_1, WW_Backspace_Bksp1,
                                                WW_NULL_13, WW_i_I_Word, WW_NULL_10, WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_RMARK      = {SPACING_FORWARD, TIMING_IBM_RMARK, &IBM_STR_RMARK};

const byte IBM_STR_FLAG_RMARK[]              = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_5, WW_HYPHEN_UNDERSCORE,
                                                WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13, WW_EQUAL_PLUS, WW_NULL_10,
                                                WW_Backspace_Bksp1, WW_NULL_13, WW_LShift, WW_1_EXCLAMATION_Spell,
                                                WW_LShift, WW_NULL_1, WW_Backspace_Bksp1, WW_NULL_13, WW_i_I_Word,
                                                WW_NULL_10, WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_FLAG_RMARK = {SPACING_FORWARD, TIMING_IBM_FLAG_RMARK, &IBM_STR_FLAG_RMARK};

const byte IBM_STR_GMARK[]                   = {WW_Code, WW_PaperUp_Micro, WW_Code, WW_NULL_2, WW_EQUAL_PLUS,
                                                WW_NULL_10, WW_Backspace_Bksp1, WW_NULL_13, WW_Code, WW_PaperDown_Micro,
                                                WW_Code, WW_Code, WW_PaperDown_Micro, WW_Code, WW_Code,
                                                WW_PaperDown_Micro, WW_Code, WW_NULL_2, WW_HYPHEN_UNDERSCORE,
                                                WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13, WW_Code, WW_PaperUp_Micro,
                                                WW_Code, WW_Code, WW_PaperUp_Micro, WW_Code, WW_NULL_2, WW_LShift,
                                                WW_1_EXCLAMATION_Spell, WW_LShift, WW_NULL_1, WW_Backspace_Bksp1,
                                                WW_NULL_13, WW_i_I_Word, WW_NULL_10, WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_GMARK      = {SPACING_FORWARD, TIMING_IBM_GMARK, &IBM_STR_GMARK};

const byte IBM_STR_FLAG_GMARK[]              = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_5, WW_HYPHEN_UNDERSCORE,
                                                WW_NULL_12, WW_Backspace_Bksp1, WW_NULL_13, WW_Code, WW_PaperUp_Micro,
                                                WW_Code, WW_NULL_2, WW_EQUAL_PLUS, WW_NULL_10, WW_Backspace_Bksp1,
                                                WW_NULL_13, WW_Code, WW_PaperDown_Micro, WW_Code, WW_Code,
                                                WW_PaperDown_Micro, WW_Code, WW_Code, WW_PaperDown_Micro, WW_Code,
                                                WW_NULL_2, WW_HYPHEN_UNDERSCORE, WW_NULL_12, WW_Backspace_Bksp1,
                                                WW_NULL_13, WW_Code, WW_PaperUp_Micro, WW_Code, WW_Code,
                                                WW_PaperUp_Micro, WW_Code, WW_NULL_2, WW_LShift,
                                                WW_1_EXCLAMATION_Spell, WW_LShift, WW_NULL_1, WW_Backspace_Bksp1,
                                                WW_NULL_13, WW_i_I_Word, WW_NULL_10, WW_NULL_14, WW_NULL};
const struct print_info IBM_PRINT_FLAG_GMARK  = {SPACING_FORWARD, TIMING_IBM_FLAG_GMARK, &IBM_STR_FLAG_GMARK};

// <release start>, <invalid> print strings.
const byte IBM_STR_RELEASESTART[]              = {WW_Code, WW_PaperDown_Micro, WW_Code, WW_Code, WW_PaperDown_Micro,
                                                  WW_Code, WW_NULL_2, WW_LShift, WW_r_R_ARtn, WW_LShift, WW_NULL_1,
                                                  WW_Code, WW_Backspace_Bksp1, WW_Code, WW_Code, WW_Backspace_Bksp1,
                                                  WW_Code, WW_Code, WW_Backspace_Bksp1, WW_Code, WW_Code,
                                                  WW_Backspace_Bksp1, WW_Code, WW_NULL_5, WW_PaperUp_Micro, WW_NULL_2,
                                                  WW_LShift, WW_s_S, WW_LShift, WW_NULL_1, WW_Code, WW_PaperDown_Micro,
                                                  WW_Code, WW_Code, WW_PaperDown_Micro, WW_Code, WW_NULL_2, WW_NULL_14,
                                                  WW_NULL};
const struct print_info IBM_PRINT_RELEASESTART = {SPACING_FORWARD, TIMING_IBM_RELEASESTART, &IBM_STR_RELEASESTART};

const byte IBM_STR_INVALID[]                   = {WW_LShift, WW_x_X_POWERWISE, WW_LShift, WW_NULL_1, WW_Backspace_Bksp1,
                                                  WW_NULL_13, WW_LShift, WW_1_EXCLAMATION_Spell, WW_LShift, WW_NULL_1,
                                                  WW_Backspace_Bksp1, WW_NULL_13, WW_i_I_Word, WW_NULL_10, WW_NULL_14,
                                                  WW_NULL};
const struct print_info IBM_PRINT_INVALID      = {SPACING_FORWARD, TIMING_IBM_INVALID, &IBM_STR_INVALID};


//**********************************************************************************************************************
//
//  IBM 1620 Jr. serial action table.
//
//**********************************************************************************************************************

const struct serial_action IBM_SERIAL_ACTIONS[128] = { 
  {CMD_NONE,        NULL},                      // NUL
  {CMD_NONE,        NULL},                      // SOH
  {CMD_NONE,        NULL},                      // STX
  {CMD_NONE,        NULL},                      // ETX
  {CMD_NONE,        NULL},                      // EOT
  {CMD_NONE,        NULL},                      // ENQ
  {CMD_NONE,        NULL},                      // ACK
  {CMD_NONE,        NULL},                      // BEL
  {CMD_NONE,        NULL},                      // BS
  {CMD_NONE,        NULL},                      // TAB
  {CMD_NONE,        NULL},                      // LF
  {CMD_NONE,        NULL},                      // VT
  {CMD_NONE,        NULL},                      // FF
  {CMD_NONE,        NULL},                      // CR
  {CMD_NONE,        NULL},                      // SO
  {CMD_NONE,        NULL},                      // SI
  {CMD_NONE,        NULL},                      // DLE
  {CMD_NONE,        NULL},                      // DC1/XON
  {CMD_NONE,        NULL},                      // DC2
  {CMD_NONE,        NULL},                      // DC3/XOFF
  {CMD_NONE,        NULL},                      // DC4
  {CMD_NONE,        NULL},                      // NAK
  {CMD_NONE,        NULL},                      // SYN
  {CMD_NONE,        NULL},                      // ETB
  {CMD_NONE,        NULL},                      // CAN
  {CMD_NONE,        NULL},                      // EM
  {CMD_NONE,        NULL},                      // SUB
  {CMD_NONE,        NULL},                      // ESC
  {CMD_NONE,        NULL},                      // FS
  {CMD_NONE,        NULL},                      // GS
  {CMD_NONE,        NULL},                      // RS
  {CMD_NONE,        NULL},                      // US
  {CMD_PRINT,       &WW_PRINT_SPACE},           //
  {CMD_PRINT,       &IBM_PRINT_FLAG_RMARK},     // !
  {CMD_PRINT,       &IBM_PRINT_FLAG_GMARK},     // "
  {CMD_PRINT,       &IBM_PRINT_RELEASESTART},   // #
  {CMD_PRINT,       &WW_PRINT_DOLLAR},          // $
  {CMD_IBM_PAUSE,   NULL},                      // %
  {CMD_IBM_RESUME,  NULL},                      // &
  {CMD_PRINT,       &WW_PRINT_TSet},            // '
  {CMD_PRINT,       &WW_PRINT_LPAREN},          // (
  {CMD_PRINT,       &WW_PRINT_RPAREN},          // )
  {CMD_PRINT,       &WW_PRINT_ASTERISK},        // *
  {CMD_PRINT,       &WW_PRINT_PLUS},            // +
  {CMD_PRINT,       &WW_PRINT_COMMA},           // , 
  {CMD_PRINT,       &WW_PRINT_HYPHEN},          // -
  {CMD_PRINT,       &WW_PRINT_PERIOD},          // .
  {CMD_PRINT,       &WW_PRINT_SLASH},           // /
  {CMD_PRINT,       &IBM_PRINT_SLASH_0},        // 0
  {CMD_PRINT,       &WW_PRINT_1},               // 1
  {CMD_PRINT,       &WW_PRINT_2},               // 2
  {CMD_PRINT,       &WW_PRINT_3},               // 3
  {CMD_PRINT,       &WW_PRINT_4},               // 4
  {CMD_PRINT,       &WW_PRINT_5},               // 5
  {CMD_PRINT,       &WW_PRINT_6},               // 6
  {CMD_PRINT,       &WW_PRINT_7},               // 7
  {CMD_PRINT,       &WW_PRINT_8},               // 8
  {CMD_PRINT,       &WW_PRINT_9},               // 9
  {CMD_PRINT,       &WW_PRINT_Tab},             // :
  {CMD_PRINT,       &WW_PRINT_CRtn},            // ;
  {CMD_PRINT,       &WW_PRINT_LARROW},          // <
  {CMD_PRINT,       &WW_PRINT_EQUAL},           // =
  {CMD_PRINT,       &WW_PRINT_RARROW},          // >
  {CMD_PRINT,       &IBM_PRINT_INVALID},        // ?
  {CMD_PRINT,       &WW_PRINT_AT},              // @
  {CMD_PRINT,       &WW_PRINT_A},               // A
  {CMD_PRINT,       &WW_PRINT_B},               // B
  {CMD_PRINT,       &WW_PRINT_C},               // C
  {CMD_PRINT,       &WW_PRINT_D},               // D
  {CMD_PRINT,       &WW_PRINT_E},               // E
  {CMD_PRINT,       &WW_PRINT_F},               // F
  {CMD_PRINT,       &WW_PRINT_G},               // G
  {CMD_PRINT,       &WW_PRINT_H},               // H
  {CMD_PRINT,       &WW_PRINT_I},               // I
  {CMD_PRINT,       &WW_PRINT_J},               // J
  {CMD_PRINT,       &WW_PRINT_K},               // K
  {CMD_PRINT,       &WW_PRINT_L},               // L
  {CMD_PRINT,       &WW_PRINT_M},               // M
  {CMD_PRINT,       &WW_PRINT_N},               // N
  {CMD_PRINT,       &WW_PRINT_O},               // O
  {CMD_PRINT,       &WW_PRINT_P},               // P
  {CMD_PRINT,       &WW_PRINT_Q},               // Q
  {CMD_PRINT,       &WW_PRINT_R},               // R
  {CMD_PRINT,       &WW_PRINT_S},               // S
  {CMD_PRINT,       &WW_PRINT_T},               // T
  {CMD_PRINT,       &WW_PRINT_U},               // U
  {CMD_PRINT,       &WW_PRINT_V},               // V
  {CMD_PRINT,       &WW_PRINT_W},               // W
  {CMD_PRINT,       &WW_PRINT_X},               // X
  {CMD_PRINT,       &WW_PRINT_Y},               // Y
  {CMD_PRINT,       &WW_PRINT_Z},               // Z
  {CMD_PRINT,       &WW_PRINT_LMar},            // [
  {CMD_PRINT,       &WW_PRINT_ClrAll},          // <backslash>
  {CMD_PRINT,       &WW_PRINT_RMar},            // ]
  {CMD_PRINT,       &WW_PRINT_UARROW},          // ^
  {CMD_PRINT,       &WW_PRINT_Backspace},       // _
  {CMD_PRINT,       &WW_PRINT_TClr},            // `
  {CMD_IBM_MODE_0,  NULL},                      // a
  {CMD_IBM_MODE_1,  NULL},                      // b
  {CMD_IBM_MODE_2,  NULL},                      // c
  {CMD_IBM_MODE_3,  NULL},                      // d
  {CMD_NONE,        NULL},                      // e
  {CMD_NONE,        NULL},                      // f
  {CMD_IBM_PING,    NULL},                      // g
  {CMD_IBM_ACK,     NULL},                      // h
  {CMD_PRINT,       &IBM_PRINT_FLAG_SLASH_0},   // i
  {CMD_PRINT,       &IBM_PRINT_FLAG_1},         // j
  {CMD_PRINT,       &IBM_PRINT_FLAG_2},         // k
  {CMD_PRINT,       &IBM_PRINT_FLAG_3},         // l
  {CMD_PRINT,       &IBM_PRINT_FLAG_4},         // m
  {CMD_PRINT,       &IBM_PRINT_FLAG_5},         // n
  {CMD_PRINT,       &IBM_PRINT_FLAG_6},         // o
  {CMD_PRINT,       &IBM_PRINT_FLAG_7},         // p
  {CMD_PRINT,       &IBM_PRINT_FLAG_8},         // q
  {CMD_PRINT,       &IBM_PRINT_FLAG_9},         // r
  {CMD_IBM_SLASH,   NULL},                      // s
  {CMD_NONE,        NULL},                      // t
  {CMD_IBM_UNSLASH, NULL},                      // u
  {CMD_PRINT,       &WW_PRINT_DARROW},          // v
  {CMD_NONE,        NULL},                      // w
  {CMD_NONE,        NULL},                      // x
  {CMD_NONE,        NULL},                      // y
  {CMD_IBM_RESET,   NULL},                      // z
  {CMD_PRINT,       &WW_PRINT_MarRel},          // {
  {CMD_PRINT,       &IBM_PRINT_RMARK},          // |
  {CMD_PRINT,       &IBM_PRINT_GMARK},          // }
  {CMD_PRINT,       &IBM_PRINT_FLAG_NUMBLANK},  // ~
  {CMD_NONE,        NULL}                       // DEL
};


//**********************************************************************************************************************
//
//  IBM 1620 Jr. key action tables.
//
//**********************************************************************************************************************

// Mode 0 (locked) key action table.
const struct key_action IBM_ACTIONS_MODE0[3 * NUM_WW_KEYS] = { 
  // Unshifted.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_SEND | ACTION_PRINT,                      '>',   &WW_PRINT_RARROW},         // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                      'v',   &WW_PRINT_DARROW},         // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                     // Z
  {ACTION_NONE,                                     0,     NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <release start>
  {ACTION_NONE,                                     0,     NULL},                     // A
  {ACTION_NONE,                                     0,     NULL},                     // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},      // <load paper>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_NONE,                                     0,     NULL},                     // X
  {ACTION_NONE,                                     0,     NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // S
  {ACTION_NONE,                                     0,     NULL},                     // C
  {ACTION_NONE,                                     0,     NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // D
  {ACTION_NONE,                                     0,     NULL},                     // B
  {ACTION_NONE,                                     0,     NULL},                     // V
  {ACTION_NONE,                                     0,     NULL},                     // T
  {ACTION_NONE,                                     0,     NULL},                     // R
  {ACTION_NONE,                                     0,     NULL},                     // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_NONE,                                     0,     NULL},                     // F
  {ACTION_NONE,                                     0,     NULL},                     // G
  {ACTION_NONE,                                     0,     NULL},                     // N
  {ACTION_NONE,                                     0,     NULL},                     // M, 7
  {ACTION_NONE,                                     0,     NULL},                     // Y
  {ACTION_NONE,                                     0,     NULL},                     // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_NONE,                                     0,     NULL},                     // J, 4
  {ACTION_NONE,                                     0,     NULL},                     // H
  {ACTION_NONE,                                     0,     NULL},                     // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_NONE,                                     0,     NULL},                     // <group mark>
  {ACTION_NONE,                                     0,     NULL},                     // K, 5
  {ACTION_NONE,                                     0,     NULL},                     // ., 9
  {ACTION_NONE,                                     0,     NULL},                     // O, 3
  {ACTION_NONE,                                     0,     NULL},                     // 0
  {ACTION_NONE,                                     0,     NULL},                     // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_NONE,                                     0,     NULL},                     // P
  {ACTION_NONE,                                     0,     NULL},                     // *, .
  {ACTION_NONE,                                     0,     NULL},                     // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_SEND | ACTION_PRINT,                      '<',   &WW_PRINT_LARROW},         // <left arrow>
  {ACTION_SEND | ACTION_PRINT,                      '^',   &WW_PRINT_UARROW},         // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      '_',   &WW_PRINT_Backspace},      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      ';',   &WW_PRINT_CRtn},           // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                     // <right margin>
  {ACTION_SEND | ACTION_PRINT,                      ':',   &WW_PRINT_Tab},            // <tab>
  {ACTION_NONE,                                     0,     NULL},                     // <margin release>
  {ACTION_NONE,                                     0,     NULL},                     // <tab set>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                     // Z
  {ACTION_NONE,                                     0,     NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <release start>
  {ACTION_NONE,                                     0,     NULL},                     // A
  {ACTION_NONE,                                     0,     NULL},                     // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},      // <load paper>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_NONE,                                     0,     NULL},                     // X
  {ACTION_NONE,                                     0,     NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // S
  {ACTION_NONE,                                     0,     NULL},                     // C
  {ACTION_NONE,                                     0,     NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // D
  {ACTION_NONE,                                     0,     NULL},                     // B
  {ACTION_NONE,                                     0,     NULL},                     // V
  {ACTION_NONE,                                     0,     NULL},                     // T
  {ACTION_NONE,                                     0,     NULL},                     // R
  {ACTION_NONE,                                     0,     NULL},                     // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_NONE,                                     0,     NULL},                     // F
  {ACTION_NONE,                                     0,     NULL},                     // G
  {ACTION_NONE,                                     0,     NULL},                     // N
  {ACTION_NONE,                                     0,     NULL},                     // M, 7
  {ACTION_NONE,                                     0,     NULL},                     // Y
  {ACTION_NONE,                                     0,     NULL},                     // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_NONE,                                     0,     NULL},                     // J, 4
  {ACTION_NONE,                                     0,     NULL},                     // H
  {ACTION_NONE,                                     0,     NULL},                     // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_NONE,                                     0,     NULL},                     // <group mark>
  {ACTION_NONE,                                     0,     NULL},                     // K, 5
  {ACTION_NONE,                                     0,     NULL},                     // ., 9
  {ACTION_NONE,                                     0,     NULL},                     // O, 3
  {ACTION_NONE,                                     0,     NULL},                     // 0
  {ACTION_NONE,                                     0,     NULL},                     // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_NONE,                                     0,     NULL},                     // P
  {ACTION_NONE,                                     0,     NULL},                     // *, .
  {ACTION_NONE,                                     0,     NULL},                     // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_NONE,                                     0,     NULL},                     // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                     // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                     // <right margin>
  {ACTION_NONE,                                     0,     NULL},                     // <tab>
  {ACTION_NONE,                                     0,     NULL},                     // <margin release>
  {ACTION_NONE,                                     0,     NULL},                     // <tab set>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000

  // Code.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_IBM_SETUP,                                0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                     // Z
  {ACTION_NONE,                                     0,     NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <release start>
  {ACTION_NONE,                                     0,     NULL},                     // A
  {ACTION_NONE,                                     0,     NULL},                     // <space>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      '[',   &WW_PRINT_LMar},           // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      '`',   &WW_PRINT_TClr},           // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_NONE,                                     0,     NULL},                     // X
  {ACTION_NONE,                                     0,     NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // S
  {ACTION_NONE,                                     0,     NULL},                     // C
  {ACTION_NONE,                                     0,     NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // D
  {ACTION_NONE,                                     0,     NULL},                     // B
  {ACTION_NONE,                                     0,     NULL},                     // V
  {ACTION_NONE,                                     0,     NULL},                     // T
  {ACTION_NONE,                                     0,     NULL},                     // R
  {ACTION_NONE,                                     0,     NULL},                     // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_NONE,                                     0,     NULL},                     // F
  {ACTION_NONE,                                     0,     NULL},                     // G
  {ACTION_NONE,                                     0,     NULL},                     // N
  {ACTION_NONE,                                     0,     NULL},                     // M, 7
  {ACTION_NONE,                                     0,     NULL},                     // Y
  {ACTION_NONE,                                     0,     NULL},                     // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_NONE,                                     0,     NULL},                     // J, 4
  {ACTION_NONE,                                     0,     NULL},                     // H
  {ACTION_NONE,                                     0,     NULL},                     // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_NONE,                                     0,     NULL},                     // <group mark>
  {ACTION_NONE,                                     0,     NULL},                     // K, 5
  {ACTION_NONE,                                     0,     NULL},                     // ., 9
  {ACTION_NONE,                                     0,     NULL},                     // O, 3
  {ACTION_NONE,                                     0,     NULL},                     // 0
  {ACTION_NONE,                                     0,     NULL},                     // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_NONE,                                     0,     NULL},                     // P
  {ACTION_NONE,                                     0,     NULL},                     // *, .
  {ACTION_NONE,                                     0,     NULL},                     // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_NONE,                                     0,     NULL},                     // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                     // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_SEND | ACTION_PRINT,                      ']',   &WW_PRINT_RMar},           // <right margin>
  {ACTION_NONE,                                     0,     NULL},                     // <tab>
  {ACTION_SEND | ACTION_PRINT,                      '{',   &WW_PRINT_MarRel},         // <margin release>
  {ACTION_SEND | ACTION_PRINT,                      '\'',  &WW_PRINT_TSet},           // <tab set>
  {ACTION_NONE,                                     0,     NULL}                      // *** not available on WW1000
};

// Mode 1 (numeric input) key action table.
const struct key_action IBM_ACTIONS_MODE1[3 * NUM_WW_KEYS] = {
  // Unshifted.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_SEND | ACTION_PRINT,                      '>',   &WW_PRINT_RARROW},         // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                      'v',   &WW_PRINT_DARROW},         // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                     // Z
  {ACTION_NONE,                                     0,     NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                      '#',   &IBM_PRINT_RELEASESTART},  // <release start>
  {ACTION_NONE,                                     0,     NULL},                     // A
  {ACTION_NONE,                                     0,     NULL},                     // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},      // <load paper>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_NONE,                                     0,     NULL},                     // X
  {ACTION_NONE,                                     0,     NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // S
  {ACTION_NONE,                                     0,     NULL},                     // C
  {ACTION_NONE,                                     0,     NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // D
  {ACTION_NONE,                                     0,     NULL},                     // B
  {ACTION_NONE,                                     0,     NULL},                     // V
  {ACTION_NONE,                                     0,     NULL},                     // T
  {ACTION_NONE,                                     0,     NULL},                     // R
  {ACTION_SEND | ACTION_PRINT,                      '@',   &WW_PRINT_AT},             // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_NONE,                                     0,     NULL},                     // F
  {ACTION_NONE,                                     0,     NULL},                     // G
  {ACTION_NONE,                                     0,     NULL},                     // N
  {ACTION_SEND | ACTION_PRINT,                      '7',   &WW_PRINT_7},              // M, 7
  {ACTION_NONE,                                     0,     NULL},                     // Y
  {ACTION_SEND | ACTION_PRINT,                      '1',   &WW_PRINT_1},              // U, 1
  {ACTION_PRINT | ACTION_IBM_MODE_1F,               0,     &IBM_PRINT_FLAG},          // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_SEND | ACTION_PRINT,                      '4',   &WW_PRINT_4},              // J, 4
  {ACTION_NONE,                                     0,     NULL},                     // H
  {ACTION_SEND | ACTION_PRINT,                      '8',   &WW_PRINT_8},              // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                      '2',   &WW_PRINT_2},              // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_SEND | ACTION_PRINT,                      '}',   &IBM_PRINT_GMARK},         // <group mark>
  {ACTION_SEND | ACTION_PRINT,                      '5',   &WW_PRINT_5},              // K, 5
  {ACTION_SEND | ACTION_PRINT,                      '9',   &WW_PRINT_9},              // ., 9
  {ACTION_SEND | ACTION_PRINT,                      '3',   &WW_PRINT_3},              // O, 3
  {ACTION_SEND | ACTION_PRINT,                      '0',   &IBM_PRINT_SLASH_0},       // 0
  {ACTION_SEND | ACTION_PRINT,                      '6',   &WW_PRINT_6},              // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_NONE,                                     0,     NULL},                     // P
  {ACTION_NONE,                                     0,     NULL},                     // *, .
  {ACTION_SEND | ACTION_PRINT,                      '|',   &IBM_PRINT_RMARK},         // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_SEND | ACTION_PRINT,                      '<',   &WW_PRINT_LARROW},         // <left arrow>
  {ACTION_SEND | ACTION_PRINT,                      '^',   &WW_PRINT_UARROW},         // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      '_',   &WW_PRINT_Backspace},      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      ';',   &WW_PRINT_CRtn},           // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                     // <right margin>
  {ACTION_SEND | ACTION_PRINT,                      ':',   &WW_PRINT_Tab},            // <tab>
  {ACTION_NONE,                                     0,     NULL},                     // <margin release>
  {ACTION_NONE,                                     0,     NULL},                     // <tab set>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                     // Z
  {ACTION_NONE,                                     0,     NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <release start>
  {ACTION_NONE,                                     0,     NULL},                     // A
  {ACTION_NONE,                                     0,     NULL},                     // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},      // <load paper>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_NONE,                                     0,     NULL},                     // X
  {ACTION_NONE,                                     0,     NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // S
  {ACTION_NONE,                                     0,     NULL},                     // C
  {ACTION_NONE,                                     0,     NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // D
  {ACTION_NONE,                                     0,     NULL},                     // B
  {ACTION_NONE,                                     0,     NULL},                     // V
  {ACTION_NONE,                                     0,     NULL},                     // T
  {ACTION_NONE,                                     0,     NULL},                     // R
  {ACTION_NONE,                                     0,     NULL},                     // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_NONE,                                     0,     NULL},                     // F
  {ACTION_NONE,                                     0,     NULL},                     // G
  {ACTION_NONE,                                     0,     NULL},                     // N
  {ACTION_NONE,                                     0,     NULL},                     // M, 7
  {ACTION_NONE,                                     0,     NULL},                     // Y
  {ACTION_NONE,                                     0,     NULL},                     // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_NONE,                                     0,     NULL},                     // J, 4
  {ACTION_NONE,                                     0,     NULL},                     // H
  {ACTION_NONE,                                     0,     NULL},                     // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_NONE,                                     0,     NULL},                     // <group mark>
  {ACTION_NONE,                                     0,     NULL},                     // K, 5
  {ACTION_NONE,                                     0,     NULL},                     // ., 9
  {ACTION_NONE,                                     0,     NULL},                     // O, 3
  {ACTION_NONE,                                     0,     NULL},                     // 0
  {ACTION_NONE,                                     0,     NULL},                     // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_NONE,                                     0,     NULL},                     // P
  {ACTION_NONE,                                     0,     NULL},                     // *, .
  {ACTION_NONE,                                     0,     NULL},                     // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_NONE,                                     0,     NULL},                     // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                     // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                     // <right margin>
  {ACTION_NONE,                                     0,     NULL},                     // <tab>
  {ACTION_NONE,                                     0,     NULL},                     // <margin release>
  {ACTION_NONE,                                     0,     NULL},                     // <tab set>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000

  // Code.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                     // Z
  {ACTION_NONE,                                     0,     NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <release start>
  {ACTION_NONE,                                     0,     NULL},                     // A
  {ACTION_NONE,                                     0,     NULL},                     // <space>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      '[',   &WW_PRINT_LMar},           // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      '`',   &WW_PRINT_TClr},           // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_NONE,                                     0,     NULL},                     // X
  {ACTION_NONE,                                     0,     NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // S
  {ACTION_NONE,                                     0,     NULL},                     // C
  {ACTION_NONE,                                     0,     NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // D
  {ACTION_NONE,                                     0,     NULL},                     // B
  {ACTION_NONE,                                     0,     NULL},                     // V
  {ACTION_NONE,                                     0,     NULL},                     // T
  {ACTION_NONE,                                     0,     NULL},                     // R
  {ACTION_NONE,                                     0,     NULL},                     // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_NONE,                                     0,     NULL},                     // F
  {ACTION_NONE,                                     0,     NULL},                     // G
  {ACTION_NONE,                                     0,     NULL},                     // N
  {ACTION_NONE,                                     0,     NULL},                     // M, 7
  {ACTION_NONE,                                     0,     NULL},                     // Y
  {ACTION_NONE,                                     0,     NULL},                     // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_NONE,                                     0,     NULL},                     // J, 4
  {ACTION_NONE,                                     0,     NULL},                     // H
  {ACTION_NONE,                                     0,     NULL},                     // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_NONE,                                     0,     NULL},                     // <group mark>
  {ACTION_NONE,                                     0,     NULL},                     // K, 5
  {ACTION_NONE,                                     0,     NULL},                     // ., 9
  {ACTION_NONE,                                     0,     NULL},                     // O, 3
  {ACTION_NONE,                                     0,     NULL},                     // 0
  {ACTION_NONE,                                     0,     NULL},                     // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_NONE,                                     0,     NULL},                     // P
  {ACTION_NONE,                                     0,     NULL},                     // *, .
  {ACTION_NONE,                                     0,     NULL},                     // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_NONE,                                     0,     NULL},                     // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                     // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_SEND | ACTION_PRINT,                      ']',   &WW_PRINT_RMar},           // <right margin>
  {ACTION_NONE,                                     0,     NULL},                     // <tab>
  {ACTION_SEND | ACTION_PRINT,                      '{',   &WW_PRINT_MarRel},         // <margin release>
  {ACTION_SEND | ACTION_PRINT,                      '\'',  &WW_PRINT_TSet},           // <tab set>
  {ACTION_NONE,                                     0,     NULL}                      // *** not available on WW1000
};

// Mode 1 (flagged numeric input) key action table.
const struct key_action IBM_ACTIONS_MODE1_FLAG[3 * NUM_WW_KEYS] = {
  // Unshifted.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                     // Z
  {ACTION_NONE,                                     0,     NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1,  '#',   &IBM_PRINT_RELEASESTART},  // <release start>
  {ACTION_NONE,                                     0,     NULL},                     // A
  {ACTION_NONE,                                     0,     NULL},                     // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},      // <load paper>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_NONE,                                     0,     NULL},                     // X
  {ACTION_NONE,                                     0,     NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // S
  {ACTION_NONE,                                     0,     NULL},                     // C
  {ACTION_NONE,                                     0,     NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // D
  {ACTION_NONE,                                     0,     NULL},                     // B
  {ACTION_NONE,                                     0,     NULL},                     // V
  {ACTION_NONE,                                     0,     NULL},                     // T
  {ACTION_NONE,                                     0,     NULL},                     // R
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1,  '~',   &WW_PRINT_AT},             // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_NONE,                                     0,     NULL},                     // F
  {ACTION_NONE,                                     0,     NULL},                     // G
  {ACTION_NONE,                                     0,     NULL},                     // N
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1,  'p',   &WW_PRINT_7},              // M, 7
  {ACTION_NONE,                                     0,     NULL},                     // Y
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1,  'j',   &WW_PRINT_1},              // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1,  'm',   &WW_PRINT_4},              // J, 4
  {ACTION_NONE,                                     0,     NULL},                     // H
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1,  'q',   &WW_PRINT_8},              // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1,  'k',   &WW_PRINT_2},              // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1,  '"',   &IBM_PRINT_GMARK},         // <group mark>
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1,  'n',   &WW_PRINT_5},              // K, 5
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1,  'r',   &WW_PRINT_9},              // ., 9
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1,  'l',   &WW_PRINT_3},              // O, 3
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1,  'i',   &IBM_PRINT_SLASH_0},       // 0
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1,  'o',   &WW_PRINT_6},              // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_NONE,                                     0,     NULL},                     // P
  {ACTION_NONE,                                     0,     NULL},                     // *, .
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1,  '!',   &IBM_PRINT_RMARK},         // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_NONE,                                     0,     NULL},                     // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                     // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                     // <right margin>
  {ACTION_NONE,                                     0,     NULL},                     // <tab>
  {ACTION_NONE,                                     0,     NULL},                     // <margin release>
  {ACTION_NONE,                                     0,     NULL},                     // <tab set>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                     // Z
  {ACTION_NONE,                                     0,     NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <release start>
  {ACTION_NONE,                                     0,     NULL},                     // A
  {ACTION_NONE,                                     0,     NULL},                     // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},      // <load paper>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_NONE,                                     0,     NULL},                     // X
  {ACTION_NONE,                                     0,     NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // S
  {ACTION_NONE,                                     0,     NULL},                     // C
  {ACTION_NONE,                                     0,     NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // D
  {ACTION_NONE,                                     0,     NULL},                     // B
  {ACTION_NONE,                                     0,     NULL},                     // V
  {ACTION_NONE,                                     0,     NULL},                     // T
  {ACTION_NONE,                                     0,     NULL},                     // R
  {ACTION_NONE,                                     0,     NULL},                     // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_NONE,                                     0,     NULL},                     // F
  {ACTION_NONE,                                     0,     NULL},                     // G
  {ACTION_NONE,                                     0,     NULL},                     // N
  {ACTION_NONE,                                     0,     NULL},                     // M, 7
  {ACTION_NONE,                                     0,     NULL},                     // Y
  {ACTION_NONE,                                     0,     NULL},                     // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_NONE,                                     0,     NULL},                     // J, 4
  {ACTION_NONE,                                     0,     NULL},                     // H
  {ACTION_NONE,                                     0,     NULL},                     // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_NONE,                                     0,     NULL},                     // <group mark>
  {ACTION_NONE,                                     0,     NULL},                     // K, 5
  {ACTION_NONE,                                     0,     NULL},                     // ., 9
  {ACTION_NONE,                                     0,     NULL},                     // O, 3
  {ACTION_NONE,                                     0,     NULL},                     // 0
  {ACTION_NONE,                                     0,     NULL},                     // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_NONE,                                     0,     NULL},                     // P
  {ACTION_NONE,                                     0,     NULL},                     // *, .
  {ACTION_NONE,                                     0,     NULL},                     // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_NONE,                                     0,     NULL},                     // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                     // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                     // <right margin>
  {ACTION_NONE,                                     0,     NULL},                     // <tab>
  {ACTION_NONE,                                     0,     NULL},                     // <margin release>
  {ACTION_NONE,                                     0,     NULL},                     // <tab set>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000

  // Code.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                     // Z
  {ACTION_NONE,                                     0,     NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <release start>
  {ACTION_NONE,                                     0,     NULL},                     // A
  {ACTION_NONE,                                     0,     NULL},                     // <space>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_NONE,                                     0,     NULL},                     // X
  {ACTION_NONE,                                     0,     NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // S
  {ACTION_NONE,                                     0,     NULL},                     // C
  {ACTION_NONE,                                     0,     NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // D
  {ACTION_NONE,                                     0,     NULL},                     // B
  {ACTION_NONE,                                     0,     NULL},                     // V
  {ACTION_NONE,                                     0,     NULL},                     // T
  {ACTION_NONE,                                     0,     NULL},                     // R
  {ACTION_NONE,                                     0,     NULL},                     // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_NONE,                                     0,     NULL},                     // F
  {ACTION_NONE,                                     0,     NULL},                     // G
  {ACTION_NONE,                                     0,     NULL},                     // N
  {ACTION_NONE,                                     0,     NULL},                     // M, 7
  {ACTION_NONE,                                     0,     NULL},                     // Y
  {ACTION_NONE,                                     0,     NULL},                     // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_NONE,                                     0,     NULL},                     // J, 4
  {ACTION_NONE,                                     0,     NULL},                     // H
  {ACTION_NONE,                                     0,     NULL},                     // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_NONE,                                     0,     NULL},                     // <group mark>
  {ACTION_NONE,                                     0,     NULL},                     // K, 5
  {ACTION_NONE,                                     0,     NULL},                     // ., 9
  {ACTION_NONE,                                     0,     NULL},                     // O, 3
  {ACTION_NONE,                                     0,     NULL},                     // 0
  {ACTION_NONE,                                     0,     NULL},                     // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_NONE,                                     0,     NULL},                     // P
  {ACTION_NONE,                                     0,     NULL},                     // *, .
  {ACTION_NONE,                                     0,     NULL},                     // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_NONE,                                     0,     NULL},                     // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                     // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                     // <right margin>
  {ACTION_NONE,                                     0,     NULL},                     // <tab>
  {ACTION_NONE,                                     0,     NULL},                     // <margin release>
  {ACTION_NONE,                                     0,     NULL},                     // <tab set>
  {ACTION_NONE,                                     0,     NULL}                      // *** not available on WW1000
};

// Mode 2 (alphameric input) key action table.
const struct key_action IBM_ACTIONS_MODE2[3 * NUM_WW_KEYS] = {
  // Unshifted.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_SEND | ACTION_PRINT,                      '>',   &WW_PRINT_RARROW},         // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                      'v',   &WW_PRINT_DARROW},         // <down arrow>
  {ACTION_SEND | ACTION_PRINT,                      'Z',   &WW_PRINT_Z},              // Z
  {ACTION_SEND | ACTION_PRINT,                      'Q',   &WW_PRINT_Q},              // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                      '#',   &IBM_PRINT_RELEASESTART},  // <release start>
  {ACTION_SEND | ACTION_PRINT,                      'A',   &WW_PRINT_A},              // A
  {ACTION_SEND | ACTION_PRINT,                      ' ',   &WW_PRINT_SPACE},          // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},      // <load paper>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_SEND | ACTION_PRINT,                      'X',   &WW_PRINT_X},              // X
  {ACTION_SEND | ACTION_PRINT,                      'W',   &WW_PRINT_W},              // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                      'S',   &WW_PRINT_S},              // S
  {ACTION_SEND | ACTION_PRINT,                      'C',   &WW_PRINT_C},              // C
  {ACTION_SEND | ACTION_PRINT,                      'E',   &WW_PRINT_E},              // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                      'D',   &WW_PRINT_D},              // D
  {ACTION_SEND | ACTION_PRINT,                      'B',   &WW_PRINT_B},              // B
  {ACTION_SEND | ACTION_PRINT,                      'V',   &WW_PRINT_V},              // V
  {ACTION_SEND | ACTION_PRINT,                      'T',   &WW_PRINT_T},              // T
  {ACTION_SEND | ACTION_PRINT,                      'R',   &WW_PRINT_R},              // R
  {ACTION_SEND | ACTION_PRINT,                      '@',   &WW_PRINT_AT},             // @
  {ACTION_SEND | ACTION_PRINT,                      '(',   &WW_PRINT_LPAREN},         // (
  {ACTION_SEND | ACTION_PRINT,                      'F',   &WW_PRINT_F},              // F
  {ACTION_SEND | ACTION_PRINT,                      'G',   &WW_PRINT_G},              // G
  {ACTION_SEND | ACTION_PRINT,                      'N',   &WW_PRINT_N},              // N
  {ACTION_SEND | ACTION_PRINT,                      'M',   &WW_PRINT_M},              // M, 7
  {ACTION_SEND | ACTION_PRINT,                      'Y',   &WW_PRINT_Y},              // Y
  {ACTION_SEND | ACTION_PRINT,                      'U',   &WW_PRINT_U},              // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_SEND | ACTION_PRINT,                      ')',   &WW_PRINT_RPAREN},         // )
  {ACTION_SEND | ACTION_PRINT,                      'J',   &WW_PRINT_J},              // J, 4
  {ACTION_SEND | ACTION_PRINT,                      'H',   &WW_PRINT_H},              // H
  {ACTION_SEND | ACTION_PRINT,                      ',',   &WW_PRINT_COMMA},          // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                      'I',   &WW_PRINT_I},              // I, 2
  {ACTION_SEND | ACTION_PRINT,                      '=',   &WW_PRINT_EQUAL},          // =
  {ACTION_SEND | ACTION_PRINT,                      '}',   &IBM_PRINT_GMARK},         // <group mark>
  {ACTION_SEND | ACTION_PRINT,                      'K',   &WW_PRINT_K},              // K, 5
  {ACTION_SEND | ACTION_PRINT,                      '.',   &WW_PRINT_PERIOD},         // ., 9
  {ACTION_SEND | ACTION_PRINT,                      'O',   &WW_PRINT_O},              // O, 3
  {ACTION_NONE,                                     0,     NULL},                     // 0
  {ACTION_SEND | ACTION_PRINT,                      'L',   &WW_PRINT_L},              // L, 6
  {ACTION_SEND | ACTION_PRINT,                      '/',   &WW_PRINT_SLASH},          // /
  {ACTION_SEND | ACTION_PRINT,                      '-',   &WW_PRINT_HYPHEN},         // -
  {ACTION_SEND | ACTION_PRINT,                      'P',   &WW_PRINT_P},              // P
  {ACTION_SEND | ACTION_PRINT,                      '*',   &WW_PRINT_ASTERISK},       // *, .
  {ACTION_SEND | ACTION_PRINT,                      '|',   &IBM_PRINT_RMARK},         // <record mark>
  {ACTION_SEND | ACTION_PRINT,                      '+',   &WW_PRINT_PLUS},           // +
  {ACTION_SEND | ACTION_PRINT,                      '$',   &WW_PRINT_DOLLAR},         // $
  {ACTION_SEND | ACTION_PRINT,                      '<',   &WW_PRINT_LARROW},         // <left arrow>
  {ACTION_SEND | ACTION_PRINT,                      '^',   &WW_PRINT_UARROW},         // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      '_',   &WW_PRINT_Backspace},      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      ';',   &WW_PRINT_CRtn},           // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                     // <right margin>
  {ACTION_SEND | ACTION_PRINT,                      ':',   &WW_PRINT_Tab},            // <tab>
  {ACTION_NONE,                                     0,     NULL},                     // <margin release>
  {ACTION_NONE,                                     0,     NULL},                     // <tab set>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                     // Z
  {ACTION_NONE,                                     0,     NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <release start>
  {ACTION_NONE,                                     0,     NULL},                     // A
  {ACTION_NONE,                                     0,     NULL},                     // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},      // <load paper>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_NONE,                                     0,     NULL},                     // X
  {ACTION_NONE,                                     0,     NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // S
  {ACTION_NONE,                                     0,     NULL},                     // C
  {ACTION_NONE,                                     0,     NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // D
  {ACTION_NONE,                                     0,     NULL},                     // B
  {ACTION_NONE,                                     0,     NULL},                     // V
  {ACTION_NONE,                                     0,     NULL},                     // T
  {ACTION_NONE,                                     0,     NULL},                     // R
  {ACTION_NONE,                                     0,     NULL},                     // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_NONE,                                     0,     NULL},                     // F
  {ACTION_NONE,                                     0,     NULL},                     // G
  {ACTION_NONE,                                     0,     NULL},                     // N
  {ACTION_SEND | ACTION_PRINT,                      '7',   &WW_PRINT_7},              // M, 7
  {ACTION_NONE,                                     0,     NULL},                     // Y
  {ACTION_SEND | ACTION_PRINT,                      '1',   &WW_PRINT_1},              // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_SEND | ACTION_PRINT,                      '4',   &WW_PRINT_4},              // J, 4
  {ACTION_NONE,                                     0,     NULL},                     // H
  {ACTION_SEND | ACTION_PRINT,                      '8',   &WW_PRINT_8},              // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                      '2',   &WW_PRINT_2},              // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_NONE,                                     0,     NULL},                     // <group mark>
  {ACTION_SEND | ACTION_PRINT,                      '5',   &WW_PRINT_5},              // K, 5
  {ACTION_SEND | ACTION_PRINT,                      '9',   &WW_PRINT_9},              // ., 9
  {ACTION_SEND | ACTION_PRINT,                      '3',   &WW_PRINT_3},              // O, 3
  {ACTION_SEND | ACTION_PRINT,                      '0',   &IBM_PRINT_SLASH_0},       // 0
  {ACTION_SEND | ACTION_PRINT,                      '6',   &WW_PRINT_6},              // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_NONE,                                     0,     NULL},                     // P
  {ACTION_SEND | ACTION_PRINT,                      '.',   &WW_PRINT_PERIOD},         // *, .
  {ACTION_NONE,                                     0,     NULL},                     // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_NONE,                                     0,     NULL},                     // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                     // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                     // <right margin>
  {ACTION_NONE,                                     0,     NULL},                     // <tab>
  {ACTION_NONE,                                     0,     NULL},                     // <margin release>
  {ACTION_NONE,                                     0,     NULL},                     // <tab set>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000

  // Code.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                     // Z
  {ACTION_NONE,                                     0,     NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <release start>
  {ACTION_NONE,                                     0,     NULL},                     // A
  {ACTION_NONE,                                     0,     NULL},                     // <space>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      '[',   &WW_PRINT_LMar},           // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      '`',   &WW_PRINT_TClr},           // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_NONE,                                     0,     NULL},                     // X
  {ACTION_NONE,                                     0,     NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // S
  {ACTION_NONE,                                     0,     NULL},                     // C
  {ACTION_NONE,                                     0,     NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // D
  {ACTION_NONE,                                     0,     NULL},                     // B
  {ACTION_NONE,                                     0,     NULL},                     // V
  {ACTION_NONE,                                     0,     NULL},                     // T
  {ACTION_NONE,                                     0,     NULL},                     // R
  {ACTION_NONE,                                     0,     NULL},                     // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_NONE,                                     0,     NULL},                     // F
  {ACTION_NONE,                                     0,     NULL},                     // G
  {ACTION_NONE,                                     0,     NULL},                     // N
  {ACTION_NONE,                                     0,     NULL},                     // M, 7
  {ACTION_NONE,                                     0,     NULL},                     // Y
  {ACTION_NONE,                                     0,     NULL},                     // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_NONE,                                     0,     NULL},                     // J, 4
  {ACTION_NONE,                                     0,     NULL},                     // H
  {ACTION_NONE,                                     0,     NULL},                     // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_NONE,                                     0,     NULL},                     // <group mark>
  {ACTION_NONE,                                     0,     NULL},                     // K, 5
  {ACTION_NONE,                                     0,     NULL},                     // ., 9
  {ACTION_NONE,                                     0,     NULL},                     // O, 3
  {ACTION_NONE,                                     0,     NULL},                     // 0
  {ACTION_NONE,                                     0,     NULL},                     // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_NONE,                                     0,     NULL},                     // P
  {ACTION_NONE,                                     0,     NULL},                     // *, .
  {ACTION_NONE,                                     0,     NULL},                     // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_NONE,                                     0,     NULL},                     // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                     // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_SEND | ACTION_PRINT,                      ']',   &WW_PRINT_RMar},           // <right margin>
  {ACTION_NONE,                                     0,     NULL},                     // <tab>
  {ACTION_SEND | ACTION_PRINT,                      '{',   &WW_PRINT_MarRel},         // <margin release>
  {ACTION_SEND | ACTION_PRINT,                      '\'',  &WW_PRINT_TSet},           // <tab set>
  {ACTION_NONE,                                     0,     NULL}                      // *** not available on WW1000
};

// Mode 3 (output) key action table.
const struct key_action IBM_ACTIONS_MODE3[3 * NUM_WW_KEYS] = {
  // Unshifted.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                     // Z
  {ACTION_NONE,                                     0,     NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                      '#',   &IBM_PRINT_RELEASESTART},  // <release start>
  {ACTION_NONE,                                     0,     NULL},                     // A
  {ACTION_NONE,                                     0,     NULL},                     // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},      // <load paper>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_NONE,                                     0,     NULL},                     // X
  {ACTION_NONE,                                     0,     NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // S
  {ACTION_NONE,                                     0,     NULL},                     // C
  {ACTION_NONE,                                     0,     NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // D
  {ACTION_NONE,                                     0,     NULL},                     // B
  {ACTION_NONE,                                     0,     NULL},                     // V
  {ACTION_NONE,                                     0,     NULL},                     // T
  {ACTION_NONE,                                     0,     NULL},                     // R
  {ACTION_NONE,                                     0,     NULL},                     // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_NONE,                                     0,     NULL},                     // F
  {ACTION_NONE,                                     0,     NULL},                     // G
  {ACTION_NONE,                                     0,     NULL},                     // N
  {ACTION_NONE,                                     0,     NULL},                     // M, 7
  {ACTION_NONE,                                     0,     NULL},                     // Y
  {ACTION_NONE,                                     0,     NULL},                     // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_NONE,                                     0,     NULL},                     // J, 4
  {ACTION_NONE,                                     0,     NULL},                     // H
  {ACTION_NONE,                                     0,     NULL},                     // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_NONE,                                     0,     NULL},                     // <group mark>
  {ACTION_NONE,                                     0,     NULL},                     // K, 5
  {ACTION_NONE,                                     0,     NULL},                     // ., 9
  {ACTION_NONE,                                     0,     NULL},                     // O, 3
  {ACTION_NONE,                                     0,     NULL},                     // 0
  {ACTION_NONE,                                     0,     NULL},                     // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_NONE,                                     0,     NULL},                     // P
  {ACTION_NONE,                                     0,     NULL},                     // *, .
  {ACTION_NONE,                                     0,     NULL},                     // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_NONE,                                     0,     NULL},                     // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                     // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      ';',   &WW_PRINT_CRtn},           // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                     // <right margin>
  {ACTION_SEND | ACTION_PRINT,                      ':',   &WW_PRINT_Tab},            // <tab>
  {ACTION_NONE,                                     0,     NULL},                     // <margin release>
  {ACTION_NONE,                                     0,     NULL},                     // <tab set>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                     // Z
  {ACTION_NONE,                                     0,     NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <release start>
  {ACTION_NONE,                                     0,     NULL},                     // A
  {ACTION_NONE,                                     0,     NULL},                     // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},      // <load paper>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_NONE,                                     0,     NULL},                     // X
  {ACTION_NONE,                                     0,     NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // S
  {ACTION_NONE,                                     0,     NULL},                     // C
  {ACTION_NONE,                                     0,     NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // D
  {ACTION_NONE,                                     0,     NULL},                     // B
  {ACTION_NONE,                                     0,     NULL},                     // V
  {ACTION_NONE,                                     0,     NULL},                     // T
  {ACTION_NONE,                                     0,     NULL},                     // R
  {ACTION_NONE,                                     0,     NULL},                     // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_NONE,                                     0,     NULL},                     // F
  {ACTION_NONE,                                     0,     NULL},                     // G
  {ACTION_NONE,                                     0,     NULL},                     // N
  {ACTION_NONE,                                     0,     NULL},                     // M, 7
  {ACTION_NONE,                                     0,     NULL},                     // Y
  {ACTION_NONE,                                     0,     NULL},                     // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_NONE,                                     0,     NULL},                     // J, 4
  {ACTION_NONE,                                     0,     NULL},                     // H
  {ACTION_NONE,                                     0,     NULL},                     // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_NONE,                                     0,     NULL},                     // <group mark>
  {ACTION_NONE,                                     0,     NULL},                     // K, 5
  {ACTION_NONE,                                     0,     NULL},                     // ., 9
  {ACTION_NONE,                                     0,     NULL},                     // O, 3
  {ACTION_NONE,                                     0,     NULL},                     // 0
  {ACTION_NONE,                                     0,     NULL},                     // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_NONE,                                     0,     NULL},                     // P
  {ACTION_NONE,                                     0,     NULL},                     // *, .
  {ACTION_NONE,                                     0,     NULL},                     // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_NONE,                                     0,     NULL},                     // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                     // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                     // <right margin>
  {ACTION_NONE,                                     0,     NULL},                     // <tab>
  {ACTION_NONE,                                     0,     NULL},                     // <margin release>
  {ACTION_NONE,                                     0,     NULL},                     // <tab set>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000

  // Code.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                     // Z
  {ACTION_NONE,                                     0,     NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <release start>
  {ACTION_NONE,                                     0,     NULL},                     // A
  {ACTION_NONE,                                     0,     NULL},                     // <space>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_NONE,                                     0,     NULL},                     // X
  {ACTION_NONE,                                     0,     NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // S
  {ACTION_NONE,                                     0,     NULL},                     // C
  {ACTION_NONE,                                     0,     NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // D
  {ACTION_NONE,                                     0,     NULL},                     // B
  {ACTION_NONE,                                     0,     NULL},                     // V
  {ACTION_NONE,                                     0,     NULL},                     // T
  {ACTION_NONE,                                     0,     NULL},                     // R
  {ACTION_NONE,                                     0,     NULL},                     // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_NONE,                                     0,     NULL},                     // F
  {ACTION_NONE,                                     0,     NULL},                     // G
  {ACTION_NONE,                                     0,     NULL},                     // N
  {ACTION_NONE,                                     0,     NULL},                     // M, 7
  {ACTION_NONE,                                     0,     NULL},                     // Y
  {ACTION_NONE,                                     0,     NULL},                     // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_NONE,                                     0,     NULL},                     // J, 4
  {ACTION_NONE,                                     0,     NULL},                     // H
  {ACTION_NONE,                                     0,     NULL},                     // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_NONE,                                     0,     NULL},                     // <group mark>
  {ACTION_NONE,                                     0,     NULL},                     // K, 5
  {ACTION_NONE,                                     0,     NULL},                     // ., 9
  {ACTION_NONE,                                     0,     NULL},                     // O, 3
  {ACTION_NONE,                                     0,     NULL},                     // 0
  {ACTION_NONE,                                     0,     NULL},                     // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_NONE,                                     0,     NULL},                     // P
  {ACTION_NONE,                                     0,     NULL},                     // *, .
  {ACTION_NONE,                                     0,     NULL},                     // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_NONE,                                     0,     NULL},                     // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                     // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                     // <right margin>
  {ACTION_NONE,                                     0,     NULL},                     // <tab>
  {ACTION_NONE,                                     0,     NULL},                     // <margin release>
  {ACTION_NONE,                                     0,     NULL},                     // <tab set>
  {ACTION_NONE,                                     0,     NULL}                      // *** not available on WW1000
};

// Setup key action table.
const struct key_action IBM_ACTIONS_SETUP[3 * NUM_WW_KEYS] = {
  // Unshifted.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <down arrow>
  {ACTION_COMMAND,                                  'Z',   NULL},                     // Z
  {ACTION_COMMAND,                                  'Q',   NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <release start>
  {ACTION_COMMAND,                                  'A',   NULL},                     // A
  {ACTION_COMMAND,                                  ' ',   NULL},                     // <space>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_COMMAND,                                  'X',   NULL},                     // X
  {ACTION_COMMAND,                                  'W',   NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_COMMAND,                                  'S',   NULL},                     // S
  {ACTION_COMMAND,                                  'C',   NULL},                     // C
  {ACTION_COMMAND,                                  'E',   NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_COMMAND,                                  'D',   NULL},                     // D
  {ACTION_COMMAND,                                  'B',   NULL},                     // B
  {ACTION_COMMAND,                                  'V',   NULL},                     // V
  {ACTION_COMMAND,                                  'T',   NULL},                     // T
  {ACTION_COMMAND,                                  'R',   NULL},                     // R
  {ACTION_NONE,                                     0,     NULL},                     // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_COMMAND,                                  'F',   NULL},                     // F
  {ACTION_COMMAND,                                  'G',   NULL},                     // G
  {ACTION_COMMAND,                                  'N',   NULL},                     // N
  {ACTION_COMMAND,                                  'M',   NULL},                     // M, 7
  {ACTION_COMMAND,                                  'Y',   NULL},                     // Y
  {ACTION_COMMAND,                                  'U',   NULL},                     // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_COMMAND,                                  'J',   NULL},                     // J, 4
  {ACTION_COMMAND,                                  'H',   NULL},                     // H
  {ACTION_NONE,                                     0,     NULL},                     // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_COMMAND,                                  'I',   NULL},                     // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_NONE,                                     0,     NULL},                     // <group mark>
  {ACTION_COMMAND,                                  'K',   NULL},                     // K, 5
  {ACTION_NONE,                                     0,     NULL},                     // ., 9
  {ACTION_COMMAND,                                  'O',   NULL},                     // O, 3
  {ACTION_NONE,                                     0,     NULL},                     // 0
  {ACTION_COMMAND,                                  'L',   NULL},                     // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_COMMAND,                                  'P',   NULL},                     // P
  {ACTION_NONE,                                     0,     NULL},                     // *, .
  {ACTION_NONE,                                     0,     NULL},                     // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_NONE,                                     0,     NULL},                     // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                     // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_COMMAND,                                  0x0d,  NULL},                     // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                     // <right margin>
  {ACTION_NONE,                                     0,     NULL},                     // <tab>
  {ACTION_NONE,                                     0,     NULL},                     // <margin release>
  {ACTION_NONE,                                     0,     NULL},                     // <tab set>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                     // Z
  {ACTION_NONE,                                     0,     NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <release start>
  {ACTION_NONE,                                     0,     NULL},                     // A
  {ACTION_NONE,                                     0,     NULL},                     // <space>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_NONE,                                     0,     NULL},                     // X
  {ACTION_NONE,                                     0,     NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // S
  {ACTION_NONE,                                     0,     NULL},                     // C
  {ACTION_NONE,                                     0,     NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // D
  {ACTION_NONE,                                     0,     NULL},                     // B
  {ACTION_NONE,                                     0,     NULL},                     // V
  {ACTION_NONE,                                     0,     NULL},                     // T
  {ACTION_NONE,                                     0,     NULL},                     // R
  {ACTION_NONE,                                     0,     NULL},                     // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_NONE,                                     0,     NULL},                     // F
  {ACTION_NONE,                                     0,     NULL},                     // G
  {ACTION_NONE,                                     0,     NULL},                     // N
  {ACTION_COMMAND,                                  '7',   NULL},                     // M, 7
  {ACTION_NONE,                                     0,     NULL},                     // Y
  {ACTION_COMMAND,                                  '1',   NULL},                     // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_COMMAND,                                  '4',   NULL},                     // J, 4
  {ACTION_NONE,                                     0,     NULL},                     // H
  {ACTION_COMMAND,                                  '8',   NULL},                     // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_COMMAND,                                  '2',   NULL},                     // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_NONE,                                     0,     NULL},                     // <group mark>
  {ACTION_COMMAND,                                  '5',   NULL},                     // K, 5
  {ACTION_COMMAND,                                  '9',   NULL},                     // ., 9
  {ACTION_COMMAND,                                  '3',   NULL},                     // O, 3
  {ACTION_COMMAND,                                  '0',   NULL},                     // 0
  {ACTION_COMMAND,                                  '6',   NULL},                     // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_NONE,                                     0,     NULL},                     // P
  {ACTION_NONE,                                     0,     NULL},                     // *, .
  {ACTION_NONE,                                     0,     NULL},                     // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_NONE,                                     0,     NULL},                     // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                     // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                     // <right margin>
  {ACTION_NONE,                                     0,     NULL},                     // <tab>
  {ACTION_NONE,                                     0,     NULL},                     // <margin release>
  {ACTION_NONE,                                     0,     NULL},                     // <tab set>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000

  // Code.
  {ACTION_NONE,                                     0,     NULL},                     // <left shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right shift>
  {ACTION_NONE,                                     0,     NULL},                     // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <setup>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                     // Z
  {ACTION_NONE,                                     0,     NULL},                     // Q
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <release start>
  {ACTION_NONE,                                     0,     NULL},                     // A
  {ACTION_NONE,                                     0,     NULL},                     // <space>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <left margin>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                     // <code>
  {ACTION_NONE,                                     0,     NULL},                     // X
  {ACTION_NONE,                                     0,     NULL},                     // W
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // S
  {ACTION_NONE,                                     0,     NULL},                     // C
  {ACTION_NONE,                                     0,     NULL},                     // E
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // D
  {ACTION_NONE,                                     0,     NULL},                     // B
  {ACTION_NONE,                                     0,     NULL},                     // V
  {ACTION_NONE,                                     0,     NULL},                     // T
  {ACTION_NONE,                                     0,     NULL},                     // R
  {ACTION_NONE,                                     0,     NULL},                     // @
  {ACTION_NONE,                                     0,     NULL},                     // (
  {ACTION_NONE,                                     0,     NULL},                     // F
  {ACTION_NONE,                                     0,     NULL},                     // G
  {ACTION_NONE,                                     0,     NULL},                     // N
  {ACTION_NONE,                                     0,     NULL},                     // M, 7
  {ACTION_NONE,                                     0,     NULL},                     // Y
  {ACTION_NONE,                                     0,     NULL},                     // U, 1
  {ACTION_NONE,                                     0,     NULL},                     // <flag>
  {ACTION_NONE,                                     0,     NULL},                     // )
  {ACTION_NONE,                                     0,     NULL},                     // J, 4
  {ACTION_NONE,                                     0,     NULL},                     // H
  {ACTION_NONE,                                     0,     NULL},                     // ,, 8
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // I, 2
  {ACTION_NONE,                                     0,     NULL},                     // =
  {ACTION_NONE,                                     0,     NULL},                     // <group mark>
  {ACTION_NONE,                                     0,     NULL},                     // K, 5
  {ACTION_NONE,                                     0,     NULL},                     // ., 9
  {ACTION_NONE,                                     0,     NULL},                     // O, 3
  {ACTION_NONE,                                     0,     NULL},                     // 0
  {ACTION_NONE,                                     0,     NULL},                     // L, 6
  {ACTION_NONE,                                     0,     NULL},                     // /
  {ACTION_NONE,                                     0,     NULL},                     // -
  {ACTION_NONE,                                     0,     NULL},                     // P
  {ACTION_NONE,                                     0,     NULL},                     // *, .
  {ACTION_NONE,                                     0,     NULL},                     // <record mark>
  {ACTION_NONE,                                     0,     NULL},                     // +
  {ACTION_NONE,                                     0,     NULL},                     // $
  {ACTION_NONE,                                     0,     NULL},                     // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                     // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <backspace>
  {ACTION_NONE,                                     0,     NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                     // <return>
  {ACTION_NONE,                                     0,     NULL},                     //
  {ACTION_NONE,                                     0,     NULL},                     // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                     // <right margin>
  {ACTION_NONE,                                     0,     NULL},                     // <tab>
  {ACTION_NONE,                                     0,     NULL},                     // <margin release>
  {ACTION_NONE,                                     0,     NULL},                     // <tab set>
  {ACTION_NONE,                                     0,     NULL}                      // *** not available on WW1000
};


//**********************************************************************************************************************
//
//  IBM 1620 Jr. Console Typewriter data declarations.
//
//**********************************************************************************************************************

// Current typewriter state.
volatile boolean artn_IBM = FALSE;      // Current auto return state.
volatile boolean bold_IBM = FALSE;      // Current bold state.
volatile boolean lock_IBM = FALSE;      // Current lock state.
volatile boolean send_ack_IBM = FALSE;  // Current send ack state.


//**********************************************************************************************************************
//
//  IBM 1620 Jr. Console Typewriter setup function.
//
//**********************************************************************************************************************

void Setup_IBM () {

  // Initialize variables.
  key_actions = &IBM_ACTIONS_MODE0;
  serial_actions = &IBM_SERIAL_ACTIONS;
  serial = SERIAL_USB;
  flow_on = CHAR_IBM_PAUSE;
  flow_off = CHAR_IBM_RESUME;
  hwflow = HWFLOW_NONE;
  artn_IBM = FALSE;
  bold_IBM = FALSE;
  lock_IBM = FALSE;
  send_ack_IBM = FALSE;

  // Turn on the Lock light to indicate mode 0.
  Print_string (&WW_PRINT_Lock);
  lock_IBM = TRUE;
}


//**********************************************************************************************************************
//
//  IBM 1620 Jr. Console Typewriter support routines.
//
//**********************************************************************************************************************

// Print IBM 1620 Jr. setup title.
void Print_IBM_setup_title () {
  Print_string (&WW_PRINT_CRtn);  Print_string (&WW_PRINT_CRtn);
  Print_characters ("---- Cadetwriter: " IBM_VERSION " Setup");
  Print_string (&WW_PRINT_CRtn);  Print_string (&WW_PRINT_CRtn);
}

// Update IBM 1620 Jr. settings.
void Update_IBM_settings () {
  byte obattery = battery;

  // Query new settings.
  Print_string (&WW_PRINT_CRtn);
  errors = Read_truefalse_setting ("record errors", errors);
  warnings = Read_truefalse_setting ("record warnings", warnings);
  battery = Read_truefalse_setting ("batteries installed", battery);
  slash = Read_truefalse_setting ("slash zeroes", slash);
  bold = Read_truefalse_setting ("bold input", bold);
  Print_string (&WW_PRINT_CRtn);

  // Save settings in EEPROM if requested.
  if (Ask_yesno_question ("Save settings", FALSE)) {
    Write_EEPROM (EEPROM_ERRORS, errors);
    Write_EEPROM (EEPROM_WARNINGS, warnings);
    Write_EEPROM (EEPROM_BATTERY, battery);
    Write_EEPROM (EEPROM_SLASH, slash);
    Write_EEPROM (EEPROM_BOLD, bold);
  }
  Print_string (&WW_PRINT_CRtn);
 
  // Set margins and tabs if battery changed.
  if (obattery != battery) {
    Set_margins_tabs (TRUE);
  }
}

// Print IBM 1620 Jr. character set.
void Print_IBM_character_set ()  {
  Print_string (&WW_PRINT_CRtn);  Print_string (&WW_PRINT_SPACE);  Print_string (&WW_PRINT_SPACE);
  Print_string (&WW_PRINT_A);  Print_string (&WW_PRINT_B);  Print_string (&WW_PRINT_C);  Print_string (&WW_PRINT_D);
  Print_string (&WW_PRINT_E);  Print_string (&WW_PRINT_F);  Print_string (&WW_PRINT_G);  Print_string (&WW_PRINT_H);
  Print_string (&WW_PRINT_I);  Print_string (&WW_PRINT_J);  Print_string (&WW_PRINT_K);  Print_string (&WW_PRINT_L);
  Print_string (&WW_PRINT_M);  Print_string (&WW_PRINT_N);  Print_string (&WW_PRINT_O);  Print_string (&WW_PRINT_P);
  Print_string (&WW_PRINT_Q);  Print_string (&WW_PRINT_R);  Print_string (&WW_PRINT_S);  Print_string (&WW_PRINT_T);
  Print_string (&WW_PRINT_U);  Print_string (&WW_PRINT_V);  Print_string (&WW_PRINT_W);  Print_string (&WW_PRINT_X);
  Print_string (&WW_PRINT_Y);  Print_string (&WW_PRINT_Z);
  Print_string (&WW_PRINT_SPACE);
  Print_string (&IBM_PRINT_SLASH_0);  Print_string (&WW_PRINT_1);  Print_string (&WW_PRINT_2);
  Print_string (&WW_PRINT_3);         Print_string (&WW_PRINT_4);  Print_string (&WW_PRINT_5);
  Print_string (&WW_PRINT_6);         Print_string (&WW_PRINT_7);  Print_string (&WW_PRINT_8);
  Print_string (&WW_PRINT_9);
  Print_string (&WW_PRINT_SPACE);
  Print_string (&IBM_PRINT_FLAG_SLASH_0);  Print_string (&IBM_PRINT_FLAG_1);  Print_string (&IBM_PRINT_FLAG_2);
  Print_string (&IBM_PRINT_FLAG_3);        Print_string (&IBM_PRINT_FLAG_4);  Print_string (&IBM_PRINT_FLAG_5);
  Print_string (&IBM_PRINT_FLAG_6);        Print_string (&IBM_PRINT_FLAG_7);  Print_string (&IBM_PRINT_FLAG_8);
  Print_string (&IBM_PRINT_FLAG_9);
  Print_string (&WW_PRINT_SPACE);
  Print_string (&WW_PRINT_DOLLAR);  Print_string (&WW_PRINT_LPAREN);  Print_string (&WW_PRINT_RPAREN);
  Print_string (&WW_PRINT_PLUS);    Print_string (&WW_PRINT_HYPHEN);  Print_string (&WW_PRINT_ASTERISK);
  Print_string (&WW_PRINT_SLASH);   Print_string (&WW_PRINT_EQUAL);   Print_string (&WW_PRINT_PERIOD);
  Print_string (&WW_PRINT_COMMA);   Print_string (&WW_PRINT_AT);
  Print_string (&WW_PRINT_SPACE);
  Print_string (&IBM_PRINT_FLAG_NUMBLANK);
  Print_string (&WW_PRINT_SPACE);
  Print_string (&IBM_PRINT_RMARK);       Print_string (&IBM_PRINT_FLAG_RMARK);  Print_string (&IBM_PRINT_GMARK);
  Print_string (&IBM_PRINT_FLAG_GMARK);
  Print_string (&WW_PRINT_SPACE);
  Print_string (&IBM_PRINT_INVALID);
  Print_string (&WW_PRINT_SPACE);
  Print_string (&IBM_PRINT_RELEASESTART);
  Print_string (&WW_PRINT_CRtn);  Print_string (&WW_PRINT_CRtn);
}



//======================================================================================================================
//
//  ASCII Terminal emulation.
//
//======================================================================================================================

//**********************************************************************************************************************
//
//  ASCII Terminal defines.
//
//**********************************************************************************************************************

// ASCII Terminal name strings.
#define ASCII_NAME     "ASCII Terminal"
#define ASCII_VERSION  ASCII_NAME " (v" VERSION_TEXT ")"

// ASCII Terminal key values.
#define ASCII_KEY_LSHIFT                    WW_KEY_LShift                      // <left shift> key code.
#define ASCII_KEY_RSHIFT                    WW_KEY_RShift                      // <right shift> key code.
#define ASCII_KEY_RARROW                    WW_KEY_RARROW_Word                 // <right arrow> key code.
#define ASCII_KEY_SETUP                     WW_KEY_PaperUp_Micro               // <setup> key code.
#define ASCII_KEY_BSLASH_BAR_FS             WW_KEY_PaperDown_Micro             // \, | key code.
#define ASCII_KEY_DARROW                    WW_KEY_DARROW_Line                 // <down arrow> key code.
#define ASCII_KEY_z_Z_SUB                   WW_KEY_z_Z                         // z, Z, SUB key code.
#define ASCII_KEY_q_Q_DC1_XON               WW_KEY_q_Q_Impr                    // q, Q, DC1/XON key code.
#define ASCII_KEY_1_EXCLAMATION             WW_KEY_1_EXCLAMATION_Spell         // 1, ! key code.
#define ASCII_KEY_BAPOSTROPHE_TILDE_RS      WW_KEY_PLUSMINUS_DEGREE            // `, ~ key code.
#define ASCII_KEY_a_A_SOH                   WW_KEY_a_A                         // a, A, SOH key code.
#define ASCII_KEY_SPACE                     WW_KEY_SPACE_REQSPACE              // <space> key code.
#define ASCII_KEY_LOADPAPER                 WW_KEY_LOADPAPER                   // <load paper> key code.
#define ASCII_KEY_LMAR                      WW_KEY_LMar                        // <left margin> key code.
#define ASCII_KEY_TCLR                      WW_KEY_TClr                        // <tab clear> key code.
#define ASCII_KEY_CONTROL                   WW_KEY_Code                        // <control> key code.
#define ASCII_KEY_x_X_CAN                   WW_KEY_x_X_POWERWISE               // x, X, CAN key code.
#define ASCII_KEY_w_W_ETB                   WW_KEY_w_W                         // w, W, ETB key code.
#define ASCII_KEY_2_AT                      WW_KEY_2_AT_Add                    // 2, @ key code.
#define ASCII_KEY_s_S_DC3_XOFF              WW_KEY_s_S                         // s, S, DC3/XOFF key code.
#define ASCII_KEY_c_C_ETX                   WW_KEY_c_C_Ctr                     // c, C, ETX key code.
#define ASCII_KEY_e_E_ENQ                   WW_KEY_e_E                         // e, E, ENQ key code.
#define ASCII_KEY_3_POUND                   WW_KEY_3_POUND_Del                 // 3, # key code.
#define ASCII_KEY_d_D_EOT                   WW_KEY_d_D_DecT                    // d, D, EOT key code.
#define ASCII_KEY_b_B_STX                   WW_KEY_b_B_Bold                    // b, B, STX key code.
#define ASCII_KEY_v_V_SYN                   WW_KEY_v_V                         // v, V, SYN key code.
#define ASCII_KEY_t_T_DC4                   WW_KEY_t_T                         // t, T, DC4 key code.
#define ASCII_KEY_r_R_DC2                   WW_KEY_r_R_ARtn                    // r, R, DC2 key code.
#define ASCII_KEY_4_DOLLAR                  WW_KEY_4_DOLLAR_Vol                // 4, $ key code.
#define ASCII_KEY_5_PERCENT                 WW_KEY_5_PERCENT                   // 5, % key code.
#define ASCII_KEY_f_F_ACK                   WW_KEY_f_F                         // f, F, ACK key code.
#define ASCII_KEY_g_G_BEL                   WW_KEY_g_G                         // g, G, BEL key code.
#define ASCII_KEY_n_N_SO                    WW_KEY_n_N_Caps                    // n, N, SO key code.
#define ASCII_KEY_m_M_CR                    WW_KEY_m_M                         // m, M, CR key code.
#define ASCII_KEY_y_Y_EM                    WW_KEY_y_Y_12UP                    // y, Y, EM key code.
#define ASCII_KEY_u_U_NAK                   WW_KEY_u_U_Cont                    // u, U, NAK key code.
#define ASCII_KEY_7_AMPERSAND               WW_KEY_7_AMPERSAND                 // 7, & key code.
#define ASCII_KEY_6_CARET                   WW_KEY_6_CENT                      // 6, ^, RS key code.
#define ASCII_KEY_j_J_LF                    WW_KEY_j_J                         // j, J, LF key code.
#define ASCII_KEY_h_H_BS                    WW_KEY_h_H_12DOWN                  // h, H, BS key code.
#define ASCII_KEY_COMMA_LESS                WW_KEY_COMMA_COMMA                 // ,, < key code.
#define ASCII_KEY_RBRACKET_RBRACE_GS        WW_KEY_RBRACKET_LBRACKET_SUPER3    // ], }, GS key code.
#define ASCII_KEY_i_I_TAB                   WW_KEY_i_I_Word                    // i, I, TAB key code.
#define ASCII_KEY_8_ASTERISK                WW_KEY_8_ASTERISK                  // 8, * key code.
#define ASCII_KEY_EQUAL_PLUS                WW_KEY_EQUAL_PLUS                  // =, + key code.
#define ASCII_KEY_k_K_VT                    WW_KEY_k_K                         // k, K, VT key code.
#define ASCII_KEY_PERIOD_GREATER            WW_KEY_PERIOD_PERIOD               // ., > key code.
#define ASCII_KEY_o_O_SI                    WW_KEY_o_O_RFlsh                   // o, O, SI key code.
#define ASCII_KEY_9_LPAREN                  WW_KEY_9_LPAREN_Stop               // 9, ( key code.
#define ASCII_KEY_l_L_FF                    WW_KEY_l_L_Lang                    // l, L, FF key code.
#define ASCII_KEY_SLASH_QUESTION            WW_KEY_SLASH_QUESTION              // /, ? key code.
#define ASCII_KEY_LBRACKET_LBRACE_ESC       WW_KEY_HALF_QUARTER_SUPER2         // [, {, ESC key code.
#define ASCII_KEY_p_P_DLE                   WW_KEY_p_P                         // p, P, DLE key code.
#define ASCII_KEY_0_RPAREN                  WW_KEY_0_RPAREN                    // 0, ) key code.
#define ASCII_KEY_HYPHEN_UNDERSCORE         WW_KEY_HYPHEN_UNDERSCORE           // -, _, US key code.
#define ASCII_KEY_SEMICOLON_COLON           WW_KEY_SEMICOLON_COLON_SECTION     // ;, : key code.
#define ASCII_KEY_APOSTROPHE_QUOTE          WW_KEY_APOSTROPHE_QUOTE_PARAGRAPH  // ', " key code.
#define ASCII_KEY_LARROW                    WW_KEY_LARROW_Word                 // <left arrow> key code.
#define ASCII_KEY_UARROW                    WW_KEY_UARROW_Line                 // <up arrow> key code.
#define ASCII_KEY_BACKSPACE                 WW_KEY_Backspace_Bksp1             // <backspace> key code.
#define ASCII_KEY_RETURN                    WW_KEY_CRtn_IndClr                 // <return> key code.
#define ASCII_KEY_DELETE_US                 WW_KEY_BACKX_Word                  // <delete> key code.
#define ASCII_KEY_LOCK                      WW_KEY_Lock                        // <shift lock> key code.
#define ASCII_KEY_RMAR                      WW_KEY_RMar                        // <right margin> key code.
#define ASCII_KEY_TAB                       WW_KEY_Tab_IndL                    // <tab> key code.
#define ASCII_KEY_ESCAPE_MARREL             WW_KEY_MarRel_RePrt                // <escape>, <margin release> key code.
#define ASCII_KEY_TSET                      WW_KEY_TSet                        // <tab set> key code.

// ASCII Terminal print string timing values (in usec).
#define TIMING_ASCII_SPACE2       (POSITIVE((1 * TIME_CHARACTER + 0 * TIME_HMOVEMENT + 0 * TIME_VMOVEMENT) - \
                                            (4 * FSCAN_1_CHANGE + 0 * FSCAN_2_CHANGES + 0 * FSCAN_3_CHANGES) + \
                                            (-1 * TIME_ADJUST)))
                                    // Residual time for special space print character.

#define TIMING_ASCII_SPACE3       (POSITIVE((1 * TIME_CHARACTER + 1 * TIME_HMOVEMENT + 0 * TIME_VMOVEMENT) - \
                                            (6 * FSCAN_1_CHANGE + 1 * FSCAN_2_CHANGES + 0 * FSCAN_3_CHANGES) + \
                                            (-1 * TIME_ADJUST)))
                                    // Residual time for special space print character.

#define TIMING_ASCII_LESS         (POSITIVE((3 * TIME_CHARACTER + 7 * TIME_HMOVEMENT + 6 * TIME_VMOVEMENT) - \
                                            (23 * FSCAN_1_CHANGE + 4 * FSCAN_2_CHANGES + 3 * FSCAN_3_CHANGES) + \
                                            (-13 * TIME_ADJUST)))
                                    // Residual time for less print character.

#define TIMING_ASCII_GREATER      (POSITIVE((3 * TIME_CHARACTER + 7 * TIME_HMOVEMENT + 3 * TIME_VMOVEMENT) - \
                                            (27 * FSCAN_1_CHANGE + 4 * FSCAN_2_CHANGES + 3 * FSCAN_3_CHANGES) + \
                                            (65 * TIME_ADJUST)))
                                    // Residual time for greater print character.

#define TIMING_ASCII_BSLASH       (POSITIVE((4 * TIME_CHARACTER + 8 * TIME_HMOVEMENT + 8 * TIME_VMOVEMENT) - \
                                            (28 * FSCAN_1_CHANGE + 7 * FSCAN_2_CHANGES + 4 * FSCAN_3_CHANGES) + \
                                            (50 * TIME_ADJUST)))
                                    // Residual time for backslash print character.

#define TIMING_ASCII_CARET        (POSITIVE((3 * TIME_CHARACTER + 7 * TIME_HMOVEMENT + 8 * TIME_VMOVEMENT) - \
                                            (30 * FSCAN_1_CHANGE + 5 * FSCAN_2_CHANGES + 2 * FSCAN_3_CHANGES) + \
                                            (11 * TIME_ADJUST)))
                                    // Residual time for caret print character.

#define TIMING_ASCII_BAPOSTROPHE  (POSITIVE((2 * TIME_CHARACTER + 6 * TIME_HMOVEMENT + 8 * TIME_VMOVEMENT) - \
                                            (28 * FSCAN_1_CHANGE + 3 * FSCAN_2_CHANGES + 2 * FSCAN_3_CHANGES) + \
                                            (-48 * TIME_ADJUST)))
                                    // Residual time for back apostophe print character.

#define TIMING_ASCII_LBRACE       (POSITIVE((3 * TIME_CHARACTER + 7 * TIME_HMOVEMENT + 0 * TIME_VMOVEMENT) - \
                                            (17 * FSCAN_1_CHANGE + 6 * FSCAN_2_CHANGES + 1 * FSCAN_3_CHANGES) + \
                                            (10 * TIME_ADJUST)))
                                    // Residual time for left brace print character.

#define TIMING_ASCII_BAR          (POSITIVE((2 * TIME_CHARACTER + 1 * TIME_HMOVEMENT + 0 * TIME_VMOVEMENT) - \
                                            (4 * FSCAN_1_CHANGE + 2 * FSCAN_2_CHANGES + 0 * FSCAN_3_CHANGES) + \
                                            (-8 * TIME_ADJUST)))
                                    // Residual time for bar print character.

#define TIMING_ASCII_RBRACE       (POSITIVE((3 * TIME_CHARACTER + 7 * TIME_HMOVEMENT + 0 * TIME_VMOVEMENT) - \
                                            (18 * FSCAN_1_CHANGE + 6 * FSCAN_2_CHANGES + 0 * FSCAN_3_CHANGES) + \
                                            (8 * TIME_ADJUST)))
                                    // Residual time for right brace print character.

#define TIMING_ASCII_TILDE        (POSITIVE((5 * TIME_CHARACTER + 9 * TIME_HMOVEMENT + 8 * TIME_VMOVEMENT) - \
                                            (33 * FSCAN_1_CHANGE + 9 * FSCAN_2_CHANGES + 5 * FSCAN_3_CHANGES) + \
                                            (88 * TIME_ADJUST)))
                                    // Residual time for tilde print character.

// ASCII Terminal characters.
#define CHAR_ASCII_LF    0x0a  // LF character.
#define CHAR_ASCII_CR    0x0d  // CR character.
#define CHAR_ASCII_XON   0x11  // XON character.
#define CHAR_ASCII_XOFF  0x13  // XOFF character.
#define CHAR_ASCII_ESC   0x1b  // ESC character.


//**********************************************************************************************************************
//
//  ASCII Terminal print strings.
//
//**********************************************************************************************************************

// <space> print string.
const byte ASCII_STR_SPACE2[]               = {WW_SPACE_REQSPACE, WW_NULL_4, WW_SPACE_REQSPACE, WW_NULL_4, WW_NULL_14,
                                               WW_NULL};
const struct print_info ASCII_PRINT_SPACE2  = {SPACING_FORWARD, TIMING_ASCII_SPACE2, &ASCII_STR_SPACE2};

const byte ASCII_STR_SPACE3[]               = {WW_SPACE_REQSPACE, WW_NULL_4, WW_SPACE_REQSPACE, WW_NULL_4, WW_Code,
                                               WW_Backspace_Bksp1, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_SPACE3  = {SPACING_FORWARD, TIMING_ASCII_SPACE3, &ASCII_STR_SPACE3};

// <, >, \, ^, `, {, |, }, ~ print strings.
const byte ASCII_STR_LESS[]                     = {WW_Code, WW_PaperDown_Micro, WW_Code, WW_Code, WW_PaperDown_Micro,
                                                   WW_Code, WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11, WW_Code,
                                                   WW_Backspace_Bksp1, WW_Code, WW_Code, WW_Backspace_Bksp1, WW_Code,
                                                   WW_Code, WW_Backspace_Bksp1, WW_Code, WW_Code, WW_Backspace_Bksp1,
                                                   WW_Code, WW_Code, WW_Backspace_Bksp1, WW_Code, WW_PaperDown_Micro,
                                                   WW_Code, WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11, WW_Backspace_Bksp1,
                                                   WW_NULL_13, WW_Code, WW_PaperUp_Micro, WW_Code, WW_Code,
                                                   WW_PaperUp_Micro, WW_Code, WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11,
                                                   WW_Code, WW_Backspace_Bksp1, WW_Code, WW_PaperUp_Micro, WW_Code,
                                                   WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_LESS        = {SPACING_FORWARD, TIMING_ASCII_LESS, &ASCII_STR_LESS};

const byte ASCII_STR_GREATER[]                  = {WW_MarRel_RePrt, WW_NULL_14, WW_Code, WW_Backspace_Bksp1, WW_Code,
                                                   WW_PaperDown_Micro, WW_Code, WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11,
                                                   WW_MarRel_RePrt, WW_NULL_14, WW_Backspace_Bksp1, WW_NULL_13, WW_Code,
                                                   WW_PaperDown_Micro, WW_Code, WW_Code, WW_PaperDown_Micro, WW_Code,
                                                   WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11, WW_Code, WW_Backspace_Bksp1,
                                                   WW_Code, WW_Code, WW_Backspace_Bksp1, WW_Code, WW_Code,
                                                   WW_Backspace_Bksp1, WW_Code, WW_Code, WW_Backspace_Bksp1, WW_Code,
                                                   WW_Code, WW_Backspace_Bksp1, WW_Code, WW_PaperUp_Micro, WW_Code,
                                                   WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11, WW_Code, WW_PaperUp_Micro,
                                                   WW_Code, WW_Code, WW_PaperUp_Micro, WW_Code, WW_NULL_2, WW_NULL_14,
                                                   WW_NULL};
const struct print_info ASCII_PRINT_GREATER     = {SPACING_FORWARD, TIMING_ASCII_GREATER, &ASCII_STR_GREATER};

const byte ASCII_STR_BSLASH[]                   = {WW_MarRel_RePrt, WW_NULL_14, WW_Code, WW_Backspace_Bksp1, WW_Code,
                                                   WW_PaperDown_Micro, WW_Code, WW_Code, WW_PaperDown_Micro, WW_Code,
                                                   WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11, WW_MarRel_RePrt, WW_NULL_14,
                                                   WW_Backspace_Bksp1, WW_NULL_13, WW_Code, WW_Backspace_Bksp1, WW_Code,
                                                   WW_PaperDown_Micro, WW_Code, WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11,
                                                   WW_Code, WW_Backspace_Bksp1, WW_Code, WW_Code, WW_Backspace_Bksp1,
                                                   WW_Code, WW_Code, WW_Backspace_Bksp1, WW_Code, WW_PaperUp_Micro,
                                                   WW_Code, WW_Code, WW_PaperUp_Micro, WW_Code, WW_Code,
                                                   WW_PaperUp_Micro, WW_Code, WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11,
                                                   WW_Backspace_Bksp1, WW_NULL_13, WW_Code, WW_Backspace_Bksp1, WW_Code,
                                                   WW_PaperDown_Micro, WW_Code, WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11,
                                                   WW_Code, WW_PaperUp_Micro, WW_Code, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_BSLASH      = {SPACING_FORWARD, TIMING_ASCII_BSLASH, &ASCII_STR_BSLASH};

const byte ASCII_STR_CARET[]                    = {WW_MarRel_RePrt, WW_NULL_14, WW_Code, WW_Backspace_Bksp1, WW_Code,
                                                   WW_PaperDown_Micro, WW_Code, WW_Code, WW_PaperDown_Micro, WW_Code,
                                                   WW_Code, WW_PaperDown_Micro, WW_Code, WW_NULL_2, WW_PERIOD_PERIOD,
                                                   WW_NULL_11, WW_Code, WW_Backspace_Bksp1, WW_Code, WW_Code,
                                                   WW_Backspace_Bksp1, WW_Code, WW_Code, WW_Backspace_Bksp1, WW_Code,
                                                   WW_Code, WW_Backspace_Bksp1, WW_Code, WW_NULL_5, WW_PERIOD_PERIOD,
                                                   WW_NULL_11, WW_Backspace_Bksp1, WW_NULL_13, WW_Code,
                                                   WW_Backspace_Bksp1, WW_Code, WW_PaperDown_Micro, WW_Code, WW_NULL_2,
                                                   WW_PERIOD_PERIOD, WW_NULL_11, WW_Code, WW_PaperUp_Micro, WW_Code,
                                                   WW_Code, WW_PaperUp_Micro, WW_Code, WW_Code, WW_PaperUp_Micro,
                                                   WW_Code, WW_Code, WW_PaperUp_Micro, WW_Code, WW_NULL_2, WW_NULL_14,
                                                   WW_NULL};
const struct print_info ASCII_PRINT_CARET       = {SPACING_FORWARD, TIMING_ASCII_CARET, &ASCII_STR_CARET};

const byte ASCII_STR_BAPOSTROPHE[]              = {WW_MarRel_RePrt, WW_NULL_14, WW_Code, WW_Backspace_Bksp1, WW_Code,
                                                   WW_PaperDown_Micro, WW_Code, WW_Code, WW_PaperDown_Micro, WW_Code,
                                                   WW_Code, WW_PaperDown_Micro, WW_Code, WW_Code, WW_PaperDown_Micro,
                                                   WW_Code, WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11, WW_Code,
                                                   WW_Backspace_Bksp1, WW_Code, WW_Code, WW_Backspace_Bksp1, WW_Code,
                                                   WW_Code, WW_Backspace_Bksp1, WW_Code, WW_Code, WW_Backspace_Bksp1,
                                                   WW_Code, WW_Code, WW_Backspace_Bksp1, WW_Code, WW_PaperUp_Micro,
                                                   WW_Code, WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11, WW_Code,
                                                   WW_PaperUp_Micro, WW_Code, WW_Code, WW_PaperUp_Micro, WW_Code,
                                                   WW_Code, WW_PaperUp_Micro, WW_Code, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_BAPOSTROPHE = {SPACING_FORWARD, TIMING_ASCII_BAPOSTROPHE, &ASCII_STR_BAPOSTROPHE};


const byte ASCII_STR_LBRACE[]                   = {WW_MarRel_RePrt, WW_NULL_14, WW_Code, WW_Backspace_Bksp1, WW_Code,
                                                   WW_NULL_5, WW_HYPHEN_UNDERSCORE, WW_NULL_12, WW_Code,
                                                   WW_Backspace_Bksp1, WW_Code, WW_Code, WW_Backspace_Bksp1, WW_Code,
                                                   WW_Code, WW_Backspace_Bksp1, WW_Code, WW_Code, WW_Backspace_Bksp1,
                                                   WW_Code, WW_NULL_5, WW_LShift, WW_9_LPAREN_Stop, WW_LShift,
                                                   WW_NULL_1, WW_Backspace_Bksp1, WW_NULL_13, WW_LShift,
                                                   WW_RBRACKET_LBRACKET_SUPER3, WW_LShift, WW_NULL_1, WW_Code,
                                                   WW_Backspace_Bksp1, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_LBRACE      = {SPACING_FORWARD, TIMING_ASCII_LBRACE, &ASCII_STR_LBRACE};

const byte ASCII_STR_BAR[]                      = {WW_LShift, WW_1_EXCLAMATION_Spell, WW_LShift, WW_NULL_1,
                                                   WW_Backspace_Bksp1, WW_NULL_13, WW_i_I_Word, WW_NULL_10, WW_NULL_14,
                                                   WW_NULL};
const struct print_info ASCII_PRINT_BAR         = {SPACING_FORWARD, TIMING_ASCII_BAR, &ASCII_STR_BAR};

const byte ASCII_STR_RBRACE[]                   = {WW_MarRel_RePrt, WW_NULL_14, WW_Code, WW_Backspace_Bksp1, WW_Code,
                                                   WW_NULL_5, WW_LShift, WW_0_RPAREN, WW_LShift, WW_NULL_1,
                                                   WW_Backspace_Bksp1, WW_NULL_13, WW_RBRACKET_LBRACKET_SUPER3,
                                                   WW_NULL_10, WW_Code, WW_Backspace_Bksp1, WW_Code, WW_Code,
                                                   WW_Backspace_Bksp1, WW_Code, WW_Code, WW_Backspace_Bksp1, WW_Code,
                                                   WW_Code, WW_Backspace_Bksp1, WW_Code, WW_NULL_5,
                                                   WW_HYPHEN_UNDERSCORE, WW_NULL_12, WW_Code, WW_Backspace_Bksp1,
                                                   WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_RBRACE      = {SPACING_FORWARD, TIMING_ASCII_RBRACE, &ASCII_STR_RBRACE};

const byte ASCII_STR_TILDE[]                    = {WW_MarRel_RePrt, WW_NULL_14, WW_Code, WW_Backspace_Bksp1, WW_Code,
                                                   WW_PaperDown_Micro, WW_Code, WW_Code, WW_PaperDown_Micro, WW_Code,
                                                   WW_Code, WW_PaperDown_Micro, WW_Code, WW_NULL_2, WW_PERIOD_PERIOD,
                                                   WW_NULL_11, WW_MarRel_RePrt, WW_NULL_14, WW_Backspace_Bksp1,
                                                   WW_NULL_13, WW_Code, WW_Backspace_Bksp1, WW_Code, WW_PaperUp_Micro,
                                                   WW_Code, WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11, WW_Code,
                                                   WW_Backspace_Bksp1, WW_Code, WW_Code, WW_Backspace_Bksp1, WW_Code,
                                                   WW_NULL_5, WW_PERIOD_PERIOD, WW_NULL_11, WW_Backspace_Bksp1,
                                                   WW_NULL_13, WW_Code, WW_Backspace_Bksp1, WW_Code, WW_PaperUp_Micro,
                                                   WW_Code, WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11, WW_Backspace_Bksp1,
                                                   WW_NULL_13, WW_Code, WW_Backspace_Bksp1, WW_Code, WW_PaperDown_Micro,
                                                   WW_Code, WW_NULL_2, WW_PERIOD_PERIOD, WW_NULL_11, WW_SPACE_REQSPACE,
                                                   WW_NULL_4, WW_Code, WW_Backspace_Bksp1, WW_Code, WW_Code,
                                                   WW_Backspace_Bksp1, WW_Code, WW_PaperUp_Micro, WW_Code, WW_Code,
                                                   WW_PaperUp_Micro, WW_Code, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_TILDE       = {SPACING_FORWARD, TIMING_ASCII_TILDE, &ASCII_STR_TILDE};

// CR, LF print strings.
const byte ASCII_STR_CR[]              = {WW_CRtn_IndClr, WW_NULL_13, WW_UARROW_Line, WW_NULL_13, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_CR = {SPACING_RETURN, TIMING_RETURN, &ASCII_STR_CR};

const byte ASCII_STR_LF[]              = {WW_DARROW_Line, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_LF = {SPACING_NONE, TIMING_VMOVE, &ASCII_STR_LF};


//**********************************************************************************************************************
//
//  ASCII Terminal serial action table.
//
//**********************************************************************************************************************

const struct serial_action ASCII_SERIAL_ACTIONS[128] = { 
  {CMD_NONE,       NULL},                      // NUL
  {CMD_NONE,       NULL},                      // SOH
  {CMD_NONE,       NULL},                      // STX
  {CMD_NONE,       NULL},                      // ETX
  {CMD_NONE,       NULL},                      // EOT
  {CMD_NONE,       NULL},                      // ENQ
  {CMD_NONE,       NULL},                      // ACK
  {CMD_PRINT,      &WW_PRINT_BEEP},            // BEL
  {CMD_PRINT,      &WW_PRINT_Backspace},       // BS
  {CMD_PRINT,      &WW_PRINT_Tab},             // TAB
  {CMD_ASCII_LF,   NULL},                      // LF
  {CMD_NONE,       NULL},                      // VT
  {CMD_NONE,       NULL},                      // FF
  {CMD_ASCII_CR,   &WW_PRINT_CRtn},            // CR
  {CMD_NONE,       NULL},                      // SO
  {CMD_NONE,       NULL},                      // SI
  {CMD_NONE,       NULL},                      // DLE
  {CMD_ASCII_XON,  NULL},                      // DC1/XON
  {CMD_NONE,       NULL},                      // DC2
  {CMD_ASCII_XOFF, NULL},                      // DC3/XOFF
  {CMD_NONE,       NULL},                      // DC4
  {CMD_NONE,       NULL},                      // NAK
  {CMD_NONE,       NULL},                      // SYN
  {CMD_NONE,       NULL},                      // ETB
  {CMD_NONE,       NULL},                      // CAN
  {CMD_NONE,       NULL},                      // EM
  {CMD_NONE,       NULL},                      // SUB
  {CMD_NONE,       NULL},                      // ESC
  {CMD_NONE,       NULL},                      // FS
  {CMD_NONE,       NULL},                      // GS
  {CMD_NONE,       NULL},                      // RS
  {CMD_NONE,       NULL},                      // US
  {CMD_PRINT,      &WW_PRINT_SPACE},           //
  {CMD_PRINT,      &WW_PRINT_EXCLAMATION},     // !
  {CMD_PRINT,      &WW_PRINT_QUOTE},           // "
  {CMD_PRINT,      &WW_PRINT_POUND},           // #
  {CMD_PRINT,      &WW_PRINT_DOLLAR},          // $
  {CMD_PRINT,      &WW_PRINT_PERCENT},         // %
  {CMD_PRINT,      &WW_PRINT_AMPERSAND},       // &
  {CMD_PRINT,      &WW_PRINT_APOSTROPHE},      // '
  {CMD_PRINT,      &WW_PRINT_LPAREN},          // (
  {CMD_PRINT,      &WW_PRINT_RPAREN},          // )
  {CMD_PRINT,      &WW_PRINT_ASTERISK},        // *
  {CMD_PRINT,      &WW_PRINT_PLUS},            // +
  {CMD_PRINT,      &WW_PRINT_COMMA},           // , 
  {CMD_PRINT,      &WW_PRINT_HYPHEN},          // -
  {CMD_PRINT,      &WW_PRINT_PERIOD},          // .
  {CMD_PRINT,      &WW_PRINT_SLASH},           // /
  {CMD_PRINT,      &WW_PRINT_0},               // 0
  {CMD_PRINT,      &WW_PRINT_1},               // 1
  {CMD_PRINT,      &WW_PRINT_2},               // 2
  {CMD_PRINT,      &WW_PRINT_3},               // 3
  {CMD_PRINT,      &WW_PRINT_4},               // 4
  {CMD_PRINT,      &WW_PRINT_5},               // 5
  {CMD_PRINT,      &WW_PRINT_6},               // 6
  {CMD_PRINT,      &WW_PRINT_7},               // 7
  {CMD_PRINT,      &WW_PRINT_8},               // 8
  {CMD_PRINT,      &WW_PRINT_9},               // 9
  {CMD_PRINT,      &WW_PRINT_COLON},           // :
  {CMD_PRINT,      &WW_PRINT_SEMICOLON},       // ;
  {CMD_PRINT,      &ASCII_PRINT_LESS},         // <
  {CMD_PRINT,      &WW_PRINT_EQUAL},           // =
  {CMD_PRINT,      &ASCII_PRINT_GREATER},      // >
  {CMD_PRINT,      &WW_PRINT_QUESTION},        // ?
  {CMD_PRINT,      &WW_PRINT_AT},              // @
  {CMD_PRINT,      &WW_PRINT_A},               // A
  {CMD_PRINT,      &WW_PRINT_B},               // B
  {CMD_PRINT,      &WW_PRINT_C},               // C
  {CMD_PRINT,      &WW_PRINT_D},               // D
  {CMD_PRINT,      &WW_PRINT_E},               // E
  {CMD_PRINT,      &WW_PRINT_F},               // F
  {CMD_PRINT,      &WW_PRINT_G},               // G
  {CMD_PRINT,      &WW_PRINT_H},               // H
  {CMD_PRINT,      &WW_PRINT_I},               // I
  {CMD_PRINT,      &WW_PRINT_J},               // J
  {CMD_PRINT,      &WW_PRINT_K},               // K
  {CMD_PRINT,      &WW_PRINT_L},               // L
  {CMD_PRINT,      &WW_PRINT_M},               // M
  {CMD_PRINT,      &WW_PRINT_N},               // N
  {CMD_PRINT,      &WW_PRINT_O},               // O
  {CMD_PRINT,      &WW_PRINT_P},               // P
  {CMD_PRINT,      &WW_PRINT_Q},               // Q
  {CMD_PRINT,      &WW_PRINT_R},               // R
  {CMD_PRINT,      &WW_PRINT_S},               // S
  {CMD_PRINT,      &WW_PRINT_T},               // T
  {CMD_PRINT,      &WW_PRINT_U},               // U
  {CMD_PRINT,      &WW_PRINT_V},               // V
  {CMD_PRINT,      &WW_PRINT_W},               // W
  {CMD_PRINT,      &WW_PRINT_X},               // X
  {CMD_PRINT,      &WW_PRINT_Y},               // Y
  {CMD_PRINT,      &WW_PRINT_Z},               // Z
  {CMD_PRINT,      &WW_PRINT_LBRACKET},        // [
  {CMD_PRINT,      &ASCII_PRINT_BSLASH},       // <backslash>
  {CMD_PRINT,      &WW_PRINT_RBRACKET},        // ]
  {CMD_PRINT,      &ASCII_PRINT_CARET},        // ^
  {CMD_PRINT,      &WW_PRINT_UNDERSCORE},      // _
  {CMD_PRINT,      &ASCII_PRINT_BAPOSTROPHE},  // `
  {CMD_PRINT,      &WW_PRINT_a},               // a
  {CMD_PRINT,      &WW_PRINT_b},               // b
  {CMD_PRINT,      &WW_PRINT_c},               // c
  {CMD_PRINT,      &WW_PRINT_d},               // d
  {CMD_PRINT,      &WW_PRINT_e},               // e
  {CMD_PRINT,      &WW_PRINT_f},               // f
  {CMD_PRINT,      &WW_PRINT_g},               // g
  {CMD_PRINT,      &WW_PRINT_h},               // h
  {CMD_PRINT,      &WW_PRINT_i},               // i
  {CMD_PRINT,      &WW_PRINT_j},               // j
  {CMD_PRINT,      &WW_PRINT_k},               // k
  {CMD_PRINT,      &WW_PRINT_l},               // l
  {CMD_PRINT,      &WW_PRINT_m},               // m
  {CMD_PRINT,      &WW_PRINT_n},               // n
  {CMD_PRINT,      &WW_PRINT_o},               // o
  {CMD_PRINT,      &WW_PRINT_p},               // p
  {CMD_PRINT,      &WW_PRINT_q},               // q
  {CMD_PRINT,      &WW_PRINT_r},               // r
  {CMD_PRINT,      &WW_PRINT_s},               // s
  {CMD_PRINT,      &WW_PRINT_t},               // t
  {CMD_PRINT,      &WW_PRINT_u},               // u
  {CMD_PRINT,      &WW_PRINT_v},               // v
  {CMD_PRINT,      &WW_PRINT_w},               // w
  {CMD_PRINT,      &WW_PRINT_x},               // x
  {CMD_PRINT,      &WW_PRINT_y},               // y
  {CMD_PRINT,      &WW_PRINT_z},               // z
  {CMD_PRINT,      &ASCII_PRINT_LBRACE},       // {
  {CMD_PRINT,      &ASCII_PRINT_BAR},          // |
  {CMD_PRINT,      &ASCII_PRINT_RBRACE},       // }
  {CMD_PRINT,      &ASCII_PRINT_TILDE},        // ~
  {CMD_NONE,       NULL}                       // DEL
};


//**********************************************************************************************************************
//
//  ASCII Terminal key action tables.
//
//**********************************************************************************************************************

// Half duplex key action table.
const struct key_action ASCII_ACTIONS_HALF[3 * NUM_WW_KEYS] = { 
  // Unshifted.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_PRINT,                                    0,     &WW_PRINT_RARROW},          // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      '\\',  &ASCII_PRINT_BSLASH},       // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_PRINT,                                    0,     &WW_PRINT_DARROW},          // <down arrow>
  {ACTION_SEND | ACTION_PRINT,                      'z',   &WW_PRINT_z},               // z, Z, SUB
  {ACTION_SEND | ACTION_PRINT,                      'q',   &WW_PRINT_q},               // q, Q, DC1/XON
  {ACTION_SEND | ACTION_PRINT,                      '1',   &WW_PRINT_1},               // 1, !
  {ACTION_SEND | ACTION_PRINT,                      '`',   &ASCII_PRINT_BAPOSTROPHE},  // `, ~
  {ACTION_SEND | ACTION_PRINT,                      'a',   &WW_PRINT_a},               // a, A, SOH
  {ACTION_SEND | ACTION_PRINT,                      ' ',   &WW_PRINT_SPACE},           // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},       // <load paper>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_SEND | ACTION_PRINT,                      'x',   &WW_PRINT_x},               // x, X, CAN
  {ACTION_SEND | ACTION_PRINT,                      'w',   &WW_PRINT_w},               // w, W, ETB
  {ACTION_SEND | ACTION_PRINT,                      '2',   &WW_PRINT_2},               // 2, @, NUL
  {ACTION_SEND | ACTION_PRINT,                      's',   &WW_PRINT_s},               // s, S, DC3/XOFF
  {ACTION_SEND | ACTION_PRINT,                      'c',   &WW_PRINT_c},               // c, C, ETX
  {ACTION_SEND | ACTION_PRINT,                      'e',   &WW_PRINT_e},               // e, E, ENQ
  {ACTION_SEND | ACTION_PRINT,                      '3',   &WW_PRINT_3},               // 3, #
  {ACTION_SEND | ACTION_PRINT,                      'd',   &WW_PRINT_d},               // d, D, EOT
  {ACTION_SEND | ACTION_PRINT,                      'b',   &WW_PRINT_b},               // b, B, STX
  {ACTION_SEND | ACTION_PRINT,                      'v',   &WW_PRINT_v},               // v, V, SYN
  {ACTION_SEND | ACTION_PRINT,                      't',   &WW_PRINT_t},               // t, T, DC4
  {ACTION_SEND | ACTION_PRINT,                      'r',   &WW_PRINT_r},               // r, R, DC2
  {ACTION_SEND | ACTION_PRINT,                      '4',   &WW_PRINT_4},               // 4, $
  {ACTION_SEND | ACTION_PRINT,                      '5',   &WW_PRINT_5},               // 5, %
  {ACTION_SEND | ACTION_PRINT,                      'f',   &WW_PRINT_f},               // f, F, ACK
  {ACTION_SEND | ACTION_PRINT,                      'g',   &WW_PRINT_g},               // g, G, BEL
  {ACTION_SEND | ACTION_PRINT,                      'n',   &WW_PRINT_n},               // n, N, SO
  {ACTION_SEND | ACTION_PRINT,                      'm',   &WW_PRINT_m},               // m, M, CR
  {ACTION_SEND | ACTION_PRINT,                      'y',   &WW_PRINT_y},               // y, Y, EM
  {ACTION_SEND | ACTION_PRINT,                      'u',   &WW_PRINT_u},               // u, U, NAK
  {ACTION_SEND | ACTION_PRINT,                      '7',   &WW_PRINT_7},               // 7, &
  {ACTION_SEND | ACTION_PRINT,                      '6',   &WW_PRINT_6},               // 6, ^, RS
  {ACTION_SEND | ACTION_PRINT,                      'j',   &WW_PRINT_j},               // j, J, LF
  {ACTION_SEND | ACTION_PRINT,                      'h',   &WW_PRINT_h},               // h, H, BS
  {ACTION_SEND | ACTION_PRINT,                      ',',   &WW_PRINT_COMMA},           // ,, <
  {ACTION_SEND | ACTION_PRINT,                      ']',   &WW_PRINT_RBRACKET},        // ], }, GS
  {ACTION_SEND | ACTION_PRINT,                      'i',   &WW_PRINT_i},               // i, I, TAB
  {ACTION_SEND | ACTION_PRINT,                      '8',   &WW_PRINT_8},               // 8, *
  {ACTION_SEND | ACTION_PRINT,                      '=',   &WW_PRINT_EQUAL},           // =, +
  {ACTION_SEND | ACTION_PRINT,                      'k',   &WW_PRINT_k},               // k, K, VT
  {ACTION_SEND | ACTION_PRINT,                      '.',   &WW_PRINT_PERIOD},          // ., >
  {ACTION_SEND | ACTION_PRINT,                      'o',   &WW_PRINT_o},               // o, O, SI
  {ACTION_SEND | ACTION_PRINT,                      '9',   &WW_PRINT_9},               // 9, (
  {ACTION_SEND | ACTION_PRINT,                      'l',   &WW_PRINT_l},               // l, L, FF
  {ACTION_SEND | ACTION_PRINT,                      '/',   &WW_PRINT_SLASH},           // /, ?
  {ACTION_SEND | ACTION_PRINT,                      '[',   &WW_PRINT_LBRACKET},        // [, {, ESC
  {ACTION_SEND | ACTION_PRINT,                      'p',   &WW_PRINT_p},               // p, P, DLE
  {ACTION_SEND | ACTION_PRINT,                      '0',   &WW_PRINT_0},               // 0, )
  {ACTION_SEND | ACTION_PRINT,                      '-',   &WW_PRINT_HYPHEN},          // -, _, US
  {ACTION_SEND | ACTION_PRINT,                      ';',   &WW_PRINT_SEMICOLON},       // ;, :
  {ACTION_SEND | ACTION_PRINT,                      '\'',  &WW_PRINT_APOSTROPHE},      // ', "
  {ACTION_PRINT,                                    0,     &WW_PRINT_LARROW},          // <left arrow>
  {ACTION_PRINT,                                    0,     &WW_PRINT_UARROW},          // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      0x08,  &WW_PRINT_Backspace},       // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_ASCII_RETURN,                             0,     NULL},                      // <return>
  {ACTION_SEND,                                     0x7f,  NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                      // <right margin>
  {ACTION_SEND | ACTION_PRINT,                      0x09,  &WW_PRINT_Tab},             // <tab>
  {ACTION_SEND,                                     0x1b,  NULL},                      // <escape>
  {ACTION_NONE,                                     0,     NULL},                      // <tab set>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_PRINT,                                    0,     &WW_PRINT_RARROW},          // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      '|',   &ASCII_PRINT_BAR},          // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_PRINT,                                    0,     &WW_PRINT_DARROW},          // <down arrow>
  {ACTION_SEND | ACTION_PRINT,                      'Z',   &WW_PRINT_Z},               // z, Z, SUB
  {ACTION_SEND | ACTION_PRINT,                      'Q',   &WW_PRINT_Q},               // q, Q, DC1/XON
  {ACTION_SEND | ACTION_PRINT,                      '!',   &WW_PRINT_EXCLAMATION},     // 1, !
  {ACTION_SEND | ACTION_PRINT,                      '~',   &ASCII_PRINT_TILDE},        // `, ~
  {ACTION_SEND | ACTION_PRINT,                      'A',   &WW_PRINT_A},               // a, A, SOH
  {ACTION_SEND | ACTION_PRINT,                      ' ',   &WW_PRINT_SPACE},           // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},       // <load paper>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_SEND | ACTION_PRINT,                      'X',   &WW_PRINT_X},               // x, X, CAN
  {ACTION_SEND | ACTION_PRINT,                      'W',   &WW_PRINT_W},               // w, W, ETB
  {ACTION_SEND | ACTION_PRINT,                      '@',   &WW_PRINT_AT},              // 2, @, NUL
  {ACTION_SEND | ACTION_PRINT,                      'S',   &WW_PRINT_S},               // s, S, DC3/XOFF
  {ACTION_SEND | ACTION_PRINT,                      'C',   &WW_PRINT_C},               // c, C, ETX
  {ACTION_SEND | ACTION_PRINT,                      'E',   &WW_PRINT_E},               // e, E, ENQ
  {ACTION_SEND | ACTION_PRINT,                      '#',   &WW_PRINT_POUND},           // 3, #
  {ACTION_SEND | ACTION_PRINT,                      'D',   &WW_PRINT_D},               // d, D, EOT
  {ACTION_SEND | ACTION_PRINT,                      'B',   &WW_PRINT_B},               // b, B, STX
  {ACTION_SEND | ACTION_PRINT,                      'V',   &WW_PRINT_V},               // v, V, SYN
  {ACTION_SEND | ACTION_PRINT,                      'T',   &WW_PRINT_T},               // t, T, DC4
  {ACTION_SEND | ACTION_PRINT,                      'R',   &WW_PRINT_R},               // r, R, DC2
  {ACTION_SEND | ACTION_PRINT,                      '$',   &WW_PRINT_DOLLAR},          // 4, $
  {ACTION_SEND | ACTION_PRINT,                      '%',   &WW_PRINT_PERCENT},         // 5, %
  {ACTION_SEND | ACTION_PRINT,                      'F',   &WW_PRINT_F},               // f, F, ACK
  {ACTION_SEND | ACTION_PRINT,                      'G',   &WW_PRINT_G},               // g, G, BEL
  {ACTION_SEND | ACTION_PRINT,                      'N',   &WW_PRINT_N},               // n, N, SO
  {ACTION_SEND | ACTION_PRINT,                      'M',   &WW_PRINT_M},               // m, M, CR
  {ACTION_SEND | ACTION_PRINT,                      'Y',   &WW_PRINT_Y},               // y, Y, EM
  {ACTION_SEND | ACTION_PRINT,                      'U',   &WW_PRINT_U},               // u, U, NAK
  {ACTION_SEND | ACTION_PRINT,                      '&',   &WW_PRINT_AMPERSAND},       // 7, &
  {ACTION_SEND | ACTION_PRINT,                      '^',   &ASCII_PRINT_CARET},        // 6, ^, RS
  {ACTION_SEND | ACTION_PRINT,                      'J',   &WW_PRINT_J},               // j, J, LF
  {ACTION_SEND | ACTION_PRINT,                      'H',   &WW_PRINT_H},               // h, H, BS
  {ACTION_SEND | ACTION_PRINT,                      '<',   &ASCII_PRINT_LESS},         // ,, <
  {ACTION_SEND | ACTION_PRINT,                      '}',   &ASCII_PRINT_RBRACE},       // ], }, GS
  {ACTION_SEND | ACTION_PRINT,                      'I',   &WW_PRINT_I},               // i, I, TAB
  {ACTION_SEND | ACTION_PRINT,                      '*',   &WW_PRINT_ASTERISK},        // 8, *
  {ACTION_SEND | ACTION_PRINT,                      '+',   &WW_PRINT_PLUS},            // =, +
  {ACTION_SEND | ACTION_PRINT,                      'K',   &WW_PRINT_K},               // k, K, VT
  {ACTION_SEND | ACTION_PRINT,                      '>',   &ASCII_PRINT_GREATER},      // ., >
  {ACTION_SEND | ACTION_PRINT,                      'O',   &WW_PRINT_O},               // o, O, SI
  {ACTION_SEND | ACTION_PRINT,                      '(',   &WW_PRINT_LPAREN},          // 9, (
  {ACTION_SEND | ACTION_PRINT,                      'L',   &WW_PRINT_L},               // l, L, FF
  {ACTION_SEND | ACTION_PRINT,                      '?',   &WW_PRINT_QUESTION},        // /, ?
  {ACTION_SEND | ACTION_PRINT,                      '{',   &ASCII_PRINT_LBRACE},       // [, {, ESC
  {ACTION_SEND | ACTION_PRINT,                      'P',   &WW_PRINT_P},               // p, P, DLE
  {ACTION_SEND | ACTION_PRINT,                      ')',   &WW_PRINT_RPAREN},          // 0, )
  {ACTION_SEND | ACTION_PRINT,                      '_',   &WW_PRINT_UNDERSCORE},      // -, _, US
  {ACTION_SEND | ACTION_PRINT,                      ':',   &WW_PRINT_COLON},           // ;, :
  {ACTION_SEND | ACTION_PRINT,                      '"',   &WW_PRINT_QUOTE},           // ', "
  {ACTION_PRINT,                                    0,     &WW_PRINT_LARROW},          // <left arrow>
  {ACTION_PRINT,                                    0,     &WW_PRINT_UARROW},          // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      0x08,  &WW_PRINT_Backspace},       // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_ASCII_RETURN,                             0,     NULL},                      // <return>
  {ACTION_SEND,                                     0x7f,  NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                      // <right margin>
  {ACTION_SEND | ACTION_PRINT,                      0x09,  &WW_PRINT_Tab},             // <tab>
  {ACTION_SEND,                                     0x1b,  NULL},                      // <escape>
  {ACTION_NONE,                                     0,     NULL},                      // <tab set>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000

  // Control.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_ASCII_SETUP,                              0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // <down arrow>
  {ACTION_SEND,                                     0x1a,  NULL},                      // z, Z, SUB
  {ACTION_SEND,                                     0x11,  NULL},                      // q, Q, DC1/XON
  {ACTION_NONE,                                     0,     NULL},                      // 1, !
  {ACTION_NONE,                                     0,     NULL},                      // `, ~
  {ACTION_SEND,                                     0x01,  NULL},                      // a, A, SOH
  {ACTION_NONE,                                     0,     NULL},                      // <space>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                    0,     &WW_PRINT_LMar},            // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                    0,     &WW_PRINT_TClr},            // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_SEND,                                     0x18,  NULL},                      // x, X, CAN
  {ACTION_SEND,                                     0x17,  NULL},                      // w, W, ETB
  {ACTION_SEND,                                     0x00,  NULL},                      // 2, @, NUL
  {ACTION_SEND,                                     0x13,  NULL},                      // s, S, DC3/XOFF
  {ACTION_SEND,                                     0x03,  NULL},                      // c, C, ETX
  {ACTION_SEND,                                     0x05,  NULL},                      // e, E, ENQ
  {ACTION_NONE,                                     0,     NULL},                      // 3, #
  {ACTION_SEND,                                     0x04,  NULL},                      // d, D, EOT
  {ACTION_SEND,                                     0x02,  NULL},                      // b, B, STX
  {ACTION_SEND,                                     0x16,  NULL},                      // v, V, SYN
  {ACTION_SEND,                                     0x14,  NULL},                      // t, T, DC4
  {ACTION_SEND,                                     0x12,  NULL},                      // r, R, DC2
  {ACTION_NONE,                                     0,     NULL},                      // 4, $
  {ACTION_NONE,                                     0,     NULL},                      // 5, %
  {ACTION_SEND,                                     0x06,  NULL},                      // f, F, ACK
  {ACTION_SEND | ACTION_PRINT,                      0x07,  &WW_PRINT_BEEP},            // g, G, BEL
  {ACTION_SEND,                                     0x0e,  NULL},                      // n, N, SO
  {ACTION_SEND | ACTION_PRINT,                      0x0d,  &ASCII_PRINT_CR},           // m, M, CR
  {ACTION_SEND,                                     0x19,  NULL},                      // y, Y, EM
  {ACTION_SEND,                                     0x15,  NULL},                      // u, U, NAK
  {ACTION_NONE,                                     0,     NULL},                      // 7, &
  {ACTION_SEND,                                     0x1e,  NULL},                      // 6, ^, RS
  {ACTION_SEND | ACTION_PRINT,                      0x0a,  &ASCII_PRINT_LF},           // j, J, LF
  {ACTION_SEND | ACTION_PRINT,                      0x08,  &WW_PRINT_Backspace},       // h, H, BS
  {ACTION_NONE,                                     0,     NULL},                      // ,, <
  {ACTION_SEND,                                     0x1d,  NULL},                      // ], }, GS
  {ACTION_SEND | ACTION_PRINT,                      0x09,  &WW_PRINT_Tab},             // i, I, TAB
  {ACTION_NONE,                                     0,     NULL},                      // 8, *
  {ACTION_NONE,                                     0,     NULL},                      // =, +
  {ACTION_SEND,                                     0x0b,  NULL},                      // k, K, VT
  {ACTION_NONE,                                     0,     NULL},                      // ., >
  {ACTION_SEND,                                     0x0f,  NULL},                      // o, O, SI
  {ACTION_NONE,                                     0,     NULL},                      // 9, (
  {ACTION_SEND,                                     0x0c,  NULL},                      // l, L, FF
  {ACTION_NONE,                                     0,     NULL},                      // /, ?
  {ACTION_SEND,                                     0x1b,  NULL},                      // [, {, ESC
  {ACTION_SEND,                                     0x10,  NULL},                      // p, P, DLE
  {ACTION_NONE,                                     0,     NULL},                      // 0, )
  {ACTION_SEND,                                     0x1f,  NULL},                      // -, _, US
  {ACTION_NONE,                                     0,     NULL},                      // ;, :
  {ACTION_NONE,                                     0,     NULL},                      // ', "
  {ACTION_NONE,                                     0,     NULL},                      // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                      // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <return>
  {ACTION_NONE,                                     0,     NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_PRINT,                                    0,     &WW_PRINT_RMar},            // <right margin>
  {ACTION_NONE,                                     0,     NULL},                      // <tab>
  {ACTION_PRINT,                                    0,     &WW_PRINT_MarRel},          // <margin release>
  {ACTION_PRINT,                                    0,     &WW_PRINT_TSet},            // <tab set>
  {ACTION_NONE,                                     0,     NULL}                       // *** not available on WW1000
};

// Half duplex uppercase key action table.
const struct key_action ASCII_ACTIONS_HALF_UPPERCASE[3 * NUM_WW_KEYS] = { 
  // Unshifted.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_PRINT,                                    0,     &WW_PRINT_RARROW},          // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      '\\',  &ASCII_PRINT_BSLASH},       // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_PRINT,                                    0,     &WW_PRINT_DARROW},          // <down arrow>
  {ACTION_SEND | ACTION_PRINT,                      'Z',   &WW_PRINT_Z},               // z, Z, SUB
  {ACTION_SEND | ACTION_PRINT,                      'Q',   &WW_PRINT_Q},               // q, Q, DC1/XON
  {ACTION_SEND | ACTION_PRINT,                      '1',   &WW_PRINT_1},               // 1, !
  {ACTION_SEND | ACTION_PRINT,                      '`',   &ASCII_PRINT_BAPOSTROPHE},  // `, ~
  {ACTION_SEND | ACTION_PRINT,                      'A',   &WW_PRINT_A},               // a, A, SOH
  {ACTION_SEND | ACTION_PRINT,                      ' ',   &WW_PRINT_SPACE},           // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},       // <load paper>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_SEND | ACTION_PRINT,                      'X',   &WW_PRINT_X},               // x, X, CAN
  {ACTION_SEND | ACTION_PRINT,                      'W',   &WW_PRINT_W},               // w, W, ETB
  {ACTION_SEND | ACTION_PRINT,                      '2',   &WW_PRINT_2},               // 2, @, NUL
  {ACTION_SEND | ACTION_PRINT,                      'S',   &WW_PRINT_S},               // s, S, DC3/XOFF
  {ACTION_SEND | ACTION_PRINT,                      'C',   &WW_PRINT_C},               // c, C, ETX
  {ACTION_SEND | ACTION_PRINT,                      'E',   &WW_PRINT_E},               // e, E, ENQ
  {ACTION_SEND | ACTION_PRINT,                      '3',   &WW_PRINT_3},               // 3, #
  {ACTION_SEND | ACTION_PRINT,                      'D',   &WW_PRINT_D},               // d, D, EOT
  {ACTION_SEND | ACTION_PRINT,                      'B',   &WW_PRINT_B},               // b, B, STX
  {ACTION_SEND | ACTION_PRINT,                      'V',   &WW_PRINT_V},               // v, V, SYN
  {ACTION_SEND | ACTION_PRINT,                      'T',   &WW_PRINT_T},               // t, T, DC4
  {ACTION_SEND | ACTION_PRINT,                      'R',   &WW_PRINT_R},               // r, R, DC2
  {ACTION_SEND | ACTION_PRINT,                      '4',   &WW_PRINT_4},               // 4, $
  {ACTION_SEND | ACTION_PRINT,                      '5',   &WW_PRINT_5},               // 5, %
  {ACTION_SEND | ACTION_PRINT,                      'F',   &WW_PRINT_F},               // f, F, ACK
  {ACTION_SEND | ACTION_PRINT,                      'G',   &WW_PRINT_G},               // g, G, BEL
  {ACTION_SEND | ACTION_PRINT,                      'N',   &WW_PRINT_N},               // n, N, SO
  {ACTION_SEND | ACTION_PRINT,                      'M',   &WW_PRINT_M},               // m, M, CR
  {ACTION_SEND | ACTION_PRINT,                      'Y',   &WW_PRINT_Y},               // y, Y, EM
  {ACTION_SEND | ACTION_PRINT,                      'U',   &WW_PRINT_U},               // u, U, NAK
  {ACTION_SEND | ACTION_PRINT,                      '7',   &WW_PRINT_7},               // 7, &
  {ACTION_SEND | ACTION_PRINT,                      '6',   &WW_PRINT_6},               // 6, ^, RS
  {ACTION_SEND | ACTION_PRINT,                      'J',   &WW_PRINT_J},               // j, J, LF
  {ACTION_SEND | ACTION_PRINT,                      'H',   &WW_PRINT_H},               // h, H, BS
  {ACTION_SEND | ACTION_PRINT,                      ',',   &WW_PRINT_COMMA},           // ,, <
  {ACTION_SEND | ACTION_PRINT,                      ']',   &WW_PRINT_RBRACKET},        // ], }, GS
  {ACTION_SEND | ACTION_PRINT,                      'I',   &WW_PRINT_I},               // i, I, TAB
  {ACTION_SEND | ACTION_PRINT,                      '8',   &WW_PRINT_8},               // 8, *
  {ACTION_SEND | ACTION_PRINT,                      '=',   &WW_PRINT_EQUAL},           // =, +
  {ACTION_SEND | ACTION_PRINT,                      'K',   &WW_PRINT_K},               // k, K, VT
  {ACTION_SEND | ACTION_PRINT,                      '.',   &WW_PRINT_PERIOD},          // ., >
  {ACTION_SEND | ACTION_PRINT,                      'O',   &WW_PRINT_O},               // o, O, SI
  {ACTION_SEND | ACTION_PRINT,                      '9',   &WW_PRINT_9},               // 9, (
  {ACTION_SEND | ACTION_PRINT,                      'L',   &WW_PRINT_L},               // l, L, FF
  {ACTION_SEND | ACTION_PRINT,                      '/',   &WW_PRINT_SLASH},           // /, ?
  {ACTION_SEND | ACTION_PRINT,                      '[',   &WW_PRINT_LBRACKET},        // [, {, ESC
  {ACTION_SEND | ACTION_PRINT,                      'P',   &WW_PRINT_P},               // p, P, DLE
  {ACTION_SEND | ACTION_PRINT,                      '0',   &WW_PRINT_0},               // 0, )
  {ACTION_SEND | ACTION_PRINT,                      '-',   &WW_PRINT_HYPHEN},          // -, _, US
  {ACTION_SEND | ACTION_PRINT,                      ';',   &WW_PRINT_SEMICOLON},       // ;, :
  {ACTION_SEND | ACTION_PRINT,                      '\'',  &WW_PRINT_APOSTROPHE},      // ', "
  {ACTION_PRINT,                                    0,     &WW_PRINT_LARROW},          // <left arrow>
  {ACTION_PRINT,                                    0,     &WW_PRINT_UARROW},          // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      0x08,  &WW_PRINT_Backspace},       // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_ASCII_RETURN,                             0,     NULL},                      // <return>
  {ACTION_SEND,                                     0x7f,  NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                      // <right margin>
  {ACTION_SEND | ACTION_PRINT,                      0x09,  &WW_PRINT_Tab},             // <tab>
  {ACTION_SEND,                                     0x1b,  NULL},                      // <escape>
  {ACTION_NONE,                                     0,     NULL},                      // <tab set>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_PRINT,                                    0,     &WW_PRINT_RARROW},          // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      '|',   &ASCII_PRINT_BAR},          // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_PRINT,                                    0,     &WW_PRINT_DARROW},          // <down arrow>
  {ACTION_SEND | ACTION_PRINT,                      'Z',   &WW_PRINT_Z},               // z, Z, SUB
  {ACTION_SEND | ACTION_PRINT,                      'Q',   &WW_PRINT_Q},               // q, Q, DC1/XON
  {ACTION_SEND | ACTION_PRINT,                      '!',   &WW_PRINT_EXCLAMATION},     // 1, !
  {ACTION_SEND | ACTION_PRINT,                      '~',   &ASCII_PRINT_TILDE},        // `, ~
  {ACTION_SEND | ACTION_PRINT,                      'A',   &WW_PRINT_A},               // a, A, SOH
  {ACTION_SEND | ACTION_PRINT,                      ' ',   &WW_PRINT_SPACE},           // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},       // <load paper>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_SEND | ACTION_PRINT,                      'X',   &WW_PRINT_X},               // x, X, CAN
  {ACTION_SEND | ACTION_PRINT,                      'W',   &WW_PRINT_W},               // w, W, ETB
  {ACTION_SEND | ACTION_PRINT,                      '@',   &WW_PRINT_AT},              // 2, @, NUL
  {ACTION_SEND | ACTION_PRINT,                      'S',   &WW_PRINT_S},               // s, S, DC3/XOFF
  {ACTION_SEND | ACTION_PRINT,                      'C',   &WW_PRINT_C},               // c, C, ETX
  {ACTION_SEND | ACTION_PRINT,                      'E',   &WW_PRINT_E},               // e, E, ENQ
  {ACTION_SEND | ACTION_PRINT,                      '#',   &WW_PRINT_POUND},           // 3, #
  {ACTION_SEND | ACTION_PRINT,                      'D',   &WW_PRINT_D},               // d, D, EOT
  {ACTION_SEND | ACTION_PRINT,                      'B',   &WW_PRINT_B},               // b, B, STX
  {ACTION_SEND | ACTION_PRINT,                      'V',   &WW_PRINT_V},               // v, V, SYN
  {ACTION_SEND | ACTION_PRINT,                      'T',   &WW_PRINT_T},               // t, T, DC4
  {ACTION_SEND | ACTION_PRINT,                      'R',   &WW_PRINT_R},               // r, R, DC2
  {ACTION_SEND | ACTION_PRINT,                      '$',   &WW_PRINT_DOLLAR},          // 4, $
  {ACTION_SEND | ACTION_PRINT,                      '%',   &WW_PRINT_PERCENT},         // 5, %
  {ACTION_SEND | ACTION_PRINT,                      'F',   &WW_PRINT_F},               // f, F, ACK
  {ACTION_SEND | ACTION_PRINT,                      'G',   &WW_PRINT_G},               // g, G, BEL
  {ACTION_SEND | ACTION_PRINT,                      'N',   &WW_PRINT_N},               // n, N, SO
  {ACTION_SEND | ACTION_PRINT,                      'M',   &WW_PRINT_M},               // m, M, CR
  {ACTION_SEND | ACTION_PRINT,                      'Y',   &WW_PRINT_Y},               // y, Y, EM
  {ACTION_SEND | ACTION_PRINT,                      'U',   &WW_PRINT_U},               // u, U, NAK
  {ACTION_SEND | ACTION_PRINT,                      '&',   &WW_PRINT_AMPERSAND},       // 7, &
  {ACTION_SEND | ACTION_PRINT,                      '^',   &ASCII_PRINT_CARET},        // 6, ^, RS
  {ACTION_SEND | ACTION_PRINT,                      'J',   &WW_PRINT_J},               // j, J, LF
  {ACTION_SEND | ACTION_PRINT,                      'H',   &WW_PRINT_H},               // h, H, BS
  {ACTION_SEND | ACTION_PRINT,                      '<',   &ASCII_PRINT_LESS},         // ,, <
  {ACTION_SEND | ACTION_PRINT,                      '}',   &ASCII_PRINT_RBRACE},       // ], }, GS
  {ACTION_SEND | ACTION_PRINT,                      'I',   &WW_PRINT_I},               // i, I, TAB
  {ACTION_SEND | ACTION_PRINT,                      '*',   &WW_PRINT_ASTERISK},        // 8, *
  {ACTION_SEND | ACTION_PRINT,                      '+',   &WW_PRINT_PLUS},            // =, +
  {ACTION_SEND | ACTION_PRINT,                      'K',   &WW_PRINT_K},               // k, K, VT
  {ACTION_SEND | ACTION_PRINT,                      '>',   &ASCII_PRINT_GREATER},      // ., >
  {ACTION_SEND | ACTION_PRINT,                      'O',   &WW_PRINT_O},               // o, O, SI
  {ACTION_SEND | ACTION_PRINT,                      '(',   &WW_PRINT_LPAREN},          // 9, (
  {ACTION_SEND | ACTION_PRINT,                      'L',   &WW_PRINT_L},               // l, L, FF
  {ACTION_SEND | ACTION_PRINT,                      '?',   &WW_PRINT_QUESTION},        // /, ?
  {ACTION_SEND | ACTION_PRINT,                      '{',   &ASCII_PRINT_LBRACE},       // [, {, ESC
  {ACTION_SEND | ACTION_PRINT,                      'P',   &WW_PRINT_P},               // p, P, DLE
  {ACTION_SEND | ACTION_PRINT,                      ')',   &WW_PRINT_RPAREN},          // 0, )
  {ACTION_SEND | ACTION_PRINT,                      '_',   &WW_PRINT_UNDERSCORE},      // -, _, US
  {ACTION_SEND | ACTION_PRINT,                      ':',   &WW_PRINT_COLON},           // ;, :
  {ACTION_SEND | ACTION_PRINT,                      '"',   &WW_PRINT_QUOTE},           // ', "
  {ACTION_PRINT,                                    0,     &WW_PRINT_LARROW},          // <left arrow>
  {ACTION_PRINT,                                    0,     &WW_PRINT_UARROW},          // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                      0x08,  &WW_PRINT_Backspace},       // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_ASCII_RETURN,                             0,     NULL},                      // <return>
  {ACTION_SEND,                                     0x7f,  NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                      // <right margin>
  {ACTION_SEND | ACTION_PRINT,                      0x09,  &WW_PRINT_Tab},             // <tab>
  {ACTION_SEND,                                     0x1b,  NULL},                      // <escape>
  {ACTION_NONE,                                     0,     NULL},                      // <tab set>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000

  // Control.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_ASCII_SETUP,                              0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // <down arrow>
  {ACTION_SEND,                                     0x1a,  NULL},                      // z, Z, SUB
  {ACTION_SEND,                                     0x11,  NULL},                      // q, Q, DC1/XON
  {ACTION_NONE,                                     0,     NULL},                      // 1, !
  {ACTION_NONE,                                     0,     NULL},                      // `, ~
  {ACTION_SEND,                                     0x01,  NULL},                      // a, A, SOH
  {ACTION_NONE,                                     0,     NULL},                      // <space>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                    0,     &WW_PRINT_LMar},            // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                    0,     &WW_PRINT_TClr},            // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_SEND,                                     0x18,  NULL},                      // x, X, CAN
  {ACTION_SEND,                                     0x17,  NULL},                      // w, W, ETB
  {ACTION_SEND,                                     0x00,  NULL},                      // 2, @, NUL
  {ACTION_SEND,                                     0x13,  NULL},                      // s, S, DC3/XOFF
  {ACTION_SEND,                                     0x03,  NULL},                      // c, C, ETX
  {ACTION_SEND,                                     0x05,  NULL},                      // e, E, ENQ
  {ACTION_NONE,                                     0,     NULL},                      // 3, #
  {ACTION_SEND,                                     0x04,  NULL},                      // d, D, EOT
  {ACTION_SEND,                                     0x02,  NULL},                      // b, B, STX
  {ACTION_SEND,                                     0x16,  NULL},                      // v, V, SYN
  {ACTION_SEND,                                     0x14,  NULL},                      // t, T, DC4
  {ACTION_SEND,                                     0x12,  NULL},                      // r, R, DC2
  {ACTION_NONE,                                     0,     NULL},                      // 4, $
  {ACTION_NONE,                                     0,     NULL},                      // 5, %
  {ACTION_SEND,                                     0x06,  NULL},                      // f, F, ACK
  {ACTION_SEND | ACTION_PRINT,                      0x07,  &WW_PRINT_BEEP},            // g, G, BEL
  {ACTION_SEND,                                     0x0e,  NULL},                      // n, N, SO
  {ACTION_SEND | ACTION_PRINT,                      0x0d,  &ASCII_PRINT_CR},           // m, M, CR
  {ACTION_SEND,                                     0x19,  NULL},                      // y, Y, EM
  {ACTION_SEND,                                     0x15,  NULL},                      // u, U, NAK
  {ACTION_NONE,                                     0,     NULL},                      // 7, &
  {ACTION_SEND,                                     0x1e,  NULL},                      // 6, ^, RS
  {ACTION_SEND | ACTION_PRINT,                      0x0a,  &ASCII_PRINT_LF},           // j, J, LF
  {ACTION_SEND | ACTION_PRINT,                      0x08,  &WW_PRINT_Backspace},       // h, H, BS
  {ACTION_NONE,                                     0,     NULL},                      // ,, <
  {ACTION_SEND,                                     0x1d,  NULL},                      // ], }, GS
  {ACTION_SEND | ACTION_PRINT,                      0x09,  &WW_PRINT_Tab},             // i, I, TAB
  {ACTION_NONE,                                     0,     NULL},                      // 8, *
  {ACTION_NONE,                                     0,     NULL},                      // =, +
  {ACTION_SEND,                                     0x0b,  NULL},                      // k, K, VT
  {ACTION_NONE,                                     0,     NULL},                      // ., >
  {ACTION_SEND,                                     0x0f,  NULL},                      // o, O, SI
  {ACTION_NONE,                                     0,     NULL},                      // 9, (
  {ACTION_SEND,                                     0x0c,  NULL},                      // l, L, FF
  {ACTION_NONE,                                     0,     NULL},                      // /, ?
  {ACTION_SEND,                                     0x1b,  NULL},                      // [, {, ESC
  {ACTION_SEND,                                     0x10,  NULL},                      // p, P, DLE
  {ACTION_NONE,                                     0,     NULL},                      // 0, )
  {ACTION_SEND,                                     0x1f,  NULL},                      // -, _, US
  {ACTION_NONE,                                     0,     NULL},                      // ;, :
  {ACTION_NONE,                                     0,     NULL},                      // ', "
  {ACTION_NONE,                                     0,     NULL},                      // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                      // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <return>
  {ACTION_NONE,                                     0,     NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_PRINT,                                    0,     &WW_PRINT_RMar},            // <right margin>
  {ACTION_NONE,                                     0,     NULL},                      // <tab>
  {ACTION_PRINT,                                    0,     &WW_PRINT_MarRel},          // <margin release>
  {ACTION_PRINT,                                    0,     &WW_PRINT_TSet},            // <tab set>
  {ACTION_NONE,                                     0,     NULL}                       // *** not available on WW1000
};

// Full duplex key action table.
const struct key_action ASCII_ACTIONS_FULL[3 * NUM_WW_KEYS] = { 
  // Unshifted.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_PRINT,                                    0,     &WW_PRINT_RARROW},          // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_SEND,                                     '\\',  NULL},                      // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_PRINT,                                    0,     &WW_PRINT_DARROW},          // <down arrow>
  {ACTION_SEND,                                     'z',   NULL},                      // z, Z, SUB
  {ACTION_SEND,                                     'q',   NULL},                      // q, Q, DC1/XON
  {ACTION_SEND,                                     '1',   NULL},                      // 1, !
  {ACTION_SEND,                                     '`',   NULL},                      // `, ~
  {ACTION_SEND,                                     'a',   NULL},                      // a, A, SOH
  {ACTION_SEND,                                     ' ',   NULL},                      // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},       // <load paper>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_SEND,                                     'x',   NULL},                      // x, X, CAN
  {ACTION_SEND,                                     'w',   NULL},                      // w, W, ETB
  {ACTION_SEND,                                     '2',   NULL},                      // 2, @, NUL
  {ACTION_SEND,                                     's',   NULL},                      // s, S, DC3/XOFF
  {ACTION_SEND,                                     'c',   NULL},                      // c, C, ETX
  {ACTION_SEND,                                     'e',   NULL},                      // e, E, ENQ
  {ACTION_SEND,                                     '3',   NULL},                      // 3, #
  {ACTION_SEND,                                     'd',   NULL},                      // d, D, EOT
  {ACTION_SEND,                                     'b',   NULL},                      // b, B, STX
  {ACTION_SEND,                                     'v',   NULL},                      // v, V, SYN
  {ACTION_SEND,                                     't',   NULL},                      // t, T, DC4
  {ACTION_SEND,                                     'r',   NULL},                      // r, R, DC2
  {ACTION_SEND,                                     '4',   NULL},                      // 4, $
  {ACTION_SEND,                                     '5',   NULL},                      // 5, %
  {ACTION_SEND,                                     'f',   NULL},                      // f, F, ACK
  {ACTION_SEND,                                     'g',   NULL},                      // g, G, BEL
  {ACTION_SEND,                                     'n',   NULL},                      // n, N, SO
  {ACTION_SEND,                                     'm',   NULL},                      // m, M, CR
  {ACTION_SEND,                                     'y',   NULL},                      // y, Y, EM
  {ACTION_SEND,                                     'u',   NULL},                      // u, U, NAK
  {ACTION_SEND,                                     '7',   NULL},                      // 7, &
  {ACTION_SEND,                                     '6',   NULL},                      // 6, ^, RS
  {ACTION_SEND,                                     'j',   NULL},                      // j, J, LF
  {ACTION_SEND,                                     'h',   NULL},                      // h, H, BS
  {ACTION_SEND,                                     ',',   NULL},                      // ,, <
  {ACTION_SEND,                                     ']',   NULL},                      // ], }, GS
  {ACTION_SEND,                                     'i',   NULL},                      // i, I, TAB
  {ACTION_SEND,                                     '8',   NULL},                      // 8, *
  {ACTION_SEND,                                     '=',   NULL},                      // =, +
  {ACTION_SEND,                                     'k',   NULL},                      // k, K, VT
  {ACTION_SEND,                                     '.',   NULL},                      // ., >
  {ACTION_SEND,                                     'o',   NULL},                      // o, O, SI
  {ACTION_SEND,                                     '9',   NULL},                      // 9, (
  {ACTION_SEND,                                     'l',   NULL},                      // l, L, FF
  {ACTION_SEND,                                     '/',   NULL},                      // /, ?
  {ACTION_SEND,                                     '[',   NULL},                      // [, {, ESC
  {ACTION_SEND,                                     'p',   NULL},                      // p, P, DLE
  {ACTION_SEND,                                     '0',   NULL},                      // 0, )
  {ACTION_SEND,                                     '-',   NULL},                      // -, _, US
  {ACTION_SEND,                                     ';',   NULL},                      // ;, :
  {ACTION_SEND,                                     '\'',  NULL},                      // ', "
  {ACTION_PRINT,                                    0,     &WW_PRINT_LARROW},          // <left arrow>
  {ACTION_PRINT,                                    0,     &WW_PRINT_UARROW},          // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_SEND,                                     0x08,  NULL},                      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_ASCII_RETURN,                             0,     NULL},                      // <return>
  {ACTION_SEND,                                     0x7f,  NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                      // <right margin>
  {ACTION_SEND,                                     0x09,  NULL},                      // <tab>
  {ACTION_SEND,                                     0x1b,  NULL},                      // <escape>
  {ACTION_NONE,                                     0,     NULL},                      // <tab set>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_PRINT,                                    0,     &WW_PRINT_RARROW},          // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_SEND,                                     '|',   NULL},                      // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_PRINT,                                    0,     &WW_PRINT_DARROW},          // <down arrow>
  {ACTION_SEND,                                     'Z',   NULL},                      // z, Z, SUB
  {ACTION_SEND,                                     'Q',   NULL},                      // q, Q, DC1/XON
  {ACTION_SEND,                                     '!',   NULL},                      // 1, !
  {ACTION_SEND,                                     '~',   NULL},                      // `, ~
  {ACTION_SEND,                                     'A',   NULL},                      // a, A, SOH
  {ACTION_SEND,                                     ' ',   NULL},                      // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},       // <load paper>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_SEND,                                     'X',   NULL},                      // x, X, CAN
  {ACTION_SEND,                                     'W',   NULL},                      // w, W, ETB
  {ACTION_SEND,                                     '@',   NULL},                      // 2, @, NUL
  {ACTION_SEND,                                     'S',   NULL},                      // s, S, DC3/XOFF
  {ACTION_SEND,                                     'C',   NULL},                      // c, C, ETX
  {ACTION_SEND,                                     'E',   NULL},                      // e, E, ENQ
  {ACTION_SEND,                                     '#',   NULL},                      // 3, #
  {ACTION_SEND,                                     'D',   NULL},                      // d, D, EOT
  {ACTION_SEND,                                     'B',   NULL},                      // b, B, STX
  {ACTION_SEND,                                     'V',   NULL},                      // v, V, SYN
  {ACTION_SEND,                                     'T',   NULL},                      // t, T, DC4
  {ACTION_SEND,                                     'R',   NULL},                      // r, R, DC2
  {ACTION_SEND,                                     '$',   NULL},                      // 4, $
  {ACTION_SEND,                                     '%',   NULL},                      // 5, %
  {ACTION_SEND,                                     'F',   NULL},                      // f, F, ACK
  {ACTION_SEND,                                     'G',   NULL},                      // g, G, BEL
  {ACTION_SEND,                                     'N',   NULL},                      // n, N, SO
  {ACTION_SEND,                                     'M',   NULL},                      // m, M, CR
  {ACTION_SEND,                                     'Y',   NULL},                      // y, Y, EM
  {ACTION_SEND,                                     'U',   NULL},                      // u, U, NAK
  {ACTION_SEND,                                     '&',   NULL},                      // 7, &
  {ACTION_SEND,                                     '^',   NULL},                      // 6, ^, RS
  {ACTION_SEND,                                     'J',   NULL},                      // j, J, LF
  {ACTION_SEND,                                     'H',   NULL},                      // h, H, BS
  {ACTION_SEND,                                     '<',   NULL},                      // ,, <
  {ACTION_SEND,                                     '}',   NULL},                      // ], }, GS
  {ACTION_SEND,                                     'I',   NULL},                      // i, I, TAB
  {ACTION_SEND,                                     '*',   NULL},                      // 8, *
  {ACTION_SEND,                                     '+',   NULL},                      // =, +
  {ACTION_SEND,                                     'K',   NULL},                      // k, K, VT
  {ACTION_SEND,                                     '>',   NULL},                      // ., >
  {ACTION_SEND,                                     'O',   NULL},                      // o, O, SI
  {ACTION_SEND,                                     '(',   NULL},                      // 9, (
  {ACTION_SEND,                                     'L',   NULL},                      // l, L, FF
  {ACTION_SEND,                                     '?',   NULL},                      // /, ?
  {ACTION_SEND,                                     '{',   NULL},                      // [, {, ESC
  {ACTION_SEND,                                     'P',   NULL},                      // p, P, DLE
  {ACTION_SEND,                                     ')',   NULL},                      // 0, )
  {ACTION_SEND,                                     '_',   NULL},                      // -, _, US
  {ACTION_SEND,                                     ':',   NULL},                      // ;, :
  {ACTION_SEND,                                     '"',   NULL},                      // ', "
  {ACTION_PRINT,                                    0,     &WW_PRINT_LARROW},          // <left arrow>
  {ACTION_PRINT,                                    0,     &WW_PRINT_UARROW},          // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_SEND,                                     0x08,  NULL},                      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_ASCII_RETURN,                             0,     NULL},                      // <return>
  {ACTION_SEND,                                     0x7f,  NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                      // <right margin>
  {ACTION_SEND,                                     0x09,  NULL},                      // <tab>
  {ACTION_SEND,                                     0x1b,  NULL},                      // <escape>
  {ACTION_NONE,                                     0,     NULL},                      // <tab set>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000

  // Control.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_ASCII_SETUP,                              0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // <down arrow>
  {ACTION_SEND,                                     0x1a,  NULL},                      // z, Z, SUB
  {ACTION_SEND,                                     0x11,  NULL},                      // q, Q, DC1/XON
  {ACTION_NONE,                                     0,     NULL},                      // 1, !
  {ACTION_NONE,                                     0,     NULL},                      // `, ~
  {ACTION_SEND,                                     0x01,  NULL},                      // a, A, SOH
  {ACTION_NONE,                                     0,     NULL},                      // <space>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                    0,     &WW_PRINT_LMar},            // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                    0,     &WW_PRINT_TClr},            // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_SEND,                                     0x18,  NULL},                      // x, X, CAN
  {ACTION_SEND,                                     0x17,  NULL},                      // w, W, ETB
  {ACTION_SEND,                                     0x00,  NULL},                      // 2, @, NUL
  {ACTION_SEND,                                     0x13,  NULL},                      // s, S, DC3/XOFF
  {ACTION_SEND,                                     0x03,  NULL},                      // c, C, ETX
  {ACTION_SEND,                                     0x05,  NULL},                      // e, E, ENQ
  {ACTION_NONE,                                     0,     NULL},                      // 3, #
  {ACTION_SEND,                                     0x04,  NULL},                      // d, D, EOT
  {ACTION_SEND,                                     0x02,  NULL},                      // b, B, STX
  {ACTION_SEND,                                     0x16,  NULL},                      // v, V, SYN
  {ACTION_SEND,                                     0x14,  NULL},                      // t, T, DC4
  {ACTION_SEND,                                     0x12,  NULL},                      // r, R, DC2
  {ACTION_NONE,                                     0,     NULL},                      // 4, $
  {ACTION_NONE,                                     0,     NULL},                      // 5, %
  {ACTION_SEND,                                     0x06,  NULL},                      // f, F, ACK
  {ACTION_SEND,                                     0x07,  NULL},                      // g, G, BEL
  {ACTION_SEND,                                     0x0e,  NULL},                      // n, N, SO
  {ACTION_SEND,                                     0x0d,  NULL},                      // m, M, CR
  {ACTION_SEND,                                     0x19,  NULL},                      // y, Y, EM
  {ACTION_SEND,                                     0x15,  NULL},                      // u, U, NAK
  {ACTION_NONE,                                     0,     NULL},                      // 7, &
  {ACTION_SEND,                                     0x1e,  NULL},                      // 6, ^, RS
  {ACTION_SEND,                                     0x0a,  NULL},                      // j, J, LF
  {ACTION_SEND,                                     0x08,  NULL},                      // h, H, BS
  {ACTION_NONE,                                     0,     NULL},                      // ,, <
  {ACTION_SEND,                                     0x1d,  NULL},                      // ], }, GS
  {ACTION_SEND,                                     0x09,  NULL},                      // i, I, TAB
  {ACTION_NONE,                                     0,     NULL},                      // 8, *
  {ACTION_NONE,                                     0,     NULL},                      // =, +
  {ACTION_SEND,                                     0x0b,  NULL},                      // k, K, VT
  {ACTION_NONE,                                     0,     NULL},                      // ., >
  {ACTION_SEND,                                     0x0f,  NULL},                      // o, O, SI
  {ACTION_NONE,                                     0,     NULL},                      // 9, (
  {ACTION_SEND,                                     0x0c,  NULL},                      // l, L, FF
  {ACTION_NONE,                                     0,     NULL},                      // /, ?
  {ACTION_SEND,                                     0x1b,  NULL},                      // [, {, ESC
  {ACTION_SEND,                                     0x10,  NULL},                      // p, P, DLE
  {ACTION_NONE,                                     0,     NULL},                      // 0, )
  {ACTION_SEND,                                     0x1f,  NULL},                      // -, _, US
  {ACTION_NONE,                                     0,     NULL},                      // ;, :
  {ACTION_NONE,                                     0,     NULL},                      // ', "
  {ACTION_NONE,                                     0,     NULL},                      // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                      // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <return>
  {ACTION_NONE,                                     0,     NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_PRINT,                                    0,     &WW_PRINT_RMar},            // <right margin>
  {ACTION_NONE,                                     0,     NULL},                      // <tab>
  {ACTION_PRINT,                                    0,     &WW_PRINT_MarRel},          // <margin release>
  {ACTION_PRINT,                                    0,     &WW_PRINT_TSet},            // <tab set>
  {ACTION_NONE,                                     0,     NULL}                       // *** not available on WW1000
};

// Full duplex uppercase key action table.
const struct key_action ASCII_ACTIONS_FULL_UPPERCASE[3 * NUM_WW_KEYS] = { 
  // Unshifted.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_PRINT,                                    0,     &WW_PRINT_RARROW},          // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_SEND,                                     '\\',  NULL},                      // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_PRINT,                                    0,     &WW_PRINT_DARROW},          // <down arrow>
  {ACTION_SEND,                                     'Z',   NULL},                      // z, Z, SUB
  {ACTION_SEND,                                     'Q',   NULL},                      // q, Q, DC1/XON
  {ACTION_SEND,                                     '1',   NULL},                      // 1, !
  {ACTION_SEND,                                     '`',   NULL},                      // `, ~
  {ACTION_SEND,                                     'A',   NULL},                      // a, A, SOH
  {ACTION_SEND,                                     ' ',   NULL},                      // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},       // <load paper>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_SEND,                                     'X',   NULL},                      // x, X, CAN
  {ACTION_SEND,                                     'W',   NULL},                      // w, W, ETB
  {ACTION_SEND,                                     '2',   NULL},                      // 2, @, NUL
  {ACTION_SEND,                                     'S',   NULL},                      // s, S, DC3/XOFF
  {ACTION_SEND,                                     'C',   NULL},                      // c, C, ETX
  {ACTION_SEND,                                     'E',   NULL},                      // e, E, ENQ
  {ACTION_SEND,                                     '3',   NULL},                      // 3, #
  {ACTION_SEND,                                     'D',   NULL},                      // d, D, EOT
  {ACTION_SEND,                                     'B',   NULL},                      // b, B, STX
  {ACTION_SEND,                                     'V',   NULL},                      // v, V, SYN
  {ACTION_SEND,                                     'T',   NULL},                      // t, T, DC4
  {ACTION_SEND,                                     'R',   NULL},                      // r, R, DC2
  {ACTION_SEND,                                     '4',   NULL},                      // 4, $
  {ACTION_SEND,                                     '5',   NULL},                      // 5, %
  {ACTION_SEND,                                     'F',   NULL},                      // f, F, ACK
  {ACTION_SEND,                                     'G',   NULL},                      // g, G, BEL
  {ACTION_SEND,                                     'N',   NULL},                      // n, N, SO
  {ACTION_SEND,                                     'M',   NULL},                      // m, M, CR
  {ACTION_SEND,                                     'Y',   NULL},                      // y, Y, EM
  {ACTION_SEND,                                     'U',   NULL},                      // u, U, NAK
  {ACTION_SEND,                                     '7',   NULL},                      // 7, &
  {ACTION_SEND,                                     '6',   NULL},                      // 6, ^, RS
  {ACTION_SEND,                                     'J',   NULL},                      // j, J, LF
  {ACTION_SEND,                                     'H',   NULL},                      // h, H, BS
  {ACTION_SEND,                                     ',',   NULL},                      // ,, <
  {ACTION_SEND,                                     ']',   NULL},                      // ], }, GS
  {ACTION_SEND,                                     'I',   NULL},                      // i, I, TAB
  {ACTION_SEND,                                     '8',   NULL},                      // 8, *
  {ACTION_SEND,                                     '=',   NULL},                      // =, +
  {ACTION_SEND,                                     'K',   NULL},                      // k, K, VT
  {ACTION_SEND,                                     '.',   NULL},                      // ., >
  {ACTION_SEND,                                     'O',   NULL},                      // o, O, SI
  {ACTION_SEND,                                     '9',   NULL},                      // 9, (
  {ACTION_SEND,                                     'L',   NULL},                      // l, L, FF
  {ACTION_SEND,                                     '/',   NULL},                      // /, ?
  {ACTION_SEND,                                     '[',   NULL},                      // [, {, ESC
  {ACTION_SEND,                                     'P',   NULL},                      // p, P, DLE
  {ACTION_SEND,                                     '0',   NULL},                      // 0, )
  {ACTION_SEND,                                     '-',   NULL},                      // -, _, US
  {ACTION_SEND,                                     ';',   NULL},                      // ;, :
  {ACTION_SEND,                                     '\'',  NULL},                      // ', "
  {ACTION_PRINT,                                    0,     &WW_PRINT_LARROW},          // <left arrow>
  {ACTION_PRINT,                                    0,     &WW_PRINT_UARROW},          // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_SEND,                                     0x08,  NULL},                      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_ASCII_RETURN,                             0,     NULL},                      // <return>
  {ACTION_SEND,                                     0x7f,  NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                      // <right margin>
  {ACTION_SEND,                                     0x09,  NULL},                      // <tab>
  {ACTION_SEND,                                     0x1b,  NULL},                      // <escape>
  {ACTION_NONE,                                     0,     NULL},                      // <tab set>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_PRINT,                                    0,     &WW_PRINT_RARROW},          // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_SEND,                                     '|',   NULL},                      // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_PRINT,                                    0,     &WW_PRINT_DARROW},          // <down arrow>
  {ACTION_SEND,                                     'Z',   NULL},                      // z, Z, SUB
  {ACTION_SEND,                                     'Q',   NULL},                      // q, Q, DC1/XON
  {ACTION_SEND,                                     '!',   NULL},                      // 1, !
  {ACTION_SEND,                                     '~',   NULL},                      // `, ~
  {ACTION_SEND,                                     'A',   NULL},                      // a, A, SOH
  {ACTION_SEND,                                     ' ',   NULL},                      // <space>
  {ACTION_PRINT,                                    0,     &WW_PRINT_LOADPAPER},       // <load paper>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_SEND,                                     'X',   NULL},                      // x, X, CAN
  {ACTION_SEND,                                     'W',   NULL},                      // w, W, ETB
  {ACTION_SEND,                                     '@',   NULL},                      // 2, @, NUL
  {ACTION_SEND,                                     'S',   NULL},                      // s, S, DC3/XOFF
  {ACTION_SEND,                                     'C',   NULL},                      // c, C, ETX
  {ACTION_SEND,                                     'E',   NULL},                      // e, E, ENQ
  {ACTION_SEND,                                     '#',   NULL},                      // 3, #
  {ACTION_SEND,                                     'D',   NULL},                      // d, D, EOT
  {ACTION_SEND,                                     'B',   NULL},                      // b, B, STX
  {ACTION_SEND,                                     'V',   NULL},                      // v, V, SYN
  {ACTION_SEND,                                     'T',   NULL},                      // t, T, DC4
  {ACTION_SEND,                                     'R',   NULL},                      // r, R, DC2
  {ACTION_SEND,                                     '$',   NULL},                      // 4, $
  {ACTION_SEND,                                     '%',   NULL},                      // 5, %
  {ACTION_SEND,                                     'F',   NULL},                      // f, F, ACK
  {ACTION_SEND,                                     'G',   NULL},                      // g, G, BEL
  {ACTION_SEND,                                     'N',   NULL},                      // n, N, SO
  {ACTION_SEND,                                     'M',   NULL},                      // m, M, CR
  {ACTION_SEND,                                     'Y',   NULL},                      // y, Y, EM
  {ACTION_SEND,                                     'U',   NULL},                      // u, U, NAK
  {ACTION_SEND,                                     '&',   NULL},                      // 7, &
  {ACTION_SEND,                                     '^',   NULL},                      // 6, ^, RS
  {ACTION_SEND,                                     'J',   NULL},                      // j, J, LF
  {ACTION_SEND,                                     'H',   NULL},                      // h, H, BS
  {ACTION_SEND,                                     '<',   NULL},                      // ,, <
  {ACTION_SEND,                                     '}',   NULL},                      // ], }, GS
  {ACTION_SEND,                                     'I',   NULL},                      // i, I, TAB
  {ACTION_SEND,                                     '*',   NULL},                      // 8, *
  {ACTION_SEND,                                     '+',   NULL},                      // =, +
  {ACTION_SEND,                                     'K',   NULL},                      // k, K, VT
  {ACTION_SEND,                                     '>',   NULL},                      // ., >
  {ACTION_SEND,                                     'O',   NULL},                      // o, O, SI
  {ACTION_SEND,                                     '(',   NULL},                      // 9, (
  {ACTION_SEND,                                     'L',   NULL},                      // l, L, FF
  {ACTION_SEND,                                     '?',   NULL},                      // /, ?
  {ACTION_SEND,                                     '{',   NULL},                      // [, {, ESC
  {ACTION_SEND,                                     'P',   NULL},                      // p, P, DLE
  {ACTION_SEND,                                     ')',   NULL},                      // 0, )
  {ACTION_SEND,                                     '_',   NULL},                      // -, _, US
  {ACTION_SEND,                                     ':',   NULL},                      // ;, :
  {ACTION_SEND,                                     '"',   NULL},                      // ', "
  {ACTION_PRINT,                                    0,     &WW_PRINT_LARROW},          // <left arrow>
  {ACTION_PRINT,                                    0,     &WW_PRINT_UARROW},          // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_SEND,                                     0x08,  NULL},                      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_ASCII_RETURN,                             0,     NULL},                      // <return>
  {ACTION_SEND,                                     0x7f,  NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                      // <right margin>
  {ACTION_SEND,                                     0x09,  NULL},                      // <tab>
  {ACTION_SEND,                                     0x1b,  NULL},                      // <escape>
  {ACTION_NONE,                                     0,     NULL},                      // <tab set>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000

  // Control.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_ASCII_SETUP,                              0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // <down arrow>
  {ACTION_SEND,                                     0x1a,  NULL},                      // z, Z, SUB
  {ACTION_SEND,                                     0x11,  NULL},                      // q, Q, DC1/XON
  {ACTION_NONE,                                     0,     NULL},                      // 1, !
  {ACTION_NONE,                                     0,     NULL},                      // `, ~
  {ACTION_SEND,                                     0x01,  NULL},                      // a, A, SOH
  {ACTION_NONE,                                     0,     NULL},                      // <space>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                    0,     &WW_PRINT_LMar},            // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                    0,     &WW_PRINT_TClr},            // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_SEND,                                     0x18,  NULL},                      // x, X, CAN
  {ACTION_SEND,                                     0x17,  NULL},                      // w, W, ETB
  {ACTION_SEND,                                     0x00,  NULL},                      // 2, @, NUL
  {ACTION_SEND,                                     0x13,  NULL},                      // s, S, DC3/XOFF
  {ACTION_SEND,                                     0x03,  NULL},                      // c, C, ETX
  {ACTION_SEND,                                     0x05,  NULL},                      // e, E, ENQ
  {ACTION_NONE,                                     0,     NULL},                      // 3, #
  {ACTION_SEND,                                     0x04,  NULL},                      // d, D, EOT
  {ACTION_SEND,                                     0x02,  NULL},                      // b, B, STX
  {ACTION_SEND,                                     0x16,  NULL},                      // v, V, SYN
  {ACTION_SEND,                                     0x14,  NULL},                      // t, T, DC4
  {ACTION_SEND,                                     0x12,  NULL},                      // r, R, DC2
  {ACTION_NONE,                                     0,     NULL},                      // 4, $
  {ACTION_NONE,                                     0,     NULL},                      // 5, %
  {ACTION_SEND,                                     0x06,  NULL},                      // f, F, ACK
  {ACTION_SEND,                                     0x07,  NULL},                      // g, G, BEL
  {ACTION_SEND,                                     0x0e,  NULL},                      // n, N, SO
  {ACTION_SEND,                                     0x0d,  NULL},                      // m, M, CR
  {ACTION_SEND,                                     0x19,  NULL},                      // y, Y, EM
  {ACTION_SEND,                                     0x15,  NULL},                      // u, U, NAK
  {ACTION_NONE,                                     0,     NULL},                      // 7, &
  {ACTION_SEND,                                     0x1e,  NULL},                      // 6, ^, RS
  {ACTION_SEND,                                     0x0a,  NULL},                      // j, J, LF
  {ACTION_SEND,                                     0x08,  NULL},                      // h, H, BS
  {ACTION_NONE,                                     0,     NULL},                      // ,, <
  {ACTION_SEND,                                     0x1d,  NULL},                      // ], }, GS
  {ACTION_SEND,                                     0x09,  NULL},                      // i, I, TAB
  {ACTION_NONE,                                     0,     NULL},                      // 8, *
  {ACTION_NONE,                                     0,     NULL},                      // =, +
  {ACTION_SEND,                                     0x0b,  NULL},                      // k, K, VT
  {ACTION_NONE,                                     0,     NULL},                      // ., >
  {ACTION_SEND,                                     0x0f,  NULL},                      // o, O, SI
  {ACTION_NONE,                                     0,     NULL},                      // 9, (
  {ACTION_SEND,                                     0x0c,  NULL},                      // l, L, FF
  {ACTION_NONE,                                     0,     NULL},                      // /, ?
  {ACTION_SEND,                                     0x1b,  NULL},                      // [, {, ESC
  {ACTION_SEND,                                     0x10,  NULL},                      // p, P, DLE
  {ACTION_NONE,                                     0,     NULL},                      // 0, )
  {ACTION_SEND,                                     0x1f,  NULL},                      // -, _, US
  {ACTION_NONE,                                     0,     NULL},                      // ;, :
  {ACTION_NONE,                                     0,     NULL},                      // ', "
  {ACTION_NONE,                                     0,     NULL},                      // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                      // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <return>
  {ACTION_NONE,                                     0,     NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_PRINT,                                    0,     &WW_PRINT_RMar},            // <right margin>
  {ACTION_NONE,                                     0,     NULL},                      // <tab>
  {ACTION_PRINT,                                    0,     &WW_PRINT_MarRel},          // <margin release>
  {ACTION_PRINT,                                    0,     &WW_PRINT_TSet},            // <tab set>
  {ACTION_NONE,                                     0,     NULL}                       // *** not available on WW1000
};

// Setup key action table.
const struct key_action ASCII_ACTIONS_SETUP[3 * NUM_WW_KEYS] = {
  // Unshifted.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // <down arrow>
  {ACTION_COMMAND,                                  'z',   NULL},                      // z, Z, SUB
  {ACTION_COMMAND,                                  'q',   NULL},                      // q, Q, DC1/XON
  {ACTION_COMMAND,                                  '1',   NULL},                      // 1, !
  {ACTION_NONE,                                     0,     NULL},                      // `, ~
  {ACTION_COMMAND,                                  'a',   NULL},                      // a, A, SOH
  {ACTION_NONE,                                     0,     NULL},                      // <space>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_COMMAND,                                  'x',   NULL},                      // x, X, CAN
  {ACTION_COMMAND,                                  'w',   NULL},                      // w, W, ETB
  {ACTION_COMMAND,                                  '2',   NULL},                      // 2, @, NUL
  {ACTION_COMMAND,                                  's',   NULL},                      // s, S, DC3/XOFF
  {ACTION_COMMAND,                                  'c',   NULL},                      // c, C, ETX
  {ACTION_COMMAND,                                  'e',   NULL},                      // e, E, ENQ
  {ACTION_COMMAND,                                  '3',   NULL},                      // 3, #
  {ACTION_COMMAND,                                  'd',   NULL},                      // d, D, EOT
  {ACTION_COMMAND,                                  'b',   NULL},                      // b, B, STX
  {ACTION_COMMAND,                                  'v',   NULL},                      // v, V, SYN
  {ACTION_COMMAND,                                  't',   NULL},                      // t, T, DC4
  {ACTION_COMMAND,                                  'r',   NULL},                      // r, R, DC2
  {ACTION_COMMAND,                                  '4',   NULL},                      // 4, $
  {ACTION_COMMAND,                                  '5',   NULL},                      // 5, %
  {ACTION_COMMAND,                                  'f',   NULL},                      // f, F, ACK
  {ACTION_COMMAND,                                  'g',   NULL},                      // g, G, BEL
  {ACTION_COMMAND,                                  'n',   NULL},                      // n, N, SO
  {ACTION_COMMAND,                                  'm',   NULL},                      // m, M, CR
  {ACTION_COMMAND,                                  'y',   NULL},                      // y, Y, EM
  {ACTION_COMMAND,                                  'u',   NULL},                      // u, U, NAK
  {ACTION_COMMAND,                                  '7',   NULL},                      // 7, &
  {ACTION_COMMAND,                                  '6',   NULL},                      // 6, ^, RS
  {ACTION_COMMAND,                                  'j',   NULL},                      // j, J, LF
  {ACTION_COMMAND,                                  'h',   NULL},                      // h, H, BS
  {ACTION_NONE,                                     0,     NULL},                      // ,, <
  {ACTION_NONE,                                     'j',   NULL},                      // ], }, GS
  {ACTION_COMMAND,                                  'i',   NULL},                      // i, I, TAB
  {ACTION_COMMAND,                                  '8',   NULL},                      // 8, *
  {ACTION_NONE,                                     0,     NULL},                      // =, +
  {ACTION_COMMAND,                                  'k',   NULL},                      // k, K, VT
  {ACTION_NONE,                                     0,     NULL},                      // ., >
  {ACTION_COMMAND,                                  'o',   NULL},                      // o, O, SI
  {ACTION_COMMAND,                                  '9',   NULL},                      // 9, (
  {ACTION_COMMAND,                                  'l',   NULL},                      // l, L, FF
  {ACTION_NONE,                                     0,     NULL},                      // /, ?
  {ACTION_NONE,                                     0,     NULL},                      // [, {, ESC
  {ACTION_COMMAND,                                  'p',   NULL},                      // p, P, DLE
  {ACTION_COMMAND,                                  '0',   NULL},                      // 0, )
  {ACTION_NONE,                                     0,     NULL},                      // -, _, US
  {ACTION_NONE,                                     0,     NULL},                      // ;, :
  {ACTION_NONE,                                     0,     NULL},                      // ', "
  {ACTION_NONE,                                     0,     NULL},                      // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                      // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_COMMAND,                                  0x0d,  NULL},                      // <return>
  {ACTION_NONE,                                     0,     NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                      // <right margin>
  {ACTION_NONE,                                     0,     NULL},                      // <tab>
  {ACTION_NONE,                                     0,     NULL},                      // <escape>
  {ACTION_NONE,                                     0,     NULL},                      // <tab set>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // <down arrow>
  {ACTION_COMMAND,                                  'Z',   NULL},                      // z, Z, SUB
  {ACTION_COMMAND,                                  'Q',   NULL},                      // q, Q, DC1/XON
  {ACTION_NONE,                                     0,     NULL},                      // 1, !
  {ACTION_NONE,                                     0,     NULL},                      // `, ~
  {ACTION_COMMAND,                                  'A',   NULL},                      // a, A, SOH
  {ACTION_NONE,                                     0,     NULL},                      // <space>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_COMMAND,                                  'X',   NULL},                      // x, X, CAN
  {ACTION_COMMAND,                                  'W',   NULL},                      // w, W, ETB
  {ACTION_NONE,                                     0,     NULL},                      // 2, @, NUL
  {ACTION_COMMAND,                                  'S',   NULL},                      // s, S, DC3/XOFF
  {ACTION_COMMAND,                                  'C',   NULL},                      // c, C, ETX
  {ACTION_COMMAND,                                  'E',   NULL},                      // e, E, ENQ
  {ACTION_NONE,                                     0,     NULL},                      // 3, #
  {ACTION_COMMAND,                                  'D',   NULL},                      // d, D, EOT
  {ACTION_COMMAND,                                  'B',   NULL},                      // b, B, STX
  {ACTION_COMMAND,                                  'V',   NULL},                      // v, V, SYN
  {ACTION_COMMAND,                                  'T',   NULL},                      // t, T, DC4
  {ACTION_COMMAND,                                  'R',   NULL},                      // r, R, DC2
  {ACTION_NONE,                                     0,     NULL},                      // 4, $
  {ACTION_NONE,                                     0,     NULL},                      // 5, %
  {ACTION_COMMAND,                                  'F',   NULL},                      // f, F, ACK
  {ACTION_COMMAND,                                  'G',   NULL},                      // g, G, BEL
  {ACTION_COMMAND,                                  'N',   NULL},                      // n, N, SO
  {ACTION_COMMAND,                                  'M',   NULL},                      // m, M, CR
  {ACTION_COMMAND,                                  'Y',   NULL},                      // y, Y, EM
  {ACTION_COMMAND,                                  'U',   NULL},                      // u, U, NAK
  {ACTION_NONE,                                     0,     NULL},                      // 7, &
  {ACTION_NONE,                                     0,     NULL},                      // 6, ^, RS
  {ACTION_COMMAND,                                  'J',   NULL},                      // j, J, LF
  {ACTION_COMMAND,                                  'H',   NULL},                      // h, H, BS
  {ACTION_NONE,                                     0,     NULL},                      // ,, <
  {ACTION_NONE,                                     'J',   NULL},                      // ], }, GS
  {ACTION_COMMAND,                                  'I',   NULL},                      // i, I, TAB
  {ACTION_NONE,                                     0,     NULL},                      // 8, *
  {ACTION_NONE,                                     0,     NULL},                      // =, +
  {ACTION_COMMAND,                                  'K',   NULL},                      // k, K, VT
  {ACTION_NONE,                                     0,     NULL},                      // ., >
  {ACTION_COMMAND,                                  'O',   NULL},                      // o, O, SI
  {ACTION_NONE,                                     0,     NULL},                      // 9, (
  {ACTION_COMMAND,                                  'L',   NULL},                      // l, L, FF
  {ACTION_NONE,                                     0,     NULL},                      // /, ?
  {ACTION_NONE,                                     0,     NULL},                      // [, {, ESC
  {ACTION_COMMAND,                                  'P',   NULL},                      // p, P, DLE
  {ACTION_NONE,                                     0,     NULL},                      // 0, )
  {ACTION_NONE,                                     0,     NULL},                      // -, _, US
  {ACTION_NONE,                                     0,     NULL},                      // ;, :
  {ACTION_NONE,                                     0,     NULL},                      // ', "
  {ACTION_NONE,                                     0,     NULL},                      // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                      // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_COMMAND,                                  0x0d,  NULL},                      // <return>
  {ACTION_NONE,                                     0,     NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                      // <right margin>
  {ACTION_NONE,                                     0,     NULL},                      // <tab>
  {ACTION_NONE,                                     0,     NULL},                      // <escape>
  {ACTION_NONE,                                     0,     NULL},                      // <tab set>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000

  // Control.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                      // z, Z, SUB
  {ACTION_NONE,                                     0,     NULL},                      // q, Q, DC1/XON
  {ACTION_NONE,                                     0,     NULL},                      // 1, !
  {ACTION_NONE,                                     0,     NULL},                      // `, ~
  {ACTION_NONE,                                     0,     NULL},                      // a, A, SOH
  {ACTION_NONE,                                     0,     NULL},                      // <space>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_NONE,                                     0,     NULL},                      // x, X, CAN
  {ACTION_NONE,                                     0,     NULL},                      // w, W, ETB
  {ACTION_NONE,                                     0,     NULL},                      // 2, @, NUL
  {ACTION_NONE,                                     0,     NULL},                      // s, S, DC3/XOFF
  {ACTION_NONE,                                     0,     NULL},                      // c, C, ETX
  {ACTION_NONE,                                     0,     NULL},                      // e, E, ENQ
  {ACTION_NONE,                                     0,     NULL},                      // 3, #
  {ACTION_NONE,                                     0,     NULL},                      // d, D, EOT
  {ACTION_NONE,                                     0,     NULL},                      // b, B, STX
  {ACTION_NONE,                                     0,     NULL},                      // v, V, SYN
  {ACTION_NONE,                                     0,     NULL},                      // t, T, DC4
  {ACTION_NONE,                                     0,     NULL},                      // r, R, DC2
  {ACTION_NONE,                                     0,     NULL},                      // 4, $
  {ACTION_NONE,                                     0,     NULL},                      // 5, %
  {ACTION_NONE,                                     0,     NULL},                      // f, F, ACK
  {ACTION_NONE,                                     0,     NULL},                      // g, G, BEL
  {ACTION_NONE,                                     0,     NULL},                      // n, N, SO
  {ACTION_NONE,                                     0,     NULL},                      // m, M, CR
  {ACTION_NONE,                                     0,     NULL},                      // y, Y, EM
  {ACTION_NONE,                                     0,     NULL},                      // u, U, NAK
  {ACTION_NONE,                                     0,     NULL},                      // 7, &
  {ACTION_NONE,                                     0,     NULL},                      // 6, ^, RS
  {ACTION_NONE,                                     0,     NULL},                      // j, J, LF
  {ACTION_NONE,                                     0,     NULL},                      // h, H, BS
  {ACTION_NONE,                                     0,     NULL},                      // ,, <
  {ACTION_NONE,                                     0,     NULL},                      // ], }, GS
  {ACTION_NONE,                                     0,     NULL},                      // i, I, TAB
  {ACTION_NONE,                                     0,     NULL},                      // 8, *
  {ACTION_NONE,                                     0,     NULL},                      // =, +
  {ACTION_NONE,                                     0,     NULL},                      // k, K, VT
  {ACTION_NONE,                                     0,     NULL},                      // ., >
  {ACTION_NONE,                                     0,     NULL},                      // o, O, SI
  {ACTION_NONE,                                     0,     NULL},                      // 9, (
  {ACTION_NONE,                                     0,     NULL},                      // l, L, FF
  {ACTION_NONE,                                     0,     NULL},                      // /, ?
  {ACTION_NONE,                                     0,     NULL},                      // [, {, ESC
  {ACTION_NONE,                                     0,     NULL},                      // p, P, DLE
  {ACTION_NONE,                                     0,     NULL},                      // 0, )
  {ACTION_NONE,                                     0,     NULL},                      // -, _, US
  {ACTION_NONE,                                     0,     NULL},                      // ;, :
  {ACTION_NONE,                                     0,     NULL},                      // ', "
  {ACTION_NONE,                                     0,     NULL},                      // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                      // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <return>
  {ACTION_NONE,                                     0,     NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                      // <right margin>
  {ACTION_NONE,                                     0,     NULL},                      // <tab>
  {ACTION_NONE,                                     0,     NULL},                      // <margin release>
  {ACTION_NONE,                                     0,     NULL},                      // <tab set>
  {ACTION_NONE,                                     0,     NULL}                       // *** not available on WW1000
};


//**********************************************************************************************************************
//
//  ASCII Terminal data declarations.
//
//**********************************************************************************************************************


//**********************************************************************************************************************
//
//  ASCII Terminal setup function.
//
//**********************************************************************************************************************

void Setup_ASCII () {

  // Initialize variables.
  if (duplex == DUPLEX_HALF) {
    if (uppercase == SETTING_FALSE) {
      key_actions = &ASCII_ACTIONS_HALF;
    } else {
      key_actions = &ASCII_ACTIONS_HALF_UPPERCASE;
    }
  } else /* duplex == DUPLEX_FULL */ {
    if (uppercase == SETTING_FALSE) {
      key_actions = &ASCII_ACTIONS_FULL;
    } else {
      key_actions = &ASCII_ACTIONS_FULL_UPPERCASE;
    }
  }
  serial_actions = &ASCII_SERIAL_ACTIONS;
  flow_on = CHAR_ASCII_XON;
  flow_off = CHAR_ASCII_XOFF;
  digitalWriteFast (serial_rts_pin, HIGH);
}


//**********************************************************************************************************************
//
//  ASCII Terminal support routines.
//
//**********************************************************************************************************************

// Print ASCII Terminal setup title.
void Print_ASCII_setup_title () {
  Print_string (&WW_PRINT_CRtn);  Print_string (&WW_PRINT_CRtn);
  Print_characters ("---- Cadetwriter: " ASCII_VERSION " Setup");
  Print_string (&WW_PRINT_CRtn);  Print_string (&WW_PRINT_CRtn);
}

// Update ASCII Terminal settings.
void Update_ASCII_settings () {
  byte obattery = battery;
  byte oserial = serial;
  byte oparity = parity;
  byte obaud = baud;
  byte odps = dps;
  byte owidth = width;

  // Query new settings.
  Print_string (&WW_PRINT_CRtn);
  errors = Read_truefalse_setting ("record errors", errors);
  warnings = Read_truefalse_setting ("record warnings", warnings);
  battery = Read_truefalse_setting ("batteries installed", battery);
  serial = Read_serial_setting ("serial", serial);
  duplex = Read_duplex_setting ("duplex", duplex);
  if (serial == SERIAL_USB) {
    parity = Read_parity_setting ("parity", parity);
  } else /* serial == SERIAL_RS232 */ {
    baud = Read_baud_setting ("baud rate", baud);
    dps = Read_dps_setting ("dps", dps);
  }
  swflow = Read_swflow_setting ("sw flow control", swflow);
  if (serial == SERIAL_RS232) {
    hwflow = Read_hwflow_setting ("hw flow control", hwflow);
  }
  uppercase = Read_truefalse_setting ("uppercase only", uppercase);
  if (duplex == DUPLEX_HALF) {
    if (uppercase == SETTING_FALSE) {
      key_actions_save = &ASCII_ACTIONS_HALF;
    } else {
      key_actions_save = &ASCII_ACTIONS_HALF_UPPERCASE;
    }
  } else /* duplex == DUPLEX_FULL */ {
    if (uppercase == SETTING_FALSE) {
      key_actions_save = &ASCII_ACTIONS_FULL;
    } else {
      key_actions_save = &ASCII_ACTIONS_FULL_UPPERCASE;
    }
  }
  autoreturn = Read_truefalse_setting ("auto return", autoreturn);
  transmiteol = Read_eol_setting ("transmit end-of-line", transmiteol);
  receiveeol = Read_eol_setting ("receive end-of-line", receiveeol);
  ignoreescape = Read_truefalse_setting ("ignore escape sequences", ignoreescape);
  width = Read_width_setting ("paper width", width);
  offset = Read_truefalse_setting ("column offset", offset);
  Print_string (&WW_PRINT_CRtn);

  // Save settings in EEPROM if requested.
  if (Ask_yesno_question ("Save settings", FALSE)) {
    Write_EEPROM (EEPROM_ERRORS, errors);
    Write_EEPROM (EEPROM_WARNINGS, warnings);
    Write_EEPROM (EEPROM_BATTERY, battery);
    Write_EEPROM (EEPROM_SERIAL, serial);
    Write_EEPROM (EEPROM_DUPLEX, duplex);
    Write_EEPROM (EEPROM_BAUD, baud);
    Write_EEPROM (EEPROM_PARITY, parity);
    Write_EEPROM (EEPROM_DPS, dps);
    Write_EEPROM (EEPROM_SWFLOW, swflow);
    Write_EEPROM (EEPROM_HWFLOW, hwflow);
    Write_EEPROM (EEPROM_UPPERCASE, uppercase);
    Write_EEPROM (EEPROM_AUTORETURN, autoreturn);
    Write_EEPROM (EEPROM_TRANSMITEOL, transmiteol);
    Write_EEPROM (EEPROM_RECEIVEEOL, receiveeol);
    Write_EEPROM (EEPROM_IGNOREESCAPE, ignoreescape);
    Write_EEPROM (EEPROM_WIDTH, width);
    Write_EEPROM (EEPROM_OFFSET, offset);
  }
  Print_string (&WW_PRINT_CRtn);

  // Reset communications if any communication changes.
  if ((oserial != serial) || (oparity != parity) || (obaud != baud) || (odps != dps)) {
    flow_in_on = FALSE;
    flow_out_on = FALSE;
    Serial_end (oserial);
    Serial_begin ();
  }
 
  // Set margins and tabs if battery or width changed.
  if ((obattery != battery) || (owidth != width)) {
    Set_margins_tabs (TRUE);
  }
}

// Print ASCII Terminal character set.
void Print_ASCII_character_set ()  {
  Print_string (&WW_PRINT_CRtn);  Print_string (&WW_PRINT_SPACE);  Print_string (&WW_PRINT_SPACE);
  Print_string (&WW_PRINT_A);  Print_string (&WW_PRINT_B);  Print_string (&WW_PRINT_C);  Print_string (&WW_PRINT_D);
  Print_string (&WW_PRINT_E);  Print_string (&WW_PRINT_F);  Print_string (&WW_PRINT_G);  Print_string (&WW_PRINT_H);
  Print_string (&WW_PRINT_I);  Print_string (&WW_PRINT_J);  Print_string (&WW_PRINT_K);  Print_string (&WW_PRINT_L);
  Print_string (&WW_PRINT_M);  Print_string (&WW_PRINT_N);  Print_string (&WW_PRINT_O);  Print_string (&WW_PRINT_P);
  Print_string (&WW_PRINT_Q);  Print_string (&WW_PRINT_R);  Print_string (&WW_PRINT_S);  Print_string (&WW_PRINT_T);
  Print_string (&WW_PRINT_U);  Print_string (&WW_PRINT_V);  Print_string (&WW_PRINT_W);  Print_string (&WW_PRINT_X);
  Print_string (&WW_PRINT_Y);  Print_string (&WW_PRINT_Z);
  Print_string (&WW_PRINT_SPACE);
  Print_string (&WW_PRINT_a);  Print_string (&WW_PRINT_b);  Print_string (&WW_PRINT_c);  Print_string (&WW_PRINT_d);
  Print_string (&WW_PRINT_e);  Print_string (&WW_PRINT_f);  Print_string (&WW_PRINT_g);  Print_string (&WW_PRINT_h);
  Print_string (&WW_PRINT_i);  Print_string (&WW_PRINT_j);  Print_string (&WW_PRINT_k);  Print_string (&WW_PRINT_l);
  Print_string (&WW_PRINT_m);  Print_string (&WW_PRINT_n);  Print_string (&WW_PRINT_o);  Print_string (&WW_PRINT_p);
  Print_string (&WW_PRINT_q);  Print_string (&WW_PRINT_r);  Print_string (&WW_PRINT_s);  Print_string (&WW_PRINT_t);
  Print_string (&WW_PRINT_u);  Print_string (&WW_PRINT_v);  Print_string (&WW_PRINT_w);  Print_string (&WW_PRINT_x);
  Print_string (&WW_PRINT_y);  Print_string (&WW_PRINT_z);
  Print_string (&WW_PRINT_CRtn);  Print_string (&WW_PRINT_SPACE);  Print_string (&WW_PRINT_SPACE);
  Print_string (&WW_PRINT_0);  Print_string (&WW_PRINT_1);  Print_string (&WW_PRINT_2);  Print_string (&WW_PRINT_3);
  Print_string (&WW_PRINT_4);  Print_string (&WW_PRINT_5);  Print_string (&WW_PRINT_6);  Print_string (&WW_PRINT_7);
  Print_string (&WW_PRINT_8);  Print_string (&WW_PRINT_9);
  Print_string (&WW_PRINT_SPACE);
  Print_string (&WW_PRINT_EXCLAMATION);  Print_string (&WW_PRINT_QUOTE);       Print_string (&WW_PRINT_POUND);
  Print_string (&WW_PRINT_DOLLAR);       Print_string (&WW_PRINT_PERCENT);     Print_string (&WW_PRINT_AMPERSAND);
  Print_string (&WW_PRINT_APOSTROPHE);   Print_string (&WW_PRINT_LPAREN);      Print_string (&WW_PRINT_RPAREN);
  Print_string (&WW_PRINT_ASTERISK);     Print_string (&WW_PRINT_PLUS);        Print_string (&WW_PRINT_COMMA);
  Print_string (&WW_PRINT_HYPHEN);       Print_string (&WW_PRINT_PERIOD);      Print_string (&WW_PRINT_SLASH);
  Print_string (&WW_PRINT_COLON);        Print_string (&WW_PRINT_SEMICOLON);   Print_string (&ASCII_PRINT_LESS);
  Print_string (&WW_PRINT_EQUAL);        Print_string (&ASCII_PRINT_GREATER);  Print_string (&WW_PRINT_QUESTION);
  Print_string (&WW_PRINT_SPACE);
  Print_string (&WW_PRINT_AT);              Print_string (&WW_PRINT_LBRACKET);   Print_string (&ASCII_PRINT_BSLASH);
  Print_string (&WW_PRINT_RBRACKET);        Print_string (&ASCII_PRINT_CARET);   Print_string (&WW_PRINT_UNDERSCORE);
  Print_string (&ASCII_PRINT_BAPOSTROPHE);  Print_string (&ASCII_PRINT_LBRACE);  Print_string (&ASCII_PRINT_BAR);
  Print_string (&ASCII_PRINT_RBRACE);       Print_string (&ASCII_PRINT_TILDE);
  Print_string (&WW_PRINT_CRtn);  Print_string (&WW_PRINT_CRtn);
}



//======================================================================================================================
//
//  Future emulation.
//
//======================================================================================================================

//**********************************************************************************************************************
//
//  Future defines.
//
//**********************************************************************************************************************


//**********************************************************************************************************************
//
//  Future print strings.
//
//**********************************************************************************************************************


//**********************************************************************************************************************
//
//  Future serial action table.
//
//**********************************************************************************************************************


//**********************************************************************************************************************
//
//  Future key action tables.
//
//**********************************************************************************************************************

// Stup key action table.
const struct key_action FUTURE_ACTIONS_SETUP[3 * NUM_WW_KEYS] = {
  // Unshifted.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // <down arrow>
  {ACTION_COMMAND,                                  'Z',   NULL},                      // z, Z, SUB
  {ACTION_COMMAND,                                  'Q',   NULL},                      // q, Q, DC1/XON
  {ACTION_COMMAND,                                  '1',   NULL},                      // 1, !
  {ACTION_NONE,                                     0,     NULL},                      // `, ~
  {ACTION_COMMAND,                                  'A',   NULL},                      // a, A, SOH
  {ACTION_NONE,                                     0,     NULL},                      // <space>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_COMMAND,                                  'X',   NULL},                      // x, X, CAN
  {ACTION_COMMAND,                                  'W',   NULL},                      // w, W, ETB
  {ACTION_COMMAND,                                  '2',   NULL},                      // 2, @, NUL
  {ACTION_COMMAND,                                  'S',   NULL},                      // s, S, DC3/XOFF
  {ACTION_COMMAND,                                  'C',   NULL},                      // c, C, ETX
  {ACTION_COMMAND,                                  'E',   NULL},                      // e, E, ENQ
  {ACTION_COMMAND,                                  '3',   NULL},                      // 3, #
  {ACTION_COMMAND,                                  'D',   NULL},                      // d, D, EOT
  {ACTION_COMMAND,                                  'B',   NULL},                      // b, B, STX
  {ACTION_COMMAND,                                  'V',   NULL},                      // v, V, SYN
  {ACTION_COMMAND,                                  'T',   NULL},                      // t, T, DC4
  {ACTION_COMMAND,                                  'R',   NULL},                      // r, R, DC2
  {ACTION_COMMAND,                                  '4',   NULL},                      // 4, $
  {ACTION_COMMAND,                                  '5',   NULL},                      // 5, %
  {ACTION_COMMAND,                                  'F',   NULL},                      // f, F, ACK
  {ACTION_COMMAND,                                  'G',   NULL},                      // g, G, BEL
  {ACTION_COMMAND,                                  'N',   NULL},                      // n, N, SO
  {ACTION_COMMAND,                                  'M',   NULL},                      // m, M, CR
  {ACTION_COMMAND,                                  'Y',   NULL},                      // y, Y, EM
  {ACTION_COMMAND,                                  'U',   NULL},                      // u, U, NAK
  {ACTION_COMMAND,                                  '7',   NULL},                      // 7, &
  {ACTION_COMMAND,                                  '6',   NULL},                      // 6, ^, RS
  {ACTION_COMMAND,                                  'J',   NULL},                      // j, J, LF
  {ACTION_COMMAND,                                  'H',   NULL},                      // h, H, BS
  {ACTION_NONE,                                     0,     NULL},                      // ,, <
  {ACTION_NONE,                                     'J',   NULL},                      // ], }, GS
  {ACTION_COMMAND,                                  'I',   NULL},                      // i, I, TAB
  {ACTION_COMMAND,                                  '8',   NULL},                      // 8, *
  {ACTION_NONE,                                     0,     NULL},                      // =, +
  {ACTION_COMMAND,                                  'K',   NULL},                      // k, K, VT
  {ACTION_NONE,                                     0,     NULL},                      // ., >
  {ACTION_COMMAND,                                  'O',   NULL},                      // o, O, SI
  {ACTION_COMMAND,                                  '9',   NULL},                      // 9, (
  {ACTION_COMMAND,                                  'L',   NULL},                      // l, L, FF
  {ACTION_NONE,                                     0,     NULL},                      // /, ?
  {ACTION_NONE,                                     0,     NULL},                      // [, {, ESC
  {ACTION_COMMAND,                                  'P',   NULL},                      // p, P, DLE
  {ACTION_COMMAND,                                  '0',   NULL},                      // 0, )
  {ACTION_NONE,                                     0,     NULL},                      // -, _, US
  {ACTION_NONE,                                     0,     NULL},                      // ;, :
  {ACTION_NONE,                                     0,     NULL},                      // ', "
  {ACTION_NONE,                                     0,     NULL},                      // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                      // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <return>
  {ACTION_NONE,                                     0,     NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                      // <right margin>
  {ACTION_NONE,                                     0,     NULL},                      // <tab>
  {ACTION_NONE,                                     0,     NULL},                      // <escape>
  {ACTION_NONE,                                     0,     NULL},                      // <tab set>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // <down arrow>
  {ACTION_COMMAND,                                  'Z',   NULL},                      // z, Z, SUB
  {ACTION_COMMAND,                                  'Q',   NULL},                      // q, Q, DC1/XON
  {ACTION_NONE,                                     0,     NULL},                      // 1, !
  {ACTION_NONE,                                     0,     NULL},                      // `, ~
  {ACTION_COMMAND,                                  'A',   NULL},                      // a, A, SOH
  {ACTION_NONE,                                     0,     NULL},                      // <space>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_COMMAND,                                  'X',   NULL},                      // x, X, CAN
  {ACTION_COMMAND,                                  'W',   NULL},                      // w, W, ETB
  {ACTION_NONE,                                     0,     NULL},                      // 2, @, NUL
  {ACTION_COMMAND,                                  'S',   NULL},                      // s, S, DC3/XOFF
  {ACTION_COMMAND,                                  'C',   NULL},                      // c, C, ETX
  {ACTION_COMMAND,                                  'E',   NULL},                      // e, E, ENQ
  {ACTION_NONE,                                     0,     NULL},                      // 3, #
  {ACTION_COMMAND,                                  'D',   NULL},                      // d, D, EOT
  {ACTION_COMMAND,                                  'B',   NULL},                      // b, B, STX
  {ACTION_COMMAND,                                  'V',   NULL},                      // v, V, SYN
  {ACTION_COMMAND,                                  'T',   NULL},                      // t, T, DC4
  {ACTION_COMMAND,                                  'R',   NULL},                      // r, R, DC2
  {ACTION_NONE,                                     0,     NULL},                      // 4, $
  {ACTION_NONE,                                     0,     NULL},                      // 5, %
  {ACTION_COMMAND,                                  'F',   NULL},                      // f, F, ACK
  {ACTION_COMMAND,                                  'G',   NULL},                      // g, G, BEL
  {ACTION_COMMAND,                                  'N',   NULL},                      // n, N, SO
  {ACTION_COMMAND,                                  'M',   NULL},                      // m, M, CR
  {ACTION_COMMAND,                                  'Y',   NULL},                      // y, Y, EM
  {ACTION_COMMAND,                                  'U',   NULL},                      // u, U, NAK
  {ACTION_NONE,                                     0,     NULL},                      // 7, &
  {ACTION_NONE,                                     0,     NULL},                      // 6, ^, RS
  {ACTION_COMMAND,                                  'J',   NULL},                      // j, J, LF
  {ACTION_COMMAND,                                  'H',   NULL},                      // h, H, BS
  {ACTION_NONE,                                     0,     NULL},                      // ,, <
  {ACTION_NONE,                                     'J',   NULL},                      // ], }, GS
  {ACTION_COMMAND,                                  'I',   NULL},                      // i, I, TAB
  {ACTION_NONE,                                     0,     NULL},                      // 8, *
  {ACTION_NONE,                                     0,     NULL},                      // =, +
  {ACTION_COMMAND,                                  'K',   NULL},                      // k, K, VT
  {ACTION_NONE,                                     0,     NULL},                      // ., >
  {ACTION_COMMAND,                                  'O',   NULL},                      // o, O, SI
  {ACTION_NONE,                                     0,     NULL},                      // 9, (
  {ACTION_COMMAND,                                  'L',   NULL},                      // l, L, FF
  {ACTION_NONE,                                     0,     NULL},                      // /, ?
  {ACTION_NONE,                                     0,     NULL},                      // [, {, ESC
  {ACTION_COMMAND,                                  'P',   NULL},                      // p, P, DLE
  {ACTION_NONE,                                     0,     NULL},                      // 0, )
  {ACTION_NONE,                                     0,     NULL},                      // -, _, US
  {ACTION_NONE,                                     0,     NULL},                      // ;, :
  {ACTION_NONE,                                     0,     NULL},                      // ', "
  {ACTION_NONE,                                     0,     NULL},                      // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                      // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <return>
  {ACTION_NONE,                                     0,     NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                      // <right margin>
  {ACTION_NONE,                                     0,     NULL},                      // <tab>
  {ACTION_NONE,                                     0,     NULL},                      // <escape>
  {ACTION_NONE,                                     0,     NULL},                      // <tab set>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000

  // Control.
  {ACTION_NONE,                                     0,     NULL},                      // <left shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right shift>
  {ACTION_NONE,                                     0,     NULL},                      // <right arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // \, |
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <setup>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // <down arrow>
  {ACTION_NONE,                                     0,     NULL},                      // z, Z, SUB
  {ACTION_NONE,                                     0,     NULL},                      // q, Q, DC1/XON
  {ACTION_NONE,                                     0,     NULL},                      // 1, !
  {ACTION_NONE,                                     0,     NULL},                      // `, ~
  {ACTION_NONE,                                     0,     NULL},                      // a, A, SOH
  {ACTION_NONE,                                     0,     NULL},                      // <space>
  {ACTION_NONE,                                     0,     NULL},                      //
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <left margin>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <tab clear>
  {ACTION_NONE,                                     0,     NULL},                      // <control>
  {ACTION_NONE,                                     0,     NULL},                      // x, X, CAN
  {ACTION_NONE,                                     0,     NULL},                      // w, W, ETB
  {ACTION_NONE,                                     0,     NULL},                      // 2, @, NUL
  {ACTION_NONE,                                     0,     NULL},                      // s, S, DC3/XOFF
  {ACTION_NONE,                                     0,     NULL},                      // c, C, ETX
  {ACTION_NONE,                                     0,     NULL},                      // e, E, ENQ
  {ACTION_NONE,                                     0,     NULL},                      // 3, #
  {ACTION_NONE,                                     0,     NULL},                      // d, D, EOT
  {ACTION_NONE,                                     0,     NULL},                      // b, B, STX
  {ACTION_NONE,                                     0,     NULL},                      // v, V, SYN
  {ACTION_NONE,                                     0,     NULL},                      // t, T, DC4
  {ACTION_NONE,                                     0,     NULL},                      // r, R, DC2
  {ACTION_NONE,                                     0,     NULL},                      // 4, $
  {ACTION_NONE,                                     0,     NULL},                      // 5, %
  {ACTION_NONE,                                     0,     NULL},                      // f, F, ACK
  {ACTION_NONE,                                     0,     NULL},                      // g, G, BEL
  {ACTION_NONE,                                     0,     NULL},                      // n, N, SO
  {ACTION_NONE,                                     0,     NULL},                      // m, M, CR
  {ACTION_NONE,                                     0,     NULL},                      // y, Y, EM
  {ACTION_NONE,                                     0,     NULL},                      // u, U, NAK
  {ACTION_NONE,                                     0,     NULL},                      // 7, &
  {ACTION_NONE,                                     0,     NULL},                      // 6, ^, RS
  {ACTION_NONE,                                     0,     NULL},                      // j, J, LF
  {ACTION_NONE,                                     0,     NULL},                      // h, H, BS
  {ACTION_NONE,                                     0,     NULL},                      // ,, <
  {ACTION_NONE,                                     0,     NULL},                      // ], }, GS
  {ACTION_NONE,                                     0,     NULL},                      // i, I, TAB
  {ACTION_NONE,                                     0,     NULL},                      // 8, *
  {ACTION_NONE,                                     0,     NULL},                      // =, +
  {ACTION_NONE,                                     0,     NULL},                      // k, K, VT
  {ACTION_NONE,                                     0,     NULL},                      // ., >
  {ACTION_NONE,                                     0,     NULL},                      // o, O, SI
  {ACTION_NONE,                                     0,     NULL},                      // 9, (
  {ACTION_NONE,                                     0,     NULL},                      // l, L, FF
  {ACTION_NONE,                                     0,     NULL},                      // /, ?
  {ACTION_NONE,                                     0,     NULL},                      // [, {, ESC
  {ACTION_NONE,                                     0,     NULL},                      // p, P, DLE
  {ACTION_NONE,                                     0,     NULL},                      // 0, )
  {ACTION_NONE,                                     0,     NULL},                      // -, _, US
  {ACTION_NONE,                                     0,     NULL},                      // ;, :
  {ACTION_NONE,                                     0,     NULL},                      // ', "
  {ACTION_NONE,                                     0,     NULL},                      // <left arrow>
  {ACTION_NONE,                                     0,     NULL},                      // <up arrow>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <backspace>
  {ACTION_NONE,                                     0,     NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                     0,     NULL},                      // <return>
  {ACTION_NONE,                                     0,     NULL},                      // <delete>
  {ACTION_NONE,                                     0,     NULL},                      // <shift lock>
  {ACTION_NONE,                                     0,     NULL},                      // <right margin>
  {ACTION_NONE,                                     0,     NULL},                      // <tab>
  {ACTION_NONE,                                     0,     NULL},                      // <escape>
  {ACTION_NONE,                                     0,     NULL},                      // <tab set>
  {ACTION_NONE,                                     0,     NULL}                       // *** not available on WW1000
};


//**********************************************************************************************************************
//
//  Future data declarations.
//
//**********************************************************************************************************************


//**********************************************************************************************************************
//
//  Future setup function.
//
//**********************************************************************************************************************

void Setup_FUTURE () {
  // key_actions = 
  // serial_actions = 
  // flow_on = 
  // flow_off = 
  // digitalWriteFast (serial_rts_pin, HIGH);
}


//**********************************************************************************************************************
//
//  Future support routines.
//
//**********************************************************************************************************************



//======================================================================================================================
//
//  Standalone Typewriter emulation.
//
//======================================================================================================================

void Setup_STANDALONE () {
  // Nothing to be done here.  All setup is done by the master setup routine.
}

void Loop_STANDALONE () {
  // Nothing to be done here.  All work is done in the Interrupt Service Routine.
}

void ISR_STANDALONE () {

  // Do not process column scan interrupts while the firmware is initializing.
  if (run_mode == MODE_INITIALIZING) return;

  // Reset all row drive lines that may have been asserted for the preceeding column signal to prevent row drive
  // spill-over into this (a successive) column time period.
  Clear_all_row_lines ();

  // Wait for the scan lines to settle down.
  delayMicroseconds (ISR_DELAY);

  // Read the keyboard row lines to see which keys are pressed. Multiple keys are possible for each column.  Locally
  // print the detected characters pressed on the keys, by asserting the corresponding row drive line(s).  The
  // row drive line(s) MUST be asserted Tproc_dly = 265 uSec or less after the start of detected column scan pulse in
  // order to be recognized by the WheelWriter logic board.  The tests below occur AFTER the column signal has been
  // asserted, so we are in the Tcol_pulse = 820 uSec (for no printing) or Tcol_pulse_long1 = 3.68 mSec (if printing
  // characters) time period.
  if (digitalReadFast (ROW_IN_1_PIN) == LOW) {
    digitalWriteFast (ROW_OUT_1_PIN, HIGH);
    digitalWriteFast (ROW_ENABLE_PIN, LOW);
  }
  if (digitalReadFast (ROW_IN_2_PIN) == LOW) {
    digitalWriteFast (ROW_OUT_2_PIN, HIGH);
    digitalWriteFast (ROW_ENABLE_PIN, LOW);
  }
  if (digitalReadFast (ROW_IN_3_PIN) == LOW) {
    digitalWriteFast (row_out_3_pin, HIGH);
    digitalWriteFast (ROW_ENABLE_PIN, LOW);
  }
  if (digitalReadFast (ROW_IN_4_PIN) == LOW) {
    digitalWriteFast (row_out_4_pin, HIGH);
    digitalWriteFast (ROW_ENABLE_PIN, LOW);
  }
  if (digitalReadFast (ROW_IN_5_PIN) == LOW) {
    digitalWriteFast (ROW_OUT_5_PIN, HIGH);
    digitalWriteFast (ROW_ENABLE_PIN, LOW);
  }
  if (digitalReadFast (ROW_IN_6_PIN) == LOW) {
    digitalWriteFast (ROW_OUT_6_PIN, HIGH);
    digitalWriteFast (ROW_ENABLE_PIN, LOW);
  }
  if (digitalReadFast (ROW_IN_7_PIN) == LOW) {
    digitalWriteFast (ROW_OUT_7_PIN, HIGH);
    digitalWriteFast (ROW_ENABLE_PIN, LOW);
  }
  if (digitalReadFast (ROW_IN_8_PIN) == LOW) {
    digitalWriteFast (ROW_OUT_8_PIN, HIGH);
    digitalWriteFast (ROW_ENABLE_PIN, LOW);
  }
}



//======================================================================================================================
//
//  Master setup and loop functions.
//
//======================================================================================================================

void setup () {

  // Set the typewriter run mode to initializing.
  run_mode = MODE_INITIALIZING;

  // Determine revision of Serial Interface Board.
  pinMode (BLUE_LED_PIN, INPUT);
  if (digitalRead (BLUE_LED_PIN) == HIGH) {  // This must be digitalRead not digitalReadFast in this case.
    blue_led_on = LOW;                 // Serial Interface Board v1.6 or v1.7.
    blue_led_off = HIGH;
    row_out_3_pin = ROW_OUT_3_PIN;
    row_out_4_pin = ROW_OUT_4_PIN;
    row_out_pins[3] = ROW_OUT_3_PIN;
    row_out_pins[4] = ROW_OUT_4_PIN;
    serial_rts_pin = SERIAL_RTS_PIN;
    serial_cts_pin = SERIAL_CTS_PIN;
  } else {
    blue_led_on = HIGH;                // Serial Interface Board v2.0.
    blue_led_off = LOW;
    row_out_3_pin = ROW_OUT_3X_PIN;
    row_out_4_pin = ROW_OUT_4X_PIN;
    row_out_pins[3] = ROW_OUT_3X_PIN;
    row_out_pins[4] = ROW_OUT_4X_PIN;
    serial_rts_pin = SERIAL_RTSX_PIN;
    serial_cts_pin = SERIAL_CTSX_PIN;
  }

  // Configure the direction of the keyboard column monitor pins as inputs.
  pinMode (COL_1_PIN, INPUT);
  pinMode (COL_2_PIN, INPUT);
  pinMode (COL_3_PIN, INPUT);
  pinMode (COL_4_PIN, INPUT);
  pinMode (COL_5_PIN, INPUT);
  pinMode (COL_6_PIN, INPUT);
  pinMode (COL_7_PIN, INPUT);
  pinMode (COL_8_PIN, INPUT);
  pinMode (COL_9_PIN, INPUT);
  pinMode (COL_10_PIN, INPUT);
  pinMode (COL_11_PIN, INPUT);
  pinMode (COL_12_PIN, INPUT);
  pinMode (COL_13_PIN, INPUT);
  pinMode (COL_14_PIN, INPUT);

  // Configure the direction of the keyboard row monitor pins as inputs.
  pinMode (ROW_IN_1_PIN, INPUT);
  pinMode (ROW_IN_2_PIN, INPUT);
  pinMode (ROW_IN_3_PIN, INPUT);
  pinMode (ROW_IN_4_PIN, INPUT);
  pinMode (ROW_IN_5_PIN, INPUT);
  pinMode (ROW_IN_6_PIN, INPUT);
  pinMode (ROW_IN_7_PIN, INPUT);
  pinMode (ROW_IN_8_PIN, INPUT);

  // Configure the direction of the logic board row drive pins as outputs.
  pinMode (ROW_OUT_1_PIN, OUTPUT);
  pinMode (ROW_OUT_2_PIN, OUTPUT);
  pinMode (row_out_3_pin, OUTPUT);
  pinMode (row_out_4_pin, OUTPUT);
  pinMode (ROW_OUT_5_PIN, OUTPUT);
  pinMode (ROW_OUT_6_PIN, OUTPUT);
  pinMode (ROW_OUT_7_PIN, OUTPUT);
  pinMode (ROW_OUT_8_PIN, OUTPUT);

  // Configure the direction of the Teensy 3.5 I/O port pins.
  pinMode (JUMPER_6_PIN, INPUT);
  pinMode (JUMPER_7_PIN, INPUT);
  pinMode (ORANGE_LED_PIN, OUTPUT);
  pinMode (BLUE_LED_PIN, OUTPUT);
  pinMode (ROW_ENABLE_PIN, OUTPUT);
  pinMode (UART_RX_PIN, INPUT);
  pinMode (UART_TX_PIN, OUTPUT);
  pinMode (serial_rts_pin, OUTPUT);
  pinMode (serial_cts_pin, INPUT);

  // Initialize the logic board row drive lines as inactive.
  Clear_all_row_lines ();

  // Initialize the status LEDs to off.
  digitalWriteFast (ORANGE_LED_PIN, LOW);
  digitalWriteFast (BLUE_LED_PIN, blue_led_off);

  // Initialize the row drive line to off.
  digitalWriteFast (ROW_ENABLE_PIN, HIGH);

  // Initialize the RS-232 TX line to off.
  digitalWriteFast (UART_TX_PIN, HIGH);

  // Determine which emulation to run based on jumper settings.
  //   J6 J7  Jumper settings.
  //           IBM 1620 Jr. Console Typewriter emulation.
  //       X   ASCII Terminal emulation.
  //    X      Reserved for future emulation.
  //    X  X   Standalone Typewriter emulation.
  if (digitalReadFast (JUMPER_6_PIN) == HIGH) {
    if (digitalReadFast (JUMPER_7_PIN) == HIGH) {
      emulation = EMULATION_IBM;         // IBM 1620 Jr. emulation.
    } else {
      emulation = EMULATION_ASCII;       // ASCII Terminal emulation.
    }
  } else {
    if (digitalReadFast (JUMPER_7_PIN) == HIGH) {
      // emulation = EMULATION_FUTURE;      // Future emulation.
         emulation = EMULATION_STANDALONE;
    } else {
      emulation = EMULATION_STANDALONE;  // Standalone emulation.
    }
  }

  // Configure the column input pins to trigger interrupts.
  noInterrupts ();
  if (emulation != EMULATION_STANDALONE) {
    attachInterrupt (digitalPinToInterrupt (COL_1_PIN), ISR_common, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_2_PIN), ISR_common, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_3_PIN), ISR_common, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_4_PIN), ISR_common, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_5_PIN), ISR_common, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_6_PIN), ISR_common, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_7_PIN), ISR_common, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_8_PIN), ISR_common, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_9_PIN), ISR_common, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_10_PIN), ISR_common, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_11_PIN), ISR_common, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_12_PIN), ISR_common, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_13_PIN), ISR_common, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_14_PIN), ISR_common, FALLING);
  } else {
    attachInterrupt (digitalPinToInterrupt (COL_1_PIN), ISR_STANDALONE, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_2_PIN), ISR_STANDALONE, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_3_PIN), ISR_STANDALONE, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_4_PIN), ISR_STANDALONE, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_5_PIN), ISR_STANDALONE, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_6_PIN), ISR_STANDALONE, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_7_PIN), ISR_STANDALONE, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_8_PIN), ISR_STANDALONE, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_9_PIN), ISR_STANDALONE, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_10_PIN), ISR_STANDALONE, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_11_PIN), ISR_STANDALONE, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_12_PIN), ISR_STANDALONE, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_13_PIN), ISR_STANDALONE, FALLING);
    attachInterrupt (digitalPinToInterrupt (COL_14_PIN), ISR_STANDALONE, FALLING);
  }
  interrupts ();

  // Initialize global variables.
  Initialize_global_variables ();

  // Initialize configuration settings.
  Initialize_configuration_settings (FALSE);

  // Initialize typewriter if the batteries are not installed.
  if ((emulation != EMULATION_STANDALONE) && (battery == SETTING_FALSE)) {
    Print_string (&WW_PRINT_POWERWISE_OFF);
    Print_string (&WW_PRINT_SPELL_CHECK);
  }

  // Complete the emulation setup.
  if      (emulation == EMULATION_IBM)        Setup_IBM ();
  else if (emulation == EMULATION_ASCII)      Setup_ASCII ();
  else if (emulation == EMULATION_FUTURE)     Setup_FUTURE ();
  else if (emulation == EMULATION_STANDALONE) Setup_STANDALONE ();

  // Open the communication port.
  if (emulation != EMULATION_STANDALONE) {
    Serial_begin ();
  }

  // Wait for the typewriter to fully power up and initialize all of its mechanical components.
  delay (2000);

  // Set the typewriter run mode to running.
  run_mode = MODE_RUNNING;

  // Wait for any initial printing to finish.
  if (emulation != EMULATION_STANDALONE) {
    Wait_print_buffer_empty ();
  }
}

void loop () {

  // Special case for Standalone Typewriter.
  if (emulation == EMULATION_STANDALONE) {
    Loop_STANDALONE ();
    return;
  }

  // Receive all pending characters.
  while (Serial_available ()) {
    byte chr = Serial_read () & 0x7f;
    if (rb_count >= SIZE_RECEIVE_BUFFER) {  // Character doesn't fit in receive buffer.
      Report_error (ERROR_RB_FULL);
      break;
    }

    // Handle escape sequences.
    if (escaping) {
      if (terminate_escape[chr]) escaping = FALSE;
      continue;
    } else if ((ignoreescape == SETTING_TRUE) && (chr == CHAR_ASCII_ESC)) {
      escaping = TRUE;
      continue;
    }

    // Handle ignore end-of-line characters.
    if (ignore_cr) {
      ignore_cr = FALSE;
      if (chr == CHAR_ASCII_CR) continue;
    } else if (ignore_lf) {
      ignore_lf = FALSE;
      if (chr == CHAR_ASCII_LF) continue;
    }

    // Receive buffer getting full, turn flow control on.
    if ((rb_count >= UPPER_THRESHOLD_RECEIVE_BUFFER) && !flow_in_on) {
      if (swflow == SWFLOW_XON_XOFF) {
        if (Serial_write (flow_on) == 1) {
          Serial_send_now ();
          flow_in_on = TRUE;
        }
      }
      if (hwflow == HWFLOW_RTR_CTS) {
        digitalWriteFast (serial_rts_pin, LOW);
        flow_in_on = TRUE;
      }
    }

    receive_buffer[rb_write] = chr;
    if (++rb_write >= SIZE_RECEIVE_BUFFER) rb_write = 0;
    ++rb_count;
  }

  // Send all pending characters.
  if ((!flow_out_on) && ((hwflow == HWFLOW_NONE) || (digitalReadFast (serial_cts_pin) == LOW))) {
    boolean sent = FALSE;
    while (sb_count > 0) {
      byte c = send_buffer[sb_read] & 0x7f;
      if (serial == SERIAL_USB) {
        if (parity == PARITY_ODD) c = odd_parity[c];
        else if (parity == PARITY_EVEN) c = even_parity[c];
      }
      if (Serial_write (c) != 1) break;
      if (++sb_read >= SIZE_SEND_BUFFER) sb_read = 0;
      Increment_counter (&sb_count, -1);
      sent = TRUE;
    }
    if (sent) Serial_send_now ();
  }

  // Print all transferred print strings.
  while (tb_count > 0) {
    if (!Print_string ((const struct print_info *)(transfer_buffer[tb_read]))) break;
    if (++tb_read >= SIZE_TRANSFER_BUFFER) tb_read = 0;
    Increment_counter (&tb_count, -1);
  }

  // For IBM 1620 Jr. - Send ack if requested and previous command's print output has been processed.
  if (emulation == EMULATION_IBM) {
    if (send_ack_IBM) {
      if ((tb_count > 0) || (pb_count > 0)) return;
      if (Serial_write (CHAR_IBM_ACK) != 1) return;
      Serial_send_now ();
      send_ack_IBM = FALSE;
    }
  }

  // Process all received characters.
  while (rb_count > 0) {

    // Make sure there is space in the print buffer.
    if (pb_count >= UPPER_THRESHOLD_PRINT_BUFFER) break;

    // Retrieve next character.
    byte chr = receive_buffer[rb_read];
    if (++rb_read >= SIZE_RECEIVE_BUFFER) rb_read = 0;
    --rb_count;

    // Receive buffer almost empty, turn flow control off.
    if ((rb_count <= LOWER_THRESHOLD_RECEIVE_BUFFER) && flow_in_on) {
      if (swflow == SWFLOW_XON_XOFF) {
        if (Serial_write (flow_off) == 1) {
          Serial_send_now ();
          flow_in_on = FALSE;
        }
      }
      if (hwflow == HWFLOW_RTR_CTS) {
        digitalWriteFast (serial_rts_pin, HIGH);
        flow_in_on = FALSE;
      }
    }

    // Update pending end-of-line conditions.
    if (chr != CHAR_ASCII_CR) pending_cr = FALSE;
    if (chr != CHAR_ASCII_LF) pending_lf = FALSE;
 
    // Process character.
    switch ((*serial_actions)[chr].action) {

      // Common actions.

      case CMD_NONE:
        // Nothing to do.
        break;

      case CMD_PRINT:
        Print_string ((*serial_actions)[chr].print);
        break;

      // IBM 1620 Jr. specific actions.

      case CMD_IBM_MODE_0:
        key_actions = &IBM_ACTIONS_MODE0;
        if (artn_IBM) { Print_string (&WW_PRINT_ARtn);  artn_IBM = FALSE; }   // Turn off A Rtn light.
        if (bold_IBM) { Print_string (&WW_PRINT_Bold);  bold_IBM = FALSE; }   // Turn off Bold light.
        if (!lock_IBM) { Print_string (&WW_PRINT_Lock);  lock_IBM = TRUE;  }  // Turn on Lock light.
        send_ack_IBM = TRUE;
        break;

      case CMD_IBM_MODE_1:
        key_actions = &IBM_ACTIONS_MODE1;
        if (!artn_IBM) { Print_string (&WW_PRINT_ARtn);  artn_IBM = TRUE; }    // Turn on A Rtn light.
        if ((bold == SETTING_TRUE) && !bold_IBM) { Print_string (&WW_PRINT_Bold);  bold_IBM = TRUE; }
                                                                               // Turn on Bold light.
        if (lock_IBM) { Print_string (&WW_PRINT_LShift);  lock_IBM = FALSE; }  // Turn off Lock light.
        send_ack_IBM = TRUE;
        break;

      case CMD_IBM_MODE_2:
        key_actions = &IBM_ACTIONS_MODE2;
        if (artn_IBM) { Print_string (&WW_PRINT_ARtn);  artn_IBM = FALSE; }    // Turn off A Rtn light.
        if ((bold == SETTING_TRUE) && !bold_IBM) { Print_string (&WW_PRINT_Bold);  bold_IBM = TRUE; }
                                                                               // Turn on Bold light.
        if (lock_IBM) { Print_string (&WW_PRINT_LShift);  lock_IBM = FALSE; }  // Turn off Lock light.
        send_ack_IBM = TRUE;
        break;

      case CMD_IBM_MODE_3:
        key_actions = &IBM_ACTIONS_MODE3;
        if (artn_IBM) { Print_string (&WW_PRINT_ARtn);  artn_IBM = FALSE; }    // Turn off A Rtn light.
        if (bold_IBM) { Print_string (&WW_PRINT_Bold);  bold_IBM = FALSE; }    // Turn off Bold light.
        if (lock_IBM) { Print_string (&WW_PRINT_LShift);  lock_IBM = FALSE; }  // Turn off Lock light.
        send_ack_IBM = TRUE;
        break;

      case CMD_IBM_PING:
        send_ack_IBM = TRUE;
        break;

      case CMD_IBM_ACK:
        // Nothing to do.
        break;

      case CMD_IBM_SLASH:
        slash = SETTING_TRUE;
        send_ack_IBM = TRUE;
        break;

      case CMD_IBM_UNSLASH:
        slash = SETTING_FALSE;
        send_ack_IBM = TRUE;
        break;

      case CMD_IBM_RESET:
        key_actions = &IBM_ACTIONS_MODE0;
        shift = FALSE;
        shift_lock = FALSE;
        code = FALSE;
        key_offset = OFFSET_NONE;
        flow_in_on = FALSE;
        flow_out_on = FALSE;
        Set_margins_tabs (TRUE);
        if (artn_IBM) { Print_string (&WW_PRINT_ARtn);  artn_IBM = FALSE; }
        if (bold_IBM) { Print_string (&WW_PRINT_Bold);  bold_IBM = FALSE; }
        if (!lock_IBM) { Print_string (&WW_PRINT_Lock);  lock_IBM = TRUE; }
        send_ack_IBM = TRUE;
        break;

      case CMD_IBM_PAUSE:
        flow_out_on = TRUE;
        break;

      case CMD_IBM_RESUME:
        flow_out_on = FALSE;
        break;

      // ASCII Terminal specific actions.

      case CMD_ASCII_CR:
        if (receiveeol == EOL_CR) {
          Print_string (&WW_PRINT_CRtn);
        } else if (receiveeol == EOL_CRLF) {
          pending_lf = TRUE;
        } else if (receiveeol == EOL_LF) {
          Print_string (&ASCII_PRINT_CR);
        } else /* receiveeol == EOL_LFCR */ {
          if (pending_cr) {
            Print_string (&WW_PRINT_CRtn);
          } else {
            Print_string (&ASCII_PRINT_CR);
          }
        }
        pending_cr = FALSE;
        break;

      case CMD_ASCII_LF:
        if (receiveeol == EOL_CR) {
          Print_string (&ASCII_PRINT_LF);
        } else if (receiveeol == EOL_CRLF) {
          if (pending_lf) {
            Print_string (&WW_PRINT_CRtn);
          } else {
            Print_string (&ASCII_PRINT_LF);
          }
        } else if (receiveeol == EOL_LF) {
          Print_string (&WW_PRINT_CRtn);
        } else /* receiveeol == EOL_LFCR */ {
          pending_cr = TRUE;
        }
        pending_lf = FALSE;
        break;

      case CMD_ASCII_XON:
        flow_out_on = TRUE;
        break;

      case CMD_ASCII_XOFF:
        flow_out_on = FALSE;
        break;

      // Future emulation specific actions.

      default:
        break;
    }
  }

  // Handle IBM 1620 Jr. setup mode.
  if (run_mode == MODE_IBM_BEING_SETUP) {

    if (shift) {  // Reset all settings to factory defaults.
      run_mode = MODE_INITIALIZING;
      Initialize_global_variables ();
      Initialize_configuration_settings (TRUE);
      digitalWriteFast (ORANGE_LED_PIN, LOW);
      digitalWriteFast (BLUE_LED_PIN, blue_led_off);
      if (artn_IBM) Print_string (&WW_PRINT_ARtn);
      if (bold_IBM) Print_string (&WW_PRINT_Bold);
      if (lock_IBM) Print_string (&WW_PRINT_LShift);
      Setup_IBM ();
      run_mode = MODE_RUNNING;
      Print_characters ("---- All settings reset to factory defaults.\r\r");
      Wait_print_buffer_empty ();

    } else {  // Interactive setup.
      if (artn_IBM) Print_string (&WW_PRINT_ARtn);
      if (bold_IBM) Print_string (&WW_PRINT_Bold);
      if (lock_IBM) Print_string (&WW_PRINT_LShift);
      Print_IBM_setup_title ();
      while (TRUE) {
        char cmd = Read_setup_command ();
        if (cmd == 's') {
          Update_IBM_settings ();
        } else if (cmd == 'd') {
          Developer_functions ();
        } else if (cmd == 'e') {
          Print_errors_warnings ();
        } else if (cmd == 'c') {
          Print_IBM_character_set ();
        } else /* cmd == 'q' */ {
          break;
        }
      }
      if (artn_IBM) Print_string (&WW_PRINT_ARtn);
      if (bold_IBM) Print_string (&WW_PRINT_Bold);
      if (lock_IBM) Print_string (&WW_PRINT_Lock);
      key_actions = key_actions_save;
      run_mode = MODE_RUNNING;
    }
  }

  // Handle ASCII Terminal setup mode.
  if (run_mode == MODE_ASCII_BEING_SETUP) {

    if (shift) {  // Reset all settings to factory defaults.
      run_mode = MODE_INITIALIZING;
      Initialize_global_variables ();
      Initialize_configuration_settings (TRUE);
      digitalWriteFast (ORANGE_LED_PIN, LOW);
      digitalWriteFast (BLUE_LED_PIN, blue_led_off);
      Setup_ASCII ();
      run_mode = MODE_RUNNING;
      Print_characters ("---- All settings reset to factory defaults.\r\r");
      Wait_print_buffer_empty ();

    } else {  // Interactive setup.
      Print_ASCII_setup_title ();
      while (TRUE) {
        char cmd = Read_setup_command ();
        if (cmd == 's') {
          Update_ASCII_settings ();
        } else if (cmd == 'd') {
          Developer_functions ();
        } else if (cmd == 'e') {
          Print_errors_warnings ();
        } else if (cmd == 'c') {
          Print_ASCII_character_set ();
        } else /* cmd == 'q' */ {
          break;
        }
      }
      key_actions = key_actions_save;
      run_mode = MODE_RUNNING;
    }
  }

  // Handle future setup mode.
  if (run_mode == MODE_FUTURE_BEING_SETUP) {

    if (shift) {  // Reset all settings to factory defaults.
      run_mode = MODE_INITIALIZING;
      Initialize_global_variables ();
      Initialize_configuration_settings (TRUE);
      digitalWriteFast (ORANGE_LED_PIN, LOW);
      digitalWriteFast (BLUE_LED_PIN, blue_led_off);
      Setup_FUTURE ();
      run_mode = MODE_RUNNING;
      Print_characters ("---- All settings reset to factory defaults.\r\r");
      Wait_print_buffer_empty ();

    } else {  // Interactive setup.
      key_actions = key_actions_save;
      run_mode = MODE_RUNNING;
    }
  }
}


//**********************************************************************************************************************
//
//  Common Interrupt Service Routine.
//
//  This routine is called on the falling edge of each column signal.  A single, common ISR is registered for all column
//  lines.  That routine reads the status of each column line (plus other logic, described below) to determine which
//  triggered the interrupt.  It then calls a column-specific routine to actually process the interrupt.
//
//  - The first step is to clear all row drive lines that may have been asserted during the previous column scan
//    period, to prevent "column spillover" resulting in false printing.  The full clear is done with 8 store
//    instructions.
//
//  - The second step is to assert any appropriate row line required by the current print character.  This, plus step 1
//    processing, must be completed within 265 uSec after the start of the current column scan period.  For maximum
//    performance, the print characters are fully decomposed into column/row pairs stored in the print buffer.
//
//  - The third step is to read each possible row line to detect key presses / releases.  A key is ignored while it is
//    in its bounce shadow.  For each column, only a subset of rows is possible due to the keyboard's wiring.  The first
//    time a key press (or in a few cases release) is seen, an appropriate action is taken.  Actions are taken from a
//    control table based on the typewriter's current mode and the specific key pressed.  Actions can include sending a
//    character to the computer via the USB or RS-232 interface, printing a simple or complex character, and/or special
//    actions specific to the key pressed.  To keep interrupt processing fast, send and print action characters are
//    passed off to the main program to handle.
//
//    The execution of the column scan ISR takes from 3 - 15 microseconds [well within the 265 microseconds limit],
//    depending on how many keys are in that column, how many of them are pressed or released & which ones they are,
//    and what print codes need to be processed.
//
//  Special cases:
//
//  Shifts - Pressing and releasing the <left shift>, <right shift>, Lock, or Code keys do not trigger actions, but the
//           "shift" state is updated.  The shift keys must be held down to cause subsequent keys to be shifted.  The
//           Lock key causes the shift state to be toggled.  When Lock is active, pressing and releasing a shift key
//           will also deactivate Lock.  The Code key functions as a special shift, similar to a ctrl key.  If both a
//           shift and the Code key are depressed, the Code key takes precedence.
//
//  Key bouncing - The Wheelwriter keyboard key bounce is typically less than 20 milliseconds, but could be more a lot
//                 more.  The space key is especially bouncy.  Bounce is treated like a shadow immediately following a
//                 key press / release.  Each key is ignored for a defined amount of time after it is pressed or
//                 released.  If the shadow is too long, then legitimate duplicate presses are ignored.
//
//  N-key rollover - The Wheelwriter keyboard is not electrically capable of true n-key rollover and suffers from
//                   ghosting.  However, the key placement in the scan matrix was designed to minimize problems in
//                   normal use.  For example, the left & right shifts and the Code key are by themselves in their own
//                   columns which makes ghosting easier to detect and deal with.  The firmware deals with the remaining
//                   issues.
//
//  Repeating keys - For the repeating keys (<space>, Backspace, C Rtn, Paper Up, Paper Down, <up arrow>, <down arrow>,
//                   <left arrow>, <right arrow>), if the elapsed time (in milliseconds) between each key press meets or
//                   exceeds a threshold, then it is treated as another press of the same key.
//
//  Scan anomolies - There are several anomolies which affect column scanning.
//
//                   1. The Wheelwriter's logic board usually scans in a rigorous order, but occassionally it
//                      immediately re-scans the same column.  This happens more frequently when multiple keys are
//                      pressed simultaneously, but can even happen when the typewriter is quiet, that is, no keys are
//                      being pressed and no printing is taking place.
//
//                   2. The logic board will also occassionally scan a column out-of-sequence when more than one key is
//                      being pressed.
//
//                   3. The Wheelwriter's keyboard matrix is subject to ghosting.  This happens when two keys are
//                      depressed at the same time and are in the same keyboard row.  For example, if the left shift key
//                      [column 1, row 1] and the 'b' key [column 8, row 1] are depressed together, then when column 1
//                      is scanned, it also looks like column 8 is being scanned.  Likewise, when column 8 is scanned,
//                      it also looks like column 1 is being scanned.  The code of the ISR is able to handle this case
//                      because it is known that column scans always occur in sequence.
//
//                      A more serious form of ghosting can occur when three keys are pressed at the same time and two
//                      of them are in the same row and two are in the same column.  This make it look like another key
//                      [in the other shared row and column combination] is pressed.  Due to the layout of the keyboard
//                      matrix, this problem cannot occur with the shift and code keys, only with other [unnatural] key
//                      combinations such as 'e', 'u', and '3' which ghost the '7' key.  This case is ignored by the
//                      firmware.
//
//**********************************************************************************************************************


void ISR_common () {

  // Do not process column scan interrupts while the firmware is initializing.
  if (run_mode == MODE_INITIALIZING) return;

  // Get the time that the ISR was entered.
  interrupt_time = millis ();

  // Wait for the scan lines to settle down.
  delayMicroseconds (ISR_DELAY);

  // Detect active column(s).
  boolean col_1 = (digitalReadFast (COL_1_PIN) == LOW);
  boolean col_2 = (digitalReadFast (COL_2_PIN) == LOW);
  boolean col_3 = (digitalReadFast (COL_3_PIN) == LOW);
  boolean col_4 = (digitalReadFast (COL_4_PIN) == LOW);
  boolean col_5 = (digitalReadFast (COL_5_PIN) == LOW);
  boolean col_6 = (digitalReadFast (COL_6_PIN) == LOW);
  boolean col_7 = (digitalReadFast (COL_7_PIN) == LOW);
  boolean col_8 = (digitalReadFast (COL_8_PIN) == LOW);
  boolean col_9 = (digitalReadFast (COL_9_PIN) == LOW);
  boolean col_10 = (digitalReadFast (COL_10_PIN) == LOW);
  boolean col_11 = (digitalReadFast (COL_11_PIN) == LOW);
  boolean col_12 = (digitalReadFast (COL_12_PIN) == LOW);
  boolean col_13 = (digitalReadFast (COL_13_PIN) == LOW);
  boolean col_14 = (digitalReadFast (COL_14_PIN) == LOW);

  // Initial sync to column 1.
  if (interrupt_column == WW_COLUMN_NULL) {
    if (digitalReadFast (COL_1_PIN) == HIGH) return;
    interrupt_column = WW_COLUMN_14;
  }

  // Process column 1 scan.
  if (col_1 && !col_2 && ((interrupt_column == WW_COLUMN_14) || (interrupt_column == WW_COLUMN_1))) {
    if (interrupt_column == WW_COLUMN_14) {  // Initial scan assert row, repeat scan leave row asserted.
      interrupt_column = WW_COLUMN_1;
      ISR_column_1 ();
      last_last_scan_duration = last_scan_duration;
      if (last_scan_time != 0UL) last_scan_duration = interrupt_time - last_scan_time;
      last_scan_time = interrupt_time;
      if (last_scan_duration < SHORT_SCAN_DURATION) {
        Report_warning (WARNING_SHORT_SCAN);
      } else if (last_scan_duration > LONG_SCAN_DURATION) {
        Report_warning (WARNING_LONG_SCAN);
      }
    }

  // Process column 2 scan.
  } else if (col_2 && !col_3 && ((interrupt_column == WW_COLUMN_1) || (interrupt_column == WW_COLUMN_2))) {
    if (interrupt_column == WW_COLUMN_1) {  // Initial scan assert row, repeat scan leave row asserted.
       interrupt_column = WW_COLUMN_2;
       ISR_column_2 ();
    }

  // Process column 3 scan.
  } else if (col_3 && !col_4 && ((interrupt_column == WW_COLUMN_2) || (interrupt_column == WW_COLUMN_3))) {
    if (interrupt_column == WW_COLUMN_2) {  // Initial scan assert row, repeat scan leave row asserted.
       interrupt_column = WW_COLUMN_3;
       ISR_column_3 ();
    }

  // Process column 4 scan.
  } else if (col_4 && !col_5 && ((interrupt_column == WW_COLUMN_3) || (interrupt_column == WW_COLUMN_4))) {
    if (interrupt_column == WW_COLUMN_3) {  // Initial scan assert row, repeat scan leave row asserted.
       interrupt_column = WW_COLUMN_4;
       ISR_column_4 ();
    }

  // Process column 5 scan.
  } else if (col_5 && !col_6 && ((interrupt_column == WW_COLUMN_4) || (interrupt_column == WW_COLUMN_5))) {
    if (interrupt_column == WW_COLUMN_4) {  // Initial scan assert row, repeat scan leave row asserted.
       interrupt_column = WW_COLUMN_5;
       ISR_column_5 ();
    }

  // Process column 6 scan.
  } else if (col_6 && !col_7 && ((interrupt_column == WW_COLUMN_5) || (interrupt_column == WW_COLUMN_6))) {
    if (interrupt_column == WW_COLUMN_5) {  // Initial scan assert row, repeat scan leave row asserted.
       interrupt_column = WW_COLUMN_6;
       ISR_column_6 ();
    }

  // Process column 7 scan.
  } else if (col_7 && !col_8 && ((interrupt_column == WW_COLUMN_6) || (interrupt_column == WW_COLUMN_7))) {
    if (interrupt_column == WW_COLUMN_6) {  // Initial scan assert row, repeat scan leave row asserted.
       interrupt_column = WW_COLUMN_7;
       ISR_column_7 ();
    }

  // Process column 8 scan.
  } else if (col_8 && !col_9 && ((interrupt_column == WW_COLUMN_7) || (interrupt_column == WW_COLUMN_8))) {
    if (interrupt_column == WW_COLUMN_7) {  // Initial scan assert row, repeat scan leave row asserted.
       interrupt_column = WW_COLUMN_8;
       ISR_column_8 ();
    }

  // Process column 9 scan.
  } else if (col_9 && !col_10 && ((interrupt_column == WW_COLUMN_8) || (interrupt_column == WW_COLUMN_9))) {
    if (interrupt_column == WW_COLUMN_8) {  // Initial scan assert row, repeat scan leave row asserted.
       interrupt_column = WW_COLUMN_9;
       ISR_column_9 ();
    }

  // Process column 10 scan.
  } else if (col_10 && !col_11 && ((interrupt_column == WW_COLUMN_9) || (interrupt_column == WW_COLUMN_10))) {
    if (interrupt_column == WW_COLUMN_9) {  // Initial scan assert row, repeat scan leave row asserted.
       interrupt_column = WW_COLUMN_10;
       ISR_column_10 ();
    }

  // Process column 11 scan.
  } else if (col_11 && !col_12 && ((interrupt_column == WW_COLUMN_10) || (interrupt_column == WW_COLUMN_11))) {
    if (interrupt_column == WW_COLUMN_10) {  // Initial scan assert row, repeat scan leave row asserted.
       interrupt_column = WW_COLUMN_11;
       ISR_column_11 ();
    }

  // Process column 12 scan.
  } else if (col_12 && !col_13 && ((interrupt_column == WW_COLUMN_11) || (interrupt_column == WW_COLUMN_12))) {
    if (interrupt_column == WW_COLUMN_11) {  // Initial scan assert row, repeat scan leave row asserted.
       interrupt_column = WW_COLUMN_12;
       ISR_column_12 ();
    }

  // Process column 13 scan.
  } else if (col_13 && !col_14 && ((interrupt_column == WW_COLUMN_12) || (interrupt_column == WW_COLUMN_13))) {
    if (interrupt_column == WW_COLUMN_12) {  // Initial scan assert row, repeat scan leave row asserted.
       interrupt_column = WW_COLUMN_13;
       ISR_column_13 ();
    }

  // Process column 14 scan.
  } else if (col_14 && !col_1 && ((interrupt_column == WW_COLUMN_13) || (interrupt_column == WW_COLUMN_14))) {
    if (interrupt_column == WW_COLUMN_13) {  // Initial scan assert row, repeat scan leave row asserted.
       interrupt_column = WW_COLUMN_14;
       ISR_column_14 ();
    }

  // Process out-of-order column 1 (shift) scan.
  } else if (col_1 && !col_2 && (column_1_row_pin != ROW_OUT_NO_PIN)) {
    Clear_all_row_lines ();
    digitalWriteFast (column_1_row_pin, HIGH);

  // Process out-of-order column 5 (code) scan.
  } else if (col_5 && !col_6 && (column_5_row_pin != ROW_OUT_NO_PIN)) {
    Clear_all_row_lines ();
    digitalWriteFast (column_5_row_pin, HIGH);

  // Unexpected column scan.
  } else {
    Clear_all_row_lines ();
    Report_warning (WARNING_UNEXPECTED_SCAN);
  }
}

//
// Column 1 ISR for keys:  <left shift>, <right shift>
//
inline void ISR_column_1 () {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // If the current print character is in this column, assert appropriate row line.
  column_1_row_pin = Test_print (WW_COLUMN_1);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_LShift]) Process_shift_key (ROW_IN_1_PIN, WW_KEY_LShift);
  if (interrupt_time >= key_shadow_times[WW_KEY_RShift]) Process_shift_key (ROW_IN_2_PIN, WW_KEY_RShift);
}

//
// Column 2 ISR for keys:  <right arrow>, <right word>, Paper Down, <down micro>, Paper Up, <up micro>, Reloc, Line
//                         Space, <down arrow>, <down line>
//
inline void ISR_column_2 () {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // If the current print character is in this column, assert appropriate row line.
  (void)Test_print (WW_COLUMN_2);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_RARROW_Word]) Process_repeating_key (ROW_IN_1_PIN, WW_KEY_RARROW_Word);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X33_22]) Process_key (ROW_IN_2_PIN, WW_KEY_X33_22);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X32_23]) Process_key (ROW_IN_3_PIN, WW_KEY_X32_23);
  if (interrupt_time >= key_shadow_times[WW_KEY_PaperDown_Micro]) Process_key (ROW_IN_4_PIN, WW_KEY_PaperDown_Micro);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X31_25]) Process_key (ROW_IN_5_PIN, WW_KEY_X31_25);
  if (interrupt_time >= key_shadow_times[WW_KEY_PaperUp_Micro]) Process_key (ROW_IN_6_PIN, WW_KEY_PaperUp_Micro);
  if (interrupt_time >= key_shadow_times[WW_KEY_Reloc_LineSpace]) Process_key (ROW_IN_7_PIN, WW_KEY_Reloc_LineSpace);
  if (interrupt_time >= key_shadow_times[WW_KEY_DARROW_Line]) Process_repeating_key (ROW_IN_8_PIN, WW_KEY_DARROW_Line);
}

//
// Column 3 ISR for keys:  z, Z, q, Q, Impr, 1, !, Spell, +/-, <degree>, a, A
//
inline void ISR_column_3 () {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // If the current print character is in this column, assert appropriate row line.
  (void)Test_print (WW_COLUMN_3);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_z_Z]) Process_key (ROW_IN_2_PIN, WW_KEY_z_Z);
  if (interrupt_time >= key_shadow_times[WW_KEY_q_Q_Impr]) Process_key (ROW_IN_4_PIN, WW_KEY_q_Q_Impr);
  if (interrupt_time >= key_shadow_times[WW_KEY_1_EXCLAMATION_Spell])
                                                                 Process_key (ROW_IN_5_PIN, WW_KEY_1_EXCLAMATION_Spell);
  if (interrupt_time >= key_shadow_times[WW_KEY_PLUSMINUS_DEGREE]) Process_key (ROW_IN_6_PIN, WW_KEY_PLUSMINUS_DEGREE);
  if (interrupt_time >= key_shadow_times[WW_KEY_a_A]) Process_key (ROW_IN_7_PIN, WW_KEY_a_A);
}

//
// Column 4 ISR for keys:  <space>, <required space>, <load paper>, L Mar, T Clr
//
inline void ISR_column_4 () {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // If the current print character is in this column, assert appropriate row line.
  (void)Test_print (WW_COLUMN_4);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_SPACE_REQSPACE])
                                                            Process_repeating_key (ROW_IN_1_PIN, WW_KEY_SPACE_REQSPACE);
  if (interrupt_time >= key_shadow_times[WW_KEY_LOADPAPER]) Process_key (ROW_IN_2_PIN, WW_KEY_LOADPAPER);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X13_43]) Process_key (ROW_IN_3_PIN, WW_KEY_X13_43);
  if (interrupt_time >= key_shadow_times[WW_KEY_LMar]) Process_key (ROW_IN_4_PIN, WW_KEY_LMar);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X12_45]) Process_key (ROW_IN_5_PIN, WW_KEY_X12_45);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X11_46]) Process_key (ROW_IN_6_PIN, WW_KEY_X11_46);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X14_47]) Process_key (ROW_IN_7_PIN, WW_KEY_X14_47);
  if (interrupt_time >= key_shadow_times[WW_KEY_TClr]) Process_key (ROW_IN_8_PIN, WW_KEY_TClr);
}

//
// Column 5 ISR for keys:  Code
//
inline void ISR_column_5 () {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // If the current print character is in this column, assert appropriate row line.
  column_5_row_pin = Test_print (WW_COLUMN_5);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_Code]) Process_code_key (ROW_IN_1_PIN, WW_KEY_Code);
}

//
// Column 6 ISR for keys:  x, X, <power wise>, w, W, 2, @, Add, s, S
//
inline void ISR_column_6 () {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // If the current print character is in this column, assert appropriate row line.
  (void)Test_print (WW_COLUMN_6);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_x_X_POWERWISE]) Process_key (ROW_IN_2_PIN, WW_KEY_x_X_POWERWISE);
  if (interrupt_time >= key_shadow_times[WW_KEY_w_W]) Process_key (ROW_IN_4_PIN, WW_KEY_w_W);
  if (interrupt_time >= key_shadow_times[WW_KEY_2_AT_Add]) Process_key (ROW_IN_5_PIN, WW_KEY_2_AT_Add);
  if (interrupt_time >= key_shadow_times[WW_KEY_s_S]) Process_key (ROW_IN_7_PIN, WW_KEY_s_S);
}

//
// Column 7 ISR for keys:  c, C, Ctr, e, E, 3, #, Del, d, D, Dec T
//
inline void ISR_column_7 () {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // If the current print character is in this column, assert appropriate row line.
  (void)Test_print (WW_COLUMN_7);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_c_C_Ctr]) Process_key (ROW_IN_2_PIN, WW_KEY_c_C_Ctr);
  if (interrupt_time >= key_shadow_times[WW_KEY_e_E]) Process_key (ROW_IN_4_PIN, WW_KEY_e_E);
  if (interrupt_time >= key_shadow_times[WW_KEY_3_POUND_Del]) Process_key (ROW_IN_5_PIN, WW_KEY_3_POUND_Del);
  if (interrupt_time >= key_shadow_times[WW_KEY_d_D_DecT]) Process_key (ROW_IN_7_PIN, WW_KEY_d_D_DecT);
}

//
// Column 8 ISR for keys:  b, B, Bold, v, V, t, T, r, R, A Rtn, 4, $, Vol, 5, %, f, F, g, G
//
inline void ISR_column_8 () {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // If the current print character is in this column, assert appropriate row line.
  (void)Test_print (WW_COLUMN_8);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_b_B_Bold]) Process_key (ROW_IN_1_PIN, WW_KEY_b_B_Bold);
  if (interrupt_time >= key_shadow_times[WW_KEY_v_V]) Process_key (ROW_IN_2_PIN, WW_KEY_v_V);
  if (interrupt_time >= key_shadow_times[WW_KEY_t_T]) Process_key (ROW_IN_3_PIN, WW_KEY_t_T);
  if (interrupt_time >= key_shadow_times[WW_KEY_r_R_ARtn]) Process_key (ROW_IN_4_PIN, WW_KEY_r_R_ARtn);
  if (interrupt_time >= key_shadow_times[WW_KEY_4_DOLLAR_Vol]) Process_key (ROW_IN_5_PIN, WW_KEY_4_DOLLAR_Vol);
  if (interrupt_time >= key_shadow_times[WW_KEY_5_PERCENT]) Process_key (ROW_IN_6_PIN, WW_KEY_5_PERCENT);
  if (interrupt_time >= key_shadow_times[WW_KEY_f_F]) Process_key (ROW_IN_7_PIN, WW_KEY_f_F);
  if (interrupt_time >= key_shadow_times[WW_KEY_g_G]) Process_key (ROW_IN_8_PIN, WW_KEY_g_G);
}

//
// Column 9 ISR for keys:  n, N, Caps, m, M, y, Y, <1/2 up>, u, U, Cont, 7, &, 6, <cent>, j, J, h, H, <1/2 down>
//
inline void ISR_column_9 () {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // If the current print character is in this column, assert appropriate row line.
  (void)Test_print (WW_COLUMN_9);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_n_N_Caps]) Process_key (ROW_IN_1_PIN, WW_KEY_n_N_Caps);
  if (interrupt_time >= key_shadow_times[WW_KEY_m_M]) Process_key (ROW_IN_2_PIN, WW_KEY_m_M);
  if (interrupt_time >= key_shadow_times[WW_KEY_y_Y_12UP]) Process_key (ROW_IN_3_PIN, WW_KEY_y_Y_12UP);
  if (interrupt_time >= key_shadow_times[WW_KEY_u_U_Cont]) Process_key (ROW_IN_4_PIN, WW_KEY_u_U_Cont);
  if (interrupt_time >= key_shadow_times[WW_KEY_7_AMPERSAND]) Process_key (ROW_IN_5_PIN, WW_KEY_7_AMPERSAND);
  if (interrupt_time >= key_shadow_times[WW_KEY_6_CENT]) Process_key (ROW_IN_6_PIN, WW_KEY_6_CENT);
  if (interrupt_time >= key_shadow_times[WW_KEY_j_J]) Process_key (ROW_IN_7_PIN, WW_KEY_j_J);
  if (interrupt_time >= key_shadow_times[WW_KEY_h_H_12DOWN]) Process_key (ROW_IN_8_PIN, WW_KEY_h_H_12DOWN);
}

//
// Column 10 ISR for keys:  ,, ], [, <superscript 3>, i, I, Word, 8, *, =, +, k, K
//
inline void ISR_column_10 () {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // If the current print character is in this column, assert appropriate row line.
  (void)Test_print (WW_COLUMN_10);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_COMMA_COMMA]) Process_key (ROW_IN_2_PIN, WW_KEY_COMMA_COMMA);
  if (interrupt_time >= key_shadow_times[WW_KEY_RBRACKET_LBRACKET_SUPER3])
                                                            Process_key (ROW_IN_3_PIN, WW_KEY_RBRACKET_LBRACKET_SUPER3);
  if (interrupt_time >= key_shadow_times[WW_KEY_i_I_Word]) Process_key (ROW_IN_4_PIN, WW_KEY_i_I_Word);
  if (interrupt_time >= key_shadow_times[WW_KEY_8_ASTERISK]) Process_key (ROW_IN_5_PIN, WW_KEY_8_ASTERISK);
  if (interrupt_time >= key_shadow_times[WW_KEY_EQUAL_PLUS]) Process_key (ROW_IN_6_PIN, WW_KEY_EQUAL_PLUS);
  if (interrupt_time >= key_shadow_times[WW_KEY_k_K]) Process_key (ROW_IN_7_PIN, WW_KEY_k_K);
}

//
// Column 11 ISR for keys:  ., o, O, R Flsh, 9, (, Stop, l, L, Lang
//
inline void ISR_column_11 () {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // If the current print character is in this column, assert appropriate row line.
  (void)Test_print (WW_COLUMN_11);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_PERIOD_PERIOD]) Process_key (ROW_IN_2_PIN, WW_KEY_PERIOD_PERIOD);
  if (interrupt_time >= key_shadow_times[WW_KEY_o_O_RFlsh]) Process_key (ROW_IN_4_PIN, WW_KEY_o_O_RFlsh);
  if (interrupt_time >= key_shadow_times[WW_KEY_9_LPAREN_Stop]) Process_key (ROW_IN_5_PIN, WW_KEY_9_LPAREN_Stop);
  if (interrupt_time >= key_shadow_times[WW_KEY_l_L_Lang]) Process_key (ROW_IN_7_PIN, WW_KEY_l_L_Lang);
}

//
// Column 12 ISR for keys:  /, ?, <half>, <quarter>, <superscript 2>, p, P, 0, ), -, _, ;, :, <section>, ', ",
//                         <paragraph>
//
inline void ISR_column_12 () {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // If the current print character is in this column, assert appropriate row line.
  (void)Test_print (WW_COLUMN_12);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_SLASH_QUESTION]) Process_key (ROW_IN_1_PIN, WW_KEY_SLASH_QUESTION);
  if (interrupt_time >= key_shadow_times[WW_KEY_HALF_QUARTER_SUPER2])
                                                                 Process_key (ROW_IN_3_PIN, WW_KEY_HALF_QUARTER_SUPER2);
  if (interrupt_time >= key_shadow_times[WW_KEY_p_P]) Process_key (ROW_IN_4_PIN, WW_KEY_p_P);
  if (interrupt_time >= key_shadow_times[WW_KEY_0_RPAREN]) Process_key (ROW_IN_5_PIN, WW_KEY_0_RPAREN);
  if (interrupt_time >= key_shadow_times[WW_KEY_HYPHEN_UNDERSCORE])
                                                                   Process_key (ROW_IN_6_PIN, WW_KEY_HYPHEN_UNDERSCORE);
  if (interrupt_time >= key_shadow_times[WW_KEY_SEMICOLON_COLON_SECTION])
                                                             Process_key (ROW_IN_7_PIN, WW_KEY_SEMICOLON_COLON_SECTION);
  if (interrupt_time >= key_shadow_times[WW_KEY_APOSTROPHE_QUOTE_PARAGRAPH])
                                                          Process_key (ROW_IN_8_PIN, WW_KEY_APOSTROPHE_QUOTE_PARAGRAPH);
}

//
// Column 13 ISR for keys:  <left arrow>, <left word>, <up arrow>, <up line>, Backspace, Bksp 1, C Rtn, Ind Clr
//
inline void ISR_column_13 () {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // If the current print character is in this column, assert appropriate row line.
  (void)Test_print (WW_COLUMN_13);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_LARROW_Word]) Process_repeating_key (ROW_IN_1_PIN, WW_KEY_LARROW_Word);
  if (interrupt_time >= key_shadow_times[WW_KEY_UARROW_Line]) Process_repeating_key (ROW_IN_2_PIN, WW_KEY_UARROW_Line);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X22_134]) Process_key (ROW_IN_4_PIN, WW_KEY_X22_134);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X21_135]) Process_key (ROW_IN_5_PIN, WW_KEY_X21_135);
  if (interrupt_time >= key_shadow_times[WW_KEY_Backspace_Bksp1])
                                                           Process_repeating_key (ROW_IN_6_PIN, WW_KEY_Backspace_Bksp1);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X23_137]) Process_key (ROW_IN_7_PIN, WW_KEY_X23_137);
  if (interrupt_time >= key_shadow_times[WW_KEY_CRtn_IndClr]) Process_return_key (ROW_IN_8_PIN, WW_KEY_CRtn_IndClr);
}

//
// Column 14 ISR for keys:  <back X>, <back word>, Lock, R Mar, Tab, IndL, Mar Rel, RePrt, T Set
//
inline void ISR_column_14 () {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // If the current print character is in this column, assert appropriate row line.
  (void)Test_print (WW_COLUMN_14);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_BACKX_Word]) Process_key (ROW_IN_1_PIN, WW_KEY_BACKX_Word);
  if (interrupt_time >= key_shadow_times[WW_KEY_Lock]) Process_shift_lock_key (ROW_IN_2_PIN, WW_KEY_Lock);
  if (interrupt_time >= key_shadow_times[WW_KEY_RMar]) Process_key (ROW_IN_3_PIN, WW_KEY_RMar);
  if (interrupt_time >= key_shadow_times[WW_KEY_Tab_IndL]) Process_key (ROW_IN_4_PIN, WW_KEY_Tab_IndL);
  if (interrupt_time >= key_shadow_times[WW_KEY_MarRel_RePrt]) Process_key (ROW_IN_6_PIN, WW_KEY_MarRel_RePrt);
  if (interrupt_time >= key_shadow_times[WW_KEY_TSet]) Process_key (ROW_IN_7_PIN, WW_KEY_TSet);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X15_148]) Process_key (ROW_IN_8_PIN, WW_KEY_X15_148);
}


//**********************************************************************************************************************
//
//  Common support routines.
//
//**********************************************************************************************************************

// Initialize global variables.
void Initialize_global_variables () {
  shift = FALSE;
  shift_lock = FALSE;
  code = FALSE;
  key_offset = OFFSET_NONE;

  escaping = FALSE;

  ignore_cr = FALSE;
  ignore_lf = FALSE;
  pending_cr = FALSE;
  pending_lf = FALSE;

  interrupt_time = 0UL;
  last_scan_time = 0UL;
  last_scan_duration = 0UL;
  last_last_scan_duration = 0UL;

  key_actions = NULL;
  key_actions_save = NULL;
  serial_actions = NULL;

  residual_time = 0;

  total_errors = 0;
  total_warnings = 0;

  current_column = 1;
  previous_string = NULL;

  cb_read = 0;
  cb_write = 0;
  cb_count = 0;
  rb_read = 0;
  rb_write = 0;
  rb_count = 0;
  sb_read = 0;
  sb_write = 0;
  sb_count = 0;
  tb_read = 0;
  tb_write = 0;
  tb_count = 0;
  pb_read = 0;
  pb_write = 0;
  pb_count = 0;

  flow_in_on = FALSE;
  flow_out_on = FALSE;

  memset ((void *)error_counts, 0, sizeof(error_counts));
  memset ((void *)warning_counts, 0, sizeof(warning_counts));

  memset ((void *)key_pressed_states, FALSE, sizeof(key_pressed_states));
  memset ((void *)key_repeat_times, 0, sizeof(key_repeat_times));
  memset ((void *)key_shadow_times, 0, sizeof(key_shadow_times));

  memset ((void *)command_buffer, 0, sizeof(command_buffer));
  memset ((void *)receive_buffer, 0, sizeof(receive_buffer));
  memset ((void *)send_buffer, 0, sizeof(send_buffer));
  memset ((void *)transfer_buffer, 0, sizeof(transfer_buffer));
  memset ((void *)print_buffer, 0, sizeof(print_buffer));
}

// Initialize configuration settings.
void Initialize_configuration_settings (boolean reset) {
  byte pemulation;

  // Initialize configuration settings.
  errors = INITIAL_ERRORS;
  warnings = INITIAL_WARNINGS;
  battery = INITIAL_BATTERY;
  lmargin = INITIAL_LMARGIN;
  rmargin = INITIAL_RMARGIN;
  slash = INITIAL_SLASH;
  bold = INITIAL_BOLD;
  serial = INITIAL_SERIAL;
  duplex = INITIAL_DUPLEX;
  baud = INITIAL_BAUD;
  parity = INITIAL_PARITY;
  dps = INITIAL_DPS;
  swflow = INITIAL_SWFLOW;
  hwflow = INITIAL_HWFLOW;
  uppercase = INITIAL_UPPERCASE;
  autoreturn = INITIAL_AUTORETURN;
  transmiteol = INITIAL_TRANSMITEOL;
  receiveeol = INITIAL_RECEIVEEOL;
  ignoreescape = INITIAL_IGNOREESCAPE;
  width = INITIAL_WIDTH;
  offset = INITIAL_OFFSET;

  // Initialize EEPROM and margins & tabs.
  if (reset || (EEPROM.read (EEPROM_FINGERPRINT) != FINGERPRINT)) {
    for (int i = 0; i < SIZE_EEPROM; ++i) {
      Write_EEPROM (i, 0);
    }
    Write_EEPROM (EEPROM_FINGERPRINT, FINGERPRINT);
    Write_EEPROM (EEPROM_VERSION, VERSION);
    Write_EEPROM (EEPROM_ERRORS, errors);
    Write_EEPROM (EEPROM_WARNINGS, warnings);
    Write_EEPROM (EEPROM_BATTERY, battery);
    Write_EEPROM (EEPROM_LMARGIN, lmargin);
    Write_EEPROM (EEPROM_RMARGIN, rmargin);
    Write_EEPROM (EEPROM_EMULATION, emulation);
    Write_EEPROM (EEPROM_SLASH, slash);
    Write_EEPROM (EEPROM_BOLD, bold);
    Write_EEPROM (EEPROM_SERIAL, serial);
    Write_EEPROM (EEPROM_DUPLEX, duplex);
    Write_EEPROM (EEPROM_BAUD, baud);
    Write_EEPROM (EEPROM_PARITY, parity);
    Write_EEPROM (EEPROM_DPS, dps);
    Write_EEPROM (EEPROM_SWFLOW, swflow);
    Write_EEPROM (EEPROM_HWFLOW, hwflow);
    Write_EEPROM (EEPROM_UPPERCASE, uppercase);
    Write_EEPROM (EEPROM_AUTORETURN, autoreturn);
    Write_EEPROM (EEPROM_TRANSMITEOL, transmiteol);
    Write_EEPROM (EEPROM_RECEIVEEOL, receiveeol);
    Write_EEPROM (EEPROM_IGNOREESCAPE, ignoreescape);
    Write_EEPROM (EEPROM_WIDTH, width);
    Write_EEPROM (EEPROM_OFFSET, offset);

    Set_margins_tabs (TRUE);

  // Retrieve configuration parameters and set margins & tabs if needed.
  } else {
    if (Read_EEPROM (EEPROM_VERSION, VERSION) != VERSION) Write_EEPROM (EEPROM_VERSION, VERSION);
    errors = Read_EEPROM (EEPROM_ERRORS, errors);
    warnings = Read_EEPROM (EEPROM_WARNINGS, warnings);
    battery = Read_EEPROM (EEPROM_BATTERY, battery);
    lmargin = Read_EEPROM (EEPROM_LMARGIN, lmargin);
    rmargin = Read_EEPROM (EEPROM_RMARGIN, rmargin);
    pemulation = Read_EEPROM (EEPROM_EMULATION, emulation);
    if (emulation != pemulation) Write_EEPROM (EEPROM_EMULATION, emulation);
    slash = Read_EEPROM (EEPROM_SLASH, slash);
    bold = Read_EEPROM (EEPROM_BOLD, bold);
    serial = Read_EEPROM (EEPROM_SERIAL, serial);
    duplex = Read_EEPROM (EEPROM_DUPLEX, duplex);
    baud = Read_EEPROM (EEPROM_BAUD, baud);
    parity = Read_EEPROM (EEPROM_PARITY, parity);
    dps = Read_EEPROM (EEPROM_DPS, dps);
    swflow = Read_EEPROM (EEPROM_SWFLOW, swflow);
    hwflow = Read_EEPROM (EEPROM_HWFLOW, hwflow);
    uppercase = Read_EEPROM (EEPROM_UPPERCASE, uppercase);
    autoreturn = Read_EEPROM (EEPROM_AUTORETURN, autoreturn);
    transmiteol = Read_EEPROM (EEPROM_TRANSMITEOL, transmiteol);
    receiveeol = Read_EEPROM (EEPROM_RECEIVEEOL, receiveeol);
    ignoreescape = Read_EEPROM (EEPROM_IGNOREESCAPE, ignoreescape);
    width = Read_EEPROM (EEPROM_WIDTH, width);
    offset = Read_EEPROM (EEPROM_OFFSET, offset);

    if (emulation == pemulation) {
      for (int i = 0; i < 166; ++i) {
        tabs[i] = Read_EEPROM (EEPROM_TABS + i, tabs[i]);
      }
      if (battery == SETTING_FALSE) {
        Set_margins_tabs (FALSE);
      }
    } else {
      Set_margins_tabs (TRUE);
    }
  }

  // Adjust serial configuration for validity
  if (serial == SERIAL_USB) {
    hwflow = HWFLOW_NONE;
  } 
}

// Clear all row drive lines.
inline void Clear_all_row_lines () {
  digitalWriteFast (ROW_ENABLE_PIN, HIGH);
  digitalWriteFast (ROW_OUT_1_PIN, LOW);
  digitalWriteFast (ROW_OUT_2_PIN, LOW);
  digitalWriteFast (row_out_3_pin, LOW);
  digitalWriteFast (row_out_4_pin, LOW);
  digitalWriteFast (ROW_OUT_5_PIN, LOW);
  digitalWriteFast (ROW_OUT_6_PIN, LOW);
  digitalWriteFast (ROW_OUT_7_PIN, LOW);
  digitalWriteFast (ROW_OUT_8_PIN, LOW);
}

// Detect and process a key press.
inline void Process_key (int pin, int key) {
  if (digitalReadFast (pin) == LOW) {  // Key pressed?
    if (!key_pressed_states[key]) {  // First time seen?
      key_pressed_states[key] = TRUE;
      key_shadow_times[key] = interrupt_time + KEY_PRESS_DEBOUNCE;
      Take_action (key_offset + key);
    }
  } else if (key_pressed_states[key]) {  // Key just released?
    key_pressed_states[key] = FALSE;
    key_shadow_times[key] = interrupt_time + KEY_RELEASE_DEBOUNCE;
  }
}

// Detect and process a repeating key press.
inline void Process_repeating_key (int pin, int key) {
  if (digitalReadFast (pin) == LOW) {  // Key pressed?
    if (!key_pressed_states[key]) {  // First time seen?
      key_pressed_states[key] = TRUE;
      key_shadow_times[key] = interrupt_time +
                              ((key == WW_KEY_SPACE_REQSPACE) ? KEY_SPACE_DEBOUNCE : KEY_PRESS_DEBOUNCE);
      key_repeat_times[key] = interrupt_time + 5 * KEY_REPEAT;
      Take_action (key_offset + key);
    } else if (interrupt_time >= key_repeat_times[key]) {  // Repeat time elapsed?
      key_repeat_times[key] = interrupt_time + KEY_REPEAT;
      Take_action (key_offset + key);
    }
  } else if (key_pressed_states[key]) {  // Key just released?
    key_pressed_states[key] = FALSE;
    key_shadow_times[key] = interrupt_time + KEY_RELEASE_DEBOUNCE;
  }
}

// Detect and process a return key press.
inline void Process_return_key (int pin, int key) {
  if (digitalReadFast (pin) == LOW) {  // Key pressed?
    if (!key_pressed_states[key]) {  // First time seen?
      key_pressed_states[key] = TRUE;
      key_shadow_times[key] = interrupt_time + KEY_RETURN_DEBOUNCE;
      key_repeat_times[key] = interrupt_time + 5 * KEY_REPEAT;
      if (!key_pressed_states[WW_KEY_TClr]) {
        Take_action (key_offset + key);
      } else {  // Special case for TClr + CRtn = clear all tabs.
        key_repeat_times[key] = 0xffffffffUL;  // Don't allow return to auto repeat.
        Transfer_print_string (&WW_PRINT_ClrAll);
      }
    } else if (interrupt_time >= key_repeat_times[key]) {  // Repeat time elapsed?
      key_repeat_times[key] = interrupt_time + KEY_REPEAT;
      Take_action (key_offset + key);
    }
  } else if (key_pressed_states[key]) {  // Key just released?
    key_pressed_states[key] = FALSE;
    key_shadow_times[key] = interrupt_time + KEY_RELEASE_DEBOUNCE;
  }
}

// Detect and process a shift key press.
inline void Process_shift_key (int pin, int key) {
  if (digitalReadFast (pin) == LOW) {  // Key pressed?
    if (!key_pressed_states[key]) {  // First time seen?
      key_pressed_states[key] = TRUE;
      key_shadow_times[key] = interrupt_time + KEY_PRESS_DEBOUNCE;
      shift = TRUE;
      key_offset = (code ? OFFSET_CODE : OFFSET_SHIFT);
    }
  } else if (key_pressed_states[key]) {  // Key just released?
    key_pressed_states[key] = FALSE;
    key_shadow_times[key] = interrupt_time + KEY_RELEASE_DEBOUNCE;
    shift_lock = FALSE;
    shift = FALSE;
    key_offset = (code ? OFFSET_CODE : OFFSET_NONE);
  }
}

// Detect and process a shift lock key press.
inline void Process_shift_lock_key (int pin, int key) {
  if (digitalReadFast (pin) == LOW) {  // Key pressed?
    if (!key_pressed_states[key]) {  // First time seen?
      key_pressed_states[key] = TRUE;
      key_shadow_times[key] = interrupt_time + KEY_PRESS_DEBOUNCE;
      if (!shift_lock) {  // Shift lock key is a toggle.
        shift_lock = TRUE;
        shift = TRUE;
        key_offset = (code ? OFFSET_CODE : OFFSET_SHIFT);
      } else {
        shift_lock = FALSE;
        shift = FALSE;
        key_offset = (code ? OFFSET_CODE : OFFSET_NONE);
      }
    }
  } else if (key_pressed_states[key]) {  // Key just released?
    key_pressed_states[key] = FALSE;
    key_shadow_times[key] = interrupt_time + KEY_RELEASE_DEBOUNCE;
  }
}

// Detect and process a code key press.
inline void Process_code_key (int pin, int key) {
  if (digitalReadFast (pin) == LOW) {  // Key pressed?
    if (!key_pressed_states[key]) {  // First time seen?
      key_pressed_states[key] = TRUE;
      key_shadow_times[key] = interrupt_time + KEY_PRESS_DEBOUNCE;
      code = TRUE;
      key_offset = OFFSET_CODE;
    }
  } else if (key_pressed_states[key]) {  // Key just released?
    key_pressed_states[key] = FALSE;
    key_shadow_times[key] = interrupt_time + KEY_RELEASE_DEBOUNCE;
    code = FALSE;
    key_offset = (shift ? OFFSET_SHIFT : OFFSET_NONE);
  }
}

// Transfer a print string to main for processing.
inline boolean Transfer_print_string (const struct print_info *str) {
  if (tb_count < SIZE_TRANSFER_BUFFER) {
    transfer_buffer[tb_write] = str;
    if (++tb_write >= SIZE_TRANSFER_BUFFER) tb_write = 0;
    Increment_counter (&tb_count, 1);
    return TRUE;
  } else {  // Print string doesn't fit in transfer buffer.
    Report_error (ERROR_TB_FULL);
    return FALSE;
  }
}

// Read an integer.
int Read_integer (int def) {
  return def;
}

// Print an integer.
boolean Print_integer (int val, int width) {
  int tmp = abs (val);
  int idx = 11;
  char chr[12];
  memset ((void *)chr, ' ', sizeof(chr));
  chr[idx--] = 0x00;
  do {
    chr[idx--] = '0' + (tmp % 10);
    tmp /= 10;
  } while (tmp > 0);
  if (val < 0) chr[idx--] = '-';
  if (width > 0) {
    return Print_characters (&chr[11 - width]);
  } else {
    return Print_characters (&chr[idx + 1]);
  }
}

// Print an unsigned long.
boolean Print_unsigned_long (unsigned long val, int width) {
  unsigned long tmp = val;
  int idx = 11;
  char chr[12];
  memset ((void *)chr, ' ', sizeof(chr));
  chr[idx--] = 0x00;
  do {
    chr[idx--] = '0' + (tmp % 10);
    tmp /= 10;
  } while (tmp > 0UL);
  if (width > 0) {
    return Print_characters (&chr[11 - width]);
  } else {
    return Print_characters (&chr[idx + 1]);
  }
}

// Print a single character.
boolean Print_character (char chr) {
  struct serial_action act = ASCII_SERIAL_ACTIONS[chr & 0x7f];
  if (act.print != NULL) {
    if (!Print_string (act.print)) return FALSE;  // Character doesn't fit in print buffer.
  }
  return TRUE;
}

// Print a string of characters.
boolean Print_characters (const char str[]) {
  const char *ptr = str;
  while (*ptr != 0) {
    struct serial_action act = ASCII_SERIAL_ACTIONS[*(ptr++) & 0x7f];
    if (act.print != NULL) {
      if (!Print_string (act.print)) return FALSE;  // Character doesn't fit in print buffer.
    }
  }
  return TRUE;
}

// Print a string of print codes.
boolean Print_string (const struct print_info *str) {
  int start;
  int pbw;
  int pbc;
  int inc;
  int tab;

  // Inject an automatic carriage return, if enabled, when the right margin is hit and the next character isn't a
  // carriage return, left arrow, right arrow, backspace, bksp 1, set right margin, set tab, clear tab, clear all tabs,
  // or beep.  If the line overflows and automatic return is disabled, then trigger a warning, beep, and ignore the
  // character.
  if ((current_column > rmargin) &&
      (str->string != (const byte (*)[1000])(&WW_STR_CRtn_IndClr)) &&
      (str->string != (const byte (*)[1000])(&WW_STR_LARROW)) &&
      (str->string != (const byte (*)[1000])(&WW_STR_RARROW)) &&
      (str->string != (const byte (*)[1000])(&WW_STR_Backspace)) &&
      (str->string != (const byte (*)[1000])(&WW_STR_Bksp1)) &&
      (str->string != (const byte (*)[1000])(&WW_STR_RMar)) &&
      (str->string != (const byte (*)[1000])(&WW_STR_TSet)) &&
      (str->string != (const byte (*)[1000])(&WW_STR_TClr)) &&
      (str->string != (const byte (*)[1000])(&WW_STR_ClrAll)) &&
      (str->string != (const byte (*)[1000])(&WW_STR_BEEP))) {
    if (autoreturn == SETTING_TRUE) {
      if (!Print_string (&WW_PRINT_CRtn)) return FALSE;  // Carriage return doesn't fit in print buffer.
    } else {
      if (!Print_string (&WW_PRINT_BEEP)) return FALSE;  // Beep doesn't fit in print buffer.
      return TRUE;                                       // Discard string.
    }
  }

  // For IBM 1620 Jr. - Slashed zeroes are controlled by the slash state.
  if (emulation == EMULATION_IBM) {
    if ((str == &WW_PRINT_0) && (slash == SETTING_TRUE)) str = &IBM_PRINT_SLASH_0;
    else if ((str == &IBM_PRINT_SLASH_0) && (slash == SETTING_FALSE)) str = &WW_PRINT_0;
    else if ((str == &IBM_PRINT_FLAG_0) && (slash == SETTING_TRUE)) str = &IBM_PRINT_FLAG_SLASH_0;
    else if ((str == &IBM_PRINT_FLAG_SLASH_0) && (slash == SETTING_FALSE)) str = &IBM_PRINT_FLAG_0;
  }

  // For ASCII Terminal - A space following '\', '{', or '}' needs special handling to print.
  if (emulation == EMULATION_ASCII) {
    if (str == &WW_PRINT_SPACE) {
      if (previous_string == &ASCII_PRINT_BSLASH) {
        str = &ASCII_PRINT_SPACE2;
      } else if ((previous_string == &ASCII_PRINT_LBRACE) || (previous_string == &ASCII_PRINT_RBRACE)) {
        str = &ASCII_PRINT_SPACE3;
      }
    }
    previous_string = str;
  }

  // Prepare for copying.
  start = pb_write;
  pbw = pb_write;
  pbc = pb_count;
  inc = 0;

  // Copy string print codes to buffer.
  for (int i = 0; ; ++i) {
    byte pchr = (*str->string)[i];
    if (pchr == WW_NULL) break;
    if (pbc++ >= (SIZE_PRINT_BUFFER - 1)) {  // String doesn't fit in print buffer.
      Report_error (ERROR_PB_FULL);
      return FALSE;
    }
    if (!pc_validation[pchr]) {
      Report_error (ERROR_BAD_CODE);
      return FALSE;
    }
    ++inc;
    if (++pbw >= SIZE_PRINT_BUFFER) pbw = 0;
    print_buffer[pbw] = pchr;
  }

  // Adjust residual print time.
  if (str->timing >= 0) {
    residual_time += str->timing;
  } else {  // Prorate return time based on current column position.
    residual_time = TIME_HMOVEMENT + (current_column * (- str->timing) / width);
  }

  // Add empty full scans as needed.
  while (residual_time >= FSCAN_0_CHANGES) {
    ++inc;
    if (pbc++ >= (SIZE_PRINT_BUFFER - 1)) {  // String doesn't fit in print buffer.
      Report_error (ERROR_PB_FULL);
      return FALSE;
    }
    if (++pbw >= SIZE_PRINT_BUFFER) pbw = 0;
    print_buffer[pbw] = WW_NULL_14;
    residual_time -= FSCAN_0_CHANGES;
  } 

  // Add null print code after print string.
  ++inc;
  if (pbc++ >= (SIZE_PRINT_BUFFER - 1)) {  // String doesn't fit in print buffer.
    Report_error (ERROR_PB_FULL);
    return FALSE;
  }
  if (++pbw >= SIZE_PRINT_BUFFER) pbw = 0;
  print_buffer[pbw] = WW_NULL;

  // Release copied string for printing.
  pb_write = pbw;
  Increment_counter (&pb_count, inc);
  print_buffer[start] = WW_SKIP;

  // Update current print position, tab stops, and margins.
  switch (str->spacing) {
    case SPACING_NONE:      // No horizontal movement.
      // Nothing to do.
      break;
    case SPACING_FORWARD:   // Forward horizontal movement.
      if (current_column < (rmargin + 1)) ++current_column;
      break;
    case SPACING_BACKWARD:  // Backward horizontal movement.
      if (current_column > 1) --current_column;
      break;
    case SPACING_TAB:       // Tab horizontal movement.
      tab = current_column + 1;
      if (tab > rmargin) tab = rmargin;
      while ((tabs[tab] == SETTING_FALSE) && (tab < rmargin)) ++tab;
      current_column = tab;
      break;
    case SPACING_RETURN:    // Return horizontal movement.
      current_column = lmargin;
      break;
    case SPACING_COLUMN1:   // Column 1 horizontal movement.
      current_column = 1;
      break;
    case SPACING_LMAR:      // No horizontal movement, set left margin.
      lmargin = current_column;
      Write_EEPROM (EEPROM_LMARGIN, lmargin);
      break;
    case SPACING_RMAR:      // No horizontal movement, set right margin.
      rmargin = current_column;
      Write_EEPROM (EEPROM_RMARGIN, rmargin);
      break;
    case SPACING_MARREL:    // No horizontal movement, margin release.
      // Nothing to do.
      break;
    case SPACING_TSET:      // No horizontal movement, set tab.
      tabs[current_column] = SETTING_TRUE;
      Write_EEPROM (EEPROM_TABS + current_column, tabs[current_column]);
      break;
    case SPACING_TCLR:      // No horizontal movement, clear tab.
      tabs[current_column] = SETTING_FALSE;
      Write_EEPROM (EEPROM_TABS + current_column, tabs[current_column]);
      break;
    case SPACING_CLRALL:   // No horizontal movement, clear all tabs.
      memset ((void *)tabs, SETTING_FALSE, sizeof(tabs));
      for (int i = 0; i < 166; ++i) {
        Write_EEPROM (EEPROM_TABS + i, SETTING_FALSE);
      }
      break;
    default:                // Invalid spacing code, ignore.
      break;
  }

  return TRUE;
}

// Test if printing has caught up.
inline boolean Test_printing_caught_up () {
  return ((interrupt_column == WW_COLUMN_1) &&
          (last_scan_duration < LONG_SCAN_DURATION) && (last_last_scan_duration < LONG_SCAN_DURATION));
}

// Wait for the print buffer to be empty.
inline void Wait_print_buffer_empty () {
    while (pb_count > 0) delay (1);
    delay (2 * LONG_SCAN_DURATION);
}

// Test column of current print code and assert row line if match, return output pin.
inline byte Test_print (int column) {
  byte pchr = print_buffer[pb_read];

  // Test if printing has caught up.
  if (pchr == WW_CATCH_UP) {
    if (!Test_printing_caught_up ()) return ROW_OUT_NO_PIN;
    if (++pb_read >= SIZE_PRINT_BUFFER) pb_read = 0;
    pchr = print_buffer[pb_read];
    Increment_counter (&pb_count, -1);
  }

  // Skip any skip print codes.
  while (pchr == WW_SKIP) {
    if (++pb_read >= SIZE_PRINT_BUFFER) pb_read = 0;
    pchr = print_buffer[pb_read];
    Increment_counter (&pb_count, -1);
  }

  // If next print code is in this column, assert associated row line.
  if (((pchr >> 4) & 0x0f) == column) {
    byte pin = row_out_pins[pchr & 0x0f];
    if (pin != ROW_OUT_NO_PIN) {
      digitalWriteFast (pin, HIGH);
      digitalWriteFast (ROW_ENABLE_PIN, LOW);
    }
    if (++pb_read >= SIZE_PRINT_BUFFER) pb_read = 0;
    Increment_counter (&pb_count, -1);
    return pin;
  }

  return ROW_OUT_NO_PIN;
}

// Based on key pressed, take action as given by current key action table.
void Take_action (int key) {
  byte action = (*key_actions)[key].action;

  // Send character if requested.
  if (action & ACTION_SEND) {
    if (sb_count < SIZE_SEND_BUFFER) {
      send_buffer[sb_write] = (*key_actions)[key].send;
      if (++sb_write >= SIZE_SEND_BUFFER) sb_write = 0;
      Increment_counter (&sb_count, 1);
    } else {  // Character doesn't fit in the send buffer.
      Report_error (ERROR_SB_FULL);
    }
  }

  // Print character if requested.
  if (action & ACTION_PRINT) {
    Transfer_print_string ((*key_actions)[key].print);
  }

  // Queue command character if requested.
  if (action & ACTION_COMMAND) {
    if (cb_count < SIZE_COMMAND_BUFFER) {
      command_buffer[cb_write] = (*key_actions)[key].send;
      if (++cb_write >= SIZE_COMMAND_BUFFER) cb_write = 0;
      Increment_counter (&cb_count, 1);
    } else {  // Character doesn't fit in the command buffer.
      Report_error (ERROR_CB_FULL);
    }
  }

  // Take action.
  switch (((*key_actions)[key].action) & ACTION_MASK) {

    // Common actions.

    case ACTION_NONE:                         // No action.
      break;

    // IBM 1620 Jr. specific actions.

    case ACTION_IBM_MODE_0:                   // Set mode 0 - locked.
      key_actions = &IBM_ACTIONS_MODE0;
      if (artn_IBM) { Transfer_print_string (&WW_PRINT_ARtn);  artn_IBM = FALSE; }  // Turn off A Rtn light.
      if (bold_IBM) { Transfer_print_string (&WW_PRINT_Bold);  bold_IBM = FALSE; }  // Turn off Bold light.
      if (!lock_IBM) { Transfer_print_string (&WW_PRINT_Lock);  lock_IBM = TRUE; }  // Turn on Lock light.
      break;

    case ACTION_IBM_MODE_1:                   // Set mode 1 - numeric input.
      key_actions = &IBM_ACTIONS_MODE1;
      if (!artn_IBM) { Transfer_print_string (&WW_PRINT_ARtn);  artn_IBM = TRUE; }    // Turn on A Rtn light.
      if ((bold == SETTING_TRUE) && !bold_IBM) { Transfer_print_string (&WW_PRINT_Bold);  bold_IBM = TRUE; }
                                                                                      // Turn on Bold light.
      if (lock_IBM) { Transfer_print_string (&WW_PRINT_LShift);  lock_IBM = FALSE; }  // Turn off Lock light.
      break;

    case ACTION_IBM_MODE_1F:                  // Set mode 1 - flagged numeric input.
      key_actions = &IBM_ACTIONS_MODE1_FLAG;
      if (!artn_IBM) { Transfer_print_string (&WW_PRINT_ARtn);  artn_IBM = TRUE; }    // Turn on A Rtn light.
      if ((bold == SETTING_TRUE) && !bold_IBM) { Transfer_print_string (&WW_PRINT_Bold);  bold_IBM = TRUE; }
                                                                                      // Turn on Bold light.
      if (lock_IBM) { Transfer_print_string (&WW_PRINT_LShift);  lock_IBM = FALSE; }  // Turn off Lock light.
      break;

    case ACTION_IBM_MODE_2:                   // Set mode 2 - alphameric input.
      key_actions = &IBM_ACTIONS_MODE2;
      if (artn_IBM) { Transfer_print_string (&WW_PRINT_ARtn);  artn_IBM = FALSE; }    // Turn off A Rtn light.
      if ((bold == SETTING_TRUE) && !bold_IBM) { Transfer_print_string (&WW_PRINT_Bold);  bold_IBM = TRUE; }
                                                                                      // Turn on Bold light.
      if (lock_IBM) { Transfer_print_string (&WW_PRINT_LShift);  lock_IBM = FALSE; }  // Turn off Lock light.
      break;

    case ACTION_IBM_MODE_3:                   // Set mode 3 - output.
      key_actions = &IBM_ACTIONS_MODE3;
      if (artn_IBM) { Transfer_print_string (&WW_PRINT_ARtn);  artn_IBM = FALSE; }    // Turn off A Rtn light.
      if (bold_IBM) { Transfer_print_string (&WW_PRINT_Bold);  bold_IBM = FALSE; }    // Turn off Bold light.
      if (lock_IBM) { Transfer_print_string (&WW_PRINT_LShift);  lock_IBM = FALSE; }  // Turn off Lock light.
      break;

    case ACTION_IBM_SETUP:                    // IBM 1620 Jr. setup mode.
      key_actions_save = key_actions;
      key_actions = &IBM_ACTIONS_SETUP;
      run_mode = MODE_IBM_BEING_SETUP;
      break;

    // ASCII Terminal specific actions.

    case ACTION_ASCII_RETURN:                 // ASCII Terminal return action.
      if (transmiteol == EOL_CR) {
        if (sb_count < SIZE_SEND_BUFFER) {
          send_buffer[sb_write] = CHAR_ASCII_CR;
          if (++sb_write >= SIZE_SEND_BUFFER) sb_write = 0;
          Increment_counter (&sb_count, 1);
        } else {  // Character doesn't fit in send buffer.
          Report_error (ERROR_SB_FULL);
        }
      } else if (transmiteol == EOL_CRLF) {
        if (sb_count < (SIZE_SEND_BUFFER - 1)) {
          send_buffer[sb_write] = CHAR_ASCII_CR;
          if (++sb_write >= SIZE_SEND_BUFFER) sb_write = 0;
          Increment_counter (&sb_count, 1);
          send_buffer[sb_write] = CHAR_ASCII_LF;
          if (++sb_write >= SIZE_SEND_BUFFER) sb_write = 0;
          Increment_counter (&sb_count, 1);
        } else {  // Character doesn't fit in send buffer.
          Report_error (ERROR_SB_FULL);
        }
      } else if (transmiteol == EOL_LF) {
        if (sb_count < SIZE_SEND_BUFFER) {
          send_buffer[sb_write] = CHAR_ASCII_LF;
          if (++sb_write >= SIZE_SEND_BUFFER) sb_write = 0;
          Increment_counter (&sb_count, 1);
        } else {  // Character doesn't fit in send buffer.
          Report_error (ERROR_SB_FULL);
        }
      } else /* transmiteol == EOL_LFCR */ {
        if (sb_count < (SIZE_SEND_BUFFER - 1)) {
          send_buffer[sb_write] = CHAR_ASCII_LF;
          if (++sb_write >= SIZE_SEND_BUFFER) sb_write = 0;
          Increment_counter (&sb_count, 1);
          send_buffer[sb_write] = CHAR_ASCII_CR;
          if (++sb_write >= SIZE_SEND_BUFFER) sb_write = 0;
          Increment_counter (&sb_count, 1);
        } else {  // Character doesn't fit in send buffer.
          Report_error (ERROR_SB_FULL);
        }
      }
      if (duplex == DUPLEX_HALF) {
        Transfer_print_string (&WW_PRINT_CRtn);
        if (transmiteol == EOL_CR) ignore_lf = TRUE;
        if (transmiteol == EOL_LF) ignore_cr = TRUE;
      }
      break;

    case ACTION_ASCII_SETUP:                  // ASCII Terminal setup mode.
      key_actions_save = key_actions;
      key_actions = &ASCII_ACTIONS_SETUP;
      run_mode = MODE_ASCII_BEING_SETUP;
      break;

    // Future specific actions.

    case ACTION_FUTURE_SETUP:                 // Future setup mode.
      key_actions_save = key_actions;
      key_actions = &FUTURE_ACTIONS_SETUP;
      run_mode = MODE_FUTURE_BEING_SETUP;
      break;

    default:                                  // Invalid action, ignore.
      break;
  }
}

// Atomically increment a counter.
__attribute__((noinline)) void Increment_counter (volatile int *counter, int increment) {
  int value;
  int result;

  do {
    asm volatile ("ldrex %0, [%1]" : "=r" (value) : "r" (counter) );
    value += increment;
    asm volatile ("strex %0, %2, %1" : "=&r" (result), "=Q" (*counter) : "r" (value));
  } while (result);
}

// Read an EEPROM entry and update if undefined.
inline byte Read_EEPROM (int location, byte value) {
  byte temp = EEPROM.read (location);
  if (temp == 0) {
    EEPROM.write (location, value);
    return value;
  } else {
    return temp;
  }
}

// Write an EEPROM entry.
inline void Write_EEPROM (int location, byte value) {
  EEPROM.write (location, value);
}

// Report an error.
void Report_error (int error) {
  if ((run_mode == MODE_RUNNING) && (errors == SETTING_TRUE) &&
      (error > ERROR_NULL) && (error < NUM_ERRORS)) {
    ++total_errors;
    ++error_counts[error];
    digitalWriteFast (ORANGE_LED_PIN, HIGH);  // Turn on orange LED to indicate error.
  }
}

// Report a warning.
void Report_warning (int warning) {
  if ((run_mode == MODE_RUNNING) && (warnings == SETTING_TRUE) &&
      (warning > WARNING_NULL) && (warning < NUM_WARNINGS)) {
    ++total_warnings;
    ++warning_counts[warning];
    digitalWriteFast (BLUE_LED_PIN, blue_led_on);  // Turn on blue LED to indicate warning.
  }
}

// Set margins and tab stops.
void Set_margins_tabs (boolean reset) {

  // Reset tab stops.
  if (reset) {
    memset ((void *)tabs, SETTING_FALSE, sizeof(tabs));
    if (emulation == EMULATION_ASCII) {
      for (int i = 8; i < 166; i += 8) tabs[i] = SETTING_TRUE;
    }
    tabs[rmargin] = SETTING_TRUE;
    for (int i = 0; i < 166; ++i) {
      Write_EEPROM (EEPROM_TABS + i, tabs[i]);
    }
  }

  // Set left margin.
  Print_string (&WW_PRINT_CRtn);
  for (int i = 0; i < 10; ++i) {
    Print_string (&WW_PRINT_MarRel);
    Print_string (&WW_PRINT_Backspace);
  }
  if ((emulation == EMULATION_ASCII) && (offset == SETTING_TRUE)) Print_string (&WW_PRINT_SPACEX);
  for (int i = 1; i < lmargin; ++i) Print_string (&WW_PRINT_SPACE);
  Print_string (&WW_PRINT_LMar);

  // Set tab stops.
  Print_string (&WW_PRINT_ClrAllX);  // Special case that doesn't clear tabs[].
  for (int i = lmargin; i <= rmargin; ++i) {
    if (tabs[i] == SETTING_TRUE) Print_string (&WW_PRINT_TSet);
    if (i < rmargin) Print_string (&WW_PRINT_SPACE);
  }

  // Set right margin.
  Print_string (&WW_PRINT_RMar);
  Print_string (&WW_PRINT_CRtn);
}

// Read a setup command character.
char Read_setup_command () {
  char cmd;
  boolean seen = FALSE;

  // Print command prompt.
  Print_characters ("Command [settings/errors/character set/QUIT]: ");
  Space_to_column (COLUMN_COMMAND);

  while (TRUE) {  // Keep reading characters until a valid command is seen.

    // Read a command character.
    cmd = Read_setup_character ();

    // Validate and return setup command.
    if ((cmd == 's') || (cmd == 'S')) {
      Print_characters ("settings\r");
      return 's';
    } else if ((cmd == 'd') || (cmd == 'D')) {
      if (seen) {  // Hidden developer option requires typing two d's.
        Print_characters ("developer\r");
        return 'd';
      } else {
        seen = TRUE;
      }
    } else if ((cmd == 'e') || (cmd == 'E')) {
      Print_characters ("errors\r");
      return 'e';
    } else if ((cmd == 'c') || (cmd == 'C')) {
      Print_characters ("character set\r");
      return 'c';
    } else if ((cmd == 'q') || (cmd == 'Q') || (cmd == CHAR_ASCII_CR)) {
      Print_characters ("quit\r\r---- End of Setup\r\r");
      return 'q';
    } else {
      seen = FALSE;
    }
  }
}

// Ask a yes or no question.
boolean Ask_yesno_question (const char str[], boolean value) {
  boolean val;

  // Print question.
  Print_string (&WW_PRINT_SPACE);  Print_string (&WW_PRINT_SPACE);
  Print_characters (str);
  if (value) {
    Print_characters (" [YES/no]? ");
  } else /* !value */ {
    Print_characters (" [yes/NO]? ");
  }
  Space_to_column (COLUMN_QUESTION);

  // Read answer.
  while (TRUE) {
    byte chr = Read_setup_character ();
    if ((chr == 'y') || (chr == 'Y')) {
      val = TRUE;
      break;
    } else if ((chr == 'n') || (chr == 'N')) {
      val = FALSE;
      break;
    } else if (chr == CHAR_ASCII_CR) {
      val = value;
      break;
    }
  }

  // Print answer.
  if (val) {
    Print_characters ("yes\r");
  } else /* !val */ {
    Print_characters ("no\r");
  }

  return val;
}

// Read a true or false setting.
byte Read_truefalse_setting (const char str[], byte value) {
  byte val;

  // Print prompt.
  Print_string (&WW_PRINT_SPACE);  Print_string (&WW_PRINT_SPACE);
  Print_characters (str);
  if (value == SETTING_TRUE) {
    Print_characters (" [TRUE/false]: ");
  } else /* value == SETTING_FALSE */ {
    Print_characters (" [true/FALSE]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read response.
  while (TRUE) {
    byte chr = Read_setup_character ();
    if ((chr == 't') || (chr == 'T')) {
      val = SETTING_TRUE;
      break;
    } else if ((chr == 'f') || (chr == 'F')) {
      val = SETTING_FALSE;
      break;
    } else if (chr == CHAR_ASCII_CR) {
      val = value;
      break;
    }
  }

  // Print response.
  if (val == SETTING_TRUE) {
    Print_characters ("true\r");
  } else /* val == SETTING_FALSE */ {
    Print_characters ("false\r");
  }

  return val;
}

// Read a serial setting.
byte Read_serial_setting (const char str[], byte value) {
  byte val;

  // Print prompt.
  Print_string (&WW_PRINT_SPACE);  Print_string (&WW_PRINT_SPACE);
  Print_characters (str);
  if (value == SERIAL_USB) {
    Print_characters (" [USB/rs232]: ");
  } else /* value == SERIAL_RS232 */ {
    Print_characters (" [usb/RS232]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read response.
  while (TRUE) {
    byte chr = Read_setup_character ();
    if ((chr == 'u') || (chr == 'U')) {
      val = SERIAL_USB;
      break;
    } else if ((chr == 'r') || (chr == 'R')) {
      val = SERIAL_RS232;
      break;
    } else if (chr == CHAR_ASCII_CR) {
      val = value;
      break;
    }
  }

  // Print response.
  if (val == SERIAL_USB) {
    Print_characters ("usb\r");
  } else /* val == SERIAL_RS232 */ {
    Print_characters ("rs232\r");
  }

  return val;
}

// Read a duplex setting.
byte Read_duplex_setting (const char str[], byte value) {
  byte val;

  // Print prompt.
  Print_string (&WW_PRINT_SPACE);  Print_string (&WW_PRINT_SPACE);
  Print_characters (str);
  if (value == DUPLEX_HALF) {
    Print_characters (" [HALF/full]: ");
  } else /* value == DUPLEX_FULL */ {
    Print_characters (" [half/FULL]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read response.
  while (TRUE) {
    byte chr = Read_setup_character ();
    if ((chr == 'h') || (chr == 'H')) {
      val = DUPLEX_HALF;
      break;
    } else if ((chr == 'f') || (chr == 'F')) {
      val = DUPLEX_FULL;
      break;
    } else if (chr == CHAR_ASCII_CR) {
      val = value;
      break;
    }
  }

  // Print response.
  if (val == DUPLEX_HALF) {
    Print_characters ("half\r");
  } else /* val == DUPLEX_FULL */ {
    Print_characters ("full\r");
  }

  return val;
}

// Accept a setup character only from a defined set
byte Read_setup_character_in (const char *charset) {
  while (TRUE) {
    byte chr = Read_setup_character ();
    if (chr != '\0' && strchr (charset, chr)) {
      return chr;
    }
  }
}

// Read a baud setting.
byte Read_baud_setting (const char str[], byte value) {
  byte chr1, chr2, chr3;
  
  // Print prompt.
  Print_string (&WW_PRINT_SPACE);  Print_string (&WW_PRINT_SPACE);
  Print_characters (str);
  Print_characters (" [50-230400, ");  Print_unsigned_long (baud_rates[value], 0);  Print_characters ("]: ");
  Space_to_column (COLUMN_RESPONSE);

  // Read and print response
  chr1 = Read_setup_character_in ("12345679\r");
  if (chr1 == '\r') {
    Print_unsigned_long (baud_rates[value], 0);  Print_character('\r');
    return value;
  }
  Print_character (chr1);
  switch (chr1) {
    case '1':
      chr2 = Read_setup_character_in ("123589");
      Print_character (chr2);
      switch (chr2) {
        case '1':
          chr3 = Read_setup_character_in ("05");
          switch (chr3) {
            case '0':
              Print_characters ("0\r");
              return BAUD_110;
            case '5':
              Print_characters ("5200\r");
              return BAUD_115200;
          }
        case '2':
          Print_characters ("00\r");
          return BAUD_1200;
        case '3':
          Print_characters ("4\r");
          return BAUD_134;
        case '5':
          Print_characters ("0\r");
          return BAUD_150;
        case '8':
          Print_characters ("00\r");
          return BAUD_1800;
        case '9':
          Print_characters ("200\r");
          return BAUD_19200;
      }
    case '2':
      chr2 = Read_setup_character_in ("034");
      switch (chr2) {
        case '0':
          Print_characters ("00\r");
          return BAUD_200;
        case '3':
          Print_characters ("30400\r");
          return BAUD_230400;
        case '4':
          Print_characters ("400\r");
          return BAUD_2400;
      }
    case '3':
      chr2 = Read_setup_character_in ("08");
      switch (chr2) {
        case '0':
          Print_characters ("00\r");
          return BAUD_300;
        case '8':
          Print_characters ("8400\r");
          return BAUD_38400;
      }
    case '4':
      Print_characters ("800\r");
      return BAUD_4800;
    case '5':
      chr2 = Read_setup_character_in ("07");
      switch (chr2) {
        case '0':
          Print_characters ("0\r");
          return BAUD_50;
        case '7':
          Print_characters ("7600\r");
          return BAUD_57600;
      }
    case '6':
      Print_characters ("00\r");
      return BAUD_600;
    case '7':
      chr2 = Read_setup_character_in ("56");
      switch (chr2) {
        case '5':
          Print_characters ("5\r");
          return BAUD_75;
        case '6':
          Print_characters ("6800\r");
          return BAUD_76800;
      }
    case '9':
      Print_characters("600\r");
      return BAUD_9600;
  }
  return value;     // only if we have missed a case above
}

// Read a parity setting.
byte Read_parity_setting (const char str[], byte value) {
  byte val;

  // Print prompt.
  Print_string (&WW_PRINT_SPACE);  Print_string (&WW_PRINT_SPACE);
  Print_characters (str);
  if (value == PARITY_NONE) {
    Print_characters (" [NONE/odd/even]: ");
  } else if (value == PARITY_ODD) {
    Print_characters (" [none/ODD/even]: ");
  } else /* value == PARITY_EVEN */ {
    Print_characters (" [none/odd/EVEN]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read response.
  while (TRUE) {
    byte chr = Read_setup_character ();
    if ((chr == 'n') || (chr == 'N')) {
      val = PARITY_NONE;
      break;
    } else if ((chr == 'o') || (chr == 'O')) {
      val = PARITY_ODD;
      break;
    } else if ((chr == 'e') || (chr == 'E')) {
      val = PARITY_EVEN;
      break;
    } else if (chr == CHAR_ASCII_CR) {
      val = value;
      break;
    }
  }

  // Print response.
  if (val == PARITY_NONE) {
    Print_characters ("none\r");
  } else if (val == PARITY_ODD) {
    Print_characters ("odd\r");
  } else /* val == PARITY_EVEN */ {
    Print_characters ("even\r");
  }

  return val;
}

// Read a databits, parity, stopbits setting.
byte Read_dps_setting (const char str[], byte value) {
  byte val;

  // Print prompt.
  Print_string (&WW_PRINT_SPACE);  Print_string (&WW_PRINT_SPACE);
  Print_characters (str);
  if (value == DPS_7O1) {
    Print_characters (" [7O1/7e1/8n1/8o1/8e1/8n2/8o2/8e2]: ");
  } else if (value == DPS_7E1) {
    Print_characters (" [7o1/7E1/8n1/8o1/8e1/8n2/8o2/8e2]: ");
  } else if (value == DPS_8N1) {
    Print_characters (" [7o1/7e1/8N1/8o1/8e1/8n2/8o2/8e2]: ");
  } else if (value == DPS_8O1) {
    Print_characters (" [7o1/7e1/8n1/8O1/8e1/8n2/8o2/8e2]: ");
  } else if (value == DPS_8E1) {
    Print_characters (" [7o1/7e1/8n1/8o1/8E1/8n2/8o2/8e2]: ");
  } else if (value == DPS_8N2) {
    Print_characters (" [7o1/7e1/8n1/8o1/8e1/8N2/8o2/8e2]: ");
  } else if (value == DPS_8O2) {
    Print_characters (" [7o1/7e1/8n1/8o1/8e1/8n2/8O2/8e2]: ");
  } else /* value == DPS_8E2 */ {
    Print_characters (" [7o1/7e1/8n1/8o1/8e1/8n2/8o2/8E2]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read and print response.
  while (TRUE) {
    byte chr = Read_setup_character ();

    if (chr == '7'){
      Print_character (chr);
      while (TRUE) {
        chr = Read_setup_character ();
        if ((chr == 'o') || (chr == 'O')) {
          val = DPS_7O1;
          Print_characters ("o1\r");
          break;
        } else if ((chr == 'e') || (chr == 'E')) {
          val = DPS_7E1;
          Print_characters ("e1\r");
          break;
        }
      }
      break;

    } else if (chr == '8') {
      Print_character (chr);
      while (TRUE) {
        chr = Read_setup_character ();
        if ((chr == 'n') || (chr == 'N')) {
          Print_character ('n');
          while (TRUE) {
            chr = Read_setup_character ();
            if (chr == '1') {
              val = DPS_8N1;
              Print_characters ("1\r");
              break;
            } else if (chr == '2') {
              val = DPS_8N2;
              Print_characters ("2\r");
              break;
            }
          }
          break;

        } else if ((chr == 'o') || (chr == 'O')) {
          Print_character ('o');
          while (TRUE) {
            chr = Read_setup_character ();
            if (chr == '1') {
              val = DPS_8O1;
              Print_characters ("1\r");
              break;
            } else if (chr == '2') {
              val = DPS_8O2;
              Print_characters ("2\r");
              break;
            }
          }
          break;

        } else if ((chr == 'e') || (chr == 'E')) {
          Print_character ('e');
          while (TRUE) {
            chr = Read_setup_character ();
            if (chr == '1') {
              val = DPS_8E1;
              Print_characters ("1\r");
              break;
            } else if (chr == '2') {
              val = DPS_8E2;
              Print_characters ("2\r");
              break;
            }
          }
          break;
        }
      }
      break;

    } else if (chr == CHAR_ASCII_CR) {
      val = value;
      Print_characters (data_parity_stops_text[val]);
      Print_character ('\r');
      break;
    }
  }

  return val;
}

// Read a software flow control setting.
byte Read_swflow_setting (const char str[], byte value) {
  byte val;

  // Print prompt.
  Print_string (&WW_PRINT_SPACE);  Print_string (&WW_PRINT_SPACE);
  Print_characters (str);
  if (value == SWFLOW_NONE) {
    Print_characters (" [NONE/xon_xoff]: ");
  } else /* value == SWFLOW_XON_XOFF */ {
    Print_characters (" [none/XON_XOFF]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read response.
  while (TRUE) {
    byte chr = Read_setup_character ();
    if ((chr == 'n') || (chr == 'N')) {
      val = SWFLOW_NONE;
      break;
    } else if ((chr == 'x') || (chr == 'X')) {
      val = SWFLOW_XON_XOFF;
      break;
    } else if (chr == CHAR_ASCII_CR) {
      val = value;
      break;
    }
  }

  // Print response.
  if (val == SWFLOW_NONE) {
    Print_characters ("none\r");
  } else /* val == SWFLOW_XON_XOFF */ {
    Print_characters ("xon_xoff\r");
  }

  return val;
}

// Read a hardware flow control setting.
byte Read_hwflow_setting (const char str[], byte value) {
  byte val;

  // Print prompt.
  Print_string (&WW_PRINT_SPACE);  Print_string (&WW_PRINT_SPACE);
  Print_characters (str);
  if (value == HWFLOW_NONE) {
    Print_characters (" [NONE/rts_cts/rtr_cts]: ");
  } else if (value == HWFLOW_RTS_CTS) {
    Print_characters (" [none/RTS_CTS/rtr_cts]: ");
  } else /* value == HWFLOW_RTR_CTS */ {
    Print_characters (" [none/rts_cts/RTR_CTS]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read and print response.
  while (TRUE) {
    byte chr = Read_setup_character ();

    if ((chr == 'n') || (chr == 'N')) {
      val = HWFLOW_NONE;
      Print_characters ("none\r");
      break;

    } else if ((chr == 'r') || (chr == 'R')) {
      Print_characters ("rt");
      while (TRUE) {
        chr = Read_setup_character ();
        if ((chr == 's') || (chr == 'S')) {
          val = HWFLOW_RTS_CTS;
          Print_characters ("s_cts\r");
          break;
        } else if ((chr == 'r') || (chr == 'R')) {
          val = HWFLOW_RTR_CTS;
          Print_characters ("r_cts\r");
          break;
        }
      }
      break;

    } else if (chr == CHAR_ASCII_CR) {
      val = value;
      if (val == HWFLOW_NONE) {
        Print_characters ("none\r");
      } else if (val == HWFLOW_RTS_CTS) {
        Print_characters ("rts_cts\r");
      } else /* val == HWFLOW_RTR_CTS */ {
        Print_characters ("rtr_cts\r");
      }
      break;
    }
  }

  return val;
}

// Read an end-of-line setting.
byte Read_eol_setting (const char str[], byte value) {
  byte val;

  // Print prompt.
  Print_string (&WW_PRINT_SPACE);  Print_string (&WW_PRINT_SPACE);
  Print_characters (str);
  if (value == EOL_CR) {
    Print_characters (" [CR/crlf/lf/lfcr]: ");
  } else if (value == EOL_CRLF) {
    Print_characters (" [cr/CRLF/lf/lfcr]: ");
  } else if (value == EOL_LF) {
    Print_characters (" [cr/crlf/LF/lfcr]: ");
  } else /* value == EOL_LFCR */ {
    Print_characters (" [cr/crlf/lf/LFCR]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read and print response.
  while (TRUE) {
    byte chr = Read_setup_character ();

    if ((chr == 'c') || (chr == 'C')) {
      Print_characters ("cr");
      while (TRUE) {
        chr = Read_setup_character ();
        if ((chr == 'l') || (chr == 'L')) {
          val = EOL_CRLF;
          Print_characters ("lf\r");
          break;
        } else if (chr == CHAR_ASCII_CR) {
          val = EOL_CR;
          Print_characters ("\r");
          break;
        }
      }
      break;

    } else if ((chr == 'l') || (chr == 'L')) {
      Print_characters ("lf");
      while (TRUE) {
        chr = Read_setup_character ();
        if ((chr == 'c') || (chr == 'C')) {
          val = EOL_LFCR;
          Print_characters ("cr\r");
          break;
        } else if (chr == CHAR_ASCII_CR) {
          val = EOL_LF;
          Print_characters ("\r");
          break;
        }
      }
      break;

    } else if (chr == CHAR_ASCII_CR) {
      val = value;
      if (val == EOL_CR) {
        Print_characters ("cr\r");
      } else if (val == EOL_CRLF) {
        Print_characters ("crlf\r");
      } else if (val == EOL_LF) {
        Print_characters ("lf\r");
      } else /* val == EOL_LFCR */ {
        Print_characters ("lfcr\r");
      }
      break;
    }
  }

  return val;
}

// Read a width setting.
byte Read_width_setting (const char str[], byte value) {
  byte val;

  // Print prompt.
  Print_string (&WW_PRINT_SPACE);  Print_string (&WW_PRINT_SPACE);
  Print_characters (str);
  Print_characters (" [80-165, ");  Print_integer (value, 0);  Print_characters ("]: ");
  Space_to_column (COLUMN_RESPONSE);

  // Read and print response.
  while (TRUE) {
    byte chr = Read_setup_character ();

    if ((chr == '8') || (chr == '9')) {
      val = 10 * (chr - '0');
      Print_character (chr);
      while (TRUE) {
        chr = Read_setup_character ();
        if ((chr >= '0') && (chr <= '9')) {
          val += chr - '0';
          Print_character (chr);
          break;
        }
      }
      break;

    } else if (chr == '1') {
      val = 100;
      Print_character (chr);
      while (TRUE) {
        chr = Read_setup_character ();
        if ((chr >= '0') && (chr <= '6')) {
          val += 10 * (chr - '0');
          Print_character (chr);
          break;
        }
      }
      while (TRUE) {
        chr = Read_setup_character ();
        if ((chr >= '0') && (chr <= ((val == 160) ? '5' : '9'))) {
          val += chr - '0';
          Print_character (chr);
          break;
        }
      }
      break;

    } else if (chr == CHAR_ASCII_CR) {
      val = value;
      Print_integer (val, 0);
      break;
    }
  }
  Print_character ('\r');

  return val;
}

// Developer functions.
void Developer_functions () {
  run_mode = MODE_RUNNING;
  Print_string (&WW_PRINT_CRtn);

  while (TRUE) {
    char cmd = Read_developer_function ();
    if (cmd == 'k') {
      Measure_keyboard_bounce ();
    } else if (cmd == 'p') {
      Calibrate_printer_timing ();
    } else if (cmd == 't') {
      Change_typewriter_settings ();
    } else /* cmd == 'e' */ {
      break;
    }
  }

  Print_string (&WW_PRINT_CRtn);
}

// Measure keyboard bounce.
void Measure_keyboard_bounce () {
  Print_characters ("\r    Not implemented yet.\r\r");
}

// Calibrate printer timing.
void Calibrate_printer_timing () {

  // Calibrate basic print strings.
  Calibrate_string ("TIMING_NOSHIFT", &WW_PRINT_a, 77);
  Calibrate_string ("TIMING_SHIFT", &WW_PRINT_A, 77);
  Calibrate_string ("TIMING_CODE", &WW_PRINT_PARAGRAPH, 77);

  // Calibrate stress case alphanumeric string.
  Calibrate_string ("TIMING_NOSHIFT,SHIFT,CODE", &WW_PRINT_QV079, -15);

  // Calibrate IBM 1620 Jr. print strings.
  Calibrate_string ("TIMING_IBM_SLASH_0", &IBM_PRINT_SLASH_0, 77);
  Calibrate_string ("TIMING_IBM_FLAG_SLASH_0", &IBM_PRINT_FLAG_SLASH_0, 77);
  Calibrate_string ("TIMING_IBM_FLAG_DIGIT", &IBM_PRINT_FLAG_0, 77);
  Calibrate_string ("TIMING_IBM_FLAG_NUMBLANK", &IBM_PRINT_FLAG_NUMBLANK, 20);
  Calibrate_string ("TIMING_IBM_RMARK", &IBM_PRINT_RMARK, 20);
  Calibrate_string ("TIMING_IBM_FLAG_RMARK", &IBM_PRINT_FLAG_RMARK, 20);
  Calibrate_string ("TIMING_IBM_GMARK", &IBM_PRINT_GMARK, 20);
  Calibrate_string ("TIMING_IBM_FLAG_GMARK", &IBM_PRINT_FLAG_GMARK, 20);
  Calibrate_string ("TIMING_IBM_RELEASESTART", &IBM_PRINT_RELEASESTART, 10);
  Calibrate_string ("TIMING_IBM_INVALID", &IBM_PRINT_INVALID, 20);

  // Calibrate ASCII Terminal print strings.
  Calibrate_string ("TIMING_ASCII_LESS", &ASCII_PRINT_LESS, 20);
  Calibrate_string ("TIMING_ASCII_GREATER", &ASCII_PRINT_GREATER, 20);
  Calibrate_string ("TIMING_ASCII_BSLASH", &ASCII_PRINT_BSLASH, 20);
  Calibrate_string ("TIMING_ASCII_CARET", &ASCII_PRINT_CARET, 20);
  Calibrate_string ("TIMING_ASCII_BAPOSTROPHE", &ASCII_PRINT_BAPOSTROPHE, 20);
  Calibrate_string ("TIMING_ASCII_LBRACE", &ASCII_PRINT_LBRACE, 20);
  Calibrate_string ("TIMING_ASCII_BAR", &ASCII_PRINT_BAR, 20);
  Calibrate_string ("TIMING_ASCII_RBRACE", &ASCII_PRINT_RBRACE, 20);
  Calibrate_string ("TIMING_ASCII_TILDE", &ASCII_PRINT_TILDE, 10);

  Print_string (&WW_PRINT_CRtn);
}

// Change typewriter settings.
void Change_typewriter_settings () {
  Print_characters ("\r    Not implemented yet.\r\r");
}

// Read a developer function character.
char Read_developer_function () {
  char cmd;

  // Print function prompt.
  Print_characters ("  Function [keyboard/printer/typewriter/EXIT]: ");
  Space_to_column (COLUMN_FUNCTION);

  while (TRUE) {  // Keep reading characters until a valid command is seen.

    // Read a command character.
    cmd = Read_setup_character ();

    // Validate and return setup command.
    if ((cmd == 'k') || (cmd == 'K')) {
      Print_characters ("keyboard\r");
      return 'k';
    } else if ((cmd == 'p') || (cmd == 'P')) {
      Print_characters ("printer\r");
      return 'p';
    } else if ((cmd == 't') || (cmd == 'T')) {
      Print_characters ("typewriter\r");
      return 't';
    } else if ((cmd == 'e') || (cmd == 'E') || (cmd == CHAR_ASCII_CR)) {
      Print_characters ("exit\r");
      return 'e';
    }
  }
}

// Calibrate the print time for a print string.
void Calibrate_string (const char *name, const struct print_info *str, int len) {
  struct print_info temp = *str;
  int base = 0;
  int last = 0;
  int cnt = 0;
  int adj = 0;
  boolean err = FALSE;

  // Calibrate printable strings.
  Print_string (&WW_PRINT_CRtn);
  if (temp.timing >= 0) {

    // Determine timing adjustment.
    base = Measure_string (&temp, len);
    if (base >= 10) {
      err = TRUE;
    } else {
      if (base > 0) {
        while (TRUE) {
          ++adj;
          temp.timing += TIME_ADJUST;
          if (Measure_string (&temp, len) == 0) break;
          if (++cnt >= 20) {
            err = TRUE;
            break;
          }
        }
      }
      if (!err) {
        cnt = 0;
        while (temp.timing > 0) {
          last = temp.timing;
          --adj;
          temp.timing = POSITIVE(temp.timing - TIME_ADJUST);
          if (Measure_string (&temp, len) != 0) {
            ++adj;
            temp.timing = last;
            break;
          }
          if (++cnt >= 20) {
            err = TRUE;
            break;
          }
        }
      }
    }
  }

  // Print timing adjustment.
  Print_characters (name);
  Print_characters (": ");
  Print_integer (adj, 0);
  if (err) {
    Print_characters (", not able to calibrate print string");
  }
  Print_string (&WW_PRINT_CRtn);
  delay (2000);
}

// Measure timing of print string.
int Measure_string (struct print_info *str, int len) {
  byte warn = warnings;
  int wcnt;

  // Initialize variables.
  warnings = SETTING_TRUE;
  warning_counts[WARNING_LONG_SCAN] = 0;
  digitalWriteFast (BLUE_LED_PIN, blue_led_off);

  // Print string.
  if (len >= 0) {
    for (int i = 0; i < len; ++i) Print_string ((const struct print_info *)(str));
  } else {  // Inject stress case alphanumeric string.
    int delta = str->timing - WW_PRINT_QV079.timing;
    struct print_info str_Q = WW_PRINT_Q;
    str_Q.timing += delta;
    struct print_info str_V = WW_PRINT_V;
    str_V.timing += delta;
    struct print_info str_0 = WW_PRINT_0;
    str_0.timing += delta;
    struct print_info str_7 = WW_PRINT_7;
    str_7.timing += delta;
    struct print_info str_9 = WW_PRINT_9;
    str_9.timing += delta;
    for (int i = 0; i > len; --i) {
      Print_string (&str_Q);
      Print_string (&str_V);
      Print_string (&str_0);
      Print_string (&str_7);
      Print_string (&str_9);
    }
  }
  while (pb_read != pb_write) delay (1);
  delay (2000);
  wcnt = warning_counts[WARNING_LONG_SCAN];
  Print_integer (wcnt, 3);
  Print_string (&WW_PRINT_CRtn);
  delay (2000);

  // Restore variables.
  warnings = warn;
  warning_counts[WARNING_LONG_SCAN] = 0;
  digitalWriteFast (BLUE_LED_PIN, blue_led_off);

  return wcnt;
}

// Print errors and warnings.
void Print_errors_warnings () {

  // Handle no errors or warnings.
  if ((total_errors == 0) && (total_warnings == 0)) {
    Print_characters ("\r  No errors or warnings.\r\r");
    return;
  }

  // Print errors.
  Print_string (&WW_PRINT_CRtn);
  if (total_errors > 0) {
    for (int i = 0; i < NUM_ERRORS; ++i) {
      if (error_counts[i] > 0) {
        Print_integer (error_counts[i], 8);
        Print_string (&WW_PRINT_SPACE);
        Print_characters (error_text[i]);
        Print_string (&WW_PRINT_CRtn);
      }
    }
    Print_string (&WW_PRINT_CRtn);
    if (Ask_yesno_question ("Reset errors", FALSE)) {
      total_errors = 0;
      memset ((void *)error_counts, 0, sizeof(error_counts));
      digitalWriteFast (ORANGE_LED_PIN, LOW);
      }
  } else {
    Print_characters ("  No errors.\r");
  }

  // Print warnings.
  Print_string (&WW_PRINT_CRtn);
  if (total_warnings > 0) {
    for (int i = 0; i < NUM_WARNINGS; ++i) {
      if (warning_counts[i] > 0) {
        Print_integer (warning_counts[i], 8);
        Print_string (&WW_PRINT_SPACE);
        Print_characters (warning_text[i]);
        Print_string (&WW_PRINT_CRtn);
      }
    }
    Print_string (&WW_PRINT_CRtn);
    if (Ask_yesno_question ("Reset warnings", FALSE)) {
      total_warnings = 0;
      memset ((void *)warning_counts, 0, sizeof(warning_counts));
      digitalWriteFast (BLUE_LED_PIN, blue_led_off);
    }
  } else {
    Print_characters ("  No warnings.\r");
  }
  Print_string (&WW_PRINT_CRtn);
}

// Read a setup character.
inline byte Read_setup_character () {
  while (cb_count == 0) {
    delay (10);
  }
  byte chr = command_buffer[cb_read];
  if (++cb_read >= SIZE_COMMAND_BUFFER) cb_read = 0;
  Increment_counter (&cb_count, -1);
  return chr;
}

// Space to print column.
void Space_to_column (int col) {
  if ((col >= 1) && (col <= width)) {
    while (current_column < col) {
      Print_string (&WW_PRINT_SPACE);
    }
  }
}

// Open serial communications port.
void Serial_begin () {
  if (serial == SERIAL_USB) {
    Serial.begin (115200);
  } else if (serial == SERIAL_RS232) {
    if (baud >= MINIMUM_HARDWARE_BAUD) {
      Serial1.begin (baud_rates[baud], data_parity_stops[dps]);
      Serial1.clear ();   // workaround; clears FIFO scrambling permitted by driver
      rs232_mode = RS232_UART;
    } else /* baud rate too slow for UART, switch to SlowSoftSerial */ {
      slow_serial_port.begin (baud_rates[baud], data_parity_stops_slow[dps]);
      rs232_mode = RS232_SLOW;
    }
  }
}

// Close serial communications port.
void Serial_end (byte port) {
    if (port == SERIAL_USB) {
    Serial.end ();
  } else /* port == SERIAL_RS232 */ {
    if (rs232_mode == RS232_UART) {
      Serial1.end ();
    } else /* rs232_mode == RS232_SLOW */ {
      slow_serial_port.end ();
    }
  }
}

// Test character available on serial communication port.
inline boolean Serial_available () {
  if (serial == SERIAL_USB) {
    return Serial.available ();
  } else /* serial == SERIAL_RS232 */{
    if (rs232_mode == RS232_UART) {
      return Serial1.available ();
    } else /* rs232_mode == RS232_SLOW */ {
      return slow_serial_port.available ();
    }
  }
}

// Read serial communication port.
inline byte Serial_read () {
  if (serial == SERIAL_USB) {
    return Serial.read ();
  } else /* serial == SERIAL_RS232 */ {
    if (rs232_mode == RS232_UART) {
      return Serial1.read ();

    } else /* rs232_mode == RS232_SLOW */ {
      return slow_serial_port.read ();
    }
  }
}

// Write serial communication port.
inline int Serial_write (byte chr) {
  if (serial == SERIAL_USB) {
    return Serial.write (chr);
  } else /* serial == SERIAL_RS232 */ {
    if (rs232_mode == RS232_UART) {
      if (Serial1.availableForWrite () >= 1) {
        return Serial1.write (chr);
      } else {
        return 0;
      }
    } else /* rs232_mode == RS232_SLOW */ {
      if (slow_serial_port.availableForWrite () >= 1) {
        return slow_serial_port.write (chr);
      } else {
        return 0;
      }
    }
  }
}

// Send now to serial communication port.
inline void Serial_send_now () {
  if (serial == SERIAL_USB) {
    Serial.send_now ();
  } else /* serial == SERIAL_RS232 */ {
    // async serial does not support (or need) send_now.
  }
}

//======================================================================================================================
//
//  End of typewriter firmware.
//
//======================================================================================================================
