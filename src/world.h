/*
 *  HackOfLife - world.h
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

#ifndef __WORLD_H
#define __WORLD_H


#define NO_PLAYER   0
#define P_DEAD      1
#define P_PLAYING   2
#define P_HATCHING  3

#define G_CONQUEST  1
#define G_CHALLENGE 2
#define G_HOTSEAT   3
#define G_NETPLAY   4
#define G_OPTIONS   5

#define STAT_SZ     20
#define MSG_SZ      8
#define MAX_MSGS    100


/* Global control functions. */
void play_game( int game_mode, int num_players );
void start_game( int num_players );
int *life_opts( void );
void pmsg( char *s );

/* Game board handlers. */
void handle_input( void );
void draw_all( int curplayer );
void draw_grid(int x1, int y1, int x2, int y2, int lx, int ly, int curplayer);
void draw_status( int x1, int y1, int x2, int y2, int curplayer );
void draw_messages( int x1, int y1, int x2, int y2 );
void next_generation( void );

/* Bitmap saving and loading. */
void save_bitmap( int challenge );
int load_challenge( char *fname );


#endif /* __WORLD_H */

