/*
 *  HackOfLife - game.h
 *  Copyright (c) 2009  Barry "Ishara" Peddycord - http://isharacomix.com
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GAME_H
#define __GAME_H


/* Macros that everyone needs at some point in their life. */
#define MAX(a,b)  ( ( (a) > (b) ) ? (a) : (b) )
#define MIN(a,b)  ( ( (a) < (b) ) ? (a) : (b) )

/* Our option values in the options array. */
#define OP_PLAYER_COLOR     0
#define OP_SERIAL_GLYPHS    1
#define OP_SPACED_GRID      2
#define OP_TERMINAL_HEIGHT  3
#define OP_TERMINAL_WIDTH   4
#define OP_STARTING_SEEDS   5
#define OP_MAX_SEEDS        6
#define OP_GRID_WIDTH       7
#define OP_GRID_HEIGHT      8
#define OP_GRID_WRAP        9
#define OP_NOISE            10
#define OP_OTHER_CELLS      11
    #define OPx_PASSIVE         0
    #define OPx_SOLID           1
    #define OPx_DEADLY          2
#define OP_TIMEOUT          12
#define OP_NET_HANG         13
#define OP_GENERATIONS      14
#define OP_RULES            15
    #define OPx_SURVIVAL        0
    #define OPx_EXTERMINATION   1
    #define OPx_PROLIFERATION   2
    #define OPx_SANDBOX         3
#define OP_RULESTRING_S(x)  (16+(x))
#define OP_RULESTRING_B(x)  (25+(x))
#define OP_N                34


/* Functions for dealing with the game's many options. */
void load_options( void );
void save_options( void );
int validate_options( void );

/* Other neat features. */
int go_menu( int x, int y, int w, int h, int s, const char *options[], int go );
void get_string( int x, int y, int w, const char *header, char *input );
void blocker( void );
void writeint( unsigned int i, FILE *ofile );
unsigned int readint( FILE *ifile );


#endif /* __GAME_H */

