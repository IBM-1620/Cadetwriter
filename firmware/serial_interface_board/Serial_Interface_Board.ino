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
//  Typewriter:         IBM/Lexmark Wheelwriter 1000, model 6781-024.
//
//  Interface boards:   WheelWriter USB Interface Board, v1.6, June 2018.  Retired.
//                      WheelWriter USB Interface Board, v1.7, May 2019.  Retired.
//                      WheelWriter Serial Interface Board, v2.0, July 2019.
//
//  Build environment:  Arduino IDE 1.8.13 (https://www.arduino.cc/en/main/software).
//                      Teensyduino 1.53 (https://www.pjrc.com/teensy/td_download.html).
//                      SlowSoftSerial library (https://github.com/MustBeArt/SlowSoftSerial).
//                      Compile options - Teensy 3.5, USB Serial, 120 MHz, Fastest with LTO.
//
//  Memory:             110800 bytes (21%) of program storage space.
//                      109792 bytes (41%) of dynamic memory for global variables.
//
//  Documentation:      IBM 1620 Jr. Console Typewriter Protocol, version 1.10, 5/24/2019.
//                      IBM 1620 Jr. Console Typewriter Test Program, version 1.10, 5/24/2019.
//                      IBM Wheelwriter 1000 by Lexmark User's Guide, P/N 1419147, 7/1994.
//
//  Authors:            Dave Babcock, Joe Fredrick, Paul Williamson (SlowSoftSerial library)
//
//  Copyright:          Copyright(C) 2019-2021 Dave Babcock, Joe Fredrick, Paul Williamson (SlowSoftSerial)
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
//  Revision History:   5R1   5/10/2019  Initial release of rewritten firmware.
//                      5R2   5/25/2019  Corrections to standalone typewriter emulation.
//                                       Removed the printing of the emulation banners.
//                                       Replaced ASCII print characters \, `, <, >, ^, and ~ with period graphics.
//                                       Added more error and warning checking.
//                                       Removed IBM 1620 Jr. initialize command code to be replaced with setup
//                                           function.
//                                       Added ASCII Terminal options - auto LF, width.
//                                       Added IBM 1620 Jr. options - slash zero, bold.
//                      5R3   6/10/2019  Added IBM 1620 Jr. & ASCII Terminal software flow control.
//                                       Replaced ASCII print characters { and } with period graphics.
//                                       Added interactive setup functions for IBM 1620 Jr. & ASCII Terminal.
//                                       Added settings for RS-232 serial, but not support for it yet.
//                      5R4   8/30/2019  Renamed firmware & variables to reflect expanded capability.
//                                       Added support for v2.0 Serial Interface Board.
//                                       Added support for RS-232 serial.
//                                       Added margin, tab, uppercase, and emulation settings to EEPROM.
//                                       Added column offset setting to support column 1 period graphics.
//                                       Updated period graphic characters to correctly print in column 1.
//                                       Added reset all settings to factory defaults operation.
//                                       Expanded end-of-line handling on input and output.
//                                       Added option to ignore escape sequences.
//                                       Made Mar Rel, L Mar, R Mar, T Set, T Clr control functions only.
//                                       Disabled unusable baud rates.
//                                       Cleaned up the printing of some special ASCII characters.
//                                       Added developer function to calibrate print string timings.
//                      5R5   11/8/2019  Added support for semi-automatic paper loading.
//                                       Increased time for ISR delay to deal with overlapping column scans.
//                                       Adjusted timing of unshifted, shifted, and code characters.
//                      5R6  11/12/2019  Corrected set of available baud rates.
//                      5R7  12/29/2019  Removed some unneeded timing dependencies.
//                      5R8   3/11/2020  Adopted the SlowSoftSerial library to allow slow baud rates.
//                                       Fixed corner cases of margin and tab settings.
//                                       Added a beep when an invalid character is typed in setup mode.
//                                       Refactored and cleaned up some code.
//                      5R9   3/21/2020  Expanded the set of invalid setup characters that trigger a beep.
//                                       Improved global counter synchronization comments.
//                      5R10 11/27/2020  Corrected RS-232 hardware flow control.
//                                       Fixed a problem with software flow control.
//                                       Switched sense of flow_in_on and flow_out_on to be more consistent.
//                                       Changed name of paper width setting to line length.
//                                       Changed line offset option to specify number of positions to offset and make it
//                                           apply to all emulations.
//                                       Changed reading of line length and line offset settings to use general-purpose
//                                           integer read.
//                                       Changed definition and handling of escape sequences setting.
//                                       Expanded escape sequences to full ISO/IEC 6429 (ECMA-48, ANSI X3.64) and added
//                                           error for bad escape sequence.
//                                       Added preliminary support for wide-carriage and 20cps Wheelwriters.
//                                           Warning:  The 20cps timing isn't calibrated yet.  No characters are lost,
//                                                     but the firmware drives the print mechanism too fast and there is
//                                                     lots of beeping.
//                      6R0   4/14/2021  Developed new Spin-Hit-Move (SHM) printer timing model.
//                                       Added developer column scan timing measurements and rewrote printer timing.
//                                       Added configuration setting for ASCII printwheel (#1353909).
//                                       Added configuration setting for impression control.
//                                       Added capability to change typewriter settings.
//                                       Added serial input parity checking.
//                                       Added POST to check setup(), loop(), and ISR().
//                                       Updated timings for 20cps print mechanism from Chris Coley.
//                                       Miscellaneous changes and fixes for minor issues.
//
//  Future ideas:       1. Support other Wheelwriter models.
//
//                      2. Extend the developer feature to include keyboard measurements.
//
//                      3. Implement a subset of ANSI Escape Sequences that make sense for a printing terminal.
//
//                      4. Add other terminal emulations such as EBCDIC, APL, DecWriter, etc.
//
//                      5. Add a means to use USB memory sticks as paper tape devices to allow the emulation of
//                         terminals such as an ASR-33 and Flexowriter.
//
//                      6. Add pagination support for individual sheets of paper or fan-fold paper.
//
//                      7. Add a keyboard key which aborts current printout and flushes print & input buffers.
//
//                      8. Translate common Unicode characters (e.g., curly quotes) to nearest ASCII equivalents.
//
//                      9. Add stand-alone, built-in BASIC and/or FORTH interpreters.
//
//======================================================================================================================

#include <limits.h>
#include <EEPROM.h>
#include <SlowSoftSerial.h>

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
//                *  Common global variables are all lowercase, complete words, and separated by underscores
//                   (print_buffer).
//                *  Function names are mixed uppercase / lowercase, complete words, separated by underscores, and begin
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
//                    The interrupt system does not allow the main program (i.e. loop()) to execute while in an ISR.
//                    Therefore, classic semaphores and mutexes cannot be used to guard critical code segments in the
//                    main program and ISRs.  If the main program were to hold a semaphore, an ISR could never acquire
//                    it, deadlocking the program.  This firmware is designed so that all data accesses and updates are
//                    carefully sequenced or never conflict or have independent read / write pointers or are
//                    synchronized.  All global, non-constant data is declared volatile, so every read and write
//                    accesses memory and are not cached.
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
//                 There are two exceptions to these column scan times:
//                 *  If the typewriter's character buffer is full, then the current column scan will be lengthened up
//                    to 3.5 seconds until printing fully catches up.
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
//                        |               | /          \  |          |<set toform> |      | <power wise> |  Ctr  |
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
//                        |               |     Line      |          | <clear all> |      |              |       |
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
//                        | A Rtn |    Cont    |   Word    | R Flsh |             | /         \  |   Ind L   |
//                    ----+-------+------------+-----------+--------+-------------+--------------+-----------+
//                      5 |   4   |     7      |     8     |   9    |      0      | \         /  |           |
//                        |   $   |     &      |     *     |   (    |      )      |  [X21_135]   |           |
//                        |  Vol  |            |           |  Stop  |             | /         \  |           |
//                    ----+-------+------------+-----------+--------+-------------+--------------+-----------+
//                      6 |   5   |     6      |     =     |        |      -      |  Backspace   |  Mar Rel  |
//                        |   %   |   <cent>   |     +     |        |      _      |              |           |
//                        |       |            |           |        |             |    Bksp 1    |  Re Prt   |
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
//  Spin-Hit-Move - As of version 6R0, the firmware implements a new, more accurate model - Spin-Hit-Move (SHM) - for
//                  predicting character print times.  It reflects the actual mechanics of the printer mechanism where
//                  the printwheel first spins to the correct position, then the print hammer hits the character
//                  leaving an image on the paper, and finally the carriage moves one position to the right.  The hit
//                  and move times are fixed and don't vary by character.  The spin time is the variable component.
//                  Once a character is printed, the printwheel usually stays in the same position, so spin time depends
//                  on where the new character to be printed is relative to the last character printed.  The printwheel
//                  can turn clockwise or counter-clockwise, so the typewriter motherboard chooses the rotation
//                  direction which takes the least time.  For each character to be printed, the firmware calculates the
//                  spin time based on the location on the printwheel of the previous and current character.
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
//             There are four special print codes used in the print buffer - null (value 0x00), count (0xfd), catch_up
//             (0xfe), and skip (value 0xff).  The null print code marks the current end of things to print in the
//             print buffer.  The null value effectively stops the ISR from advancing through the print buffer since it
//             doesn't match any of the valid column values.  When seen during any column scan, skip print codes are
//             ignored and the print buffer is advanced to the next print code.  Catch_up is used to block advancing
//             through the print buffer until the motherboard has fully read the last print code.  Count increments the
//             count of characters printed and is only used when timing the print mechanism.
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
//             quickly.  When it is partialy full, the typewriter beeps three times.  When it is full, the logic board
//             stretches the current column scan pulse by as much as 3.5 seconds, stalling "keyboard" input until the
//             print mechanism fully catches up.  This is a safe thing to do as a stand-alone typewriter, but as a
//             computer terminal it can cause loss of input data from the actual keyboard.  This firmware avoids filling
//             the typewriter's buffer by carefully balancing the time it takes to print a character and the total
//             column scan time that it takes to assert the row drive lines for all of the character's print codes.
//             Null full scans are added after every character's print codes to match it's total print time.
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
//             *  A type code which describes the type of character it is - simple, complex, dynamic.
//             *  A spacing code which indicates how this character affects the horizontal position on the line.
//             *  A timing value which is the difference between the time it takes to physically print this character
//                (under normal conditions) and the time it takes to assert all of its print codes.
//             *  A pointer to the character's print data.  This can be one of three addresses:
//                -  For simple characters, it points to a null-terminated array of bytes containing the scan codes to
//                   assert.
//                -  For complex characters, it points to an array of pointers to other print information structures.
//                   Complex characters can be nested to any level.
//                -  For dynamic characters, it points to a pointer which points to a print information structure.  This
//                   is used for characters which have alternate print sequences depending on runtime settings.
//
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
//                logic board seems to deal with these cases correctly.
//
//             Here are the print codes, timing, and scan schedule for printing an 'm':
//               Print codes:  WW_m_M, WW_NULL_9, WW_NULL_14, WW_NULL
//               Timing:  TIME_SPIN + TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_9 + TIME_RELEASE_NOSHIFT_9)
//               Scan schedule:
//                           C1    C2    C3    C4    C5    C6    C7    C8    C9    C10   C11   C12   C13   C14
//                                                                            m
//                                                                          NULL9                         NULL14
//                         ----------------------------- null pad as needed ----------------------------- NULL14  NULL
// 
//             Here are the print codes, timing, and scan schedule for printing an 'M':
//               Print codes:  WW_LShift, WW_m_M, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL;
//               Timing:  TIME_SPIN + TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_9 + TIME_RELEASE_SHIFT_9)
//               Scan schedule:
//                           C1    C2    C3    C4    C5    C6    C7    C8    C9    C10   C11   C12   C13   C14
//                         LShift                                             M
//                         LShift                                          (null9)
//                         NULL1                                                                          NULL14
//                         ----------------------------- null pad as needed ----------------------------- NULL14  NULL
//
//             The IBM 1620's record mark character is a composite character made up of a sequence of other print
//             characters:
//               Print characters:  WW_PRINT_EQUAL, WW_PRINT_Backspace, WW_PRINT_EXCLAMATION, WW_PRINT_Backspace,
//                                  WW_PRINT_i, NULL_PRINT_INFO
//               Timing:  TIME_SPIN + TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_10 + TIME_RELEASE_NOSHIFT_10) +
//                        TIME_HMOVE - (TIME_PRESS_NOSHIFT_13 + TIME_RELEASE_NOSHIFT_13) +
//                        TIME_SPIN + TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_3 + TIME_RELEASE_SHIFT_3) +
//                        TIME_HMOVE - (TIME_PRESS_NOSHIFT_13 + TIME_RELEASE_NOSHIFT_13) +
//                        TIME_SPIN + TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_10 + TIME_RELEASE_NOSHIFT_10)
//               Scan schedule:
//                           C1    C2    C3    C4    C5    C6    C7    C8    C9    C10   C11   C12   C13   C14
//                                                                                  =
//                                                                               NULL10                   NULL14
//                         ----------------------------- null pad as needed ----------------------------- NULL14
//                                                                                                 Backsp
//                                                                                                 NULL13 NULL14
//                         ----------------------------- null pad as needed ----------------------------- NULL14
//                         LShift         !
//                         LShift      (null3)
//                         NULL1                                                                          NULL14
//                         ----------------------------- null pad as needed ----------------------------- NULL14
//                                                                                                 Backsp
//                                                                                                 NULL13 NULL14
//                         ----------------------------- null pad as needed ----------------------------- NULL14
//                                                                                  i
//                                                                               NULL10                   NULL14
//                         ----------------------------- null pad as needed ----------------------------- NULL14  NULL
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
//                    Note:  1.5 years after period graphics were created, a special, limited-edition IBM printwheel
//                           (#1353909) was found that contains the entire printable ASCII character set.  Using this
//                           wheel, all characters print cleanly at full speed and period graphic characters aren't
//                           needed.  A configuration setting controls whether native or period graphic ASCII characters
//                           are printed.
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
//                    The third limitation is the speed of printing period graphic characters.  Depending on the
//                    complexity of the character, it can take up to 10x print time for all of the carriage movements
//                    and printed periods.  Furthermore, the Wheelwriter's motherboard injects additional column scan
//                    delays when multiple micro-column movements are used.  Why is not known.
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
//                    automatically moved one full character position to the right and usually has to be moved back to
//                    the character being printed.  For example, here are the period graphic versions of the less than,
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
//                         micro-columns 1 and 2, the carriage needs to move to the left.  This is prevented when the
//                         carriage is in the virtual column 1 position.  So, a MarRel is needed to allow a backspace or
//                         bksp1 to move left into micro-columns 1 and 2.  If the carriage is in the physical column 1
//                         position, then it is up against a mechanical stop and there is no way to move into
//                         micro-columns 1 and 2.  To deal with this, the firmware supports a line offset setting, which
//                         is 1 by default, that relocates all columns one print position to the right.  Note that the
//                         column marker on the front of the typewriter will be off by one column, but otherwise
//                         everything will work correctly.
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
//               *  Tab array[200] setting (TRUE, FALSE) used by all emulations.  Initial value:
//                                                                                IBM 1620 Jr. = all FALSE,
//                                                                                ASCII Terminal = TRUE every 8 columns.
//               *  Column offset setting (0 - 9) used by all emulations.  Initial value = 0 (IBM), 1 (ASCII).
//               *  ASCII printwheel setting (TRUE, FALSE) used by all emulations.  Initial value = FALSE;
//               *  Impression setting (LIGHT, NORMAL, HEAVY) used by all emulations.  Initial value = NORMAL.
//               *  Slashed zeroes setting (TRUE, FALSE) of the IBM 1620 Jr. Console Typewriter.  Initial value = TRUE.
//               *  Print bold input setting (TRUE, FALSE) of the IBM 1620 Jr. Console Typewriter.  Initial value =
//                  FALSE.
//               *  Serial setting (USB, RS232) of the ASCII Terminal.  Initial value = USB.
//               *  Duplex setting (FULL, HALF) of the ASCII Terminal.  Initial value = FULL.
//               *  Baud setting (50 - 230400) of the ASCII Terminal (RS-232 only).  Initial value = 9600.
//               *  Parity setting (NONE, ODD, EVEN) of the ASCII Terminal (USB only).  Initial value = NONE.
//               *  Databits, parity, stopbits setting (7O1, 7E1, 8N1, 8O1, 8E1, 8N2, 8O2, 8E2) of the ASCII Terminal
//                  (RS-232 only).  Initial value = 8N1.
//               *  Software flow control setting (NONE, XON_XOFF) of the ASCII Terminal.  Initial value = XON_XOFF.
//               *  Hardware flow control setting (NONE, RTS_CTS, RTR_CTS) of the ASCII Terminal.  Initial value = NONE.
//               *  Uppercase only setting (TRUE, FALSE) of the ASCII Terminal.  Initial value = FALSE.
//               *  Auto return setting (TRUE, FALSE) of the ASCII Terminal.  Initial value = TRUE.
//               *  Transmit EOL setting (EOL_CR, EOL_CRLF, EOL_LF, EOL_LFCR) of the ASCII TERMINAL.
//                  Initial value = EOL_CR.
//               *  Receive EOL setting (EOL_CR, EOL_CRLF, EOL_LF, EOL_LFCR) of the ASCII TERMINAL.
//                  Initial value = EOL_CRLF.
//               *  Escape sequences setting (NONE, IGNORE).  Initial value = IGNORE.
//               *  Line length setting (80 - 165) of the ASCII Terminal.  For wide-carriage Wheelwriters, the range is
//                  80 - 198.  Initial value = 80.
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
//                     margin.  It then spaces (length - 1) positions to the right [the typewriter never stops at the
//                     right margin, it only beeps when getting close] and sets the right margin.  It is most likely
//                     that the margins were already set to 1 and length, but these startup actions will guarantee it.
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
//                   "SetUp" keys on the keyboard.  The "SetUp" key is the upper-rightmost key, labelled "Paper Up
//                   Micro" on an unmodified Wheelwriter.  The commands available in Setup are:
//                   *  settings - This command allows operating parameters of the typewriter and/or emulation to be
//                                 changed, Settings are viewed and changed interactively.  For each setting, the
//                                 options are presented and the current value is in uppercase.  Typing the first
//                                 letter of an option selects it, pressing the carriage return retains the current
//                                 value.  There are a few exceptions when more than one character must be typed to
//                                 uniquely identify the option value, such as the ASCII Terminal's RS-232 baud rate
//                                 setting which can require two or three digits be typed.  New settings pertain only
//                                 to the current session unless explicitly saved.
//                   *  errors - This prints counts of all errors and warnings that have occurred since the last reset.
//                               The user can optionally reset the error and warning counts to 0.
//                   *  character set - This causes the emulator's full character set to be printed.
//                   *  typewriter - This allows the user to change built-in typewriter options & features.  Many of the
//                                   typewriter features are toggles with no definitive way to turn them off.  Some of
//                                   them, like underline, are automatically off when the typewriter is turned on.
//                                   Others, line spell check and bold, are preserved during power off.  The firmware
//                                   cannot tell whether these features are on or off and need user intervention to
//                                   set/clear them.  A few must be manually turned off for correct Cadetwriter
//                                   operation.  A few typewriter options - line spacing and powerwise - might be useful
//                                   to the user in some situations.  Finally, there are options for setting and
//                                   clearing margins and tabs.  The available options are:
//                                   1. spell check - This toggles the typewriter's spell check option every time a "1"
//                                                    is pressed.  When on, the feature sounds double beeps when it
//                                                    thinks a word is misspelled, which happens a lot with computer
//                                                    output.  It should be turned off.  If the typewriter beeps after
//                                                    pressing "1", then the feature was just turned on.  If the
//                                                    printwheel jiggles, then spell check was just turned off.
//                                   2. line spacing - This changes the typewriter's line spacing every time a "2" is
//                                                     pressed.  It varies from 1 to 1.5 to 2 to 3 and back to 1.  The
//                                                     current spacing is shown by indicator lights.
//                                   3. powerwise - This changes the typewriter's powerwise setting.  It can be set to
//                                                  0 (for off) or 1 - 90 minutes to control how long the typewriter
//                                                  waits before partially powering down due to inactivity.  After
//                                                  pressing "3", the firmware asks for a number between 0 and 90, ended
//                                                  with a return.
//                                   4. a rtn - This toggles the typewriter's automatic carriage return setting every
//                                              time a "4" is pressed.  The feature's current state is shown by the
//                                              "A Rtn" indicator.  This option must be turned off for correct
//                                              Cadetwriter operation.
//                                   5. bold - This toggles whether characters are printed bolded.  Its current state is
//                                             shown by the "Bold" indicator.  It does not affect correct Cadetwriter
//                                             operation.
//                                   6. change margins & tabs - This option allows the user to position the left margin,
//                                                              right margin, and all tab stops.  When "6" is pressed,
//                                                              the firmware prints directions, column numbers, and the
//                                                              current placement of the margins and tabs.  The user can
//                                                              then place the margins and tabs at new positions.
//                                                              Pressing the carriage return ends the operation.
//                                   7. restore margins & tabs - This option resets all margins and tabs to their
//                                                               "default" settings - left margin at column 1, right
//                                                               margin at "line length" column, and tabs at every 8
//                                                               positions.
//                                   q. quit - This ends typewriter changes and returns to main setup menu.
//                   *  quit - This ends Setup mode.
//
//                   There is also a "restore to factory defaults" option in all emulations.  It is triggered by
//                   pressing either of the Shift keys with the Code/Ctrl key and "Setup" key on the keyboard.
//
//  RS-232 serial - The Teensy 3.5 microcontroller includes UART hardware support for 6 RS-232 interfaces.  The Serial
//                  Interface Board uses the first RS-232 port (UART0) for host computer communication.  There are
//                  limitations to the baud rates supported.  The following table gives the available baud rates and
//                  their error.  Only baud rates with an error of less than 2.5% are usable.  In addition, the MAX3232
//                  serial line driver/receiver cannot handle baud rates greater than 250000.  So, only baud rates 1200
//                  thru 230400 can be used with the hardware UART.
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
//                 the serial data on the same GPIO pins.
//
//                 The firmware supports hardware flow control (RTS/CTS and RTR/CTS) as well as software flow control
//                 (XON/XOFF).  The difference between RTS/CTS and RTR/CTS is the handling of the DE-9 connector’s pin
//                 #7.  In older RS-232 specifications, pin #7 was RTS [Request To Send] which was used by the DTE to
//                 request permission to send data to the DCE.  As of the 1991 RS-232-E specification, RTS is assumed
//                 to always be asserted and pin #7 is now RTR [Ready To Receive] which the DTE uses to signal that it
//                 is able to receive data from the DCE.  Pin #8 has always been CTS [Clear To Send] used to signal that
//                 the DTE can send data to the DCE.  Many vendors incorrectly state their interface uses RTS when in
//                 fact it uses RTR, including the Teensy 3.5 documentation.  With true RTS, Cadetwriter cannot stop
//                 the computer from overwhelming it with data to print using hardware flow control.
//
//                 HW Flow Setting       RTS/RTR (pin 7) DTE -> DCE        CTS (pin 8) DTE <- DCE  
//                 ------------------------------------------------------------------------------
//                      none                     enabled                           ignored                 
//                     rts_cts            enabled if data to send           if enabled, send data   
//                     rtr_cts        enabled if ready to receive data      if enabled, send data   
// 
//  Escape sequences - The ISO/IEC 6429 (ECMA-48, ANSI X3.64) standard defines escape sequences (a series of ASCII
//                     characters starting with ESCape) which can be sent to a terminal.  Most of these only make sense
//                     for display screen terminals, but a few could also apply to printing terminals (setting/clearing
//                     margins & tabs and carriage movement).  A host computer may mistakenly send escape sequences to
//                     Cadetwriter.  If the escape sequences setting is set to NONE, then the coded characters in
//                     the sequence itself will be printed.  If the setting is IGNORE, then the entire escape sequence
//                     will be silently discarded.  The firmware using a Finite-State Machine (FSM) to properly
//                     recognize all of the standard-defined escape sequences.  The sequences fall into these groups:
//
//                     Type      Escape   Opener   Parameters   Intermediates         Final
//                               (1)      (0-1)    (0-n)        (0-n)                 (1)
//                     -----------------------------------------------------------------------------------------
//                     CSI       <esc>    [        0-9:;<=>?    <sp>!"#$%&'()*+,-./   @A-Z[\]^_`a-z{|}~
//                               (0x1B)   (0x5B)   (0x30-0x3F)  (0x20-0x2F)           (0x40-0x7E)
//
//                     Function  <esc>                          <sp>!"#$%&'()*+,-./   0-9:;<=>?@A-Z[\]^_`a-z{|}~
//                               (0x1B)                         (0x20-0x2F)           (0x30-0x7E)
//
//                     String    <esc>    PX]^_    <bs><tab><lf><vt><ff><cr><sp>!"#   <esc> \  -or-  <bel>
//                                                 $^&'()*+,-./0-9:;<=>?@A-Z[\]^_`
//                                                 a-z{|}~
//                               (0x1B)   (0x50)   (0x08-0x0D, 0x20-0x7E)             (0x1B 0x5C) -or- (0x07)
//                                        (0x58)
//                                        (0x5D-
//                                         0x5F)
//
//                     Special cases:
//                     *  NUL (0x00) and DEL (0x7F) are always ignored.
//                     *  CAN (0x18) and SUB (0x1A) immediately abort the sequence.
//                     *  All control characters (0x01 - 0x1F) except CAN (0x18), SUB (0x1A), and ESC (0x1B) anywhere,
//                        except in String bodies, perform their normal action.
//                     *  P (0x50), X (0x58), ] (0x5D), ^ (0x5E), and _ (0x5F) cannot be used to terminate Function
//                        sequences unless they contain at least one Intermediate character.
//                     *  Any ESC (0x1B) after the first, except in Strings, immediately aborts the current sequence and
//                        starts another one.
//
//  Developer functions - To aid developers who wish to adapt other model Wheelwriter typewriters, there is a "hidden"
//                        set of developer tests built into this firmware.  The tests are accessed from the Setup menu
//                        by pressing code/control-d.  The available functions are:
//                        *  buffer - This test tries to determine the Wheelwriter's input buffer size.  It prints a
//                                    line as long as needed to cause the buffer to completely fill.  If the buffer is
//                                    larger than 32 characters and/or the Wheelwriter has a 20cps printer, then the
//                                    printed line could extend past the line length setting.  To protect the platen,
//                                    extra paper should be loaded to fully cover the platen.  It may not be possible to
//                                    measure the buffer's size if it is too large.  When converting a new Wheelwriter,
//                                    this should be the first test run.
//                        *  scan - This automatically measures the typewriter's column scanning timing.  This includes
//                                  the time of "null" column scans, the column scan times for a pressed & released key,
//                                  and the total time for flooding the typewriter with a full line of worst-case print
//                                  characters.  These test results help determine the correct values for the CSCAN_*,
//                                  FSCAN_*, and *_SCAN_DURATION constants.  To date, all model Wheelwriters adapted
//                                  have had consistent 820/3680 microsecond column timing.  This should be the second
//                                  test run.
//                        *  print - This test automatically measures all of the basic timing components of the
//                                   Wheelwriter's printer mechanism.  This includes the time of "null" character
//                                   printing, the Spin/Hit/Move timing values, the tab & return timing values, the
//                                   times for horizontal & vertical movements, and the timing of miscellaneous
//                                   typewriter operations such as beep & jiggle.  These test results help determine the
//                                   correct values for the TIME_* constants.  This should be the third test run.
//                        *  measure - Once all of the typewriter's timing values have been measured, the associated
//                                     program timing constants set, and the updated firmware compiled & uploaded to the
//                                     typewriter, then this test can be run.  It should be the last test run.  It
//                                     prints two blocks of text - Jabberwocky and 10 full lines of random characters -
//                                     twice and measures the effective print speed, in characters-per-second (cps),
//                                     for each block.  The first pair of text blocks is printed using the calibrated
//                                     timing constants.  If all of them are set correctly, the typewriter should print
//                                     close to it's maximum speed, have no pauses, produce no warnings/errors, and
//                                     trigger no triple beeps.  If there are any triple beeps, the the timing constants
//                                     are not set correctly.  The second pair of text blocks is printed at the maximum
//                                     speed the printer is capable of, ignoring all timing constants.  This will
//                                     trigger lots of triple beeps, but is a gauge of how accurate the timing values
//                                     are.
//                        *  comm - This test will check the communication with a remote computer by echoing all
//                                  characters received and sending all characters typed.  This is not implemented yet.
//                        *  keyboard - This will allow the developer to measure physical keyboard timing values (key
//                                      press, key release, n-key rollover, key bounce, etc.) and check the operation
//                                      and column/row scan codes of all keyboard keys.  These test results help
//                                      determine the correct values for the KEY_*_DEBOUNCE constants.  This is not
//                                      implemented yet.
//                     
//  Board revisions - This firmware automatically supports all revisions of the Serial Interface Board.
//                    1.6: The first production board.  The basic board has a hardware problem with random characters
//                         being printed and random carriage movements when new firmware is downloaded and at power on.
//                         To correct the problem, two pull-down resistor packs need to be added to the underneath side
//                         of the board.  It directly supports the USB interface and an RS-232 interface via an external
//                         level shifter.  This board is identified by BLUE_LED_PIN == HIGH.  Setting the ROW_ENABLE_PIN
//                         has no effect.  This board was retired with the release of the v1.7 board.
//                    1.7: The second production board.  It resolves the random characters problem a different way and 
//                         requires no pull-down resistors.  It directly supports the USB interface and an RS-232
//                         interface via an external level shifter.  This board is identified by BLUE_LED_PIN == HIGH.
//                         It requires that the ROW_ENABLE_PIN be set LOW to enable fake key presses to be seen by the
//                         logic board.  This board was retired with the release of the v2.0 board.
//                    2.0: The latest production board.  This board directly supports both the USB and RS-232 serial
//                         interfaces.  It also has Hardware support for a 15th keyboard column scan line.  This board
//                         is identified by BLUE_LED_PIN == LOW.  It requires that the ROW_ENABLE_PIN be set LOW to
//                         enable fake key presses to be seen by the logic board.
//
//  Wheelwriter models - The Cadetwriter Serial Interface Board and firmware were developed using a Wheelwriter 1000,
//                       but have been successfully used with other model Wheelwriters, both single-board and dual-board
//                       varities. To date, no changes to the Serial Interface Board have been needed.  However, the
//                       location of the board does change depending on the space available in the typewriter.  Minor
//                       firmware changes are needed when the Wheelwriter has a wide carriage and/or faster print speed.
//                       These changes are controlled by a set of FEATURE_* defines below.
//
//======================================================================================================================



//======================================================================================================================
//
//  Common data and code used by all emulations.
//
//======================================================================================================================

// Logic values.
#define TRUE   1  // Boolean true.
#define FALSE  0  // Boolean false.


//**********************************************************************************************************************
//
//  Wheelwriter feature definitions.
//
//**********************************************************************************************************************

// Wheelwriter 1000
#define FEATURE_20CPS           FALSE
#define FEATURE_WIDE_CARRIAGE   FALSE
#define FEATURE_BUFFER_SIZE     32
#define FEATURE_VERSION_SUFFIX  ""

// Wheelwriter 1500
// #define FEATURE_20CPS           TRUE
// #define FEATURE_WIDE_CARRIAGE   TRUE
// #define FEATURE_BUFFER_SIZE     32
// #define FEATURE_VERSION_SUFFIX  "_1500"

// Wheelwriter 5
// #define FEATURE_20CPS           TRUE
// #define FEATURE_WIDE_CARRIAGE   TRUE
// #define FEATURE_BUFFER_SIZE     32    // ??
// #define FEATURE_VERSION_SUFFIX  "_5"

// Wheelwriter 30 Series II
// #define FEATURE_20CPS           TRUE
// #define FEATURE_WIDE_CARRIAGE   TRUE
// #define FEATURE_BUFFER_SIZE     64       // ?? 
// #define FEATURE_VERSION_SUFFIX  "_30II"


//**********************************************************************************************************************
//
//  Global definitions.
//
//**********************************************************************************************************************

// Firmware version values.
#define VERSION       61
#define VERSION_TEXT  "6R0" FEATURE_VERSION_SUFFIX

#define VERSION_ESCAPEOFFSET_CHANGED  60  // Version when escape sequences and line offset changed.

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
#define WW_COLUMN_14    0x0e  // <back X>, <back word>, Lock, R Mar, Tab, Ind L, Mar Rel, Re Prt, T Set

// Wheelwriter row values.
#define WW_ROW_NULL  0x00  // Null row.
#define WW_ROW_1     0x01  // <left shift>, <right arrow>, <right word>, <space>, <required space>, Code, b, B, Bold, n,
                           // N, Caps, /, ?, <left arrow>, <left word>, <back X>, <back word>
#define WW_ROW_2     0x02  // <right shift>, z, Z, <load paper>, x, X, <power wise>, c, C, Ctr, v, V, m, M, ,, .,
                           // <up arrow>, <up line>, Lock
#define WW_ROW_3     0x03  // t, T, y, Y, <1/2 up>, ], [, <superscript 3>, <half>, <quarter>, <superscript 2>, R Mar
#define WW_ROW_4     0x04  // Paper Down, <down micro>, q, Q, Impr, L Mar, w, W, e, E, r, R, A Rtn, u, U, Cont, i, I,
                           // Word, o, O, R Flsh, p, P, Tab, Ind L
#define WW_ROW_5     0x05  // 1, !, Spell, 2, @, Add, 3, #, Del, 4, $, Vol, 7, &, 8, *, 9, (, Stop, 0, )
#define WW_ROW_6     0x06  // Paper Up, <up micro>, +/-, <degree>, 5, %, 6, <cent>, =, +, -, _, Backspace, Bksp 1, Mar
                           // Rel, Re Prt
#define WW_ROW_7     0x07  // Reloc, Line Space, a, A, s, S, d, D, Dec T, f, F, j, J, k, K, l, L, Lang, ;, :, <section>,
                           // T Set
#define WW_ROW_8     0x08  // <down arrow>, <down line>, T Clr, g, G, h, H, <1/2 down>, ', ", <paragraph>, C Rtn,
                           // Ind Clr

// Wheelwriter key values.
#define WW_KEY_LShift                       0  // <left shift> key code.
#define WW_KEY_RShift                       1  // <right shift> key code.
#define WW_KEY_RIGHTARROW_RightWord         2  // <right arrow>, <right word> key code.
#define WW_KEY_X33_22                       3  // *** not available on Wheelwriter 1000.
#define WW_KEY_X32_23                       4  // *** not available on Wheelwriter 1000.
#define WW_KEY_PaperDown_DownMicro          5  // Paper Down, <down micro> key code.
#define WW_KEY_X31_25                       6  // *** not available on Wheelwriter 1000.
#define WW_KEY_PaperUp_UpMicro              7  // Paper Up, <up micro> key code.
#define WW_KEY_Reloc_LineSpace              8  // Reloc, Line Space key code.
#define WW_KEY_DOWNARROW_DownLine           9  // <down arrow>, <down line> key code.
#define WW_KEY_z_Z                         10  // z, Z key code.
#define WW_KEY_q_Q_Impr                    11  // q, Q, Impr key code.
#define WW_KEY_1_EXCLAMATION_Spell         12  // 1, !, Spell key code.
#define WW_KEY_PLUSMINUS_DEGREE            13  // +/-, <degree> key code.
#define WW_KEY_a_A                         14  // a, A key code.
#define WW_KEY_SPACE_REQSPACE              15  // <space>, <required space> key code.
#define WW_KEY_LOADPAPER_SETTOPOFFORM      16  // <load paper>, <set top of form> key code.
#define WW_KEY_X13_43                      17  // *** not available on Wheelwriter 1000.
#define WW_KEY_LMar                        18  // L Mar key code.
#define WW_KEY_X12_45                      19  // *** not available on Wheelwriter 1000.
#define WW_KEY_X11_46                      20  // *** not available on Wheelwriter 1000.
#define WW_KEY_X14_47                      21  // *** not available on Wheelwriter 1000.
#define WW_KEY_TClr_TClrAll                22  // T Clr, T Clr All key code.
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
#define WW_KEY_LEFTARROW_LeftWord          65  // <left arrow>, <left word> key code.
#define WW_KEY_UPARROW_UpLine              66  // <up arrow>, <up line> key code.
#define WW_KEY_X22_134                     67  // *** not available on Wheelwriter 1000.
#define WW_KEY_X21_135                     68  // *** not available on Wheelwriter 1000.
#define WW_KEY_Backspace_Bksp1             69  // Backspace, Bksp 1 key code.
#define WW_KEY_X23_137                     70  // *** not available on Wheelwriter 1000.
#define WW_KEY_CRtn_IndClr                 71  // C Rtn, IndClr key code.
#define WW_KEY_BACKX_Word                  72  // <back X>, <back word> key code.
#define WW_KEY_Lock                        73  // Lock key code.
#define WW_KEY_RMar                        74  // R Mar key code.
#define WW_KEY_Tab_IndL                    75  // Tab key code.
#define WW_KEY_MarRel_RePrt                76  // Mar Rel, Re Prt key code.
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
#define WW_COUNT     0xfd                 // Count character when timing printer.
#define WW_CATCH_UP  0xfe                 // Wait for printing to catch up.
#define WW_SKIP      0xff                 // Skip print code.

// Wheelwriter print codes.
#define WW_LShift                      (WW_NULL_1 | WW_ROW_1)   // <left shift> print code.
#define WW_RShift                      (WW_NULL_1 | WW_ROW_2)   // <right shift> print code.
#define WW_RIGHTARROW_RightWord        (WW_NULL_2 | WW_ROW_1)   // <right arrow>, <right word> print code.
#define WW_PaperDown_DownMicro         (WW_NULL_2 | WW_ROW_4)   // Paper Down, <down micro> print code.
#define WW_PaperUp_UpMicro             (WW_NULL_2 | WW_ROW_6)   // Paper Up, <up micro> print code.
#define WW_Reloc_LineSpace             (WW_NULL_2 | WW_ROW_7)   // Reloc, Line Space print code.
#define WW_DOWNARROW_DownLine          (WW_NULL_2 | WW_ROW_8)   // <down arrow>, <down line> print code.
#define WW_z_Z                         (WW_NULL_3 | WW_ROW_2)   // z, Z print code.
#define WW_q_Q_Impr                    (WW_NULL_3 | WW_ROW_4)   // q, Q, Impr print code.
#define WW_1_EXCLAMATION_Spell         (WW_NULL_3 | WW_ROW_5)   // 1, !, Spell print code.
#define WW_PLUSMINUS_DEGREE            (WW_NULL_3 | WW_ROW_6)   // +/-, <degree> print code.
#define WW_BAPOSTROPHE_APW             (WW_NULL_3 | WW_ROW_6)   // ` print code (ASCII PrintWheel).
#define WW_TILDE_APW                   (WW_NULL_3 | WW_ROW_6)   // ~ print code (ASCII PrintWheel).
#define WW_a_A                         (WW_NULL_3 | WW_ROW_7)   // a, A print code.
#define WW_SPACE_REQSPACE              (WW_NULL_4 | WW_ROW_1)   // <space>, <required space> print code.
#define WW_LOADPAPER_SETTOPOFFORM      (WW_NULL_4 | WW_ROW_2)   // <load paper>, <set top of form> print code.
#define WW_LMar                        (WW_NULL_4 | WW_ROW_4)   // L Mar print code.
#define WW_TClr_TClrAll                (WW_NULL_4 | WW_ROW_8)   // T Clr, T Clr All print code.
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
#define WW_CARET_APW                   (WW_NULL_9 | WW_ROW_6)   // ^ print code (ASCII PrintWheel).
#define WW_j_J                         (WW_NULL_9 | WW_ROW_7)   // j, J print code.
#define WW_h_H_12DOWN                  (WW_NULL_9 | WW_ROW_8)   // h, H, <1/2 down> print code.
#define WW_COMMA_COMMA                 (WW_NULL_10 | WW_ROW_2)  // ,, , print code.
#define WW_RBRACKET_LBRACKET_SUPER3    (WW_NULL_10 | WW_ROW_3)  // ], [, <superscript 3> print code.
#define WW_BSLASH_APW                  (WW_NULL_10 | WW_ROW_3)  // \ print code (ASCII PrintWheel).
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
#define WW_RBRACE_APW                  (WW_NULL_12 | WW_ROW_3)  // } print code (ASCII PrintWheel).
#define WW_LBRACE_APW                  (WW_NULL_12 | WW_ROW_3)  // { print code (ASCII PrintWheel).
#define WW_BAR_APW                     (WW_NULL_12 | WW_ROW_3)  // | print code (ASCII PrintWheel).
#define WW_p_P                         (WW_NULL_12 | WW_ROW_4)  // p, P print code.
#define WW_0_RPAREN                    (WW_NULL_12 | WW_ROW_5)  // 0, ) print code.
#define WW_HYPHEN_UNDERSCORE           (WW_NULL_12 | WW_ROW_6)  // -, _ print code.
#define WW_SEMICOLON_COLON_SECTION     (WW_NULL_12 | WW_ROW_7)  // :, :, <section> print code.
#define WW_LESS_APW                    (WW_NULL_12 | WW_ROW_7)  // < print code (ASCII PrintWheel).
#define WW_APOSTROPHE_QUOTE_PARAGRAPH  (WW_NULL_12 | WW_ROW_8)  // ', ", <paragraph> print code.
#define WW_GREATER_APW                 (WW_NULL_12 | WW_ROW_8)  // > print code (ASCII PrintWheel).
#define WW_LEFTARROW_LeftWord          (WW_NULL_13 | WW_ROW_1)  // <left arrow>, <left word> print code.
#define WW_UPARROW_UpLine              (WW_NULL_13 | WW_ROW_2)  // <up arrow>, <up line> print code.
#define WW_Backspace_Bksp1             (WW_NULL_13 | WW_ROW_6)  // Backspace, Bksp 1 print code.
#define WW_CRtn_IndClr                 (WW_NULL_13 | WW_ROW_8)  // C Rtn, Ind Clr print code.
#define WW_BACKX_Word                  (WW_NULL_14 | WW_ROW_1)  // <back X>, <back word> print code.
#define WW_Lock                        (WW_NULL_14 | WW_ROW_2)  // Lock print code.
#define WW_RMar                        (WW_NULL_14 | WW_ROW_3)  // R Mar print code.
#define WW_Tab_IndL                    (WW_NULL_14 | WW_ROW_4)  // Tab print code.
#define WW_MarRel_RePrt                (WW_NULL_14 | WW_ROW_6)  // Mar Rel, Re Prt print code.
#define WW_TSet                        (WW_NULL_14 | WW_ROW_7)  // T Set print code.

// Keyboard key offset values.
#define OFFSET_NONE   0                            // Not shifted.
#define OFFSET_SHIFT  (NUM_WW_KEYS)                // Shifted.
#define OFFSET_CODE   (NUM_WW_KEYS + NUM_WW_KEYS)  // Code shifted.

// Keyboard scan timing values (in msec).
#define SHORT_SCAN_DURATION  10UL  // Threshold time for Wheelwriter short scans.
#define LONG_SCAN_DURATION   22UL  // Threshold time for Wheelwriter long scans.

// Keyboard scan timing values (in usec).
#define CSCAN_MINIMUM     100                                      // Minimum time for column scan to be counted.
#define CSCAN_NOCHANGE    820                                      // Time for column scan with no key change.
#define CSCAN_CHANGE     3680                                      // Time for column scan with a key change.
#define FSCAN_0_CHANGES  (14 * CSCAN_NOCHANGE)                     // Time for full scan with 0 key changes.
#define FSCAN_1_CHANGE   (13 * CSCAN_NOCHANGE +     CSCAN_CHANGE)  // Time for full scan with 1 key change.
#define FSCAN_2_CHANGES  (12 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)  // Time for full scan with 2 key changes.
#define FSCAN_3_CHANGES  (11 * CSCAN_NOCHANGE + 3 * CSCAN_CHANGE)  // Time for full scan with 3 key changes.

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
#define ACTION_MASK          0xf0      // Action mask.

#define ACTION_IBM_MODE_0    (1 << 4)  // IBM 1620 Jr. Mode 0 action.
#define ACTION_IBM_MODE_1    (2 << 4)  // IBM 1620 Jr. Mode 1 action.
#define ACTION_IBM_MODE_1F   (3 << 4)  // IBM 1620 Jr. Mode 1 flag action.
#define ACTION_IBM_MODE_2    (4 << 4)  // IBM 1620 Jr. Mode 2 action.
#define ACTION_IBM_MODE_3    (5 << 4)  // IBM 1620 Jr. Mode 3 action.
#define ACTION_IBM_SETUP     (6 << 4)  // IBM 1620 Jr. setup action.

#define ACTION_ASCII_RETURN  (7 << 4)  // ASCII Terminal return action.
#define ACTION_ASCII_SETUP   (8 << 4)  // ASCII Terminal setup action.

#define ACTION_FUTURE_SETUP  (9 << 4)  // Future setup action.

// Serial input actions.
#define CMD_NONE           0  // No action.
#define CMD_PRINT          1  // Print character action.

#define CMD_IBM_MODE_0    10  // Set mode 0 IBM 1620 Jr. action.
#define CMD_IBM_MODE_1    11  // Set mode 1 IBM 1620 Jr. action.
#define CMD_IBM_MODE_2    12  // Set mode 2 IBM 1620 Jr. action.
#define CMD_IBM_MODE_3    13  // Set mode 3 IBM 1620 Jr. action.
#define CMD_IBM_PING      14  // Ping IBM 1620 Jr. action.
#define CMD_IBM_ACK       15  // Ack IBM 1620 Jr. action.
#define CMD_IBM_SLASH     16  // Slash zeroes IBM 1620 Jr. action.
#define CMD_IBM_UNSLASH   17  // Unslash zeroes IBM 1620 Jr. action.
#define CMD_IBM_RESET     18  // Reset IBM 1620 Jr. action.
#define CMD_IBM_PAUSE     19  // Pause IBM 1620 Jr. action.
#define CMD_IBM_RESUME    20  // Resume IBM 1620 Jr. action.

#define CMD_ASCII_CR      30  // CR action.
#define CMD_ASCII_LF      31  // LF action.
#define CMD_ASCII_XON     32  // XON ASCII Terminal action.
#define CMD_ASCII_XOFF    33  // XOFF ASCII Terminal action.

// Run mode values.
#define MODE_INITIALIZING        0  // Typewriter is initializing.
#define MODE_RUNNING             1  // Typewriter is running.
#define MODE_IBM_BEING_SETUP     2  // IBM 1620 Jr. is being setup.
#define MODE_ASCII_BEING_SETUP   3  // ASCII Terminal is being setup.
#define MODE_FUTURE_BEING_SETUP  4  // Future is being setup.

// Print element values.
#define ELEMENT_SIMPLE     0  // Simple print element.
#define ELEMENT_COMPOSITE  1  // Composite print element.
#define ELEMENT_DYNAMIC    2  // Dynamic print element.

// Print element spacing values.
#define SPACING_NONE        0  // No horizontal movement.
#define SPACING_UNKNOWN     1  // Unknown horizontal movement, treat as none.
#define SPACING_FORWARD     2  // Forward horizontal movement.
#define SPACING_BACKWARD    3  // Backward horizontal movement.
#define SPACING_TAB         4  // Tab horizontal movement.
#define SPACING_TABX        5  // Tab horizontal movement, no residual_time adjustment.
#define SPACING_RETURN      6  // Return horizontal movement.
#define SPACING_RETURNX     7  // Return horizontal movement, no residual_time adjustment.
#define SPACING_LOADPAPER   8  // Load paper horizontal movement, treat as return.
#define SPACING_LMAR        9  // No horizontal movement, set left margin.
#define SPACING_RMAR       10  // No horizontal movement, set right margin.
#define SPACING_MARREL     11  // No horizontal movement, margin release.
#define SPACING_TSET       12  // No horizontal movement, set tab.
#define SPACING_TCLR       13  // No horizontal movement, clear tab.
#define SPACING_TCLRALL    14  // No horizontal movement, clear all tabs.
#define SPACING_MARTABS    15  // Reset all margins and tabs.

// Print element position values.
#define POSITION_INITIAL      0  // Initial printwheel rotational position.
#define POSITION_COUNT       96  // Number of positions on printwheel.
#define POSITION_NOCHANGE   253  // Printwheel rotational position not changed.
#define POSITION_RESET      254  // Printwheel rotational position reset to 0.
#define POSITION_UNDEFINED  255  // Rotational position undefined.

// Printer timing values for Spin-Hit-Move model (in usec).
#define TIME_PRESS_MIN    FSCAN_1_CHANGE
#define TIME_PRESS_MAX    (2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_MIN  FSCAN_1_CHANGE
#define TIME_RELEASE_MAX  (FSCAN_1_CHANGE + FSCAN_2_CHANGES)

#if FEATURE_20CPS
  #define TIME_NULL             29000
  #define TIME_SPIN_MIN             0
  #define TIME_SPIN_MAX         56000
  #define TIME_SPIN_FACTOR       1180
  #define TIME_HIT               2500
  #define TIME_MOVE             48000
  #define TIME_TAB_OFFSET      220000
  #define TIME_TAB_FACTOR        6000
  #define TIME_RETURN_OFFSET   220000
  #define TIME_RETURN_FACTOR     5400
  #define TIME_HMOVE            50000  // Adjusted to account for extra composite character slowdown
  #define TIME_HMOVE_MICRO      50000  // Adjusted to account for extra composite character slowdown
  #define TIME_VMOVE           150000  // Adjusted to account for extra composite character slowdown
  #define TIME_VMOVE_HALF      150000  // Adjusted to account for extra composite character slowdown
  #define TIME_VMOVE_MICRO     150000  // Adjusted to account for extra composite character slowdown
  #define TIME_BEEP            180000
  #define TIME_JIGGLE          220000
  #define TIME_LOADPAPER      3000000
  #define TIME_UNKNOWN        1000000
#else
  #define TIME_NULL             29000
  #define TIME_SPIN_MIN             0
  #define TIME_SPIN_MAX         45000
  #define TIME_SPIN_FACTOR        940
  #define TIME_HIT               2500
  #define TIME_MOVE             59000
  #define TIME_TAB_OFFSET      200000
  #define TIME_TAB_FACTOR        5400
  #define TIME_RETURN_OFFSET   200000
  #define TIME_RETURN_FACTOR     5400
  #define TIME_HMOVE            60000  // Adjusted to account for extra composite character slowdown
  #define TIME_HMOVE_MICRO      60000  // Adjusted to account for extra composite character slowdown
  #define TIME_VMOVE           150000  // Adjusted to account for extra composite character slowdown
  #define TIME_VMOVE_HALF      150000  // Adjusted to account for extra composite character slowdown
  #define TIME_VMOVE_MICRO     150000  // Adjusted to account for extra composite character slowdown
  #define TIME_BEEP            180000
  #define TIME_JIGGLE          220000
  #define TIME_LOADPAPER      3000000
  #define TIME_UNKNOWN        1000000
#endif

#define TIME_PRESS_NOSHIFT_1   (                      CSCAN_CHANGE)
#define TIME_PRESS_NOSHIFT_2   (     CSCAN_NOCHANGE + CSCAN_CHANGE)
#define TIME_PRESS_NOSHIFT_3   ( 2 * CSCAN_NOCHANGE + CSCAN_CHANGE)
#define TIME_PRESS_NOSHIFT_4   ( 3 * CSCAN_NOCHANGE + CSCAN_CHANGE)
#define TIME_PRESS_NOSHIFT_5   ( 4 * CSCAN_NOCHANGE + CSCAN_CHANGE)
#define TIME_PRESS_NOSHIFT_6   ( 5 * CSCAN_NOCHANGE + CSCAN_CHANGE)
#define TIME_PRESS_NOSHIFT_7   ( 6 * CSCAN_NOCHANGE + CSCAN_CHANGE)
#define TIME_PRESS_NOSHIFT_8   ( 7 * CSCAN_NOCHANGE + CSCAN_CHANGE)
#define TIME_PRESS_NOSHIFT_9   ( 8 * CSCAN_NOCHANGE + CSCAN_CHANGE)
#define TIME_PRESS_NOSHIFT_10  ( 9 * CSCAN_NOCHANGE + CSCAN_CHANGE)
#define TIME_PRESS_NOSHIFT_11  (10 * CSCAN_NOCHANGE + CSCAN_CHANGE)
#define TIME_PRESS_NOSHIFT_12  (11 * CSCAN_NOCHANGE + CSCAN_CHANGE)
#define TIME_PRESS_NOSHIFT_13  (12 * CSCAN_NOCHANGE + CSCAN_CHANGE)
#define TIME_PRESS_NOSHIFT_14  (13 * CSCAN_NOCHANGE + CSCAN_CHANGE)

#define TIME_RELEASE_NOSHIFT_1   (13 * CSCAN_NOCHANGE + FSCAN_1_CHANGE)
#define TIME_RELEASE_NOSHIFT_2   (12 * CSCAN_NOCHANGE + FSCAN_1_CHANGE)
#define TIME_RELEASE_NOSHIFT_3   (11 * CSCAN_NOCHANGE + FSCAN_1_CHANGE)
#define TIME_RELEASE_NOSHIFT_4   (10 * CSCAN_NOCHANGE + FSCAN_1_CHANGE)
#define TIME_RELEASE_NOSHIFT_5   ( 9 * CSCAN_NOCHANGE + FSCAN_1_CHANGE)
#define TIME_RELEASE_NOSHIFT_6   ( 8 * CSCAN_NOCHANGE + FSCAN_1_CHANGE)
#define TIME_RELEASE_NOSHIFT_7   ( 7 * CSCAN_NOCHANGE + FSCAN_1_CHANGE)
#define TIME_RELEASE_NOSHIFT_8   ( 6 * CSCAN_NOCHANGE + FSCAN_1_CHANGE)
#define TIME_RELEASE_NOSHIFT_9   ( 5 * CSCAN_NOCHANGE + FSCAN_1_CHANGE)
#define TIME_RELEASE_NOSHIFT_10  ( 4 * CSCAN_NOCHANGE + FSCAN_1_CHANGE)
#define TIME_RELEASE_NOSHIFT_11  ( 3 * CSCAN_NOCHANGE + FSCAN_1_CHANGE)
#define TIME_RELEASE_NOSHIFT_12  ( 2 * CSCAN_NOCHANGE + FSCAN_1_CHANGE)
#define TIME_RELEASE_NOSHIFT_13  (     CSCAN_NOCHANGE + FSCAN_1_CHANGE)
#define TIME_RELEASE_NOSHIFT_14  (                      FSCAN_1_CHANGE)

#define TIME_PRESS_SHIFT_2   (                      2 * CSCAN_CHANGE)
#define TIME_PRESS_SHIFT_3   (     CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_SHIFT_4   ( 2 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_SHIFT_6   ( 4 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_SHIFT_7   ( 5 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_SHIFT_8   ( 6 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_SHIFT_9   ( 7 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_SHIFT_10  ( 8 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_SHIFT_11  ( 9 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_SHIFT_12  (10 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_SHIFT_13  (11 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_SHIFT_14  (12 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)

#define TIME_RELEASE_SHIFT_2   (12 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_SHIFT_3   (11 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_SHIFT_4   (10 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_SHIFT_6   ( 8 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_SHIFT_7   ( 7 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_SHIFT_8   ( 6 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_SHIFT_9   ( 5 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_SHIFT_10  ( 4 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_SHIFT_11  ( 3 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_SHIFT_12  ( 2 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_SHIFT_13  (     CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_SHIFT_14  (                      2 * FSCAN_1_CHANGE)

#define TIME_PRESS_CODE_2   (14 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_CODE_3   (15 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_CODE_4   (16 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_CODE_6   ( 4 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_CODE_7   ( 5 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_CODE_8   ( 6 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_CODE_9   ( 7 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_CODE_10  ( 8 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_CODE_11  ( 9 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_CODE_12  (10 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_CODE_13  (11 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)
#define TIME_PRESS_CODE_14  (12 * CSCAN_NOCHANGE + 2 * CSCAN_CHANGE)

#define TIME_RELEASE_CODE_2   (12 * CSCAN_NOCHANGE +     FSCAN_2_CHANGES)
#define TIME_RELEASE_CODE_3   (11 * CSCAN_NOCHANGE +     FSCAN_2_CHANGES)
#define TIME_RELEASE_CODE_4   (10 * CSCAN_NOCHANGE +     FSCAN_2_CHANGES)
#define TIME_RELEASE_CODE_6   ( 8 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_CODE_7   ( 7 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_CODE_8   ( 6 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_CODE_9   ( 5 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_CODE_10  ( 4 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_CODE_11  ( 3 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_CODE_12  ( 2 * CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_CODE_13  (     CSCAN_NOCHANGE + 2 * FSCAN_1_CHANGE)
#define TIME_RELEASE_CODE_14  (                      2 * FSCAN_1_CHANGE)

// ISR values.
#define ISR_DELAY  20  // ISR delay (in usec) before reading column lines.

// Setup values.
#define COLUMN_COMMAND   68  // Column to space to for command response so prompt can be read.
#define COLUMN_RESPONSE  64  // Column to space to for setting responses so prompts can be read.
#define COLUMN_QUESTION  49  // Column to space to for question answers so prompts can be read.
#define COLUMN_FUNCTION  69  // Column to space to for function response so prompt can be read.

// Size of data arrays.
#define SIZE_COMMAND_BUFFER     100  // Size of command buffer, in characters.
#define SIZE_RECEIVE_BUFFER    1000  // Size of serial receive buffer, in characters.
#define SIZE_SEND_BUFFER       1000  // Size of serial send buffer, in characters.
#define SIZE_TRANSFER_BUFFER    100  // Size of ISR -> main transfer buffer, in print characters.
#define SIZE_PRINT_BUFFER    100000  // Size of print buffer, in print codes.

// RTS values.
#define RTS_TIMEOUT_DELAY  500  // Timeout delay (in msec) before turning off RTS.

// Buffer flow control thresholds.
#define LOWER_THRESHOLD_RECEIVE_BUFFER    200  // Flow control lower threshold of serial receive buffer, in characters.
#define UPPER_THRESHOLD_RECEIVE_BUFFER    800  // Flow control upper threshold of serial receive buffer, in characters.
#define LOWER_THRESHOLD_PRINT_BUFFER    20000  // Flow control lower threshold of print buffer, in print codes.
#define UPPER_THRESHOLD_PRINT_BUFFER    80000  // Flow control upper threshold of print buffer, in print codes.

// RS-232 RTS states.
#define RTS_UNDEFINED    0  // Undefine RTS state.
#define RTS_OFF          1  // RTS is off.
#define RTS_ON           2  // RTS is on.
#define RTS_EMPTYING_SW  3  // RTS is emptying software buffer.
#define RTS_EMPTYING_HW  4  // RTS is emptying hardware buffer.
#define RTS_TIMEOUT      5  // RTS is in timeout.

// Escape character values.
#define ESC_IGNORE         0  // Ignore escape character type.
#define ESC_CONTROL        1  // Control escape character type.
#define ESC_CTRL_BEL       2  // Control BEL escape character type.
#define ESC_CTRL_BS_CR     3  // Control BS thru CR escape character type.
#define ESC_CTRL_CAN_SUB   4  // Control CAN SUB escape character type.
#define ESC_CTRL_ESC       5  // Control ESC escape character type.
#define ESC_PARAMETER      6  // Parameter escape character type.
#define ESC_INTERMEDIATE   7  // Intermediate escape character type.
#define ESC_UPPERCASE      8  // Uppercase escape character type.
#define ESC_UC_STRING      9  // Uppercase P X ] ^ _ escape character type.
#define ESC_UC_LBRACKET   10  // Uppercase [ escape character type.
#define ESC_UC_BSLASH     11  // Uppercase \ escape character type.
#define ESC_LOWERCASE     12  // Lowercase escape character type.

// Escape states.
#define ESC_INIT_STATE  0  // Initial escape state.
#define ESC_CSIP_STATE  1  // CSI-P escape state.
#define ESC_CSII_STATE  2  // CSI-I escape state.
#define ESC_FUNC_STATE  3  // Function escape state.
#define ESC_STR_STATE   4  // String escape state.

// Escape actions.
#define ESC_NONE   0  // No escape action.
#define ESC_PRINT  1  // Print escape action.
#define ESC_EXIT   2  // Exit escape action.
#define ESC_ERROR  3  // Error escape action.
#define ESC_INIT   4  // Initial escape state action.
#define ESC_CSIP   5  // CSI-P escape state action.
#define ESC_CSII   6  // CSI-I escape state action.
#define ESC_FUNC   7  // Function escape state action.
#define ESC_STR    8  // String escape state action.

// EEPROM data locations.
#define EEPROM_FINGERPRINT     0  // EEPROM location of fingerprint byte.                        Used by all emulations.
#define EEPROM_VERSION         1  // EEPROM location of firmware version byte.                   Used by all emulations.
#define EEPROM_EMULATION       7  // EEPROM location of emulation byte.                          Used by all emulations.
#define EEPROM_ERRORS          2  // EEPROM location of errors setting byte.                     Used by all emulations.
#define EEPROM_WARNINGS        3  // EEPROM location of warnings setting byte.                   Used by all emulations.
#define EEPROM_BATTERY         4  // EEPROM location of battery setting byte.                    Used by all emulations.
#define EEPROM_LMARGIN         5  // EEPROM location of left margin setting byte.                Used by all emulations.
#define EEPROM_RMARGIN         6  // EEPROM location of right margin setting byte.               Used by all emulations.
#define EEPROM_OFFSET         30  // EEPROM location of line offset byte.                        Used by all emulations.
#define EEPROM_ASCIIWHEEL     34  // EEPROM location of ASCII printwheel setting byte.           Used by all emulations.
#define EEPROM_IMPRESSION     35  // EEPROM location of impression setting byte.                 Used by all emulations.

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
#define EEPROM_LENGTH         29  // EEPROM location of line length byte.                        Used by ASCII Terminal.
#define EEPROM_RECEIVEEOL     31  // EEPROM location of receive end-of-line setting byte.        Used by ASCII Terminal.
#define EEPROM_ESCAPESEQUENCE 32  // EEPROM location of escape sequence setting byte.            Used by ASCII Terminal.
#define EEPROM_UPPERCASE      33  // EEPROM location of uppercase only setting byte.             Used by ASCII Terminal.

#define EEPROM_TABS          100  // EEPROM location of tab table bytes [200].                   Used by all emulations.

#define SIZE_EEPROM         4096  // Size of EEPROM.

// EEPROM fingerprint value.
#define FINGERPRINT  0xdb  // EEPROM fingerprint value.

// Error codes.
#define ERROR_NULL                0  // Null error.
#define ERROR_CB_FULL             1  // Command buffer full error.
#define ERROR_RB_FULL             2  // Receive buffer full error.
#define ERROR_SB_FULL             3  // Send buffer full error.
#define ERROR_TB_FULL             4  // Transfer buffer full error.
#define ERROR_PB_FULL             5  // Print buffer full error.
#define ERROR_BAD_CODE            6  // Bad print code error.
#define ERROR_BAD_ESCAPE          7  // Bad escape sequence error.
#define ERROR_BAD_TYPE            8  // Bad element type code.
#define ERROR_BAD_SPACING         9  // Bad print spacing code.
#define ERROR_BAD_POSITION       10  // Bad printwheel position code.
#define ERROR_BAD_KEY_ACTION     11  // Bad key action code.
#define ERROR_BAD_SERIAL_ACTION  12  // Bad serial action code.

#define NUM_ERRORS               13  // Number of error codes.

// Warning codes.
#define WARNING_NULL             0  // Null warning.
#define WARNING_SHORT_SCAN       1  // Short column scan warning.
#define WARNING_LONG_SCAN        2  // Long column scan warning.
#define WARNING_UNEXPECTED_SCAN  3  // Unexpected column scan warning.
#define WARNING_PARITY           4  // Serial input parity warning.

#define NUM_WARNINGS             5  // Number of warning codes.

// Emulation codes.
                                 // J6 J7  Jumper settings.
#define EMULATION_NULL        0  //        - Null emulation.
#define EMULATION_IBM         1  //        - IBM 1620 Jr. Console Typewriter emulation.
#define EMULATION_ASCII       2  //     X  - ASCII Terminal emulation.
#define EMULATION_FUTURE      3  //  X     - Reserved for future emulation.
#define EMULATION_STANDALONE  4  //  X  X  - Standalone Typewriter emulation.

// True or false setting values.
#define SETTING_UNDEFINED  0  // Undefined setting.
#define SETTING_TRUE       1  // True setting.
#define SETTING_FALSE      2  // False setting.

// Serial values.
#define SERIAL_UNDEFINED  0  // Undefined serial setting.
#define SERIAL_USB        1  // USB serial.
#define SERIAL_RS232      2  // RS-232 serial (UART or SlowSoftSerial).

// RS-232 values (not stored in EEPROM).
#define RS232_UNDEFINED   0  // Undefined RS-232 mode.
#define RS232_UART        1  // Hardware UART.
#define RS232_SLOW        2  // SlowSoftSerial port.

// Duplex values.
#define DUPLEX_UNDEFINED  0  // Undefined duplex setting.
#define DUPLEX_HALF       1  // Half duplex setting.
#define DUPLEX_FULL       2  // Full duplex setting.

// Baud values.
#define BAUD_UNDEFINED          0  // Undefined baud setting.
#define BAUD_50                 1  // 50 baud.   Can't use with UART0, supported by the SlowSoftSerial library.
#define BAUD_75                 2  // 75 baud.   Can't use with UART0, supported by the SlowSoftSerial library.
#define BAUD_110                3  // 110 baud.  Can't use with UART0, supported by the SlowSoftSerial library.
#define BAUD_134                4  // 134 baud.  Can't use with UART0, supported by the SlowSoftSerial library.
#define BAUD_150                5  // 150 baud.  Can't use with UART0, supported by the SlowSoftSerial library.
#define BAUD_200                6  // 200 baud.  Can't use with UART0, supported by the SlowSoftSerial library.
#define BAUD_300                7  // 300 baud.  Can't use with UART0, supported by the SlowSoftSerial library.
#define BAUD_600                8  // 600 baud.  Can't use with UART0, supported by the SlowSoftSerial library.
#define BAUD_1200               9  // 1200 baud.
#define BAUD_1800              10  // 1800 baud.
#define BAUD_2400              11  // 2400 baud.
#define BAUD_4800              12  // 4800 baud.
#define BAUD_9600              13  // 9600 baud.
#define BAUD_19200             14  // 19200 baud.
#define BAUD_38400             15  // 38400 baud.
#define BAUD_57600             16  // 57600 baud.
#define BAUD_76800             17  // 76800 baud.
#define BAUD_115200            18  // 115200 baud.
#define BAUD_230400            19  // 230400 baud.
#define BAUD_460800            20  // 460800 baud.  Not supported by MAX3232.
#define BAUD_921600            21  // 921600 baud.  Not supported by MAX3232.

#define NUM_BAUDS              22  // Number of baud values.
#define MINIMUM_HARDWARE_BAUD  BAUD_1200

// Parity values.
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

// Impression values.
#define IMPRESSION_UNDEFINED  0  // Undefined impression setting.
#define IMPRESSION_LIGHT      1  // Light impression setting.
#define IMPRESSION_NORMAL     2  // Normal impression setting.
#define IMPRESSION_HEAVY      3  // Heavy impression setting.

// Software flow control values.
#define SWFLOW_UNDEFINED  0  // Undefined software flow control setting.
#define SWFLOW_NONE       1  // No software flow control setting.
#define SWFLOW_XON_XOFF   2  // XON/XOFF flow control setting.

// Hardware flow control values.
#define HWFLOW_UNDEFINED  0  // Undefined hardware flow control setting.
#define HWFLOW_NONE       1  // No hardware flow control setting.
#define HWFLOW_RTS_CTS    2  // RTS/CTS flow control setting.
#define HWFLOW_RTR_CTS    3  // RTR/CTS flow control setting.

// End of line values.
#define EOL_UNDEFINED  0  // Undefined end-of-line setting.
#define EOL_CR         1  // CR end-of-line setting.
#define EOL_CRLF       2  // CR LF end-of-line setting.
#define EOL_LF         3  // LF end-of-line setting.
#define EOL_LFCR       4  // LF CR end-of-line setting.

// Escape sequence values.
#define ESCAPE_UNDEFINED  0  // Undefined escape sequence setting.
#define ESCAPE_NONE       1  // None escape sequences setting.
#define ESCAPE_IGNORE     2  // Ignore escape sequence setting.

// Line length values.
#define LENGTH_UNDEFINED    0  // Undefined length setting.
#define LENGTH_DEFAULT     80  // Default line length.
#define LENGTH_MINIMUM     80  // Minimum line length.
#if FEATURE_WIDE_CARRIAGE
  #define LENGTH_MAXIMUM    198  // Maximum line length.
#else
  #define LENGTH_MAXIMUM    165  // Maximum line length.
#endif

// Configuration parameters initial values.
#define INITIAL_EMULATION       EMULATION_NULL     // Current emulation.
#define INITIAL_ERRORS          SETTING_TRUE       // Report errors.
#define INITIAL_WARNINGS        SETTING_FALSE      // Report warnings.
#define INITIAL_BATTERY         SETTING_FALSE      // Battery installed.
#define INITIAL_LMARGIN         1                  // Left margin.
#define INITIAL_RMARGIN         LENGTH_DEFAULT     // Right margin.
#define INITIAL_OFFSET          0                  // Column offset.
#define INITIAL_ASCIIWHEEL      SETTING_FALSE      // ASCII printwheel.
#define INITIAL_SLASH           SETTING_TRUE       // Print slashed zeroes.
#define INITIAL_BOLD            SETTING_FALSE      // Print bold input.
#define INITIAL_SERIAL          SERIAL_USB         // Serial type.
#define INITIAL_DUPLEX          DUPLEX_FULL        // Duplex.
#define INITIAL_BAUD            BAUD_9600          // Baud rate.
#define INITIAL_PARITY          PARITY_NONE        // Parity.
#define INITIAL_DPS             DPS_8N1            // Databits, parity, stopbits.
#define INITIAL_IMPRESSION      IMPRESSION_NORMAL  // Impression.
#define INITIAL_SWFLOW          SWFLOW_XON_XOFF    // Software flow control.
#define INITIAL_HWFLOW          HWFLOW_NONE        // Hardware flow control.
#define INITIAL_UPPERCASE       SETTING_FALSE      // Uppercase only.
#define INITIAL_AUTORETURN      SETTING_TRUE       // Auto return.
#define INITIAL_TRANSMITEOL     EOL_CR             // Send end-of-line.
#define INITIAL_RECEIVEEOL      EOL_CRLF           // Receive end-of-line.
#define INITIAL_ESCAPESEQUENCE  ESCAPE_IGNORE      // Escape sequences.
#define INITIAL_LENGTH          LENGTH_DEFAULT     // Line length.
#define INITIAL_IBM_OFFSET      0                  // Column offset for IBM 1620 Jr.
#define INITIAL_ASCII_OFFSET    1                  // Column offset for ASCII Terminal when no ASCII printwheel.
#define INITIAL_ASCII_OFFSETX   0                  // Column offset for ASCII Terminal when ASCII printwheel.
#define INITIAL_FUTURE_OFFSET   1                  // Column offset for future emulation when no ASCII printwheel.
#define INITIAL_FUTURE_OFFSETX  0                  // Column offset for future emulation when ASCII printwheel.


//**********************************************************************************************************************
//
//  Wheelwriter control structures.
//
//**********************************************************************************************************************

// Keyboard key action structure.
struct key_action {
  byte                     action;     // Action to take.
  char                     send;       // Character to send or process as setup command.
  short                    _pad_;      // Structure alignment padding.
  union {                              // Print info.
    const void              *init;     //   Used for static initialization.
    const struct print_info *element;  //   Element to print.
  };
};
#define NULL_KEY_ACTION  (const struct key_action*)NULL

// Serial input action structure.
struct serial_action {
  int                      action;     // Action to take.
  union {                              // Print info.
    const void              *init;     //   Used for static initialization.
    const struct print_info *element;  //   Element to print.
  };
};
#define NULL_SERIAL_ACTION  (const struct serial_action*)NULL

// Print element information structure.
struct print_info {
  byte                    type;               // Element type.
  byte                    spacing;            // Spacing type.
  byte                    position;           // Printwheel rotational position.
  byte                    _pad_;              // Structure alignment padding.
  int                     timing;             // Timing value (in usec).
  union {                                     // Print codes.
    const void              *init;            //   Used for static initialization.
    const byte              *element;         //   Used for simple elements.
    const struct print_info **pelement;       //   Used for complex and dynamic elements.
  };
};
#define NULL_PRINT_INFO  (const struct print_info*)NULL


//**********************************************************************************************************************
//
//  Wheelwriter print strings.
//
//**********************************************************************************************************************

// <null> print string.
const byte WW_STR_NULL[]              = {WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_NULL = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0, 0, &WW_STR_NULL};

// <load paper>, <set top of form> print strings.
const byte WW_STR_LOADPAPER[]                 = {WW_CATCH_UP, WW_LOADPAPER_SETTOPOFFORM, WW_NULL_4, WW_NULL_14,
                                                 WW_NULL};
const struct print_info WW_PRINT_LOADPAPER    = {ELEMENT_SIMPLE, SPACING_LOADPAPER, 0, 0,
                                                 TIME_LOADPAPER - (TIME_PRESS_NOSHIFT_4 + TIME_RELEASE_NOSHIFT_4),
                                                 &WW_STR_LOADPAPER};
const byte WW_STR_SETTOPOFFORM[]              = {WW_CATCH_UP, WW_Code, WW_LOADPAPER_SETTOPOFFORM, WW_Code, WW_NULL_4,
                                                 WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_SETTOPOFFORM = {ELEMENT_SIMPLE, SPACING_LOADPAPER, 0, 0,
                                                 TIME_LOADPAPER - (TIME_PRESS_CODE_4 + TIME_RELEASE_CODE_4),
                                                 &WW_STR_SETTOPOFFORM};

// <space>, <required space>, Tab, C Rtn print strings.
const byte WW_STR_SPACE[]              = {WW_SPACE_REQSPACE, WW_NULL_4, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_SPACE = {ELEMENT_SIMPLE, SPACING_FORWARD, POSITION_NOCHANGE, 0,
                                          TIME_MOVE - (TIME_PRESS_NOSHIFT_4 + TIME_RELEASE_NOSHIFT_4),
                                          &WW_STR_SPACE};

const byte WW_STR_REQSPACE[]              = {WW_Code, WW_SPACE_REQSPACE, WW_Code, WW_NULL_4, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_REQSPACE = {ELEMENT_SIMPLE, SPACING_FORWARD, POSITION_NOCHANGE, 0,
                                             TIME_MOVE - (TIME_PRESS_CODE_4 + TIME_RELEASE_CODE_4),
                                             &WW_STR_REQSPACE};

const byte WW_STR_Tab[]              = {WW_Tab_IndL, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Tab = {ELEMENT_SIMPLE, SPACING_TAB, POSITION_NOCHANGE, 0,
                                        TIME_TAB_OFFSET - (TIME_PRESS_NOSHIFT_14 + TIME_RELEASE_NOSHIFT_14),
                                        &WW_STR_Tab};

const byte WW_STR_CRtn[]              = {WW_CRtn_IndClr, WW_NULL_13, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_CRtn = {ELEMENT_SIMPLE, SPACING_RETURN, POSITION_NOCHANGE, 0,
                                         TIME_RETURN_OFFSET - (TIME_PRESS_NOSHIFT_13 + TIME_RELEASE_NOSHIFT_13),
                                         &WW_STR_CRtn};

// <left shift>, <right shift>, Lock, <code> print strings.
const byte WW_STR_LShift[]              = {WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_LShift = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                           TIME_NULL - (TIME_PRESS_NOSHIFT_1 + TIME_RELEASE_NOSHIFT_1),
                                           &WW_STR_LShift};

const byte WW_STR_RShift[]              = {WW_RShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_RShift = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                           TIME_NULL - (TIME_PRESS_NOSHIFT_1 + TIME_RELEASE_NOSHIFT_1),
                                           &WW_STR_RShift};

const byte WW_STR_Lock[]              = {WW_Lock, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Lock = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                         TIME_NULL - (TIME_PRESS_NOSHIFT_14 + TIME_RELEASE_NOSHIFT_14),
                                         &WW_STR_Lock};

const byte WW_STR_Code[]              = {WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Code = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                         TIME_NULL - (TIME_PRESS_NOSHIFT_5 + TIME_RELEASE_NOSHIFT_5),
                                         &WW_STR_Code};

// A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z print strings.
const byte WW_STR_A[]              = {WW_LShift, WW_a_A, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_A = {ELEMENT_SIMPLE, SPACING_FORWARD, 31, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_3 + TIME_RELEASE_SHIFT_3),
                                      &WW_STR_A};

const byte WW_STR_B[]              = {WW_RShift, WW_b_B_Bold, WW_RShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_B = {ELEMENT_SIMPLE, SPACING_FORWARD, 17, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_8 + TIME_RELEASE_SHIFT_8),
                                      &WW_STR_B};

const byte WW_STR_C[]              = {WW_LShift, WW_c_C_Ctr, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_C = {ELEMENT_SIMPLE, SPACING_FORWARD, 26, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_7 + TIME_RELEASE_SHIFT_7),
                                      &WW_STR_C};

const byte WW_STR_D[]              = {WW_LShift, WW_d_D_DecT, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_D = {ELEMENT_SIMPLE, SPACING_FORWARD, 28, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_7 + TIME_RELEASE_SHIFT_7),
                                      &WW_STR_D};

const byte WW_STR_E[]              = {WW_LShift, WW_e_E, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_E = {ELEMENT_SIMPLE, SPACING_FORWARD, 29, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_7 + TIME_RELEASE_SHIFT_7),
                                      &WW_STR_E};

const byte WW_STR_F[]              = {WW_LShift, WW_f_F, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_F = {ELEMENT_SIMPLE, SPACING_FORWARD, 16, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_8 + TIME_RELEASE_SHIFT_8),
                                      &WW_STR_F};

const byte WW_STR_G[]              = {WW_LShift, WW_g_G, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_G = {ELEMENT_SIMPLE, SPACING_FORWARD, 14, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_8 + TIME_RELEASE_SHIFT_8),
                                      &WW_STR_G};

const byte WW_STR_H[]              = {WW_LShift, WW_h_H_12DOWN, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_H = {ELEMENT_SIMPLE, SPACING_FORWARD, 19, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_9 + TIME_RELEASE_SHIFT_9),
                                      &WW_STR_H};

const byte WW_STR_I[]              = {WW_LShift, WW_i_I_Word, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_I = {ELEMENT_SIMPLE, SPACING_FORWARD, 30, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_10 + TIME_RELEASE_SHIFT_10),
                                      &WW_STR_I};

const byte WW_STR_J[]              = {WW_LShift, WW_j_J, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_J = {ELEMENT_SIMPLE, SPACING_FORWARD, 32, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_9 + TIME_RELEASE_SHIFT_9),
                                      &WW_STR_J};

const byte WW_STR_K[]              = {WW_LShift, WW_k_K, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_K = {ELEMENT_SIMPLE, SPACING_FORWARD, 42, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_10 + TIME_RELEASE_SHIFT_10),
                                      &WW_STR_K};

const byte WW_STR_L[]              = {WW_LShift, WW_l_L_Lang, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_L = {ELEMENT_SIMPLE, SPACING_FORWARD, 23, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_11 + TIME_RELEASE_SHIFT_11),
                                      &WW_STR_L};

const byte WW_STR_M[]              = {WW_LShift, WW_m_M, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_M = {ELEMENT_SIMPLE, SPACING_FORWARD, 35, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_9 + TIME_RELEASE_SHIFT_9),
                                      &WW_STR_M};

const byte WW_STR_N[]              = {WW_RShift, WW_n_N_Caps, WW_RShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_N = {ELEMENT_SIMPLE, SPACING_FORWARD, 25, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_9 + TIME_RELEASE_SHIFT_9),
                                      &WW_STR_N};

const byte WW_STR_O[]              = {WW_LShift, WW_o_O_RFlsh, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_O = {ELEMENT_SIMPLE, SPACING_FORWARD, 33, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_11 + TIME_RELEASE_SHIFT_11),
                                      &WW_STR_O};

const byte WW_STR_P[]              = {WW_LShift, WW_p_P, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_P = {ELEMENT_SIMPLE, SPACING_FORWARD, 20, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_12 + TIME_RELEASE_SHIFT_12),
                                      &WW_STR_P};

const byte WW_STR_Q[]              = {WW_LShift, WW_q_Q_Impr, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Q = {ELEMENT_SIMPLE, SPACING_FORWARD, 61, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_3 + TIME_RELEASE_SHIFT_3),
                                      &WW_STR_Q};

const byte WW_STR_R[]              = {WW_LShift, WW_r_R_ARtn, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_R = {ELEMENT_SIMPLE, SPACING_FORWARD, 22, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_8 + TIME_RELEASE_SHIFT_8),
                                      &WW_STR_R};

const byte WW_STR_S[]              = {WW_LShift, WW_s_S, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_S = {ELEMENT_SIMPLE, SPACING_FORWARD, 24, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_6 + TIME_RELEASE_SHIFT_6),
                                      &WW_STR_S};

const byte WW_STR_T[]              = {WW_LShift, WW_t_T, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_T = {ELEMENT_SIMPLE, SPACING_FORWARD, 27, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_8 + TIME_RELEASE_SHIFT_8),
                                      &WW_STR_T};

const byte WW_STR_U[]              = {WW_LShift, WW_u_U_Cont, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_U = {ELEMENT_SIMPLE, SPACING_FORWARD, 15, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_9 + TIME_RELEASE_SHIFT_9),
                                      &WW_STR_U};

const byte WW_STR_V[]              = {WW_LShift, WW_v_V, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_V = {ELEMENT_SIMPLE, SPACING_FORWARD, 12, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_8 + TIME_RELEASE_SHIFT_8),
                                      &WW_STR_V};

const byte WW_STR_W[]              = {WW_LShift, WW_w_W, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_W = {ELEMENT_SIMPLE, SPACING_FORWARD, 40, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_6 + TIME_RELEASE_SHIFT_6),
                                      &WW_STR_W};

const byte WW_STR_X[]              = {WW_LShift, WW_x_X_POWERWISE, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_X = {ELEMENT_SIMPLE, SPACING_FORWARD, 44, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_6 + TIME_RELEASE_SHIFT_6),
                                      &WW_STR_X};

const byte WW_STR_Y[]              = {WW_LShift, WW_y_Y_12UP, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Y = {ELEMENT_SIMPLE, SPACING_FORWARD, 37, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_9 + TIME_RELEASE_SHIFT_9),
                                      &WW_STR_Y};

const byte WW_STR_Z[]              = {WW_LShift, WW_z_Z, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL}; 
const struct print_info WW_PRINT_Z = {ELEMENT_SIMPLE, SPACING_FORWARD, 18, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_3 + TIME_RELEASE_SHIFT_3),
                                      &WW_STR_Z};

// a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z print strings.
const byte WW_STR_a[]              = {WW_a_A, WW_NULL_3, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_a = {ELEMENT_SIMPLE, SPACING_FORWARD, 0, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_3 + TIME_RELEASE_NOSHIFT_3),
                                      &WW_STR_a};

const byte WW_STR_b[]              = {WW_b_B_Bold, WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_b = {ELEMENT_SIMPLE, SPACING_FORWARD, 88, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_8 + TIME_RELEASE_NOSHIFT_8),
                                      &WW_STR_b};

const byte WW_STR_c[]              = {WW_c_C_Ctr, WW_NULL_7, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_c = {ELEMENT_SIMPLE, SPACING_FORWARD, 4, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_7 + TIME_RELEASE_NOSHIFT_7),
                                      &WW_STR_c};

const byte WW_STR_d[]              = {WW_d_D_DecT, WW_NULL_7, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_d = {ELEMENT_SIMPLE, SPACING_FORWARD, 6, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_7 + TIME_RELEASE_NOSHIFT_7),
                                      &WW_STR_d};

const byte WW_STR_e[]              = {WW_e_E, WW_NULL_7, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_e = {ELEMENT_SIMPLE, SPACING_FORWARD, 95, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_7 + TIME_RELEASE_NOSHIFT_7),
                                      &WW_STR_e};

const byte WW_STR_f[]              = {WW_f_F, WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_f = {ELEMENT_SIMPLE, SPACING_FORWARD, 9, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_8 + TIME_RELEASE_NOSHIFT_8),
                                      &WW_STR_f};

const byte WW_STR_g[]              = {WW_g_G, WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_g = {ELEMENT_SIMPLE, SPACING_FORWARD, 89, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_8 + TIME_RELEASE_NOSHIFT_8),
                                      &WW_STR_g};

const byte WW_STR_h[]              = {WW_h_H_12DOWN, WW_NULL_9, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_h = {ELEMENT_SIMPLE, SPACING_FORWARD, 7, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_9 + TIME_RELEASE_NOSHIFT_9),
                                      &WW_STR_h};

const byte WW_STR_i[]              = {WW_i_I_Word, WW_NULL_10, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_i = {ELEMENT_SIMPLE, SPACING_FORWARD, 92, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_10 + TIME_RELEASE_NOSHIFT_10),
                                      &WW_STR_i};

const byte WW_STR_j[]              = {WW_j_J, WW_NULL_9, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_j = {ELEMENT_SIMPLE, SPACING_FORWARD, 85, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_9 + TIME_RELEASE_NOSHIFT_9),
                                      &WW_STR_j};

const byte WW_STR_k[]              = {WW_k_K, WW_NULL_10, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_k = {ELEMENT_SIMPLE, SPACING_FORWARD, 10, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_10 + TIME_RELEASE_NOSHIFT_10),
                                      &WW_STR_k};

const byte WW_STR_l[]              = {WW_l_L_Lang, WW_NULL_11, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_l = {ELEMENT_SIMPLE, SPACING_FORWARD, 8, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_11 + TIME_RELEASE_NOSHIFT_11),
                                      &WW_STR_l};

const byte WW_STR_m[]              = {WW_m_M, WW_NULL_9, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_m = {ELEMENT_SIMPLE, SPACING_FORWARD, 3, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_9 + TIME_RELEASE_NOSHIFT_9),
                                      &WW_STR_m};

const byte WW_STR_n[]              = {WW_n_N_Caps, WW_NULL_9, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_n = {ELEMENT_SIMPLE, SPACING_FORWARD, 1, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_9 + TIME_RELEASE_NOSHIFT_9),
                                      &WW_STR_n};

const byte WW_STR_o[]              = {WW_o_O_RFlsh, WW_NULL_11, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_o = {ELEMENT_SIMPLE, SPACING_FORWARD, 94, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_11 + TIME_RELEASE_NOSHIFT_11),
                                      &WW_STR_o};

const byte WW_STR_p[]              = {WW_p_P, WW_NULL_12, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_p = {ELEMENT_SIMPLE, SPACING_FORWARD, 81, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_12 + TIME_RELEASE_NOSHIFT_12),
                                      &WW_STR_p};

const byte WW_STR_q[]              = {WW_q_Q_Impr, WW_NULL_3, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_q = {ELEMENT_SIMPLE, SPACING_FORWARD, 91, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_3 + TIME_RELEASE_NOSHIFT_3),
                                      &WW_STR_q};

const byte WW_STR_r[]              = {WW_r_R_ARtn, WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_r = {ELEMENT_SIMPLE, SPACING_FORWARD, 2, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_8 + TIME_RELEASE_NOSHIFT_8),
                                      &WW_STR_r};

const byte WW_STR_s[]              = {WW_s_S, WW_NULL_6, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_s = {ELEMENT_SIMPLE, SPACING_FORWARD, 5, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_6 + TIME_RELEASE_NOSHIFT_6),
                                      &WW_STR_s};

const byte WW_STR_t[]              = {WW_t_T, WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_t = {ELEMENT_SIMPLE, SPACING_FORWARD, 93, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_8 + TIME_RELEASE_NOSHIFT_8),
                                      &WW_STR_t};

const byte WW_STR_u[]              = {WW_u_U_Cont, WW_NULL_9, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_u = {ELEMENT_SIMPLE, SPACING_FORWARD, 90, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_9 + TIME_RELEASE_NOSHIFT_9),
                                      &WW_STR_u};

const byte WW_STR_v[]              = {WW_v_V, WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_v = {ELEMENT_SIMPLE, SPACING_FORWARD, 82, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_8 + TIME_RELEASE_NOSHIFT_8),
                                      &WW_STR_v};

const byte WW_STR_w[]              = {WW_w_W, WW_NULL_6, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_w = {ELEMENT_SIMPLE, SPACING_FORWARD, 84, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_6 + TIME_RELEASE_NOSHIFT_6),
                                      &WW_STR_w};

const byte WW_STR_x[]              = {WW_x_X_POWERWISE, WW_NULL_6, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_x = {ELEMENT_SIMPLE, SPACING_FORWARD, 80, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_6 + TIME_RELEASE_NOSHIFT_6),
                                      &WW_STR_x};

const byte WW_STR_y[]              = {WW_y_Y_12UP, WW_NULL_9, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_y = {ELEMENT_SIMPLE, SPACING_FORWARD, 87, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_9 + TIME_RELEASE_NOSHIFT_9),
                                      &WW_STR_y};

const byte WW_STR_z[]              = {WW_z_Z, WW_NULL_3, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_z = {ELEMENT_SIMPLE, SPACING_FORWARD, 83, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_3 + TIME_RELEASE_NOSHIFT_3),
                                      &WW_STR_z};

// 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 print strings.
const byte WW_STR_0[]              = {WW_0_RPAREN, WW_NULL_12, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_0 = {ELEMENT_SIMPLE, SPACING_FORWARD, 47, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_12 + TIME_RELEASE_NOSHIFT_12),
                                      &WW_STR_0};

const byte WW_STR_1[]              = {WW_1_EXCLAMATION_Spell, WW_NULL_3, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_1 = {ELEMENT_SIMPLE, SPACING_FORWARD, 45, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_3 + TIME_RELEASE_NOSHIFT_3),
                                      &WW_STR_1};

const byte WW_STR_2[]              = {WW_2_AT_Add, WW_NULL_6, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_2 = {ELEMENT_SIMPLE, SPACING_FORWARD, 46, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_6 + TIME_RELEASE_NOSHIFT_6),
                                      &WW_STR_2};

const byte WW_STR_3[]              = {WW_3_POUND_Del, WW_NULL_7, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_3 = {ELEMENT_SIMPLE, SPACING_FORWARD, 43, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_7 + TIME_RELEASE_NOSHIFT_7),
                                      &WW_STR_3};

const byte WW_STR_4[]              = {WW_4_DOLLAR_Vol, WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_4 = {ELEMENT_SIMPLE, SPACING_FORWARD, 49, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_8 + TIME_RELEASE_NOSHIFT_8),
                                      &WW_STR_4};

const byte WW_STR_5[]              = {WW_5_PERCENT, WW_NULL_8, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_5 = {ELEMENT_SIMPLE, SPACING_FORWARD, 48, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_8 + TIME_RELEASE_NOSHIFT_8),
                                      &WW_STR_5};

const byte WW_STR_6[]              = {WW_6_CENT, WW_NULL_9, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_6 = {ELEMENT_SIMPLE, SPACING_FORWARD, 50, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_9 + TIME_RELEASE_NOSHIFT_9),
                                      &WW_STR_6};

const byte WW_STR_7[]              = {WW_7_AMPERSAND, WW_NULL_9, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_7 = {ELEMENT_SIMPLE, SPACING_FORWARD, 52, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_9 + TIME_RELEASE_NOSHIFT_9),
                                      &WW_STR_7};

const byte WW_STR_8[]              = {WW_8_ASTERISK, WW_NULL_10, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_8 = {ELEMENT_SIMPLE, SPACING_FORWARD, 51, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_10 + TIME_RELEASE_NOSHIFT_10),
                                      &WW_STR_8};

const byte WW_STR_9[]              = {WW_9_LPAREN_Stop, WW_NULL_11, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_9 = {ELEMENT_SIMPLE, SPACING_FORWARD, 41, 0,
                                      TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_11 + TIME_RELEASE_NOSHIFT_11),
                                      &WW_STR_9};

// !, @, #, $, %, <cent>, &, *, (, ) print strings.
const byte WW_STR_EXCLAMATION[]              = {WW_LShift, WW_1_EXCLAMATION_Spell, WW_LShift, WW_NULL_1, WW_NULL_14,
                                                WW_NULL};
const struct print_info WW_PRINT_EXCLAMATION = {ELEMENT_SIMPLE, SPACING_FORWARD, 72, 0,
                                                TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_3 + TIME_RELEASE_SHIFT_3),
                                                &WW_STR_EXCLAMATION};

const byte WW_STR_AT[]              = {WW_LShift, WW_2_AT_Add, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_AT = {ELEMENT_SIMPLE, SPACING_FORWARD, 60, 0,
                                       TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_6 + TIME_RELEASE_SHIFT_6),
                                       &WW_STR_AT};

const byte WW_STR_POUND[]              = {WW_LShift, WW_3_POUND_Del, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_POUND = {ELEMENT_SIMPLE, SPACING_FORWARD, 55, 0,
                                          TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_7 + TIME_RELEASE_SHIFT_7),
                                          &WW_STR_POUND};

const byte WW_STR_DOLLAR[]              = {WW_LShift, WW_4_DOLLAR_Vol, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_DOLLAR = {ELEMENT_SIMPLE, SPACING_FORWARD, 54, 0,
                                           TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_8 + TIME_RELEASE_SHIFT_8),
                                           &WW_STR_DOLLAR};

const byte WW_STR_PERCENT[]              = {WW_LShift, WW_5_PERCENT, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_PERCENT = {ELEMENT_SIMPLE, SPACING_FORWARD, 56, 0,
                                            TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_8 + TIME_RELEASE_SHIFT_8),
                                            &WW_STR_PERCENT};

const byte WW_STR_CENT[]              = {WW_LShift, WW_6_CENT, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_CENT = {ELEMENT_SIMPLE, SPACING_FORWARD, 57, 0,
                                         TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_9 + TIME_RELEASE_SHIFT_9),
                                         &WW_STR_CENT};

const byte WW_STR_AMPERSAND[]              = {WW_LShift, WW_7_AMPERSAND, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_AMPERSAND = {ELEMENT_SIMPLE, SPACING_FORWARD, 62, 0,
                                              TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_9 + TIME_RELEASE_SHIFT_9),
                                              &WW_STR_AMPERSAND};

const byte WW_STR_ASTERISK[]              = {WW_LShift, WW_8_ASTERISK, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_ASTERISK = {ELEMENT_SIMPLE, SPACING_FORWARD, 53, 0,
                                             TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_10 + TIME_RELEASE_SHIFT_10),
                                             &WW_STR_ASTERISK};

const byte WW_STR_LPAREN[]              = {WW_LShift, WW_9_LPAREN_Stop, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_LPAREN = {ELEMENT_SIMPLE, SPACING_FORWARD, 34, 0,
                                           TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_11 + TIME_RELEASE_SHIFT_11),
                                           &WW_STR_LPAREN};

const byte WW_STR_RPAREN[]              = {WW_LShift, WW_0_RPAREN, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_RPAREN = {ELEMENT_SIMPLE, SPACING_FORWARD, 21, 0,
                                           TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_12 + TIME_RELEASE_SHIFT_12),
                                           &WW_STR_RPAREN};

// ., ,, [, ], =, +, /, ?, -, _, :, ;, ', ", <half>, <quarter>, +/-, <degree>, <superscript 2>, <superscript 3>,
// <section>, <paragraph> print strings.
const byte WW_STR_PERIOD[]              = {WW_PERIOD_PERIOD, WW_NULL_11, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_PERIOD = {ELEMENT_SIMPLE, SPACING_FORWARD, 36, 0,
                                           TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_11 + TIME_RELEASE_NOSHIFT_11),
                                           &WW_STR_PERIOD};

const byte WW_STR_COMMA[]              = {WW_COMMA_COMMA, WW_NULL_10, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_COMMA = {ELEMENT_SIMPLE, SPACING_FORWARD, 38, 0,
                                          TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_10 + TIME_RELEASE_NOSHIFT_10),
                                          &WW_STR_COMMA};

const byte WW_STR_LBRACKET[]              = {WW_LShift, WW_RBRACKET_LBRACKET_SUPER3, WW_LShift, WW_NULL_1, WW_NULL_14,
                                             WW_NULL};
const struct print_info WW_PRINT_LBRACKET = {ELEMENT_SIMPLE, SPACING_FORWARD, 64, 0,
                                             TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_10 + TIME_RELEASE_SHIFT_10),
                                             &WW_STR_LBRACKET};

const byte WW_STR_RBRACKET[]              = {WW_RBRACKET_LBRACKET_SUPER3, WW_NULL_10, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_RBRACKET = {ELEMENT_SIMPLE, SPACING_FORWARD, 63, 0,
                                             TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_10 + TIME_RELEASE_NOSHIFT_10),
                                             &WW_STR_RBRACKET};

const byte WW_STR_EQUAL[]              = {WW_EQUAL_PLUS, WW_NULL_10, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_EQUAL = {ELEMENT_SIMPLE, SPACING_FORWARD, 76, 0,
                                          TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_10 + TIME_RELEASE_NOSHIFT_10),
                                          &WW_STR_EQUAL};

const byte WW_STR_PLUS[]              = {WW_LShift, WW_EQUAL_PLUS, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_PLUS = {ELEMENT_SIMPLE, SPACING_FORWARD, 58, 0,
                                         TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_10 + TIME_RELEASE_SHIFT_10),
                                         &WW_STR_PLUS};

const byte WW_STR_SLASH[]              = {WW_SLASH_QUESTION, WW_NULL_12, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_SLASH = {ELEMENT_SIMPLE, SPACING_FORWARD, 39, 0,
                                          TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_12 + TIME_RELEASE_NOSHIFT_12),
                                          &WW_STR_SLASH};

const byte WW_STR_QUESTION[]              = {WW_RShift, WW_SLASH_QUESTION, WW_RShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_QUESTION = {ELEMENT_SIMPLE, SPACING_FORWARD, 73, 0,
                                             TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_12 + TIME_RELEASE_SHIFT_12),
                                             &WW_STR_QUESTION};

const byte WW_STR_HYPHEN[]              = {WW_HYPHEN_UNDERSCORE, WW_NULL_12, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_HYPHEN = {ELEMENT_SIMPLE, SPACING_FORWARD, 13, 0,
                                           TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_12 + TIME_RELEASE_NOSHIFT_12),
                                           &WW_STR_HYPHEN};

const byte WW_STR_UNDERSCORE[]              = {WW_LShift, WW_HYPHEN_UNDERSCORE, WW_LShift, WW_NULL_1, WW_NULL_14,
                                               WW_NULL};
const struct print_info WW_PRINT_UNDERSCORE = {ELEMENT_SIMPLE, SPACING_FORWARD, 78, 0,
                                               TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_12 + TIME_RELEASE_SHIFT_12),
                                               &WW_STR_UNDERSCORE};

const byte WW_STR_COLON[]              = {WW_LShift, WW_SEMICOLON_COLON_SECTION, WW_LShift, WW_NULL_1, WW_NULL_14,
                                          WW_NULL};
const struct print_info WW_PRINT_COLON = {ELEMENT_SIMPLE, SPACING_FORWARD, 77, 0,
                                          TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_12 + TIME_RELEASE_SHIFT_12),
                                          &WW_STR_COLON};

const byte WW_STR_SEMICOLON[]              = {WW_SEMICOLON_COLON_SECTION, WW_NULL_12, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_SEMICOLON = {ELEMENT_SIMPLE, SPACING_FORWARD, 79, 0,
                                              TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_12 + TIME_RELEASE_NOSHIFT_12),
                                              &WW_STR_SEMICOLON};

const byte WW_STR_APOSTROPHE[]              = {WW_APOSTROPHE_QUOTE_PARAGRAPH, WW_NULL_12, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_APOSTROPHE = {ELEMENT_SIMPLE, SPACING_FORWARD, 75, 0,
                                               TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_12 + TIME_RELEASE_NOSHIFT_12),
                                               &WW_STR_APOSTROPHE};

const byte WW_STR_QUOTE[]              = {WW_LShift, WW_APOSTROPHE_QUOTE_PARAGRAPH, WW_LShift, WW_NULL_1, WW_NULL_14,
                                          WW_NULL};
const struct print_info WW_PRINT_QUOTE = {ELEMENT_SIMPLE, SPACING_FORWARD, 74, 0,
                                          TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_12 + TIME_RELEASE_SHIFT_12),
                                          &WW_STR_QUOTE};

const byte WW_STR_HALF[]              = {WW_HALF_QUARTER_SUPER2, WW_NULL_12, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_HALF = {ELEMENT_SIMPLE, SPACING_FORWARD, 70, 0,
                                         TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_12 + TIME_RELEASE_NOSHIFT_12),
                                         &WW_STR_HALF};

const byte WW_STR_QUARTER[]              = {WW_LShift, WW_HALF_QUARTER_SUPER2, WW_LShift, WW_NULL_1, WW_NULL_14,
                                            WW_NULL};
const struct print_info WW_PRINT_QUARTER = {ELEMENT_SIMPLE, SPACING_FORWARD, 71, 0,
                                            TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_12 + TIME_RELEASE_SHIFT_12),
                                            &WW_STR_QUARTER};

const byte WW_STR_PLUSMINUS[]              = {WW_PLUSMINUS_DEGREE, WW_NULL_3, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_PLUSMINUS = {ELEMENT_SIMPLE, SPACING_FORWARD, 59, 0,
                                              TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_3 + TIME_RELEASE_NOSHIFT_3),
                                              &WW_STR_PLUSMINUS};

const byte WW_STR_DEGREE[]              = {WW_LShift, WW_PLUSMINUS_DEGREE, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_DEGREE = {ELEMENT_SIMPLE, SPACING_FORWARD, 67, 0,
                                           TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_3 + TIME_RELEASE_SHIFT_3),
                                           &WW_STR_DEGREE};

const byte WW_STR_SUPER2[]              = {WW_Code, WW_HALF_QUARTER_SUPER2, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_SUPER2 = {ELEMENT_SIMPLE, SPACING_FORWARD, 65, 0,
                                           TIME_HIT + TIME_MOVE - (TIME_PRESS_CODE_12 + TIME_RELEASE_CODE_12),
                                           &WW_STR_SUPER2};

const byte WW_STR_SUPER3[]              = {WW_Code, WW_RBRACKET_LBRACKET_SUPER3, WW_Code, WW_NULL_5, WW_NULL_14,
                                           WW_NULL};
const struct print_info WW_PRINT_SUPER3 = {ELEMENT_SIMPLE, SPACING_FORWARD, 66, 0,
                                           TIME_HIT + TIME_MOVE - (TIME_PRESS_CODE_10 + TIME_RELEASE_CODE_10),
                                           &WW_STR_SUPER3};

const byte WW_STR_SECTION[]              = {WW_Code, WW_SEMICOLON_COLON_SECTION, WW_Code, WW_NULL_5, WW_NULL_14,
                                            WW_NULL};
const struct print_info WW_PRINT_SECTION = {ELEMENT_SIMPLE, SPACING_FORWARD, 68, 0,
                                            TIME_HIT + TIME_MOVE - (TIME_PRESS_CODE_12 + TIME_RELEASE_CODE_12),
                                            &WW_STR_SECTION};

const byte WW_STR_PARAGRAPH[]              = {WW_Code, WW_APOSTROPHE_QUOTE_PARAGRAPH, WW_Code, WW_NULL_5, WW_NULL_14,
                                              WW_NULL};
const struct print_info WW_PRINT_PARAGRAPH = {ELEMENT_SIMPLE, SPACING_FORWARD, 69, 0,
                                              TIME_HIT + TIME_MOVE - (TIME_PRESS_CODE_12 + TIME_RELEASE_CODE_12),
                                              &WW_STR_PARAGRAPH};

// Bold, Caps, Word, Cont print strings.
const byte WW_STR_Bold[]              = {WW_Code, WW_b_B_Bold, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Bold = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                         TIME_NULL - (TIME_PRESS_CODE_8 + TIME_RELEASE_CODE_8),
                                         &WW_STR_Bold};

const byte WW_STR_Caps[]              = {WW_Code, WW_n_N_Caps, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Caps = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                         TIME_NULL - (TIME_PRESS_CODE_9 + TIME_RELEASE_CODE_9),
                                         &WW_STR_Caps};

const byte WW_STR_Word[]              = {WW_Code, WW_i_I_Word, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Word = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                         TIME_NULL - (TIME_PRESS_CODE_10 + TIME_RELEASE_CODE_10),
                                         &WW_STR_Word};

const byte WW_STR_Cont[]              = {WW_Code, WW_u_U_Cont, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Cont = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                         TIME_NULL - (TIME_PRESS_CODE_9 + TIME_RELEASE_CODE_9),
                                         &WW_STR_Cont};

// Spell, Add, Del, A Rtn, Lang, Line Space, Impr, Vol, Ctr, Dec T, R Flsh, IndL, Indr Clr, Re Prt, Stop print strings.
const byte WW_STR_Spell[]              = {WW_Code, WW_1_EXCLAMATION_Spell, WW_Code, WW_NULL_3, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Spell = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_RESET, 0,
                                          TIME_JIGGLE - (TIME_PRESS_CODE_3 + TIME_RELEASE_CODE_3),
                                          &WW_STR_Spell};

const byte WW_STR_Add[]              = {WW_Code, WW_2_AT_Add, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Add = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_RESET, 0,
                                        TIME_JIGGLE - (TIME_PRESS_CODE_6 + TIME_RELEASE_CODE_6),
                                        &WW_STR_Add};

const byte WW_STR_Del[]              = {WW_Code, WW_3_POUND_Del, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Del = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_RESET, 0,
                                        TIME_JIGGLE - (TIME_PRESS_CODE_7 + TIME_RELEASE_CODE_7),
                                        &WW_STR_Del};

const byte WW_STR_ARtn[]              = {WW_Code, WW_r_R_ARtn, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_ARtn = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                         TIME_NULL - (TIME_PRESS_CODE_8 + TIME_RELEASE_CODE_8),
                                         &WW_STR_ARtn};

const byte WW_STR_Lang[]              = {WW_Code, WW_l_L_Lang, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Lang = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                         TIME_NULL - (TIME_PRESS_CODE_11 + TIME_RELEASE_CODE_11),
                                         &WW_STR_Lang};

const byte WW_STR_LineSpace[]              = {WW_Code, WW_Reloc_LineSpace, WW_Code, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_LineSpace = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                              TIME_NULL - (TIME_PRESS_CODE_2 + TIME_RELEASE_CODE_2),
                                              &WW_STR_LineSpace};

const byte WW_STR_Impr[]              = {WW_Code, WW_q_Q_Impr, WW_Code, WW_NULL_3, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Impr = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                         TIME_NULL - (TIME_PRESS_CODE_3 + TIME_RELEASE_CODE_3),
                                         &WW_STR_Impr};

const byte WW_STR_Vol[]              = {WW_Code, WW_4_DOLLAR_Vol, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Vol = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                        TIME_NULL - (TIME_PRESS_CODE_8 + TIME_RELEASE_CODE_8),
                                        &WW_STR_Vol};

const byte WW_STR_Ctr[]              = {WW_Code, WW_c_C_Ctr, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Ctr = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                        TIME_NULL - (TIME_PRESS_CODE_7 + TIME_RELEASE_CODE_7),
                                        &WW_STR_Ctr};

const byte WW_STR_DecT[]              = {WW_Code, WW_d_D_DecT, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_DecT = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                         TIME_NULL - (TIME_PRESS_CODE_7 + TIME_RELEASE_CODE_7),
                                         &WW_STR_DecT};

const byte WW_STR_RFlsh[]              = {WW_Code, WW_o_O_RFlsh, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_RFlsh = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                          TIME_NULL - (TIME_PRESS_CODE_11 + TIME_RELEASE_CODE_11),
                                          &WW_STR_RFlsh};

const byte WW_STR_IndL[]              = {WW_Code, WW_Tab_IndL, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_IndL = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                         TIME_NULL - (TIME_PRESS_CODE_14 + TIME_RELEASE_CODE_14),
                                         &WW_STR_IndL};

const byte WW_STR_IndClr[]              = {WW_Code, WW_CRtn_IndClr, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_IndClr = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                           TIME_NULL - (TIME_PRESS_CODE_13 + TIME_RELEASE_CODE_13),
                                           &WW_STR_IndClr};

const byte WW_STR_RePrt[]              = {WW_Code, WW_MarRel_RePrt, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_RePrt = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_UNDEFINED, 0,
                                          TIME_UNKNOWN - (TIME_PRESS_CODE_14 + TIME_RELEASE_CODE_14),
                                          &WW_STR_RePrt};

const byte WW_STR_Stop[]              = {WW_Code, WW_9_LPAREN_Stop, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Stop = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                         TIME_NULL - (TIME_PRESS_CODE_11 + TIME_RELEASE_CODE_11),
                                         &WW_STR_Stop};

// <left arrow>, <right arrow>, <up arrow>, <down arrow>, <left word>, <right word>, <up line>, <down line>, Paper Up,
// Paper Down, <up micro>, <down micro>, <1/2 up>, <1/2 down>, Backspace, Bksp 1, <back x>, <back word>, Reloc print
// strings.
const byte WW_STR_LEFTARROW[]              = {WW_LEFTARROW_LeftWord, WW_NULL_13, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_LEFTARROW = {ELEMENT_SIMPLE, SPACING_BACKWARD, POSITION_NOCHANGE, 0,
                                              TIME_HMOVE - (TIME_PRESS_NOSHIFT_13 + TIME_RELEASE_NOSHIFT_13),
                                              &WW_STR_LEFTARROW};

const byte WW_STR_RIGHTARROW[]              = {WW_RIGHTARROW_RightWord, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_RIGHTARROW = {ELEMENT_SIMPLE, SPACING_FORWARD, POSITION_NOCHANGE, 0,
                                               TIME_HMOVE - (TIME_PRESS_NOSHIFT_2 + TIME_RELEASE_NOSHIFT_2),
                                               &WW_STR_RIGHTARROW};

const byte WW_STR_UPARROW[]              = {WW_UPARROW_UpLine, WW_NULL_13, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_UPARROW = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                            TIME_VMOVE - (TIME_PRESS_NOSHIFT_13 + TIME_RELEASE_NOSHIFT_13),
                                            &WW_STR_UPARROW};

const byte WW_STR_DOWNARROW[]              = {WW_DOWNARROW_DownLine, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_DOWNARROW = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                              TIME_VMOVE - (TIME_PRESS_NOSHIFT_2 + TIME_RELEASE_NOSHIFT_2),
                                              &WW_STR_DOWNARROW};

const byte WW_STR_LeftWord[]              = {WW_Code, WW_LEFTARROW_LeftWord, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_LeftWord = {ELEMENT_SIMPLE, SPACING_UNKNOWN, POSITION_NOCHANGE, 0,
                                             TIME_HMOVE - (TIME_PRESS_CODE_13 + TIME_RELEASE_CODE_13),
                                             &WW_STR_LeftWord};

const byte WW_STR_RightWord[]              = {WW_Code, WW_RIGHTARROW_RightWord, WW_Code, WW_NULL_2, WW_NULL_14,
                                              WW_NULL};
const struct print_info WW_PRINT_RightWord = {ELEMENT_SIMPLE, SPACING_UNKNOWN, POSITION_NOCHANGE, 0,
                                              TIME_HMOVE - (TIME_PRESS_CODE_2 + TIME_RELEASE_CODE_2),
                                              &WW_STR_RightWord};

const byte WW_STR_UpLine[]              = {WW_Code, WW_UPARROW_UpLine, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_UpLine = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                           TIME_VMOVE - (TIME_PRESS_CODE_13 + TIME_RELEASE_CODE_13),
                                           &WW_STR_UpLine};

const byte WW_STR_DownLine[]              = {WW_Code, WW_DOWNARROW_DownLine, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_DownLine = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                             TIME_VMOVE - (TIME_PRESS_CODE_2 + TIME_RELEASE_CODE_2),
                                             &WW_STR_DownLine};

const byte WW_STR_PaperUp[]              = {WW_PaperUp_UpMicro, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_PaperUp = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                            TIME_VMOVE_HALF - (TIME_PRESS_NOSHIFT_2 + TIME_RELEASE_NOSHIFT_2),
                                            &WW_STR_PaperUp};

const byte WW_STR_PaperDown[]              = {WW_PaperDown_DownMicro, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_PaperDown = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                              TIME_VMOVE_HALF - (TIME_PRESS_NOSHIFT_2 + TIME_RELEASE_NOSHIFT_2),
                                              &WW_STR_PaperDown};

const byte WW_STR_UpMicro[]              = {WW_Code, WW_PaperUp_UpMicro, WW_Code, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_UpMicro = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                            TIME_VMOVE_MICRO - (TIME_PRESS_CODE_2 + TIME_RELEASE_CODE_2),
                                            &WW_STR_UpMicro};

const byte WW_STR_DownMicro[]              = {WW_Code, WW_PaperDown_DownMicro, WW_Code, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_DownMicro = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                              TIME_VMOVE_MICRO - (TIME_PRESS_CODE_2 + TIME_RELEASE_CODE_2),
                                              &WW_STR_DownMicro};

const byte WW_STR_12UP[]              = {WW_Code, WW_y_Y_12UP, WW_Code, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_12UP = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                         TIME_VMOVE_HALF - (TIME_PRESS_CODE_9 + TIME_RELEASE_CODE_9),
                                         &WW_STR_12UP};

const byte WW_STR_12DOWN[]              = {WW_Code, WW_h_H_12DOWN, WW_Code, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_12DOWN = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                           TIME_VMOVE_HALF - (TIME_PRESS_CODE_9 + TIME_RELEASE_CODE_9),
                                           &WW_STR_12DOWN};

const byte WW_STR_Backspace[]              = {WW_Backspace_Bksp1, WW_NULL_13, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Backspace = {ELEMENT_SIMPLE, SPACING_BACKWARD, POSITION_NOCHANGE, 0,
                                              TIME_HMOVE - (TIME_PRESS_NOSHIFT_13 + TIME_RELEASE_NOSHIFT_13),
                                              &WW_STR_Backspace};

const byte WW_STR_Bksp1[]              = {WW_Code, WW_Backspace_Bksp1, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Bksp1 = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                          TIME_HMOVE_MICRO - (TIME_PRESS_CODE_13 + TIME_RELEASE_CODE_13),
                                          &WW_STR_Bksp1};

const byte WW_STR_BACKX[]              = {WW_BACKX_Word, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_BACKX = {ELEMENT_SIMPLE, SPACING_BACKWARD, POSITION_NOCHANGE, 0,
                                          TIME_HMOVE - (TIME_PRESS_NOSHIFT_14 + TIME_RELEASE_NOSHIFT_14),
                                          &WW_STR_BACKX};

const byte WW_STR_BWord[]              = {WW_Code, WW_BACKX_Word, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_BWord = {ELEMENT_SIMPLE, SPACING_UNKNOWN, POSITION_NOCHANGE, 0,
                                          TIME_HMOVE - (TIME_PRESS_CODE_14 + TIME_RELEASE_CODE_14),
                                          &WW_STR_BWord};

const byte WW_STR_Reloc[]              = {WW_Reloc_LineSpace, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_Reloc = {ELEMENT_SIMPLE, SPACING_UNKNOWN, POSITION_NOCHANGE, 0,
                                          TIME_UNKNOWN - (TIME_PRESS_NOSHIFT_2 + TIME_RELEASE_NOSHIFT_2),
                                          &WW_STR_Reloc};

// L Mar, R Mar, Mar Rel, T Set, T Clr, Clr All, Mar Tabs print strings.
const byte WW_STR_LMar[]              = {WW_LMar, WW_NULL_4, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_LMar = {ELEMENT_SIMPLE, SPACING_LMAR, POSITION_RESET, 0,
                                         TIME_JIGGLE - (TIME_PRESS_NOSHIFT_4 + TIME_RELEASE_NOSHIFT_4),
                                         &WW_STR_LMar};

const byte WW_STR_RMar[]              = {WW_RMar, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_RMar = {ELEMENT_SIMPLE, SPACING_RMAR, POSITION_RESET, 0,
                                         TIME_JIGGLE - (TIME_PRESS_NOSHIFT_14 + TIME_RELEASE_NOSHIFT_14),
                                         &WW_STR_RMar};

const byte WW_STR_MarRel[]               = {WW_MarRel_RePrt, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_MarRel  = {ELEMENT_SIMPLE, SPACING_MARREL, POSITION_RESET, 0,
                                            TIME_JIGGLE - (TIME_PRESS_NOSHIFT_14 + TIME_RELEASE_NOSHIFT_14),
                                            &WW_STR_MarRel};
const struct print_info WW_PRINT_MarRelX = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_RESET, 0,
                                            TIME_JIGGLE - (TIME_PRESS_NOSHIFT_14 + TIME_RELEASE_NOSHIFT_14),
                                            &WW_STR_MarRel};
                                              // Special case for Return_column_1() that always prints.

const byte WW_STR_TSet[]              = {WW_TSet, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_TSet = {ELEMENT_SIMPLE, SPACING_TSET, POSITION_RESET, 0,
                                         TIME_JIGGLE - (TIME_PRESS_NOSHIFT_14 + TIME_RELEASE_NOSHIFT_14),
                                         &WW_STR_TSet};

const byte WW_STR_TClr[]              = {WW_TClr_TClrAll, WW_NULL_4, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_TClr = {ELEMENT_SIMPLE, SPACING_TCLR, POSITION_RESET, 0,
                                         TIME_JIGGLE - (TIME_PRESS_NOSHIFT_4 + TIME_RELEASE_NOSHIFT_4),
                                         &WW_STR_TClr};

const byte WW_STR_TClrAll[]               = {WW_TClr_TClrAll, WW_CRtn_IndClr, WW_TClr_TClrAll, WW_NULL_4, WW_NULL_14,
                                             WW_NULL};
const struct print_info WW_PRINT_TClrAll  = {ELEMENT_SIMPLE, SPACING_TCLRALL, POSITION_RESET, 0,
                                             TIME_JIGGLE + TIME_RETURN_OFFSET - (TIME_PRESS_NOSHIFT_4 +
                                                                                 TIME_RELEASE_NOSHIFT_4),
                                             &WW_STR_TClrAll};
const struct print_info WW_PRINT_TClrAllX = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_RESET, 0,
                                             TIME_JIGGLE + TIME_RETURN_OFFSET - (TIME_PRESS_NOSHIFT_4 +
                                                                                 TIME_RELEASE_NOSHIFT_4),
                                             &WW_STR_TClrAll};
                                               // Special case for Set_margins_tabs() that doesn't clear tabs[].

const byte WW_STR_MARTABS[]              = {WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_MARTABS = {ELEMENT_SIMPLE, SPACING_MARTABS, POSITION_RESET, 0, 0, &WW_STR_MARTABS};

// Special function print strings.
      byte WW_STR_POWERWISE[]              = {WW_Code, WW_x_X_POWERWISE, WW_Code, WW_Code, WW_0_RPAREN, WW_Code,
                                              WW_Code, WW_0_RPAREN, WW_Code, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL}; 
const struct print_info WW_PRINT_POWERWISE = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                              TIME_NULL - (TIME_PRESS_CODE_6 + TIME_RELEASE_CODE_6),
                                              &WW_STR_POWERWISE};

const byte WW_STR_POWERWISE_OFF[]              = {WW_Code, WW_x_X_POWERWISE, WW_Code, WW_0_RPAREN, WW_Code, WW_NULL_5,
                                                  WW_NULL_14, WW_NULL}; 
const struct print_info WW_PRINT_POWERWISE_OFF = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                                  TIME_NULL - (TIME_PRESS_CODE_6 + TIME_RELEASE_CODE_6),
                                                  &WW_STR_POWERWISE_OFF};

const byte WW_STR_SPELL_CHECK[]              = {WW_Code, WW_1_EXCLAMATION_Spell, WW_Code, WW_NULL_3, WW_NULL_14,
                                                WW_NULL};
const struct print_info WW_PRINT_SPELL_CHECK = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                                TIME_NULL - (TIME_PRESS_CODE_3 + TIME_RELEASE_CODE_3),
                                                &WW_STR_SPELL_CHECK};

const byte WW_STR_BEEP[]              = {WW_Code, WW_x_X_POWERWISE, WW_Code, WW_9_LPAREN_Stop, WW_Code, WW_Code,
                                         WW_9_LPAREN_Stop, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info WW_PRINT_BEEP = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_RESET, 0,
                                         TIME_BEEP - (TIME_PRESS_CODE_6 + TIME_RELEASE_CODE_6),
                                         &WW_STR_BEEP};


//**********************************************************************************************************************
//
//  Wheelwriter print character set.
//
//**********************************************************************************************************************

const struct print_info *PRINT_CHARACTERS[] = {&WW_PRINT_a,           &WW_PRINT_n,         &WW_PRINT_r,
                                               &WW_PRINT_m,           &WW_PRINT_c,         &WW_PRINT_s,
                                               &WW_PRINT_d,           &WW_PRINT_h,         &WW_PRINT_l,
                                               &WW_PRINT_f,           &WW_PRINT_k,      /* &WW_PRINT_COMMA, */
                                               &WW_PRINT_V,           &WW_PRINT_HYPHEN,    &WW_PRINT_G,
                                               &WW_PRINT_U,           &WW_PRINT_F,         &WW_PRINT_B,
                                               &WW_PRINT_Z,           &WW_PRINT_H,         &WW_PRINT_P,
                                               &WW_PRINT_RPAREN,      &WW_PRINT_R,         &WW_PRINT_L,
                                               &WW_PRINT_S,           &WW_PRINT_N,         &WW_PRINT_C,
                                               &WW_PRINT_T,           &WW_PRINT_D,         &WW_PRINT_E,
                                               &WW_PRINT_I,           &WW_PRINT_A,         &WW_PRINT_J,
                                               &WW_PRINT_O,           &WW_PRINT_LPAREN,    &WW_PRINT_M,
                                               &WW_PRINT_PERIOD,      &WW_PRINT_Y,         &WW_PRINT_COMMA,
                                               &WW_PRINT_SLASH,       &WW_PRINT_W,         &WW_PRINT_9,
                                               &WW_PRINT_K,           &WW_PRINT_3,         &WW_PRINT_X,
                                               &WW_PRINT_1,           &WW_PRINT_2,         &WW_PRINT_0,
                                               &WW_PRINT_5,           &WW_PRINT_4,         &WW_PRINT_6,
                                               &WW_PRINT_8,           &WW_PRINT_7,         &WW_PRINT_ASTERISK,
                                               &WW_PRINT_DOLLAR,      &WW_PRINT_POUND,     &WW_PRINT_PERCENT,
                                               &WW_PRINT_CENT,        &WW_PRINT_PLUS,      &WW_PRINT_PLUSMINUS,
                                               &WW_PRINT_AT,          &WW_PRINT_Q,         &WW_PRINT_AMPERSAND,
                                               &WW_PRINT_RBRACKET,    &WW_PRINT_LBRACKET,  &WW_PRINT_SUPER2,
                                               &WW_PRINT_SUPER3,      &WW_PRINT_DEGREE,    &WW_PRINT_SECTION,
                                               &WW_PRINT_PARAGRAPH,   &WW_PRINT_HALF,      &WW_PRINT_QUARTER,
                                               &WW_PRINT_EXCLAMATION, &WW_PRINT_QUESTION,  &WW_PRINT_QUOTE,
                                               &WW_PRINT_APOSTROPHE,  &WW_PRINT_EQUAL,     &WW_PRINT_COLON,
                                               &WW_PRINT_UNDERSCORE,  &WW_PRINT_SEMICOLON, &WW_PRINT_x,
                                               &WW_PRINT_p,           &WW_PRINT_v,         &WW_PRINT_z,
                                               &WW_PRINT_w,           &WW_PRINT_j,      /* &WW_PRINT_PERIOD, */
                                               &WW_PRINT_y,           &WW_PRINT_b,         &WW_PRINT_g,
                                               &WW_PRINT_u,           &WW_PRINT_q,         &WW_PRINT_i,
                                               &WW_PRINT_t,           &WW_PRINT_o,         &WW_PRINT_e};
#define NUM_PRINT_CHARACTERS  94  // Number of unique printwheel characters.


//**********************************************************************************************************************
//
//  Common data declarations.
//
//**********************************************************************************************************************

// Serial port for RS-232 at slow baud rates.
SlowSoftSerial slow_serial_port (UART_RX_PIN, UART_TX_PIN);
volatile int rs232_mode = RS232_UNDEFINED;

// RS-232 variables.
volatile int rts_state = RTS_UNDEFINED;         // Current RTS state.
volatile int rts_size = 0;                      // Serial buffer size.
volatile unsigned long rts_timeout_time = 0UL;  // Time when RTS timeout

// Serial Interface Board variables.
volatile int blue_led_on = LOW;                 // Dynamic level to turn blue LED on.
volatile int blue_led_off = HIGH;               // Dynamic level to turn blue LED off.
volatile byte row_out_3_pin = ROW_OUT_3_PIN;    // Dynamic Row 3 output pin number.
volatile byte row_out_4_pin = ROW_OUT_4_PIN;    // Dynamic Row 4 output pin number.
volatile byte serial_rts_pin = SERIAL_RTS_PIN;  // Dynamic RTS pin number.
volatile byte serial_cts_pin = SERIAL_CTS_PIN;  // Dynamic CTS pin number.

// Shift state variables.
volatile boolean shift = FALSE;         // Current shift state.
volatile boolean shift_lock = FALSE;    // Current shift lock state.
volatile boolean code = FALSE;          // Current code shift state.
volatile int key_offset = OFFSET_NONE;  // Offset into action table for shifted keys.

// Escape sequence variables.
volatile boolean escaping = FALSE;           // Currently in escape sequence.
volatile int escape_state = ESC_INIT_STATE;  // Current escape FSA state.

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

// Column scan timing variables.
volatile boolean in_column_scan_timing = FALSE;                 // Currently in column scan timing.
volatile boolean press_space_key = FALSE;                       // Assert space key.
volatile boolean flood_a_5_keys = FALSE;                        // Flood typewriter with 'a' & '5' keys at maximum
                                                                // speed.
volatile int column_scan_count = 0;                             // Count of column scans.
volatile int flood_key_count = 0;                               // Count of flood keys.
volatile unsigned long last_interrupt_time_microseconds = 0UL;  // Time of last interrupt.
volatile unsigned long interrupt_time_microseconds = 0UL;       // Time of current interrupt.
volatile unsigned long elapsed_time_microseconds = 0UL;         // Elapsed time of last column scan.
volatile unsigned long total_scan_time = 0UL;                   // Total column scan time.
volatile unsigned long minimum_scan_time = 0UL;                 // Minimum column scan time.
volatile unsigned long maximum_scan_time = 0UL;                 // Maximum column scan time.
volatile unsigned long total_key_pressed_scan_time = 0UL;       // Total key pressed column scan time.
volatile unsigned long minimum_key_pressed_scan_time = 0UL;     // Minimum key pressed column scan time.
volatile unsigned long maximum_key_pressed_scan_time = 0UL;     // Maximum key pressed column scan time.
volatile unsigned long total_key_released_scan_time = 0UL;      // Total key released column scan time.
volatile unsigned long minimum_key_released_scan_time = 0UL;    // Minimum key released column scan time.
volatile unsigned long maximum_key_released_scan_time = 0UL;    // Maximum key released column scan time.
volatile unsigned long flood_key_scan_time = 0UL;               // Flood space key column scan time.

// Printer timing variables.
volatile boolean in_printer_timing = FALSE;      // Currently in printer timing.
volatile boolean in_full_speed_test = FALSE;     // Currently doing a full speed test.
volatile boolean count_characters = FALSE;       // Count elements printed.
volatile int print_character_count = 0;          // Count of characters printed.
volatile long start_print_time = 0UL;            // Start time of timing print (in msec).
volatile long last_print_time = 0UL;             // Elapsed time at last long scan (in msec).
volatile int last_character_count = 0;           // Count of characters at last long scan.
volatile int buffer_size = FEATURE_BUFFER_SIZE;  // Size of Wheelwriter input buffer.

// POST LED variables.
volatile boolean orange_led = FALSE;  // Orange LED status during POST.
volatile int blue_led_count = 0;      // Blue LED counter during POST.

// Error text table.
const char* ERROR_TEXT[NUM_ERRORS] = {NULL,
                                      "Command buffer full errors.",
                                      "Receive buffer full errors.",
                                      "Send buffer full errors.",
                                      "Transfer buffer full errors.",
                                      "Print buffer full errors.",
                                      "Bad print code errors.",
                                      "Bad escape sequence errors.",
                                      "Bad element type code errors.",
                                      "Bad print spacing code errors.",
                                      "Bad printwheel position code errors.",
                                      "Bad key action code errors.",
                                      "Bad serial action code errors."};

// Warning text table.
const char* WARNING_TEXT[NUM_WARNINGS] = {NULL,
                                          "Short column scan warnings.",
                                          "Long column scan warnings.",
                                          "Unexpected column scan warnings.",
                                          "Serial input incorrect parity warnings"};

// Row output pin table.
volatile byte row_out_pins[16] = {ROW_OUT_NO_PIN, ROW_OUT_1_PIN, ROW_OUT_2_PIN, ROW_OUT_3_PIN, ROW_OUT_4_PIN,
                                  ROW_OUT_5_PIN, ROW_OUT_6_PIN, ROW_OUT_7_PIN, ROW_OUT_8_PIN, ROW_OUT_NO_PIN,
                                  ROW_OUT_NO_PIN, ROW_OUT_NO_PIN, ROW_OUT_NO_PIN, ROW_OUT_NO_PIN, ROW_OUT_NO_PIN,
                                  ROW_OUT_NO_PIN};

// Databits parity stopbits tables.
const int DATA_PARITY_STOPS[NUM_DPSS]       = {0, SERIAL_7O1, SERIAL_7E1, SERIAL_8N1, SERIAL_8O1, SERIAL_8E1,
                                               SERIAL_8N2, SERIAL_8O2, SERIAL_8E2};
const int DATA_PARITY_STOPS_SLOW[NUM_DPSS]  = {0, SSS_SERIAL_7O1, SSS_SERIAL_7E1, SSS_SERIAL_8N1, SSS_SERIAL_8O1,
                                               SSS_SERIAL_8E1, SSS_SERIAL_8N2, SSS_SERIAL_8O2, SSS_SERIAL_8E2};
const char* DATA_PARITY_STOPS_TEXT[NUM_DPSS] = {NULL, "7o1", "7e1", "8n1", "8o1", "8e1", "8n2", "8o2", "8e2"};

// Baud rate table.  Note: Baud rates below 1200 are not usable with the hardware UART on Teensy 3.5, but are supported
//                         by the SlowSoftSerial library.
//                         Baud rates above 230400 are not supported by the MAX3232 chip.
const int BAUD_RATES[NUM_BAUDS] = {0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800, 9600, 19200, 38400,
                                   57600, 76800, 115200, 230400, 460800, 921600};

// Parity lookup tables.
const byte ODD_PARITY[128]  = {0x80, 0x01, 0x02, 0x83, 0x04, 0x85, 0x86, 0x07,
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
const byte EVEN_PARITY[128] = {0x00, 0x81, 0x82, 0x03, 0x84, 0x05, 0x06, 0x87,
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
const boolean PC_VALIDATION[256] = {
        TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
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
        FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,  TRUE,  TRUE};

// Escape character table.
const byte ESCAPE_CHARACTER[128] = {ESC_IGNORE,       ESC_CONTROL,      ESC_CONTROL,      ESC_CONTROL,
                                    ESC_CONTROL,      ESC_CONTROL,      ESC_CONTROL,      ESC_CTRL_BEL,
                                    ESC_CTRL_BS_CR,   ESC_CTRL_BS_CR,   ESC_CTRL_BS_CR,   ESC_CTRL_BS_CR,
                                    ESC_CTRL_BS_CR,   ESC_CTRL_BS_CR,   ESC_CONTROL,      ESC_CONTROL,
                                    ESC_CONTROL,      ESC_CONTROL,      ESC_CONTROL,      ESC_CONTROL,
                                    ESC_CONTROL,      ESC_CONTROL,      ESC_CONTROL,      ESC_CONTROL,
                                    ESC_CTRL_CAN_SUB, ESC_CONTROL,      ESC_CTRL_CAN_SUB, ESC_CTRL_ESC,
                                    ESC_CONTROL,      ESC_CONTROL,      ESC_CONTROL,      ESC_CONTROL,
                                    ESC_INTERMEDIATE, ESC_INTERMEDIATE, ESC_INTERMEDIATE, ESC_INTERMEDIATE,
                                    ESC_INTERMEDIATE, ESC_INTERMEDIATE, ESC_INTERMEDIATE, ESC_INTERMEDIATE,
                                    ESC_INTERMEDIATE, ESC_INTERMEDIATE, ESC_INTERMEDIATE, ESC_INTERMEDIATE,
                                    ESC_INTERMEDIATE, ESC_INTERMEDIATE, ESC_INTERMEDIATE, ESC_INTERMEDIATE,
                                    ESC_PARAMETER,    ESC_PARAMETER,    ESC_PARAMETER,    ESC_PARAMETER,
                                    ESC_PARAMETER,    ESC_PARAMETER,    ESC_PARAMETER,    ESC_PARAMETER,
                                    ESC_PARAMETER,    ESC_PARAMETER,    ESC_PARAMETER,    ESC_PARAMETER,
                                    ESC_PARAMETER,    ESC_PARAMETER,    ESC_PARAMETER,    ESC_PARAMETER,
                                    ESC_UPPERCASE,    ESC_UPPERCASE,    ESC_UPPERCASE,    ESC_UPPERCASE,
                                    ESC_UPPERCASE,    ESC_UPPERCASE,    ESC_UPPERCASE,    ESC_UPPERCASE,
                                    ESC_UPPERCASE,    ESC_UPPERCASE,    ESC_UPPERCASE,    ESC_UPPERCASE,
                                    ESC_UPPERCASE,    ESC_UPPERCASE,    ESC_UPPERCASE,    ESC_UPPERCASE,
                                    ESC_UC_STRING,    ESC_UPPERCASE,    ESC_UPPERCASE,    ESC_UPPERCASE,
                                    ESC_UPPERCASE,    ESC_UPPERCASE,    ESC_UPPERCASE,    ESC_UPPERCASE,
                                    ESC_UC_STRING,    ESC_UPPERCASE,    ESC_UPPERCASE,    ESC_UC_LBRACKET,
                                    ESC_UC_BSLASH,    ESC_UC_STRING,    ESC_UC_STRING,    ESC_UC_STRING,
                                    ESC_LOWERCASE,    ESC_LOWERCASE,    ESC_LOWERCASE,    ESC_LOWERCASE,
                                    ESC_LOWERCASE,    ESC_LOWERCASE,    ESC_LOWERCASE,    ESC_LOWERCASE,
                                    ESC_LOWERCASE,    ESC_LOWERCASE,    ESC_LOWERCASE,    ESC_LOWERCASE,
                                    ESC_LOWERCASE,    ESC_LOWERCASE,    ESC_LOWERCASE,    ESC_LOWERCASE,
                                    ESC_LOWERCASE,    ESC_LOWERCASE,    ESC_LOWERCASE,    ESC_LOWERCASE,
                                    ESC_LOWERCASE,    ESC_LOWERCASE,    ESC_LOWERCASE,    ESC_LOWERCASE,
                                    ESC_LOWERCASE,    ESC_LOWERCASE,    ESC_LOWERCASE,    ESC_LOWERCASE,
                                    ESC_LOWERCASE,    ESC_LOWERCASE,    ESC_LOWERCASE,    ESC_IGNORE};

// Escape FSA table.
const byte ESCAPE_FSA[5][13] = {{ESC_NONE,  ESC_PRINT, ESC_PRINT, ESC_PRINT, ESC_EXIT, ESC_INIT, ESC_EXIT,
                                 ESC_FUNC,  ESC_EXIT,  ESC_STR,   ESC_CSIP,  ESC_EXIT, ESC_EXIT},
                                {ESC_NONE,  ESC_PRINT, ESC_PRINT, ESC_PRINT, ESC_EXIT, ESC_INIT, ESC_NONE,
                                 ESC_CSII,  ESC_EXIT,  ESC_EXIT,  ESC_EXIT,  ESC_EXIT, ESC_EXIT},
                                {ESC_NONE,  ESC_PRINT, ESC_PRINT, ESC_PRINT, ESC_EXIT, ESC_INIT, ESC_ERROR,
                                 ESC_NONE,  ESC_EXIT,  ESC_EXIT,  ESC_EXIT,  ESC_EXIT, ESC_EXIT},
                                {ESC_NONE,  ESC_PRINT, ESC_PRINT, ESC_PRINT, ESC_EXIT, ESC_INIT, ESC_EXIT,
                                 ESC_NONE,  ESC_EXIT,  ESC_EXIT,  ESC_EXIT,  ESC_EXIT, ESC_EXIT},
                                {ESC_NONE,  ESC_PRINT, ESC_EXIT,  ESC_NONE,  ESC_EXIT, ESC_INIT, ESC_NONE,
                                 ESC_NONE,  ESC_NONE,  ESC_NONE,  ESC_NONE,  ESC_NONE, ESC_NONE}};

// Test print string.
const char* TEST_PRINT_STRING = "'Twas brillig, and the slithy toves\r"
                                "\tDid gyre and gimble in the wabe:\r"
                                "All mimsy were the borogoves,\r"
                                "\tAnd the mome raths outgrabe.\r\r"

                                "\"Beware the Jabberwock, my son!\r"
                                "\tThe jaws that bite, the claws that catch!\r"
                                "Beware the Jubjub bird, and shun\r"
                                "\tThe frumious Bandersnatch!\"\r\r"

                                "He took his vorpal sword in hand;\r"
                                "\tLong time the manxome foe he sought-\r"
                                "So rested he by the Tumtum tree\r"
                                "\tAnd stood awhile in thought.\r\r"

                                "And, as in uffish thought he stood,\r"
                                "\tThe Jabberwock, with eyes of flame,\r"
                                "Came whiffling through the tulgey wood,\r"
                                "\tAnd burbled as it came!\r\r"

                                "One, two! One, two! And through and through\r"
                                "\tThe vorpal blade went snicker-snack!\r"
                                "He left it dead, and with its head\r"
                                "\tHe went galumphing back.\r\r"

                                "\"And hast thou slain the Jabberwock?\r"
                                "\tCome to my arms, my beamish boy!\r"
                                "O frabjous day! Callooh! Callay!\"\r"
                                "\tHe chortled in his joy.\r\r"

                                "'Twas brillig, and the slithy toves\r"
                                "\tDid gyre and gimble in the wabe:\r"
                                "All mimsy were the borogoves,\r"
                                "\tAnd the mome raths outgrabe.";
#define NUM_TEST_PRINT_STRING  981  // Length of test print string.

// Typewriter settings strings.
const char* TYPEWRITER_LEGEND = "  1. spell check   2. line spacing   3. powerwise   4. a rtn     5. bold\r"
                                "  6. change margins & tabs           7. restore margins & tabs   Q. QUIT";
const char* TYPEWRITER_PROMPT = "  cmd> ";
const char* TYPEWRITER_SPELL_CHECK = "spell check: on = beep, off = printwheel jiggle  (must be off)";
const char* TYPEWRITER_LINE_SPACING = "line spacing: see 1, 1 1/2, 2, 3 indicators";
const char* TYPEWRITER_POWERWISE = "powerwise: 0 (off) - 90 minutes inactivity, enter value = ";
const char* TYPEWRITER_ARTN = "a rtn: see A Rtn indicator  (must be off)";
const char* TYPEWRITER_BOLD = "bold: see Bold indicator";
const char* TYPEWRITER_CHANGE_MARGINS_TABS = "change margins & tabs using:\r"
                                             "       left & right arrows = move carriage position\r"
                                             "                       Tab = tab to next tab stop\r"
                                             "                Code-C Rtn = return carriage\r"
                                             "                  Code-Esc = release left margin\r"
                                             "                     L Mar = set left margin (L)\r"
                                             "                     R Mar = set right margin (R)\r"
                                             "                     T Set = set tab (+)\r"
                                             "                     T Clr = clear tab (-)\r"
                                             "                Code-T Clr = clear all tabs (=)\r"
                                             "                     C Rtn = end margin & tab changing";
const char* TYPEWRITER_CHANGE_MARGINS_TABSX = "change margins & tabs";
const char* TYPEWRITER_RESTORE_MARGINS_TABS = "restore margins & tabs";
const char* TYPEWRITER_QUIT = "quit";

// Digit characters.
const byte DIGIT_CHARACTERS[] = {WW_0_RPAREN, WW_1_EXCLAMATION_Spell, WW_2_AT_Add, WW_3_POUND_Del, WW_4_DOLLAR_Vol,
                                 WW_5_PERCENT, WW_6_CENT, WW_7_AMPERSAND, WW_8_ASTERISK, WW_9_LPAREN_Stop};

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
volatile int current_column = INITIAL_LMARGIN;              // Current typewriter print column.
volatile int current_position = POSITION_INITIAL;           // Current rotational position of printwheel.
volatile boolean in_composite_character = FALSE;            // Processing composite character.
volatile boolean in_margin_tab_setting = FALSE;             // Processing margin and tab setting.
volatile const struct print_info *previous_element = NULL;  // Previous print element.
volatile unsigned long shortest_scan_duration = 0UL;        // Duration of shortest full scan.
volatile unsigned long longest_scan_duration = 0UL;         // Duration of longest full scan.
volatile byte bad_print_code = 0;                           // Bad print code.

// Software flow control variables.
volatile boolean flow_in_on = TRUE;   // Input flow control turned on.
volatile boolean flow_out_on = TRUE;  // Output flow control turned on.
volatile byte flow_on = 0x00;         // Turn on flow control character.
volatile byte flow_off = 0x00;        // Turn off flow control character.

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
volatile int tb_read = 0;   // Index of current read position of transfer buffer.
volatile int tb_write = 0;  // Index of current write position of transfer buffer.
volatile int tb_count = 0;  // Count of of characters in transfer buffer.
volatile const struct print_info *transfer_buffer[SIZE_TRANSFER_BUFFER] = {NULL};  // Circular transfer buffer.

// Print buffer variables (write: main, read: ISR).
volatile int pb_read = 0;   // Index of current read position of print buffer.
volatile int pb_write = 0;  // Index of current write position of print buffer.
volatile int pb_count = 0;  // Count of of characters in print buffer.
volatile byte print_buffer[SIZE_PRINT_BUFFER] = {0};  // Circular print buffer.

// Configuration parameters stored in EEPROM.
volatile byte emulation = INITIAL_EMULATION;            // Current emulation.           Used by all emulations.
volatile byte errors = INITIAL_ERRORS;                  // Report errors.               Used by all emulations.
volatile byte warnings = INITIAL_WARNINGS;              // Report warnings.             Used by all emulations.
volatile byte battery = INITIAL_BATTERY;                // Battery installed.           Used by all emulations.
volatile byte lmargin = INITIAL_LMARGIN;                // Left margin.                 Used by all emulations.
volatile byte rmargin = INITIAL_RMARGIN;                // Right margin.                Used by all emulations.
volatile byte offset = INITIAL_OFFSET;                  // Column offset.               Used by all emulations.
volatile byte asciiwheel = INITIAL_ASCIIWHEEL;          // ASCII printwheel.            Used by all emulations.
volatile byte impression = INITIAL_IMPRESSION;          // Impression.                  Used by all emulations.

volatile byte slash = INITIAL_SLASH;                    // Print slashed zeroes.        Used by IBM 1620 Jr.
volatile byte bold = INITIAL_BOLD;                      // Print bold input.            Used by IBM 1620 Jr.

volatile byte serial = INITIAL_SERIAL;                  // Serial type.                 Used by ASCII Terminal.
volatile byte duplex = INITIAL_DUPLEX;                  // Duplex.                      Used by ASCII Terminal.
volatile byte baud = INITIAL_BAUD;                      // Baud rate.                   Used by ASCII Terminal (RS-232).
volatile byte parity = INITIAL_PARITY;                  // Parity.                      Used by ASCII Terminal (USB).
volatile byte dps = INITIAL_DPS;                        // Databits, parity, stopbits.  Used by ASCII Terminal (RS-232).
volatile byte swflow = INITIAL_SWFLOW;                  // Software flow control.       Used by ASCII Terminal.
volatile byte hwflow = INITIAL_HWFLOW;                  // Hardware flow control.       Used by ASCII Terminal (RS-232).
volatile byte uppercase = INITIAL_UPPERCASE;            // Uppercase only.              Used by ASCII Terminal.
volatile byte autoreturn = INITIAL_AUTORETURN;          // Auto return.                 Used by ASCII Terminal.
volatile byte transmiteol = INITIAL_TRANSMITEOL;        // Send end-of-line.            Used by ASCII Terminal.
volatile byte receiveeol = INITIAL_RECEIVEEOL;          // Receive end-of-line.         Used by ASCII Terminal.
volatile byte escapesequence = INITIAL_ESCAPESEQUENCE;  // Escape sequence.             Used by ASCII Terminal.
volatile byte length = INITIAL_LENGTH;                  // Line length.                 Used by ASCII Terminal.
volatile byte tabs[200] = {SETTING_UNDEFINED};          // Tab settings.                Used by ASCII Terminal.



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
#define IBM_KEY_LSHIFT                  WW_KEY_LShift                      // <left shift> key code.
#define IBM_KEY_RSHIFT                  WW_KEY_RShift                      // <right shift> key code.
#define IBM_KEY_RIGHTARROW              WW_KEY_RIGHTARROW_RightWord        // <right arrow> key code.
#define IBM_KEY_DOWNARROW               WW_KEY_DOWNARROW_DownLine          // <down arrow> key code.
#define IBM_KEY_Z                       WW_KEY_z_Z                         // Z key code.
#define IBM_KEY_Q                       WW_KEY_q_Q_Impr                    // Q key code.
#define IBM_KEY_RELEASESTART            WW_KEY_PLUSMINUS_DEGREE            // <release start> key code.
#define IBM_KEY_A                       WW_KEY_a_A                         // A key code.
#define IBM_KEY_SPACE                   WW_KEY_SPACE_REQSPACE              // <space> key code.
#define IBM_KEY_LOADPAPER_SETTOPOFFORM  WW_KEY_LOADPAPER_SETTOPOFFORM      // <load paper>, <set top of form> key code.
#define IBM_KEY_LMAR                    WW_KEY_LMar                        // <left margin> key code.
#define IBM_KEY_TCLR_TCLRALL            WW_KEY_TClr_TClrAll                // <tab clear>, <tab clear all> key code.
#define IBM_KEY_X                       WW_KEY_x_X_POWERWISE               // X key code.
#define IBM_KEY_W                       WW_KEY_w_W                         // W key code.
#define IBM_KEY_S                       WW_KEY_s_S                         // S key code.
#define IBM_KEY_C                       WW_KEY_c_C_Ctr                     // C key code.
#define IBM_KEY_E                       WW_KEY_e_E                         // E key code.
#define IBM_KEY_D                       WW_KEY_d_D_DecT                    // D key code.
#define IBM_KEY_B                       WW_KEY_b_B_Bold                    // B key code.
#define IBM_KEY_V                       WW_KEY_v_V                         // V key code.
#define IBM_KEY_T                       WW_KEY_t_T                         // T key code.
#define IBM_KEY_R                       WW_KEY_r_R_ARtn                    // R key code.
#define IBM_KEY_AT                      WW_KEY_4_DOLLAR_Vol                // @ key code.
#define IBM_KEY_LPAREN                  WW_KEY_5_PERCENT                   // ( key code.
#define IBM_KEY_F                       WW_KEY_f_F                         // F key code.
#define IBM_KEY_G                       WW_KEY_g_G                         // G key code.
#define IBM_KEY_N                       WW_KEY_n_N_Caps                    // N key code.
#define IBM_KEY_M_7                     WW_KEY_m_M                         // M, 7 key code.
#define IBM_KEY_Y                       WW_KEY_y_Y_12UP                    // Y key code.
#define IBM_KEY_U_1                     WW_KEY_u_U_Cont                    // U, 1 key code.
#define IBM_KEY_FLAG                    WW_KEY_7_AMPERSAND                 // <flag> key code.
#define IBM_KEY_RPAREN                  WW_KEY_6_CENT                      // ) key code.
#define IBM_KEY_J_4                     WW_KEY_j_J                         // J, 4 key code.
#define IBM_KEY_H                       WW_KEY_h_H_12DOWN                  // H key code.
#define IBM_KEY_COMMA_8                 WW_KEY_COMMA_COMMA                 // ,, 8 key code.
#define IBM_KEY_I_2                     WW_KEY_i_I_Word                    // I, 2 key code.
#define IBM_KEY_EQUAL                   WW_KEY_8_ASTERISK                  // = key code.
#define IBM_KEY_GMARK                   WW_KEY_EQUAL_PLUS                  // <group mark> key code.
#define IBM_KEY_K_5                     WW_KEY_k_K                         // K, 5 key code.
#define IBM_KEY_PERIOD_9                WW_KEY_PERIOD_PERIOD               // ., 9 key code.
#define IBM_KEY_O_3                     WW_KEY_o_O_RFlsh                   // O, 3 key code.
#define IBM_KEY_0                       WW_KEY_9_LPAREN_Stop               // 0 key code.
#define IBM_KEY_L_6                     WW_KEY_l_L_Lang                    // L, 6 key code.
#define IBM_KEY_SLASH                   WW_KEY_SLASH_QUESTION              // / key code.
#define IBM_KEY_HYPHEN                  WW_KEY_HALF_QUARTER_SUPER2         // - key code.
#define IBM_KEY_P                       WW_KEY_p_P                         // P key code.
#define IBM_KEY_ASTERISK_PERIOD         WW_KEY_0_RPAREN                    // *, . key code.
#define IBM_KEY_RMARK                   WW_KEY_HYPHEN_UNDERSCORE           // <record mark> key code.
#define IBM_KEY_PLUS                    WW_KEY_SEMICOLON_COLON_SECTION     // + key code.
#define IBM_KEY_DOLLAR                  WW_KEY_APOSTROPHE_QUOTE_PARAGRAPH  // $ key code.
#define IBM_KEY_LEFTARROW               WW_KEY_LEFTARROW_LeftWord          // <left arrow> key code.
#define IBM_KEY_UPARROW                 WW_KEY_UPARROW_UpLine              // <up arrow> key code.
#define IBM_KEY_BACKSPACE               WW_KEY_Backspace_Bksp1             // <backspace> key code.
#define IBM_KEY_RETURN                  WW_KEY_CRtn_IndClr                 // <return> key code.
#define IBM_KEY_LOCK                    WW_KEY_Lock                        // <shift lock> key code.
#define IBM_KEY_RMAR                    WW_KEY_RMar                        // <right margin> key code.
#define IBM_KEY_TAB                     WW_KEY_Tab_IndL                    // <tab> key code.
#define IBM_KEY_MARREL                  WW_KEY_MarRel_RePrt                // <margin release> key code.
#define IBM_KEY_TSET                    WW_KEY_TSet                        // <tab set> key code.

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
//  IBM 1620 Jr. dynamic print string.
//
//**********************************************************************************************************************

// Dynamic print strings.
volatile const struct print_info *ibm_print_zero;
volatile const struct print_info *ibm_print_rmark;
volatile const struct print_info *ibm_print_gmark;


//**********************************************************************************************************************
//
//  IBM 1620 Jr. print strings.
//
//**********************************************************************************************************************

// <flag>, <slash 0>, <flag slash 0>, <flag 0>, <flag 1>, <flag 2>, <flag 3>, <flag 4>, <flag 5>, <flag 6>, <flag 7>,
// <flag 8>, <slash 9> print strings.
const struct print_info *IBM_STR_FLAG[] = {&WW_PRINT_12UP, &WW_PRINT_HYPHEN, &WW_PRINT_Backspace, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG  = {ELEMENT_COMPOSITE, SPACING_NONE, 0, 0, 0, &IBM_STR_FLAG};

const struct print_info *IBM_STR_SLASH_0[] = {&WW_PRINT_0, &WW_PRINT_Backspace, &WW_PRINT_SLASH, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_SLASH_0  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_SLASH_0};
const struct print_info IBM_PRINT_0        = {ELEMENT_DYNAMIC, 0, 0, 0, 0, &ibm_print_zero};

const struct print_info *IBM_STR_FLAG_SLASH_0[] = {&IBM_PRINT_FLAG, &IBM_PRINT_SLASH_0, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG_SLASH_0  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_FLAG_SLASH_0};

const struct print_info *IBM_STR_FLAG_0[] = {&IBM_PRINT_FLAG, &IBM_PRINT_0, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG_0  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_FLAG_0};

const struct print_info *IBM_STR_FLAG_1[] = {&IBM_PRINT_FLAG, &WW_PRINT_1, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG_1  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_FLAG_1};

const struct print_info *IBM_STR_FLAG_2[] = {&IBM_PRINT_FLAG, &WW_PRINT_2, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG_2  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_FLAG_2};

const struct print_info *IBM_STR_FLAG_3[] = {&IBM_PRINT_FLAG, &WW_PRINT_3, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG_3  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_FLAG_3};

const struct print_info *IBM_STR_FLAG_4[] = {&IBM_PRINT_FLAG, &WW_PRINT_4, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG_4  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_FLAG_4};

const struct print_info *IBM_STR_FLAG_5[] = {&IBM_PRINT_FLAG, &WW_PRINT_5, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG_5  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_FLAG_5};

const struct print_info *IBM_STR_FLAG_6[] = {&IBM_PRINT_FLAG, &WW_PRINT_6, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG_6  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_FLAG_6};

const struct print_info *IBM_STR_FLAG_7[] = {&IBM_PRINT_FLAG, &WW_PRINT_7, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG_7  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_FLAG_7};

const struct print_info *IBM_STR_FLAG_8[] = {&IBM_PRINT_FLAG, &WW_PRINT_8, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG_8  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_FLAG_8};

const struct print_info *IBM_STR_FLAG_9[] = {&IBM_PRINT_FLAG, &WW_PRINT_9, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG_9  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_FLAG_9};

// <flag numblank> print string.
const struct print_info *IBM_STR_FLAG_NUMBLANK[] = {&IBM_PRINT_FLAG, &WW_PRINT_AT, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG_NUMBLANK  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0,
                                                    &IBM_STR_FLAG_NUMBLANK};

// <rmark>, <flag rmark>, <gmark>, <flag gmark> print strings.
const struct print_info *IBM_STR_RMARK_IPW[] = {&WW_PRINT_EQUAL, &WW_PRINT_Backspace, &WW_PRINT_EXCLAMATION,
                                                &WW_PRINT_Backspace, &WW_PRINT_i, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_RMARK_IPW  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_RMARK_IPW};
const struct print_info *IBM_STR_RMARK_APW[] = {&WW_PRINT_EQUAL, &WW_PRINT_Backspace, &WW_PRINT_EXCLAMATION,
                                                NULL_PRINT_INFO};
const struct print_info IBM_PRINT_RMARK_APW  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_RMARK_APW};
const struct print_info IBM_PRINT_RMARK      = {ELEMENT_DYNAMIC, 0, 0, 0, 0, &ibm_print_rmark};

const struct print_info *IBM_STR_FLAG_RMARK_IPW[] = {&IBM_PRINT_FLAG, &IBM_PRINT_RMARK_IPW, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG_RMARK_IPW  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0,
                                                     &IBM_STR_FLAG_RMARK_IPW};
const struct print_info *IBM_STR_FLAG_RMARK[]     = {&IBM_PRINT_FLAG, &IBM_PRINT_RMARK, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG_RMARK      = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_FLAG_RMARK};

const struct print_info *IBM_STR_GMARK_IPW[] = {&WW_PRINT_UpMicro, &WW_PRINT_EQUAL, &WW_PRINT_Backspace,
                                                &WW_PRINT_DownMicro, &WW_PRINT_DownMicro, &WW_PRINT_DownMicro,
                                                &WW_PRINT_HYPHEN, &WW_PRINT_Backspace, &WW_PRINT_UpMicro,
                                                &WW_PRINT_UpMicro, &WW_PRINT_EXCLAMATION, &WW_PRINT_Backspace,
                                                &WW_PRINT_i, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_GMARK_IPW  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_GMARK_IPW};
const struct print_info *IBM_STR_GMARK_APW[] = {&WW_PRINT_UpMicro, &WW_PRINT_EQUAL, &WW_PRINT_Backspace,
                                                &WW_PRINT_DownMicro, &WW_PRINT_DownMicro, &WW_PRINT_DownMicro,
                                                &WW_PRINT_HYPHEN, &WW_PRINT_Backspace, &WW_PRINT_UpMicro,
                                                &WW_PRINT_UpMicro, &WW_PRINT_EXCLAMATION, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_GMARK_APW  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_GMARK_APW};
const struct print_info IBM_PRINT_GMARK      = {ELEMENT_DYNAMIC, 0, 0, 0, 0, &ibm_print_gmark};

const struct print_info *IBM_STR_FLAG_GMARK_IPW[] = {&IBM_PRINT_FLAG, &IBM_PRINT_GMARK_IPW, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG_GMARK_IPW  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0,
                                                     &IBM_STR_FLAG_GMARK_IPW};
const struct print_info *IBM_STR_FLAG_GMARK[]     = {&IBM_PRINT_FLAG, &IBM_PRINT_GMARK, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_FLAG_GMARK      = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_FLAG_GMARK};

// <release start>, <invalid> print strings.
const struct print_info *IBM_STR_RELEASESTART[] = {&WW_PRINT_DownMicro, &WW_PRINT_DownMicro, &WW_PRINT_R,
                                                   &WW_PRINT_Bksp1, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1,
                                                   &WW_PRINT_PaperUp, &WW_PRINT_S, &WW_PRINT_DownMicro,
                                                   &WW_PRINT_DownMicro, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_RELEASESTART  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_RELEASESTART};

const struct print_info *IBM_STR_INVALID[] = {&WW_PRINT_X, &WW_PRINT_Backspace, &WW_PRINT_EXCLAMATION,
                                              &WW_PRINT_Backspace, &WW_PRINT_i, NULL_PRINT_INFO};
const struct print_info IBM_PRINT_INVALID  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &IBM_STR_INVALID};


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
  {CMD_PRINT,       &IBM_PRINT_0},              // 0
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
  {CMD_PRINT,       &WW_PRINT_LEFTARROW},       // <
  {CMD_PRINT,       &WW_PRINT_EQUAL},           // =
  {CMD_PRINT,       &WW_PRINT_RIGHTARROW},      // >
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
  {CMD_PRINT,       &WW_PRINT_TClrAll},         // <backslash>
  {CMD_PRINT,       &WW_PRINT_RMar},            // ]
  {CMD_PRINT,       &WW_PRINT_UPARROW},         // ^
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
  {CMD_PRINT,       &IBM_PRINT_FLAG_0},         // i
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
  {CMD_PRINT,       &WW_PRINT_DOWNARROW},       // v
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
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_SEND | ACTION_PRINT,                     '>',  0, &WW_PRINT_RIGHTARROW},     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                     'v',  0, &WW_PRINT_DOWNARROW},      // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // Z
  {ACTION_NONE,                                    0,    0, NULL},                     // Q
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <release start>
  {ACTION_NONE,                                    0,    0, NULL},                     // A
  {ACTION_NONE,                                    0,    0, NULL},                     // <space>
  {ACTION_SEND | ACTION_PRINT,                     ';',  0, &WW_PRINT_LOADPAPER},      // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_NONE,                                    0,    0, NULL},                     // X
  {ACTION_NONE,                                    0,    0, NULL},                     // W
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // S
  {ACTION_NONE,                                    0,    0, NULL},                     // C
  {ACTION_NONE,                                    0,    0, NULL},                     // E
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // D
  {ACTION_NONE,                                    0,    0, NULL},                     // B
  {ACTION_NONE,                                    0,    0, NULL},                     // V
  {ACTION_NONE,                                    0,    0, NULL},                     // T
  {ACTION_NONE,                                    0,    0, NULL},                     // R
  {ACTION_NONE,                                    0,    0, NULL},                     // @
  {ACTION_NONE,                                    0,    0, NULL},                     // (
  {ACTION_NONE,                                    0,    0, NULL},                     // F
  {ACTION_NONE,                                    0,    0, NULL},                     // G
  {ACTION_NONE,                                    0,    0, NULL},                     // N
  {ACTION_NONE,                                    0,    0, NULL},                     // M, 7
  {ACTION_NONE,                                    0,    0, NULL},                     // Y
  {ACTION_NONE,                                    0,    0, NULL},                     // U, 1
  {ACTION_NONE,                                    0,    0, NULL},                     // <flag>
  {ACTION_NONE,                                    0,    0, NULL},                     // )
  {ACTION_NONE,                                    0,    0, NULL},                     // J, 4
  {ACTION_NONE,                                    0,    0, NULL},                     // H
  {ACTION_NONE,                                    0,    0, NULL},                     // ,, 8
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // I, 2
  {ACTION_NONE,                                    0,    0, NULL},                     // =
  {ACTION_NONE,                                    0,    0, NULL},                     // <group mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // K, 5
  {ACTION_NONE,                                    0,    0, NULL},                     // ., 9
  {ACTION_NONE,                                    0,    0, NULL},                     // O, 3
  {ACTION_NONE,                                    0,    0, NULL},                     // 0
  {ACTION_NONE,                                    0,    0, NULL},                     // L, 6
  {ACTION_NONE,                                    0,    0, NULL},                     // /
  {ACTION_NONE,                                    0,    0, NULL},                     // -
  {ACTION_NONE,                                    0,    0, NULL},                     // P
  {ACTION_NONE,                                    0,    0, NULL},                     // *, .
  {ACTION_NONE,                                    0,    0, NULL},                     // <record mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // +
  {ACTION_NONE,                                    0,    0, NULL},                     // $
  {ACTION_SEND | ACTION_PRINT,                     '<',  0, &WW_PRINT_LEFTARROW},      // <left arrow>
  {ACTION_SEND | ACTION_PRINT,                     '^',  0, &WW_PRINT_UPARROW},        // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     '_',  0, &WW_PRINT_Backspace},      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     ';',  0, &WW_PRINT_CRtn},           // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right margin>
  {ACTION_SEND | ACTION_PRINT,                     ':',  0, &WW_PRINT_Tab},            // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                     // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_SEND | ACTION_PRINT,                     '>',  0, &WW_PRINT_RIGHTARROW},     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                     'v',  0, &WW_PRINT_DOWNARROW},      // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // Z
  {ACTION_NONE,                                    0,    0, NULL},                     // Q
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <release start>
  {ACTION_NONE,                                    0,    0, NULL},                     // A
  {ACTION_NONE,                                    0,    0, NULL},                     // <space>
  {ACTION_SEND | ACTION_PRINT,                     ';',  0, &WW_PRINT_LOADPAPER},      // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     '\\', 0, &WW_PRINT_TClrAll},        // <tab clear all>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_NONE,                                    0,    0, NULL},                     // X
  {ACTION_NONE,                                    0,    0, NULL},                     // W
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // S
  {ACTION_NONE,                                    0,    0, NULL},                     // C
  {ACTION_NONE,                                    0,    0, NULL},                     // E
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // D
  {ACTION_NONE,                                    0,    0, NULL},                     // B
  {ACTION_NONE,                                    0,    0, NULL},                     // V
  {ACTION_NONE,                                    0,    0, NULL},                     // T
  {ACTION_NONE,                                    0,    0, NULL},                     // R
  {ACTION_NONE,                                    0,    0, NULL},                     // @
  {ACTION_NONE,                                    0,    0, NULL},                     // (
  {ACTION_NONE,                                    0,    0, NULL},                     // F
  {ACTION_NONE,                                    0,    0, NULL},                     // G
  {ACTION_NONE,                                    0,    0, NULL},                     // N
  {ACTION_NONE,                                    0,    0, NULL},                     // M, 7
  {ACTION_NONE,                                    0,    0, NULL},                     // Y
  {ACTION_NONE,                                    0,    0, NULL},                     // U, 1
  {ACTION_NONE,                                    0,    0, NULL},                     // <flag>
  {ACTION_NONE,                                    0,    0, NULL},                     // )
  {ACTION_NONE,                                    0,    0, NULL},                     // J, 4
  {ACTION_NONE,                                    0,    0, NULL},                     // H
  {ACTION_NONE,                                    0,    0, NULL},                     // ,, 8
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // I, 2
  {ACTION_NONE,                                    0,    0, NULL},                     // =
  {ACTION_NONE,                                    0,    0, NULL},                     // <group mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // K, 5
  {ACTION_NONE,                                    0,    0, NULL},                     // ., 9
  {ACTION_NONE,                                    0,    0, NULL},                     // O, 3
  {ACTION_NONE,                                    0,    0, NULL},                     // 0
  {ACTION_NONE,                                    0,    0, NULL},                     // L, 6
  {ACTION_NONE,                                    0,    0, NULL},                     // /
  {ACTION_NONE,                                    0,    0, NULL},                     // -
  {ACTION_NONE,                                    0,    0, NULL},                     // P
  {ACTION_NONE,                                    0,    0, NULL},                     // *, .
  {ACTION_NONE,                                    0,    0, NULL},                     // <record mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // +
  {ACTION_NONE,                                    0,    0, NULL},                     // $
  {ACTION_SEND | ACTION_PRINT,                     '<',  0, &WW_PRINT_LEFTARROW},      // <left arrow>
  {ACTION_SEND | ACTION_PRINT,                     '^',  0, &WW_PRINT_UPARROW},        // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                     // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000

  // Code.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_IBM_SETUP,                               0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_UpMicro},        // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // Z
  {ACTION_NONE,                                    0,    0, NULL},                     // Q
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <release start>
  {ACTION_NONE,                                    0,    0, NULL},                     // A
  {ACTION_NONE,                                    0,    0, NULL},                     // <space>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_SETTOPOFFORM},   // <set top of form>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     '[',  0, &WW_PRINT_LMar},           // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     '`',  0, &WW_PRINT_TClr},           // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_NONE,                                    0,    0, NULL},                     // X
  {ACTION_NONE,                                    0,    0, NULL},                     // W
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // S
  {ACTION_NONE,                                    0,    0, NULL},                     // C
  {ACTION_NONE,                                    0,    0, NULL},                     // E
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // D
  {ACTION_NONE,                                    0,    0, NULL},                     // B
  {ACTION_NONE,                                    0,    0, NULL},                     // V
  {ACTION_NONE,                                    0,    0, NULL},                     // T
  {ACTION_NONE,                                    0,    0, NULL},                     // R
  {ACTION_NONE,                                    0,    0, NULL},                     // @
  {ACTION_NONE,                                    0,    0, NULL},                     // (
  {ACTION_NONE,                                    0,    0, NULL},                     // F
  {ACTION_NONE,                                    0,    0, NULL},                     // G
  {ACTION_NONE,                                    0,    0, NULL},                     // N
  {ACTION_NONE,                                    0,    0, NULL},                     // M, 7
  {ACTION_NONE,                                    0,    0, NULL},                     // Y
  {ACTION_NONE,                                    0,    0, NULL},                     // U, 1
  {ACTION_NONE,                                    0,    0, NULL},                     // <flag>
  {ACTION_NONE,                                    0,    0, NULL},                     // )
  {ACTION_NONE,                                    0,    0, NULL},                     // J, 4
  {ACTION_NONE,                                    0,    0, NULL},                     // H
  {ACTION_NONE,                                    0,    0, NULL},                     // ,, 8
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // I, 2
  {ACTION_NONE,                                    0,    0, NULL},                     // =
  {ACTION_NONE,                                    0,    0, NULL},                     // <group mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // K, 5
  {ACTION_NONE,                                    0,    0, NULL},                     // ., 9
  {ACTION_NONE,                                    0,    0, NULL},                     // O, 3
  {ACTION_NONE,                                    0,    0, NULL},                     // 0
  {ACTION_NONE,                                    0,    0, NULL},                     // L, 6
  {ACTION_NONE,                                    0,    0, NULL},                     // /
  {ACTION_NONE,                                    0,    0, NULL},                     // -
  {ACTION_NONE,                                    0,    0, NULL},                     // P
  {ACTION_NONE,                                    0,    0, NULL},                     // *, .
  {ACTION_NONE,                                    0,    0, NULL},                     // <record mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // +
  {ACTION_NONE,                                    0,    0, NULL},                     // $
  {ACTION_NONE,                                    0,    0, NULL},                     // <left arrow>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_DownMicro},      // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_SEND | ACTION_PRINT,                     ']',  0, &WW_PRINT_RMar},           // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab>
  {ACTION_SEND | ACTION_PRINT,                     '{',  0, &WW_PRINT_MarRel},         // <margin release>
  {ACTION_SEND | ACTION_PRINT,                     '\'', 0, &WW_PRINT_TSet},           // <tab set>
  {ACTION_NONE,                                    0,    0, NULL}                      // *** not available on WW1000
};

// Mode 1 (numeric input) key action table.
const struct key_action IBM_ACTIONS_MODE1[3 * NUM_WW_KEYS] = {
  // Unshifted.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_SEND | ACTION_PRINT,                     '>',  0, &WW_PRINT_RIGHTARROW},     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                     'v',  0, &WW_PRINT_DOWNARROW},      // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // Z
  {ACTION_NONE,                                    0,    0, NULL},                     // Q
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                     '#',  0, &IBM_PRINT_RELEASESTART},  // <release start>
  {ACTION_NONE,                                    0,    0, NULL},                     // A
  {ACTION_NONE,                                    0,    0, NULL},                     // <space>
  {ACTION_SEND | ACTION_PRINT,                     ';',  0, &WW_PRINT_LOADPAPER},      // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_NONE,                                    0,    0, NULL},                     // X
  {ACTION_NONE,                                    0,    0, NULL},                     // W
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // S
  {ACTION_NONE,                                    0,    0, NULL},                     // C
  {ACTION_NONE,                                    0,    0, NULL},                     // E
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // D
  {ACTION_NONE,                                    0,    0, NULL},                     // B
  {ACTION_NONE,                                    0,    0, NULL},                     // V
  {ACTION_NONE,                                    0,    0, NULL},                     // T
  {ACTION_NONE,                                    0,    0, NULL},                     // R
  {ACTION_SEND | ACTION_PRINT,                     '@',  0, &WW_PRINT_AT},             // @
  {ACTION_NONE,                                    0,    0, NULL},                     // (
  {ACTION_NONE,                                    0,    0, NULL},                     // F
  {ACTION_NONE,                                    0,    0, NULL},                     // G
  {ACTION_NONE,                                    0,    0, NULL},                     // N
  {ACTION_SEND | ACTION_PRINT,                     '7',  0, &WW_PRINT_7},              // M, 7
  {ACTION_NONE,                                    0,    0, NULL},                     // Y
  {ACTION_SEND | ACTION_PRINT,                     '1',  0, &WW_PRINT_1},              // U, 1
  {ACTION_PRINT | ACTION_IBM_MODE_1F,              0,    0, &IBM_PRINT_FLAG},          // <flag>
  {ACTION_NONE,                                    0,    0, NULL},                     // )
  {ACTION_SEND | ACTION_PRINT,                     '4',  0, &WW_PRINT_4},              // J, 4
  {ACTION_NONE,                                    0,    0, NULL},                     // H
  {ACTION_SEND | ACTION_PRINT,                     '8',  0, &WW_PRINT_8},              // ,, 8
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                     '2',  0, &WW_PRINT_2},              // I, 2
  {ACTION_NONE,                                    0,    0, NULL},                     // =
  {ACTION_SEND | ACTION_PRINT,                     '}',  0, &IBM_PRINT_GMARK},         // <group mark>
  {ACTION_SEND | ACTION_PRINT,                     '5',  0, &WW_PRINT_5},              // K, 5
  {ACTION_SEND | ACTION_PRINT,                     '9',  0, &WW_PRINT_9},              // ., 9
  {ACTION_SEND | ACTION_PRINT,                     '3',  0, &WW_PRINT_3},              // O, 3
  {ACTION_SEND | ACTION_PRINT,                     '0',  0, &IBM_PRINT_0},             // 0
  {ACTION_SEND | ACTION_PRINT,                     '6',  0, &WW_PRINT_6},              // L, 6
  {ACTION_NONE,                                    0,    0, NULL},                     // /
  {ACTION_NONE,                                    0,    0, NULL},                     // -
  {ACTION_NONE,                                    0,    0, NULL},                     // P
  {ACTION_NONE,                                    0,    0, NULL},                     // *, .
  {ACTION_SEND | ACTION_PRINT,                     '|',  0, &IBM_PRINT_RMARK},         // <record mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // +
  {ACTION_NONE,                                    0,    0, NULL},                     // $
  {ACTION_SEND | ACTION_PRINT,                     '<',  0, &WW_PRINT_LEFTARROW},      // <left arrow>
  {ACTION_SEND | ACTION_PRINT,                     '^',  0, &WW_PRINT_UPARROW},        // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     '_',  0, &WW_PRINT_Backspace},      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     ';',  0, &WW_PRINT_CRtn},           // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right margin>
  {ACTION_SEND | ACTION_PRINT,                     ':',  0, &WW_PRINT_Tab},            // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                     // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_SEND | ACTION_PRINT,                     '>',  0, &WW_PRINT_RIGHTARROW},     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                     'v',  0, &WW_PRINT_DOWNARROW},      // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // Z
  {ACTION_NONE,                                    0,    0, NULL},                     // Q
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <release start>
  {ACTION_NONE,                                    0,    0, NULL},                     // A
  {ACTION_NONE,                                    0,    0, NULL},                     // <space>
  {ACTION_SEND | ACTION_PRINT,                     ';',  0, &WW_PRINT_LOADPAPER},      // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     '\\', 0, &WW_PRINT_TClrAll},        // <tab clear all>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_NONE,                                    0,    0, NULL},                     // X
  {ACTION_NONE,                                    0,    0, NULL},                     // W
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // S
  {ACTION_NONE,                                    0,    0, NULL},                     // C
  {ACTION_NONE,                                    0,    0, NULL},                     // E
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // D
  {ACTION_NONE,                                    0,    0, NULL},                     // B
  {ACTION_NONE,                                    0,    0, NULL},                     // V
  {ACTION_NONE,                                    0,    0, NULL},                     // T
  {ACTION_NONE,                                    0,    0, NULL},                     // R
  {ACTION_NONE,                                    0,    0, NULL},                     // @
  {ACTION_NONE,                                    0,    0, NULL},                     // (
  {ACTION_NONE,                                    0,    0, NULL},                     // F
  {ACTION_NONE,                                    0,    0, NULL},                     // G
  {ACTION_NONE,                                    0,    0, NULL},                     // N
  {ACTION_NONE,                                    0,    0, NULL},                     // M, 7
  {ACTION_NONE,                                    0,    0, NULL},                     // Y
  {ACTION_NONE,                                    0,    0, NULL},                     // U, 1
  {ACTION_NONE,                                    0,    0, NULL},                     // <flag>
  {ACTION_NONE,                                    0,    0, NULL},                     // )
  {ACTION_NONE,                                    0,    0, NULL},                     // J, 4
  {ACTION_NONE,                                    0,    0, NULL},                     // H
  {ACTION_NONE,                                    0,    0, NULL},                     // ,, 8
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // I, 2
  {ACTION_NONE,                                    0,    0, NULL},                     // =
  {ACTION_NONE,                                    0,    0, NULL},                     // <group mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // K, 5
  {ACTION_NONE,                                    0,    0, NULL},                     // ., 9
  {ACTION_NONE,                                    0,    0, NULL},                     // O, 3
  {ACTION_NONE,                                    0,    0, NULL},                     // 0
  {ACTION_NONE,                                    0,    0, NULL},                     // L, 6
  {ACTION_NONE,                                    0,    0, NULL},                     // /
  {ACTION_NONE,                                    0,    0, NULL},                     // -
  {ACTION_NONE,                                    0,    0, NULL},                     // P
  {ACTION_NONE,                                    0,    0, NULL},                     // *, .
  {ACTION_NONE,                                    0,    0, NULL},                     // <record mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // +
  {ACTION_NONE,                                    0,    0, NULL},                     // $
  {ACTION_SEND | ACTION_PRINT,                     '<',  0, &WW_PRINT_LEFTARROW},      // <left arrow>
  {ACTION_SEND | ACTION_PRINT,                     '^',  0, &WW_PRINT_UPARROW},        // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                     // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000

  // Code.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_UpMicro},        // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // Z
  {ACTION_NONE,                                    0,    0, NULL},                     // Q
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <release start>
  {ACTION_NONE,                                    0,    0, NULL},                     // A
  {ACTION_NONE,                                    0,    0, NULL},                     // <space>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_SETTOPOFFORM},   // <set top of form>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     '[',  0, &WW_PRINT_LMar},           // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     '`',  0, &WW_PRINT_TClr},           // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_NONE,                                    0,    0, NULL},                     // X
  {ACTION_NONE,                                    0,    0, NULL},                     // W
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // S
  {ACTION_NONE,                                    0,    0, NULL},                     // C
  {ACTION_NONE,                                    0,    0, NULL},                     // E
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // D
  {ACTION_NONE,                                    0,    0, NULL},                     // B
  {ACTION_NONE,                                    0,    0, NULL},                     // V
  {ACTION_NONE,                                    0,    0, NULL},                     // T
  {ACTION_NONE,                                    0,    0, NULL},                     // R
  {ACTION_NONE,                                    0,    0, NULL},                     // @
  {ACTION_NONE,                                    0,    0, NULL},                     // (
  {ACTION_NONE,                                    0,    0, NULL},                     // F
  {ACTION_NONE,                                    0,    0, NULL},                     // G
  {ACTION_NONE,                                    0,    0, NULL},                     // N
  {ACTION_NONE,                                    0,    0, NULL},                     // M, 7
  {ACTION_NONE,                                    0,    0, NULL},                     // Y
  {ACTION_NONE,                                    0,    0, NULL},                     // U, 1
  {ACTION_NONE,                                    0,    0, NULL},                     // <flag>
  {ACTION_NONE,                                    0,    0, NULL},                     // )
  {ACTION_NONE,                                    0,    0, NULL},                     // J, 4
  {ACTION_NONE,                                    0,    0, NULL},                     // H
  {ACTION_NONE,                                    0,    0, NULL},                     // ,, 8
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // I, 2
  {ACTION_NONE,                                    0,    0, NULL},                     // =
  {ACTION_NONE,                                    0,    0, NULL},                     // <group mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // K, 5
  {ACTION_NONE,                                    0,    0, NULL},                     // ., 9
  {ACTION_NONE,                                    0,    0, NULL},                     // O, 3
  {ACTION_NONE,                                    0,    0, NULL},                     // 0
  {ACTION_NONE,                                    0,    0, NULL},                     // L, 6
  {ACTION_NONE,                                    0,    0, NULL},                     // /
  {ACTION_NONE,                                    0,    0, NULL},                     // -
  {ACTION_NONE,                                    0,    0, NULL},                     // P
  {ACTION_NONE,                                    0,    0, NULL},                     // *, .
  {ACTION_NONE,                                    0,    0, NULL},                     // <record mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // +
  {ACTION_NONE,                                    0,    0, NULL},                     // $
  {ACTION_NONE,                                    0,    0, NULL},                     // <left arrow>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_DownMicro},      // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_SEND | ACTION_PRINT,                     ']',  0, &WW_PRINT_RMar},           // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab>
  {ACTION_SEND | ACTION_PRINT,                     '{',  0, &WW_PRINT_MarRel},         // <margin release>
  {ACTION_SEND | ACTION_PRINT,                     '\'', 0, &WW_PRINT_TSet},           // <tab set>
  {ACTION_NONE,                                    0,    0, NULL}                      // *** not available on WW1000
};

// Mode 1 (flagged numeric input) key action table.
const struct key_action IBM_ACTIONS_MODE1_FLAG[3 * NUM_WW_KEYS] = {
  // Unshifted.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // Z
  {ACTION_NONE,                                    0,    0, NULL},                     // Q
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1, '#',  0, &IBM_PRINT_RELEASESTART},  // <release start>
  {ACTION_NONE,                                    0,    0, NULL},                     // A
  {ACTION_NONE,                                    0,    0, NULL},                     // <space>
  {ACTION_NONE,                                    0,    0, NULL},                     // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_NONE,                                    0,    0, NULL},                     // X
  {ACTION_NONE,                                    0,    0, NULL},                     // W
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // S
  {ACTION_NONE,                                    0,    0, NULL},                     // C
  {ACTION_NONE,                                    0,    0, NULL},                     // E
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // D
  {ACTION_NONE,                                    0,    0, NULL},                     // B
  {ACTION_NONE,                                    0,    0, NULL},                     // V
  {ACTION_NONE,                                    0,    0, NULL},                     // T
  {ACTION_NONE,                                    0,    0, NULL},                     // R
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1, '~',  0, &WW_PRINT_AT},             // @
  {ACTION_NONE,                                    0,    0, NULL},                     // (
  {ACTION_NONE,                                    0,    0, NULL},                     // F
  {ACTION_NONE,                                    0,    0, NULL},                     // G
  {ACTION_NONE,                                    0,    0, NULL},                     // N
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1, 'p',  0, &WW_PRINT_7},              // M, 7
  {ACTION_NONE,                                    0,    0, NULL},                     // Y
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1, 'j',  0, &WW_PRINT_1},              // U, 1
  {ACTION_NONE,                                    0,    0, NULL},                     // <flag>
  {ACTION_NONE,                                    0,    0, NULL},                     // )
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1, 'm',  0, &WW_PRINT_4},              // J, 4
  {ACTION_NONE,                                    0,    0, NULL},                     // H
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1, 'q',  0, &WW_PRINT_8},              // ,, 8
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1, 'k',  0, &WW_PRINT_2},              // I, 2
  {ACTION_NONE,                                    0,    0, NULL},                     // =
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1, '"',  0, &IBM_PRINT_GMARK},         // <group mark>
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1, 'n',  0, &WW_PRINT_5},              // K, 5
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1, 'r',  0, &WW_PRINT_9},              // ., 9
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1, 'l',  0, &WW_PRINT_3},              // O, 3
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1, 'i',  0, &IBM_PRINT_0},             // 0
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1, 'o',  0, &WW_PRINT_6},              // L, 6
  {ACTION_NONE,                                    0,    0, NULL},                     // /
  {ACTION_NONE,                                    0,    0, NULL},                     // -
  {ACTION_NONE,                                    0,    0, NULL},                     // P
  {ACTION_NONE,                                    0,    0, NULL},                     // *, .
  {ACTION_SEND | ACTION_PRINT | ACTION_IBM_MODE_1, '!',  0, &IBM_PRINT_RMARK},         // <record mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // +
  {ACTION_NONE,                                    0,    0, NULL},                     // $
  {ACTION_NONE,                                    0,    0, NULL},                     // <left arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                     // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // Z
  {ACTION_NONE,                                    0,    0, NULL},                     // Q
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <release start>
  {ACTION_NONE,                                    0,    0, NULL},                     // A
  {ACTION_NONE,                                    0,    0, NULL},                     // <space>
  {ACTION_NONE,                                    0,    0, NULL},                     // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_NONE,                                    0,    0, NULL},                     // X
  {ACTION_NONE,                                    0,    0, NULL},                     // W
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // S
  {ACTION_NONE,                                    0,    0, NULL},                     // C
  {ACTION_NONE,                                    0,    0, NULL},                     // E
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // D
  {ACTION_NONE,                                    0,    0, NULL},                     // B
  {ACTION_NONE,                                    0,    0, NULL},                     // V
  {ACTION_NONE,                                    0,    0, NULL},                     // T
  {ACTION_NONE,                                    0,    0, NULL},                     // R
  {ACTION_NONE,                                    0,    0, NULL},                     // @
  {ACTION_NONE,                                    0,    0, NULL},                     // (
  {ACTION_NONE,                                    0,    0, NULL},                     // F
  {ACTION_NONE,                                    0,    0, NULL},                     // G
  {ACTION_NONE,                                    0,    0, NULL},                     // N
  {ACTION_NONE,                                    0,    0, NULL},                     // M, 7
  {ACTION_NONE,                                    0,    0, NULL},                     // Y
  {ACTION_NONE,                                    0,    0, NULL},                     // U, 1
  {ACTION_NONE,                                    0,    0, NULL},                     // <flag>
  {ACTION_NONE,                                    0,    0, NULL},                     // )
  {ACTION_NONE,                                    0,    0, NULL},                     // J, 4
  {ACTION_NONE,                                    0,    0, NULL},                     // H
  {ACTION_NONE,                                    0,    0, NULL},                     // ,, 8
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // I, 2
  {ACTION_NONE,                                    0,    0, NULL},                     // =
  {ACTION_NONE,                                    0,    0, NULL},                     // <group mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // K, 5
  {ACTION_NONE,                                    0,    0, NULL},                     // ., 9
  {ACTION_NONE,                                    0,    0, NULL},                     // O, 3
  {ACTION_NONE,                                    0,    0, NULL},                     // 0
  {ACTION_NONE,                                    0,    0, NULL},                     // L, 6
  {ACTION_NONE,                                    0,    0, NULL},                     // /
  {ACTION_NONE,                                    0,    0, NULL},                     // -
  {ACTION_NONE,                                    0,    0, NULL},                     // P
  {ACTION_NONE,                                    0,    0, NULL},                     // *, .
  {ACTION_NONE,                                    0,    0, NULL},                     // <record mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // +
  {ACTION_NONE,                                    0,    0, NULL},                     // $
  {ACTION_NONE,                                    0,    0, NULL},                     // <left arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                     // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000

  // Code.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // Z
  {ACTION_NONE,                                    0,    0, NULL},                     // Q
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <release start>
  {ACTION_NONE,                                    0,    0, NULL},                     // A
  {ACTION_NONE,                                    0,    0, NULL},                     // <space>
  {ACTION_NONE,                                    0,    0, NULL},                     // <set top of form>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_NONE,                                    0,    0, NULL},                     // X
  {ACTION_NONE,                                    0,    0, NULL},                     // W
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // S
  {ACTION_NONE,                                    0,    0, NULL},                     // C
  {ACTION_NONE,                                    0,    0, NULL},                     // E
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // D
  {ACTION_NONE,                                    0,    0, NULL},                     // B
  {ACTION_NONE,                                    0,    0, NULL},                     // V
  {ACTION_NONE,                                    0,    0, NULL},                     // T
  {ACTION_NONE,                                    0,    0, NULL},                     // R
  {ACTION_NONE,                                    0,    0, NULL},                     // @
  {ACTION_NONE,                                    0,    0, NULL},                     // (
  {ACTION_NONE,                                    0,    0, NULL},                     // F
  {ACTION_NONE,                                    0,    0, NULL},                     // G
  {ACTION_NONE,                                    0,    0, NULL},                     // N
  {ACTION_NONE,                                    0,    0, NULL},                     // M, 7
  {ACTION_NONE,                                    0,    0, NULL},                     // Y
  {ACTION_NONE,                                    0,    0, NULL},                     // U, 1
  {ACTION_NONE,                                    0,    0, NULL},                     // <flag>
  {ACTION_NONE,                                    0,    0, NULL},                     // )
  {ACTION_NONE,                                    0,    0, NULL},                     // J, 4
  {ACTION_NONE,                                    0,    0, NULL},                     // H
  {ACTION_NONE,                                    0,    0, NULL},                     // ,, 8
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // I, 2
  {ACTION_NONE,                                    0,    0, NULL},                     // =
  {ACTION_NONE,                                    0,    0, NULL},                     // <group mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // K, 5
  {ACTION_NONE,                                    0,    0, NULL},                     // ., 9
  {ACTION_NONE,                                    0,    0, NULL},                     // O, 3
  {ACTION_NONE,                                    0,    0, NULL},                     // 0
  {ACTION_NONE,                                    0,    0, NULL},                     // L, 6
  {ACTION_NONE,                                    0,    0, NULL},                     // /
  {ACTION_NONE,                                    0,    0, NULL},                     // -
  {ACTION_NONE,                                    0,    0, NULL},                     // P
  {ACTION_NONE,                                    0,    0, NULL},                     // *, .
  {ACTION_NONE,                                    0,    0, NULL},                     // <record mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // +
  {ACTION_NONE,                                    0,    0, NULL},                     // $
  {ACTION_NONE,                                    0,    0, NULL},                     // <left arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                     // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab set>
  {ACTION_NONE,                                    0,    0, NULL}                      // *** not available on WW1000
};

// Mode 2 (alphameric input) key action table.
const struct key_action IBM_ACTIONS_MODE2[3 * NUM_WW_KEYS] = {
  // Unshifted.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_SEND | ACTION_PRINT,                     '>',  0, &WW_PRINT_RIGHTARROW},     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                     'v',  0, &WW_PRINT_DOWNARROW},      // <down arrow>
  {ACTION_SEND | ACTION_PRINT,                     'Z',  0, &WW_PRINT_Z},              // Z
  {ACTION_SEND | ACTION_PRINT,                     'Q',  0, &WW_PRINT_Q},              // Q
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                     '#',  0, &IBM_PRINT_RELEASESTART},  // <release start>
  {ACTION_SEND | ACTION_PRINT,                     'A',  0, &WW_PRINT_A},              // A
  {ACTION_SEND | ACTION_PRINT,                     ' ',  0, &WW_PRINT_SPACE},          // <space>
  {ACTION_SEND | ACTION_PRINT,                     ';',  0, &WW_PRINT_LOADPAPER},      // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_SEND | ACTION_PRINT,                     'X',  0, &WW_PRINT_X},              // X
  {ACTION_SEND | ACTION_PRINT,                     'W',  0, &WW_PRINT_W},              // W
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                     'S',  0, &WW_PRINT_S},              // S
  {ACTION_SEND | ACTION_PRINT,                     'C',  0, &WW_PRINT_C},              // C
  {ACTION_SEND | ACTION_PRINT,                     'E',  0, &WW_PRINT_E},              // E
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                     'D',  0, &WW_PRINT_D},              // D
  {ACTION_SEND | ACTION_PRINT,                     'B',  0, &WW_PRINT_B},              // B
  {ACTION_SEND | ACTION_PRINT,                     'V',  0, &WW_PRINT_V},              // V
  {ACTION_SEND | ACTION_PRINT,                     'T',  0, &WW_PRINT_T},              // T
  {ACTION_SEND | ACTION_PRINT,                     'R',  0, &WW_PRINT_R},              // R
  {ACTION_SEND | ACTION_PRINT,                     '@',  0, &WW_PRINT_AT},             // @
  {ACTION_SEND | ACTION_PRINT,                     '(',  0, &WW_PRINT_LPAREN},         // (
  {ACTION_SEND | ACTION_PRINT,                     'F',  0, &WW_PRINT_F},              // F
  {ACTION_SEND | ACTION_PRINT,                     'G',  0, &WW_PRINT_G},              // G
  {ACTION_SEND | ACTION_PRINT,                     'N',  0, &WW_PRINT_N},              // N
  {ACTION_SEND | ACTION_PRINT,                     'M',  0, &WW_PRINT_M},              // M, 7
  {ACTION_SEND | ACTION_PRINT,                     'Y',  0, &WW_PRINT_Y},              // Y
  {ACTION_SEND | ACTION_PRINT,                     'U',  0, &WW_PRINT_U},              // U, 1
  {ACTION_NONE,                                    0,    0, NULL},                     // <flag>
  {ACTION_SEND | ACTION_PRINT,                     ')',  0, &WW_PRINT_RPAREN},         // )
  {ACTION_SEND | ACTION_PRINT,                     'J',  0, &WW_PRINT_J},              // J, 4
  {ACTION_SEND | ACTION_PRINT,                     'H',  0, &WW_PRINT_H},              // H
  {ACTION_SEND | ACTION_PRINT,                     ',',  0, &WW_PRINT_COMMA},          // ,, 8
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                     'I',  0, &WW_PRINT_I},              // I, 2
  {ACTION_SEND | ACTION_PRINT,                     '=',  0, &WW_PRINT_EQUAL},          // =
  {ACTION_SEND | ACTION_PRINT,                     '}',  0, &IBM_PRINT_GMARK},         // <group mark>
  {ACTION_SEND | ACTION_PRINT,                     'K',  0, &WW_PRINT_K},              // K, 5
  {ACTION_SEND | ACTION_PRINT,                     '.',  0, &WW_PRINT_PERIOD},         // ., 9
  {ACTION_SEND | ACTION_PRINT,                     'O',  0, &WW_PRINT_O},              // O, 3
  {ACTION_NONE,                                    0,    0, NULL},                     // 0
  {ACTION_SEND | ACTION_PRINT,                     'L',  0, &WW_PRINT_L},              // L, 6
  {ACTION_SEND | ACTION_PRINT,                     '/',  0, &WW_PRINT_SLASH},          // /
  {ACTION_SEND | ACTION_PRINT,                     '-',  0, &WW_PRINT_HYPHEN},         // -
  {ACTION_SEND | ACTION_PRINT,                     'P',  0, &WW_PRINT_P},              // P
  {ACTION_SEND | ACTION_PRINT,                     '*',  0, &WW_PRINT_ASTERISK},       // *, .
  {ACTION_SEND | ACTION_PRINT,                     '|',  0, &IBM_PRINT_RMARK},         // <record mark>
  {ACTION_SEND | ACTION_PRINT,                     '+',  0, &WW_PRINT_PLUS},           // +
  {ACTION_SEND | ACTION_PRINT,                     '$',  0, &WW_PRINT_DOLLAR},         // $
  {ACTION_SEND | ACTION_PRINT,                     '<',  0, &WW_PRINT_LEFTARROW},      // <left arrow>
  {ACTION_SEND | ACTION_PRINT,                     '^',  0, &WW_PRINT_UPARROW},        // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     '_',  0, &WW_PRINT_Backspace},      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     ';',  0, &WW_PRINT_CRtn},           // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right margin>
  {ACTION_SEND | ACTION_PRINT,                     ':',  0, &WW_PRINT_Tab},            // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                     // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_SEND | ACTION_PRINT,                     '>',  0, &WW_PRINT_RIGHTARROW},     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                     'v',  0, &WW_PRINT_DOWNARROW},      // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // Z
  {ACTION_NONE,                                    0,    0, NULL},                     // Q
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <release start>
  {ACTION_NONE,                                    0,    0, NULL},                     // A
  {ACTION_NONE,                                    0,    0, NULL},                     // <space>
  {ACTION_SEND | ACTION_PRINT,                     ';',  0, &WW_PRINT_LOADPAPER},      // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     '\\', 0, &WW_PRINT_TClrAll},        // <tab clear all>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_NONE,                                    0,    0, NULL},                     // X
  {ACTION_NONE,                                    0,    0, NULL},                     // W
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // S
  {ACTION_NONE,                                    0,    0, NULL},                     // C
  {ACTION_NONE,                                    0,    0, NULL},                     // E
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // D
  {ACTION_NONE,                                    0,    0, NULL},                     // B
  {ACTION_NONE,                                    0,    0, NULL},                     // V
  {ACTION_NONE,                                    0,    0, NULL},                     // T
  {ACTION_NONE,                                    0,    0, NULL},                     // R
  {ACTION_NONE,                                    0,    0, NULL},                     // @
  {ACTION_NONE,                                    0,    0, NULL},                     // (
  {ACTION_NONE,                                    0,    0, NULL},                     // F
  {ACTION_NONE,                                    0,    0, NULL},                     // G
  {ACTION_NONE,                                    0,    0, NULL},                     // N
  {ACTION_SEND | ACTION_PRINT,                     '7',  0, &WW_PRINT_7},              // M, 7
  {ACTION_NONE,                                    0,    0, NULL},                     // Y
  {ACTION_SEND | ACTION_PRINT,                     '1',  0, &WW_PRINT_1},              // U, 1
  {ACTION_NONE,                                    0,    0, NULL},                     // <flag>
  {ACTION_NONE,                                    0,    0, NULL},                     // )
  {ACTION_SEND | ACTION_PRINT,                     '4',  0, &WW_PRINT_4},              // J, 4
  {ACTION_NONE,                                    0,    0, NULL},                     // H
  {ACTION_SEND | ACTION_PRINT,                     '8',  0, &WW_PRINT_8},              // ,, 8
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                     '2',  0, &WW_PRINT_2},              // I, 2
  {ACTION_NONE,                                    0,    0, NULL},                     // =
  {ACTION_NONE,                                    0,    0, NULL},                     // <group mark>
  {ACTION_SEND | ACTION_PRINT,                     '5',  0, &WW_PRINT_5},              // K, 5
  {ACTION_SEND | ACTION_PRINT,                     '9',  0, &WW_PRINT_9},              // ., 9
  {ACTION_SEND | ACTION_PRINT,                     '3',  0, &WW_PRINT_3},              // O, 3
  {ACTION_SEND | ACTION_PRINT,                     '0',  0, &IBM_PRINT_0},             // 0
  {ACTION_SEND | ACTION_PRINT,                     '6',  0, &WW_PRINT_6},              // L, 6
  {ACTION_NONE,                                    0,    0, NULL},                     // /
  {ACTION_NONE,                                    0,    0, NULL},                     // -
  {ACTION_NONE,                                    0,    0, NULL},                     // P
  {ACTION_SEND | ACTION_PRINT,                     '.',  0, &WW_PRINT_PERIOD},         // *, .
  {ACTION_NONE,                                    0,    0, NULL},                     // <record mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // +
  {ACTION_NONE,                                    0,    0, NULL},                     // $
  {ACTION_SEND | ACTION_PRINT,                     '<',  0, &WW_PRINT_LEFTARROW},      // <left arrow>
  {ACTION_SEND | ACTION_PRINT,                     '^',  0, &WW_PRINT_UPARROW},        // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                     // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000

  // Code.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_UpMicro},        // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // Z
  {ACTION_NONE,                                    0,    0, NULL},                     // Q
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <release start>
  {ACTION_NONE,                                    0,    0, NULL},                     // A
  {ACTION_NONE,                                    0,    0, NULL},                     // <space>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_SETTOPOFFORM},   // <set top of form>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     '[',  0, &WW_PRINT_LMar},           // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     '`',  0, &WW_PRINT_TClr},           // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_NONE,                                    0,    0, NULL},                     // X
  {ACTION_NONE,                                    0,    0, NULL},                     // W
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // S
  {ACTION_NONE,                                    0,    0, NULL},                     // C
  {ACTION_NONE,                                    0,    0, NULL},                     // E
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // D
  {ACTION_NONE,                                    0,    0, NULL},                     // B
  {ACTION_NONE,                                    0,    0, NULL},                     // V
  {ACTION_NONE,                                    0,    0, NULL},                     // T
  {ACTION_NONE,                                    0,    0, NULL},                     // R
  {ACTION_NONE,                                    0,    0, NULL},                     // @
  {ACTION_NONE,                                    0,    0, NULL},                     // (
  {ACTION_NONE,                                    0,    0, NULL},                     // F
  {ACTION_NONE,                                    0,    0, NULL},                     // G
  {ACTION_NONE,                                    0,    0, NULL},                     // N
  {ACTION_NONE,                                    0,    0, NULL},                     // M, 7
  {ACTION_NONE,                                    0,    0, NULL},                     // Y
  {ACTION_NONE,                                    0,    0, NULL},                     // U, 1
  {ACTION_NONE,                                    0,    0, NULL},                     // <flag>
  {ACTION_NONE,                                    0,    0, NULL},                     // )
  {ACTION_NONE,                                    0,    0, NULL},                     // J, 4
  {ACTION_NONE,                                    0,    0, NULL},                     // H
  {ACTION_NONE,                                    0,    0, NULL},                     // ,, 8
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // I, 2
  {ACTION_NONE,                                    0,    0, NULL},                     // =
  {ACTION_NONE,                                    0,    0, NULL},                     // <group mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // K, 5
  {ACTION_NONE,                                    0,    0, NULL},                     // ., 9
  {ACTION_NONE,                                    0,    0, NULL},                     // O, 3
  {ACTION_NONE,                                    0,    0, NULL},                     // 0
  {ACTION_NONE,                                    0,    0, NULL},                     // L, 6
  {ACTION_NONE,                                    0,    0, NULL},                     // /
  {ACTION_NONE,                                    0,    0, NULL},                     // -
  {ACTION_NONE,                                    0,    0, NULL},                     // P
  {ACTION_NONE,                                    0,    0, NULL},                     // *, .
  {ACTION_NONE,                                    0,    0, NULL},                     // <record mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // +
  {ACTION_NONE,                                    0,    0, NULL},                     // $
  {ACTION_NONE,                                    0,    0, NULL},                     // <left arrow>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_DownMicro},      // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_SEND | ACTION_PRINT,                     ']',  0, &WW_PRINT_RMar},           // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab>
  {ACTION_SEND | ACTION_PRINT,                     '{',  0, &WW_PRINT_MarRel},         // <margin release>
  {ACTION_SEND | ACTION_PRINT,                     '\'', 0, &WW_PRINT_TSet},           // <tab set>
  {ACTION_NONE,                                    0,    0, NULL}                      // *** not available on WW1000
};

// Mode 3 (output) key action table.
const struct key_action IBM_ACTIONS_MODE3[3 * NUM_WW_KEYS] = {
  // Unshifted.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // Z
  {ACTION_NONE,                                    0,    0, NULL},                     // Q
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_SEND | ACTION_PRINT,                     '#',  0, &IBM_PRINT_RELEASESTART},  // <release start>
  {ACTION_NONE,                                    0,    0, NULL},                     // A
  {ACTION_NONE,                                    0,    0, NULL},                     // <space>
  {ACTION_NONE,                                    0,    0, NULL},                     // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_NONE,                                    0,    0, NULL},                     // X
  {ACTION_NONE,                                    0,    0, NULL},                     // W
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // S
  {ACTION_NONE,                                    0,    0, NULL},                     // C
  {ACTION_NONE,                                    0,    0, NULL},                     // E
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // D
  {ACTION_NONE,                                    0,    0, NULL},                     // B
  {ACTION_NONE,                                    0,    0, NULL},                     // V
  {ACTION_NONE,                                    0,    0, NULL},                     // T
  {ACTION_NONE,                                    0,    0, NULL},                     // R
  {ACTION_NONE,                                    0,    0, NULL},                     // @
  {ACTION_NONE,                                    0,    0, NULL},                     // (
  {ACTION_NONE,                                    0,    0, NULL},                     // F
  {ACTION_NONE,                                    0,    0, NULL},                     // G
  {ACTION_NONE,                                    0,    0, NULL},                     // N
  {ACTION_NONE,                                    0,    0, NULL},                     // M, 7
  {ACTION_NONE,                                    0,    0, NULL},                     // Y
  {ACTION_NONE,                                    0,    0, NULL},                     // U, 1
  {ACTION_NONE,                                    0,    0, NULL},                     // <flag>
  {ACTION_NONE,                                    0,    0, NULL},                     // )
  {ACTION_NONE,                                    0,    0, NULL},                     // J, 4
  {ACTION_NONE,                                    0,    0, NULL},                     // H
  {ACTION_NONE,                                    0,    0, NULL},                     // ,, 8
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // I, 2
  {ACTION_NONE,                                    0,    0, NULL},                     // =
  {ACTION_NONE,                                    0,    0, NULL},                     // <group mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // K, 5
  {ACTION_NONE,                                    0,    0, NULL},                     // ., 9
  {ACTION_NONE,                                    0,    0, NULL},                     // O, 3
  {ACTION_NONE,                                    0,    0, NULL},                     // 0
  {ACTION_NONE,                                    0,    0, NULL},                     // L, 6
  {ACTION_NONE,                                    0,    0, NULL},                     // /
  {ACTION_NONE,                                    0,    0, NULL},                     // -
  {ACTION_NONE,                                    0,    0, NULL},                     // P
  {ACTION_NONE,                                    0,    0, NULL},                     // *, .
  {ACTION_NONE,                                    0,    0, NULL},                     // <record mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // +
  {ACTION_NONE,                                    0,    0, NULL},                     // $
  {ACTION_NONE,                                    0,    0, NULL},                     // <left arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     ';',  0, &WW_PRINT_CRtn},           // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right margin>
  {ACTION_SEND | ACTION_PRINT,                     ':',  0, &WW_PRINT_Tab},            // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                     // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // Z
  {ACTION_NONE,                                    0,    0, NULL},                     // Q
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <release start>
  {ACTION_NONE,                                    0,    0, NULL},                     // A
  {ACTION_NONE,                                    0,    0, NULL},                     // <space>
  {ACTION_NONE,                                    0,    0, NULL},                     // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_NONE,                                    0,    0, NULL},                     // X
  {ACTION_NONE,                                    0,    0, NULL},                     // W
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // S
  {ACTION_NONE,                                    0,    0, NULL},                     // C
  {ACTION_NONE,                                    0,    0, NULL},                     // E
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // D
  {ACTION_NONE,                                    0,    0, NULL},                     // B
  {ACTION_NONE,                                    0,    0, NULL},                     // V
  {ACTION_NONE,                                    0,    0, NULL},                     // T
  {ACTION_NONE,                                    0,    0, NULL},                     // R
  {ACTION_NONE,                                    0,    0, NULL},                     // @
  {ACTION_NONE,                                    0,    0, NULL},                     // (
  {ACTION_NONE,                                    0,    0, NULL},                     // F
  {ACTION_NONE,                                    0,    0, NULL},                     // G
  {ACTION_NONE,                                    0,    0, NULL},                     // N
  {ACTION_NONE,                                    0,    0, NULL},                     // M, 7
  {ACTION_NONE,                                    0,    0, NULL},                     // Y
  {ACTION_NONE,                                    0,    0, NULL},                     // U, 1
  {ACTION_NONE,                                    0,    0, NULL},                     // <flag>
  {ACTION_NONE,                                    0,    0, NULL},                     // )
  {ACTION_NONE,                                    0,    0, NULL},                     // J, 4
  {ACTION_NONE,                                    0,    0, NULL},                     // H
  {ACTION_NONE,                                    0,    0, NULL},                     // ,, 8
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // I, 2
  {ACTION_NONE,                                    0,    0, NULL},                     // =
  {ACTION_NONE,                                    0,    0, NULL},                     // <group mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // K, 5
  {ACTION_NONE,                                    0,    0, NULL},                     // ., 9
  {ACTION_NONE,                                    0,    0, NULL},                     // O, 3
  {ACTION_NONE,                                    0,    0, NULL},                     // 0
  {ACTION_NONE,                                    0,    0, NULL},                     // L, 6
  {ACTION_NONE,                                    0,    0, NULL},                     // /
  {ACTION_NONE,                                    0,    0, NULL},                     // -
  {ACTION_NONE,                                    0,    0, NULL},                     // P
  {ACTION_NONE,                                    0,    0, NULL},                     // *, .
  {ACTION_NONE,                                    0,    0, NULL},                     // <record mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // +
  {ACTION_NONE,                                    0,    0, NULL},                     // $
  {ACTION_NONE,                                    0,    0, NULL},                     // <left arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                     // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000

  // Code.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // Z
  {ACTION_NONE,                                    0,    0, NULL},                     // Q
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <release start>
  {ACTION_NONE,                                    0,    0, NULL},                     // A
  {ACTION_NONE,                                    0,    0, NULL},                     // <space>
  {ACTION_NONE,                                    0,    0, NULL},                     // <set top of form>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_NONE,                                    0,    0, NULL},                     // X
  {ACTION_NONE,                                    0,    0, NULL},                     // W
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // S
  {ACTION_NONE,                                    0,    0, NULL},                     // C
  {ACTION_NONE,                                    0,    0, NULL},                     // E
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // D
  {ACTION_NONE,                                    0,    0, NULL},                     // B
  {ACTION_NONE,                                    0,    0, NULL},                     // V
  {ACTION_NONE,                                    0,    0, NULL},                     // T
  {ACTION_NONE,                                    0,    0, NULL},                     // R
  {ACTION_NONE,                                    0,    0, NULL},                     // @
  {ACTION_NONE,                                    0,    0, NULL},                     // (
  {ACTION_NONE,                                    0,    0, NULL},                     // F
  {ACTION_NONE,                                    0,    0, NULL},                     // G
  {ACTION_NONE,                                    0,    0, NULL},                     // N
  {ACTION_NONE,                                    0,    0, NULL},                     // M, 7
  {ACTION_NONE,                                    0,    0, NULL},                     // Y
  {ACTION_NONE,                                    0,    0, NULL},                     // U, 1
  {ACTION_NONE,                                    0,    0, NULL},                     // <flag>
  {ACTION_NONE,                                    0,    0, NULL},                     // )
  {ACTION_NONE,                                    0,    0, NULL},                     // J, 4
  {ACTION_NONE,                                    0,    0, NULL},                     // H
  {ACTION_NONE,                                    0,    0, NULL},                     // ,, 8
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // I, 2
  {ACTION_NONE,                                    0,    0, NULL},                     // =
  {ACTION_NONE,                                    0,    0, NULL},                     // <group mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // K, 5
  {ACTION_NONE,                                    0,    0, NULL},                     // ., 9
  {ACTION_NONE,                                    0,    0, NULL},                     // O, 3
  {ACTION_NONE,                                    0,    0, NULL},                     // 0
  {ACTION_NONE,                                    0,    0, NULL},                     // L, 6
  {ACTION_NONE,                                    0,    0, NULL},                     // /
  {ACTION_NONE,                                    0,    0, NULL},                     // -
  {ACTION_NONE,                                    0,    0, NULL},                     // P
  {ACTION_NONE,                                    0,    0, NULL},                     // *, .
  {ACTION_NONE,                                    0,    0, NULL},                     // <record mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // +
  {ACTION_NONE,                                    0,    0, NULL},                     // $
  {ACTION_NONE,                                    0,    0, NULL},                     // <left arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                     // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab set>
  {ACTION_NONE,                                    0,    0, NULL}                      // *** not available on WW1000
};

// Setup key action table.
const struct key_action IBM_ACTIONS_SETUP[3 * NUM_WW_KEYS] = {
  // Unshifted.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <down arrow>
  {ACTION_COMMAND,                                 'Z',  0, NULL},                     // Z
  {ACTION_COMMAND,                                 'Q',  0, NULL},                     // Q
  {ACTION_COMMAND,                                 '~',  0, NULL},                     // *** unlabelled key
  {ACTION_COMMAND,                                 '~',  0, NULL},                     // <release start>
  {ACTION_COMMAND,                                 'A',  0, NULL},                     // A
  {ACTION_COMMAND,                                 ' ',  0, NULL},                     // <space>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_COMMAND,                                 'X',  0, NULL},                     // X
  {ACTION_COMMAND,                                 'W',  0, NULL},                     // W
  {ACTION_COMMAND,                                 '~',  0, NULL},                     // *** unlabelled key
  {ACTION_COMMAND,                                 'S',  0, NULL},                     // S
  {ACTION_COMMAND,                                 'C',  0, NULL},                     // C
  {ACTION_COMMAND,                                 'E',  0, NULL},                     // E
  {ACTION_COMMAND,                                 '~',  0, NULL},                     // *** unlabelled key
  {ACTION_COMMAND,                                 'D',  0, NULL},                     // D
  {ACTION_COMMAND,                                 'B',  0, NULL},                     // B
  {ACTION_COMMAND,                                 'V',  0, NULL},                     // V
  {ACTION_COMMAND,                                 'T',  0, NULL},                     // T
  {ACTION_COMMAND,                                 'R',  0, NULL},                     // R
  {ACTION_COMMAND,                                 '@',  0, NULL},                     // @
  {ACTION_COMMAND,                                 '(',  0, NULL},                     // (
  {ACTION_COMMAND,                                 'F',  0, NULL},                     // F
  {ACTION_COMMAND,                                 'G',  0, NULL},                     // G
  {ACTION_COMMAND,                                 'N',  0, NULL},                     // N
  {ACTION_COMMAND,                                 'M',  0, NULL},                     // M, 7
  {ACTION_COMMAND,                                 'Y',  0, NULL},                     // Y
  {ACTION_COMMAND,                                 'U',  0, NULL},                     // U, 1
  {ACTION_COMMAND,                                 '~',  0, NULL},                     // <flag>
  {ACTION_COMMAND,                                 ')',  0, NULL},                     // )
  {ACTION_COMMAND,                                 'J',  0, NULL},                     // J, 4
  {ACTION_COMMAND,                                 'H',  0, NULL},                     // H
  {ACTION_COMMAND,                                 ',',  0, NULL},                     // ,, 8
  {ACTION_COMMAND,                                 '~',  0, NULL},                     // *** unlabelled key
  {ACTION_COMMAND,                                 'I',  0, NULL},                     // I, 2
  {ACTION_COMMAND,                                 '=',  0, NULL},                     // =
  {ACTION_COMMAND,                                 '~',  0, NULL},                     // <group mark>
  {ACTION_COMMAND,                                 'K',  0, NULL},                     // K, 5
  {ACTION_COMMAND,                                 '.',  0, NULL},                     // ., 9
  {ACTION_COMMAND,                                 'O',  0, NULL},                     // O, 3
  {ACTION_COMMAND,                                 '0',  0, NULL},                     // 0
  {ACTION_COMMAND,                                 'L',  0, NULL},                     // L, 6
  {ACTION_COMMAND,                                 '/',  0, NULL},                     // /
  {ACTION_COMMAND,                                 '-',  0, NULL},                     // -
  {ACTION_COMMAND,                                 'P',  0, NULL},                     // P
  {ACTION_COMMAND,                                 '*',  0, NULL},                     // *, .
  {ACTION_COMMAND,                                 '~',  0, NULL},                     // <record mark>
  {ACTION_COMMAND,                                 '+',  0, NULL},                     // +
  {ACTION_COMMAND,                                 '$',  0, NULL},                     // $
  {ACTION_NONE,                                    0,    0, NULL},                     // <left arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_COMMAND,                                 0x0d, 0, NULL},                     // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                     // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <down arrow>
  {ACTION_COMMAND,                                 'Z',  0, NULL},                     // Z
  {ACTION_COMMAND,                                 'Q',  0, NULL},                     // Q
  {ACTION_COMMAND,                                 '~',  0, NULL},                     // *** unlabelled key
  {ACTION_COMMAND,                                 '~',  0, NULL},                     // <release start>
  {ACTION_COMMAND,                                 'A',  0, NULL},                     // A
  {ACTION_COMMAND,                                 ' ',  0, NULL},                     // <space>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_COMMAND,                                 'X',  0, NULL},                     // X
  {ACTION_COMMAND,                                 'W',  0, NULL},                     // W
  {ACTION_COMMAND,                                 '~',  0, NULL},                     // *** unlabelled key
  {ACTION_COMMAND,                                 'S',  0, NULL},                     // S
  {ACTION_COMMAND,                                 'C',  0, NULL},                     // C
  {ACTION_COMMAND,                                 'E',  0, NULL},                     // E
  {ACTION_COMMAND,                                 '~',  0, NULL},                     // *** unlabelled key
  {ACTION_COMMAND,                                 'D',  0, NULL},                     // D
  {ACTION_COMMAND,                                 'B',  0, NULL},                     // B
  {ACTION_COMMAND,                                 'V',  0, NULL},                     // V
  {ACTION_COMMAND,                                 'T',  0, NULL},                     // T
  {ACTION_COMMAND,                                 'R',  0, NULL},                     // R
  {ACTION_COMMAND,                                 '@',  0, NULL},                     // @
  {ACTION_COMMAND,                                 '(',  0, NULL},                     // (
  {ACTION_COMMAND,                                 'F',  0, NULL},                     // F
  {ACTION_COMMAND,                                 'G',  0, NULL},                     // G
  {ACTION_COMMAND,                                 'N',  0, NULL},                     // N
  {ACTION_COMMAND,                                 '7',  0, NULL},                     // M, 7
  {ACTION_COMMAND,                                 'Y',  0, NULL},                     // Y
  {ACTION_COMMAND,                                 '1',  0, NULL},                     // U, 1
  {ACTION_COMMAND,                                 '~',  0, NULL},                     // <flag>
  {ACTION_COMMAND,                                 ')',  0, NULL},                     // )
  {ACTION_COMMAND,                                 '4',  0, NULL},                     // J, 4
  {ACTION_COMMAND,                                 'H',  0, NULL},                     // H
  {ACTION_COMMAND,                                 '8',  0, NULL},                     // ,, 8
  {ACTION_COMMAND,                                 '~',  0, NULL},                     // *** unlabelled key
  {ACTION_COMMAND,                                 '2',  0, NULL},                     // I, 2
  {ACTION_COMMAND,                                 '=',  0, NULL},                     // =
  {ACTION_COMMAND,                                 '~',  0, NULL},                     // <group mark>
  {ACTION_COMMAND,                                 '5',  0, NULL},                     // K, 5
  {ACTION_COMMAND,                                 '9',  0, NULL},                     // ., 9
  {ACTION_COMMAND,                                 '3',  0, NULL},                     // O, 3
  {ACTION_COMMAND,                                 '0',  0, NULL},                     // 0
  {ACTION_COMMAND,                                 '6',  0, NULL},                     // L, 6
  {ACTION_COMMAND,                                 '/',  0, NULL},                     // /
  {ACTION_COMMAND,                                 '-',  0, NULL},                     // -
  {ACTION_COMMAND,                                 'P',  0, NULL},                     // P
  {ACTION_COMMAND,                                 '.',  0, NULL},                     // *, .
  {ACTION_COMMAND,                                 '~',  0, NULL},                     // <record mark>
  {ACTION_COMMAND,                                 '+',  0, NULL},                     // +
  {ACTION_COMMAND,                                 '$',  0, NULL},                     // $
  {ACTION_NONE,                                    0,    0, NULL},                     // <left arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_COMMAND,                                 0x0d, 0, NULL},                     // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                     // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000

  // Code.
  {ACTION_NONE,                                    0,    0, NULL},                     // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // Z
  {ACTION_NONE,                                    0,    0, NULL},                     // Q
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <release start>
  {ACTION_NONE,                                    0,    0, NULL},                     // A
  {ACTION_NONE,                                    0,    0, NULL},                     // <space>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                     // <code>
  {ACTION_NONE,                                    0,    0, NULL},                     // X
  {ACTION_NONE,                                    0,    0, NULL},                     // W
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // S
  {ACTION_NONE,                                    0,    0, NULL},                     // C
  {ACTION_NONE,                                    0,    0, NULL},                     // E
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_COMMAND,                                 0x04, 0, NULL},                     // D
  {ACTION_NONE,                                    0,    0, NULL},                     // B
  {ACTION_NONE,                                    0,    0, NULL},                     // V
  {ACTION_NONE,                                    0,    0, NULL},                     // T
  {ACTION_NONE,                                    0,    0, NULL},                     // R
  {ACTION_NONE,                                    0,    0, NULL},                     // @
  {ACTION_NONE,                                    0,    0, NULL},                     // (
  {ACTION_NONE,                                    0,    0, NULL},                     // F
  {ACTION_NONE,                                    0,    0, NULL},                     // G
  {ACTION_NONE,                                    0,    0, NULL},                     // N
  {ACTION_NONE,                                    0,    0, NULL},                     // M, 7
  {ACTION_NONE,                                    0,    0, NULL},                     // Y
  {ACTION_NONE,                                    0,    0, NULL},                     // U, 1
  {ACTION_NONE,                                    0,    0, NULL},                     // <flag>
  {ACTION_NONE,                                    0,    0, NULL},                     // )
  {ACTION_NONE,                                    0,    0, NULL},                     // J, 4
  {ACTION_NONE,                                    0,    0, NULL},                     // H
  {ACTION_NONE,                                    0,    0, NULL},                     // ,, 8
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // I, 2
  {ACTION_NONE,                                    0,    0, NULL},                     // =
  {ACTION_NONE,                                    0,    0, NULL},                     // <group mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // K, 5
  {ACTION_NONE,                                    0,    0, NULL},                     // ., 9
  {ACTION_NONE,                                    0,    0, NULL},                     // O, 3
  {ACTION_NONE,                                    0,    0, NULL},                     // 0
  {ACTION_NONE,                                    0,    0, NULL},                     // L, 6
  {ACTION_NONE,                                    0,    0, NULL},                     // /
  {ACTION_NONE,                                    0,    0, NULL},                     // -
  {ACTION_NONE,                                    0,    0, NULL},                     // P
  {ACTION_NONE,                                    0,    0, NULL},                     // *, .
  {ACTION_NONE,                                    0,    0, NULL},                     // <record mark>
  {ACTION_NONE,                                    0,    0, NULL},                     // +
  {ACTION_NONE,                                    0,    0, NULL},                     // $
  {ACTION_NONE,                                    0,    0, NULL},                     // <left arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                     // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                     // <return>
  {ACTION_NONE,                                    0,    0, NULL},                     //
  {ACTION_NONE,                                    0,    0, NULL},                     // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                     // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                     // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                     // <tab set>
  {ACTION_NONE,                                    0,    0, NULL}                      // *** not available on WW1000
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

void Setup_IBM (void) {

  // Initialize variables.
  key_actions = &IBM_ACTIONS_MODE0;
  serial_actions = &IBM_SERIAL_ACTIONS;
  serial = SERIAL_USB;
  flow_on = CHAR_IBM_RESUME;
  flow_off = CHAR_IBM_PAUSE;
  swflow = SWFLOW_XON_XOFF;
  hwflow = HWFLOW_NONE;
  artn_IBM = FALSE;
  bold_IBM = FALSE;
  lock_IBM = FALSE;
  send_ack_IBM = FALSE;

  // Initialize dynamic print elements.
  if (slash == SETTING_TRUE) {
    ibm_print_zero = &IBM_PRINT_SLASH_0;
  } else {
    ibm_print_zero = &WW_PRINT_0;
  }
  if (asciiwheel == SETTING_TRUE) {
    ibm_print_rmark = &IBM_PRINT_RMARK_APW;
    ibm_print_gmark = &IBM_PRINT_GMARK_APW;
  } else {
    ibm_print_rmark = &IBM_PRINT_RMARK_IPW;
    ibm_print_gmark = &IBM_PRINT_GMARK_IPW;
  }

  // Turn on the Lock light to indicate mode 0.
  (void)Print_element (&WW_PRINT_Lock);
  lock_IBM = TRUE;
}


//**********************************************************************************************************************
//
//  IBM 1620 Jr. Console Typewriter support routines.
//
//**********************************************************************************************************************

// Print IBM 1620 Jr. setup title.
void Print_IBM_setup_title (void) {
  (void)Print_string ("\r\r---- Cadetwriter: " IBM_VERSION " Setup\r\r");
}

// Update IBM 1620 Jr. settings.
void Update_IBM_settings (void) {
  byte obattery = battery;
  byte oimpression = impression;
  byte ooffset = offset;

  // Query new settings.
  (void)Print_element (&WW_PRINT_CRtn);
  errors = Read_truefalse_setting ("record errors", errors);
  warnings = Read_truefalse_setting ("record warnings", warnings);
  battery = Read_truefalse_setting ("batteries installed", battery);
  impression = Read_impression_setting ("impression", impression);
  Change_impression (oimpression, impression);
  slash = Read_truefalse_setting ("slash zeroes", slash);
  if (slash == SETTING_TRUE) {
    ibm_print_zero = &IBM_PRINT_SLASH_0;
  } else {
    ibm_print_zero = &WW_PRINT_0;
  }
  bold = Read_truefalse_setting ("bold input", bold);
  offset = Read_integer_setting ("line offset", offset, 0, 10);
  asciiwheel = Read_truefalse_setting ("ASCII printwheel", asciiwheel);
  if (asciiwheel == SETTING_TRUE) {
    ibm_print_rmark = &IBM_PRINT_RMARK_APW;
    ibm_print_gmark = &IBM_PRINT_GMARK_APW;
  } else {
    ibm_print_rmark = &IBM_PRINT_RMARK_IPW;
    ibm_print_gmark = &IBM_PRINT_GMARK_IPW;
  }
  (void)Print_element (&WW_PRINT_CRtn);

  // Save settings in EEPROM if requested.
  if (Ask_yesno_question ("Save settings", FALSE)) {
    Write_EEPROM (EEPROM_ERRORS, errors);
    Write_EEPROM (EEPROM_WARNINGS, warnings);
    Write_EEPROM (EEPROM_BATTERY, battery);
    Write_EEPROM (EEPROM_OFFSET, offset);
    Write_EEPROM (EEPROM_ASCIIWHEEL, asciiwheel);
    Write_EEPROM (EEPROM_IMPRESSION, impression);
    Write_EEPROM (EEPROM_SLASH, slash);
    Write_EEPROM (EEPROM_BOLD, bold);
  }
  (void)Print_element (&WW_PRINT_CRtn);
 
  // Reset margins and tabs if battery or offset changed.
  if ((obattery != battery) || (ooffset != offset)) {
    Set_margins_tabs (TRUE);
  }
}

// Print IBM 1620 Jr. character set.
void Print_IBM_character_set (void) {
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_element (&WW_PRINT_SPACE);  (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&WW_PRINT_A);  (void)Print_element (&WW_PRINT_B);  (void)Print_element (&WW_PRINT_C);
  (void)Print_element (&WW_PRINT_D);  (void)Print_element (&WW_PRINT_E);  (void)Print_element (&WW_PRINT_F);
  (void)Print_element (&WW_PRINT_G);  (void)Print_element (&WW_PRINT_H);  (void)Print_element (&WW_PRINT_I);
  (void)Print_element (&WW_PRINT_J);  (void)Print_element (&WW_PRINT_K);  (void)Print_element (&WW_PRINT_L);
  (void)Print_element (&WW_PRINT_M);  (void)Print_element (&WW_PRINT_N);  (void)Print_element (&WW_PRINT_O);
  (void)Print_element (&WW_PRINT_P);  (void)Print_element (&WW_PRINT_Q);  (void)Print_element (&WW_PRINT_R);
  (void)Print_element (&WW_PRINT_S);  (void)Print_element (&WW_PRINT_T);  (void)Print_element (&WW_PRINT_U);
  (void)Print_element (&WW_PRINT_V);  (void)Print_element (&WW_PRINT_W);  (void)Print_element (&WW_PRINT_X);
  (void)Print_element (&WW_PRINT_Y);  (void)Print_element (&WW_PRINT_Z);
  (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&IBM_PRINT_0);  (void)Print_element (&WW_PRINT_1);  (void)Print_element (&WW_PRINT_2);
  (void)Print_element (&WW_PRINT_3);   (void)Print_element (&WW_PRINT_4);  (void)Print_element (&WW_PRINT_5);
  (void)Print_element (&WW_PRINT_6);   (void)Print_element (&WW_PRINT_7);  (void)Print_element (&WW_PRINT_8);
  (void)Print_element (&WW_PRINT_9);
  (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&IBM_PRINT_FLAG_0);  (void)Print_element (&IBM_PRINT_FLAG_1);
  (void)Print_element (&IBM_PRINT_FLAG_2);  (void)Print_element (&IBM_PRINT_FLAG_3);
  (void)Print_element (&IBM_PRINT_FLAG_4);  (void)Print_element (&IBM_PRINT_FLAG_5);
  (void)Print_element (&IBM_PRINT_FLAG_6);  (void)Print_element (&IBM_PRINT_FLAG_7);
  (void)Print_element (&IBM_PRINT_FLAG_8);  (void)Print_element (&IBM_PRINT_FLAG_9);
  (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&WW_PRINT_DOLLAR);  (void)Print_element (&WW_PRINT_LPAREN);
  (void)Print_element (&WW_PRINT_RPAREN);  (void)Print_element (&WW_PRINT_PLUS);
  (void)Print_element (&WW_PRINT_HYPHEN);  (void)Print_element (&WW_PRINT_ASTERISK);
  (void)Print_element (&WW_PRINT_SLASH);   (void)Print_element (&WW_PRINT_EQUAL);
  (void)Print_element (&WW_PRINT_PERIOD);  (void)Print_element (&WW_PRINT_COMMA);
  (void)Print_element (&WW_PRINT_AT);
  (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&IBM_PRINT_FLAG_NUMBLANK);
  (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&IBM_PRINT_RMARK);  (void)Print_element (&IBM_PRINT_FLAG_RMARK);
  (void)Print_element (&IBM_PRINT_GMARK);  (void)Print_element (&IBM_PRINT_FLAG_GMARK);
  (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&IBM_PRINT_INVALID);
  (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&IBM_PRINT_RELEASESTART);
  (void)Print_element (&WW_PRINT_CRtn);  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);
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
#define ASCII_KEY_RIGHTARROW                WW_KEY_RIGHTARROW_RightWord        // <right arrow> key code.
#define ASCII_KEY_SETUP                     WW_KEY_PaperUp_UpMicro             // <setup> key code.
#define ASCII_KEY_BSLASH_BAR_FS             WW_KEY_PaperDown_DownMicro         // \, | key code.
#define ASCII_KEY_DOWNARROW                 WW_KEY_DOWNARROW_DownLine          // <down arrow> key code.
#define ASCII_KEY_z_Z_SUB                   WW_KEY_z_Z                         // z, Z, SUB key code.
#define ASCII_KEY_q_Q_DC1_XON               WW_KEY_q_Q_Impr                    // q, Q, DC1/XON key code.
#define ASCII_KEY_1_EXCLAMATION             WW_KEY_1_EXCLAMATION_Spell         // 1, ! key code.
#define ASCII_KEY_BAPOSTROPHE_TILDE_RS      WW_KEY_PLUSMINUS_DEGREE            // `, ~ key code.
#define ASCII_KEY_a_A_SOH                   WW_KEY_a_A                         // a, A, SOH key code.
#define ASCII_KEY_SPACE                     WW_KEY_SPACE_REQSPACE              // <space> key code.
#define ASCII_KEY_LOADPAPER_SETTOPOFFORM    WW_KEY_LOADPAPER_SETTOPOFFORM      // <load paper> key code.
#define ASCII_KEY_LMAR                      WW_KEY_LMar                        // <left margin> key code.
#define ASCII_KEY_TCLR_TCLRALL              WW_KEY_TClr_TClrAll                // <tab clear>, <tab clear all> key code.
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
#define ASCII_KEY_LEFTARROW                 WW_KEY_LEFTARROW_LeftWord          // <left arrow> key code.
#define ASCII_KEY_UPARROW                   WW_KEY_UPARROW_UpLine              // <up arrow> key code.
#define ASCII_KEY_BACKSPACE                 WW_KEY_Backspace_Bksp1             // <backspace> key code.
#define ASCII_KEY_RETURN                    WW_KEY_CRtn_IndClr                 // <return> key code.
#define ASCII_KEY_DELETE_US                 WW_KEY_BACKX_Word                  // <delete> key code.
#define ASCII_KEY_LOCK                      WW_KEY_Lock                        // <shift lock> key code.
#define ASCII_KEY_RMAR                      WW_KEY_RMar                        // <right margin> key code.
#define ASCII_KEY_TAB                       WW_KEY_Tab_IndL                    // <tab> key code.
#define ASCII_KEY_ESCAPE_MARREL             WW_KEY_MarRel_RePrt                // <escape>, <margin release> key code.
#define ASCII_KEY_TSET                      WW_KEY_TSet                        // <tab set> key code.

// ASCII Terminal characters.
#define CHAR_ASCII_LF    0x0a  // LF character.
#define CHAR_ASCII_CR    0x0d  // CR character.
#define CHAR_ASCII_XON   0x11  // XON character.
#define CHAR_ASCII_XOFF  0x13  // XOFF character.
#define CHAR_ASCII_ESC   0x1b  // ESC character.


//**********************************************************************************************************************
//
//  ASCII Terminal dynamic print strings.
//
//**********************************************************************************************************************

// Dynamic print strings.
volatile const struct print_info *ascii_print_less;
volatile const struct print_info *ascii_print_greater;
volatile const struct print_info *ascii_print_bslash;
volatile const struct print_info *ascii_print_caret;
volatile const struct print_info *ascii_print_bapostrophe;
volatile const struct print_info *ascii_print_lbrace;
volatile const struct print_info *ascii_print_bar;
volatile const struct print_info *ascii_print_rbrace;
volatile const struct print_info *ascii_print_tilde;


//**********************************************************************************************************************
//
//  ASCII Terminal print strings.
//
//**********************************************************************************************************************

// Special <space> print strings.
const struct print_info *ASCII_STR_SPACEX[] = {&WW_PRINT_SPACE, &WW_PRINT_SPACE, &WW_PRINT_Bksp1, NULL_PRINT_INFO};
const struct print_info ASCII_PRINT_SPACEX  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &ASCII_STR_SPACEX};

// <, >, \, ^, `, {, |, }, ~ print strings (ASCII printwheel & Print Graphics).
const byte ASCII_STR_LESS_APW[]              = {WW_Code, WW_LESS_APW, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_LESS_APW = {ELEMENT_SIMPLE, SPACING_FORWARD, 68, 0,
                                                TIME_HIT + TIME_MOVE - (TIME_PRESS_CODE_12 + TIME_RELEASE_CODE_12),
                                                &ASCII_STR_LESS_APW};
const struct print_info *ASCII_STR_LESS_PG[] = {&WW_PRINT_DownMicro, &WW_PRINT_DownMicro, &WW_PRINT_PERIOD,
                                                &WW_PRINT_Bksp1, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1,
                                                &WW_PRINT_Bksp1, &WW_PRINT_DownMicro, &WW_PRINT_PERIOD,
                                                &WW_PRINT_Backspace, &WW_PRINT_UpMicro, &WW_PRINT_UpMicro,
                                                &WW_PRINT_PERIOD, &WW_PRINT_Bksp1, &WW_PRINT_UpMicro, NULL_PRINT_INFO};
const struct print_info ASCII_PRINT_LESS_PG  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &ASCII_STR_LESS_PG};
const struct print_info ASCII_PRINT_LESS     = {ELEMENT_DYNAMIC, 0, 0, 0, 0, &ascii_print_less};

const byte ASCII_STR_GREATER_APW[]              = {WW_Code, WW_GREATER_APW, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_GREATER_APW = {ELEMENT_SIMPLE, SPACING_FORWARD, 69, 0,
                                                   TIME_HIT + TIME_MOVE - (TIME_PRESS_CODE_12 + TIME_RELEASE_CODE_12),
                                                   &ASCII_STR_GREATER_APW};
const struct print_info *ASCII_STR_GREATER_PG[] = {&WW_PRINT_MarRel, &WW_PRINT_Bksp1, &WW_PRINT_DownMicro,
                                                   &WW_PRINT_PERIOD, &WW_PRINT_MarRel, &WW_PRINT_Backspace,
                                                   &WW_PRINT_DownMicro, &WW_PRINT_DownMicro, &WW_PRINT_PERIOD,
                                                   &WW_PRINT_Bksp1, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1,
                                                   &WW_PRINT_Bksp1, &WW_PRINT_UpMicro, &WW_PRINT_PERIOD,
                                                   &WW_PRINT_UpMicro, &WW_PRINT_UpMicro, NULL_PRINT_INFO};
const struct print_info ASCII_PRINT_GREATER_PG  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &ASCII_STR_GREATER_PG};
const struct print_info ASCII_PRINT_GREATER     = {ELEMENT_DYNAMIC, 0, 0, 0, 0, &ascii_print_greater};

const byte ASCII_STR_BSLASH_APW[]              = {WW_Code, WW_BSLASH_APW, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_BSLASH_APW = {ELEMENT_SIMPLE, SPACING_FORWARD, 66, 0,
                                                  TIME_HIT + TIME_MOVE - (TIME_PRESS_CODE_10 + TIME_RELEASE_CODE_10),
                                                  &ASCII_STR_BSLASH_APW};
const struct print_info *ASCII_STR_BSLASH_PG[] = {&WW_PRINT_MarRel, &WW_PRINT_Bksp1, &WW_PRINT_DownMicro,
                                                  &WW_PRINT_DownMicro, &WW_PRINT_PERIOD, &WW_PRINT_MarRel,
                                                  &WW_PRINT_Backspace, &WW_PRINT_Bksp1, &WW_PRINT_DownMicro,
                                                  &WW_PRINT_PERIOD, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1,
                                                  &WW_PRINT_UpMicro, &WW_PRINT_UpMicro, &WW_PRINT_UpMicro,
                                                  &WW_PRINT_PERIOD, &WW_PRINT_Backspace, &WW_PRINT_Bksp1,
                                                  &WW_PRINT_DownMicro, &WW_PRINT_PERIOD, &WW_PRINT_UpMicro,
                                                  NULL_PRINT_INFO};
const struct print_info ASCII_PRINT_BSLASH_PG  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &ASCII_STR_BSLASH_PG};
const struct print_info ASCII_PRINT_BSLASH      = {ELEMENT_DYNAMIC, 0, 0, 0, 0, &ascii_print_bslash};

const byte ASCII_STR_CARET_APW[]              = {WW_LShift, WW_CARET_APW, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_CARET_APW = {ELEMENT_SIMPLE, SPACING_FORWARD, 57, 0,
                                                 TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_9 + TIME_RELEASE_SHIFT_9),
                                                 &ASCII_STR_CARET_APW};
const struct print_info *ASCII_STR_CARET_PG[] = {&WW_PRINT_MarRel, &WW_PRINT_Bksp1, &WW_PRINT_DownMicro,
                                                 &WW_PRINT_DownMicro, &WW_PRINT_DownMicro, &WW_PRINT_PERIOD,
                                                 &WW_PRINT_Bksp1, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1,
                                                 &WW_PRINT_PERIOD, &WW_PRINT_Backspace, &WW_PRINT_Bksp1,
                                                 &WW_PRINT_DownMicro, &WW_PRINT_PERIOD, &WW_PRINT_UpMicro,
                                                 &WW_PRINT_UpMicro, &WW_PRINT_UpMicro, &WW_PRINT_UpMicro,
                                                 NULL_PRINT_INFO};
const struct print_info ASCII_PRINT_CARET_PG  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &ASCII_STR_CARET_PG};
const struct print_info ASCII_PRINT_CARET     = {ELEMENT_DYNAMIC, 0, 0, 0, 0, &ascii_print_caret};

const byte ASCII_STR_BAPOSTROPHE_APW[]              = {WW_BAPOSTROPHE_APW, WW_NULL_3, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_BAPOSTROPHE_APW = {ELEMENT_SIMPLE, SPACING_FORWARD, 59, 0,
                                                       TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_3 +
                                                                               TIME_RELEASE_NOSHIFT_3),
                                                       &ASCII_STR_BAPOSTROPHE_APW};
const struct print_info *ASCII_STR_BAPOSTROPHE_PG[] = {&WW_PRINT_MarRel, &WW_PRINT_Bksp1, &WW_PRINT_DownMicro,
                                                       &WW_PRINT_DownMicro, &WW_PRINT_DownMicro, &WW_PRINT_DownMicro,
                                                       &WW_PRINT_PERIOD, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1,
                                                       &WW_PRINT_Bksp1, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1,
                                                       &WW_PRINT_UpMicro, &WW_PRINT_PERIOD, &WW_PRINT_UpMicro,
                                                       &WW_PRINT_UpMicro, &WW_PRINT_UpMicro, NULL_PRINT_INFO};
const struct print_info ASCII_PRINT_BAPOSTROPHE_PG  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0,
                                                       &ASCII_STR_BAPOSTROPHE_PG};
const struct print_info ASCII_PRINT_BAPOSTROPHE     = {ELEMENT_DYNAMIC, 0, 0, 0, 0, &ascii_print_bapostrophe};

const byte ASCII_STR_LBRACE_APW[]              = {WW_LShift, WW_LBRACE_APW, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_LBRACE_APW = {ELEMENT_SIMPLE, SPACING_FORWARD, 70, 0,
                                                  TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_12 + TIME_RELEASE_SHIFT_12),
                                                  &ASCII_STR_LBRACE_APW};
const struct print_info *ASCII_STR_LBRACE_PG[] = {&WW_PRINT_MarRel, &WW_PRINT_Bksp1, &WW_PRINT_HYPHEN, &WW_PRINT_Bksp1,
                                                  &WW_PRINT_Bksp1, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1, &WW_PRINT_LPAREN,
                                                  &WW_PRINT_Backspace, &WW_PRINT_LBRACKET, &WW_PRINT_Bksp1,
                                                  NULL_PRINT_INFO};
const struct print_info ASCII_PRINT_LBRACE_PG  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &ASCII_STR_LBRACE_PG};
const struct print_info ASCII_PRINT_LBRACE     = {ELEMENT_DYNAMIC, 0, 0, 0, 0, &ascii_print_lbrace};

const byte ASCII_STR_BAR_APW[]              = {WW_Code, WW_BAR_APW, WW_Code, WW_NULL_5, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_BAR_APW = {ELEMENT_SIMPLE, SPACING_FORWARD, 71, 0,
                                               TIME_HIT + TIME_MOVE - (TIME_PRESS_CODE_12 + TIME_RELEASE_CODE_12),
                                               &ASCII_STR_BAR_APW};
const struct print_info *ASCII_STR_BAR_PG[] = {&WW_PRINT_EXCLAMATION, &WW_PRINT_Backspace, &WW_PRINT_i,
                                               NULL_PRINT_INFO};
const struct print_info ASCII_PRINT_BAR_PG  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &ASCII_STR_BAR_PG};
const struct print_info ASCII_PRINT_BAR     = {ELEMENT_DYNAMIC, 0, 0, 0, 0, &ascii_print_bar};

const byte ASCII_STR_RBRACE_APW[]              = {WW_RBRACE_APW, WW_NULL_12, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_RBRACE_APW = {ELEMENT_SIMPLE, SPACING_FORWARD, 65, 0,
                                                  TIME_HIT + TIME_MOVE - (TIME_PRESS_NOSHIFT_12 +
                                                                          TIME_RELEASE_NOSHIFT_12),
                                                  &ASCII_STR_RBRACE_APW};
const struct print_info *ASCII_STR_RBRACE_PG[] = {&WW_PRINT_MarRel, &WW_PRINT_Bksp1, &WW_PRINT_RPAREN,
                                                  &WW_PRINT_Backspace, &WW_PRINT_RBRACKET, &WW_PRINT_Bksp1,
                                                  &WW_PRINT_Bksp1, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1, &WW_PRINT_HYPHEN,
                                                  &WW_PRINT_Bksp1, NULL_PRINT_INFO};
const struct print_info ASCII_PRINT_RBRACE_PG  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &ASCII_STR_RBRACE_PG};
const struct print_info ASCII_PRINT_RBRACE     = {ELEMENT_DYNAMIC, 0, 0, 0, 0, &ascii_print_rbrace};

const byte ASCII_STR_TILDE_APW[]              = {WW_LShift, WW_TILDE_APW, WW_LShift, WW_NULL_1, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_TILDE_APW = {ELEMENT_SIMPLE, SPACING_FORWARD, 67, 0,
                                                 TIME_HIT + TIME_MOVE - (TIME_PRESS_SHIFT_3 + TIME_RELEASE_SHIFT_3),
                                                 &ASCII_STR_TILDE_APW};
const struct print_info *ASCII_STR_TILDE_PG[] = {&WW_PRINT_MarRel, &WW_PRINT_Bksp1, &WW_PRINT_DownMicro,
                                                 &WW_PRINT_DownMicro, &WW_PRINT_DownMicro, &WW_PRINT_PERIOD,
                                                 &WW_PRINT_MarRel, &WW_PRINT_Backspace, &WW_PRINT_Bksp1,
                                                 &WW_PRINT_UpMicro, &WW_PRINT_PERIOD, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1,
                                                 &WW_PRINT_PERIOD, &WW_PRINT_Backspace, &WW_PRINT_Bksp1,
                                                 &WW_PRINT_UpMicro, &WW_PRINT_PERIOD, &WW_PRINT_Backspace,
                                                 &WW_PRINT_Bksp1, &WW_PRINT_DownMicro, &WW_PRINT_PERIOD,
                                                 &WW_PRINT_RIGHTARROW, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1,
                                                 &WW_PRINT_UpMicro, &WW_PRINT_UpMicro, NULL_PRINT_INFO};
const struct print_info ASCII_PRINT_TILDE_PG  = {ELEMENT_COMPOSITE, SPACING_FORWARD, 0, 0, 0, &ASCII_STR_TILDE_PG};
const struct print_info ASCII_PRINT_TILDE     = {ELEMENT_DYNAMIC, 0, 0, 0, 0, &ascii_print_tilde};

// CR, LF print strings.
const struct print_info *ASCII_STR_CR[] = {&WW_PRINT_CRtn, &WW_PRINT_UPARROW, NULL_PRINT_INFO};
const struct print_info ASCII_PRINT_CR  = {ELEMENT_COMPOSITE, SPACING_RETURN, 0, 0, 0, &ASCII_STR_CR};

const byte ASCII_STR_LF[]              = {WW_DOWNARROW_DownLine, WW_NULL_2, WW_NULL_14, WW_NULL};
const struct print_info ASCII_PRINT_LF = {ELEMENT_SIMPLE, SPACING_NONE, POSITION_NOCHANGE, 0,
                                          TIME_VMOVE - (TIME_PRESS_NOSHIFT_2 + TIME_RELEASE_NOSHIFT_2),
                                          &ASCII_STR_LF};


//**********************************************************************************************************************
//
//  ASCII Terminal serial action table.
//
//**********************************************************************************************************************

const struct serial_action ASCII_SERIAL_ACTIONS[128] = { 
  {CMD_NONE,          &WW_PRINT_NULL},            // NUL - Used for adding null scans to pad composite characters.
  {CMD_NONE,          NULL},                      // SOH
  {CMD_NONE,          NULL},                      // STX
  {CMD_NONE,          NULL},                      // ETX
  {CMD_NONE,          NULL},                      // EOT
  {CMD_NONE,          NULL},                      // ENQ
  {CMD_NONE,          NULL},                      // ACK
  {CMD_PRINT,         &WW_PRINT_BEEP},            // BEL
  {CMD_PRINT,         &WW_PRINT_Backspace},       // BS
  {CMD_PRINT,         &WW_PRINT_Tab},             // TAB
  {CMD_ASCII_LF,      NULL},                      // LF
  {CMD_NONE,          NULL},                      // VT
  {CMD_NONE,          NULL},                      // FF
  {CMD_ASCII_CR,      &WW_PRINT_CRtn},            // CR
  {CMD_NONE,          NULL},                      // SO
  {CMD_NONE,          NULL},                      // SI
  {CMD_NONE,          NULL},                      // DLE
  {CMD_ASCII_XON,     NULL},                      // DC1/XON
  {CMD_NONE,          NULL},                      // DC2
  {CMD_ASCII_XOFF,    NULL},                      // DC3/XOFF
  {CMD_NONE,          NULL},                      // DC4
  {CMD_NONE,          NULL},                      // NAK
  {CMD_NONE,          NULL},                      // SYN
  {CMD_NONE,          NULL},                      // ETB
  {CMD_NONE,          NULL},                      // CAN
  {CMD_NONE,          NULL},                      // EM
  {CMD_NONE,          NULL},                      // SUB
  {CMD_NONE,          NULL},                      // ESC
  {CMD_NONE,          NULL},                      // FS
  {CMD_NONE,          NULL},                      // GS
  {CMD_NONE,          NULL},                      // RS
  {CMD_NONE,          NULL},                      // US
  {CMD_PRINT,         &WW_PRINT_SPACE},           //
  {CMD_PRINT,         &WW_PRINT_EXCLAMATION},     // !
  {CMD_PRINT,         &WW_PRINT_QUOTE},           // "
  {CMD_PRINT,         &WW_PRINT_POUND},           // #
  {CMD_PRINT,         &WW_PRINT_DOLLAR},          // $
  {CMD_PRINT,         &WW_PRINT_PERCENT},         // %
  {CMD_PRINT,         &WW_PRINT_AMPERSAND},       // &
  {CMD_PRINT,         &WW_PRINT_APOSTROPHE},      // '
  {CMD_PRINT,         &WW_PRINT_LPAREN},          // (
  {CMD_PRINT,         &WW_PRINT_RPAREN},          // )
  {CMD_PRINT,         &WW_PRINT_ASTERISK},        // *
  {CMD_PRINT,         &WW_PRINT_PLUS},            // +
  {CMD_PRINT,         &WW_PRINT_COMMA},           // , 
  {CMD_PRINT,         &WW_PRINT_HYPHEN},          // -
  {CMD_PRINT,         &WW_PRINT_PERIOD},          // .
  {CMD_PRINT,         &WW_PRINT_SLASH},           // /
  {CMD_PRINT,         &WW_PRINT_0},               // 0
  {CMD_PRINT,         &WW_PRINT_1},               // 1
  {CMD_PRINT,         &WW_PRINT_2},               // 2
  {CMD_PRINT,         &WW_PRINT_3},               // 3
  {CMD_PRINT,         &WW_PRINT_4},               // 4
  {CMD_PRINT,         &WW_PRINT_5},               // 5
  {CMD_PRINT,         &WW_PRINT_6},               // 6
  {CMD_PRINT,         &WW_PRINT_7},               // 7
  {CMD_PRINT,         &WW_PRINT_8},               // 8
  {CMD_PRINT,         &WW_PRINT_9},               // 9
  {CMD_PRINT,         &WW_PRINT_COLON},           // :
  {CMD_PRINT,         &WW_PRINT_SEMICOLON},       // ;
  {CMD_PRINT,         &ASCII_PRINT_LESS},         // <
  {CMD_PRINT,         &WW_PRINT_EQUAL},           // =
  {CMD_PRINT,         &ASCII_PRINT_GREATER},      // >
  {CMD_PRINT,         &WW_PRINT_QUESTION},        // ?
  {CMD_PRINT,         &WW_PRINT_AT},              // @
  {CMD_PRINT,         &WW_PRINT_A},               // A
  {CMD_PRINT,         &WW_PRINT_B},               // B
  {CMD_PRINT,         &WW_PRINT_C},               // C
  {CMD_PRINT,         &WW_PRINT_D},               // D
  {CMD_PRINT,         &WW_PRINT_E},               // E
  {CMD_PRINT,         &WW_PRINT_F},               // F
  {CMD_PRINT,         &WW_PRINT_G},               // G
  {CMD_PRINT,         &WW_PRINT_H},               // H
  {CMD_PRINT,         &WW_PRINT_I},               // I
  {CMD_PRINT,         &WW_PRINT_J},               // J
  {CMD_PRINT,         &WW_PRINT_K},               // K
  {CMD_PRINT,         &WW_PRINT_L},               // L
  {CMD_PRINT,         &WW_PRINT_M},               // M
  {CMD_PRINT,         &WW_PRINT_N},               // N
  {CMD_PRINT,         &WW_PRINT_O},               // O
  {CMD_PRINT,         &WW_PRINT_P},               // P
  {CMD_PRINT,         &WW_PRINT_Q},               // Q
  {CMD_PRINT,         &WW_PRINT_R},               // R
  {CMD_PRINT,         &WW_PRINT_S},               // S
  {CMD_PRINT,         &WW_PRINT_T},               // T
  {CMD_PRINT,         &WW_PRINT_U},               // U
  {CMD_PRINT,         &WW_PRINT_V},               // V
  {CMD_PRINT,         &WW_PRINT_W},               // W
  {CMD_PRINT,         &WW_PRINT_X},               // X
  {CMD_PRINT,         &WW_PRINT_Y},               // Y
  {CMD_PRINT,         &WW_PRINT_Z},               // Z
  {CMD_PRINT,         &WW_PRINT_LBRACKET},        // [
  {CMD_PRINT,         &ASCII_PRINT_BSLASH},       // <backslash>
  {CMD_PRINT,         &WW_PRINT_RBRACKET},        // ]
  {CMD_PRINT,         &ASCII_PRINT_CARET},        // ^
  {CMD_PRINT,         &WW_PRINT_UNDERSCORE},      // _
  {CMD_PRINT,         &ASCII_PRINT_BAPOSTROPHE},  // `
  {CMD_PRINT,         &WW_PRINT_a},               // a
  {CMD_PRINT,         &WW_PRINT_b},               // b
  {CMD_PRINT,         &WW_PRINT_c},               // c
  {CMD_PRINT,         &WW_PRINT_d},               // d
  {CMD_PRINT,         &WW_PRINT_e},               // e
  {CMD_PRINT,         &WW_PRINT_f},               // f
  {CMD_PRINT,         &WW_PRINT_g},               // g
  {CMD_PRINT,         &WW_PRINT_h},               // h
  {CMD_PRINT,         &WW_PRINT_i},               // i
  {CMD_PRINT,         &WW_PRINT_j},               // j
  {CMD_PRINT,         &WW_PRINT_k},               // k
  {CMD_PRINT,         &WW_PRINT_l},               // l
  {CMD_PRINT,         &WW_PRINT_m},               // m
  {CMD_PRINT,         &WW_PRINT_n},               // n
  {CMD_PRINT,         &WW_PRINT_o},               // o
  {CMD_PRINT,         &WW_PRINT_p},               // p
  {CMD_PRINT,         &WW_PRINT_q},               // q
  {CMD_PRINT,         &WW_PRINT_r},               // r
  {CMD_PRINT,         &WW_PRINT_s},               // s
  {CMD_PRINT,         &WW_PRINT_t},               // t
  {CMD_PRINT,         &WW_PRINT_u},               // u
  {CMD_PRINT,         &WW_PRINT_v},               // v
  {CMD_PRINT,         &WW_PRINT_w},               // w
  {CMD_PRINT,         &WW_PRINT_x},               // x
  {CMD_PRINT,         &WW_PRINT_y},               // y
  {CMD_PRINT,         &WW_PRINT_z},               // z
  {CMD_PRINT,         &ASCII_PRINT_LBRACE},       // {
  {CMD_PRINT,         &ASCII_PRINT_BAR},          // |
  {CMD_PRINT,         &ASCII_PRINT_RBRACE},       // }
  {CMD_PRINT,         &ASCII_PRINT_TILDE},        // ~
  {CMD_NONE,          NULL}                       // DEL
};


//**********************************************************************************************************************
//
//  ASCII Terminal key action tables.
//
//**********************************************************************************************************************

// Half duplex key action table.
const struct key_action ASCII_ACTIONS_HALF[3 * NUM_WW_KEYS] = { 
  // Unshifted.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_RIGHTARROW},      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     '\\', 0, &ASCII_PRINT_BSLASH},       // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_DOWNARROW},       // <down arrow>
  {ACTION_SEND | ACTION_PRINT,                     'z',  0, &WW_PRINT_z},               // z, Z, SUB
  {ACTION_SEND | ACTION_PRINT,                     'q',  0, &WW_PRINT_q},               // q, Q, DC1/XON
  {ACTION_SEND | ACTION_PRINT,                     '1',  0, &WW_PRINT_1},               // 1, !
  {ACTION_SEND | ACTION_PRINT,                     '`',  0, &ASCII_PRINT_BAPOSTROPHE},  // `, ~
  {ACTION_SEND | ACTION_PRINT,                     'a',  0, &WW_PRINT_a},               // a, A, SOH
  {ACTION_SEND | ACTION_PRINT,                     ' ',  0, &WW_PRINT_SPACE},           // <space>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LOADPAPER},       // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LMar},            // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TClr},            // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_SEND | ACTION_PRINT,                     'x',  0, &WW_PRINT_x},               // x, X, CAN
  {ACTION_SEND | ACTION_PRINT,                     'w',  0, &WW_PRINT_w},               // w, W, ETB
  {ACTION_SEND | ACTION_PRINT,                     '2',  0, &WW_PRINT_2},               // 2, @, NUL
  {ACTION_SEND | ACTION_PRINT,                     's',  0, &WW_PRINT_s},               // s, S, DC3/XOFF
  {ACTION_SEND | ACTION_PRINT,                     'c',  0, &WW_PRINT_c},               // c, C, ETX
  {ACTION_SEND | ACTION_PRINT,                     'e',  0, &WW_PRINT_e},               // e, E, ENQ
  {ACTION_SEND | ACTION_PRINT,                     '3',  0, &WW_PRINT_3},               // 3, #
  {ACTION_SEND | ACTION_PRINT,                     'd',  0, &WW_PRINT_d},               // d, D, EOT
  {ACTION_SEND | ACTION_PRINT,                     'b',  0, &WW_PRINT_b},               // b, B, STX
  {ACTION_SEND | ACTION_PRINT,                     'v',  0, &WW_PRINT_v},               // v, V, SYN
  {ACTION_SEND | ACTION_PRINT,                     't',  0, &WW_PRINT_t},               // t, T, DC4
  {ACTION_SEND | ACTION_PRINT,                     'r',  0, &WW_PRINT_r},               // r, R, DC2
  {ACTION_SEND | ACTION_PRINT,                     '4',  0, &WW_PRINT_4},               // 4, $
  {ACTION_SEND | ACTION_PRINT,                     '5',  0, &WW_PRINT_5},               // 5, %
  {ACTION_SEND | ACTION_PRINT,                     'f',  0, &WW_PRINT_f},               // f, F, ACK
  {ACTION_SEND | ACTION_PRINT,                     'g',  0, &WW_PRINT_g},               // g, G, BEL
  {ACTION_SEND | ACTION_PRINT,                     'n',  0, &WW_PRINT_n},               // n, N, SO
  {ACTION_SEND | ACTION_PRINT,                     'm',  0, &WW_PRINT_m},               // m, M, CR
  {ACTION_SEND | ACTION_PRINT,                     'y',  0, &WW_PRINT_y},               // y, Y, EM
  {ACTION_SEND | ACTION_PRINT,                     'u',  0, &WW_PRINT_u},               // u, U, NAK
  {ACTION_SEND | ACTION_PRINT,                     '7',  0, &WW_PRINT_7},               // 7, &
  {ACTION_SEND | ACTION_PRINT,                     '6',  0, &WW_PRINT_6},               // 6, ^, RS
  {ACTION_SEND | ACTION_PRINT,                     'j',  0, &WW_PRINT_j},               // j, J, LF
  {ACTION_SEND | ACTION_PRINT,                     'h',  0, &WW_PRINT_h},               // h, H, BS
  {ACTION_SEND | ACTION_PRINT,                     ',',  0, &WW_PRINT_COMMA},           // ,, <
  {ACTION_SEND | ACTION_PRINT,                     ']',  0, &WW_PRINT_RBRACKET},        // ], }, GS
  {ACTION_SEND | ACTION_PRINT,                     'i',  0, &WW_PRINT_i},               // i, I, TAB
  {ACTION_SEND | ACTION_PRINT,                     '8',  0, &WW_PRINT_8},               // 8, *
  {ACTION_SEND | ACTION_PRINT,                     '=',  0, &WW_PRINT_EQUAL},           // =, +
  {ACTION_SEND | ACTION_PRINT,                     'k',  0, &WW_PRINT_k},               // k, K, VT
  {ACTION_SEND | ACTION_PRINT,                     '.',  0, &WW_PRINT_PERIOD},          // ., >
  {ACTION_SEND | ACTION_PRINT,                     'o',  0, &WW_PRINT_o},               // o, O, SI
  {ACTION_SEND | ACTION_PRINT,                     '9',  0, &WW_PRINT_9},               // 9, (
  {ACTION_SEND | ACTION_PRINT,                     'l',  0, &WW_PRINT_l},               // l, L, FF
  {ACTION_SEND | ACTION_PRINT,                     '/',  0, &WW_PRINT_SLASH},           // /, ?
  {ACTION_SEND | ACTION_PRINT,                     '[',  0, &WW_PRINT_LBRACKET},        // [, {, ESC
  {ACTION_SEND | ACTION_PRINT,                     'p',  0, &WW_PRINT_p},               // p, P, DLE
  {ACTION_SEND | ACTION_PRINT,                     '0',  0, &WW_PRINT_0},               // 0, )
  {ACTION_SEND | ACTION_PRINT,                     '-',  0, &WW_PRINT_HYPHEN},          // -, _, US
  {ACTION_SEND | ACTION_PRINT,                     ';',  0, &WW_PRINT_SEMICOLON},       // ;, :
  {ACTION_SEND | ACTION_PRINT,                     '\'', 0, &WW_PRINT_APOSTROPHE},      // ', "
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LEFTARROW},       // <left arrow>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_UPARROW},         // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     0x08, 0, &WW_PRINT_Backspace},       // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_ASCII_RETURN,                            0,    0, NULL},                      // <return>
  {ACTION_SEND,                                    0x7f, 0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_RMar},            // <right margin>
  {ACTION_SEND | ACTION_PRINT,                     0x09, 0, &WW_PRINT_Tab},             // <tab>
  {ACTION_SEND,                                    0x1b, 0, NULL},                      // <escape>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TSet},            // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_RIGHTARROW},      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     '|',  0, &ASCII_PRINT_BAR},          // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_DOWNARROW},       // <down arrow>
  {ACTION_SEND | ACTION_PRINT,                     'Z',  0, &WW_PRINT_Z},               // z, Z, SUB
  {ACTION_SEND | ACTION_PRINT,                     'Q',  0, &WW_PRINT_Q},               // q, Q, DC1/XON
  {ACTION_SEND | ACTION_PRINT,                     '!',  0, &WW_PRINT_EXCLAMATION},     // 1, !
  {ACTION_SEND | ACTION_PRINT,                     '~',  0, &ASCII_PRINT_TILDE},        // `, ~
  {ACTION_SEND | ACTION_PRINT,                     'A',  0, &WW_PRINT_A},               // a, A, SOH
  {ACTION_SEND | ACTION_PRINT,                     ' ',  0, &WW_PRINT_SPACE},           // <space>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LOADPAPER},       // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LMar},            // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TClr},            // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_SEND | ACTION_PRINT,                     'X',  0, &WW_PRINT_X},               // x, X, CAN
  {ACTION_SEND | ACTION_PRINT,                     'W',  0, &WW_PRINT_W},               // w, W, ETB
  {ACTION_SEND | ACTION_PRINT,                     '@',  0, &WW_PRINT_AT},              // 2, @, NUL
  {ACTION_SEND | ACTION_PRINT,                     'S',  0, &WW_PRINT_S},               // s, S, DC3/XOFF
  {ACTION_SEND | ACTION_PRINT,                     'C',  0, &WW_PRINT_C},               // c, C, ETX
  {ACTION_SEND | ACTION_PRINT,                     'E',  0, &WW_PRINT_E},               // e, E, ENQ
  {ACTION_SEND | ACTION_PRINT,                     '#',  0, &WW_PRINT_POUND},           // 3, #
  {ACTION_SEND | ACTION_PRINT,                     'D',  0, &WW_PRINT_D},               // d, D, EOT
  {ACTION_SEND | ACTION_PRINT,                     'B',  0, &WW_PRINT_B},               // b, B, STX
  {ACTION_SEND | ACTION_PRINT,                     'V',  0, &WW_PRINT_V},               // v, V, SYN
  {ACTION_SEND | ACTION_PRINT,                     'T',  0, &WW_PRINT_T},               // t, T, DC4
  {ACTION_SEND | ACTION_PRINT,                     'R',  0, &WW_PRINT_R},               // r, R, DC2
  {ACTION_SEND | ACTION_PRINT,                     '$',  0, &WW_PRINT_DOLLAR},          // 4, $
  {ACTION_SEND | ACTION_PRINT,                     '%',  0, &WW_PRINT_PERCENT},         // 5, %
  {ACTION_SEND | ACTION_PRINT,                     'F',  0, &WW_PRINT_F},               // f, F, ACK
  {ACTION_SEND | ACTION_PRINT,                     'G',  0, &WW_PRINT_G},               // g, G, BEL
  {ACTION_SEND | ACTION_PRINT,                     'N',  0, &WW_PRINT_N},               // n, N, SO
  {ACTION_SEND | ACTION_PRINT,                     'M',  0, &WW_PRINT_M},               // m, M, CR
  {ACTION_SEND | ACTION_PRINT,                     'Y',  0, &WW_PRINT_Y},               // y, Y, EM
  {ACTION_SEND | ACTION_PRINT,                     'U',  0, &WW_PRINT_U},               // u, U, NAK
  {ACTION_SEND | ACTION_PRINT,                     '&',  0, &WW_PRINT_AMPERSAND},       // 7, &
  {ACTION_SEND | ACTION_PRINT,                     '^',  0, &ASCII_PRINT_CARET},        // 6, ^, RS
  {ACTION_SEND | ACTION_PRINT,                     'J',  0, &WW_PRINT_J},               // j, J, LF
  {ACTION_SEND | ACTION_PRINT,                     'H',  0, &WW_PRINT_H},               // h, H, BS
  {ACTION_SEND | ACTION_PRINT,                     '<',  0, &ASCII_PRINT_LESS},         // ,, <
  {ACTION_SEND | ACTION_PRINT,                     '}',  0, &ASCII_PRINT_RBRACE},       // ], }, GS
  {ACTION_SEND | ACTION_PRINT,                     'I',  0, &WW_PRINT_I},               // i, I, TAB
  {ACTION_SEND | ACTION_PRINT,                     '*',  0, &WW_PRINT_ASTERISK},        // 8, *
  {ACTION_SEND | ACTION_PRINT,                     '+',  0, &WW_PRINT_PLUS},            // =, +
  {ACTION_SEND | ACTION_PRINT,                     'K',  0, &WW_PRINT_K},               // k, K, VT
  {ACTION_SEND | ACTION_PRINT,                     '>',  0, &ASCII_PRINT_GREATER},      // ., >
  {ACTION_SEND | ACTION_PRINT,                     'O',  0, &WW_PRINT_O},               // o, O, SI
  {ACTION_SEND | ACTION_PRINT,                     '(',  0, &WW_PRINT_LPAREN},          // 9, (
  {ACTION_SEND | ACTION_PRINT,                     'L',  0, &WW_PRINT_L},               // l, L, FF
  {ACTION_SEND | ACTION_PRINT,                     '?',  0, &WW_PRINT_QUESTION},        // /, ?
  {ACTION_SEND | ACTION_PRINT,                     '{',  0, &ASCII_PRINT_LBRACE},       // [, {, ESC
  {ACTION_SEND | ACTION_PRINT,                     'P',  0, &WW_PRINT_P},               // p, P, DLE
  {ACTION_SEND | ACTION_PRINT,                     ')',  0, &WW_PRINT_RPAREN},          // 0, )
  {ACTION_SEND | ACTION_PRINT,                     '_',  0, &WW_PRINT_UNDERSCORE},      // -, _, US
  {ACTION_SEND | ACTION_PRINT,                     ':',  0, &WW_PRINT_COLON},           // ;, :
  {ACTION_SEND | ACTION_PRINT,                     '"',  0, &WW_PRINT_QUOTE},           // ', "
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LEFTARROW},       // <left arrow>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_UPARROW},         // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     0x08, 0, &WW_PRINT_Backspace},       // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_ASCII_RETURN,                            0,    0, NULL},                      // <return>
  {ACTION_SEND,                                    0x7f, 0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_RMar},            // <right margin>
  {ACTION_SEND | ACTION_PRINT,                     0x09, 0, &WW_PRINT_Tab},             // <tab>
  {ACTION_SEND,                                    0x1b, 0, NULL},                      // <escape>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TSet},            // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000

  // Control.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_ASCII_SETUP,                             0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_UpMicro},         // <down arrow>
  {ACTION_SEND,                                    0x1a, 0, NULL},                      // z, Z, SUB
  {ACTION_SEND,                                    0x11, 0, NULL},                      // q, Q, DC1/XON
  {ACTION_NONE,                                    0,    0, NULL},                      // 1, !
  {ACTION_NONE,                                    0,    0, NULL},                      // `, ~
  {ACTION_SEND,                                    0x01, 0, NULL},                      // a, A, SOH
  {ACTION_NONE,                                    0,    0, NULL},                      // <space>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_SETTOPOFFORM},    // <set top of form>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TClrAll},         // <tab clear all>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_SEND,                                    0x18, 0, NULL},                      // x, X, CAN
  {ACTION_SEND,                                    0x17, 0, NULL},                      // w, W, ETB
  {ACTION_SEND,                                    0x00, 0, NULL},                      // 2, @, NUL
  {ACTION_SEND,                                    0x13, 0, NULL},                      // s, S, DC3/XOFF
  {ACTION_SEND,                                    0x03, 0, NULL},                      // c, C, ETX
  {ACTION_SEND,                                    0x05, 0, NULL},                      // e, E, ENQ
  {ACTION_NONE,                                    0,    0, NULL},                      // 3, #
  {ACTION_SEND,                                    0x04, 0, NULL},                      // d, D, EOT
  {ACTION_SEND,                                    0x02, 0, NULL},                      // b, B, STX
  {ACTION_SEND,                                    0x16, 0, NULL},                      // v, V, SYN
  {ACTION_SEND,                                    0x14, 0, NULL},                      // t, T, DC4
  {ACTION_SEND,                                    0x12, 0, NULL},                      // r, R, DC2
  {ACTION_NONE,                                    0,    0, NULL},                      // 4, $
  {ACTION_NONE,                                    0,    0, NULL},                      // 5, %
  {ACTION_SEND,                                    0x06, 0, NULL},                      // f, F, ACK
  {ACTION_SEND | ACTION_PRINT,                     0x07, 0, &WW_PRINT_BEEP},            // g, G, BEL
  {ACTION_SEND,                                    0x0e, 0, NULL},                      // n, N, SO
  {ACTION_SEND | ACTION_PRINT,                     0x0d, 0, &ASCII_PRINT_CR},           // m, M, CR
  {ACTION_SEND,                                    0x19, 0, NULL},                      // y, Y, EM
  {ACTION_SEND,                                    0x15, 0, NULL},                      // u, U, NAK
  {ACTION_NONE,                                    0,    0, NULL},                      // 7, &
  {ACTION_SEND,                                    0x1e, 0, NULL},                      // 6, ^, RS
  {ACTION_SEND | ACTION_PRINT,                     0x0a, 0, &ASCII_PRINT_LF},           // j, J, LF
  {ACTION_SEND | ACTION_PRINT,                     0x08, 0, &WW_PRINT_Backspace},       // h, H, BS
  {ACTION_NONE,                                    0,    0, NULL},                      // ,, <
  {ACTION_SEND,                                    0x1d, 0, NULL},                      // ], }, GS
  {ACTION_SEND | ACTION_PRINT,                     0x09, 0, &WW_PRINT_Tab},             // i, I, TAB
  {ACTION_NONE,                                    0,    0, NULL},                      // 8, *
  {ACTION_NONE,                                    0,    0, NULL},                      // =, +
  {ACTION_SEND,                                    0x0b, 0, NULL},                      // k, K, VT
  {ACTION_NONE,                                    0,    0, NULL},                      // ., >
  {ACTION_SEND,                                    0x0f, 0, NULL},                      // o, O, SI
  {ACTION_NONE,                                    0,    0, NULL},                      // 9, (
  {ACTION_SEND,                                    0x0c, 0, NULL},                      // l, L, FF
  {ACTION_NONE,                                    0,    0, NULL},                      // /, ?
  {ACTION_SEND,                                    0x1b, 0, NULL},                      // [, {, ESC
  {ACTION_SEND,                                    0x10, 0, NULL},                      // p, P, DLE
  {ACTION_NONE,                                    0,    0, NULL},                      // 0, )
  {ACTION_SEND,                                    0x1f, 0, NULL},                      // -, _, US
  {ACTION_NONE,                                    0,    0, NULL},                      // ;, :
  {ACTION_NONE,                                    0,    0, NULL},                      // ', "
  {ACTION_NONE,                                    0,    0, NULL},                      // <left arrow>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_DownMicro},       // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_CRtn},            // <return>
  {ACTION_NONE,                                    0,    0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right margin>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_MARTABS},         // <tab>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_MarRel},          // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                      // <tab set>
  {ACTION_NONE,                                    0,    0, NULL}                       // *** not available on WW1000
};

// Half duplex uppercase key action table.
const struct key_action ASCII_ACTIONS_HALF_UPPERCASE[3 * NUM_WW_KEYS] = { 
  // Unshifted.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_RIGHTARROW},      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     '\\', 0, &ASCII_PRINT_BSLASH},       // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_DOWNARROW},       // <down arrow>
  {ACTION_SEND | ACTION_PRINT,                     'Z',  0, &WW_PRINT_Z},               // z, Z, SUB
  {ACTION_SEND | ACTION_PRINT,                     'Q',  0, &WW_PRINT_Q},               // q, Q, DC1/XON
  {ACTION_SEND | ACTION_PRINT,                     '1',  0, &WW_PRINT_1},               // 1, !
  {ACTION_SEND | ACTION_PRINT,                     '`',  0, &ASCII_PRINT_BAPOSTROPHE},  // `, ~
  {ACTION_SEND | ACTION_PRINT,                     'A',  0, &WW_PRINT_A},               // a, A, SOH
  {ACTION_SEND | ACTION_PRINT,                     ' ',  0, &WW_PRINT_SPACE},           // <space>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LOADPAPER},       // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LMar},             // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TClr},            // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_SEND | ACTION_PRINT,                     'X',  0, &WW_PRINT_X},               // x, X, CAN
  {ACTION_SEND | ACTION_PRINT,                     'W',  0, &WW_PRINT_W},               // w, W, ETB
  {ACTION_SEND | ACTION_PRINT,                     '2',  0, &WW_PRINT_2},               // 2, @, NUL
  {ACTION_SEND | ACTION_PRINT,                     'S',  0, &WW_PRINT_S},               // s, S, DC3/XOFF
  {ACTION_SEND | ACTION_PRINT,                     'C',  0, &WW_PRINT_C},               // c, C, ETX
  {ACTION_SEND | ACTION_PRINT,                     'E',  0, &WW_PRINT_E},               // e, E, ENQ
  {ACTION_SEND | ACTION_PRINT,                     '3',  0, &WW_PRINT_3},               // 3, #
  {ACTION_SEND | ACTION_PRINT,                     'D',  0, &WW_PRINT_D},               // d, D, EOT
  {ACTION_SEND | ACTION_PRINT,                     'B',  0, &WW_PRINT_B},               // b, B, STX
  {ACTION_SEND | ACTION_PRINT,                     'V',  0, &WW_PRINT_V},               // v, V, SYN
  {ACTION_SEND | ACTION_PRINT,                     'T',  0, &WW_PRINT_T},               // t, T, DC4
  {ACTION_SEND | ACTION_PRINT,                     'R',  0, &WW_PRINT_R},               // r, R, DC2
  {ACTION_SEND | ACTION_PRINT,                     '4',  0, &WW_PRINT_4},               // 4, $
  {ACTION_SEND | ACTION_PRINT,                     '5',  0, &WW_PRINT_5},               // 5, %
  {ACTION_SEND | ACTION_PRINT,                     'F',  0, &WW_PRINT_F},               // f, F, ACK
  {ACTION_SEND | ACTION_PRINT,                     'G',  0, &WW_PRINT_G},               // g, G, BEL
  {ACTION_SEND | ACTION_PRINT,                     'N',  0, &WW_PRINT_N},               // n, N, SO
  {ACTION_SEND | ACTION_PRINT,                     'M',  0, &WW_PRINT_M},               // m, M, CR
  {ACTION_SEND | ACTION_PRINT,                     'Y',  0, &WW_PRINT_Y},               // y, Y, EM
  {ACTION_SEND | ACTION_PRINT,                     'U',  0, &WW_PRINT_U},               // u, U, NAK
  {ACTION_SEND | ACTION_PRINT,                     '7',  0, &WW_PRINT_7},               // 7, &
  {ACTION_SEND | ACTION_PRINT,                     '6',  0, &WW_PRINT_6},               // 6, ^, RS
  {ACTION_SEND | ACTION_PRINT,                     'J',  0, &WW_PRINT_J},               // j, J, LF
  {ACTION_SEND | ACTION_PRINT,                     'H',  0, &WW_PRINT_H},               // h, H, BS
  {ACTION_SEND | ACTION_PRINT,                     ',',  0, &WW_PRINT_COMMA},           // ,, <
  {ACTION_SEND | ACTION_PRINT,                     ']',  0, &WW_PRINT_RBRACKET},        // ], }, GS
  {ACTION_SEND | ACTION_PRINT,                     'I',  0, &WW_PRINT_I},               // i, I, TAB
  {ACTION_SEND | ACTION_PRINT,                     '8',  0, &WW_PRINT_8},               // 8, *
  {ACTION_SEND | ACTION_PRINT,                     '=',  0, &WW_PRINT_EQUAL},           // =, +
  {ACTION_SEND | ACTION_PRINT,                     'K',  0, &WW_PRINT_K},               // k, K, VT
  {ACTION_SEND | ACTION_PRINT,                     '.',  0, &WW_PRINT_PERIOD},          // ., >
  {ACTION_SEND | ACTION_PRINT,                     'O',  0, &WW_PRINT_O},               // o, O, SI
  {ACTION_SEND | ACTION_PRINT,                     '9',  0, &WW_PRINT_9},               // 9, (
  {ACTION_SEND | ACTION_PRINT,                     'L',  0, &WW_PRINT_L},               // l, L, FF
  {ACTION_SEND | ACTION_PRINT,                     '/',  0, &WW_PRINT_SLASH},           // /, ?
  {ACTION_SEND | ACTION_PRINT,                     '[',  0, &WW_PRINT_LBRACKET},        // [, {, ESC
  {ACTION_SEND | ACTION_PRINT,                     'P',  0, &WW_PRINT_P},               // p, P, DLE
  {ACTION_SEND | ACTION_PRINT,                     '0',  0, &WW_PRINT_0},               // 0, )
  {ACTION_SEND | ACTION_PRINT,                     '-',  0, &WW_PRINT_HYPHEN},          // -, _, US
  {ACTION_SEND | ACTION_PRINT,                     ';',  0, &WW_PRINT_SEMICOLON},       // ;, :
  {ACTION_SEND | ACTION_PRINT,                     '\'', 0, &WW_PRINT_APOSTROPHE},      // ', "
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LEFTARROW},       // <left arrow>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_UPARROW},         // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     0x08, 0, &WW_PRINT_Backspace},       // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_ASCII_RETURN,                            0,    0, NULL},                      // <return>
  {ACTION_SEND,                                    0x7f, 0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_RMar},            // <right margin>
  {ACTION_SEND | ACTION_PRINT,                     0x09, 0, &WW_PRINT_Tab},             // <tab>
  {ACTION_SEND,                                    0x1b, 0, NULL},                      // <escape>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TSet},            // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_RIGHTARROW},      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     '|',  0, &ASCII_PRINT_BAR},          // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_DOWNARROW},       // <down arrow>
  {ACTION_SEND | ACTION_PRINT,                     'Z',  0, &WW_PRINT_Z},               // z, Z, SUB
  {ACTION_SEND | ACTION_PRINT,                     'Q',  0, &WW_PRINT_Q},               // q, Q, DC1/XON
  {ACTION_SEND | ACTION_PRINT,                     '!',  0, &WW_PRINT_EXCLAMATION},     // 1, !
  {ACTION_SEND | ACTION_PRINT,                     '~',  0, &ASCII_PRINT_TILDE},        // `, ~
  {ACTION_SEND | ACTION_PRINT,                     'A',  0, &WW_PRINT_A},               // a, A, SOH
  {ACTION_SEND | ACTION_PRINT,                     ' ',  0, &WW_PRINT_SPACE},           // <space>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LOADPAPER},       // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LMar},            // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TClr},            // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_SEND | ACTION_PRINT,                     'X',  0, &WW_PRINT_X},               // x, X, CAN
  {ACTION_SEND | ACTION_PRINT,                     'W',  0, &WW_PRINT_W},               // w, W, ETB
  {ACTION_SEND | ACTION_PRINT,                     '@',  0, &WW_PRINT_AT},              // 2, @, NUL
  {ACTION_SEND | ACTION_PRINT,                     'S',  0, &WW_PRINT_S},               // s, S, DC3/XOFF
  {ACTION_SEND | ACTION_PRINT,                     'C',  0, &WW_PRINT_C},               // c, C, ETX
  {ACTION_SEND | ACTION_PRINT,                     'E',  0, &WW_PRINT_E},               // e, E, ENQ
  {ACTION_SEND | ACTION_PRINT,                     '#',  0, &WW_PRINT_POUND},           // 3, #
  {ACTION_SEND | ACTION_PRINT,                     'D',  0, &WW_PRINT_D},               // d, D, EOT
  {ACTION_SEND | ACTION_PRINT,                     'B',  0, &WW_PRINT_B},               // b, B, STX
  {ACTION_SEND | ACTION_PRINT,                     'V',  0, &WW_PRINT_V},               // v, V, SYN
  {ACTION_SEND | ACTION_PRINT,                     'T',  0, &WW_PRINT_T},               // t, T, DC4
  {ACTION_SEND | ACTION_PRINT,                     'R',  0, &WW_PRINT_R},               // r, R, DC2
  {ACTION_SEND | ACTION_PRINT,                     '$',  0, &WW_PRINT_DOLLAR},          // 4, $
  {ACTION_SEND | ACTION_PRINT,                     '%',  0, &WW_PRINT_PERCENT},         // 5, %
  {ACTION_SEND | ACTION_PRINT,                     'F',  0, &WW_PRINT_F},               // f, F, ACK
  {ACTION_SEND | ACTION_PRINT,                     'G',  0, &WW_PRINT_G},               // g, G, BEL
  {ACTION_SEND | ACTION_PRINT,                     'N',  0, &WW_PRINT_N},               // n, N, SO
  {ACTION_SEND | ACTION_PRINT,                     'M',  0, &WW_PRINT_M},               // m, M, CR
  {ACTION_SEND | ACTION_PRINT,                     'Y',  0, &WW_PRINT_Y},               // y, Y, EM
  {ACTION_SEND | ACTION_PRINT,                     'U',  0, &WW_PRINT_U},               // u, U, NAK
  {ACTION_SEND | ACTION_PRINT,                     '&',  0, &WW_PRINT_AMPERSAND},       // 7, &
  {ACTION_SEND | ACTION_PRINT,                     '^',  0, &ASCII_PRINT_CARET},        // 6, ^, RS
  {ACTION_SEND | ACTION_PRINT,                     'J',  0, &WW_PRINT_J},               // j, J, LF
  {ACTION_SEND | ACTION_PRINT,                     'H',  0, &WW_PRINT_H},               // h, H, BS
  {ACTION_SEND | ACTION_PRINT,                     '<',  0, &ASCII_PRINT_LESS},         // ,, <
  {ACTION_SEND | ACTION_PRINT,                     '}',  0, &ASCII_PRINT_RBRACE},       // ], }, GS
  {ACTION_SEND | ACTION_PRINT,                     'I',  0, &WW_PRINT_I},               // i, I, TAB
  {ACTION_SEND | ACTION_PRINT,                     '*',  0, &WW_PRINT_ASTERISK},        // 8, *
  {ACTION_SEND | ACTION_PRINT,                     '+',  0, &WW_PRINT_PLUS},            // =, +
  {ACTION_SEND | ACTION_PRINT,                     'K',  0, &WW_PRINT_K},               // k, K, VT
  {ACTION_SEND | ACTION_PRINT,                     '>',  0, &ASCII_PRINT_GREATER},      // ., >
  {ACTION_SEND | ACTION_PRINT,                     'O',  0, &WW_PRINT_O},               // o, O, SI
  {ACTION_SEND | ACTION_PRINT,                     '(',  0, &WW_PRINT_LPAREN},          // 9, (
  {ACTION_SEND | ACTION_PRINT,                     'L',  0, &WW_PRINT_L},               // l, L, FF
  {ACTION_SEND | ACTION_PRINT,                     '?',  0, &WW_PRINT_QUESTION},        // /, ?
  {ACTION_SEND | ACTION_PRINT,                     '{',  0, &ASCII_PRINT_LBRACE},       // [, {, ESC
  {ACTION_SEND | ACTION_PRINT,                     'P',  0, &WW_PRINT_P},               // p, P, DLE
  {ACTION_SEND | ACTION_PRINT,                     ')',  0, &WW_PRINT_RPAREN},          // 0, )
  {ACTION_SEND | ACTION_PRINT,                     '_',  0, &WW_PRINT_UNDERSCORE},      // -, _, US
  {ACTION_SEND | ACTION_PRINT,                     ':',  0, &WW_PRINT_COLON},           // ;, :
  {ACTION_SEND | ACTION_PRINT,                     '"',  0, &WW_PRINT_QUOTE},           // ', "
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LEFTARROW},       // <left arrow>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_UPARROW},         // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_SEND | ACTION_PRINT,                     0x08, 0, &WW_PRINT_Backspace},       // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_ASCII_RETURN,                            0,    0, NULL},                      // <return>
  {ACTION_SEND,                                    0x7f, 0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_RMar},            // <right margin>
  {ACTION_SEND | ACTION_PRINT,                     0x09, 0, &WW_PRINT_Tab},             // <tab>
  {ACTION_SEND,                                    0x1b, 0, NULL},                      // <escape>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TSet},            // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000

  // Control.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_ASCII_SETUP,                             0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_UpMicro},         // <down arrow>
  {ACTION_SEND,                                    0x1a, 0, NULL},                      // z, Z, SUB
  {ACTION_SEND,                                    0x11, 0, NULL},                      // q, Q, DC1/XON
  {ACTION_NONE,                                    0,    0, NULL},                      // 1, !
  {ACTION_NONE,                                    0,    0, NULL},                      // `, ~
  {ACTION_SEND,                                    0x01, 0, NULL},                      // a, A, SOH
  {ACTION_NONE,                                    0,    0, NULL},                      // <space>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_SETTOPOFFORM},    // <set top of form>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TClrAll},         // <tab clear all>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_SEND,                                    0x18, 0, NULL},                      // x, X, CAN
  {ACTION_SEND,                                    0x17, 0, NULL},                      // w, W, ETB
  {ACTION_SEND,                                    0x00, 0, NULL},                      // 2, @, NUL
  {ACTION_SEND,                                    0x13, 0, NULL},                      // s, S, DC3/XOFF
  {ACTION_SEND,                                    0x03, 0, NULL},                      // c, C, ETX
  {ACTION_SEND,                                    0x05, 0, NULL},                      // e, E, ENQ
  {ACTION_NONE,                                    0,    0, NULL},                      // 3, #
  {ACTION_SEND,                                    0x04, 0, NULL},                      // d, D, EOT
  {ACTION_SEND,                                    0x02, 0, NULL},                      // b, B, STX
  {ACTION_SEND,                                    0x16, 0, NULL},                      // v, V, SYN
  {ACTION_SEND,                                    0x14, 0, NULL},                      // t, T, DC4
  {ACTION_SEND,                                    0x12, 0, NULL},                      // r, R, DC2
  {ACTION_NONE,                                    0,    0, NULL},                      // 4, $
  {ACTION_NONE,                                    0,    0, NULL},                      // 5, %
  {ACTION_SEND,                                    0x06, 0, NULL},                      // f, F, ACK
  {ACTION_SEND | ACTION_PRINT,                     0x07, 0, &WW_PRINT_BEEP},            // g, G, BEL
  {ACTION_SEND,                                    0x0e, 0, NULL},                      // n, N, SO
  {ACTION_SEND | ACTION_PRINT,                     0x0d, 0, &ASCII_PRINT_CR},           // m, M, CR
  {ACTION_SEND,                                    0x19, 0, NULL},                      // y, Y, EM
  {ACTION_SEND,                                    0x15, 0, NULL},                      // u, U, NAK
  {ACTION_NONE,                                    0,    0, NULL},                      // 7, &
  {ACTION_SEND,                                    0x1e, 0, NULL},                      // 6, ^, RS
  {ACTION_SEND | ACTION_PRINT,                     0x0a, 0, &ASCII_PRINT_LF},           // j, J, LF
  {ACTION_SEND | ACTION_PRINT,                     0x08, 0, &WW_PRINT_Backspace},       // h, H, BS
  {ACTION_NONE,                                    0,    0, NULL},                      // ,, <
  {ACTION_SEND,                                    0x1d, 0, NULL},                      // ], }, GS
  {ACTION_SEND | ACTION_PRINT,                     0x09, 0, &WW_PRINT_Tab},             // i, I, TAB
  {ACTION_NONE,                                    0,    0, NULL},                      // 8, *
  {ACTION_NONE,                                    0,    0, NULL},                      // =, +
  {ACTION_SEND,                                    0x0b, 0, NULL},                      // k, K, VT
  {ACTION_NONE,                                    0,    0, NULL},                      // ., >
  {ACTION_SEND,                                    0x0f, 0, NULL},                      // o, O, SI
  {ACTION_NONE,                                    0,    0, NULL},                      // 9, (
  {ACTION_SEND,                                    0x0c, 0, NULL},                      // l, L, FF
  {ACTION_NONE,                                    0,    0, NULL},                      // /, ?
  {ACTION_SEND,                                    0x1b, 0, NULL},                      // [, {, ESC
  {ACTION_SEND,                                    0x10, 0, NULL},                      // p, P, DLE
  {ACTION_NONE,                                    0,    0, NULL},                      // 0, )
  {ACTION_SEND,                                    0x1f, 0, NULL},                      // -, _, US
  {ACTION_NONE,                                    0,    0, NULL},                      // ;, :
  {ACTION_NONE,                                    0,    0, NULL},                      // ', "
  {ACTION_NONE,                                    0,    0, NULL},                      // <left arrow>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_DownMicro},       // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_CRtn},            // <return>
  {ACTION_NONE,                                    0,    0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right margin>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_MARTABS},         // <tab>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_MarRel},          // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                      // <tab set>
  {ACTION_NONE,                                    0,    0, NULL}                       // *** not available on WW1000
};

// Full duplex key action table.
const struct key_action ASCII_ACTIONS_FULL[3 * NUM_WW_KEYS] = { 
  // Unshifted.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_RIGHTARROW},      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_SEND,                                    '\\', 0, NULL},                      // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_DOWNARROW},       // <down arrow>
  {ACTION_SEND,                                    'z',  0, NULL},                      // z, Z, SUB
  {ACTION_SEND,                                    'q',  0, NULL},                      // q, Q, DC1/XON
  {ACTION_SEND,                                    '1',  0, NULL},                      // 1, !
  {ACTION_SEND,                                    '`',  0, NULL},                      // `, ~
  {ACTION_SEND,                                    'a',  0, NULL},                      // a, A, SOH
  {ACTION_SEND,                                    ' ',  0, NULL},                      // <space>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LOADPAPER},       // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LMar},            // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TClr},            // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_SEND,                                    'x',  0, NULL},                      // x, X, CAN
  {ACTION_SEND,                                    'w',  0, NULL},                      // w, W, ETB
  {ACTION_SEND,                                    '2',  0, NULL},                      // 2, @, NUL
  {ACTION_SEND,                                    's',  0, NULL},                      // s, S, DC3/XOFF
  {ACTION_SEND,                                    'c',  0, NULL},                      // c, C, ETX
  {ACTION_SEND,                                    'e',  0, NULL},                      // e, E, ENQ
  {ACTION_SEND,                                    '3',  0, NULL},                      // 3, #
  {ACTION_SEND,                                    'd',  0, NULL},                      // d, D, EOT
  {ACTION_SEND,                                    'b',  0, NULL},                      // b, B, STX
  {ACTION_SEND,                                    'v',  0, NULL},                      // v, V, SYN
  {ACTION_SEND,                                    't',  0, NULL},                      // t, T, DC4
  {ACTION_SEND,                                    'r',  0, NULL},                      // r, R, DC2
  {ACTION_SEND,                                    '4',  0, NULL},                      // 4, $
  {ACTION_SEND,                                    '5',  0, NULL},                      // 5, %
  {ACTION_SEND,                                    'f',  0, NULL},                      // f, F, ACK
  {ACTION_SEND,                                    'g',  0, NULL},                      // g, G, BEL
  {ACTION_SEND,                                    'n',  0, NULL},                      // n, N, SO
  {ACTION_SEND,                                    'm',  0, NULL},                      // m, M, CR
  {ACTION_SEND,                                    'y',  0, NULL},                      // y, Y, EM
  {ACTION_SEND,                                    'u',  0, NULL},                      // u, U, NAK
  {ACTION_SEND,                                    '7',  0, NULL},                      // 7, &
  {ACTION_SEND,                                    '6',  0, NULL},                      // 6, ^, RS
  {ACTION_SEND,                                    'j',  0, NULL},                      // j, J, LF
  {ACTION_SEND,                                    'h',  0, NULL},                      // h, H, BS
  {ACTION_SEND,                                    ',',  0, NULL},                      // ,, <
  {ACTION_SEND,                                    ']',  0, NULL},                      // ], }, GS
  {ACTION_SEND,                                    'i',  0, NULL},                      // i, I, TAB
  {ACTION_SEND,                                    '8',  0, NULL},                      // 8, *
  {ACTION_SEND,                                    '=',  0, NULL},                      // =, +
  {ACTION_SEND,                                    'k',  0, NULL},                      // k, K, VT
  {ACTION_SEND,                                    '.',  0, NULL},                      // ., >
  {ACTION_SEND,                                    'o',  0, NULL},                      // o, O, SI
  {ACTION_SEND,                                    '9',  0, NULL},                      // 9, (
  {ACTION_SEND,                                    'l',  0, NULL},                      // l, L, FF
  {ACTION_SEND,                                    '/',  0, NULL},                      // /, ?
  {ACTION_SEND,                                    '[',  0, NULL},                      // [, {, ESC
  {ACTION_SEND,                                    'p',  0, NULL},                      // p, P, DLE
  {ACTION_SEND,                                    '0',  0, NULL},                      // 0, )
  {ACTION_SEND,                                    '-',  0, NULL},                      // -, _, US
  {ACTION_SEND,                                    ';',  0, NULL},                      // ;, :
  {ACTION_SEND,                                    '\'', 0, NULL},                      // ', "
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LEFTARROW},       // <left arrow>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_UPARROW},         // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_SEND,                                    0x08, 0, NULL},                      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_ASCII_RETURN,                            0,    0, NULL},                      // <return>
  {ACTION_SEND,                                    0x7f, 0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_RMar},            // <right margin>
  {ACTION_SEND,                                    0x09, 0, NULL},                      // <tab>
  {ACTION_SEND,                                    0x1b, 0, NULL},                      // <escape>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TSet},            // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_RIGHTARROW},      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_SEND,                                    '|',  0, NULL},                      // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_DOWNARROW},       // <down arrow>
  {ACTION_SEND,                                    'Z',  0, NULL},                      // z, Z, SUB
  {ACTION_SEND,                                    'Q',  0, NULL},                      // q, Q, DC1/XON
  {ACTION_SEND,                                    '!',  0, NULL},                      // 1, !
  {ACTION_SEND,                                    '~',  0, NULL},                      // `, ~
  {ACTION_SEND,                                    'A',  0, NULL},                      // a, A, SOH
  {ACTION_SEND,                                    ' ',  0, NULL},                      // <space>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LOADPAPER},       // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LMar},            // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TClr},            // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_SEND,                                    'X',  0, NULL},                      // x, X, CAN
  {ACTION_SEND,                                    'W',  0, NULL},                      // w, W, ETB
  {ACTION_SEND,                                    '@',  0, NULL},                      // 2, @, NUL
  {ACTION_SEND,                                    'S',  0, NULL},                      // s, S, DC3/XOFF
  {ACTION_SEND,                                    'C',  0, NULL},                      // c, C, ETX
  {ACTION_SEND,                                    'E',  0, NULL},                      // e, E, ENQ
  {ACTION_SEND,                                    '#',  0, NULL},                      // 3, #
  {ACTION_SEND,                                    'D',  0, NULL},                      // d, D, EOT
  {ACTION_SEND,                                    'B',  0, NULL},                      // b, B, STX
  {ACTION_SEND,                                    'V',  0, NULL},                      // v, V, SYN
  {ACTION_SEND,                                    'T',  0, NULL},                      // t, T, DC4
  {ACTION_SEND,                                    'R',  0, NULL},                      // r, R, DC2
  {ACTION_SEND,                                    '$',  0, NULL},                      // 4, $
  {ACTION_SEND,                                    '%',  0, NULL},                      // 5, %
  {ACTION_SEND,                                    'F',  0, NULL},                      // f, F, ACK
  {ACTION_SEND,                                    'G',  0, NULL},                      // g, G, BEL
  {ACTION_SEND,                                    'N',  0, NULL},                      // n, N, SO
  {ACTION_SEND,                                    'M',  0, NULL},                      // m, M, CR
  {ACTION_SEND,                                    'Y',  0, NULL},                      // y, Y, EM
  {ACTION_SEND,                                    'U',  0, NULL},                      // u, U, NAK
  {ACTION_SEND,                                    '&',  0, NULL},                      // 7,0,   &
  {ACTION_SEND,                                    '^',  0, NULL},                      // 6, ^, RS
  {ACTION_SEND,                                    'J',  0, NULL},                      // j, J, LF
  {ACTION_SEND,                                    'H',  0, NULL},                      // h, H, BS
  {ACTION_SEND,                                    '<',  0, NULL},                      // ,, <
  {ACTION_SEND,                                    '}',  0, NULL},                      // ], }, GS
  {ACTION_SEND,                                    'I',  0, NULL},                      // i, I, TAB
  {ACTION_SEND,                                    '*',  0, NULL},                      // 8, *
  {ACTION_SEND,                                    '+',  0, NULL},                      // =, +
  {ACTION_SEND,                                    'K',  0, NULL},                      // k, K, VT
  {ACTION_SEND,                                    '>',  0, NULL},                      // ., >
  {ACTION_SEND,                                    'O',  0, NULL},                      // o, O, SI
  {ACTION_SEND,                                    '(',  0, NULL},                      // 9, (
  {ACTION_SEND,                                    'L',  0, NULL},                      // l, L, FF
  {ACTION_SEND,                                    '?',  0, NULL},                      // /, ?
  {ACTION_SEND,                                    '{',  0, NULL},                      // [, {, ESC
  {ACTION_SEND,                                    'P',  0, NULL},                      // p, P, DLE
  {ACTION_SEND,                                    ')',  0, NULL},                      // 0, )
  {ACTION_SEND,                                    '_',  0, NULL},                      // -, _, US
  {ACTION_SEND,                                    ':',  0, NULL},                      // ;, :
  {ACTION_SEND,                                    '"',  0, NULL},                      // ', "
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LEFTARROW},       // <left arrow>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_UPARROW},         // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_SEND,                                    0x08, 0, NULL},                      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_ASCII_RETURN,                            0,    0, NULL},                      // <return>
  {ACTION_SEND,                                    0x7f, 0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_RMar},            // <right margin>
  {ACTION_SEND,                                    0x09, 0, NULL},                      // <tab>
  {ACTION_SEND,                                    0x1b, 0, NULL},                      // <escape>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TSet},            // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000

  // Control.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_ASCII_SETUP,                             0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_UpMicro},         // <down arrow>
  {ACTION_SEND,                                    0x1a, 0, NULL},                      // z, Z, SUB
  {ACTION_SEND,                                    0x11, 0, NULL},                      // q, Q, DC1/XON
  {ACTION_NONE,                                    0,    0, NULL},                      // 1, !
  {ACTION_NONE,                                    0,    0, NULL},                      // `, ~
  {ACTION_SEND,                                    0x01, 0, NULL},                      // a, A, SOH
  {ACTION_NONE,                                    0,    0, NULL},                      // <space>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_SETTOPOFFORM},    // <set top of form>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TClrAll},         // <tab clear all>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_SEND,                                    0x18, 0, NULL},                      // x, X, CAN
  {ACTION_SEND,                                    0x17, 0, NULL},                      // w, W, ETB
  {ACTION_SEND,                                    0x00, 0, NULL},                      // 2, @, NUL
  {ACTION_SEND,                                    0x13, 0, NULL},                      // s, S, DC3/XOFF
  {ACTION_SEND,                                    0x03, 0, NULL},                      // c, C, ETX
  {ACTION_SEND,                                    0x05, 0, NULL},                      // e, E, ENQ
  {ACTION_NONE,                                    0,    0, NULL},                      // 3, #
  {ACTION_SEND,                                    0x04, 0, NULL},                      // d, D, EOT
  {ACTION_SEND,                                    0x02, 0, NULL},                      // b, B, STX
  {ACTION_SEND,                                    0x16, 0, NULL},                      // v, V, SYN
  {ACTION_SEND,                                    0x14, 0, NULL},                      // t, T, DC4
  {ACTION_SEND,                                    0x12, 0, NULL},                      // r, R, DC2
  {ACTION_NONE,                                    0,    0, NULL},                      // 4, $
  {ACTION_NONE,                                    0,    0, NULL},                      // 5, %
  {ACTION_SEND,                                    0x06, 0, NULL},                      // f, F, ACK
  {ACTION_SEND,                                    0x07, 0, NULL},                      // g, G, BEL
  {ACTION_SEND,                                    0x0e, 0, NULL},                      // n, N, SO
  {ACTION_SEND,                                    0x0d, 0, NULL},                      // m, M, CR
  {ACTION_SEND,                                    0x19, 0, NULL},                      // y, Y, EM
  {ACTION_SEND,                                    0x15, 0, NULL},                      // u, U, NAK
  {ACTION_NONE,                                    0,    0, NULL},                      // 7, &
  {ACTION_SEND,                                    0x1e, 0, NULL},                      // 6, ^, RS
  {ACTION_SEND,                                    0x0a, 0, NULL},                      // j, J, LF
  {ACTION_SEND,                                    0x08, 0, NULL},                      // h, H, BS
  {ACTION_NONE,                                    0,    0, NULL},                      // ,, <
  {ACTION_SEND,                                    0x1d, 0, NULL},                      // ], }, GS
  {ACTION_SEND,                                    0x09, 0, NULL},                      // i, I, TAB
  {ACTION_NONE,                                    0,    0, NULL},                      // 8, *
  {ACTION_NONE,                                    0,    0, NULL},                      // =, +
  {ACTION_SEND,                                    0x0b, 0, NULL},                      // k, K, VT
  {ACTION_NONE,                                    0,    0, NULL},                      // ., >
  {ACTION_SEND,                                    0x0f, 0, NULL},                      // o, O, SI
  {ACTION_NONE,                                    0,    0, NULL},                      // 9, (
  {ACTION_SEND,                                    0x0c, 0, NULL},                      // l, L, FF
  {ACTION_NONE,                                    0,    0, NULL},                      // /, ?
  {ACTION_SEND,                                    0x1b, 0, NULL},                      // [, {, ESC
  {ACTION_SEND,                                    0x10, 0, NULL},                      // p, P, DLE
  {ACTION_NONE,                                    0,    0, NULL},                      // 0, )
  {ACTION_SEND,                                    0x1f, 0, NULL},                      // -, _, US
  {ACTION_NONE,                                    0,    0, NULL},                      // ;, :
  {ACTION_NONE,                                    0,    0, NULL},                      // ', "
  {ACTION_NONE,                                    0,    0, NULL},                      // <left arrow>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_DownMicro},       // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_CRtn},            // <return>
  {ACTION_NONE,                                    0,    0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right margin>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_MARTABS},         // <tab>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_MarRel},          // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                      // <tab set>
  {ACTION_NONE,                                    0,    0, NULL}                       // *** not available on WW1000
};

// Full duplex uppercase key action table.
const struct key_action ASCII_ACTIONS_FULL_UPPERCASE[3 * NUM_WW_KEYS] = { 
  // Unshifted.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_RIGHTARROW},      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_SEND,                                    '\\', 0, NULL},                      // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_DOWNARROW},       // <down arrow>
  {ACTION_SEND,                                    'Z',  0, NULL},                      // z, Z, SUB
  {ACTION_SEND,                                    'Q',  0, NULL},                      // q, Q, DC1/XON
  {ACTION_SEND,                                    '1',  0, NULL},                      // 1, !
  {ACTION_SEND,                                    '`',  0, NULL},                      // `, ~
  {ACTION_SEND,                                    'A',  0, NULL},                      // a, A, SOH
  {ACTION_SEND,                                    ' ',  0, NULL},                      // <space>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LOADPAPER},       // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LMar},            // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TClr},            // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_SEND,                                    'X',  0, NULL},                      // x, X, CAN
  {ACTION_SEND,                                    'W',  0, NULL},                      // w, W, ETB
  {ACTION_SEND,                                    '2',  0, NULL},                      // 2, @, NUL
  {ACTION_SEND,                                    'S',  0, NULL},                      // s, S, DC3/XOFF
  {ACTION_SEND,                                    'C',  0, NULL},                      // c, C, ETX
  {ACTION_SEND,                                    'E',  0, NULL},                      // e, E, ENQ
  {ACTION_SEND,                                    '3',  0, NULL},                      // 3, #
  {ACTION_SEND,                                    'D',  0, NULL},                      // d, D, EOT
  {ACTION_SEND,                                    'B',  0, NULL},                      // b, B, STX
  {ACTION_SEND,                                    'V',  0, NULL},                      // v, V, SYN
  {ACTION_SEND,                                    'T',  0, NULL},                      // t, T, DC4
  {ACTION_SEND,                                    'R',  0, NULL},                      // r, R, DC2
  {ACTION_SEND,                                    '4',  0, NULL},                      // 4, $
  {ACTION_SEND,                                    '5',  0, NULL},                      // 5, %
  {ACTION_SEND,                                    'F',  0, NULL},                      // f, F, ACK
  {ACTION_SEND,                                    'G',  0, NULL},                      // g, G, BEL
  {ACTION_SEND,                                    'N',  0, NULL},                      // n, N, SO
  {ACTION_SEND,                                    'M',  0, NULL},                      // m, M, CR
  {ACTION_SEND,                                    'Y',  0, NULL},                      // y, Y, EM
  {ACTION_SEND,                                    'U',  0, NULL},                      // u, U, NAK
  {ACTION_SEND,                                    '7',  0, NULL},                      // 7, &
  {ACTION_SEND,                                    '6',  0, NULL},                      // 6, ^, RS
  {ACTION_SEND,                                    'J',  0, NULL},                      // j, J, LF
  {ACTION_SEND,                                    'H',  0, NULL},                      // h, H, BS
  {ACTION_SEND,                                    ',',  0, NULL},                      // ,, <
  {ACTION_SEND,                                    ']',  0, NULL},                      // ], }, GS
  {ACTION_SEND,                                    'I',  0, NULL},                      // i, I, TAB
  {ACTION_SEND,                                    '8',  0, NULL},                      // 8, *
  {ACTION_SEND,                                    '=',  0, NULL},                      // =, +
  {ACTION_SEND,                                    'K',  0, NULL},                      // k, K, VT
  {ACTION_SEND,                                    '.',  0, NULL},                      // ., >
  {ACTION_SEND,                                    'O',  0, NULL},                      // o, O, SI
  {ACTION_SEND,                                    '9',  0, NULL},                      // 9, (
  {ACTION_SEND,                                    'L',  0, NULL},                      // l, L, FF
  {ACTION_SEND,                                    '/',  0, NULL},                      // /, ?
  {ACTION_SEND,                                    '[',  0, NULL},                      // [, {, ESC
  {ACTION_SEND,                                    'P',  0, NULL},                      // p, P, DLE
  {ACTION_SEND,                                    '0',  0, NULL},                      // 0, )
  {ACTION_SEND,                                    '-',  0, NULL},                      // -, _, US
  {ACTION_SEND,                                    ';',  0, NULL},                      // ;, :
  {ACTION_SEND,                                    '\'', 0, NULL},                      // ', "
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LEFTARROW},       // <left arrow>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_UPARROW},         // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_SEND,                                    0x08, 0, NULL},                      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_ASCII_RETURN,                            0,    0, NULL},                      // <return>
  {ACTION_SEND,                                    0x7f, 0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_RMar},            // <right margin>
  {ACTION_SEND,                                    0x09, 0, NULL},                      // <tab>
  {ACTION_SEND,                                    0x1b, 0, NULL},                      // <escape>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TSet},            // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_RIGHTARROW},      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_SEND,                                    '|',  0, NULL},                      // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_DOWNARROW},       // <down arrow>
  {ACTION_SEND,                                    'Z',  0, NULL},                      // z, Z, SUB
  {ACTION_SEND,                                    'Q',  0, NULL},                      // q, Q, DC1/XON
  {ACTION_SEND,                                    '!',  0, NULL},                      // 1, !
  {ACTION_SEND,                                    '~',  0, NULL},                      // `, ~
  {ACTION_SEND,                                    'A',  0, NULL},                      // a, A, SOH
  {ACTION_SEND,                                    ' ',  0, NULL},                      // <space>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LOADPAPER},       // <load paper>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LMar},            // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TClr},            // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_SEND,                                    'X',  0, NULL},                      // x, X, CAN
  {ACTION_SEND,                                    'W',  0, NULL},                      // w, W, ETB
  {ACTION_SEND,                                    '@',  0, NULL},                      // 2, @, NUL
  {ACTION_SEND,                                    'S',  0, NULL},                      // s, S, DC3/XOFF
  {ACTION_SEND,                                    'C',  0, NULL},                      // c, C, ETX
  {ACTION_SEND,                                    'E',  0, NULL},                      // e, E, ENQ
  {ACTION_SEND,                                    '#',  0, NULL},                      // 3, #
  {ACTION_SEND,                                    'D',  0, NULL},                      // d, D, EOT
  {ACTION_SEND,                                    'B',  0, NULL},                      // b, B, STX
  {ACTION_SEND,                                    'V',  0, NULL},                      // v, V, SYN
  {ACTION_SEND,                                    'T',  0, NULL},                      // t, T, DC4
  {ACTION_SEND,                                    'R',  0, NULL},                      // r, R, DC2
  {ACTION_SEND,                                    '$',  0, NULL},                      // 4, $
  {ACTION_SEND,                                    '%',  0, NULL},                      // 5, %
  {ACTION_SEND,                                    'F',  0, NULL},                      // f, F, ACK
  {ACTION_SEND,                                    'G',  0, NULL},                      // g, G, BEL
  {ACTION_SEND,                                    'N',  0, NULL},                      // n, N, SO
  {ACTION_SEND,                                    'M',  0, NULL},                      // m, M, CR
  {ACTION_SEND,                                    'Y',  0, NULL},                      // y, Y, EM
  {ACTION_SEND,                                    'U',  0, NULL},                      // u, U, NAK
  {ACTION_SEND,                                    '&',  0, NULL},                      // 7, &
  {ACTION_SEND,                                    '^',  0, NULL},                      // 6, ^, RS
  {ACTION_SEND,                                    'J',  0, NULL},                      // j, J, LF
  {ACTION_SEND,                                    'H',  0, NULL},                      // h, H, BS
  {ACTION_SEND,                                    '<',  0, NULL},                      // ,, <
  {ACTION_SEND,                                    '}',  0, NULL},                      // ], }, GS
  {ACTION_SEND,                                    'I',  0, NULL},                      // i, I, TAB
  {ACTION_SEND,                                    '*',  0, NULL},                      // 8, *
  {ACTION_SEND,                                    '+',  0, NULL},                      // =, +
  {ACTION_SEND,                                    'K',  0, NULL},                      // k, K, VT
  {ACTION_SEND,                                    '>',  0, NULL},                      // ., >
  {ACTION_SEND,                                    'O',  0, NULL},                      // o, O, SI
  {ACTION_SEND,                                    '(',  0, NULL},                      // 9, (
  {ACTION_SEND,                                    'L',  0, NULL},                      // l, L, FF
  {ACTION_SEND,                                    '?',  0, NULL},                      // /, ?
  {ACTION_SEND,                                    '{',  0, NULL},                      // [, {, ESC
  {ACTION_SEND,                                    'P',  0, NULL},                      // p, P, DLE
  {ACTION_SEND,                                    ')',  0, NULL},                      // 0, )
  {ACTION_SEND,                                    '_',  0, NULL},                      // -, _, US
  {ACTION_SEND,                                    ':',  0, NULL},                      // ;, :
  {ACTION_SEND,                                    '"',  0, NULL},                      // ', "
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_LEFTARROW},       // <left arrow>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_UPARROW},         // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_SEND,                                    0x08, 0, NULL},                      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_ASCII_RETURN,                            0,    0, NULL},                      // <return>
  {ACTION_SEND,                                    0x7f, 0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_RMar},            // <right margin>
  {ACTION_SEND,                                    0x09, 0, NULL},                      // <tab>
  {ACTION_SEND,                                    0x1b, 0, NULL},                      // <escape>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TSet},            // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000

  // Control.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_ASCII_SETUP,                             0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_UpMicro},         // <down arrow>
  {ACTION_SEND,                                    0x1a, 0, NULL},                      // z, Z, SUB
  {ACTION_SEND,                                    0x11, 0, NULL},                      // q, Q, DC1/XON
  {ACTION_NONE,                                    0,    0, NULL},                      // 1, !
  {ACTION_NONE,                                    0,    0, NULL},                      // `, ~
  {ACTION_SEND,                                    0x01, 0, NULL},                      // a, A, SOH
  {ACTION_NONE,                                    0,    0, NULL},                      // <space>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_SETTOPOFFORM},    // <set top of form>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_TClrAll},         // <tab clear all>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_SEND,                                    0x18, 0, NULL},                      // x, X, CAN
  {ACTION_SEND,                                    0x17, 0, NULL},                      // w, W, ETB
  {ACTION_SEND,                                    0x00, 0, NULL},                      // 2, @, NUL
  {ACTION_SEND,                                    0x13, 0, NULL},                      // s, S, DC3/XOFF
  {ACTION_SEND,                                    0x03, 0, NULL},                      // c, C, ETX
  {ACTION_SEND,                                    0x05, 0, NULL},                      // e, E, ENQ
  {ACTION_NONE,                                    0,    0, NULL},                      // 3, #
  {ACTION_SEND,                                    0x04, 0, NULL},                      // d, D, EOT
  {ACTION_SEND,                                    0x02, 0, NULL},                      // b, B, STX
  {ACTION_SEND,                                    0x16, 0, NULL},                      // v, V, SYN
  {ACTION_SEND,                                    0x14, 0, NULL},                      // t, T, DC4
  {ACTION_SEND,                                    0x12, 0, NULL},                      // r, R, DC2
  {ACTION_NONE,                                    0,    0, NULL},                      // 4, $
  {ACTION_NONE,                                    0,    0, NULL},                      // 5, %
  {ACTION_SEND,                                    0x06, 0, NULL},                      // f, F, ACK
  {ACTION_SEND,                                    0x07, 0, NULL},                      // g, G, BEL
  {ACTION_SEND,                                    0x0e, 0, NULL},                      // n, N, SO
  {ACTION_SEND,                                    0x0d, 0, NULL},                      // m, M, CR
  {ACTION_SEND,                                    0x19, 0, NULL},                      // y, Y, EM
  {ACTION_SEND,                                    0x15, 0, NULL},                      // u, U, NAK
  {ACTION_NONE,                                    0,    0, NULL},                      // 7,0,   &
  {ACTION_SEND,                                    0x1e, 0, NULL},                      // 6, ^, RS
  {ACTION_SEND,                                    0x0a, 0, NULL},                      // j, J, LF
  {ACTION_SEND,                                    0x08, 0, NULL},                      // h, H, BS
  {ACTION_NONE,                                    0,    0, NULL},                      // ,, <
  {ACTION_SEND,                                    0x1d, 0, NULL},                      // ], }, GS
  {ACTION_SEND,                                    0x09, 0, NULL},                      // i, I, TAB
  {ACTION_NONE,                                    0,    0, NULL},                      // 8, *
  {ACTION_NONE,                                    0,    0, NULL},                      // =, +
  {ACTION_SEND,                                    0x0b, 0, NULL},                      // k, K, VT
  {ACTION_NONE,                                    0,    0, NULL},                      // ., >
  {ACTION_SEND,                                    0x0f, 0, NULL},                      // o, O, SI
  {ACTION_NONE,                                    0,    0, NULL},                      // 9, (
  {ACTION_SEND,                                    0x0c, 0, NULL},                      // l, L, FF
  {ACTION_NONE,                                    0,    0, NULL},                      // /, ?
  {ACTION_SEND,                                    0x1b, 0, NULL},                      // [, {, ESC
  {ACTION_SEND,                                    0x10, 0, NULL},                      // p, P, DLE
  {ACTION_NONE,                                    0,    0, NULL},                      // 0, )
  {ACTION_SEND,                                    0x1f, 0, NULL},                      // -, _, US
  {ACTION_NONE,                                    0,    0, NULL},                      // ;, :
  {ACTION_NONE,                                    0,    0, NULL},                      // ', "
  {ACTION_NONE,                                    0,    0, NULL},                      // <left arrow>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_DownMicro},       // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_CRtn},            // <return>
  {ACTION_NONE,                                    0,    0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right margin>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_MARTABS},         // <tab>
  {ACTION_PRINT,                                   0,    0, &WW_PRINT_MarRel},          // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                      // <tab set>
  {ACTION_NONE,                                    0,    0, NULL}                       // *** not available on WW1000
};

// Setup key action table.
const struct key_action ASCII_ACTIONS_SETUP[3 * NUM_WW_KEYS] = {
  // Unshifted.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_COMMAND,                                 0x81, 0, NULL},                      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_COMMAND,                                 '\\', 0, NULL},                      // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_COMMAND,                                 0x91, 0, NULL},                      // <down arrow>
  {ACTION_COMMAND,                                 'z',  0, NULL},                      // z, Z, SUB
  {ACTION_COMMAND,                                 'q',  0, NULL},                      // q, Q, DC1/XON
  {ACTION_COMMAND,                                 '1',  0, NULL},                      // 1, !
  {ACTION_COMMAND,                                 '`',  0, NULL},                      // `, ~
  {ACTION_COMMAND,                                 'a',  0, NULL},                      // a, A, SOH
  {ACTION_COMMAND,                                 ' ',  0, NULL},                      // <space>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_COMMAND,                                 0x85, 0, NULL},                      // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_COMMAND,                                 0x88, 0, NULL},                      // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_COMMAND,                                 'x',  0, NULL},                      // x, X, CAN
  {ACTION_COMMAND,                                 'w',  0, NULL},                      // w, W, ETB
  {ACTION_COMMAND,                                 '2',  0, NULL},                      // 2, @, NUL
  {ACTION_COMMAND,                                 's',  0, NULL},                      // s, S, DC3/XOFF
  {ACTION_COMMAND,                                 'c',  0, NULL},                      // c, C, ETX
  {ACTION_COMMAND,                                 'e',  0, NULL},                      // e, E, ENQ
  {ACTION_COMMAND,                                 '3',  0, NULL},                      // 3, #
  {ACTION_COMMAND,                                 'd',  0, NULL},                      // d, D, EOT
  {ACTION_COMMAND,                                 'b',  0, NULL},                      // b, B, STX
  {ACTION_COMMAND,                                 'v',  0, NULL},                      // v, V, SYN
  {ACTION_COMMAND,                                 't',  0, NULL},                      // t, T, DC4
  {ACTION_COMMAND,                                 'r',  0, NULL},                      // r, R, DC2
  {ACTION_COMMAND,                                 '4',  0, NULL},                      // 4, $
  {ACTION_COMMAND,                                 '5',  0, NULL},                      // 5, %
  {ACTION_COMMAND,                                 'f',  0, NULL},                      // f, F, ACK
  {ACTION_COMMAND,                                 'g',  0, NULL},                      // g, G, BEL
  {ACTION_COMMAND,                                 'n',  0, NULL},                      // n, N, SO
  {ACTION_COMMAND,                                 'm',  0, NULL},                      // m, M, CR
  {ACTION_COMMAND,                                 'y',  0, NULL},                      // y, Y, EM
  {ACTION_COMMAND,                                 'u',  0, NULL},                      // u, U, NAK
  {ACTION_COMMAND,                                 '7',  0, NULL},                      // 7,0,   &
  {ACTION_COMMAND,                                 '6',  0, NULL},                      // 6, ^, RS
  {ACTION_COMMAND,                                 'j',  0, NULL},                      // j, J, LF
  {ACTION_COMMAND,                                 'h',  0, NULL},                      // h, H, BS
  {ACTION_COMMAND,                                 ',',  0, NULL},                      // ,, <
  {ACTION_COMMAND,                                 ']',  0, NULL},                      // ], }, GS
  {ACTION_COMMAND,                                 'i',  0, NULL},                      // i, I, TAB
  {ACTION_COMMAND,                                 '8',  0, NULL},                      // 8, *
  {ACTION_COMMAND,                                 '=',  0, NULL},                      // =, +
  {ACTION_COMMAND,                                 'k',  0, NULL},                      // k, K, VT
  {ACTION_COMMAND,                                 '.',  0, NULL},                      // ., >
  {ACTION_COMMAND,                                 'o',  0, NULL},                      // o, O, SI
  {ACTION_COMMAND,                                 '9',  0, NULL},                      // 9, (
  {ACTION_COMMAND,                                 'l',  0, NULL},                      // l, L, FF
  {ACTION_COMMAND,                                 '/',  0, NULL},                      // /, ?
  {ACTION_COMMAND,                                 '[',  0, NULL},                      // [, {, ESC
  {ACTION_COMMAND,                                 'p',  0, NULL},                      // p, P, DLE
  {ACTION_COMMAND,                                 '0',  0, NULL},                      // 0, )
  {ACTION_COMMAND,                                 '-',  0, NULL},                      // -, _, US
  {ACTION_COMMAND,                                 ';',  0, NULL},                      // ;, :
  {ACTION_COMMAND,                                 '\'', 0, NULL},                      // ', "
  {ACTION_COMMAND,                                 0x80, 0, NULL},                      // <left arrow>
  {ACTION_COMMAND,                                 0x90, 0, NULL},                      // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_COMMAND,                                 0x0d, 0, NULL},                      // <return>
  {ACTION_NONE,                                    0,    0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_COMMAND,                                 0x86, 0, NULL},                      // <right margin>
  {ACTION_COMMAND,                                 0x82, 0, NULL},                      // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                      // <escape>
  {ACTION_COMMAND,                                 0x87, 0, NULL},                      // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_COMMAND,                                 0x81, 0, NULL},                      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_COMMAND,                                 '|',  0, NULL},                      // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_COMMAND,                                 0x91, 0, NULL},                      // <down arrow>
  {ACTION_COMMAND,                                 'Z',  0, NULL},                      // z, Z, SUB
  {ACTION_COMMAND,                                 'Q',  0, NULL},                      // q, Q, DC1/XON
  {ACTION_COMMAND,                                 '!',  0, NULL},                      // 1, !
  {ACTION_COMMAND,                                 '~',  0, NULL},                      // `, ~
  {ACTION_COMMAND,                                 'A',  0, NULL},                      // a, A, SOH
  {ACTION_COMMAND,                                 ' ',  0, NULL},                      // <space>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_COMMAND,                                 0x85, 0, NULL},                      // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_COMMAND,                                 0x88, 0, NULL},                      // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_COMMAND,                                 'X',  0, NULL},                      // x, X, CAN
  {ACTION_COMMAND,                                 'W',  0, NULL},                      // w, W, ETB
  {ACTION_COMMAND,                                 '@',  0, NULL},                      // 2, @, NUL
  {ACTION_COMMAND,                                 'S',  0, NULL},                      // s, S, DC3/XOFF
  {ACTION_COMMAND,                                 'C',  0, NULL},                      // c, C, ETX
  {ACTION_COMMAND,                                 'E',  0, NULL},                      // e, E, ENQ
  {ACTION_COMMAND,                                 '#',  0, NULL},                      // 3, #
  {ACTION_COMMAND,                                 'D',  0, NULL},                      // d, D, EOT
  {ACTION_COMMAND,                                 'B',  0, NULL},                      // b, B, STX
  {ACTION_COMMAND,                                 'V',  0, NULL},                      // v, V, SYN
  {ACTION_COMMAND,                                 'T',  0, NULL},                      // t, T, DC4
  {ACTION_COMMAND,                                 'R',  0, NULL},                      // r, R, DC2
  {ACTION_COMMAND,                                 '$',  0, NULL},                      // 4, $
  {ACTION_COMMAND,                                 '%',  0, NULL},                      // 5, %
  {ACTION_COMMAND,                                 'F',  0, NULL},                      // f, F, ACK
  {ACTION_COMMAND,                                 'G',  0, NULL},                      // g, G, BEL
  {ACTION_COMMAND,                                 'N',  0, NULL},                      // n, N, SO
  {ACTION_COMMAND,                                 'M',  0, NULL},                      // m, M, CR
  {ACTION_COMMAND,                                 'Y',  0, NULL},                      // y, Y, EM
  {ACTION_COMMAND,                                 'U',  0, NULL},                      // u, U, NAK
  {ACTION_COMMAND,                                 '&',  0, NULL},                      // 7,0,   &
  {ACTION_COMMAND,                                 '^',  0, NULL},                      // 6, ^, RS
  {ACTION_COMMAND,                                 'J',  0, NULL},                      // j, J, LF
  {ACTION_COMMAND,                                 'H',  0, NULL},                      // h, H, BS
  {ACTION_COMMAND,                                 '<',  0, NULL},                      // ,, <
  {ACTION_COMMAND,                                 '}',  0, NULL},                      // ], }, GS
  {ACTION_COMMAND,                                 'I',  0, NULL},                      // i, I, TAB
  {ACTION_COMMAND,                                 '*',  0, NULL},                      // 8, *
  {ACTION_COMMAND,                                 '+',  0, NULL},                      // =, +
  {ACTION_COMMAND,                                 'K',  0, NULL},                      // k, K, VT
  {ACTION_COMMAND,                                 '>',  0, NULL},                      // ., >
  {ACTION_COMMAND,                                 'O',  0, NULL},                      // o, O, SI
  {ACTION_COMMAND,                                 '(',  0, NULL},                      // 9, (
  {ACTION_COMMAND,                                 'L',  0, NULL},                      // l, L, FF
  {ACTION_COMMAND,                                 '?',  0, NULL},                      // /, ?
  {ACTION_COMMAND,                                 '{',  0, NULL},                      // [, {, ESC
  {ACTION_COMMAND,                                 'P',  0, NULL},                      // p, P, DLE
  {ACTION_COMMAND,                                 ')',  0, NULL},                      // 0, )
  {ACTION_COMMAND,                                 '_',  0, NULL},                      // -, _, US
  {ACTION_COMMAND,                                 ':',  0, NULL},                      // ;, :
  {ACTION_COMMAND,                                 '"',  0, NULL},                      // ', "
  {ACTION_COMMAND,                                 0x80, 0, NULL},                      // <left arrow>
  {ACTION_COMMAND,                                 0x90, 0, NULL},                      // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_COMMAND,                                 0x0d, 0, NULL},                      // <return>
  {ACTION_NONE,                                    0,    0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_COMMAND,                                 0x86, 0, NULL},                      // <right margin>
  {ACTION_COMMAND,                                 0x82, 0, NULL},                      // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                      // <escape>
  {ACTION_COMMAND,                                 0x87, 0, NULL},                      // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000

  // Control.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_NONE,                                    0,    0, NULL},                      // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // z, Z, SUB
  {ACTION_NONE,                                    0,    0, NULL},                      // q, Q, DC1/XON
  {ACTION_NONE,                                    0,    0, NULL},                      // 1, !
  {ACTION_NONE,                                    0,    0, NULL},                      // `, ~
  {ACTION_NONE,                                    0,    0, NULL},                      // a, A, SOH
  {ACTION_NONE,                                    0,    0, NULL},                      // <space>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_COMMAND,                                 0x89, 0, NULL},                      // <tab clear all>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_NONE,                                    0,    0, NULL},                      // x, X, CAN
  {ACTION_NONE,                                    0,    0, NULL},                      // w, W, ETB
  {ACTION_NONE,                                    0,    0, NULL},                      // 2, @, NUL
  {ACTION_NONE,                                    0,    0, NULL},                      // s, S, DC3/XOFF
  {ACTION_NONE,                                    0,    0, NULL},                      // c, C, ETX
  {ACTION_NONE,                                    0,    0, NULL},                      // e, E, ENQ
  {ACTION_NONE,                                    0,    0, NULL},                      // 3, #
  {ACTION_COMMAND,                                 0x04, 0, NULL},                      // d, D, EOT
  {ACTION_NONE,                                    0,    0, NULL},                      // b, B, STX
  {ACTION_NONE,                                    0,    0, NULL},                      // v, V, SYN
  {ACTION_NONE,                                    0,    0, NULL},                      // t, T, DC4
  {ACTION_NONE,                                    0,    0, NULL},                      // r, R, DC2
  {ACTION_NONE,                                    0,    0, NULL},                      // 4, $
  {ACTION_NONE,                                    0,    0, NULL},                      // 5, %
  {ACTION_NONE,                                    0,    0, NULL},                      // f, F, ACK
  {ACTION_NONE,                                    0,    0, NULL},                      // g, G, BEL
  {ACTION_NONE,                                    0,    0, NULL},                      // n, N, SO
  {ACTION_NONE,                                    0,    0, NULL},                      // m, M, CR
  {ACTION_NONE,                                    0,    0, NULL},                      // y, Y, EM
  {ACTION_NONE,                                    0,    0, NULL},                      // u, U, NAK
  {ACTION_NONE,                                    0,    0, NULL},                      // 7, &
  {ACTION_NONE,                                    0,    0, NULL},                      // 6, ^, RS
  {ACTION_NONE,                                    0,    0, NULL},                      // j, J, LF
  {ACTION_NONE,                                    0,    0, NULL},                      // h, H, BS
  {ACTION_NONE,                                    0,    0, NULL},                      // ,, <
  {ACTION_NONE,                                    0,    0, NULL},                      // ], }, GS
  {ACTION_NONE,                                    0,    0, NULL},                      // i, I, TAB
  {ACTION_NONE,                                    0,    0, NULL},                      // 8, *
  {ACTION_NONE,                                    0,    0, NULL},                      // =, +
  {ACTION_NONE,                                    0,    0, NULL},                      // k, K, VT
  {ACTION_NONE,                                    0,    0, NULL},                      // ., >
  {ACTION_NONE,                                    0,    0, NULL},                      // o, O, SI
  {ACTION_NONE,                                    0,    0, NULL},                      // 9, (
  {ACTION_NONE,                                    0,    0, NULL},                      // l, L, FF
  {ACTION_NONE,                                    0,    0, NULL},                      // /, ?
  {ACTION_NONE,                                    0,    0, NULL},                      // [, {, ESC
  {ACTION_NONE,                                    0,    0, NULL},                      // p, P, DLE
  {ACTION_NONE,                                    0,    0, NULL},                      // 0, )
  {ACTION_NONE,                                    0,    0, NULL},                      // -, _, US
  {ACTION_NONE,                                    0,    0, NULL},                      // ;, :
  {ACTION_NONE,                                    0,    0, NULL},                      // ', "
  {ACTION_NONE,                                    0,    0, NULL},                      // <left arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_COMMAND,                                 0x83, 0, NULL},                      // <return>
  {ACTION_NONE,                                    0,    0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // <tab>
  {ACTION_COMMAND,                                 0x84, 0, NULL},                      // <margin release>
  {ACTION_NONE,                                    0,    0, NULL},                      // <tab set>
  {ACTION_NONE,                                    0,    0, NULL}                       // *** not available on WW1000
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

void Setup_ASCII (void) {

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

  // Initialize dynamic print elements.
  if (asciiwheel == SETTING_TRUE) {
    ascii_print_less = &ASCII_PRINT_LESS_APW;
    ascii_print_greater = &ASCII_PRINT_GREATER_APW;
    ascii_print_bslash = &ASCII_PRINT_BSLASH_APW;
    ascii_print_caret = &ASCII_PRINT_CARET_APW;
    ascii_print_bapostrophe = &ASCII_PRINT_BAPOSTROPHE_APW;
    ascii_print_lbrace = &ASCII_PRINT_LBRACE_APW;
    ascii_print_bar = &ASCII_PRINT_BAR_APW;
    ascii_print_rbrace = &ASCII_PRINT_RBRACE_APW;
    ascii_print_tilde = &ASCII_PRINT_TILDE_APW;
  } else {
    ascii_print_less = &ASCII_PRINT_LESS_PG;
    ascii_print_greater = &ASCII_PRINT_GREATER_PG;
    ascii_print_bslash = &ASCII_PRINT_BSLASH_PG;
    ascii_print_caret = &ASCII_PRINT_CARET_PG;
    ascii_print_bapostrophe = &ASCII_PRINT_BAPOSTROPHE_PG;
    ascii_print_lbrace = &ASCII_PRINT_LBRACE_PG;
    ascii_print_bar = &ASCII_PRINT_BAR_PG;
    ascii_print_rbrace = &ASCII_PRINT_RBRACE_PG;
    ascii_print_tilde = &ASCII_PRINT_TILDE_PG;
  }
}


//**********************************************************************************************************************
//
//  ASCII Terminal support routines.
//
//**********************************************************************************************************************

// Print ASCII Terminal setup title.
void Print_ASCII_setup_title (void) {
  (void)Print_string ("\r\r---- Cadetwriter: " ASCII_VERSION " Setup\r\r");
}

// Update ASCII Terminal settings.
void Update_ASCII_settings (void) {
  byte obattery = battery;
  byte oserial = serial;
  byte oparity = parity;
  byte obaud = baud;
  byte odps = dps;
  byte oimpression = impression;
  byte olength = length;
  byte ooffset = offset;
  byte ohwflow = hwflow;

  // Query new settings.
  (void)Print_element (&WW_PRINT_CRtn);
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
  impression = Read_impression_setting ("impression", impression);
  Change_impression (oimpression, impression);
  swflow = Read_swflow_setting ("sw flow control", swflow);
  if (serial == SERIAL_USB) {
    hwflow = HWFLOW_NONE;
  } else /* serial == SERIAL_RS232 */ {
    hwflow = Read_hwflow_setting ("hw flow control", hwflow);
  }
  asciiwheel = Read_truefalse_setting ("ASCII printwheel", asciiwheel);
  if (asciiwheel == SETTING_TRUE) {
    ascii_print_less = &ASCII_PRINT_LESS_APW;
    ascii_print_greater = &ASCII_PRINT_GREATER_APW;
    ascii_print_bslash = &ASCII_PRINT_BSLASH_APW;
    ascii_print_caret = &ASCII_PRINT_CARET_APW;
    ascii_print_bapostrophe = &ASCII_PRINT_BAPOSTROPHE_APW;
    ascii_print_lbrace = &ASCII_PRINT_LBRACE_APW;
    ascii_print_bar = &ASCII_PRINT_BAR_APW;
    ascii_print_rbrace = &ASCII_PRINT_RBRACE_APW;
    ascii_print_tilde = &ASCII_PRINT_TILDE_APW;
  } else {
    ascii_print_less = &ASCII_PRINT_LESS_PG;
    ascii_print_greater = &ASCII_PRINT_GREATER_PG;
    ascii_print_bslash = &ASCII_PRINT_BSLASH_PG;
    ascii_print_caret = &ASCII_PRINT_CARET_PG;
    ascii_print_bapostrophe = &ASCII_PRINT_BAPOSTROPHE_PG;
    ascii_print_lbrace = &ASCII_PRINT_LBRACE_PG;
    ascii_print_bar = &ASCII_PRINT_BAR_PG;
    ascii_print_rbrace = &ASCII_PRINT_RBRACE_PG;
    ascii_print_tilde = &ASCII_PRINT_TILDE_PG;
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
  escapesequence = Read_escapesequence_setting ("escape sequences", escapesequence);
  length = Read_integer_setting ("line length", length, LENGTH_MINIMUM, LENGTH_MAXIMUM);
  offset = Read_integer_setting ("line offset", offset, 1, 10);
  (void)Print_element (&WW_PRINT_CRtn);

  // Reset right margin if length changed.
  if (olength != length) {
    rmargin = length;
    Write_EEPROM (EEPROM_RMARGIN, rmargin);
  }

  // Save settings in EEPROM if requested.
  if (Ask_yesno_question ("Save settings", FALSE)) {
    Write_EEPROM (EEPROM_ERRORS, errors);
    Write_EEPROM (EEPROM_WARNINGS, warnings);
    Write_EEPROM (EEPROM_BATTERY, battery);
    Write_EEPROM (EEPROM_OFFSET, offset);
    Write_EEPROM (EEPROM_ASCIIWHEEL, asciiwheel);
    Write_EEPROM (EEPROM_SERIAL, serial);
    Write_EEPROM (EEPROM_DUPLEX, duplex);
    Write_EEPROM (EEPROM_BAUD, baud);
    Write_EEPROM (EEPROM_PARITY, parity);
    Write_EEPROM (EEPROM_DPS, dps);
    Write_EEPROM (EEPROM_IMPRESSION, impression);
    Write_EEPROM (EEPROM_SWFLOW, swflow);
    Write_EEPROM (EEPROM_HWFLOW, hwflow);
    Write_EEPROM (EEPROM_UPPERCASE, uppercase);
    Write_EEPROM (EEPROM_AUTORETURN, autoreturn);
    Write_EEPROM (EEPROM_TRANSMITEOL, transmiteol);
    Write_EEPROM (EEPROM_RECEIVEEOL, receiveeol);
    Write_EEPROM (EEPROM_ESCAPESEQUENCE, escapesequence);
    Write_EEPROM (EEPROM_LENGTH, length);
  }
  (void)Print_element (&WW_PRINT_CRtn);

  // Reset communications if any communication changes.
  if ((oserial != serial) || (oparity != parity) || (obaud != baud) || (odps != dps) || (ohwflow != hwflow)) {
    flow_in_on = TRUE;
    flow_out_on = TRUE;
    Serial_end (oserial);
    Serial_begin ();
  }
 
  // Reset margins and tabs if battery, length, or offset changed.
  if ((obattery != battery) || (olength != length) || (ooffset != offset)) {
    Set_margins_tabs (TRUE);
  }
}

// Print ASCII Terminal character set.
void Print_ASCII_character_set (void) {
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_element (&WW_PRINT_SPACE);  (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&WW_PRINT_A);  (void)Print_element (&WW_PRINT_B);  (void)Print_element (&WW_PRINT_C);
  (void)Print_element (&WW_PRINT_D);  (void)Print_element (&WW_PRINT_E);  (void)Print_element (&WW_PRINT_F);
  (void)Print_element (&WW_PRINT_G);  (void)Print_element (&WW_PRINT_H);  (void)Print_element (&WW_PRINT_I);
  (void)Print_element (&WW_PRINT_J);  (void)Print_element (&WW_PRINT_K);  (void)Print_element (&WW_PRINT_L);
  (void)Print_element (&WW_PRINT_M);  (void)Print_element (&WW_PRINT_N);  (void)Print_element (&WW_PRINT_O);
  (void)Print_element (&WW_PRINT_P);  (void)Print_element (&WW_PRINT_Q);  (void)Print_element (&WW_PRINT_R);
  (void)Print_element (&WW_PRINT_S);  (void)Print_element (&WW_PRINT_T);  (void)Print_element (&WW_PRINT_U);
  (void)Print_element (&WW_PRINT_V);  (void)Print_element (&WW_PRINT_W);  (void)Print_element (&WW_PRINT_X);
  (void)Print_element (&WW_PRINT_Y);  (void)Print_element (&WW_PRINT_Z);
  (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&WW_PRINT_a);  (void)Print_element (&WW_PRINT_b);  (void)Print_element (&WW_PRINT_c);
  (void)Print_element (&WW_PRINT_d);  (void)Print_element (&WW_PRINT_e);  (void)Print_element (&WW_PRINT_f);
  (void)Print_element (&WW_PRINT_g);  (void)Print_element (&WW_PRINT_h);  (void)Print_element (&WW_PRINT_i);
  (void)Print_element (&WW_PRINT_j);  (void)Print_element (&WW_PRINT_k);  (void)Print_element (&WW_PRINT_l);
  (void)Print_element (&WW_PRINT_m);  (void)Print_element (&WW_PRINT_n);  (void)Print_element (&WW_PRINT_o);
  (void)Print_element (&WW_PRINT_p);  (void)Print_element (&WW_PRINT_q);  (void)Print_element (&WW_PRINT_r);
  (void)Print_element (&WW_PRINT_s);  (void)Print_element (&WW_PRINT_t);  (void)Print_element (&WW_PRINT_u);
  (void)Print_element (&WW_PRINT_v);  (void)Print_element (&WW_PRINT_w);  (void)Print_element (&WW_PRINT_x);
  (void)Print_element (&WW_PRINT_y);  (void)Print_element (&WW_PRINT_z);
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_element (&WW_PRINT_SPACE);  (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&WW_PRINT_0);  (void)Print_element (&WW_PRINT_1);  (void)Print_element (&WW_PRINT_2);
  (void)Print_element (&WW_PRINT_3);  (void)Print_element (&WW_PRINT_4);  (void)Print_element (&WW_PRINT_5);
  (void)Print_element (&WW_PRINT_6);  (void)Print_element (&WW_PRINT_7);  (void)Print_element (&WW_PRINT_8);
  (void)Print_element (&WW_PRINT_9);
  (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&WW_PRINT_EXCLAMATION);  (void)Print_element (&WW_PRINT_QUOTE);
  (void)Print_element (&WW_PRINT_POUND);        (void)Print_element (&WW_PRINT_DOLLAR);
  (void)Print_element (&WW_PRINT_PERCENT);      (void)Print_element (&WW_PRINT_AMPERSAND);
  (void)Print_element (&WW_PRINT_APOSTROPHE);   (void)Print_element (&WW_PRINT_LPAREN);
  (void)Print_element (&WW_PRINT_RPAREN);       (void)Print_element (&WW_PRINT_ASTERISK);
  (void)Print_element (&WW_PRINT_PLUS);         (void)Print_element (&WW_PRINT_COMMA);
  (void)Print_element (&WW_PRINT_HYPHEN);       (void)Print_element (&WW_PRINT_PERIOD);
  (void)Print_element (&WW_PRINT_SLASH);        (void)Print_element (&WW_PRINT_COLON);
  (void)Print_element (&WW_PRINT_SEMICOLON);    (void)Print_element (&ASCII_PRINT_LESS);
  (void)Print_element (&WW_PRINT_EQUAL);        (void)Print_element (&ASCII_PRINT_GREATER);
  (void)Print_element (&WW_PRINT_QUESTION);
  (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&WW_PRINT_AT);              (void)Print_element (&WW_PRINT_LBRACKET);
  (void)Print_element (&ASCII_PRINT_BSLASH);       (void)Print_element (&WW_PRINT_RBRACKET);
  (void)Print_element (&ASCII_PRINT_CARET);        (void)Print_element (&WW_PRINT_UNDERSCORE);
  (void)Print_element (&ASCII_PRINT_BAPOSTROPHE);  (void)Print_element (&ASCII_PRINT_LBRACE);
  (void)Print_element (&ASCII_PRINT_BAR);          (void)Print_element (&ASCII_PRINT_RBRACE);
  (void)Print_element (&ASCII_PRINT_TILDE);
  (void)Print_element (&WW_PRINT_CRtn);  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);
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

const struct serial_action FUTURE_SERIAL_ACTIONS[128] = { 
  {CMD_NONE,          NULL},                      // NUL
  {CMD_NONE,          NULL},                      // SOH
  {CMD_NONE,          NULL},                      // STX
  {CMD_NONE,          NULL},                      // ETX
  {CMD_NONE,          NULL},                      // EOT
  {CMD_NONE,          NULL},                      // ENQ
  {CMD_NONE,          NULL},                      // ACK
  {CMD_PRINT,         &WW_PRINT_BEEP},            // BEL
  {CMD_PRINT,         &WW_PRINT_Backspace},       // BS
  {CMD_PRINT,         &WW_PRINT_Tab},             // TAB
  {CMD_ASCII_LF,      NULL},                      // LF
  {CMD_NONE,          NULL},                      // VT
  {CMD_NONE,          NULL},                      // FF
  {CMD_ASCII_CR,      &WW_PRINT_CRtn},            // CR
  {CMD_NONE,          NULL},                      // SO
  {CMD_NONE,          NULL},                      // SI
  {CMD_NONE,          NULL},                      // DLE
  {CMD_ASCII_XON,     NULL},                      // DC1/XON
  {CMD_NONE,          NULL},                      // DC2
  {CMD_ASCII_XOFF,    NULL},                      // DC3/XOFF
  {CMD_NONE,          NULL},                      // DC4
  {CMD_NONE,          NULL},                      // NAK
  {CMD_NONE,          NULL},                      // SYN
  {CMD_NONE,          NULL},                      // ETB
  {CMD_NONE,          NULL},                      // CAN
  {CMD_NONE,          NULL},                      // EM
  {CMD_NONE,          NULL},                      // SUB
  {CMD_NONE,          NULL},                      // ESC
  {CMD_NONE,          NULL},                      // FS
  {CMD_NONE,          NULL},                      // GS
  {CMD_NONE,          NULL},                      // RS
  {CMD_NONE,          NULL},                      // US
  {CMD_PRINT,         &WW_PRINT_SPACE},           //
  {CMD_PRINT,         &WW_PRINT_EXCLAMATION},     // !
  {CMD_PRINT,         &WW_PRINT_QUOTE},           // "
  {CMD_PRINT,         &WW_PRINT_POUND},           // #
  {CMD_PRINT,         &WW_PRINT_DOLLAR},          // $
  {CMD_PRINT,         &WW_PRINT_PERCENT},         // %
  {CMD_PRINT,         &WW_PRINT_AMPERSAND},       // &
  {CMD_PRINT,         &WW_PRINT_APOSTROPHE},      // '
  {CMD_PRINT,         &WW_PRINT_LPAREN},          // (
  {CMD_PRINT,         &WW_PRINT_RPAREN},          // )
  {CMD_PRINT,         &WW_PRINT_ASTERISK},        // *
  {CMD_PRINT,         &WW_PRINT_PLUS},            // +
  {CMD_PRINT,         &WW_PRINT_COMMA},           // , 
  {CMD_PRINT,         &WW_PRINT_HYPHEN},          // -
  {CMD_PRINT,         &WW_PRINT_PERIOD},          // .
  {CMD_PRINT,         &WW_PRINT_SLASH},           // /
  {CMD_PRINT,         &WW_PRINT_0},               // 0
  {CMD_PRINT,         &WW_PRINT_1},               // 1
  {CMD_PRINT,         &WW_PRINT_2},               // 2
  {CMD_PRINT,         &WW_PRINT_3},               // 3
  {CMD_PRINT,         &WW_PRINT_4},               // 4
  {CMD_PRINT,         &WW_PRINT_5},               // 5
  {CMD_PRINT,         &WW_PRINT_6},               // 6
  {CMD_PRINT,         &WW_PRINT_7},               // 7
  {CMD_PRINT,         &WW_PRINT_8},               // 8
  {CMD_PRINT,         &WW_PRINT_9},               // 9
  {CMD_PRINT,         &WW_PRINT_COLON},           // :
  {CMD_PRINT,         &WW_PRINT_SEMICOLON},       // ;
  {CMD_PRINT,         &ASCII_PRINT_LESS},         // <
  {CMD_PRINT,         &WW_PRINT_EQUAL},           // =
  {CMD_PRINT,         &ASCII_PRINT_GREATER},      // >
  {CMD_PRINT,         &WW_PRINT_QUESTION},        // ?
  {CMD_PRINT,         &WW_PRINT_AT},              // @
  {CMD_PRINT,         &WW_PRINT_A},               // A
  {CMD_PRINT,         &WW_PRINT_B},               // B
  {CMD_PRINT,         &WW_PRINT_C},               // C
  {CMD_PRINT,         &WW_PRINT_D},               // D
  {CMD_PRINT,         &WW_PRINT_E},               // E
  {CMD_PRINT,         &WW_PRINT_F},               // F
  {CMD_PRINT,         &WW_PRINT_G},               // G
  {CMD_PRINT,         &WW_PRINT_H},               // H
  {CMD_PRINT,         &WW_PRINT_I},               // I
  {CMD_PRINT,         &WW_PRINT_J},               // J
  {CMD_PRINT,         &WW_PRINT_K},               // K
  {CMD_PRINT,         &WW_PRINT_L},               // L
  {CMD_PRINT,         &WW_PRINT_M},               // M
  {CMD_PRINT,         &WW_PRINT_N},               // N
  {CMD_PRINT,         &WW_PRINT_O},               // O
  {CMD_PRINT,         &WW_PRINT_P},               // P
  {CMD_PRINT,         &WW_PRINT_Q},               // Q
  {CMD_PRINT,         &WW_PRINT_R},               // R
  {CMD_PRINT,         &WW_PRINT_S},               // S
  {CMD_PRINT,         &WW_PRINT_T},               // T
  {CMD_PRINT,         &WW_PRINT_U},               // U
  {CMD_PRINT,         &WW_PRINT_V},               // V
  {CMD_PRINT,         &WW_PRINT_W},               // W
  {CMD_PRINT,         &WW_PRINT_X},               // X
  {CMD_PRINT,         &WW_PRINT_Y},               // Y
  {CMD_PRINT,         &WW_PRINT_Z},               // Z
  {CMD_PRINT,         &WW_PRINT_LBRACKET},        // [
  {CMD_PRINT,         &ASCII_PRINT_BSLASH},       // <backslash>
  {CMD_PRINT,         &WW_PRINT_RBRACKET},        // ]
  {CMD_PRINT,         &ASCII_PRINT_CARET},        // ^
  {CMD_PRINT,         &WW_PRINT_UNDERSCORE},      // _
  {CMD_PRINT,         &ASCII_PRINT_BAPOSTROPHE},  // `
  {CMD_PRINT,         &WW_PRINT_a},               // a
  {CMD_PRINT,         &WW_PRINT_b},               // b
  {CMD_PRINT,         &WW_PRINT_c},               // c
  {CMD_PRINT,         &WW_PRINT_d},               // d
  {CMD_PRINT,         &WW_PRINT_e},               // e
  {CMD_PRINT,         &WW_PRINT_f},               // f
  {CMD_PRINT,         &WW_PRINT_g},               // g
  {CMD_PRINT,         &WW_PRINT_h},               // h
  {CMD_PRINT,         &WW_PRINT_i},               // i
  {CMD_PRINT,         &WW_PRINT_j},               // j
  {CMD_PRINT,         &WW_PRINT_k},               // k
  {CMD_PRINT,         &WW_PRINT_l},               // l
  {CMD_PRINT,         &WW_PRINT_m},               // m
  {CMD_PRINT,         &WW_PRINT_n},               // n
  {CMD_PRINT,         &WW_PRINT_o},               // o
  {CMD_PRINT,         &WW_PRINT_p},               // p
  {CMD_PRINT,         &WW_PRINT_q},               // q
  {CMD_PRINT,         &WW_PRINT_r},               // r
  {CMD_PRINT,         &WW_PRINT_s},               // s
  {CMD_PRINT,         &WW_PRINT_t},               // t
  {CMD_PRINT,         &WW_PRINT_u},               // u
  {CMD_PRINT,         &WW_PRINT_v},               // v
  {CMD_PRINT,         &WW_PRINT_w},               // w
  {CMD_PRINT,         &WW_PRINT_x},               // x
  {CMD_PRINT,         &WW_PRINT_y},               // y
  {CMD_PRINT,         &WW_PRINT_z},               // z
  {CMD_PRINT,         &ASCII_PRINT_LBRACE},       // {
  {CMD_PRINT,         &ASCII_PRINT_BAR},          // |
  {CMD_PRINT,         &ASCII_PRINT_RBRACE},       // }
  {CMD_PRINT,         &ASCII_PRINT_TILDE},        // ~
  {CMD_NONE,          NULL}                       // DEL
};


//**********************************************************************************************************************
//
//  Future key action tables.
//
//**********************************************************************************************************************

// Stup key action table.
const struct key_action FUTURE_ACTIONS_SETUP[3 * NUM_WW_KEYS] = {
  // Unshifted.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_COMMAND,                                 '\\', 0, NULL},                      // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_NONE,                                    0,    0, NULL},                      // <down arrow>
  {ACTION_COMMAND,                                 'z',  0, NULL},                      // z, Z, SUB
  {ACTION_COMMAND,                                 'q',  0, NULL},                      // q, Q, DC1/XON
  {ACTION_COMMAND,                                 '1',  0, NULL},                      // 1, !
  {ACTION_COMMAND,                                 '`',  0, NULL},                      // `, ~
  {ACTION_COMMAND,                                 'a',  0, NULL},                      // a, A, SOH
  {ACTION_COMMAND,                                 ' ',  0, NULL},                      // <space>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_COMMAND,                                 'x',  0, NULL},                      // x, X, CAN
  {ACTION_COMMAND,                                 'w',  0, NULL},                      // w, W, ETB
  {ACTION_COMMAND,                                 '2',  0, NULL},                      // 2, @, NUL
  {ACTION_COMMAND,                                 's',  0, NULL},                      // s, S, DC3/XOFF
  {ACTION_COMMAND,                                 'c',  0, NULL},                      // c, C, ETX
  {ACTION_COMMAND,                                 'e',  0, NULL},                      // e, E, ENQ
  {ACTION_COMMAND,                                 '3',  0, NULL},                      // 3, #
  {ACTION_COMMAND,                                 'd',  0, NULL},                      // d, D, EOT
  {ACTION_COMMAND,                                 'b',  0, NULL},                      // b, B, STX
  {ACTION_COMMAND,                                 'v',  0, NULL},                      // v, V, SYN
  {ACTION_COMMAND,                                 't',  0, NULL},                      // t, T, DC4
  {ACTION_COMMAND,                                 'r',  0, NULL},                      // r, R, DC2
  {ACTION_COMMAND,                                 '4',  0, NULL},                      // 4, $
  {ACTION_COMMAND,                                 '5',  0, NULL},                      // 5, %
  {ACTION_COMMAND,                                 'f',  0, NULL},                      // f, F, ACK
  {ACTION_COMMAND,                                 'g',  0, NULL},                      // g, G, BEL
  {ACTION_COMMAND,                                 'n',  0, NULL},                      // n, N, SO
  {ACTION_COMMAND,                                 'm',  0, NULL},                      // m, M, CR
  {ACTION_COMMAND,                                 'y',  0, NULL},                      // y, Y, EM
  {ACTION_COMMAND,                                 'u',  0, NULL},                      // u, U, NAK
  {ACTION_COMMAND,                                 '7',  0, NULL},                      // 7, &
  {ACTION_COMMAND,                                 '6',  0, NULL},                      // 6, ^, RS
  {ACTION_COMMAND,                                 'j',  0, NULL},                      // j, J, LF
  {ACTION_COMMAND,                                 'h',  0, NULL},                      // h, H, BS
  {ACTION_COMMAND,                                 ',',  0, NULL},                      // ,, <
  {ACTION_COMMAND,                                 ']',  0, NULL},                      // ], }, GS
  {ACTION_COMMAND,                                 'i',  0, NULL},                      // i, I, TAB
  {ACTION_COMMAND,                                 '8',  0, NULL},                      // 8, *
  {ACTION_COMMAND,                                 '=',  0, NULL},                      // =, +
  {ACTION_COMMAND,                                 'k',  0, NULL},                      // k, K, VT
  {ACTION_COMMAND,                                 '.',  0, NULL},                      // ., >
  {ACTION_COMMAND,                                 'o',  0, NULL},                      // o, O, SI
  {ACTION_COMMAND,                                 '9',  0, NULL},                      // 9, (
  {ACTION_COMMAND,                                 'l',  0, NULL},                      // l, L, FF
  {ACTION_COMMAND,                                 '/',  0, NULL},                      // /, ?
  {ACTION_COMMAND,                                 '[',  0, NULL},                      // [, {, ESC
  {ACTION_COMMAND,                                 'p',  0, NULL},                      // p, P, DLE
  {ACTION_COMMAND,                                 '0',  0, NULL},                      // 0, )
  {ACTION_COMMAND,                                 '-',  0, NULL},                      // -, _, US
  {ACTION_COMMAND,                                 ';',  0, NULL},                      // ;, :
  {ACTION_COMMAND,                                 '\'', 0, NULL},                      // ', "
  {ACTION_NONE,                                    0,    0, NULL},                      // <left arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_COMMAND,                                 0x0d, 0, NULL},                      // <return>
  {ACTION_NONE,                                    0,    0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                      // <escape>
  {ACTION_NONE,                                    0,    0, NULL},                      // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000

  // Shifted.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_COMMAND,                                 '|',  0, NULL},                      // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_NONE,                                    0,    0, NULL},                      // <down arrow>
  {ACTION_COMMAND,                                 'Z',  0, NULL},                      // z, Z, SUB
  {ACTION_COMMAND,                                 'Q',  0, NULL},                      // q, Q, DC1/XON
  {ACTION_COMMAND,                                 '!',  0, NULL},                      // 1, !
  {ACTION_COMMAND,                                 '~',  0, NULL},                      // `, ~
  {ACTION_COMMAND,                                 'A',  0, NULL},                      // a, A, SOH
  {ACTION_COMMAND,                                 ' ',  0, NULL},                      // <space>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_COMMAND,                                 'X',  0, NULL},                      // x, X, CAN
  {ACTION_COMMAND,                                 'W',  0, NULL},                      // w, W, ETB
  {ACTION_COMMAND,                                 '@',  0, NULL},                      // 2, @, NUL
  {ACTION_COMMAND,                                 'S',  0, NULL},                      // s, S, DC3/XOFF
  {ACTION_COMMAND,                                 'C',  0, NULL},                      // c, C, ETX
  {ACTION_COMMAND,                                 'E',  0, NULL},                      // e, E, ENQ
  {ACTION_COMMAND,                                 '#',  0, NULL},                      // 3, #
  {ACTION_COMMAND,                                 'D',  0, NULL},                      // d, D, EOT
  {ACTION_COMMAND,                                 'B',  0, NULL},                      // b, B, STX
  {ACTION_COMMAND,                                 'V',  0, NULL},                      // v, V, SYN
  {ACTION_COMMAND,                                 'T',  0, NULL},                      // t, T, DC4
  {ACTION_COMMAND,                                 'R',  0, NULL},                      // r, R, DC2
  {ACTION_COMMAND,                                 '$',  0, NULL},                      // 4, $
  {ACTION_COMMAND,                                 '%',  0, NULL},                      // 5, %
  {ACTION_COMMAND,                                 'F',  0, NULL},                      // f, F, ACK
  {ACTION_COMMAND,                                 'G',  0, NULL},                      // g, G, BEL
  {ACTION_COMMAND,                                 'N',  0, NULL},                      // n, N, SO
  {ACTION_COMMAND,                                 'M',  0, NULL},                      // m, M, CR
  {ACTION_COMMAND,                                 'Y',  0, NULL},                      // y, Y, EM
  {ACTION_COMMAND,                                 'U',  0, NULL},                      // u, U, NAK
  {ACTION_COMMAND,                                 '&',  0, NULL},                      // 7, &
  {ACTION_COMMAND,                                 '^',  0, NULL},                      // 6, ^, RS
  {ACTION_COMMAND,                                 'J',  0, NULL},                      // j, J, LF
  {ACTION_COMMAND,                                 'H',  0, NULL},                      // h, H, BS
  {ACTION_COMMAND,                                 '<',  0, NULL},                      // ,, <
  {ACTION_COMMAND,                                 '}',  0, NULL},                      // ], }, GS
  {ACTION_COMMAND,                                 'I',  0, NULL},                      // i, I, TAB
  {ACTION_COMMAND,                                 '*',  0, NULL},                      // 8, *
  {ACTION_COMMAND,                                 '+',  0, NULL},                      // =, +
  {ACTION_COMMAND,                                 'K',  0, NULL},                      // k, K, VT
  {ACTION_COMMAND,                                 '>',  0, NULL},                      // ., >
  {ACTION_COMMAND,                                 'O',  0, NULL},                      // o, O, SI
  {ACTION_COMMAND,                                 '(',  0, NULL},                      // 9, (
  {ACTION_COMMAND,                                 'L',  0, NULL},                      // l, L, FF
  {ACTION_COMMAND,                                 '?',  0, NULL},                      // /, ?
  {ACTION_COMMAND,                                 '{',  0, NULL},                      // [, {, ESC
  {ACTION_COMMAND,                                 'P',  0, NULL},                      // p, P, DLE
  {ACTION_COMMAND,                                 ')',  0, NULL},                      // 0, )
  {ACTION_COMMAND,                                 '_',  0, NULL},                      // -, _, US
  {ACTION_COMMAND,                                 ':',  0, NULL},                      // ;, :
  {ACTION_COMMAND,                                 '"',  0, NULL},                      // ', "
  {ACTION_NONE,                                    0,    0, NULL},                      // <left arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_COMMAND,                                 0x0d, 0, NULL},                      // <return>
  {ACTION_NONE,                                    0,    0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                      // <escape>
  {ACTION_NONE,                                    0,    0, NULL},                      // <tab set>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000

  // Control.
  {ACTION_NONE,                                    0,    0, NULL},                      // <left shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right shift>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // \, |
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <setup>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_NONE,                                    0,    0, NULL},                      // <down arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // z, Z, SUB
  {ACTION_NONE,                                    0,    0, NULL},                      // q, Q, DC1/XON
  {ACTION_NONE,                                    0,    0, NULL},                      // 1, !
  {ACTION_NONE,                                    0,    0, NULL},                      // `, ~
  {ACTION_NONE,                                    0,    0, NULL},                      // a, A, SOH
  {ACTION_NONE,                                    0,    0, NULL},                      // <space>
  {ACTION_NONE,                                    0,    0, NULL},                      //
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <left margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <tab clear>
  {ACTION_NONE,                                    0,    0, NULL},                      // <control>
  {ACTION_NONE,                                    0,    0, NULL},                      // x, X, CAN
  {ACTION_NONE,                                    0,    0, NULL},                      // w, W, ETB
  {ACTION_NONE,                                    0,    0, NULL},                      // 2, @, NUL
  {ACTION_NONE,                                    0,    0, NULL},                      // s, S, DC3/XOFF
  {ACTION_NONE,                                    0,    0, NULL},                      // c, C, ETX
  {ACTION_NONE,                                    0,    0, NULL},                      // e, E, ENQ
  {ACTION_NONE,                                    0,    0, NULL},                      // 3, #
  {ACTION_COMMAND,                                 0x04, 0, NULL},                      // d, D, EOT
  {ACTION_NONE,                                    0,    0, NULL},                      // b, B, STX
  {ACTION_NONE,                                    0,    0, NULL},                      // v, V, SYN
  {ACTION_NONE,                                    0,    0, NULL},                      // t, T, DC4
  {ACTION_NONE,                                    0,    0, NULL},                      // r, R, DC2
  {ACTION_NONE,                                    0,    0, NULL},                      // 4, $
  {ACTION_NONE,                                    0,    0, NULL},                      // 5, %
  {ACTION_NONE,                                    0,    0, NULL},                      // f, F, ACK
  {ACTION_NONE,                                    0,    0, NULL},                      // g, G, BEL
  {ACTION_NONE,                                    0,    0, NULL},                      // n, N, SO
  {ACTION_NONE,                                    0,    0, NULL},                      // m, M, CR
  {ACTION_NONE,                                    0,    0, NULL},                      // y, Y, EM
  {ACTION_NONE,                                    0,    0, NULL},                      // u, U, NAK
  {ACTION_NONE,                                    0,    0, NULL},                      // 7, &
  {ACTION_NONE,                                    0,    0, NULL},                      // 6, ^, RS
  {ACTION_NONE,                                    0,    0, NULL},                      // j, J, LF
  {ACTION_NONE,                                    0,    0, NULL},                      // h, H, BS
  {ACTION_NONE,                                    0,    0, NULL},                      // ,, <
  {ACTION_NONE,                                    0,    0, NULL},                      // ], }, GS
  {ACTION_NONE,                                    0,    0, NULL},                      // i, I, TAB
  {ACTION_NONE,                                    0,    0, NULL},                      // 8, *
  {ACTION_NONE,                                    0,    0, NULL},                      // =, +
  {ACTION_NONE,                                    0,    0, NULL},                      // k, K, VT
  {ACTION_NONE,                                    0,    0, NULL},                      // ., >
  {ACTION_NONE,                                    0,    0, NULL},                      // o, O, SI
  {ACTION_NONE,                                    0,    0, NULL},                      // 9, (
  {ACTION_NONE,                                    0,    0, NULL},                      // l, L, FF
  {ACTION_NONE,                                    0,    0, NULL},                      // /, ?
  {ACTION_NONE,                                    0,    0, NULL},                      // [, {, ESC
  {ACTION_NONE,                                    0,    0, NULL},                      // p, P, DLE
  {ACTION_NONE,                                    0,    0, NULL},                      // 0, )
  {ACTION_NONE,                                    0,    0, NULL},                      // -, _, US
  {ACTION_NONE,                                    0,    0, NULL},                      // ;, :
  {ACTION_NONE,                                    0,    0, NULL},                      // ', "
  {ACTION_NONE,                                    0,    0, NULL},                      // <left arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // <up arrow>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <backspace>
  {ACTION_NONE,                                    0,    0, NULL},                      // *** not available on WW1000
  {ACTION_NONE,                                    0,    0, NULL},                      // <return>
  {ACTION_NONE,                                    0,    0, NULL},                      // <delete>
  {ACTION_NONE,                                    0,    0, NULL},                      // <shift lock>
  {ACTION_NONE,                                    0,    0, NULL},                      // <right margin>
  {ACTION_NONE,                                    0,    0, NULL},                      // <tab>
  {ACTION_NONE,                                    0,    0, NULL},                      // <escape>
  {ACTION_NONE,                                    0,    0, NULL},                      // <tab set>
  {ACTION_NONE,                                    0,    0, NULL}                       // *** not available on WW1000
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

void Setup_FUTURE (void) {
  // key_actions = &FUTURE_ACTIONS_
  // serial_actions = &FUTURE_SERIAL_ACTIONS;
  // flow_on = CHAR_ASCII_XON;
  // flow_off = CHAR_ASCII_XOFF;
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

void Setup_STANDALONE (void) {
  // Nothing to be done here.  All setup is done by the master setup routine.
}

void Loop_STANDALONE (void) {
  // Nothing to be done here.  All work is done in the Interrupt Service Routine.
}

void ISR_STANDALONE (void) {

  // Do not process column scan interrupts while the firmware is initializing.
  if (run_mode == MODE_INITIALIZING) return;

  // Reset all row drive lines that may have been asserted for the preceeding column signal to prevent row drive
  // spill-over into this (a successive) column time period.
  Clear_all_row_lines ();

  // During the first ~5 seconds, blink the blue LED to verify that column scanning is happening.
  if (blue_led_count >= 0) {
    if (blue_led_count++ == 0) digitalWriteFast (BLUE_LED_PIN, blue_led_on);
    else if ((blue_led_count % 610) == 0) digitalWriteFast (BLUE_LED_PIN, blue_led_on);
    else if ((blue_led_count % 305) == 0) digitalWriteFast (BLUE_LED_PIN, blue_led_off);
    if (blue_led_count >= 6405) blue_led_count = -1;
  }

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

void setup (void) {

  // Set the typewriter run mode to initializing.
  run_mode = MODE_INITIALIZING;

  // Turn on orange LED to indicate setup() is running.
  pinMode (ORANGE_LED_PIN, OUTPUT);
  digitalWriteFast (ORANGE_LED_PIN, HIGH);
  orange_led = TRUE;

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
  // pinMode (ORANGE_LED_PIN, OUTPUT);  // Already done at start of setup().
  pinMode (BLUE_LED_PIN, OUTPUT);
  pinMode (ROW_ENABLE_PIN, OUTPUT);
  pinMode (UART_RX_PIN, INPUT);
  pinMode (UART_TX_PIN, OUTPUT);
  pinMode (serial_rts_pin, OUTPUT);
  pinMode (serial_cts_pin, INPUT);

  // Initialize the logic board row drive lines as inactive.
  Clear_all_row_lines ();

  // Initialize the status LEDs to off.
  // digitalWriteFast (ORANGE_LED_PIN, LOW);  // Turned on during setup().
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

  // Initialize powerwise and spell check typewriter settings if the batteries are not installed.
  if ((emulation != EMULATION_STANDALONE) && (battery == SETTING_FALSE)) {
    (void)Print_element (&WW_PRINT_POWERWISE_OFF);
    (void)Print_element (&WW_PRINT_SPELL_CHECK);
  }

  // Initialize impression typewriter setting.
  if (emulation != EMULATION_STANDALONE) {
    Change_impression (IMPRESSION_NORMAL, impression);
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

  // Wait for initial printing to finish.
  if (emulation != EMULATION_STANDALONE) {
    (void)Print_element (&WW_PRINT_CRtn);
    Wait_print_buffer_empty (1000);
  }
}

void loop (void) {

  // Turn off orange LED on first loop() call.
  if (orange_led) {
    digitalWriteFast (ORANGE_LED_PIN, LOW);
    orange_led = FALSE;
  }

  // Special case for Standalone Typewriter.
  if (emulation == EMULATION_STANDALONE) {
    Loop_STANDALONE ();
    return;
  }

  // Receive all pending characters.
  while (Serial_available ()) {
    byte chr = Serial_read ();
    if ((parity == PARITY_ODD) && (chr != ODD_PARITY[chr & 0x7f])) Report_warning (WARNING_PARITY);
    if ((parity == PARITY_EVEN) && (chr != EVEN_PARITY[chr & 0x7f])) Report_warning (WARNING_PARITY);
    chr = chr & 0x7f;

    // Handle escape sequences.
    if (escaping) {
      switch ((int)ESCAPE_FSA[escape_state][ESCAPE_CHARACTER[chr]]) {
        case ESC_NONE:
          continue;
        case ESC_PRINT:
          break;
        case ESC_EXIT:
          escaping = FALSE;
          continue;
        case ESC_ERROR:
          Report_error (ERROR_BAD_ESCAPE);
          escaping = FALSE;
          break;
        case ESC_INIT:
          escape_state = ESC_INIT_STATE;
          continue;
        case ESC_CSIP:
          escape_state = ESC_CSIP_STATE;
          continue;
        case ESC_CSII:
          escape_state = ESC_CSII_STATE;
          continue;
        case ESC_FUNC:
          escape_state = ESC_FUNC_STATE;
          continue;
        case ESC_STR:
          escape_state = ESC_STR_STATE;
          continue;
        default:
          Report_error (ERROR_BAD_ESCAPE);
          escaping = FALSE;
          break;
      }
    } else if ((chr == CHAR_ASCII_ESC) && (escapesequence == ESCAPE_IGNORE)) {
      escaping = TRUE;
      escape_state = ESC_INIT_STATE;
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

    // Handle receive buffer full errors.
    if (rb_count >= SIZE_RECEIVE_BUFFER) {
      Report_error (ERROR_RB_FULL);
      break;
    }

    // Receive buffer getting full, turn flow control off.
    if (flow_in_on && (rb_count >= UPPER_THRESHOLD_RECEIVE_BUFFER)) {
      if (swflow == SWFLOW_XON_XOFF) {
        if (Serial_write (flow_off) == 1) {
          Serial_send_now ();
          flow_in_on = FALSE;
        }
      }
      if ((serial == SERIAL_RS232) && (hwflow == HWFLOW_RTR_CTS)) {
        digitalWriteFast (serial_rts_pin, HIGH);
        flow_in_on = FALSE;
      }
    }

    // Store character in receive buffer.
    receive_buffer[rb_write] = chr;
    if (++rb_write >= SIZE_RECEIVE_BUFFER) rb_write = 0;
    ++rb_count;
  }

  // Send all pending characters.
  boolean sent = FALSE;
  while (sb_count > 0) {
    byte c = send_buffer[sb_read] & 0x7f;
    if (serial == SERIAL_USB) {
      if (parity == PARITY_ODD) c = ODD_PARITY[c];
      else if (parity == PARITY_EVEN) c = EVEN_PARITY[c];
    }
    if (Serial_write (c) != 1) break;
    if (++sb_read >= SIZE_SEND_BUFFER) sb_read = 0;
    Update_counter (&sb_count, -1);
    sent = TRUE;
  }
  if (sent) Serial_send_now ();

  // Turn off RTS when all characters have been sent.
  if ((sb_count == 0) && (serial == SERIAL_RS232) && (hwflow == HWFLOW_RTS_CTS)) {
    if (rts_state == RTS_ON) {
      rts_state = RTS_EMPTYING_SW;
    }
    if (rts_state == RTS_EMPTYING_SW) {
      if (rs232_mode == RS232_UART) {
        if (Serial1.availableForWrite () == rts_size) {
          rts_state = RTS_EMPTYING_HW;
        }
      } else /* rs232_mode == RS232_SLOW */ {
        if (slow_serial_port.availableForWrite () == rts_size) {
          rts_state = RTS_TIMEOUT;
          rts_timeout_time = millis () + RTS_TIMEOUT_DELAY;
        }
      }
    }
    if ((rts_state == RTS_EMPTYING_HW) && (UART0_S1 & UART_S1_TC)) {
      rts_state = RTS_TIMEOUT;
      rts_timeout_time = millis () + RTS_TIMEOUT_DELAY;
    }
    if ((rts_state == RTS_TIMEOUT) && (millis () >= rts_timeout_time)) {
      digitalWriteFast (serial_rts_pin, HIGH);
      rts_state = RTS_OFF;
    }
  }

  // Print all transferred print strings.
  while (tb_count > 0) {
    if (!Print_element ((const struct print_info *)(transfer_buffer[tb_read]))) break;
    if (++tb_read >= SIZE_TRANSFER_BUFFER) tb_read = 0;
    Update_counter (&tb_count, -1);
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

    // Receive buffer almost empty, turn flow control on.
    if (!flow_in_on && (rb_count <= LOWER_THRESHOLD_RECEIVE_BUFFER)) {
      if (swflow == SWFLOW_XON_XOFF) {
        if (Serial_write (flow_on) == 1) {
          Serial_send_now ();
          flow_in_on = TRUE;
        }
      }
      if ((serial == SERIAL_RS232) && (hwflow == HWFLOW_RTR_CTS)) {
        digitalWriteFast (serial_rts_pin, LOW);
        flow_in_on = TRUE;
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
        (void)Print_element ((*serial_actions)[chr].element);
        break;

      // IBM 1620 Jr. specific actions.

      case CMD_IBM_MODE_0:
        key_actions = &IBM_ACTIONS_MODE0;
        if (artn_IBM) { (void)Print_element (&WW_PRINT_ARtn);  artn_IBM = FALSE; }   // Turn off A Rtn light.
        if (bold_IBM) { (void)Print_element (&WW_PRINT_Bold);  bold_IBM = FALSE; }   // Turn off Bold light.
        if (!lock_IBM) { (void)Print_element (&WW_PRINT_Lock);  lock_IBM = TRUE; }   // Turn on Lock light.
        send_ack_IBM = TRUE;
        break;

      case CMD_IBM_MODE_1:
        key_actions = &IBM_ACTIONS_MODE1;
        if (!artn_IBM) { (void)Print_element (&WW_PRINT_ARtn);  artn_IBM = TRUE; }    // Turn on A Rtn light.
        if ((bold == SETTING_TRUE) && !bold_IBM) { (void)Print_element (&WW_PRINT_Bold);  bold_IBM = TRUE; }
                                                                                      // Turn on Bold light.
        if (lock_IBM) { (void)Print_element (&WW_PRINT_LShift);  lock_IBM = FALSE; }  // Turn off Lock light.
        send_ack_IBM = TRUE;
        break;

      case CMD_IBM_MODE_2:
        key_actions = &IBM_ACTIONS_MODE2;
        if (artn_IBM) { (void)Print_element (&WW_PRINT_ARtn);  artn_IBM = FALSE; }    // Turn off A Rtn light.
        if ((bold == SETTING_TRUE) && !bold_IBM) { (void)Print_element (&WW_PRINT_Bold);  bold_IBM = TRUE; }
                                                                                      // Turn on Bold light.
        if (lock_IBM) { (void)Print_element (&WW_PRINT_LShift);  lock_IBM = FALSE; }  // Turn off Lock light.
        send_ack_IBM = TRUE;
        break;

      case CMD_IBM_MODE_3:
        key_actions = &IBM_ACTIONS_MODE3;
        if (artn_IBM) { (void)Print_element (&WW_PRINT_ARtn);  artn_IBM = FALSE; }    // Turn off A Rtn light.
        if (bold_IBM) { (void)Print_element (&WW_PRINT_Bold);  bold_IBM = FALSE; }    // Turn off Bold light.
        if (lock_IBM) { (void)Print_element (&WW_PRINT_LShift);  lock_IBM = FALSE; }  // Turn off Lock light.
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
        flow_in_on = TRUE;
        flow_out_on = TRUE;
        Set_margins_tabs (TRUE);
        if (artn_IBM) { (void)Print_element (&WW_PRINT_ARtn);  artn_IBM = FALSE; }
        if (bold_IBM) { (void)Print_element (&WW_PRINT_Bold);  bold_IBM = FALSE; }
        if (!lock_IBM) { (void)Print_element (&WW_PRINT_Lock);  lock_IBM = TRUE; }
        send_ack_IBM = TRUE;
        break;

      case CMD_IBM_PAUSE:
        flow_out_on = FALSE;
        break;

      case CMD_IBM_RESUME:
        flow_out_on = TRUE;
        break;

      // ASCII Terminal specific actions.

      case CMD_ASCII_CR:
        if (receiveeol == EOL_CR) {
          (void)Print_element (&WW_PRINT_CRtn);
        } else if (receiveeol == EOL_CRLF) {
          pending_lf = TRUE;
        } else if (receiveeol == EOL_LF) {
          (void)Print_element (&ASCII_PRINT_CR);
        } else /* receiveeol == EOL_LFCR */ {
          if (pending_cr) {
            (void)Print_element (&WW_PRINT_CRtn);
          } else {
            (void)Print_element (&ASCII_PRINT_CR);
          }
        }
        pending_cr = FALSE;
        break;

      case CMD_ASCII_LF:
        if (receiveeol == EOL_CR) {
          (void)Print_element (&ASCII_PRINT_LF);
        } else if (receiveeol == EOL_CRLF) {
          if (pending_lf) {
            (void)Print_element (&WW_PRINT_CRtn);
          } else {
            (void)Print_element (&ASCII_PRINT_LF);
          }
        } else if (receiveeol == EOL_LF) {
          (void)Print_element (&WW_PRINT_CRtn);
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
        Report_error (ERROR_BAD_SERIAL_ACTION);
        break;
    }
  }

  // Handle IBM 1620 Jr. setup mode.
  if (run_mode == MODE_IBM_BEING_SETUP) {

    // Stop computer sending data.
    if (flow_in_on) {
      if (Serial_write (flow_off) == 1) {
        Serial_send_now ();
      }
    }

    // Reset all settings to factory defaults.
    if (shift) {
      run_mode = MODE_INITIALIZING;
      Initialize_global_variables ();
      Initialize_configuration_settings (TRUE);
      digitalWriteFast (ORANGE_LED_PIN, LOW);
      digitalWriteFast (BLUE_LED_PIN, blue_led_off);
      if (artn_IBM) (void)Print_element (&WW_PRINT_ARtn);
      if (bold_IBM) (void)Print_element (&WW_PRINT_Bold);
      if (lock_IBM) (void)Print_element (&WW_PRINT_LShift);
      Setup_IBM ();
      run_mode = MODE_RUNNING;
      (void)Print_string ("---- All settings reset to factory defaults.\r\r");
      Wait_print_buffer_empty (1000);

    // Interactive setup.
    } else {
      if (artn_IBM) (void)Print_element (&WW_PRINT_ARtn);
      if (bold_IBM) (void)Print_element (&WW_PRINT_Bold);
      if (lock_IBM) (void)Print_element (&WW_PRINT_LShift);
      Print_IBM_setup_title ();
      while (TRUE) {
        char cmd = Read_setup_command ();
        if (cmd == 's') {
          Update_IBM_settings ();
        } else if (cmd == 'e') {
          Print_errors_warnings ();
        } else if (cmd == 'c') {
          Print_IBM_character_set ();
        } else if (cmd == 't') {
          Change_typewriter_settings ();
        } else if (cmd == 'd') {
          Developer_functions ();
        } else /* cmd == 'q' */ {
          break;
        }
      }
      if (artn_IBM) (void)Print_element (&WW_PRINT_ARtn);
      if (bold_IBM) (void)Print_element (&WW_PRINT_Bold);
      if (lock_IBM) (void)Print_element (&WW_PRINT_Lock);
      key_actions = key_actions_save;
      run_mode = MODE_RUNNING;
    }

    // Restore computer sending data.
    if (flow_in_on) {
      if (Serial_write (flow_on) == 1) {
        Serial_send_now ();
      }
    }
  }

  // Handle ASCII Terminal setup mode.
  if (run_mode == MODE_ASCII_BEING_SETUP) {

    // Stop computer sending data.
    if (flow_in_on) {
      if (swflow == SWFLOW_XON_XOFF) {
        if (Serial_write (flow_off) == 1) {
          Serial_send_now ();
        }
      }
      if ((serial == SERIAL_RS232) && (hwflow == HWFLOW_RTR_CTS)) {
        digitalWriteFast (serial_rts_pin, HIGH);
      }
    }

    // Reset all settings to factory defaults.
    if (shift) {
      run_mode = MODE_INITIALIZING;
      Initialize_global_variables ();
      Initialize_configuration_settings (TRUE);
      digitalWriteFast (ORANGE_LED_PIN, LOW);
      digitalWriteFast (BLUE_LED_PIN, blue_led_off);
      Setup_ASCII ();
      run_mode = MODE_RUNNING;
      (void)Print_string ("---- All settings reset to factory defaults.\r\r");
      Wait_print_buffer_empty (1000);

    // Interactive setup.
    } else {
      Print_ASCII_setup_title ();
      while (TRUE) {
        char cmd = Read_setup_command ();
        if (cmd == 's') {
          Update_ASCII_settings ();
        } else if (cmd == 'e') {
          Print_errors_warnings ();
        } else if (cmd == 'c') {
          Print_ASCII_character_set ();
        } else if (cmd == 't') {
          Change_typewriter_settings ();
        } else if (cmd == 'd') {
          Developer_functions ();
        } else /* cmd == 'q' */ {
          break;
        }
      }
      key_actions = key_actions_save;
      run_mode = MODE_RUNNING;
    }

    // Restore computer sending data.
    if (flow_in_on) {
      if (swflow == SWFLOW_XON_XOFF) {
        if (Serial_write (flow_on) == 1) {
          Serial_send_now ();
        }
      }
      if ((serial == SERIAL_RS232) && (hwflow == HWFLOW_RTR_CTS)) {
        digitalWriteFast (serial_rts_pin, LOW);
      }
    }
  }

  // Handle future setup mode.
  if (run_mode == MODE_FUTURE_BEING_SETUP) {

    // Stop computer sending data.
    if (flow_in_on) {
      if (swflow == SWFLOW_XON_XOFF) {
        if (Serial_write (flow_off) == 1) {
          Serial_send_now ();
        }
      }
      if ((serial == SERIAL_RS232) && (hwflow == HWFLOW_RTR_CTS)) {
        digitalWriteFast (serial_rts_pin, HIGH);
      }
    }

    // Reset all settings to factory defaults.
    if (shift) {
      run_mode = MODE_INITIALIZING;
      Initialize_global_variables ();
      Initialize_configuration_settings (TRUE);
      digitalWriteFast (ORANGE_LED_PIN, LOW);
      digitalWriteFast (BLUE_LED_PIN, blue_led_off);
      Setup_FUTURE ();
      run_mode = MODE_RUNNING;
      (void)Print_string ("---- All settings reset to factory defaults.\r\r");
      Wait_print_buffer_empty (1000);

    // Interactive setup.
    } else {
      key_actions = key_actions_save;
      run_mode = MODE_RUNNING;
    }

    // Restore computer sending data.
    if (flow_in_on) {
      if (swflow == SWFLOW_XON_XOFF) {
        if (Serial_write (flow_on) == 1) {
          Serial_send_now ();
        }
      }
      if ((serial == SERIAL_RS232) && (hwflow == HWFLOW_RTR_CTS)) {
        digitalWriteFast (serial_rts_pin, LOW);
      }
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
//           will also deactivate Lock.  The Code key functions as a special shift, similar to a control key.  If both a
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


void ISR_common (void) {

  // Do not process column scan interrupts while the firmware is initializing.
  if (run_mode == MODE_INITIALIZING) return;

  // Get the time that the ISR was entered.
  interrupt_time = millis ();

  // Measure column scan timing.
  if (in_column_scan_timing) {
    interrupt_time_microseconds = micros ();
    if (last_interrupt_time_microseconds > 0UL) {
      elapsed_time_microseconds = interrupt_time_microseconds - last_interrupt_time_microseconds;
      if (interrupt_time_microseconds < last_interrupt_time_microseconds) elapsed_time_microseconds =
                                                                          ~elapsed_time_microseconds + 1;
      if (elapsed_time_microseconds >= CSCAN_MINIMUM) {
        ++column_scan_count;
        total_scan_time += elapsed_time_microseconds;
        minimum_scan_time = min (minimum_scan_time, elapsed_time_microseconds);
        maximum_scan_time = max (maximum_scan_time, elapsed_time_microseconds);
      }
    }
    last_interrupt_time_microseconds = interrupt_time_microseconds;
  }

  // During the first ~5 seconds, blink the blue LED to verify that column scanning is happening.
  if (blue_led_count >= 0) {
    if (blue_led_count++ == 0) digitalWriteFast (BLUE_LED_PIN, blue_led_on);
    else if ((blue_led_count % 610) == 0) digitalWriteFast (BLUE_LED_PIN, blue_led_on);
    else if ((blue_led_count % 305) == 0) digitalWriteFast (BLUE_LED_PIN, blue_led_off);
    if (blue_led_count >= 6405) blue_led_count = -1;
  }

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
      if ((last_scan_duration > 0UL) && (last_scan_duration < SHORT_SCAN_DURATION)) {
        if ((shortest_scan_duration == 0UL) || (last_scan_duration < shortest_scan_duration))
                                                                shortest_scan_duration = last_scan_duration;
        Report_warning (WARNING_SHORT_SCAN);
      } else if (last_scan_duration > LONG_SCAN_DURATION) {
        if (last_scan_duration > longest_scan_duration) longest_scan_duration = last_scan_duration;
        Report_warning (WARNING_LONG_SCAN);
        if (in_printer_timing) {
          Clear_counter (&pb_count);
          pb_write = pb_read;
          print_buffer[pb_write] = WW_NULL;
        }
        if (in_full_speed_test) {
          last_print_time = millis () - start_print_time;
          last_character_count = print_character_count;
        }
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
inline void ISR_column_1 (void) {

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
inline void ISR_column_2 (void) {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // If the current print character is in this column, assert appropriate row line.
  (void)Test_print (WW_COLUMN_2);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_RIGHTARROW_RightWord])
                                                      Process_repeating_key (ROW_IN_1_PIN, WW_KEY_RIGHTARROW_RightWord);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X33_22]) Process_key (ROW_IN_2_PIN, WW_KEY_X33_22);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X32_23]) Process_key (ROW_IN_3_PIN, WW_KEY_X32_23);
  if (interrupt_time >= key_shadow_times[WW_KEY_PaperDown_DownMicro])
                                                                 Process_key (ROW_IN_4_PIN, WW_KEY_PaperDown_DownMicro);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X31_25]) Process_key (ROW_IN_5_PIN, WW_KEY_X31_25);
  if (interrupt_time >= key_shadow_times[WW_KEY_PaperUp_UpMicro]) Process_key (ROW_IN_6_PIN, WW_KEY_PaperUp_UpMicro);
  if (interrupt_time >= key_shadow_times[WW_KEY_Reloc_LineSpace]) Process_key (ROW_IN_7_PIN, WW_KEY_Reloc_LineSpace);
  if (interrupt_time >= key_shadow_times[WW_KEY_DOWNARROW_DownLine])
                                                        Process_repeating_key (ROW_IN_8_PIN, WW_KEY_DOWNARROW_DownLine);
}

//
// Column 3 ISR for keys:  z, Z, q, Q, Impr, 1, !, Spell, +/-, <degree>, a, A
//
inline void ISR_column_3 (void) {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // Press 'a' key if timing column scans.
  if (flood_a_5_keys && (flood_key_count > 0) && ((flood_key_count % 3) == 2)) {
    Assert_key (WW_a_A);
    --flood_key_count;
    return;
  }

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
inline void ISR_column_4 (void) {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // Press space key if timing column scans.
  if (press_space_key) {
    Assert_key (WW_SPACE_REQSPACE);
    return;
  }

  // If the current print character is in this column, assert appropriate row line.
  (void)Test_print (WW_COLUMN_4);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_SPACE_REQSPACE])
                                                            Process_repeating_key (ROW_IN_1_PIN, WW_KEY_SPACE_REQSPACE);
  if (interrupt_time >= key_shadow_times[WW_KEY_LOADPAPER_SETTOPOFFORM])
                                                              Process_key (ROW_IN_2_PIN, WW_KEY_LOADPAPER_SETTOPOFFORM);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X13_43]) Process_key (ROW_IN_3_PIN, WW_KEY_X13_43);
  if (interrupt_time >= key_shadow_times[WW_KEY_LMar]) Process_key (ROW_IN_4_PIN, WW_KEY_LMar);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X12_45]) Process_key (ROW_IN_5_PIN, WW_KEY_X12_45);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X11_46]) Process_key (ROW_IN_6_PIN, WW_KEY_X11_46);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X14_47]) Process_key (ROW_IN_7_PIN, WW_KEY_X14_47);
  if (interrupt_time >= key_shadow_times[WW_KEY_TClr_TClrAll]) Process_key (ROW_IN_8_PIN, WW_KEY_TClr_TClrAll);
}

//
// Column 5 ISR for keys:  Code
//
inline void ISR_column_5 (void) {

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
inline void ISR_column_6 (void) {

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
inline void ISR_column_7 (void) {

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
inline void ISR_column_8 (void) {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // Press '5' key if timing column scans.
  if (flood_a_5_keys && (flood_key_count > 0) && ((flood_key_count % 3) != 2)) {
    if ((flood_key_count % 3) == 1) {
      Assert_key (WW_5_PERCENT);
    }
    --flood_key_count;
    return;
  }

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
inline void ISR_column_9 (void) {

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
inline void ISR_column_10 (void) {

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
inline void ISR_column_11 (void) {

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
inline void ISR_column_12 (void) {

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
inline void ISR_column_13 (void) {

  // Clear all row drive lines.
  Clear_all_row_lines ();

  // If the current print character is in this column, assert appropriate row line.
  (void)Test_print (WW_COLUMN_13);

  // Accounting for bounce shadow, if any keyboard row is asserted take appropriate action.
  if (interrupt_time >= key_shadow_times[WW_KEY_LEFTARROW_LeftWord])
                                                        Process_repeating_key (ROW_IN_1_PIN, WW_KEY_LEFTARROW_LeftWord);
  if (interrupt_time >= key_shadow_times[WW_KEY_UPARROW_UpLine])
                                                            Process_repeating_key (ROW_IN_2_PIN, WW_KEY_UPARROW_UpLine);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X22_134]) Process_key (ROW_IN_4_PIN, WW_KEY_X22_134);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X21_135]) Process_key (ROW_IN_5_PIN, WW_KEY_X21_135);
  if (interrupt_time >= key_shadow_times[WW_KEY_Backspace_Bksp1])
                                                           Process_repeating_key (ROW_IN_6_PIN, WW_KEY_Backspace_Bksp1);
  // if (interrupt_time >= key_shadow_times[WW_KEY_X23_137]) Process_key (ROW_IN_7_PIN, WW_KEY_X23_137);
  if (interrupt_time >= key_shadow_times[WW_KEY_CRtn_IndClr]) Process_return_key (ROW_IN_8_PIN, WW_KEY_CRtn_IndClr);
}

//
// Column 14 ISR for keys:  <back X>, <back word>, Lock, R Mar, Tab, Ind L, Mar Rel, Re Prt, T Set
//
inline void ISR_column_14 (void) {

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
void Initialize_global_variables (void) {

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

  current_column = INITIAL_LMARGIN;
  current_position = POSITION_INITIAL;
  in_composite_character = FALSE;
  in_margin_tab_setting = FALSE;
  previous_element = NULL;

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

  flow_in_on = TRUE;
  flow_out_on = TRUE;

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
  asciiwheel = INITIAL_ASCIIWHEEL;
  if (emulation == EMULATION_IBM) {
    offset = INITIAL_IBM_OFFSET;
  } else if (emulation == EMULATION_ASCII) {
    if (asciiwheel == SETTING_FALSE) {
      offset = INITIAL_ASCII_OFFSET;
    } else {
      offset = INITIAL_ASCII_OFFSETX;
    }
  } else /* emulation == EMULATION_FUTURE */ {
    if (asciiwheel == SETTING_FALSE) {
      offset = INITIAL_FUTURE_OFFSET;
    } else {
      offset = INITIAL_FUTURE_OFFSETX;
    }
  } 
  slash = INITIAL_SLASH;
  bold = INITIAL_BOLD;
  serial = INITIAL_SERIAL;
  duplex = INITIAL_DUPLEX;
  baud = INITIAL_BAUD;
  parity = INITIAL_PARITY;
  dps = INITIAL_DPS;
  impression = INITIAL_IMPRESSION;
  swflow = INITIAL_SWFLOW;
  hwflow = INITIAL_HWFLOW;
  uppercase = INITIAL_UPPERCASE;
  autoreturn = INITIAL_AUTORETURN;
  transmiteol = INITIAL_TRANSMITEOL;
  receiveeol = INITIAL_RECEIVEEOL;
  escapesequence = INITIAL_ESCAPESEQUENCE;
  length = INITIAL_LENGTH;

  if (reset || (EEPROM.read (EEPROM_FINGERPRINT) != FINGERPRINT)) {

    // Initialize EEPROM and margins & tabs.
    for (int i = 0; i < SIZE_EEPROM; ++i) {
      Write_EEPROM (i, 0);
    }
    Write_EEPROM (EEPROM_FINGERPRINT, FINGERPRINT);
    Write_EEPROM (EEPROM_VERSION, VERSION);
    Write_EEPROM (EEPROM_EMULATION, emulation);
    Write_EEPROM (EEPROM_ERRORS, errors);
    Write_EEPROM (EEPROM_WARNINGS, warnings);
    Write_EEPROM (EEPROM_BATTERY, battery);
    Write_EEPROM (EEPROM_LMARGIN, lmargin);
    Write_EEPROM (EEPROM_RMARGIN, rmargin);
    Write_EEPROM (EEPROM_OFFSET, offset);
    Write_EEPROM (EEPROM_ASCIIWHEEL, asciiwheel);
    Write_EEPROM (EEPROM_SLASH, slash);
    Write_EEPROM (EEPROM_BOLD, bold);
    Write_EEPROM (EEPROM_SERIAL, serial);
    Write_EEPROM (EEPROM_DUPLEX, duplex);
    Write_EEPROM (EEPROM_BAUD, baud);
    Write_EEPROM (EEPROM_PARITY, parity);
    Write_EEPROM (EEPROM_DPS, dps);
    Write_EEPROM (EEPROM_IMPRESSION, impression);
    Write_EEPROM (EEPROM_SWFLOW, swflow);
    Write_EEPROM (EEPROM_HWFLOW, hwflow);
    Write_EEPROM (EEPROM_UPPERCASE, uppercase);
    Write_EEPROM (EEPROM_AUTORETURN, autoreturn);
    Write_EEPROM (EEPROM_TRANSMITEOL, transmiteol);
    Write_EEPROM (EEPROM_RECEIVEEOL, receiveeol);
    Write_EEPROM (EEPROM_ESCAPESEQUENCE, escapesequence);
    Write_EEPROM (EEPROM_LENGTH, length);

    Set_margins_tabs (TRUE);

  } else {

    // Special handling for redefined escape sequence and line offset settings.
    // Map old setting value     -> new setting value.
    //     ignoreescape == true  -> escapesequence == ignore.
    //     ignoreescape == false -> escapesequence == none.
    //     offset == true        -> offset = 1.
    //     offset == false       -> offset = 0.
    if (Read_EEPROM (EEPROM_VERSION, VERSION) < VERSION_ESCAPEOFFSET_CHANGED) {
      if (Read_EEPROM (EEPROM_ESCAPESEQUENCE, SETTING_TRUE) == SETTING_TRUE) {
        Write_EEPROM (EEPROM_ESCAPESEQUENCE, ESCAPE_IGNORE);
      } else /* Read_EEPROM (EEPROM_ESCAPESEQUENCE, SETTING_TRUE) == SETTING_FALSE */ {
        Write_EEPROM (EEPROM_ESCAPESEQUENCE, ESCAPE_NONE);
      }
      if (Read_EEPROM (EEPROM_OFFSET, SETTING_TRUE) == SETTING_TRUE) {
        Write_EEPROM (EEPROM_OFFSET, 1);
      } else /* Read_EEPROM (EEPROM_OFFSET, SETTING_TRUE) == SETTING_FALSE */ {
        Write_EEPROM (EEPROM_OFFSET, 0);
      }
    }

    // Retrieve configuration parameters and set margins & tabs if needed.
    if (Read_EEPROM (EEPROM_VERSION, 0) != VERSION) Write_EEPROM (EEPROM_VERSION, VERSION);
    pemulation = Read_EEPROM (EEPROM_EMULATION, emulation);
    if (emulation != pemulation) Write_EEPROM (EEPROM_EMULATION, emulation);
    errors = Read_EEPROM (EEPROM_ERRORS, errors);
    warnings = Read_EEPROM (EEPROM_WARNINGS, warnings);
    battery = Read_EEPROM (EEPROM_BATTERY, battery);
    lmargin = Read_EEPROM (EEPROM_LMARGIN, lmargin);
    rmargin = Read_EEPROM (EEPROM_RMARGIN, rmargin);
    offset = Read_EEPROM (EEPROM_OFFSET, offset);
    asciiwheel = Read_EEPROM (EEPROM_ASCIIWHEEL, asciiwheel);
    slash = Read_EEPROM (EEPROM_SLASH, slash);
    bold = Read_EEPROM (EEPROM_BOLD, bold);
    serial = Read_EEPROM (EEPROM_SERIAL, serial);
    duplex = Read_EEPROM (EEPROM_DUPLEX, duplex);
    baud = Read_EEPROM (EEPROM_BAUD, baud);
    parity = Read_EEPROM (EEPROM_PARITY, parity);
    dps = Read_EEPROM (EEPROM_DPS, dps);
    impression = Read_EEPROM (EEPROM_IMPRESSION, impression);
    swflow = Read_EEPROM (EEPROM_SWFLOW, swflow);
    hwflow = Read_EEPROM (EEPROM_HWFLOW, hwflow);
    uppercase = Read_EEPROM (EEPROM_UPPERCASE, uppercase);
    autoreturn = Read_EEPROM (EEPROM_AUTORETURN, autoreturn);
    transmiteol = Read_EEPROM (EEPROM_TRANSMITEOL, transmiteol);
    receiveeol = Read_EEPROM (EEPROM_RECEIVEEOL, receiveeol);
    escapesequence = Read_EEPROM (EEPROM_ESCAPESEQUENCE, escapesequence);
    length = Read_EEPROM (EEPROM_LENGTH, length);

    if (emulation == pemulation) {
      for (unsigned int i = 0; i < sizeof(tabs); ++i) tabs[i] = Read_EEPROM (EEPROM_TABS + i, tabs[i]);
      if (battery == SETTING_FALSE) {
        Set_margins_tabs (FALSE);
      }
    } else {
      Set_margins_tabs (TRUE);
    }
  }
}

// Clear all row drive lines.
inline void Clear_all_row_lines (void) {
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
      if (!key_pressed_states[WW_KEY_TClr_TClrAll]) {
        Take_action (key_offset + key);
      } else {  // Special case for T Clr + C Rtn = clear all tabs.
        key_repeat_times[key] = 0xffffffffUL;  // Don't allow return to auto repeat.
        Transfer_print_string (&WW_PRINT_TClrAll);
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

// Send a single character.
inline boolean Send_character (char chr) {
  if (sb_count < SIZE_SEND_BUFFER) {
    send_buffer[sb_write] = chr;
    if (++sb_write >= SIZE_SEND_BUFFER) sb_write = 0;
    Update_counter (&sb_count, +1);
  } else {  // Character doesn't fit in the send buffer.
    Report_error (ERROR_SB_FULL);
    return FALSE;
  }
  return TRUE;
}

// Send a string of characters.
inline boolean Send_characters (const char str[]) {
  const char *ptr = str;

  while (*ptr != 0) {
    if (!Send_character (*(ptr++))) return FALSE;  // Character doesn't fit in send buffer.
  }
  return TRUE;
}

// Transfer a print string to main for processing.
inline boolean Transfer_print_string (const struct print_info *str) {
  if (tb_count < SIZE_TRANSFER_BUFFER) {
    transfer_buffer[tb_write] = str;
    if (++tb_write >= SIZE_TRANSFER_BUFFER) tb_write = 0;
    Update_counter (&tb_count, +1);
    return TRUE;
  } else {  // Print element doesn't fit in transfer buffer.
    Report_error (ERROR_TB_FULL);
    return FALSE;
  }
}

// Read an integer.
int Read_integer (int value) {
  char chr;
  boolean neg = FALSE;
  int tmp = 0;
  int tmpx;

  // Read response.
  while (TRUE) {
    chr = Read_setup_character_from_set ("-0123456789\x90\x91\r");
    if (chr == '\x90') {
      (void)Print_element (&WW_PRINT_UPARROW);
    } else if (chr == '\x91') {
      (void)Print_element (&WW_PRINT_DOWNARROW);
    } else {
      break;
    }
  }

  // Process response.
  if (chr == '\r') {
    (void)Print_integer (value, 0);
    return value;
  } else if (chr == '-') {
    (void)Print_character (chr);
    neg = TRUE;
  } else /* chr == <digit> */ {
    (void)Print_character (chr);
    tmp = chr - '0';
  }
  while (TRUE) {
    chr = Read_setup_character_from_set ("0123456789\r");
    if (chr == '\r') break;
    tmpx = 10 * tmp + (chr - '0');
    if (tmpx < 0) return (neg ? INT_MIN : INT_MAX);
    (void)Print_character (chr);
    tmp = tmpx;
  }

  return (neg ? - tmp : tmp);
}

// Print an integer.
boolean Print_integer (int val, int wid) {
  int tmp = abs (val);
  int idx = 10;
  char chr[12];

  memset ((void *)chr, ' ', sizeof(chr));
  chr[sizeof(chr) - 1] = 0x00;

  do {
    chr[idx--] = '0' + (tmp % 10);
    tmp /= 10;
  } while (tmp > 0);

  if (val < 0) chr[idx--] = '-';

  if (wid > 0) {
    return Print_string (&chr[11 - wid]);
  } else {
    return Print_string (&chr[idx + 1]);
  }
}

// Print a scaled integer.
boolean Print_scaled_integer (int val, int fract, int wid) {
  int tmp = abs (val);
  int idx = 12;
  char chr[14];

  memset ((void *)chr, ' ', sizeof(chr));
  chr[sizeof(chr) - 1] = 0x00;

  for (int i = 0; i < fract; ++i) {
    chr[idx--] = '0' + (tmp % 10);
    tmp /= 10;
  }
  chr[idx--] = '.';

  do {
    chr[idx--] = '0' + (tmp % 10);
    tmp /= 10;
  } while (tmp > 0);

  if (val < 0) chr[idx--] = '-';

  if (wid > 0) {
    return Print_string (&chr[13 - wid]);
  } else {
    return Print_string (&chr[idx + 1]);
  }
}

// Print a single character.
inline boolean Print_character (char chr) {
  struct serial_action act = ASCII_SERIAL_ACTIONS[chr & 0x7f];

  if (act.element != NULL_PRINT_INFO) {
    if (!Print_element (act.element)) return FALSE;  // Character doesn't fit in print buffer.
  }
  return TRUE;
}

// Print a string of characters.
inline boolean Print_string (const char str[]) {
  const char *ptr = str;

  while (*ptr != 0) {
    struct serial_action act = ASCII_SERIAL_ACTIONS[*(ptr++) & 0x7f];
    if (act.element != NULL_PRINT_INFO) {
      if (!Print_element (act.element)) return FALSE;  // Character doesn't fit in print buffer.
    }
  }
  return TRUE;
}

// Print a print element.
boolean Print_element (const struct print_info *elem) {
  int start;
  int pbw;
  int pbc;
  int inc;
  int tab;
  int next;
  int temp;
  boolean ret;
  const byte *pelem;

  // Process special print elements.
  if (elem->type == ELEMENT_COMPOSITE) {
    boolean comp = in_composite_character;
    in_composite_character = TRUE;
    const struct print_info **ptr = elem->pelement;
    while (*ptr != NULL_PRINT_INFO) {
      if (!Print_element (*ptr++)) {
        in_composite_character = comp;
        return FALSE;
      }
    }
    for (int i = 0; i < elem->timing; i += FSCAN_0_CHANGES) {
      if (!Print_character (0x00)) {
        in_composite_character = comp;
        return FALSE;
      }
    }
    in_composite_character = comp;
    if (!in_composite_character) Update_print_position (elem);
    previous_element = elem;
    return TRUE; 
  } else if (elem->type == ELEMENT_DYNAMIC) {
    ret = Print_element (*(elem->pelement));
    if (ret) previous_element = elem;
    return ret;
  } else if (elem->type != ELEMENT_SIMPLE) {
    Report_error (ERROR_BAD_TYPE);
    return FALSE;
  }

  // Discard needless MarRel elements if not at left margin.
  if ((elem->spacing == SPACING_MARREL) && (current_column != lmargin)) return TRUE;

  // Discard Tab elements if at or beyond right margin.
  if ((elem->spacing == SPACING_TAB) && (current_column >= rmargin)) return TRUE;

  // Discard Backspace elements if at physical left stop and not setting margins & tabs or at left margin and not a
  // composite character and not setting margins & tabs.
  if ((elem->spacing == SPACING_BACKWARD) && (((current_column == (- offset + 1)) && !in_margin_tab_setting) ||
                                             ((current_column == lmargin) && !in_composite_character &&
                                              !in_margin_tab_setting))) return TRUE;

  // Discard L Mar elements if before column 1.
  if ((elem->spacing == SPACING_LMAR) && (current_column < 1)) return TRUE;

  // Inject an automatic carriage return, if enabled, when the right margin is hit and the next character isn't a
  // carriage return, left arrow, right arrow, backspace, bksp 1, set right margin, set tab, clear tab, clear all tabs,
  // or beep.  Right arrow is a special case to allow setting the right margin beyond where it is currently set.
  // If the line overflows and automatic return is disabled, then trigger a warning, beep, and ignore the character.
  if ((current_column > rmargin) && (!in_margin_tab_setting) &&
      (elem->element != (const byte*)&WW_STR_CRtn) &&
      (elem->element != (const byte*)&WW_STR_LEFTARROW) &&
      (elem->element != (const byte*)&WW_STR_RIGHTARROW) &&
      (elem->element != (const byte*)&WW_STR_Backspace) &&
      (elem->element != (const byte*)&WW_STR_Bksp1) &&
      (elem->element != (const byte*)&WW_STR_RMar) &&
      (elem->element != (const byte*)&WW_STR_TSet) &&
      (elem->element != (const byte*)&WW_STR_TClr) &&
      (elem->element != (const byte*)&WW_STR_TClrAll) &&
      (elem->element != (const byte*)&WW_STR_BEEP)) {
    if (autoreturn == SETTING_TRUE) {
      if (!Print_element (&WW_PRINT_CRtn)) return FALSE;  // Carriage return doesn't fit in print buffer.
    } else {
      if (!Print_element (&WW_PRINT_BEEP)) return FALSE;  // Beep doesn't fit in print buffer.
      return TRUE;                                        // Discard element.
    }
  }

  // For ASCII Terminal using Period Graphic characters, a space following '\', '{', or '}' needs special handling to
  // print correctly.
  if ((emulation == EMULATION_ASCII) && (asciiwheel == SETTING_FALSE) && (elem == &WW_PRINT_SPACE) &&
      ((previous_element == &ASCII_PRINT_BSLASH) || (previous_element == &ASCII_PRINT_LBRACE) ||
       (previous_element == &ASCII_PRINT_RBRACE))) {
    previous_element = NULL;
    ret = Print_element (&ASCII_PRINT_SPACEX);
    if (ret) previous_element = elem;
    return ret;
  }

  // Prepare for copying.
  start = pb_write;
  pbw = pb_write;
  pbc = pb_count;
  inc = 0;
  pelem = elem->element;

  // Add count character to buffer if requested.
  if (count_characters) {
    if (pbc++ >= (SIZE_PRINT_BUFFER - 1)) {  // Element doesn't fit in print buffer.
      Report_error (ERROR_PB_FULL);
      return FALSE;
    }
    ++inc;
    if (++pbw >= SIZE_PRINT_BUFFER) pbw = 0;
    print_buffer[pbw] = WW_COUNT;
  }

  // Copy element print codes to buffer.
  while (TRUE) {
    byte pchr = *pelem++;
    if (pchr == WW_NULL) break;
    if (pbc++ >= (SIZE_PRINT_BUFFER - 1)) {  // Element doesn't fit in print buffer.
      Report_error (ERROR_PB_FULL);
      return FALSE;
    }
    if (!PC_VALIDATION[pchr]) {
      bad_print_code = pchr;
      Report_error (ERROR_BAD_CODE);
      return FALSE;
    }
    ++inc;
    if (++pbw >= SIZE_PRINT_BUFFER) pbw = 0;
    print_buffer[pbw] = pchr;
  }

  // Adjust residual print time.
  if (elem->spacing == SPACING_RETURN) {
    residual_time += elem->timing + (current_column - lmargin) * TIME_RETURN_FACTOR;  // Prorate return time based on
                                                                                      // current column position.
  } else if (elem->spacing == SPACING_TAB) {
    tab = min (current_column + 1, rmargin);
    while ((tab < rmargin) && (tabs[tab] == SETTING_FALSE)) ++tab;
    residual_time += elem->timing + (tab - current_column) * TIME_TAB_FACTOR;  // Prorate tab time based on number of
                                                                               // columns to skip.
  } else {
    residual_time += elem->timing;
  }

  // Update spin position and adjust residual print time.
  if (elem->position < POSITION_COUNT) {
    next = elem->position;
    temp = abs (next - current_position);
    residual_time += TIME_SPIN_FACTOR * min (POSITION_COUNT - temp, temp);
    current_position = next;
  } else if (elem->position == POSITION_RESET) {
    current_position = 0;
  } else if (elem->position < POSITION_NOCHANGE) {
    Report_error (ERROR_BAD_POSITION);
    return FALSE;
  }

  // Add empty full scans as needed.  Do not pad when doing full speed print test.
  if (!in_full_speed_test) {
    while (residual_time >= (in_composite_character ? 0 : FSCAN_0_CHANGES)) {
      ++inc;
      if (pbc++ >= (SIZE_PRINT_BUFFER - 1)) {  // Element doesn't fit in print buffer.
        Report_error (ERROR_PB_FULL);
        return FALSE;
      }
      if (++pbw >= SIZE_PRINT_BUFFER) pbw = 0;
      print_buffer[pbw] = WW_NULL_14;
      residual_time -= FSCAN_0_CHANGES;
    } 
  } else {  // In full speed print test remove current character's full scan alignment.
    --inc;
    --pbc;
    if (--pbw < 0) pbw = SIZE_PRINT_BUFFER - 1;
    residual_time = 0;
  }

  // Add null print code after print string.
  ++inc;
  if (pbc++ >= (SIZE_PRINT_BUFFER - 1)) {  // Element doesn't fit in print buffer.
    Report_error (ERROR_PB_FULL);
    return FALSE;
  }
  if (++pbw >= SIZE_PRINT_BUFFER) pbw = 0;
  print_buffer[pbw] = WW_NULL;

  // Release copied string for printing.
  pb_write = pbw;
  Update_counter (&pb_count, inc);
  print_buffer[start] = WW_SKIP;

  // Update current print position, tab stops, and margins.
  if (!in_composite_character) Update_print_position (elem);

  previous_element = elem;
  return TRUE;
}

// Update current print position, tab stops, and margins.
void Update_print_position (const struct print_info *elem) {
  int tab;

  switch (elem->spacing) {
    case SPACING_NONE:      // No horizontal movement.
    case SPACING_UNKNOWN:
      // Nothing to do.
      break;
    case SPACING_FORWARD:   // Forward horizontal movement.
      ++current_column;
      break;
    case SPACING_BACKWARD:  // Backward horizontal movement.
      if (current_column > (- offset + 1)) --current_column;
      break;
    case SPACING_TAB:       // Tab horizontal movement.
    case SPACING_TABX:
      tab = min (current_column + 1, rmargin);
      while ((tab < rmargin) && (tabs[tab] == SETTING_FALSE)) ++tab;
      current_column = tab;
      break;
    case SPACING_RETURN:    // Return horizontal movement.
    case SPACING_RETURNX:
    case SPACING_LOADPAPER:
      current_column = lmargin;
      residual_time = 0;
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
    case SPACING_TCLRALL:   // No horizontal movement, clear all tabs.
      memset ((void *)tabs, SETTING_FALSE, sizeof(tabs));
      tabs[rmargin] = SETTING_TRUE;
      for (unsigned int i = 0; i < sizeof(tabs); ++i) Write_EEPROM (EEPROM_TABS + i, SETTING_FALSE);
      break;
    case SPACING_MARTABS:   // Reset all margins and tabs.
      Set_margins_tabs (TRUE);
      break;
    default:                // Invalid spacing code.
      Report_error (ERROR_BAD_SPACING);
      break;
  }
}

// Test if print buffer is empty.
inline boolean Test_print_buffer_empty (void) {
  return ((pb_count == 0) && (last_scan_duration < LONG_SCAN_DURATION) &&
          (last_last_scan_duration < LONG_SCAN_DURATION));
}

// Wait for the print buffer to be empty and optionally for printing to finish.
inline void Wait_print_buffer_empty (int wait) {
  if (run_mode == MODE_INITIALIZING) return;
  while (!Test_print_buffer_empty ()) delay (1);
  if (wait > 0) delay (wait);
}

// Assert a key's row line in current column.
inline void Assert_key (byte key) {
  byte pin = row_out_pins[key & 0x0f];
  if (pin != ROW_OUT_NO_PIN) {
    digitalWriteFast (pin, HIGH);
    digitalWriteFast (ROW_ENABLE_PIN, LOW);
  }
}

// Test column of current print code and assert row line if match, return output pin.
inline byte Test_print (int column) {
  byte pchr = print_buffer[pb_read];

  while (TRUE) {

    // Skip any skip print codes.
    if (pchr == WW_SKIP) {
      if (++pb_read >= SIZE_PRINT_BUFFER) pb_read = 0;
      pchr = print_buffer[pb_read];
      Update_counter (&pb_count, -1);
      continue;
    }

    // Count character when timing printer.
    else if (pchr == WW_COUNT) {
      ++print_character_count;
      if (++pb_read >= SIZE_PRINT_BUFFER) pb_read = 0;
      pchr = print_buffer[pb_read];
      Update_counter (&pb_count, -1);
      continue;
    }

    // Test if printing has caught up.
    else if (pchr == WW_CATCH_UP) {
      if ((last_scan_duration >= LONG_SCAN_DURATION) || (last_last_scan_duration >= LONG_SCAN_DURATION))
                                                                                                  return ROW_OUT_NO_PIN;
      if (++pb_read >= SIZE_PRINT_BUFFER) pb_read = 0;
      pchr = print_buffer[pb_read];
      Update_counter (&pb_count, -1);
      continue;
    }

    else
      break;
  }

  // If next print code is in this column, assert associated row line.
  if (((pchr >> 4) & 0x0f) == column) {
    byte pin = row_out_pins[pchr & 0x0f];
    if (pin != ROW_OUT_NO_PIN) {
      digitalWriteFast (pin, HIGH);
      digitalWriteFast (ROW_ENABLE_PIN, LOW);
    }
    if (++pb_read >= SIZE_PRINT_BUFFER) pb_read = 0;
    Update_counter (&pb_count, -1);
    return pin;
  }

  return ROW_OUT_NO_PIN;
}

// Based on key pressed, take action as given by current key action table.
void Take_action (int key) {
  byte action = (*key_actions)[key].action;

  // Send character if requested.
  if (action & ACTION_SEND) {
    Send_character ((*key_actions)[key].send);
  }

  // Print character if requested.
  if (action & ACTION_PRINT) {
    Transfer_print_string ((*key_actions)[key].element);
  }

  // Queue command character if requested.
  if (action & ACTION_COMMAND) {
    if (cb_count < SIZE_COMMAND_BUFFER) {
      command_buffer[cb_write] = (*key_actions)[key].send;
      if (++cb_write >= SIZE_COMMAND_BUFFER) cb_write = 0;
      Update_counter (&cb_count, +1);
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
          Update_counter (&sb_count, +1);
        } else {  // Character doesn't fit in send buffer.
          Report_error (ERROR_SB_FULL);
        }
      } else if (transmiteol == EOL_CRLF) {
        if (sb_count < (SIZE_SEND_BUFFER - 1)) {
          send_buffer[sb_write] = CHAR_ASCII_CR;
          if (++sb_write >= SIZE_SEND_BUFFER) sb_write = 0;
          Update_counter (&sb_count, +1);
          send_buffer[sb_write] = CHAR_ASCII_LF;
          if (++sb_write >= SIZE_SEND_BUFFER) sb_write = 0;
          Update_counter (&sb_count, +1);
        } else {  // Character doesn't fit in send buffer.
          Report_error (ERROR_SB_FULL);
        }
      } else if (transmiteol == EOL_LF) {
        if (sb_count < SIZE_SEND_BUFFER) {
          send_buffer[sb_write] = CHAR_ASCII_LF;
          if (++sb_write >= SIZE_SEND_BUFFER) sb_write = 0;
          Update_counter (&sb_count, +1);
        } else {  // Character doesn't fit in send buffer.
          Report_error (ERROR_SB_FULL);
        }
      } else /* transmiteol == EOL_LFCR */ {
        if (sb_count < (SIZE_SEND_BUFFER - 1)) {
          send_buffer[sb_write] = CHAR_ASCII_LF;
          if (++sb_write >= SIZE_SEND_BUFFER) sb_write = 0;
          Update_counter (&sb_count, +1);
          send_buffer[sb_write] = CHAR_ASCII_CR;
          if (++sb_write >= SIZE_SEND_BUFFER) sb_write = 0;
          Update_counter (&sb_count, +1);
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

    default:                                  // Invalid action.
      Report_error (ERROR_BAD_KEY_ACTION);
      break;
  }
}

// Safely zero/increment/decrement a shared counter without disabling interrupts.
// Note:  The Teensy 3.5's Cortex-M4 processor implements the ARMv7-M architecture which supports synchronization
//        primitives.  As long as all updates to the shared counters in the main code and ISRs call these routines, the
//        counters will be correct.  For information on the LDREX/STREX instructions, refer to sections 2.2.7 and 3.4.8
//        in: http://infocenter.arm.com/help/topic/com.arm.doc.dui0553b/DUI0553.pdf
__attribute__((noinline)) void Clear_counter (volatile int *counter) {
  int value;
  int result;
  do {
    asm volatile ("ldrex %0, [%1]" : "=r" (value) : "r" (counter) );
    value = 0;
    asm volatile ("strex %0, %2, %1" : "=&r" (result), "=Q" (*counter) : "r" (value));
  } while (result);
}

__attribute__((noinline)) void Update_counter (volatile int *counter, int increment) {
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
  if ((run_mode == MODE_RUNNING) && (errors == SETTING_TRUE) && (error > ERROR_NULL) && (error < NUM_ERRORS)) {
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

  // Disable right margin checking.
  in_margin_tab_setting = TRUE;

  // Reset margin and tab stop values.
  if (reset) {
    lmargin = 1;
    Write_EEPROM (EEPROM_LMARGIN, lmargin);
    rmargin = length;
    Write_EEPROM (EEPROM_RMARGIN, rmargin);
    memset ((void *)tabs, SETTING_FALSE, sizeof(tabs));
    if (emulation == EMULATION_ASCII) {
      for (unsigned int i = 8; i < sizeof(tabs); i += 8) tabs[i] = SETTING_TRUE;
    }
    tabs[rmargin] = SETTING_TRUE;
    for (unsigned int i = 0; i < sizeof(tabs); ++i) Write_EEPROM (EEPROM_TABS + i, tabs[i]);
  }

  // Set left margin.
  Return_column_1 ();
  for (int i = 1; i < lmargin; ++i) (void)Print_element (&WW_PRINT_SPACE);
  Wait_print_buffer_empty (100);
  (void)Print_element (&WW_PRINT_LMar);
  Wait_print_buffer_empty (100);

  // Set tab stops.
  (void)Print_element (&WW_PRINT_TClrAllX);  // Special case that doesn't clear tabs[].
  for (int i = lmargin; i <= rmargin; ++i) {
    if (tabs[i] == SETTING_TRUE) (void)Print_element (&WW_PRINT_TSet);
    if (i < rmargin) (void)Print_element (&WW_PRINT_SPACE);
  }
  Wait_print_buffer_empty (100);

  // Set right margin.
  (void)Print_element (&WW_PRINT_RMar);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);

  // Enable right margin checking.
  in_margin_tab_setting = FALSE;
}

// Print margins and tab stops.
boolean Print_margins_tabs (boolean index) {

  // Disable right margin checking.
  in_margin_tab_setting = TRUE;

  // Print column index if requested.
  if (index) {
    Return_column_1 ();
    for (int i = 1; i <= length; ++i) {
      if ((i % 10) == 9) {
        if (!Print_integer ((i + 1) / 10, 2)) {in_margin_tab_setting = FALSE; return FALSE;}
                                                                               // Character doesn't fit in print buffer.
      } else if ((i % 10) != 0) {
        if (!Print_element (&WW_PRINT_SPACE)) {in_margin_tab_setting = FALSE; return FALSE;}
                                                                               // Character doesn't fit in print buffer.
      }
    }
    Return_column_1 ();
    for (int i = 1; i <= length; ++i) {
      if (!Print_integer (i % 10, 0)) {in_margin_tab_setting = FALSE; return FALSE;}
                                                                               // Character doesn't fit in print buffer.
    }
  }

  // Print margin and tab stop markers.
  if (!Print_element (&WW_PRINT_CRtn)) {in_margin_tab_setting = FALSE; return FALSE;}
                                                                               // Character doesn't fit in print buffer.
  for (int i = lmargin; i <= rmargin; ++i) {
    if (i == lmargin) {if (!Print_element (&WW_PRINT_L)) {in_margin_tab_setting = FALSE; return FALSE;}}
                                                                               // Character doesn't fit in print buffer.
    else if (i == rmargin) {if (!Print_element (&WW_PRINT_R)) {in_margin_tab_setting = FALSE; return FALSE;}}
                                                                               // Character doesn't fit in print buffer.
    else if (tabs[i] == SETTING_TRUE) {if (!Print_element (&WW_PRINT_T)) {in_margin_tab_setting = FALSE; return FALSE;}}
                                                                               // Character doesn't fit in print buffer.
    else {if (!Print_element (&WW_PRINT_SPACE)) {in_margin_tab_setting = FALSE; return FALSE;}}
                                                                               // Character doesn't fit in print buffer.
  }
  if (!Print_element (&WW_PRINT_CRtn)) {in_margin_tab_setting = FALSE; return FALSE;}
                                                                               // Character doesn't fit in print buffer.
  Wait_print_buffer_empty (1000);

  // Enable right margin checking.
  in_margin_tab_setting = FALSE;

  return TRUE;
}

// Change typewriter impression setting.
void Change_impression (byte cur, byte next) {
  if (((cur == IMPRESSION_LIGHT)  && (next == IMPRESSION_NORMAL)) ||
      ((cur == IMPRESSION_NORMAL) && (next == IMPRESSION_HEAVY)) ||
      ((cur == IMPRESSION_HEAVY)  && (next == IMPRESSION_LIGHT))) {
    (void)Print_element (&WW_PRINT_Impr);
  } else if (((cur == IMPRESSION_LIGHT)  && (next == IMPRESSION_HEAVY)) ||
             ((cur == IMPRESSION_NORMAL) && (next == IMPRESSION_LIGHT)) ||
             ((cur == IMPRESSION_HEAVY)  && (next == IMPRESSION_NORMAL))) {
    (void)Print_element (&WW_PRINT_Impr);
    (void)Print_element (&WW_PRINT_Impr);
  }
}

// Read a setup command character.
char Read_setup_command (void) {
  char cmd;

  // Print command prompt.
  (void)Print_string ("Command [settings/errors/character set/typewriter/QUIT]: ");
  Space_to_column (COLUMN_COMMAND);

  // Read a command character.
  while (TRUE) {
    cmd = Read_setup_character_from_set ("sSeEcCtT\x04\x90\x91qQ\r");
    if (cmd == '\x90') {
      (void)Print_element (&WW_PRINT_UPARROW);
    } else if (cmd == '\x91') {
      (void)Print_element (&WW_PRINT_DOWNARROW);
    } else {
      break;
    }
  }

  // Return setup command.
  if ((cmd == 's') || (cmd == 'S')) {
    (void)Print_string ("settings\r");
    return 's';
  } else if ((cmd == 'e') || (cmd == 'E')) {
    (void)Print_string ("errors\r");
    return 'e';
  } else if ((cmd == 'c') || (cmd == 'C')) {
    (void)Print_string ("character set\r");
    return 'c';
  } else if ((cmd == 't') || (cmd == 'T')) {
    (void)Print_string ("typewriter\r");
    return 't';
  } else if (cmd == '\x04') {  // Control-D
    (void)Print_string ("developer\r");
    return 'd';
  } else /* (cmd == 'q') || (cmd == 'Q') || (cmd == '\r') */ {
    (void)Print_string ("quit\r\r---- End of Setup\r\r");
    return 'q';
  }
}

// Ask a yes or no question.
boolean Ask_yesno_question (const char str[], boolean value) {
  char chr;
  boolean val;

  // Print prompt.
  (void)Print_string ("  ");  (void)Print_string (str);
  if (value) {
    (void)Print_string (" [YES/no]? ");
  } else /* !value */ {
    (void)Print_string (" [yes/NO]? ");
  }
  Space_to_column (COLUMN_QUESTION);

  // Read response.
  while (TRUE) {
    chr = Read_setup_character_from_set ("yYnN\x90\x91\r");
    if (chr == '\x90') {
      (void)Print_element (&WW_PRINT_UPARROW);
    } else if (chr == '\x91') {
      (void)Print_element (&WW_PRINT_DOWNARROW);
    } else {
      break;
    }
  }

  // Process response.
  if ((chr == 'y') || (chr == 'Y')) {
    val = TRUE;
  } else if ((chr == 'n') || (chr == 'N')) {
    val = FALSE;
  } else /* chr == '\r' */ {
    val = value;
  }

  // Print response.
  if (val) {
    (void)Print_string ("yes\r");
  } else /* !val */ {
    (void)Print_string ("no\r");
  }

  return val;
}

// Read a true or false setting.
byte Read_truefalse_setting (const char str[], byte value) {
  char chr;
  byte val;

  // Print prompt.
  (void)Print_string ("  ");  (void)Print_string (str);
  if (value == SETTING_TRUE) {
    (void)Print_string (" [TRUE/false]: ");
  } else /* value == SETTING_FALSE */ {
    (void)Print_string (" [true/FALSE]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read response.
  while (TRUE) {
    chr = Read_setup_character_from_set ("tTfF\x90\x91\r");
    if (chr == '\x90') {
      (void)Print_element (&WW_PRINT_UPARROW);
    } else if (chr == '\x91') {
      (void)Print_element (&WW_PRINT_DOWNARROW);
    } else {
      break;
    }
  }

  // Process response.
  if ((chr == 't') || (chr == 'T')) {
    val = SETTING_TRUE;
  } else if ((chr == 'f') || (chr == 'F')) {
    val = SETTING_FALSE;
  } else /* chr == '\r' */ {
    val = value;
  }

  // Print response.
  if (val == SETTING_TRUE) {
    (void)Print_string ("true\r");
  } else /* val == SETTING_FALSE */ {
    (void)Print_string ("false\r");
  }

  return val;
}

// Read a serial setting.
byte Read_serial_setting (const char str[], byte value) {
  char chr;
  byte val;

  // Print prompt.
  (void)Print_string ("  ");  (void)Print_string (str);
  if (value == SERIAL_USB) {
    (void)Print_string (" [USB/rs232]: ");
  } else /* value == SERIAL_RS232 */ {
    (void)Print_string (" [usb/RS232]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read response.
  while (TRUE) {
    chr = Read_setup_character_from_set ("uUrR\x90\x91\r");
    if (chr == '\x90') {
      (void)Print_element (&WW_PRINT_UPARROW);
    } else if (chr == '\x91') {
      (void)Print_element (&WW_PRINT_DOWNARROW);
    } else {
      break;
    }
  }

  // Process response.
  if ((chr == 'u') || (chr == 'U')) {
    val = SERIAL_USB;
  } else if ((chr == 'r') || (chr == 'R')) {
    val = SERIAL_RS232;
  } else /* chr == '\r' */ {
    val = value;
  }

  // Print response.
  if (val == SERIAL_USB) {
    (void)Print_string ("usb\r");
  } else /* val == SERIAL_RS232 */ {
    (void)Print_string ("rs232\r");
  }

  return val;
}

// Read a duplex setting.
byte Read_duplex_setting (const char str[], byte value) {
  char chr;
  byte val;

  // Print prompt.
  (void)Print_string ("  ");  (void)Print_string (str);
  if (value == DUPLEX_HALF) {
    (void)Print_string (" [HALF/full]: ");
  } else /* value == DUPLEX_FULL */ {
    (void)Print_string (" [half/FULL]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read response.
  while (TRUE) {
    chr = Read_setup_character_from_set ("hHfF\x90\x91\r");
    if (chr == '\x90') {
      (void)Print_element (&WW_PRINT_UPARROW);
    } else if (chr == '\x91') {
      (void)Print_element (&WW_PRINT_DOWNARROW);
    } else {
      break;
    }
  }

  // Process response.
  if ((chr == 'h') || (chr == 'H')) {
    val = DUPLEX_HALF;
  } else if ((chr == 'f') || (chr == 'F')) {
    val = DUPLEX_FULL;
  } else /* chr == '\r' */ {
    val = value;
  }

  // Print response.
  if (val == DUPLEX_HALF) {
    (void)Print_string ("half\r");
  } else /* val == DUPLEX_FULL */ {
    (void)Print_string ("full\r");
  }

  return val;
}

// Read a baud setting.
byte Read_baud_setting (const char str[], byte value) {
  char chr;

  // Print prompt.
  (void)Print_string ("  ");  (void)Print_string (str);
  (void)Print_string (" [50-230400, ");  (void)Print_integer (BAUD_RATES[value], 0);  (void)Print_string ("]: ");
  Space_to_column (COLUMN_RESPONSE);

  // Read response.
  while (TRUE) {
    chr = Read_setup_character_from_set ("12345679\r");
    if (chr == '\x90') {
      (void)Print_element (&WW_PRINT_UPARROW);
    } else if (chr == '\x91') {
      (void)Print_element (&WW_PRINT_DOWNARROW);
    } else {
      break;
    }
  }

  // Process response.
  if (chr == '1') {
    (void)Print_character ('1');
    chr = Read_setup_character_from_set ("123589");
    if (chr == '1') {
      (void)Print_character ('1');
      chr = Read_setup_character_from_set ("05");
      if (chr == '0') {
        (void)Print_string ("0\r");
        return BAUD_110;
      } else /* chr == '5' */ {
        (void)Print_string ("5200\r");
        return BAUD_115200;
      }
    } else if (chr == '2') {
      (void)Print_string ("200\r");
      return BAUD_1200;
    } else if (chr == '3') {
      (void)Print_string ("34\r");
      return BAUD_134;
    } else if (chr == '5') {
      (void)Print_string ("50\r");
      return BAUD_150;
    } else if (chr == '8') {
      (void)Print_string ("800\r");
      return BAUD_1800;
    } else /* chr == '9' */ {
      (void)Print_string ("9200\r");
      return BAUD_19200;
    }

  } else if (chr == '2') {
    (void)Print_character ('2');
    chr = Read_setup_character_from_set ("034");
    if (chr == '0') {
      (void)Print_string ("00\r");
      return BAUD_200;
    } else if (chr == '3') {
      (void)Print_string ("30400\r");
      return BAUD_230400;
    } else /* chr == '4' */ {
      (void)Print_string ("400\r");
      return BAUD_2400;
    }

  } else if (chr == '3') {
    (void)Print_character ('3');
    chr = Read_setup_character_from_set ("08");
    if (chr == '0') {
      (void)Print_string ("00\r");
      return BAUD_300;
    } else /* chr == '8' */ {
      (void)Print_string ("8400\r");
      return BAUD_38400;
    }

  } else if (chr == '4') {
    (void)Print_string ("4800\r");
    return BAUD_4800;

  } else if (chr == '5') {
    (void)Print_character ('5');
    chr = Read_setup_character_from_set ("07");
    if (chr == '0') {
      (void)Print_string ("0\r");
      return BAUD_50;
    } else /* chr == '7' */ {
      (void)Print_string ("7600\r");
      return BAUD_57600;
    }

  } else if (chr == '6') {
    (void)Print_string ("600\r");
    return BAUD_600;

  } else if (chr == '7') {
    (void)Print_character ('7');
    chr = Read_setup_character_from_set ("56");
    if (chr == '5') {
      (void)Print_string ("5\r");
      return BAUD_75;
    } else /* chr == '6' */ {
      (void)Print_string ("6800\r");
      return BAUD_76800;
    }

  } else if (chr == '9') {
    (void)Print_string ("9600\r");
    return BAUD_9600;

  } else /* chr == '\r' */ {
    (void)Print_integer (BAUD_RATES[value], 0);
    (void)Print_character ('\r');
    return value;
  }
}

// Read a parity setting.
byte Read_parity_setting (const char str[], byte value) {
  char chr;
  byte val;

  // Print prompt.
  (void)Print_string ("  ");  (void)Print_string (str);
  if (value == PARITY_NONE) {
    (void)Print_string (" [NONE/odd/even]: ");
  } else if (value == PARITY_ODD) {
    (void)Print_string (" [none/ODD/even]: ");
  } else /* value == PARITY_EVEN */ {
    (void)Print_string (" [none/odd/EVEN]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read response.
  while (TRUE) {
    chr = Read_setup_character_from_set ("nNoOeE\x90\x91\r");
    if (chr == '\x90') {
      (void)Print_element (&WW_PRINT_UPARROW);
    } else if (chr == '\x91') {
      (void)Print_element (&WW_PRINT_DOWNARROW);
    } else {
      break;
    }
  }

  // Process response.
  if ((chr == 'n') || (chr == 'N')) {
    val = PARITY_NONE;
  } else if ((chr == 'o') || (chr == 'O')) {
    val = PARITY_ODD;
  } else if ((chr == 'e') || (chr == 'E')) {
    val = PARITY_EVEN;
  } else /* chr == '\r' */ {
    val = value;
  }

  // Print response.
  if (val == PARITY_NONE) {
    (void)Print_string ("none\r");
  } else if (val == PARITY_ODD) {
    (void)Print_string ("odd\r");
  } else /* val == PARITY_EVEN */ {
    (void)Print_string ("even\r");
  }

  return val;
}

// Read a databits, parity, stopbits setting.
byte Read_dps_setting (const char str[], byte value) {
  char chr;

  // Print prompt.
  (void)Print_string ("  ");  (void)Print_string (str);
  if (value == DPS_7O1) {
    (void)Print_string (" [7O1/7e1/8n1/8o1/8e1/8n2/8o2/8e2]: ");
  } else if (value == DPS_7E1) {
    (void)Print_string (" [7o1/7E1/8n1/8o1/8e1/8n2/8o2/8e2]: ");
  } else if (value == DPS_8N1) {
    (void)Print_string (" [7o1/7e1/8N1/8o1/8e1/8n2/8o2/8e2]: ");
  } else if (value == DPS_8O1) {
    (void)Print_string (" [7o1/7e1/8n1/8O1/8e1/8n2/8o2/8e2]: ");
  } else if (value == DPS_8E1) {
    (void)Print_string (" [7o1/7e1/8n1/8o1/8E1/8n2/8o2/8e2]: ");
  } else if (value == DPS_8N2) {
    (void)Print_string (" [7o1/7e1/8n1/8o1/8e1/8N2/8o2/8e2]: ");
  } else if (value == DPS_8O2) {
    (void)Print_string (" [7o1/7e1/8n1/8o1/8e1/8n2/8O2/8e2]: ");
  } else /* value == DPS_8E2 */ {
    (void)Print_string (" [7o1/7e1/8n1/8o1/8e1/8n2/8o2/8E2]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read response.
  while (TRUE) {
    chr = Read_setup_character_from_set ("78\r");
    if (chr == '\x90') {
      (void)Print_element (&WW_PRINT_UPARROW);
    } else if (chr == '\x91') {
      (void)Print_element (&WW_PRINT_DOWNARROW);
    } else {
      break;
    }
  }

  // Process response.
  if (chr == '7') {
    (void)Print_character ('7');
    chr = Read_setup_character_from_set ("oOeE");
    if ((chr == 'o') || (chr == 'O')) {
      (void)Print_string ("o1\r");
      return DPS_7O1;
    } else /* (chr == 'e') || (chr == 'E') */ {
      (void)Print_string ("e1\r");
      return DPS_7E1;
    }

  } else if (chr == '8') {
    (void)Print_character ('8');
    chr = Read_setup_character_from_set ("nNoOeE");
    if ((chr == 'n') || (chr == 'N')) {
      (void)Print_character ('n');
      chr = Read_setup_character_from_set ("12");
      if (chr == '1') {
        (void)Print_string ("1\r");
        return DPS_8N1;
      } else /* chr == '2' */ {
        (void)Print_string ("2\r");
        return DPS_8N2;
      }

    } else if ((chr == 'o') || (chr == 'O')) {
      (void)Print_character ('o');
      chr = Read_setup_character_from_set ("12");
      if (chr == '1') {
        (void)Print_string ("1\r");
        return DPS_8O1;
      } else /* chr == '2' */ {
        (void)Print_string ("2\r");
        return DPS_8O2;
      }

    } else /* (chr == 'e') || (chr == 'E') */ {
      (void)Print_character ('e');
      chr = Read_setup_character_from_set ("12");
      if (chr == '1') {
        (void)Print_string ("1\r");
        return DPS_8E1;
      } else /* chr == '2' */ {
        (void)Print_string ("2\r");
        return DPS_8E2;
      }
    }

  } else /* chr == '\r' */ {
    (void)Print_string (DATA_PARITY_STOPS_TEXT[value]);
    (void)Print_character ('\r');
    return value;
  }
}

// Read an impression setting.
byte Read_impression_setting (const char str[], byte value) {
  char chr;
  byte val;

  // Print prompt.
  (void)Print_string ("  ");  (void)Print_string (str);
  if (value == IMPRESSION_LIGHT) {
    (void)Print_string (" [LIGHT/normal/heavy]: ");
  } else if (value == IMPRESSION_NORMAL) {
    (void)Print_string (" [light/NORMAL/heavy]: ");
  } else /* value == IMPRESSION_HEAVY */ {
    (void)Print_string (" [light/normal/HEAVY]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read response.
  while (TRUE) {
    chr = Read_setup_character_from_set ("lLnNhH\x90\x91\r");
    if (chr == '\x90') {
      (void)Print_element (&WW_PRINT_UPARROW);
    } else if (chr == '\x91') {
      (void)Print_element (&WW_PRINT_DOWNARROW);
    } else {
      break;
    }
  }

  // Process response.
  if ((chr == 'l') || (chr == 'L')) {
    val = IMPRESSION_LIGHT;
  } else if ((chr == 'n') || (chr == 'N')) {
    val = IMPRESSION_NORMAL;
  } else if ((chr == 'h') || (chr == 'H')) {
    val = IMPRESSION_HEAVY;
  } else /* chr == '\r' */ {
    val = value;
  }

  // Print response.
  if (val == IMPRESSION_LIGHT) {
    (void)Print_string ("light\r");
  } else if (val == IMPRESSION_NORMAL) {
    (void)Print_string ("normal\r");
  } else /* val == IMPRESSION_HEAVY */ {
    (void)Print_string ("heavy\r");
  }

  return val;
}

// Read a software flow control setting.
byte Read_swflow_setting (const char str[], byte value) {
  char chr;
  byte val;

  // Print prompt.
  (void)Print_string ("  ");  (void)Print_string (str);
  if (value == SWFLOW_NONE) {
    (void)Print_string (" [NONE/xon_xoff]: ");
  } else /* value == SWFLOW_XON_XOFF */ {
    (void)Print_string (" [none/XON_XOFF]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read response.
  while (TRUE) {
    chr = Read_setup_character_from_set ("nNxX\x90\x91\r");
    if (chr == '\x90') {
      (void)Print_element (&WW_PRINT_UPARROW);
    } else if (chr == '\x91') {
      (void)Print_element (&WW_PRINT_DOWNARROW);
    } else {
      break;
    }
  }

  // Process response.
  if ((chr == 'n') || (chr == 'N')) {
    val = SWFLOW_NONE;
  } else if ((chr == 'x') || (chr == 'X')) {
    val = SWFLOW_XON_XOFF;
  } else /* chr == '\r' */ {
    val = value;
  }

  // Print response.
  if (val == SWFLOW_NONE) {
    (void)Print_string ("none\r");
  } else /* val == SWFLOW_XON_XOFF */ {
    (void)Print_string ("xon_xoff\r");
  }

  return val;
}

// Read a hardware flow control setting.
byte Read_hwflow_setting (const char str[], byte value) {
  char chr;

  // Print prompt.
  (void)Print_string ("  ");  (void)Print_string (str);
  if (value == HWFLOW_NONE) {
    (void)Print_string (" [NONE/rts_cts/rtr_cts]: ");
  } else if (value == HWFLOW_RTS_CTS) {
    (void)Print_string (" [none/RTS_CTS/rtr_cts]: ");
  } else /* value == HWFLOW_RTR_CTS */ {
    (void)Print_string (" [none/rts_cts/RTR_CTS]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read response.
  while (TRUE) {
    chr = Read_setup_character_from_set ("nNrR\x90\x91\r");
    if (chr == '\x90') {
      (void)Print_element (&WW_PRINT_UPARROW);
    } else if (chr == '\x91') {
      (void)Print_element (&WW_PRINT_DOWNARROW);
    } else {
      break;
    }
  }

  // Process response.
  if ((chr == 'n') || (chr == 'N')) {
    (void)Print_string ("none\r");
    return HWFLOW_NONE;
  } else if ((chr == 'r') || (chr == 'R')) {
    (void)Print_string ("rt");
    chr = Read_setup_character_from_set ("sSrR");
    if ((chr == 's') || (chr == 'S')) {
      (void)Print_string ("s_cts\r");
      return HWFLOW_RTS_CTS;
    } else /* (chr == 'r') || (chr == 'R') */ {
      (void)Print_string ("r_cts\r");
      return HWFLOW_RTR_CTS;
    }
  } else /* chr == '\r' */ {
    if (value == HWFLOW_NONE) {
      (void)Print_string ("none\r");
    } else if (value == HWFLOW_RTS_CTS) {
      (void)Print_string ("rts_cts\r");
    } else /* value == HWFLOW_RTR_CTS */ {
      (void)Print_string ("rtr_cts\r");
    }
    return value;
  }
}

// Read an end-of-line setting.
byte Read_eol_setting (const char str[], byte value) {
  char chr;

  // Print prompt.
  (void)Print_string ("  ");  (void)Print_string (str);
  if (value == EOL_CR) {
    (void)Print_string (" [CR/crlf/lf/lfcr]: ");
  } else if (value == EOL_CRLF) {
    (void)Print_string (" [cr/CRLF/lf/lfcr]: ");
  } else if (value == EOL_LF) {
    (void)Print_string (" [cr/crlf/LF/lfcr]: ");
  } else /* value == EOL_LFCR */ {
    (void)Print_string (" [cr/crlf/lf/LFCR]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read response.
  while (TRUE) {
    chr = Read_setup_character_from_set ("cClL\x90\x91\r");
    if (chr == '\x90') {
      (void)Print_element (&WW_PRINT_UPARROW);
    } else if (chr == '\x91') {
      (void)Print_element (&WW_PRINT_DOWNARROW);
    } else {
      break;
    }
  }

  // Process response.
  if ((chr == 'c') || (chr == 'C')) {
    (void)Print_string ("cr");
    chr = Read_setup_character_from_set ("lL\r");
    if ((chr == 'l') || (chr == 'L')) {
      (void)Print_string ("lf\r");
      return EOL_CRLF;
    } else /* chr == '\r' */ {
      (void)Print_string ("\r");
      return EOL_CR;
    }

  } else if ((chr == 'l') || (chr == 'L')) {
    (void)Print_string ("lf");
    chr = Read_setup_character_from_set ("cC\r");
    if ((chr == 'c') || (chr == 'C')) {
      (void)Print_string ("cr\r");
      return EOL_LFCR;
    } else /* chr == '\r' */ {
      (void)Print_string ("\r");
      return EOL_LF;
    }

  } else /* chr == '\r' */ {
    if (value == EOL_CR) {
      (void)Print_string ("cr\r");
    } else if (value == EOL_CRLF) {
      (void)Print_string ("crlf\r");
    } else if (value == EOL_LF) {
      (void)Print_string ("lf\r");
    } else /* value == EOL_LFCR */ {
      (void)Print_string ("lfcr\r");
    }
    return value;
  }
}

// Read an escape sequence setting.
byte Read_escapesequence_setting (const char str[], byte value) {
  char chr;
  byte val;

  // Print prompt.
  (void)Print_string ("  ");  (void)Print_string (str);
  if (value == ESCAPE_NONE) {
    (void)Print_string (" [NONE/ignore]: ");
  } else /* value == ESCAPE_IGNORE */ {
    (void)Print_string (" [none/IGNORE]: ");
  }
  Space_to_column (COLUMN_RESPONSE);

  // Read response.
  while (TRUE) {
    chr = Read_setup_character_from_set ("nNiI\x90\x91\r");
    if (chr == '\x90') {
      (void)Print_element (&WW_PRINT_UPARROW);
    } else if (chr == '\x91') {
      (void)Print_element (&WW_PRINT_DOWNARROW);
    } else {
      break;
    }
  }

  // Process response.
  if ((chr == 'n') || (chr == 'N')) {
    val = ESCAPE_NONE;
  } else if ((chr == 'i') || (chr == 'I')) {
    val = ESCAPE_IGNORE;
  } else /* chr == '\r' */ {
    val = value;
  }

  // Print response.
  if (val == ESCAPE_NONE) {
    (void)Print_string ("none\r");
  } else /* val == ESCAPE_IGNORE */ {
    (void)Print_string ("ignore\r");
  }

  return val;
}

// Read an integer setting.
byte Read_integer_setting (const char str[], byte value, byte min, byte max) {

  while (TRUE) {

    // Print prompt.
    (void)Print_string ("  ");  (void)Print_string (str);
    (void)Print_string (" [");  (void)Print_integer (min, 0);  (void)Print_string ("-");  (void)Print_integer (max, 0);
    (void)Print_string (", ");  (void)Print_integer (value, 0);  (void)Print_string ("]: ");
    Space_to_column (COLUMN_RESPONSE);

    int val = Read_integer (value);
    if ((val >= min) && (val <= max)) {
      (void)Print_string ("\r");
      return (byte)val;
    }

    (void)Print_string ("\a invalid\r");
  }
}

// Developer functions.
void Developer_functions (void) {

  run_mode = MODE_RUNNING;
  (void)Print_element (&WW_PRINT_CRtn);

  while (TRUE) {
    char cmd = Read_developer_function ();
    if (cmd == 'b') {
      Measure_buffer_size ();
    } else if (cmd == 'c') {
      Check_communication ();
    } else if (cmd == 'k') {
      Measure_keyboard_timing ();
    } else if (cmd == 'm') {
      Measure_print_speed ();
    } else if (cmd == 'p') {
      Measure_printer_timing ();
    } else if (cmd == 's') {
      Measure_column_scanning ();
    } else /* cmd == 'e' */ {
      break;
    }
  }

  (void)Print_element (&WW_PRINT_CRtn);
}

// Measure buffer size.
void Measure_buffer_size (void) {
  byte ar;
  byte len;
  int cnt;
  int scan;
  unsigned long time;

  // Temporarily increase length and disable auto return.
  len = length;
  length = 200;
  ar = autoreturn;
  autoreturn = SETTING_FALSE;

  // Position carriage as far to the left as possible.
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);
  in_margin_tab_setting = TRUE;
  (void)Print_element (&WW_PRINT_MarRelX);
  for (int i = 0; i < 20; ++i) (void)Print_element (&WW_PRINT_Backspace);
  in_margin_tab_setting = FALSE;
  Wait_print_buffer_empty (1000);

  // Measure typewriter buffer size.
  time = Measure_print_timing (NULL_PRINT_INFO, &WW_PRINT_PERIOD, &WW_PRINT_PERIOD, 0, 0, 0, length, FALSE, FALSE, &cnt,
                               &scan);

  // Restore length and auto return.
  length = len;
  autoreturn = ar;

  // Print buffer size.
  (void)Print_element (&WW_PRINT_CRtn);
  if (scan >= (int)(1000UL * LONG_SCAN_DURATION)) {
    (void)Print_string ("buffer size: ");
    buffer_size = (int)((scan * cnt + time / 2UL) / time);
    (void)Print_integer (buffer_size, 0);
  } else {
    (void)Print_string("Unable to measure buffer size.");
  }
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);

  (void)Print_element (&WW_PRINT_CRtn);
}

// Check communication.
void Check_communication (void) {
  (void)Print_string ("\r    Not implemented yet.\r\r");
}

// Measure keyboard timing.
void Measure_keyboard_timing (void) {
  (void)Print_string ("\r    Not implemented yet.\r\r");
}

// Measure print speed.
void Measure_print_speed (void) {
  unsigned long start;
  unsigned long time;
  int cps;

  // Measure print speed with calibrated timing values.
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_string ("The following two tests measure effective print speed with calibrated timing values.\r"
                      "There should be no triple beeps, only near-end-of-line single beeps with the full lines of "
                      "random characters.");
  (void)Print_element (&WW_PRINT_CRtn);

  // Check average text.
  Wait_print_buffer_empty (1000);
  start = millis ();
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_string (TEST_PRINT_STRING);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (0);
  time = millis () - start;
  Wait_print_buffer_empty (1000);
  cps = (int)((10000UL * (NUM_TEST_PRINT_STRING + 2UL) + time / 2UL) / time);
  (void)Print_string ("speed: ");
  (void)Print_scaled_integer (cps, 1, 0);
  Print_string (", ");
  Print_integer (NUM_TEST_PRINT_STRING + 2, 0);
  Print_string (", ");
  Print_integer ((int)time, 0);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);

  // Check random characters.
  Wait_print_buffer_empty (1000);
  start = millis ();
  (void)Print_element (&WW_PRINT_CRtn);
  for (int i = 0; i < (10 * length); ++i) {
    (void)Print_element (PRINT_CHARACTERS[random (NUM_PRINT_CHARACTERS)]);
  }
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (0);
  time = millis () - start;
  Wait_print_buffer_empty (1000);
  cps = (int)((10000UL * (10UL * (length + 1UL) + 2UL) + time / 2UL) / time);
  (void)Print_string ("speed: ");
  (void)Print_scaled_integer (cps, 1, 0);
  Print_string (", ");
  Print_integer (10 * (length + 1) + 2, 0);
  Print_string (", ");
  Print_integer ((int)time, 0);
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);

  // Measure maximum printer speed.
  (void)Print_string ("The following two tests measure print speed with characters presented to the motherboard at the "
                      "fastest rate possible.\r"
                      "There will be many triple and single beeps.");
  (void)Print_element (&WW_PRINT_CRtn);
  in_full_speed_test = TRUE;
  count_characters = TRUE;

  // Check average text.
  Wait_print_buffer_empty (1000);
  print_character_count = 0;
  last_character_count = 0;
  start_print_time = millis ();
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_string (TEST_PRINT_STRING);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (0);
  time = millis () - start_print_time;
  Wait_print_buffer_empty (1000);
  if (last_character_count > 0) {
    cps = (int)((10000UL * last_character_count + last_print_time / 2UL) / last_print_time);
  } else {
    cps = (int)((10000UL * (NUM_TEST_PRINT_STRING + 2UL) + time / 2UL) / time);
  }
  (void)Print_string ("speed: ");
  (void)Print_scaled_integer (cps, 1, 0);
  if (last_character_count > 0) {
    Print_string (", ");
    Print_integer (last_character_count, 0);
    Print_string (", ");
    Print_integer (last_print_time, 0);
    Print_string (", ");
    Print_integer ((int)time, 0);
  } else {
    Print_string (", ");
    Print_integer (NUM_TEST_PRINT_STRING + 2, 0);
    Print_string (", ");
    Print_integer ((int)time, 0);
  }
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);

  // Check random characters.
  Wait_print_buffer_empty (1000);
  print_character_count = 0;
  last_character_count = 0;
  start_print_time = millis ();
  (void)Print_element (&WW_PRINT_CRtn);
  for (int i = 0; i < (10 * length); ++i) {
    (void)Print_element (PRINT_CHARACTERS[random (NUM_PRINT_CHARACTERS)]);
  }
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (0);
  time = millis () - start_print_time;
  Wait_print_buffer_empty (1000);
  if (last_character_count > 0) {
    cps = (int)((10000UL * last_character_count + last_print_time / 2UL) / last_print_time);
  } else {
    cps = (int)((10000UL * (10UL * (length + 1UL) + 2UL) + time / 2UL) / time);
  }
  (void)Print_string ("speed: ");
  (void)Print_scaled_integer (cps, 1, 0);
  if (last_character_count > 0) {
    Print_string (", ");
    Print_integer (last_character_count, 0);
    Print_string (", ");
    Print_integer (last_print_time, 0);
    Print_string (", ");
    Print_integer ((int)time, 0);
  } else {
    Print_string (", ");
    Print_integer (10 * (length + 1) + 2, 0);
    Print_string (", ");
    Print_integer ((int)time, 0);
  }
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);
  in_full_speed_test = FALSE;
  count_characters = FALSE;

  (void)Print_element (&WW_PRINT_CRtn);
}

// Measure printer timing.
void Measure_printer_timing (void) {
  int col;
  int cnt;
  int bcnt;
  int scan;
  unsigned long time;
  unsigned long btime;

  // Measure null print timing.
  time = Measure_print_timing (NULL_PRINT_INFO, &WW_PRINT_LShift, &WW_PRINT_LShift, 0, 0, 0, length, FALSE, FALSE, &cnt,
                               NULL);
  (void)Print_string ("TIME_NULL: ");
  (void)Print_integer ((int)(time / cnt), 0);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (3000);

  // Measure print timing components.
  Measure_shm_timing ("TIME_MOVE", NULL, &WW_PRINT_SPACE, &WW_PRINT_SPACE,
                      TIME_PRESS_NOSHIFT_4 + TIME_RELEASE_NOSHIFT_4,
                      TIME_PRESS_NOSHIFT_4 + TIME_RELEASE_NOSHIFT_4, length);
  Measure_shm_timing ("TIME_HIT_MOVE", NULL, &WW_PRINT_a, &WW_PRINT_a,
                      TIME_PRESS_NOSHIFT_3 + TIME_RELEASE_NOSHIFT_3,
                      TIME_PRESS_NOSHIFT_3 + TIME_RELEASE_NOSHIFT_3, length);
  Measure_shm_timing ("TIME_SPIN(12)_HIT_MOVE", NULL, &WW_PRINT_e, &WW_PRINT_z,
                      TIME_PRESS_NOSHIFT_7 + TIME_RELEASE_NOSHIFT_7,
                      TIME_PRESS_NOSHIFT_3 + TIME_RELEASE_NOSHIFT_3, length);
  Measure_shm_timing ("TIME_SPIN(24)_HIT_MOVE", NULL, &WW_PRINT_l, &WW_PRINT_x,
                      TIME_PRESS_NOSHIFT_11 + TIME_RELEASE_NOSHIFT_11,
                      TIME_PRESS_NOSHIFT_6 + TIME_RELEASE_NOSHIFT_6, length);
  Measure_shm_timing ("TIME_SPIN(36)_HIT_MOVE", NULL, &WW_PRINT_v, &WW_PRINT_2,
                      TIME_PRESS_NOSHIFT_8 + TIME_RELEASE_NOSHIFT_8,
                      TIME_PRESS_NOSHIFT_6 + TIME_RELEASE_NOSHIFT_6, length);
  Measure_shm_timing ("TIME_SPIN(48)_HIT_MOVE", NULL, &WW_PRINT_m, &WW_PRINT_8,
                      TIME_PRESS_NOSHIFT_9 + TIME_RELEASE_NOSHIFT_9,
                      TIME_PRESS_NOSHIFT_10 + TIME_RELEASE_NOSHIFT_10, length);

  // Measure baseline time.
  btime = Measure_tab_return_timing ("TIME_BASE", 0, FALSE, 0UL, &bcnt);

  // Temporarily set tabs for measuring tab, carriage return, and carriage movement timing.
  (void)Print_element (&WW_PRINT_TClrAllX);
  (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&WW_PRINT_TSet);
  for (col = 2; col < ((length - bcnt) / 4); ++col) (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&WW_PRINT_TSet);
  for (; col < ((length - bcnt) / 2); ++col) (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&WW_PRINT_TSet);
  for (; col < (length - bcnt); ++col) (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&WW_PRINT_TSet);
  for (; col < length; ++col) (void)Print_element (&WW_PRINT_SPACE);
  (void)Print_element (&WW_PRINT_TSet);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (10000);

  // Measure tab and carriage return timing.
  (void)Measure_tab_return_timing ("TIME_TAB", 1, FALSE, btime, NULL);
  (void)Measure_tab_return_timing ("TIME_RETURN", 3, TRUE, btime, NULL);
  (void)Print_element (&WW_PRINT_Tab);
  (void)Print_element (&WW_PRINT_TClr);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);
  (void)Measure_tab_return_timing ("TIME_TAB", ((length - bcnt) / 4) - 1, FALSE, btime, NULL);
  (void)Measure_tab_return_timing ("TIME_RETURN", ((length - bcnt) / 4) + 1, TRUE, btime, NULL);
  (void)Print_element (&WW_PRINT_Tab);
  (void)Print_element (&WW_PRINT_TClr);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (2000);
  (void)Measure_tab_return_timing ("TIME_TAB", ((length - bcnt) / 2) - 1, FALSE, btime, NULL);
  (void)Measure_tab_return_timing ("TIME_RETURN", ((length - bcnt) / 2) + 1, TRUE, btime, NULL);
  (void)Print_element (&WW_PRINT_Tab);
  (void)Print_element (&WW_PRINT_TClr);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (4000);
  (void)Measure_tab_return_timing ("TIME_TAB", length - bcnt - 1, FALSE, btime, NULL);
  (void)Measure_tab_return_timing ("TIME_RETURN", length - bcnt + 1, TRUE, btime, NULL);
  (void)Print_element (&WW_PRINT_Tab);
  (void)Print_element (&WW_PRINT_TClr);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (8000);
  (void)Measure_tab_return_timing ("TIME_RETURN", length + 1, TRUE, btime, NULL);

  // Measure carriage movement timing.
  Measure_shm_timing ("TIME_SPACE", NULL, &WW_PRINT_SPACE, &WW_PRINT_SPACE,
                      TIME_PRESS_NOSHIFT_4 + TIME_RELEASE_NOSHIFT_4,
                      TIME_PRESS_NOSHIFT_4 + TIME_RELEASE_NOSHIFT_4, length);
  Measure_shm_timing ("TIME_BACKSPACE", &WW_PRINT_Tab, &WW_PRINT_Backspace, &WW_PRINT_Backspace,
                      TIME_PRESS_NOSHIFT_13 + TIME_RELEASE_NOSHIFT_13,
                      TIME_PRESS_NOSHIFT_13 + TIME_RELEASE_NOSHIFT_13, length);
  Measure_shm_timing ("TIME_BKSP1", &WW_PRINT_Tab, &WW_PRINT_Bksp1, &WW_PRINT_Bksp1,
                      TIME_PRESS_CODE_13 + TIME_RELEASE_CODE_13,
                      TIME_PRESS_CODE_13 + TIME_RELEASE_CODE_13, 5 * length);
  Measure_shm_timing ("TIME_PAPER_UP_DOWN", NULL, &WW_PRINT_PaperUp, &WW_PRINT_PaperDown,
                      TIME_PRESS_NOSHIFT_2 + TIME_RELEASE_NOSHIFT_2,
                      TIME_PRESS_NOSHIFT_2 + TIME_RELEASE_NOSHIFT_2, length);
  Measure_shm_timing ("TIME_UP_DOWN_ARROW", NULL, &WW_PRINT_UPARROW, &WW_PRINT_DOWNARROW,
                      TIME_PRESS_NOSHIFT_13 + TIME_RELEASE_NOSHIFT_13,
                      TIME_PRESS_NOSHIFT_2 + TIME_RELEASE_NOSHIFT_2, length);
  Measure_shm_timing ("TIME_UP_DOWN_MICRO", NULL, &WW_PRINT_UpMicro, &WW_PRINT_DownMicro,
                      TIME_PRESS_CODE_2 + TIME_RELEASE_CODE_2,
                      TIME_PRESS_CODE_2 + TIME_RELEASE_CODE_2, length);

  // Measure beep timing.
  time = Measure_print_timing (NULL_PRINT_INFO, &WW_PRINT_BEEP, &WW_PRINT_BEEP, 0, 0, 0, length, FALSE, FALSE, &cnt,
                               &scan);
  (void)Print_string ("TIME_BEEP: ");
  (void)Print_integer (max ((int)(time / cnt), (int)(scan / buffer_size)), 0);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (2000);

  // Measure printwheel jiggle timing.
  time = Measure_print_timing (NULL_PRINT_INFO, &WW_PRINT_TSet, &WW_PRINT_TClr, 0, 0, 0, length, FALSE, FALSE, &cnt,
                               &scan);
  (void)Print_element (&WW_PRINT_TClr);
  (void)Print_string ("TIME_JIGGLE: ");
  (void)Print_integer (max ((int)(time / cnt), (int)(scan / buffer_size)), 0);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (2000);

/* DJB - Not ready for release yet.
  // Check ASCII Period Graphic characters.
  Print_composite ("CHECK_ASCII_LESS_PG", &ASCII_PRINT_LESS_PG, length);
  Print_composite ("CHECK_ASCII_GREATER_PG", &ASCII_PRINT_GREATER_PG, length);
  Print_composite ("CHECK_ASCII_BSLASH_PG", &ASCII_PRINT_BSLASH_PG, length);
  Print_composite ("CHECK_ASCII_CARET_PG", &ASCII_PRINT_CARET_PG, length);
  Print_composite ("CHECK_ASCII_BAPOSTROPHE_PG", &ASCII_PRINT_BAPOSTROPHE_PG, length);
  Print_composite ("CHECK_ASCII_LBRACE_PG", &ASCII_PRINT_LBRACE_PG, length);
  Print_composite ("CHECK_ASCII_BAR_PG", &ASCII_PRINT_BAR_PG, length);
  Print_composite ("CHECK_ASCII_RBRACE_PG", &ASCII_PRINT_RBRACE_PG, length);
  Print_composite ("CHECK_ASCII_TILDE_PG", &ASCII_PRINT_TILDE_PG, length);

  // Check IBM 1620 composite characters.
  Print_composite ("CHECK_IBM_SLASH_0", &IBM_PRINT_SLASH_0, length);
  Print_composite ("CHECK_IBM_FLAG_SLASH_0", &IBM_PRINT_FLAG_SLASH_0, length);
  Print_composite ("CHECK_IBM_FLAG_DIGIT", &IBM_PRINT_FLAG_1, length);
  Print_composite ("CHECK_IBM_FLAG_NUMBLANK", &IBM_PRINT_FLAG_NUMBLANK, length);
  Print_composite ("CHECK_IBM_RMARK", &IBM_PRINT_RMARK_IPW, length);
  Print_composite ("CHECK_IBM_FLAG_RMARK", &IBM_PRINT_FLAG_RMARK_IPW, length);
  Print_composite ("CHECK_IBM_GMARK", &IBM_PRINT_GMARK_IPW, length);
  Print_composite ("CHECK_IBM_FLAG_GMARK", &IBM_PRINT_FLAG_GMARK_IPW, length);
  Print_composite ("CHECK_IBM_RELEASESTART", &IBM_PRINT_RELEASESTART, length);
  Print_composite ("CHECK_IBM_INVALID", &IBM_PRINT_INVALID, length);
*/

  // Restore tabs and margins.
  Set_margins_tabs (FALSE);

  (void)Print_element (&WW_PRINT_CRtn);
}

// Measure column scanning.
void Measure_column_scanning (void) {

  // Wait for the print buffer to be empty and printing finished.
  Wait_print_buffer_empty (3000);

  // Initiate keyboard column scan time measurement.
  column_scan_count = 0;
  last_interrupt_time_microseconds = 0UL;
  total_scan_time = 0UL;
  minimum_scan_time = 0xFFFFFFFFUL;
  maximum_scan_time = 0UL;

  // Wait for 1000 column scans.
  in_column_scan_timing = TRUE;
  while (column_scan_count < 1000) delayMicroseconds (10);
  in_column_scan_timing = FALSE;
  delay (100);

  // Print keyboard column scan time measurements.
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_string ("empty column scan (min): ");
  (void)Print_integer ((int)minimum_scan_time, 4);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);
  (void)Print_string ("empty column scan (avg): ");
  (void)Print_integer ((int)(total_scan_time / column_scan_count), 4);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);
  (void)Print_string ("empty column scan (max): ");
  (void)Print_integer ((int)maximum_scan_time, 4);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);

  // Initiate key press and release column scan time measurements.
  total_key_pressed_scan_time = 0UL;
  minimum_key_pressed_scan_time = 0xFFFFFFFFUL;
  maximum_key_pressed_scan_time = 0UL;
  total_key_released_scan_time = 0UL;
  minimum_key_released_scan_time = 0xFFFFFFFFUL;
  maximum_key_released_scan_time = 0UL;

  // Press, hold, and release space key and measure.
  for (int i = 0; i < length; ++i) {
    column_scan_count = 0;
    last_interrupt_time_microseconds = 0UL;
    total_scan_time = 0UL;
    minimum_scan_time = 0xFFFFFFFFUL;
    maximum_scan_time = 0UL;
    in_column_scan_timing = TRUE;
    delay (100);
    press_space_key = TRUE;
    delay (100);
    total_key_pressed_scan_time += maximum_scan_time;
    minimum_key_pressed_scan_time = min (minimum_key_pressed_scan_time, maximum_scan_time);
    maximum_key_pressed_scan_time = max (maximum_key_pressed_scan_time, maximum_scan_time);
    maximum_scan_time = 0UL;
    press_space_key = FALSE;
    delay (100);
    in_column_scan_timing = FALSE;
    delay (100);
    total_key_released_scan_time += maximum_scan_time;
    minimum_key_released_scan_time = min (minimum_key_released_scan_time, maximum_scan_time);
    maximum_key_released_scan_time = max (maximum_key_released_scan_time, maximum_scan_time);
  }
  
  // Print key press and release column scan time measurements.
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_string ("key pressed column scan (min): ");
  (void)Print_integer ((int)minimum_key_pressed_scan_time, 5);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);
  (void)Print_string ("key pressed column scan (avg): ");
  (void)Print_integer ((int)(total_key_pressed_scan_time / length), 5);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);
  (void)Print_string ("key pressed column scan (max): ");
  (void)Print_integer ((int)maximum_key_pressed_scan_time, 5);
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);
  (void)Print_string ("key released column scan (min): ");
  (void)Print_integer ((int)minimum_key_released_scan_time, 5);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);
  (void)Print_string ("key released column scan (avg): ");
  (void)Print_integer ((int)(total_key_released_scan_time / length), 5);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);
  (void)Print_string ("key released column scan (max): ");
  (void)Print_integer ((int)maximum_key_released_scan_time, 5);
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);

  // Initiate flood column scan time measurements.
  column_scan_count = 0;
  last_interrupt_time_microseconds = 0UL;
  total_scan_time = 0UL;
  minimum_scan_time = 0xFFFFFFFFUL;
  maximum_scan_time = 0UL;
  in_column_scan_timing = TRUE;
  delay (100);

  // Flood typewriter with 'a' & '5' keys at maximum speed.
  flood_key_count = 3 * (length / 2);
  flood_a_5_keys = TRUE;
 
  // Wait until flood printing is complete.
  while (flood_key_count > 0) delayMicroseconds (10);
  Wait_print_buffer_empty (3000);
  flood_key_scan_time = maximum_scan_time;
  in_column_scan_timing = FALSE;
  flood_a_5_keys = FALSE;
  delay (100);

  // Print flood column scan time measurement.
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_element (&WW_PRINT_CRtn);
  if (flood_key_scan_time < 100000UL) {
    (void)Print_string("Unable to measure flood column scan.");
  } else {
    (void)Print_string ("flood column scan: ");
    (void)Print_integer ((int)flood_key_scan_time, 0);
  }
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);

  (void)Print_element (&WW_PRINT_CRtn);
}

// Read a developer function character.
char Read_developer_function (void) {
  char func;

  // Print function prompt.
  (void)Print_string ("  Function [buffer/comm/keyboard/measure/print/scan/EXIT]: ");
  Space_to_column (COLUMN_FUNCTION);

  // Read a function character.
  while (TRUE) {
    func = Read_setup_character_from_set ("bBcCkKmMpPsSeE\x90\x91\r");
    if (func == '\x90') {
      (void)Print_element (&WW_PRINT_UPARROW);
    } else if (func == '\x91') {
      (void)Print_element (&WW_PRINT_DOWNARROW);
    } else {
      break;
    }
  }

  // Validate and return developer function.
  if ((func == 'b') || (func == 'B')) {
    (void)Print_string ("buffer\r");
    return 'b';
  } else if ((func == 'c') || (func == 'C')) {
    (void)Print_string ("comm\r");
    return 'c';
  } else if ((func == 'k') || (func == 'K')) {
    (void)Print_string ("keyboard\r");
    return 'k';
  } else if ((func == 'm') || (func == 'M')) {
    (void)Print_string ("measure\r");
    return 'm';
  } else if ((func == 'p') || (func == 'P')) {
    (void)Print_string ("print\r");
    return 'p';
  } else if ((func == 's') || (func == 'S')) {
    (void)Print_string ("scan\r");
    return 's';
  } else /* (func == 'e') || (func == 'E') || (func == '\r') */ {
    (void)Print_string ("exit\r");
    return 'e';
  }
}

// Print composite character.
void Print_composite (const char *name, const struct print_info *str, int len) {

/*
  // Print title.
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_string (name);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);
*/

  // Measure and print composite timing.
  (void)Measure_composite_timing (str, 0, len);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);
}

// Measure and optionally print timing of spin, hit, and move.
void Measure_shm_timing (const char *name, const struct print_info *str1, const struct print_info *str2,
                         const struct print_info *str3, int fill2, int fill3, int len) {
  int base = max (fill2, fill3);
  int pad2 = base - fill2;
  int pad3 = base - fill3;
  int pad;
  unsigned long time;
  int cnt;
  int scan;
  unsigned long ltime;
  int lcnt;

  // Print title.
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_string (name);
  (void)Print_string (", ");
  (void)Print_integer (fill2, 0);
  (void)Print_string (", ");
  (void)Print_integer (fill3, 0);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (4000);

  // Measure and print timing.
  if (str1 != NULL_PRINT_INFO) {
    (void)Print_element (str1);
    Wait_print_buffer_empty (6000);
  }
  ltime = Measure_print_timing (NULL_PRINT_INFO, str2, str3, 0, pad2, pad3, len, TRUE, FALSE, &lcnt, NULL);
  Wait_print_buffer_empty (6000);
  if (str1 != NULL_PRINT_INFO) {
    (void)Print_element (str1);
    Wait_print_buffer_empty (6000);
  }
  time = Measure_print_timing (NULL_PRINT_INFO, str2, str3, 0, pad2 + 4000, pad3 + 4000, len, TRUE, FALSE, &cnt, &scan);
  Wait_print_buffer_empty (6000);
  if (cnt > (len - 10)) {
    pad = 5000;
  } else {
    pad = max (1000 * ((2 * (len - lcnt - 10)) / (cnt - lcnt)), 5000);
  }
  while (scan != 0) {
    ltime = time;
    lcnt = cnt;
    if (str1 != NULL_PRINT_INFO) {
      (void)Print_element (str1);
      Wait_print_buffer_empty (6000);
    }
    time = Measure_print_timing (NULL_PRINT_INFO, str2, str3, 0, pad2 + pad, pad3 + pad, len, TRUE, FALSE, &cnt, &scan);
    Wait_print_buffer_empty (6000);
    pad += 1000;
  }
  if (str1 != NULL_PRINT_INFO) {
    (void)Print_element (str1);
    Wait_print_buffer_empty (6000);
  }
  (void)Measure_print_timing (NULL_PRINT_INFO, str2, str3, 0, (int)((ltime / lcnt) - fill2),
                              (int)((ltime / lcnt) - fill3), len, TRUE, TRUE, NULL, NULL);
  Wait_print_buffer_empty (6000);

  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);
}

// Measure and print timing of tabs and carriage returns.
unsigned long Measure_tab_return_timing (const char *name, int len, boolean ret, unsigned long btime, int *pcnt) {
  unsigned long time;
  unsigned long ttime;
  unsigned long rtime;
  int cnt;

  // Print title.
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_string (name);
  if (len != 0) {
    (void)Print_string ("(");
    (void)Print_integer (len, 0);
    (void)Print_string (")");
  }
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (3000);

  // Make five measurements and average.
  ttime = 0UL;
  for (int i = 0; i < 5; ++i) {
    if (ret) {
      (void)Print_element (&WW_PRINT_Tab);
      Wait_print_buffer_empty (3000);
      time = Measure_print_timing (&WW_PRINT_CRtn, &WW_PRINT_m, &WW_PRINT_8, 0, 0, 0, length, FALSE, FALSE, &cnt, NULL);
    } else if (btime != 0UL) {
      time = Measure_print_timing (&WW_PRINT_Tab, &WW_PRINT_m, &WW_PRINT_8, 0, 0, 0, length, FALSE, FALSE, &cnt, NULL);
    } else {
      time = Measure_print_timing (NULL_PRINT_INFO, &WW_PRINT_m, &WW_PRINT_8, 0, 0, 0, length, FALSE, FALSE, &cnt,
                                   NULL);
    }
    ttime += time;
  }
  if (btime == 0UL) {
    rtime = ttime / 5UL / cnt;
  } else {
    rtime = (ttime / 5UL) - (cnt * btime);
  }

  (void)Print_string ("time: ");
  (void)Print_integer ((int)rtime, 0);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (2000);

  // Return results.
  if (pcnt != NULL) *pcnt = cnt;
  return rtime;
}

// Measure timing of a print sequence.
unsigned long Measure_print_timing (const struct print_info *str1, const struct print_info *str2,
                                    const struct print_info *str3, int pad1, int pad2, int pad3, int len, boolean print,
                                    boolean force, int *pcnt, int *pscan) {
  struct print_info tstr1;
  struct print_info tstr2;
  struct print_info tstr3;
  byte warn;
  int begin;
  unsigned long start;
  unsigned long end;
  unsigned long time;
  int cnt;
  int scan;
  int cps;

  // Initialize variables.
  if (str1 != NULL_PRINT_INFO) {
    tstr1 = *str1;
    if (tstr1.spacing == SPACING_TAB) tstr1.spacing = SPACING_TABX;
    if (tstr1.spacing == SPACING_RETURN) tstr1.spacing = SPACING_RETURNX;
    tstr1.position = POSITION_NOCHANGE;
    tstr1.timing = pad1;
  }
  tstr2 = *str2;
  if (tstr2.spacing == SPACING_TAB) tstr2.spacing = SPACING_TABX;
  if (tstr2.spacing == SPACING_RETURN) tstr2.spacing = SPACING_RETURNX;
  tstr2.position = POSITION_NOCHANGE;
  tstr2.timing = pad2;
  tstr3 = *str3;
  if (tstr3.spacing == SPACING_TAB) tstr3.spacing = SPACING_TABX;
  if (tstr3.spacing == SPACING_RETURN) tstr3.spacing = SPACING_RETURNX;
  tstr3.position = POSITION_NOCHANGE;
  tstr3.timing = pad3;
  warn = warnings;
  warnings = SETTING_TRUE;
  Reset_warnings ();

  // Set initial position of printwheel.
  (void)Print_element ((const struct print_info *)(&tstr2));
  Wait_print_buffer_empty (1000);

  // Load print string into buffer.
  begin = pb_write;
  if (++pb_write >= SIZE_PRINT_BUFFER) pb_write = 0;
  print_buffer[pb_write] = WW_NULL;
  Update_counter (&pb_count, +1);
  residual_time = 0;
  print_character_count = 0;
  count_characters = TRUE;
  if (str1 != NULL_PRINT_INFO) {
    (void)Print_element ((const struct print_info *)(&tstr1));
  }
  for (int i = 1; i <  len; ++i) {
    if ((i % 2) == 0) {
      (void)Print_element ((const struct print_info *)(&tstr2));
    } else {
      (void)Print_element ((const struct print_info *)(&tstr3));
    }
  }
  count_characters = FALSE;

  // Print string.
  print_character_count = 0;
  longest_scan_duration = 0UL;
  in_printer_timing = TRUE;
  start = micros ();
  print_buffer[begin] = WW_SKIP;
  while (pb_count > 0) delay (1);
  end = micros ();
  time = end - start;
  if (end < start) time = ~time + 1;
  in_printer_timing = FALSE;
  Wait_print_buffer_empty (6000);
  cnt = print_character_count;
  scan = 1000UL * longest_scan_duration;
  longest_scan_duration = 0UL;
  digitalWriteFast (BLUE_LED_PIN, blue_led_off);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (3000);

  // Print timing results.
  if (print) {
    (void)Print_integer (pad1, 0);
    (void)Print_string (", ");
    (void)Print_integer (pad2, 0);
    (void)Print_string (", ");
    (void)Print_integer (pad3, 0);
    (void)Print_string (", ");
    (void)Print_integer (cnt, 0);
    (void)Print_string (", ");
    (void)Print_integer ((int)(time / cnt), 0);
    (void)Print_string (", ");
    (void)Print_integer (scan / buffer_size, 0);
    if ((scan != 0) || force) {
      (void)Print_string (", ");
      cps = (int)((10000000UL * cnt + time / 2UL) / time);
      (void)Print_scaled_integer (cps, 1, 0);
    }
    (void)Print_element (&WW_PRINT_CRtn);
    Wait_print_buffer_empty (3000);
  }

  // Reset warnings.
  warnings = warn;
  Reset_warnings ();

  // Return results.
  if (pcnt != NULL) *pcnt = cnt;
  if (pscan != NULL) *pscan = scan;
  return time;
}

// Measure timing of a composite character.
unsigned long Measure_composite_timing (const struct print_info *str, int pad, int len) {
  byte warn;
  int begin;
  unsigned long start;
  unsigned long end;
  unsigned long time;
  int cnt;
  int scan;

  // Initialize variables.
  warn = warnings;
  warnings = SETTING_TRUE;
  Reset_warnings ();

  // Load composite characters into buffer.
  begin = pb_write;
  if (++pb_write >= SIZE_PRINT_BUFFER) pb_write = 0;
  print_buffer[pb_write] = WW_NULL;
  Update_counter (&pb_count, +1);
  residual_time = 0;
  count_characters = TRUE;
  for (int i = 1; i <  len; ++i) {
    (void)Print_element (str);
  }
  count_characters = FALSE;

  // Print characters.
  print_character_count = 0;
  longest_scan_duration = 0UL;
  in_printer_timing = TRUE;
  start = micros ();
  print_buffer[begin] = WW_SKIP;
  while (pb_count > 0) delay (1);
  end = micros ();
  time = end - start;
  if (end < start) time = ~time + 1;
  in_printer_timing = FALSE;
  Wait_print_buffer_empty (6000);
  cnt = print_character_count;
  scan = 1000 * longest_scan_duration;
  longest_scan_duration = 0UL;
  digitalWriteFast (BLUE_LED_PIN, blue_led_off);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (3000);

  // Print timing results.
  (void)Print_integer (pad, 0);
  (void)Print_string (", ");
  (void)Print_integer (cnt, 0);
  (void)Print_string (", ");
  (void)Print_integer ((int)(time / cnt), 0);
  (void)Print_string (", ");
  (void)Print_integer (scan / 32, 0);
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (3000);

  // Restore variables.
  warnings = warn;
  Reset_warnings ();

  // Return results.
  return time;
}

// Reset all errors.
inline void Reset_errors (void) {
  total_errors = 0;
  bad_print_code = 0;
  memset ((void *)error_counts, 0, sizeof(error_counts));
  digitalWriteFast (ORANGE_LED_PIN, LOW);
}

// Reset all warnings.
inline void Reset_warnings (void) {
  total_warnings = 0;
  shortest_scan_duration = 0UL;
  longest_scan_duration = 0UL;
  memset ((void *)warning_counts, 0, sizeof(warning_counts));
  digitalWriteFast (BLUE_LED_PIN, blue_led_off);
}

// Print errors and warnings.
void Print_errors_warnings (void) {

  // Handle no errors or warnings.
  if ((total_errors == 0) && (total_warnings == 0)) {
    (void)Print_string ("\r  No errors or warnings.\r\r");
    return;
  }

  // Print errors.
  (void)Print_element (&WW_PRINT_CRtn);
  if (total_errors > 0) {
    for (int i = 0; i < NUM_ERRORS; ++i) {
      if (error_counts[i] > 0) {
        (void)Print_integer (error_counts[i], 8);
        (void)Print_element (&WW_PRINT_SPACE);
        (void)Print_string (ERROR_TEXT[i]);
        (void)Print_element (&WW_PRINT_CRtn);
        if (i == ERROR_BAD_CODE) {
          (void)Print_string ("         bad print code: ");
          (void)Print_integer ((int)bad_print_code, 0);
          (void)Print_element (&WW_PRINT_CRtn);
        }
      }
    }
    if (Ask_yesno_question ("\rReset errors", FALSE)) {
      Reset_errors ();
    }
  } else {
    (void)Print_string ("  No errors.\r");
  }

  // Print warnings.
  (void)Print_element (&WW_PRINT_CRtn);
  if (total_warnings > 0) {
    for (int i = 0; i < NUM_WARNINGS; ++i) {
      if (warning_counts[i] > 0) {
        (void)Print_integer (warning_counts[i], 8);
        (void)Print_element (&WW_PRINT_SPACE);
        (void)Print_string (WARNING_TEXT[i]);
        (void)Print_element (&WW_PRINT_CRtn);
        if (i == WARNING_SHORT_SCAN) {
          (void)Print_string ("         shortest scan duration: ");
          (void)Print_integer ((int)shortest_scan_duration, 0);
          (void)Print_element (&WW_PRINT_CRtn);
        }
        if (i == WARNING_LONG_SCAN) {
          (void)Print_string ("         longest scan duration: ");
          (void)Print_integer ((int)longest_scan_duration, 0);
          (void)Print_element (&WW_PRINT_CRtn);
        }
      }
    }
    if (Ask_yesno_question ("\rReset warnings", FALSE)) {
      Reset_warnings ();
    }
  } else {
    (void)Print_string ("  No warnings.\r");
  }
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (1000);
}

// Change typewriter settings.
void Change_typewriter_settings (void) {
  char cmd;
  int val;
  boolean first = TRUE;

  // Print typewriter legend.
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_string (TYPEWRITER_LEGEND);
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_element (&WW_PRINT_CRtn);
  (void)Print_element (&WW_PRINT_CRtn);

  // Read and process typewriter settings.
  while (TRUE) {

    // Print prompt.
    (void)Print_string (TYPEWRITER_PROMPT);

    // Read response.
    while (TRUE) {
      cmd = Read_setup_character_from_set ("1234567\x90\x91qQ\r");
      if (cmd == '\x90') {
        (void)Print_element (&WW_PRINT_UPARROW);
      } else if (cmd == '\x91') {
        (void)Print_element (&WW_PRINT_DOWNARROW);
      } else {
        break;
      }
    }

    // Process response.
    switch (cmd) {

      case '1':     // Spell check.
        (void)Print_element (&WW_PRINT_Spell);
        (void)Print_string (TYPEWRITER_SPELL_CHECK);
        (void)Print_element (&WW_PRINT_CRtn);
        break;

      case '2':     // Line spacing.
        (void)Print_element (&WW_PRINT_LineSpace);
        (void)Print_string (TYPEWRITER_LINE_SPACING);
        (void)Print_element (&WW_PRINT_CRtn);
        break;

      case '3':     // Powerwise.
        (void)Print_string (TYPEWRITER_POWERWISE);
        val = max (min (Read_integer (0), 90), 0);
        if (val < 10) {
          WW_STR_POWERWISE[4] = WW_Code;
        } else {
          WW_STR_POWERWISE[4] = DIGIT_CHARACTERS[val / 10];
        }
        WW_STR_POWERWISE[7] = DIGIT_CHARACTERS[val % 10];
        (void)Print_element (&WW_PRINT_POWERWISE);
        (void)Print_element (&WW_PRINT_CRtn);
        break;

      case '4':     // A Rtn.
        (void)Print_element (&WW_PRINT_ARtn);
        (void)Print_string (TYPEWRITER_ARTN);
        (void)Print_element (&WW_PRINT_CRtn);
        break;

      case '5':     // Bold.
        (void)Print_element (&WW_PRINT_Bold);
        (void)Print_string (TYPEWRITER_BOLD);
        (void)Print_element (&WW_PRINT_CRtn);
        break;

      case '6':     // Change margins & tabs.
        if (first) {
          first = FALSE;
          (void)Print_string (TYPEWRITER_CHANGE_MARGINS_TABS);
          (void)Print_element (&WW_PRINT_CRtn);
          (void)Print_element (&WW_PRINT_CRtn);
          (void)Print_element (&WW_PRINT_CRtn);
          (void)Print_element (&WW_PRINT_CRtn);
        } else {
          (void)Print_string (TYPEWRITER_CHANGE_MARGINS_TABSX);
        }
        (void)Print_margins_tabs (TRUE);
        in_margin_tab_setting = TRUE;
        while (TRUE) {
          cmd = Read_setup_character_from_set ("\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x90\x91\r");
          if      (cmd == 0x80)   (void)Print_element (&WW_PRINT_LEFTARROW);
          else if (cmd == 0x81)   (void)Print_element (&WW_PRINT_RIGHTARROW);
          else if (cmd == 0x82)   (void)Print_element (&WW_PRINT_Tab);
          else if (cmd == 0x83)   (void)Print_element (&WW_PRINT_CRtn);
          else if (cmd == 0x84)   (void)Print_element (&WW_PRINT_MarRelX);
          else if (cmd == 0x85) {if (current_column >= 1) 
                                 {(void)Print_element (&WW_PRINT_LMar);    (void)Print_element (&WW_PRINT_L);}}
          else if (cmd == 0x86)  {(void)Print_element (&WW_PRINT_RMar);    (void)Print_element (&WW_PRINT_R);}
          else if (cmd == 0x87) {if (tabs[current_column] == SETTING_FALSE)
                                 {(void)Print_element (&WW_PRINT_TSet);    (void)Print_element (&WW_PRINT_PLUS);}
                                 else
                                 {(void)Print_element (&WW_PRINT_T);}}
          else if (cmd == 0x88) {if (tabs[current_column] == SETTING_TRUE)
                                 {(void)Print_element (&WW_PRINT_TClr);    (void)Print_element (&WW_PRINT_HYPHEN);}}
          else if (cmd == 0x89)  {(void)Print_element (&WW_PRINT_TClrAll); (void)Print_element (&WW_PRINT_EQUAL);}
          else if (cmd == 0x90)   (void)Print_element (&WW_PRINT_UPARROW);
          else if (cmd == 0x91)   (void)Print_element (&WW_PRINT_DOWNARROW);
          else /* cmd == '\r' */ {(void)Print_element (&WW_PRINT_CRtn);    break;}
        }
        in_margin_tab_setting = FALSE;
        (void)Print_margins_tabs (FALSE);
        (void)Print_element (&WW_PRINT_CRtn);
        break;

      case '7':     // Restore margins & tabs.
        (void)Print_string (TYPEWRITER_RESTORE_MARGINS_TABS);
        (void)Print_element (&WW_PRINT_CRtn);
        Set_margins_tabs (TRUE);
        (void)Print_margins_tabs (TRUE);
        (void)Print_element (&WW_PRINT_CRtn);
        break;

      case 'q':     // Quit.
      case 'Q':
      case '\r':
        (void)Print_string (TYPEWRITER_QUIT);
        (void)Print_element (&WW_PRINT_CRtn);
        (void)Print_element (&WW_PRINT_CRtn);
        return;

      default:
        break;
    }
  }
}

// Read a setup character.
byte Read_setup_character (void) {
  while (cb_count == 0) delay (1);
  byte chr = command_buffer[cb_read];
  if (++cb_read >= SIZE_COMMAND_BUFFER) cb_read = 0;
  Update_counter (&cb_count, -1);
  return chr;
}

// Read a setup character from a defined set.
byte Read_setup_character_from_set (const char *charset) {
  while (TRUE) {
    while (cb_count == 0) delay (1);
    byte chr = command_buffer[cb_read];
    if (++cb_read >= SIZE_COMMAND_BUFFER) cb_read = 0;
    Update_counter (&cb_count, -1);
    if (strchr (charset, chr)) {
      return chr;
    } else {
      (void)Print_element (&WW_PRINT_BEEP);
    }
  }
}

// Space to print column.
void Space_to_column (int col) {
  if ((col >= 1) && (col <= length)) {
    while (current_column < col) {
      (void)Print_element (&WW_PRINT_SPACE);
    }
  }
}

// Return to column 1.
void Return_column_1 (void) {
  (void)Print_element (&WW_PRINT_CRtn);
  Wait_print_buffer_empty (100);
  (void)Print_element (&WW_PRINT_MarRelX);  // Special case that always prints.
  for (int i = 0; i < 20; ++i) (void)Print_element (&WW_PRINT_Backspace);
  for (int i = 0; i < offset; ++i) (void)Print_element (&WW_PRINT_SPACE);
  Wait_print_buffer_empty (100);
  current_column = 1;
}

// Open serial communications port.
void Serial_begin (void) {

  if (serial == SERIAL_USB) {

    Serial.begin (115200);

  } else /* serial == SERIAL_RS232 */ {

    if (baud >= MINIMUM_HARDWARE_BAUD) {
      Serial1.begin (BAUD_RATES[baud], DATA_PARITY_STOPS[dps]);
      Serial1.clear ();  // Workaround to clear FIFO scrambling permitted by driver.
      rs232_mode = RS232_UART;
      rts_size = Serial1.availableForWrite ();
    } else {  // The baud rate is too slow for UART, switch to SlowSoftSerial.
      slow_serial_port.begin (BAUD_RATES[baud], DATA_PARITY_STOPS_SLOW[dps]);
      rs232_mode = RS232_SLOW;
      rts_size = slow_serial_port.availableForWrite ();
    }

    if (hwflow == HWFLOW_NONE) {
      digitalWriteFast (serial_rts_pin, LOW);
      rts_state = RTS_ON;
      return;
    } else if (hwflow == HWFLOW_RTS_CTS) {
      digitalWriteFast (serial_rts_pin, HIGH);
      rts_state = RTS_OFF;
    } else /* hwflow == HWFLOW_RTR_CTS */ {
      digitalWriteFast (serial_rts_pin, LOW);
      rts_state = RTS_ON;
    }

    if (rs232_mode == RS232_UART) {
      Serial1.attachCts (serial_cts_pin);
    } else /* rs232_mode == RS232_SLOW */ {
      slow_serial_port.attachCts (serial_cts_pin);
    }
  }

  if (swflow == SWFLOW_XON_XOFF) {
    Send_character (CHAR_ASCII_XON);
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
inline boolean Serial_available (void) {
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
inline byte Serial_read (void) {
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
int Serial_write (byte chr) {
  if (flow_out_on) {
    if (serial == SERIAL_USB) {
      return Serial.write (chr);
    } else /* serial == SERIAL_RS232 */ {
      if ((hwflow == HWFLOW_RTS_CTS) && (rts_state != RTS_ON)) {
        digitalWriteFast (serial_rts_pin, LOW);
        rts_state = RTS_ON;
      }
      if ((hwflow == HWFLOW_NONE) || (digitalReadFast (serial_cts_pin) == LOW)) {
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
      } else {
        return 0;
      }
    }
  } else {
    return 0;
  }
}

// Send now to serial communication port.
inline void Serial_send_now (void) {
  if (serial == SERIAL_USB) {
    Serial.send_now ();
  } else /* serial == SERIAL_RS232 */ {
    // RS-232 serial does not support or need send_now.
  }
}

//======================================================================================================================
//
//  End of typewriter firmware.
//
//======================================================================================================================
