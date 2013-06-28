/*
 *  HackOfLife - graphics.h
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

#ifndef __GRAPHICS_H
#define __GRAPHICS_H


/* Start and end the curses subsystem. */
void start_ui( void );
void end_ui( void );

/* Terminal size functions. */
int tw( void );
int th( void );
void set_size( int w, int h );

/* Coloring functions. */
unsigned char cur_color( void );
void colorize( int fg, int bg );
void reset_color( void );
void erasea( int x1, int y1, int x2, int y2 );


#endif /* __GRAPHICS_H */

