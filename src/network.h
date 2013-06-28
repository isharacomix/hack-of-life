/*
 *  HackOfLife - network.h
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

#ifndef __NETWORK_H
#define __NETWORK_H


/* Definitions. */
#define NET_CHUNK 1024


/* Preperation routines. */
void net_start( void );
void net_end( void );

/* Various handlers. */
int net_host( char *port );
int net_connect( char *host, char *port );
void send_message( char *s, int c );
char *get_message( int c );
char *wait_message( int c, int skip );
void kill_client( int c );
int is_connected( void );


#endif /* __NETWORK_H */

