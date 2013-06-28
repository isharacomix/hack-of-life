/*
 *  HackOfLife - network.c
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
 *  Handles all network communication with the server. Tries to be as
 *  multi-platform as possible.
 */

#include <config.h>

#ifdef USE_IPV6
    #define IPVERSION AF_INET6
#else
    #define IPVERSION AF_INET
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <time.h>


/* The code is wildly different for Windows and for not Windows. */
#ifdef WIN32
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    
    typedef int socklen_t;
    WSADATA wsaData;
    
    #define NOTTIMEOUT ( WSAGetLastError() != 0 &&             \
                         WSAGetLastError() != WSAEWOULDBLOCK )
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    
    #define closesocket(X) close(X)
    
    typedef int SOCKET;
    #define INVALID_SOCKET  -1
    #define NOTTIMEOUT ( errno != EAGAIN && errno != EWOULDBLOCK )
#endif


#include <fcntl.h>
#include <errno.h>
#include <curses.h>

#include "game.h"
#include "network.h"


static struct addrinfo addr, *tgt;  /*< Our connection data and results. */

static SOCKET sock = INVALID_SOCKET;    /*< Primary socket. */
static SOCKET clients[5];               /*< Array of clients. */

static char data[NET_CHUNK];        /*< The inbound buffer. */
static char delayed[5][80];         /*< Delay buffers - just in case. */
static char delaybuff[80];          /*< Delay swapbuffer. */
static char hosting;                /*< 1 if I am a server. */


/* Prepare the network subsystem. */
void net_start( void )
{
    int i;          /*< Iterator. */
    
    /* Purge all sockets. */
    sock = INVALID_SOCKET;
    for ( i = 0; i < 5; i++ )
        clients[i] = INVALID_SOCKET;
    
    /* Clean our data. */
    memset( data, 0, sizeof(char)*NET_CHUNK );
    memset( delaybuff, 0, sizeof(char)*80 );
    memset( delayed, 0, sizeof(char)*80*5 );
    hosting = 0;
    
    #ifdef WIN32
        WSAStartup( MAKEWORD(2,2), &wsaData );
    #endif
}


/* Shut down the network subsystem. */
void net_end( void )
{
    int i;              /*< Integer. */
    
    memset( data, 0, NET_CHUNK );
    
    /* If we're dealing with the server, then shut down all clients. */
    if ( hosting )
    {
        for ( i = 0; i < 5; i++ )
        {
            if ( clients[i] != INVALID_SOCKET )
            {
                closesocket( clients[i] );
                clients[i] = INVALID_SOCKET;
            }
        }
    }
    
    /* Now shut down the primary socket. */
    if ( sock != INVALID_SOCKET )
    {
        closesocket( sock );
        sock = INVALID_SOCKET;
    }
    
    #ifdef WIN32
        WSACleanup();
    #endif
}


/* Start a server at your address. */
int net_host( char *port )
{
    struct addrinfo hints, *ai, *aj;    /*< Storage variables. */
    int result;                         /*< Error checking variable. */
    int i, c;                           /*< Counters! */
    #ifndef WIN32
        struct sockaddr_in newsock;         /*< The newly connected socket. */
        socklen_t alen;                     /*< The length of that socket. */
    #else
        static unsigned long truevariable = 1;
    #endif
    
    /* Set up our hint data so our server does its job. */
    memset( &hints, 0, sizeof( struct addrinfo ) );
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = IPVERSION;
    
    /* Initialize the port and get ready for serving. */
    result = getaddrinfo( NULL, port, &hints, &ai );
    if ( result != 0 )
        return 0;
    
    /* Try to find an appropriate socket. */
    aj = ai;
    while ( aj != NULL )
    {
        sock = socket( aj->ai_family, aj->ai_socktype, aj->ai_protocol );

        if ( sock == INVALID_SOCKET )
            aj = aj->ai_next;
        else
        {
            result = bind( sock, aj->ai_addr, aj->ai_addrlen );
            if ( result != 0 )
            {
                closesocket( sock );
                sock = INVALID_SOCKET;
                aj = aj->ai_next;
            }
            else
            {
                result = listen( sock, 5 );
                if ( result != 0 )
                {
                    closesocket( sock );
                    sock = INVALID_SOCKET;
                    aj = aj->ai_next;
                }
                else
                    aj = NULL;
            }
        }
    }
    
    /* Clean up the structure. */
    freeaddrinfo( ai );
    
    /* Was there an error? */
    if ( sock == INVALID_SOCKET )
        return 0;
    
    /* Set it to non-blocking, of course. */
    #ifndef WIN32
        fcntl( sock, F_SETFL, O_NONBLOCK );
    #else
        ioctlsocket( sock, FIONBIO, &truevariable );
    #endif
    
    /* Now we wait for our clients. */
    erase();
    move( 1, 1 );
    printw( "Waiting for other players." );
    move( 2, 1 );
    printw( "Press Enter to start the game." );
    move( 3, 1 );
    printw( "Players: " );
    refresh();
    
    /* Now wait for our clients. Display a counter on the screen of all of
       our peoples. */
    i = 0;
    c = ERR;
    while ( i < 5 && c != '\n' && c != '\r' )
    {
        /* Windows doesn't need a sockaddr object. Isn't that special? */
        #ifndef WIN32
            memset( &newsock, 0, sizeof( newsock ) );
            alen = sizeof( newsock );
            clients[i] = accept( sock, (struct sockaddr *) &newsock, &alen );
        #else
            clients[i] = accept( sock, NULL, NULL );
        #endif
        
        if ( clients[i] != INVALID_SOCKET )
        {
            #ifndef WIN32
                fcntl( clients[i], F_SETFL, O_NONBLOCK );
            #else
                ioctlsocket( clients[i], FIONBIO, &truevariable );
            #endif
            printw( "O " );
            i++;
        }
        
        c = getch();
        napms( 10 );
    }
    
    /* We are now a server! */
    hosting = 1;
    closesocket( sock );
    sock = INVALID_SOCKET;
    
    /* Return the number of clients. */
    return i;
}


/* Connect to a server at a given address. This always fails if we have a
   connection already. */
int net_connect( char *host, char *port )
{
    static struct addrinfo *cur;
    #ifdef WIN32
        static unsigned long truevariable = 1;
    #endif
    
    /* Are we already connected? */
    if ( sock != INVALID_SOCKET )
        return 0;
    
    /* Initialize all of our data.. */
    memset( data, 0, NET_CHUNK );
    
    /* Now let's get ready for some networking! */
    memset( &addr, 0, sizeof(addr) );
    addr.ai_family = AF_UNSPEC;
    addr.ai_socktype = SOCK_STREAM;
    addr.ai_flags = AI_PASSIVE;
    
    /* Try to match the domain name to an IP address. */
    if ( getaddrinfo( host, port, &addr, &tgt ) )
    {
        freeaddrinfo(tgt);
        return 0;
    }
    
    /* Start trying to connect to the list of returned addresses. */
    cur = tgt;
    while ( cur )
    {
        sock = socket( cur->ai_family, cur->ai_socktype, cur->ai_protocol );
        
        if ( sock == INVALID_SOCKET )
            cur = cur->ai_next;
        else if ( connect( sock, cur->ai_addr, cur->ai_addrlen ) )
        {
            closesocket( sock );
            sock = INVALID_SOCKET;
            cur = cur->ai_next;
        }
        else
            cur = NULL;
    }
    
    /* Free the data allocated by the searching system. */
    freeaddrinfo(tgt);
    
    /* If the connection fails... */
    if ( sock == INVALID_SOCKET )
        return 0;
    
    /* Otherwise, set the socket as non-blocking. */
    #ifndef WIN32
        fcntl( sock, F_SETFL, O_NONBLOCK );
    #else
        ioctlsocket( sock, FIONBIO, &truevariable );
    #endif
    
    /* We are not a server. */
    hosting = 0;
    return 1;
}


/* Send a string of data to the desired recipient (or the server, in the case
   of clients). The message should start with a 4-digit number saying how
   many bytes long the message is. */
void send_message( char *s, int c )
{
    int n, o, i;                /*< Counters. */
    SOCKET *tgt;                /*< The target socket. */
    
    o = 0;
    n = strlen(s)+4;
    sprintf( data, "%04d%s", (n-4), s );
    
    /* Get the right socket. */
    if ( hosting )
        tgt = clients+c;
    else
        tgt = &sock;
    
    /* Only send to a good socket. */
    if ( *tgt == INVALID_SOCKET )
        return;
    
    /* Now send the data. All of it, darn it! */
    while ( o < n )
    {
        i = send( *tgt, data+o, n-o, 0 );
        if ( i > 0 )
            o += i;
        else
        {
            closesocket( *tgt );
            *tgt = INVALID_SOCKET;
            return;
        }
    }
}


/* Get a message from the provided socket (or in the case of a client, just
   from the server). Returns either NULL (if no data) or a pointer to our
   data buffer. First we read 4 bytes to see how long the message is. Then
   we get the message. */
char *get_message( int c )
{
    int o, n, l;            /*< Counters. */
    SOCKET *tgt;            /*< What socket are we using? */
    
    if ( !hosting )
        c = 0;
    
    if ( delayed[c][0] )
    {
        strncpy( delaybuff, delayed[c], 79 );
        memset( delayed[c], 0, sizeof(char)*80 );
        return delaybuff;
    }
    
    memset( data, 0, sizeof(char)*NET_CHUNK );
    
    /* Get the right socket. */
    if ( hosting )
        tgt = clients+c;
    else
        tgt = &sock;
    
    /* Invalid socket? */
    if ( *tgt == INVALID_SOCKET )
        return "";
    
    /* First we figure out how big the message is. */
    o = recv( *tgt, data, 4, 0 );
    if ( o <= 0 )
    {
        /* If not report why. */
        if ( o == 0 || NOTTIMEOUT )
        {
            closesocket( *tgt );
            *tgt = INVALID_SOCKET;
            return "";
        }
        else
            return NULL;
    }
    else
    {
        n = sscanf( data, "%d", &l );
        memset( data, 0, sizeof(char)*4 );
        
        if ( n != 1 || l <= 0 )
            l = NET_CHUNK;
        
        /* Now try to get the right amount of data. */
        o = recv( *tgt, data, l, 0 );
        if ( o <= 0 )
        {
            if ( o == 0 || NOTTIMEOUT )
            {
                closesocket( *tgt );
                *tgt = INVALID_SOCKET;
                return "";
            }
            else
                return NULL;
        }
        else
            return data;
    }
}


/* Like get_message, only this one blocks until we get input. If skip is -1,
   you can press the 'X' key to break out of this, at which point, it will
   return NULL. If skip is positive, it will wait for 10 times that many
   milliseconds before timing out.
   We only wait for serious messages. If a message starts with 'c', then it's
   a chat message and needs to be delayed.*/
char *wait_message( int c, int skip )
{
    int i;              /*< Key clicker. */
    char *m;            /*< Our report val. */
    
    if ( !hosting )
        c = 0;
    
    memset( delaybuff, 0, sizeof(char)*80 );
    memset( delayed, 0, sizeof(char)*80*5 );
    
    i = ERR;
    m = get_message(c);
    while ( (m == NULL || m[0] == 'c') && i != 'X' && skip != 1 )
    {
        /* Put the chat message on the delay system. */
        if ( m != NULL && m[0] == 'c' )
            strncpy( delaybuff, m, 79 );
        
        napms(10);
        
        if ( skip == -1 )
            i = getch();
        else if ( skip > 0 )
            skip--;
        
        m = get_message(c);
    }
    
    strncpy( delayed[c], delaybuff, 79 );
    return m;
}


/* Kill the specified client. */
void kill_client( int c )
{
    if ( clients[c] != INVALID_SOCKET )
    {
        closesocket( clients[c] );
        clients[c] = INVALID_SOCKET;
    }
}


/* Returns 2 if we're hosting, 1 if we're connected, and 0 if we're offline. */
int is_connected( void )
{
    if ( hosting )
        return 2;
    if ( sock != INVALID_SOCKET )
        return 1;
    return 0;
}

