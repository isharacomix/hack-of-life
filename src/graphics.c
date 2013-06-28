/*
 *  HackOfLife - graphics.c
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
 *
 *  Handles the ncurses system. Allows other modules in the game to figure
 *  out the state of the terminal, adjust for coloring, etc. Also gives us
 *  fancy handlers for various forms of input, such as menus and text input.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include <curses.h>

#include "game.h"
#include "graphics.h"


/* Variables. */
static int term_w, term_h;              /*< Our terminal dimensions. */
static WINDOW *wnd = NULL;              /*< Our terminal window. */
static int colors = 0;                  /*< Are we using color? */


/* Begin the curses subsystem. It sets up the keyboard, puts everything in
   raw mode, and attempts to set up our colors. This function is safe to
   call multiple times. */
void start_ui( void )
{
    /* Set up the window if we haven't done so already. */
    if ( wnd == NULL )
        wnd = initscr();
    
    keypad(stdscr, TRUE);           /* Set up keyboard macros. */
    cbreak();                       /* Set input to 'raw' mode. */
    noecho();                       /* Disable key echo. */
    nonl();                         /* Disable the translated newline. */
    scrollok(stdscr, FALSE);        /* No scrolling! */
    curs_set(0);                    /* No cursor. */
    leaveok(stdscr, TRUE);          /* Cursor doesn't matter. */
    timeout(0);                     /* No waiting for input. */
    
    /* Color setup. */
    if ( has_colors() )
    {
        int c1, c2, i, j, n;    /*< Counters. */
        start_color();
        
        c1 = COLOR_BLACK;
        c2 = COLOR_BLACK;
        n = 0;
        
        /* Set up all of the color pairs. In order to ensure colors do not
           change across platforms, we force OUR color values. */
        for (i = 0; i < 8; i++)
        {
            switch (i)
            {
                case 0: c1 = COLOR_BLACK; break;
                case 1: c1 = COLOR_RED; break;
                case 2: c1 = COLOR_YELLOW; break;
                case 3: c1 = COLOR_GREEN; break;
                case 4: c1 = COLOR_CYAN; break;
                case 5: c1 = COLOR_BLUE; break;
                case 6: c1 = COLOR_MAGENTA; break;
                case 7: c1 = COLOR_WHITE; break;
            }
            
            for (j = 0; j < 8; j++)
            {
                switch (j)
                {
                    case 0: c2 = COLOR_BLACK; break;
                    case 1: c2 = COLOR_RED; break;
                    case 2: c2 = COLOR_YELLOW; break;
                    case 3: c2 = COLOR_GREEN; break;
                    case 4: c2 = COLOR_CYAN; break;
                    case 5: c2 = COLOR_BLUE; break;
                    case 6: c2 = COLOR_MAGENTA; break;
                    case 7: c2 = COLOR_WHITE; break;
                }
                
                n++;
                init_pair( n, c1, c2 );
            }

        }
        
        colors = 1;
    }
    
    /* We've done everything! Now clear the screen and move on. */
    reset_color();
    clear();
    refresh();
}


/* This returns the terminal screen to normal, but doesn't really "end"
   curses, so you can resume the session at any time. */
void end_ui(void)
{
    clear();
    flushinp();
    endwin();
}


/* Return the width of the terminal. */
int tw( void )
{
    return term_w;
}


/* Return the height of the terminal. */
int th( void )
{
    return term_h;
}


/* Set the terminal size as w by h. The program will happily try to work with
   these numbers as if they were the actual values of window. */
void set_size( int w, int h )
{
    if ( w > 0 ) term_w = w;
    if ( h > 0 ) term_h = h;
}


/* Return the current color/attribute as a single value such that the
   first digit is a hex value for fg color, and the second is an octal
   digit for bg color. This won't ever be larger than 255. */
unsigned char cur_color( void )
{
    attr_t atr_code;        /*< Our attribute code. */
    short col_code;         /*< Our color code. */
    unsigned char report;   /*< The report code. */
    
    if ( !colors )
        return 1;
    
    /* Get the parameters. */
    attr_get( &atr_code, &col_code, NULL );
    col_code --;
    
    /* Do the maths. */
    report = col_code / 8;
    if ( atr_code & A_BOLD )
        report += 8;
    report *= 16;
    report += col_code % 8;
    
    return report;
}


/* Set the color of the terminal. We check to see if colors are available
   before trying this out. */
void colorize( int fg, int bg )
{
    /* Higher values indicate bold colors. */
    if ( fg > 7 )
    {
        attron( A_BOLD );
        fg -= 8;
    }
    else
        attroff( A_BOLD );
    
    /* Now get the right color code. */
    if ( colors && fg >= 0 && bg >= 0 && fg < 8 && bg < 8 )
        attron( COLOR_PAIR( 8*fg+bg+1 ) );
}


/* Reset colors to the default (white on black). */
void reset_color( void )
{
    attrset( A_NORMAL );
    colorize( 7, 0 );
}


/* Erase a section from x1,y1 to x2,y2 in the window. */
void erasea( int x1, int y1, int x2, int y2 )
{
    int i;          /*< Iterator. */
    
    while ( y1 <= y2 )
    {
        for ( i = x1; i <= x2; i++ )
        {
            move( y1, i );
            addch( ' ' );
        }
        y1++;
    }
}

