/*
 *  HackOfLife - world.c
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
 *  The super-massive game handler. Everything that happens within the game
 *  world happens here, and I do mean everything!
 *
 *  The cells of the grid of life are defined as 2-digit decimal numbers.
 *      00      Cell is dead (there are no colorless seeds)
 *      0x      Cell is a seed of color 'x'
 *      1x      Cell is alive and is color 'x'
 *  The player is always color '1'. A cell is alive if its value > 9.
 *  Colorless cells are number 10.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include <time.h>
#include <curses.h>

#include "game.h"
#include "graphics.h"
#include "random.h"
#include "world.h"
#include "network.h"


static int loptions[OP_N];      /*< Our list of game options. */
static char *lgrid;             /*< The life grid. Contains the cells. */
static char *wgrid;             /*< The working grid. Used for counting
                                        neighbors. */
static int gw, gh;              /*< The dimensions of the life grid. */
static int cam_x, cam_y;        /*< The camera position (the top left cell
                                        on in the viewport). */
#define el(x,y) ( (y)*gw + (x) )    /*< This gets the element provided. */

static int gen_no;              /*< The current generation. */
static int scores[10];          /*< The score counts. */
static int seeds[10];           /*< The seed counts. */
static int colors[10];          /*< The colors of the various players. */
static int players[10][3];      /*< Player status and locations. */

static int game_mode;           /*< This is our game mode. */
static int stasis;              /*< Are we in stasis mode? SANDBOX ONLY */
static int chatting;            /*< Are we in chatting mode? */
static int viewing;             /*< Are we in viewing mode? */

static char fname[50];          /*< File name buffer. */
static char cbuffer[100];       /*< Chat buffer. */
static char wbuffer[100];       /*< Working buffer. */
static char msgs[MAX_MSGS][80]; /*< Our message buffer. */
static int top_msg;             /*< The front of the circular buffer. */
static FILE *logfile = NULL;    /*< Message logger. */


/* Start playing the game based on the current rules, specifying the number of
   other players (not P1). We continue playing until we quit or the game ends
   (losing conquest mode, finishing challenge mode, etc) */
void play_game( int type, int num_players )
{
    int i, j, n;            /*< Iterators. */
    char *m;                /*< Net message holder. */
    
    /* Make sure the options make sense. If they don't, then do not play. */
    if ( !validate_options() )
        return;
    
    game_mode = type;
    
    /* Print a message so our players don't feel bad. */
    switch ( game_mode )
    {
        case G_CONQUEST     : pmsg("Starting a new Conquest game. Clear as "
                                   "many levels as you can!");
                              break;
        case G_CHALLENGE    : pmsg("Starting a new Challenge game. Finished "
                                   "loading the specified file.");
                              break;
        case G_HOTSEAT      : pmsg("Starting a new Hotseat game. Good luck!");
                              break;
        case G_NETPLAY      : pmsg("Starting a new Netplay game. Have fun!");
                              break;
        default             : pmsg("There was an error. What the heck have "
                                   "you done?");
                              break;
    }
    
    /* Reserve the memory needed to play the Game of Life. */
    gw = loptions[OP_GRID_WIDTH];
    gh = loptions[OP_GRID_HEIGHT];
    lgrid = malloc( sizeof(char)*gw*gh );
    wgrid = malloc( sizeof(char)*gw*gh );
    
    /* ABORT! CATASTROPHE! PANIC! IF WE KEEP TRYING TO RUN THE GAME HERE, THE
       WORLD WILL COME TO A CATACLYSMIC END! */
    if ( lgrid == NULL || wgrid == NULL )
    {
        /* Redundant error checking is redundant. */
        if ( lgrid != NULL ) free( lgrid );
        if ( wgrid != NULL ) free( wgrid );
        
        return;
    }
    
    /* Prepare the game. */
    start_game( num_players );
    
    /* Scatter the generators randomly. */
    for ( n = 0; n < num_players+1; n++ )
    {
        players[n+1][1] = rnd_31int()%gw;
        players[n+1][2] = rnd_31int()%gh;
    }
    
    /* In challenge mode, load the starting places from an input file. */
    if ( game_mode == G_CHALLENGE )
    {
        erase();
        get_string( 1, 1, 50, "Filename of Challenge Bitmap:", wbuffer );
        if ( load_challenge( wbuffer ) == 0 )
            return;
        memset( wbuffer, 0, sizeof(char)*100 );
    }
    
    /* If we're guesting, receive our initial locations. Otherwise, if we're
       the host, then send our initial locations. */
    if ( is_connected() == 1 )
    {
        /* Wait for my starting position. We'll wait a little longer than
           usual, but not *THAT* long. */
        m = wait_message( 0, 2000 );
        
        if ( !m || m[0] == 0 )
            return;
        else
        {
            i = 1;
            while ( m && i < num_players+2 &&
                    sscanf(m,"%d %d",&(players[i][1]),&(players[i][2])) == 2 )
            {
                i++;
                
                m = strchr( m, ' ' );
                if ( m )
                {
                    m++;
                    m = strchr( m, ' ' );
                    if ( m )
                        m++;
                }
            }
        }
    }    
    else if ( is_connected() == 2 )
    {
        /* Send everyone their starting positions! */
        for ( i = 0; i < num_players; i++ )
        {
            n = 0;
            memset( wbuffer, 0, sizeof(char)*100 );
            
            n+=sprintf(wbuffer+n, "%d %d ",players[i+2][1],players[i+2][2]);
            for ( j = 0; j < num_players; j++ )
            {
                if ( i == j )
                    n+=sprintf(wbuffer+n,"%d %d ",players[1][1],players[1][2]);
                else
                    n+=sprintf(wbuffer+n,"%d %d ",
                               players[j+2][1],players[j+2][2]);
            }
            
            send_message( wbuffer, i );
        }
    }
    
    /* Start playing the game loop. */
    draw_all( 1 );
    while ( game_mode )
    {
        /* Draw the screen and handle player input. */
        handle_input();
        
        /* Iterate to the next generation. */
        if ( !stasis )
        {
            next_generation();
            gen_no++;
        }
        
        /* When extermination mode is running, if you run out of seeds and
           cells, you die. */
        if ( loptions[OP_RULES] == OPx_EXTERMINATION && 
             ( game_mode == G_CONQUEST || game_mode == G_CHALLENGE ) )
        {
            for ( i = 2; i < 10 && scores[i] == 0; i++ ){};
            if ( i == 10 )
            {
                pmsg("Congratulations! Press any key to continue.");
                draw_all(0);
                blocker();
                start_game( num_players );
            }
        }
        
        /* If the time limit has expired, end the round. */
        if ( loptions[OP_GENERATIONS] && gen_no > loptions[OP_GENERATIONS] )
        {
            j = 1;
            for ( i = 2; i < 10; i++ )
            {
                if ( scores[i] > scores[j] )
                    j = i;
            }
            
            /* If playing CONQUEST and the player wins, start a new round. */
            if ( j == 1 && game_mode == G_CONQUEST )
            {
                pmsg("Congratulations! Press any key to continue.");
                draw_all(0);
                blocker();
                start_game( num_players );
            }
            else
                game_mode = 0;
        }
        
        /* If you are all out of cells and seeds, then you die. */
        if ( loptions[OP_STARTING_SEEDS] )
        {
            for ( i = 0; i < 10; i++ )
            {
                if ( players[i][0] && (scores[i]+seeds[i] == 0) )
                    players[i][0] = P_DEAD;
            }
        }
        
        /* The game always ends if everyone is dead. */
        j = 0;
        for ( i = 0; i < 10; i++ )
        {
            if ( players[i][0] > P_DEAD )
                j++;
        }
        if ( game_mode == G_HOTSEAT || game_mode == G_NETPLAY ) j--;
        if ( j <= 0 )
            game_mode = 0;
        
        /* Does anyone have 1/3 of the map? */
        if ( game_mode && loptions[OP_RULES] == OPx_PROLIFERATION )
        {
            for ( i = 1; i < 10; i++ )
            {
                if ( scores[i] > 1.0/3.0*gw*gh )
                {
                    /* If playing CONQUEST and the player wins, start a new
                       round. */
                    if ( i == 1 && game_mode == G_CONQUEST )
                    {
                        /* print you win. */
                        pmsg("Congratulations! Press any key to continue.");
                        draw_all(0);
                        blocker();
                        start_game( num_players );
                    }
                    else
                        game_mode = 0;
                }
            }
        }
    }
    
    /* The game is over. Pause so the player can cope with it. */
    pmsg("This round has ended. Press any key to return to the Main Menu.");
    draw_all(0);
    blocker();
    
    if ( logfile )
    {
        fclose( logfile );
        logfile = NULL;
    }
    
    /* Free the memory associated with the game board. */
    free( lgrid );
    free( wgrid );
}


/* Start a new game based on the game mode. */
void start_game( int num_players )
{
    int i, j, c;            /*< Iterators. */
    
    /* Reset global variables, such as score counters and the like. */
    gen_no = 0;
    memset( scores, 0, sizeof(int)*10 );
    memset( players, 0, sizeof(int)*10*3 );
    memset( lgrid, 0, sizeof(char)*gw*gh );
    memset( wgrid, 0, sizeof(char)*gw*gh );
    stasis = 0;
    chatting = 0;
    viewing = 0;
    
    /* Set the colors randomly. */
    colors[0] = 7;
    colors[1] = loptions[OP_PLAYER_COLOR];
    i = 2;
    while ( i < 7 )
    {
        c = (rnd_31int()%6) + 1;
        for ( j = 0; j < i && colors[j] != c; j++ ){};
        if ( j == i )
        {
            colors[i] = c;
            i++;
        }
    }
    
    /* Initialize the player values and cell seeds. */
    players[1][0] = P_PLAYING;
    for ( i = 0; i < 8 && i < num_players; i++ )
    {
        players[i+2][0] =
            (game_mode==G_HOTSEAT || game_mode==G_NETPLAY)? P_PLAYING : P_DEAD;
    }
    for ( i = 0; i < 10; i++ )
        seeds[i] = loptions[OP_STARTING_SEEDS];
    
    /* Populate the field with random items in conquest mode. */
    if ( game_mode == G_CONQUEST && num_players > 0 )
    {
        for ( i = 0; i < gw*gh; i++ )
        {
            if ( rnd_r2() < .20 )
            {
                j = rnd_31int() % num_players;
                lgrid[i] = 12+j;
                scores[2+j] ++;
            }
        }
    }
    
    /* Populate the field with noise if the option is set. */
    if ( game_mode != G_CHALLENGE && loptions[OP_NOISE] )
    {
        for ( i = 0; i < gw*gh; i++ )
        {
            if ( !lgrid[i] && rnd_r2() < .20 )
            {
                lgrid[i] = 10;
                scores[0] ++;
            }
        }
    }
}


/* Global accessor for 'loptions'. */
int *life_opts( void )
{
    return loptions;
}


/* Add message to the message buffer. */
void pmsg( char *s )
{
    strncpy( msgs[top_msg], s, 79 );
    
    /* Move to the next spot in the buffer. */
    top_msg++;
    if ( top_msg == MAX_MSGS )
        top_msg = 0; 
    
    /* Log to the file if we need to. */
    if ( logfile )
        fprintf( logfile, "%s\n", s );
}


/* Handle input either from the keyboard or the network layer. Regardless of
   the rules of the game. A turn ends when all players are READY or when the
   timer runs out. */
void handle_input( void )
{
    int i, n;                   /*< Iterators and holding vars. */
    int timelimit;              /*< Time limit in milliseconds. */
    int busy;                   /*< Are we busy working? */
    int dx, dy;                 /*< Changes in position. */
    int c, bigC;                /*< What is the recently pressed key? */
    int acted;                  /*< This is true if we sent our act message
                                    to the other players. */
    int ready[10];              /*< Who are we waiting on in net play? */
    char *m;                    /*< Network message string. */
    
    /* Set the players to zero and find the first one. */
    memset( ready, 0, sizeof(int)*10 );
    acted = 0;
    busy = 1;
    if ( game_mode == G_HOTSEAT )
    {
        for ( i = 0; i < 10 && players[i][0] <= P_DEAD; i++ ){};
        busy = ( i == 10 ) ? 0 : 1;
    }
    else
        i = 1;
    
    /* Run until all living players have executed their turn, or until the
       timeout occurs. */
    draw_all( i );
    timelimit = loptions[OP_TIMEOUT];
    if ( stasis && timelimit <= 0 ) timelimit = 50;
    while ( busy && ( timelimit > 0 || !(loptions[OP_TIMEOUT]) ||
                      game_mode == G_HOTSEAT || game_mode == G_NETPLAY ) )
    {
        /* Nap for 10 milliseconds at a time.. */
        napms( 10 );
        if ( timelimit > 0)
            timelimit -= 10;
        
        /* Set values to 0. */
        dx = 0;
        dy = 0;
        
        /* Flush input and only count the most recently pressed key. */
        c = getch();
        
        /* Check the current mode of the game before trying to interpret
           the keypresses. */
        if ( c == ERR )
        {
            /* Do nothing. */
        }
        else if ( chatting )
        {
            n = strlen(cbuffer);
            
            /* Enter/return end chatting. Other characters are printed
               and/or deleted. */
            if ( c == '\n' || c == '\r' )
            {
                if ( strlen( cbuffer+1 ) > 0 )
                {
                    pmsg( cbuffer+1 );
                    if ( is_connected() == 1 )
                        send_message( cbuffer, 0 );
                    else if ( is_connected() == 2 )
                    {
                        for ( n = 0; n < 5; n++ )
                            send_message( cbuffer, n );
                    }
                    memset( cbuffer, 0, sizeof(char)*100 );
                }
                chatting = 0;
            }
            else if ( ( c > 0 && c < 0x100 && isprint(c) ) && n < 99 )
                cbuffer[n] = c;
            else if ((c == KEY_BACKSPACE || c == '\b' || c == 127) && n>1)
                cbuffer[n-1] = 0;
            
            draw_all(i);
        }
        else
        {
            /* Use this code to get the directions. */
            bigC = ( c > 0 && c < 0x100 && isalpha(c) ) ? toupper(c) : ERR;
            
            /* Support the regular RL keys. */
            if ( bigC == 'K' || bigC == 'Y' || bigC == 'U' ) dy = -1;
            if ( bigC == 'J' || bigC == 'B' || bigC == 'N' ) dy =  1;
            if ( bigC == 'H' || bigC == 'Y' || bigC == 'B' ) dx = -1;
            if ( bigC == 'L' || bigC == 'U' || bigC == 'N' ) dx =  1;
            
            /* And support the arrow keys too. */
            if ( c == KEY_UP    ) dy = -1;
            if ( c == KEY_DOWN  ) dy =  1;
            if ( c == KEY_LEFT  ) dx = -1;
            if ( c == KEY_RIGHT ) dx =  1;
            
            /* Handle generator/camera movement or seed placement. */
            if ( viewing )
            {
                cam_x += dx;
                cam_y += dy;
                draw_all( i );
            }
            else if ( (dx || dy || c == '.' || c == '>') && !ready[i] )
                ready[i] = c;
            
            /* If an keypress is associated with an action that alters the
               board, then we defer acting on it until later. */
            if ( (c == ' ' || c == ',' || c == 'Q' || c == 'e') && !ready[i] )
                ready[i] = c;
            
            /* Viewing mode... used for just looking around. */
            else if ( c == 'w' )
            {
                viewing = ( viewing ) ? 0 : 1;
                draw_all( i );
            }
            
            /* Open log file. */
            else if ( c == 'a' )
            {
                if ( logfile )
                {
                    fclose( logfile );
                    logfile = NULL;
                    
                    pmsg( "Logging has stopped." );
                }
                else
                {
                    memset( fname, 0, sizeof(char)*50 );
                    sprintf( fname, "log%d.txt", (int) time(NULL) );
                    logfile = fopen( fname, "w" );
                    
                    if ( logfile )
                        pmsg( "Now logging to file!" );
                    else
                        pmsg( "Could not open the logfile." );
                }
            }
            
            /* Screenshot of board (bmp file) */
            else if ( c == 's' )
            {
                save_bitmap( 0 );
                draw_all( i );
            }
            
            /* Challenge screenshot of board (saves the player's cells
               as red, for use in Challenge mode). */
            else if ( c == 'S' )
            {            
                save_bitmap( 1 );
                draw_all( i );
            }
            
            /* End the game. */
            else if ( c == 'X' )
            {
                game_mode = 0;
                return;
            }
            
            /* Display the game rules. */
            else if ( c == '?' )
            {
                /* Print the grid specs. */
                n = sprintf( wbuffer, "%dx%d grid with", gw, gh );
                
                if ( !loptions[OP_GRID_WRAP] )
                    n += sprintf( wbuffer+n, "out" );
                n += sprintf( wbuffer+n, " wrapping." );
                pmsg( wbuffer );
                
                /* Timeouts. */
                if ( loptions[OP_TIMEOUT] )
                    sprintf( wbuffer, "Turns timeout after %d milliseconds.",
                             loptions[OP_TIMEOUT] );
                else
                    sprintf( wbuffer, "Turns do not timeout.");
                pmsg( wbuffer );
                
                /* Survival rulestring. */
                sprintf( wbuffer, "Survival Rulestring: %03d%% %03d%% %03d%% "
                                  "%03d%% %03d%% %03d%% %03d%% %03d%% %03d%%",
                   loptions[OP_RULESTRING_S(0)], loptions[OP_RULESTRING_S(1)],
                   loptions[OP_RULESTRING_S(2)], loptions[OP_RULESTRING_S(3)],
                   loptions[OP_RULESTRING_S(4)], loptions[OP_RULESTRING_S(5)],
                   loptions[OP_RULESTRING_S(6)], loptions[OP_RULESTRING_S(7)],
                   loptions[OP_RULESTRING_S(8)] );
                pmsg( wbuffer );
                
                /* Birth rulestring. */
                sprintf( wbuffer, "Birth Rulestring   : %03d%% %03d%% %03d%% "
                                  "%03d%% %03d%% %03d%% %03d%% %03d%% %03d%%",
                   loptions[OP_RULESTRING_B(0)], loptions[OP_RULESTRING_B(1)],
                   loptions[OP_RULESTRING_B(2)], loptions[OP_RULESTRING_B(3)],
                   loptions[OP_RULESTRING_B(4)], loptions[OP_RULESTRING_B(5)],
                   loptions[OP_RULESTRING_B(6)], loptions[OP_RULESTRING_B(7)],
                   loptions[OP_RULESTRING_B(8)] );
                pmsg( wbuffer );
                draw_all( i );
            }
            
            /* Enter chat mode. */
            else if ( c == '\n' || c == '\r' )
            {
                memset( cbuffer, 0, sizeof(char)*100 );
                cbuffer[0] = 'c';
                chatting = 1;
                draw_all( i );
            }
        }
        
        /* Check for network input. */
        if ( game_mode == G_NETPLAY )
        {
            /* Send a ! to let the server know I made a move. The server then
               sends that player's ID number. (2-6) */
            if ( ready[i] && !acted )
            {
                acted = 1;
                
                if ( is_connected() == 1 )
                    send_message( "!", 0 );
                else
                {
                    for ( n = 2; n < 7; n++ )
                    {
                        if ( players[n][0] )
                        {
                            sprintf( fname, "%d", n );
                            send_message( fname, n-2 );
                        }
                    }
                }
            }
            
            /* Clients wait and try to receive ID numbers to check to see if
               players are ready to go. The server waits for '!' signals. If
               a signal starts with 'c', then it's a chat message. */
            if ( is_connected() == 1 )
            {
                m = get_message( 0 );
                if ( m )
                {
                    if ( m[0] == 0 )
                    {
                        game_mode = 0;
                        pmsg( "Disconnected from server..." );
                        return;
                    }
                    else if ( m[0] == 'c' )
                    {
                        pmsg( m+1 );
                        draw_all( i );
                    }
                    else if ( m[0] == '!' )
                    {
                        busy = 0;
                        if ( !ready[i] )
                            ready[i] = 1;
                    }
                }
            }
            else
            {
                for ( n = 2; players[n][0]; n++ )
                {
                    /* Receive messages that we need to receive. */
                    m = get_message( n-2 );
                    if ( m == NULL )
                        continue;
                    
                    /* If the client disconnects, consider it Quitting. */
                    if ( m[0] == 0 )
                    {
                        kill_client( n-2 );
                        players[n][0] = P_DEAD;
                    }
                    else if ( m[0] == 'c' )
                    {
                        pmsg( m+1 );
                        strncpy( wbuffer, m, 99 );
                        for ( c = 0; c < 5; c++ )
                        {
                            if ( c != n-2 )
                                send_message( wbuffer, c );
                        }
                        draw_all( i );
                    }
                    
                    /* Set ready flag to everyone. */
                    if ( m[0] == 0 || m[0] == '!' )
                        ready[n] = 1;
                }
            }
        }
        
        /* Check all players to see if we're done. While this is done mostly
           for multiplayer mode, we can use the same code for single-player
           as well. */
        if ( game_mode == G_NETPLAY )
        {
            if ( is_connected() == 2 )
            {
                for ( i = 0; i<10 && ((players[i][0] > P_DEAD && ready[i]) ||
                                       players[i][0] <= P_DEAD) ; i++ ){};
                
                /* Take clients to next stage. */
                if ( i == 10 || ( (loptions[OP_TIMEOUT] || stasis ) &&
                                   timelimit <= 0 ) )
                {
                    busy = 0;
                    for ( n = 0; n < 5; n++ )
                        send_message( "!", n );
                    if ( !ready[i] )
                        ready[i] = 1;
                }
                
                i = 1;
            }
        }
        else if ( ready[i] )
        {
            i++;
            while ( i < 10 && players[i][0] <= P_DEAD ) i++;
            busy = ( i == 10 ) ? 0 : 1;
            acted = 0;
            
            if ( busy )
                draw_all( i );
        }
    }
    
    /* Get the actual commands spread across the network. */
    if ( game_mode == G_NETPLAY )
    {
        memset( fname, 0, sizeof(char)*50 );
        
        /* Override the multibyte keys. */
        if ( ready[1] == KEY_DOWN )       ready[1] = 'j';
        else if ( ready[1] == KEY_UP )    ready[1] = 'k';
        else if ( ready[1] == KEY_LEFT )  ready[1] = 'h';
        else if ( ready[1] == KEY_RIGHT ) ready[1] = 'l';
        
        /* Get the right input. */
        if ( is_connected() == 1 )
        {
            fname[0] = ready[1];
            send_message( fname, 0 );
            
            /* Now we wait for the official queue of data to return. */
            m = wait_message( 0, loptions[OP_NET_HANG] );
            memset( ready, 0, sizeof(int)*10 );
            
            if ( m && m[0] )
            {
                for ( n = 0; n < 10; n++ )
                    ready[n] = m[n];
            }
            else
            {
                game_mode = 0;
                pmsg( "Disconnected from server..." );
                return;
            }
        }
        else
        {
            ready[0] = 1;
            for ( n = 2; n < 7; n++ )
            {
                if ( players[n][0] > P_DEAD )
                {
                    m = wait_message( n-2, loptions[OP_NET_HANG] );
                    
                    if ( m && m[0] )
                        ready[n] = m[0];
                    else
                    {
                        ready[n] = 'Q';
                        kill_client( n-2 );
                    }
                }
                else if ( players[n][0] == P_DEAD )
                    ready[n] = 'Q';
            }
            
            /* Create a message string. */
            for ( n = 0; n < 10; n++ )
                fname[n] = (char) ready[n];
            
            /* We have to reorder the tokens depending on our client. */
            for ( n = 2; n < 7 && players[n][0]; n++ )
            {
                c = fname[n];
                fname[n] = fname[1];
                fname[1] = c;
                
                send_message( fname, n-2 );
                
                c = fname[n];
                fname[n] = fname[1];
                fname[1] = c;
            }
        }
    }
    
    /* Now go through and PERFORM the actions. */
    for ( i = 1; i < 10; i++ )
    {
        if ( ready[i] && players[i][0] > P_DEAD )
        {
            c = ready[i];
            bigC = ( c > 0 && c < 0x100 && isalpha(c) ) ? toupper(c) : ERR;
            
            /* Repeat this song and dance. */
            dx = 0;
            dy = 0;
            
            /* Support the regular RL keys. */
            if ( bigC == 'K' || bigC == 'Y' || bigC == 'U' ) dy = -1;
            if ( bigC == 'J' || bigC == 'B' || bigC == 'N' ) dy =  1;
            if ( bigC == 'H' || bigC == 'Y' || bigC == 'B' ) dx = -1;
            if ( bigC == 'L' || bigC == 'U' || bigC == 'N' ) dx =  1;
            
            /* And support the arrow keys too. */
            if ( c == KEY_UP    ) dy = -1;
            if ( c == KEY_DOWN  ) dy =  1;
            if ( c == KEY_LEFT  ) dx = -1;
            if ( c == KEY_RIGHT ) dx =  1;
        
            if ( dx || dy || c == '.' || c == '>' )
            {
                /* Handle boundary issues by altering dx and dy to end up
                   as the correct result. */
                if ( loptions[OP_GRID_WRAP] )
                {
                    if ( players[i][1] == 0 && dx == -1 ) dx = gw-1;
                    else if ( players[i][1] == gw-1 && dx == 1 ) dx = 1-gw;
                    
                    if ( players[i][2] == 0 && dy == -1  ) dy = gh-1;
                    else if ( players[i][2] == gh-1 && dy == 1 ) dy = 1-gh;
                }
                else
                {
                    if ( ( players[i][1] == 0 && dx == -1 ) ||
                         ( players[i][1] == gw-1 && dx == 1 ) ) dx = 0;
                    if ( ( players[i][2] == 0 && dy == -1 ) ||
                         ( players[i][2] == gh-1 && dy == 1 ) ) dy = 0;
                }
                
                /* Handle the placement/drawing of the seed/player. */
                if ( c == bigC || c == '>' )
                {
                    if ( lgrid[el(players[i][1]+dx,players[i][2]+dy)] == 0
                         && ( loptions[OP_STARTING_SEEDS] == 0 ||
                              seeds[i] > 0 ) )
                    {
                        lgrid[el(players[i][1]+dx,players[i][2]+dy)] = i;
                        
                        if ( loptions[OP_STARTING_SEEDS] )
                            seeds[i] --;
                    }
                }
                else
                {
                    if ( loptions[OP_OTHER_CELLS] &&
                         lgrid[el(players[i][1]+dx,players[i][2]+dy)] > 9 &&
                         lgrid[el(players[i][1]+dx,players[i][2]+dy)]%10 != i )
                    {
                        /* Do nothing. */
                    }
                    else
                    {
                        players[i][1] += dx;
                        players[i][2] += dy;
                    }
                }
            }
                
            /* Handle hatching seeds. */
            if ( c == ' ' )
                players[i][0] = P_HATCHING;
                
            /* Do harvesting. */
            else if ( c == ',' )
            {
                if ( lgrid[ el( players[i][1], players[i][2] ) ]%10 == i 
                     && loptions[OP_RULES] != OPx_EXTERMINATION )
                {
                    /* Drop the score if it was a live cell. */
                    if ( lgrid[ el( players[i][1], players[i][2] ) ] > 9 )
                        scores[ i ] --;
                    lgrid[ el( players[i][1], players[i][2] ) ] = 0;
                    
                    /* Increment the seed stores. */
                    if ( seeds[i] < loptions[OP_MAX_SEEDS] )
                        seeds[i] ++;
                }
            }
            
            /* Do quitting. This just kills your generator. */
            else if ( c == 'Q' )
                players[i][0] = P_DEAD;
            
            /* Stasis mode. Disable the cells from proceeding to their
               next generation. Because of the side effect, this counts
               as an action. */
            else if ( c == 'e' && loptions[OP_RULES] == OPx_SANDBOX )
                    stasis = ( stasis ) ? 0 : 1;
        }
    }
}


/* Draw everything. Wrapper for draw_grid, draw_status, draw_messages. */
void draw_all( int curplayer )
{
    curplayer %= 10;
    
    /* Center the camera if we need to. */
    if ( !viewing && curplayer )
    {
        /* Handle the extra spacing required by the SPACED_GRID parameter. */
        if ( life_opts()[OP_SPACED_GRID] )
        {
            if ( ( players[curplayer][1] - cam_x < (tw() - STAT_SZ)*.125 ) ||
                 ( players[curplayer][1] - cam_x > (tw() - STAT_SZ)*.375 ) )
                cam_x = players[curplayer][1] - (tw() - STAT_SZ)*.25;
        }
        else
        {
            if ( ( players[curplayer][1] - cam_x < (tw() - STAT_SZ)*.25 ) ||
                 ( players[curplayer][1] - cam_x > (tw() - STAT_SZ)*.75 ) )
                cam_x = players[curplayer][1] - (tw() - STAT_SZ)*.50;
        }
        
        /* Vertical alignment is always the same. */
        if ( ( players[curplayer][2] - cam_y < (th() - MSG_SZ)*.25 ) ||
             ( players[curplayer][2] - cam_y > (th() - MSG_SZ)*.75 ) )
            cam_y = players[curplayer][2] - (th() - MSG_SZ)*.50;
    }
    
    /* Draw everything!!! */
    erase();
    draw_status( 0, 0, STAT_SZ, th()-MSG_SZ, curplayer );
    draw_grid( STAT_SZ+1, 0, tw()-1, th()-MSG_SZ, cam_x, cam_y, curplayer );
    draw_messages( 0, th()-MSG_SZ+1, tw()-1, th()-2 );
    
    /* Draw cbuffer. */
    if ( chatting )
    {
        move ( th()-1, 0 );
        addnstr( cbuffer+1, tw()-2 );
        colorize( 0, 7 );
        addch( ' ' );
    }
    
    refresh();
}


/* Draw the board in the terminal from x1,y1 to x2,y2 (inclusive), with the top
   left corner starting at lx,ly. */
void draw_grid( int x1, int y1, int x2, int y2, int lx, int ly, int curplayer )
{
    char c;                /*< Cell holder. */
    int s1, s2, s3, s4;    /*< Storage variables. */
    
    curplayer %= 10;
    
    /* Store our wrappers. */
    s1 = x1;
    s2 = lx;
    s3 = y1;
    s4 = ly;
    
    /* First we draw the actual grid of cells and seeds. */
    while ( y1 <= y2 )
    {
        while ( x1 <= x2 )
        {
            /* 42 is a magic number representing an out-of-bounds locations. */
            if ( lx >= gw || ly >= gh || lx < 0 || ly < 0 )
            {
                c = 42;
                colorize( 7, 0 );
            }
            else
            {
                c = lgrid[ el(lx,ly) ];
                colorize( colors[c%10] + ((curplayer == c%10) ? 8:0), 0 );
            }
            
            /* Draw the right token in the right spot. Inefficient, but I don't
               really care all that much. */
            move( y1, x1 );
            
            /* Based on the glyph set, we print them all. */
            if ( loptions[OP_SERIAL_GLYPHS] )
            {
                if ( c == 0 )       addch( ' ' );
                else if ( c == 42 ) addch( '-' );
                else if ( c == 1 )  addch( '.' );
                else if ( c < 10 )  addch( 'a'+c-1 );
                else if ( c == 10 ) addch( '*' );
                else if ( c == 11 ) addch( 'o' );
                else                addch( 'A'+c-11 );
            }
            else
            {
                if ( c == 0 )       addch( ' ' );
                else if ( c == 42 ) addch( '-' );
                else if ( c < 10 )  addch( '.' );
                else                addch( 'o' );
            }
            
            lx++;
            x1 += ( loptions[OP_SPACED_GRID] ) ? 2 : 1;
        }
        
        ly++;
        y1++;
        
        /* Restore munged variables. */
        x1 = s1;
        lx = s2;
    }
    
    /* Restore munged variables. */
    y1 = s3;
    ly = s4;
    
    /* s3 and s4 become w and height. */
    s3 = x2-x1;
    s4 = y2-y1;
    
    if ( loptions[OP_SPACED_GRID] )
        s3 /= 2;
    
    /* Draw the player generators now. */
    for ( s1 = 0; s1 < 10; s1++ )
    {
        if ( players[s1][0] > P_DEAD && players[s1][1] >= lx && 
             players[s1][1] <= lx+s3 && players[s1][2] >= ly &&
             players[s1][2] <= ly+s4 )
        {
            if ( loptions[OP_SPACED_GRID] )
                move( players[s1][2]-ly+y1, (players[s1][1]-lx)*2+x1 );
            else
                move( players[s1][2]-ly+y1, players[s1][1]-lx+x1 );
            colorize( colors[s1] + ((curplayer == s1) ? 8:0), 0 );
            
            /* Draw the right glyphs. */
            if ( loptions[OP_SERIAL_GLYPHS] )
            {
                if ( s1 == 1 )
                    addch( '@' );
                else
                    addch( '1'+s1-1 );
            }
            else
                addch( '@' );
        }
    }
}


/* Draw the status window that displays the score and items of interest. */
void draw_status( int x1, int y1, int x2, int y2, int curplayer )
{
    int i;                  /*< Iterator. */
    
    curplayer %= 10;
    
    /* Draw the current game type and the current rules. */
    colorize( 7, 0 );
    move( y1, x1 );
    switch ( loptions[OP_RULES] )
    {
        case OPx_SURVIVAL       : printw( "SURVIVAL " );
                                  break;
        case OPx_EXTERMINATION  : printw( "EXTERMINATION " );
                                  break;
        case OPx_PROLIFERATION  : printw( "PROLIFERATION " );
                                  break;
        case OPx_SANDBOX        : printw( "SANDBOX " );
                                  break;
        default                 : printw( "UNKNOWN MODE " );
    }
    if ( viewing || stasis )
    {
        printw( "[" );
        if ( viewing ) printw( "V" );
        if ( stasis ) printw( "S" );
        printw( "]" );
    }
    
    /* First, display the scores. */
    for ( i = 1; i < 7; i++ )
    {
        if ( players[i][0] )
        {
            colorize( colors[i] + ((curplayer - i) ? 0 : 8), 0 );
            move( y1+i, x1 );
            printw( "%d", scores[i] );
        }
    }
    colorize( 7, 0 );
    move( y1+7, x1 );
    printw( "%d", scores[0] );
    move( y1+8, x1 );
    printw( " / %d", gw*gh );
    
    colorize( colors[ curplayer ], 0 );
    
    /* Next comes the generation info. */
    move( y1+10, x1 );
    printw("GENERATION:");
    move( y1+11, x1 );
    printw( " %d", gen_no );
    if ( loptions[OP_GENERATIONS] )
    {
        move( y1+12, x1 );
        printw( "  / %d", loptions[OP_GENERATIONS] );
    }
    
    /* Now we do the seeds. */
    if ( curplayer )
    {
        move( y1+14, x1 );
        printw( "@ %d,%d", players[curplayer][1], players[curplayer][2] );
        if ( loptions[OP_STARTING_SEEDS] )
        {
            move( y1+15, x1 );
            printw( " %d seeds", seeds[curplayer] );
            move( y1+16, x1 );
            printw( "  / %d max", loptions[OP_MAX_SEEDS] );
        }
    }
    
    /* Draw the right bar. */
    colorize( 7, 0 );
    while ( y1 <= y2 )
    {
        move( y1, x2 );
        addch('|');
        y1 ++;
    }
}


/* Draw the message buffer... at least as much as we can see. */
void draw_messages( int x1, int y1, int x2, int y2 )
{
    int i, n;           /* Iterators. */
    
    colorize( 7, 0 );
    
    /* Draw the borders. */
    for ( i = x1; i <= x2; i++ )
    {
        move( y1, i ); addch( '=' );
        move( y2, i ); addch( '=' );
    }
    
    /* Draw the messages. */
    for ( i = y2-1, n = 1; i > y1 && n < MAX_MSGS; i--, n++ )
    {
        move( i, x1 );
        addnstr( msgs[ (top_msg+MAX_MSGS-n)%MAX_MSGS ], x2-x1 );
    }
}


/* Iterate the board over one generation of Life, handling colorization as
   well. This is so horribly inefficient, but for a 7DRL, I'm not concerned
   with efficiency. */
void next_generation( void )
{
    int i, j;              /*< Iterators. */
    int neighbors[10];     /*< Colored neighbor counters. */
    int c, m, b, t, w;     /*< Counters. */
    
    /* First, we store all of our neighbors in wgrid. The only option that
       affects this is OP_GRID_WRAP. We store this as a 2-digit decimal
       number. [color_of_neighbors][number_of_neighbors] */
    for ( i = 0; i < gh; i++ )
    {
        for ( j = 0; j < gw; j++ )
        {
            memset( neighbors, 0, sizeof(int)*10 );
            wgrid[ el(j,i) ] = 0;
            
            /* It's easiest to break the counting into wrapping and not-
               wrapping. In wrapped mode, there is always a neighbor. */
            if ( loptions[OP_GRID_WRAP] )
            {
                /* Temporary evaluations. */
                m = (j)?(j-1):(gw-1);       /* j-1 */
                b = (i)?(i-1):(gh-1);       /* i-1 */
                t = (j<gw-1)?(j+1):(0);     /* j+1 */
                w = (i<gh-1)?(i+1):(0);     /* i+1 */
                
                /* Top left. */
                c = lgrid[el( m,b )];
                if ( c > 9 )
                    neighbors[ c%10 ] ++;
                
                /* Top. */
                c = lgrid[el( j,b )];
                if ( c > 9 )
                    neighbors[ c%10 ] ++;
                
                /* Top right. */
                c = lgrid[el( t,b )];
                if ( c > 9 )
                    neighbors[ c%10 ] ++;
                
                /* Left. */
                c = lgrid[el( m,i )];
                if ( c > 9 )
                    neighbors[ c%10 ] ++;
                
                /* Right. */
                c = lgrid[el( t,i )];
                if ( c > 9 )
                    neighbors[ c%10 ] ++;
                
                /* Bottom left. */
                c = lgrid[el( m,w )];
                if ( c > 9 )
                    neighbors[ c%10 ] ++;
                
                /* Bottom. */
                c = lgrid[el( j,w )];
                if ( c > 9 )
                    neighbors[ c%10 ] ++;
                
                /* Bottom right. */
                c = lgrid[el( t,w )];
                if ( c > 9 )
                    neighbors[ c%10 ] ++;
            }
            else
            {
                /* Check top neighbors. */
                if ( i > 0 )
                {
                    if ( j > 0 && lgrid[ el(j-1,i-1) ] > 9 )
                        neighbors[ lgrid[ el(j-1,i-1) ] % 10 ] ++;
                    if ( lgrid[ el(j,i-1) ] > 9 )
                        neighbors[ lgrid[ el(j,i-1) ] % 10 ] ++;
                    if ( j < gw-1 && lgrid[ el(j+1,i-1) ] > 9 )
                        neighbors[ lgrid[ el(j+1,i-1) ] % 10 ] ++;
                }
                
                /* Check left neighbor. */
                if ( j > 0 && lgrid[ el(j-1,i) ] > 9 )
                    neighbors[ lgrid[ el(j-1,i) ] % 10 ] ++;
                
                /* Check right neighbor. */
                if ( j < gw-1 && lgrid[ el(j+1,i) ] > 9 )
                    neighbors[ lgrid[ el(j+1,i) ] % 10 ] ++;
                
                /* Check bottom neighbors. */
                if ( i < gh-1 )
                {
                    if ( j > 0 && lgrid[ el(j-1,i+1) ] > 9 )
                        neighbors[ lgrid[ el(j-1,i+1) ] % 10 ] ++;
                    if ( lgrid[ el(j,i+1) ] > 9 )
                        neighbors[ lgrid[ el(j,i+1) ] % 10 ] ++;
                    if ( j < gw-1 && lgrid[ el(j+1,i+1) ] > 9 )
                        neighbors[ lgrid[ el(j+1,i+1) ] % 10 ] ++;
                }
            }
            
            /* Now we count the neighbors and determine the new color of this
               cell. */
            t = neighbors[0];   /* The total number of neighbors. */
            b = 0;              /* The ID of the best color. */
            m = 0;              /* The highest value of colors. */
            w = 0;              /* Warning that there may be a tie. */
            for ( c = 1; c < 10; c++ )
            {
                t += neighbors[c];
                if ( neighbors[c] > m )
                {
                    m = neighbors[c];
                    w = 0;
                    b = c;
                }
                else if ( neighbors[c] == m )
                    w = 1;
            }
            
            /* Set the color. */
            if ( w == 1 )
                b = 0;
            wgrid[ el(j,i) ] = b*10 + t;
        }
    }
    
    /* Now go through the two arrays and set life/death based our neighbors
       array. */
    for ( i = 0; i < gw*gh; i++ )
    {
        /* Get the random value from 0-99. Bias is negligible. */
        c = rnd_31int() % 100;
        
        /* If the cell is alive, check the Survival rulestring. If not, check
           the Birth rulestring. Update the score while we're at it. */
        if ( lgrid[i] > 9 )
        {
            if ( c >= loptions[OP_RULESTRING_S( wgrid[i]%10 )] )
            {
                scores[ lgrid[i]%10 ] --;
                lgrid[i] = 0;
            }
            else if ( wgrid[i]/10 )
            {
                scores[ lgrid[i]%10 ] --;
                lgrid[i] = 10 + wgrid[i]/10;
                scores[ lgrid[i]%10 ] ++;
            }
        }
        else
        {
            /* If the cell is dead OR if it has a seed, the rules of birth
               take precedence over seed hatching. */
            if ( c < loptions[OP_RULESTRING_B( wgrid[i]%10 )] )
            {
                lgrid[i] = 10 + wgrid[i]/10;
                scores[ lgrid[i]%10 ] ++;
            }
            
            /* Spawn seeds if the player has designated hatching. */
            if ( lgrid[i] > 0 && players[ (int) lgrid[i] ][0] == P_HATCHING )
            {
                lgrid[i] += 10;
                scores[ lgrid[i]%10 ] ++;
            }
        }
    }
    
    /* Turn off all players set to hatching. Also, if a player is standing on
       an opposing color's cell while DEADLY is running, set him to dead. */
    for ( i = 0; i < 10; i++ )
    {
        if ( players[i][0] == P_HATCHING )
            players[i][0] = P_PLAYING;
            
        if ( loptions[OP_OTHER_CELLS] == OPx_DEADLY &&
             players[i][0] == P_PLAYING &&
             lgrid[ el( players[i][1], players[i][2] ) ] > 10 &&
             lgrid[ el( players[i][1], players[i][2] ) ] - 10 != i )
            players[i][0] = P_DEAD;
    }
}


/* Save a bitmap representation of the life grid to a file. If challenge mode
   is set to true, then the players will be colored by id number. Otherwise,
   they will be colored as they are in the game. */
void save_bitmap( int challenge )
{
    FILE *ofile;            /*< The file pointer. */
    int i, j;               /*< Iterators. */
    unsigned int ival;      /*< Value holder. */
    unsigned char cval;     /*< Another one. */
    unsigned char cols[8][3] =
    { {0,0,0}, {0,0,255}, {0,255,255}, {0,255,0}, {255,255,0}, {255,0,0},
      {255,0,255}, {255,255,255} };
    
    /* Get the file open. */
    memset( fname, 0, sizeof(char)*50 );
    sprintf( fname, "life%d.bmp", (int) time(NULL) );
    ofile = fopen( fname, "wb" );
    
    /* NULL check. */
    if ( ofile == NULL )
    {
        pmsg( "Could not save the screenshot for some reason..." );
        return;
    }
    
    /* Write the bitmap header data. */
    cval = 'B';                        fwrite( &cval, sizeof(char), 1, ofile );
    cval = 'M';                        fwrite( &cval, sizeof(char), 1, ofile );
    ival = 54+(gw*gh*3*4);             writeint( ival, ofile );
    ival = 0;                          writeint( ival, ofile );
    ival = 54;                         writeint( ival, ofile );
    ival = 40;                         writeint( ival, ofile );
    ival = gw*4;                       writeint( ival, ofile );
    ival = gh*4;                       writeint( ival, ofile );
    cval = 1;                          fwrite( &cval, sizeof(char), 1, ofile );
    cval = 0;                          fwrite( &cval, sizeof(char), 1, ofile );
    cval = 24;                         fwrite( &cval, sizeof(char), 1, ofile );
    cval = 0;                          fwrite( &cval, sizeof(char), 1, ofile );
    ival = 0;                          writeint( ival, ofile );
    ival = gw*gh*3*4;                  writeint( ival, ofile );
    ival = 2835;                       writeint( ival, ofile );
                                       writeint( ival, ofile );
    ival = 0;                          writeint( ival, ofile );
                                       writeint( ival, ofile );
    
    /* Write the pixel data to the file, bottom-up. */
    for ( i = (gh*4)-1; i >= 0; i-- )
    {
        for ( j = 0; j < gw*4; j++ )
        {
            /* For the border pixels, we care about the generators. For the
               center pixels, we care about the cell data. */
            if ( i%4 == 0 || i%4 == 3 || j%4 == 0 || j%4 == 3 )
            {
                ival = 0;
                for ( cval = 0; cval < 10; cval ++ )
                {
                    if ( players[cval][0] > P_DEAD && players[cval][1] == j/4
                         && players[cval][2] == i/4 )
                        ival = ( challenge ) ? cval : colors[cval];
                }
                fwrite( cols[ival], sizeof(char), 3, ofile );
            }
            else
            {
                ival = 0;
                cval = lgrid[ el( j/4, i/4 ) ];
                
                /* Get the right color. */
                if ( cval == 10 ) ival = 7;
                else              ival = cval%10;
                
                /* If it's not alive, only draw two pixels. */
                if ( cval < 9 && j%2 == i%2 )
                    ival = 0;
                
                /* Challenge mode uses id colors. */
                if ( !challenge && ival != 0 && ival != 7 )
                    ival = colors[ival];
                
                /* Write the block. */
                fwrite( cols[ival], sizeof(char), 3, ofile );
            }
        }
    }
    
    pmsg( "Saved a screenshot." );
    fclose( ofile );
}


/* Load a challenge bitmap from fname. If this is read successfully, we return
   '1', and otherwise, we return '0'. */
int load_challenge( char *fname )
{
    FILE *ifile;                /*< The input file. */
    int i, j;                   /*< Iterators. */
    unsigned int offs;          /*< Integer storage. */
    unsigned char pixels[12];   /*< Pixel input. */
    char c;                     /*< Output val. */
    
    /* Free the pointers, as we need to resize the grid. */
    free( wgrid );
    free( lgrid );
    
    /* No file? Fail! */
    ifile = fopen( fname, "rb" );
    if ( ifile == NULL )
        return 0;
    
    /* Find the pointer where the data begins and read the data. */
    fseek( ifile, 10, SEEK_SET );
    
    /* Load our data. */
    offs = readint( ifile );
    gw = readint( ifile );
    gw = readint( ifile );
    gh = readint( ifile );
    
    /* Get our grid size and make sure we can get the data. */
    gw /= 4;
    gh /= 4;
    
    lgrid = calloc( sizeof(char), gw*gh );
    wgrid = calloc( sizeof(char), gw*gh );
    if ( lgrid == NULL || wgrid == NULL )
    {
        /* Redundant error checking is redundant. */
        if ( lgrid != NULL ) free( lgrid );
        if ( wgrid != NULL ) free( wgrid );
        fclose( ifile );
        
        return 0;
    }
    
    players[1][1] = 0;
    players[1][2] = 0;
    
    /* Now start getting data. */
    fseek( ifile, offs, SEEK_SET );
    for ( i = gh-1; i >= 0; i-- )
    {
        fseek( ifile, gw*4*3, SEEK_CUR );
        for ( j = 0; j < gw; j++ )
        {
            if ( fread( pixels, sizeof(unsigned char), 12, ifile ) != 12 )
            {
                /* If we come up short, just shut down. */
                free( lgrid );
                free( wgrid );
                fclose( ifile );
                
                return 0;
            }
            
            /* Is the player here? */
            if ( !pixels[0] && !pixels[1] && pixels[2] )
            {
                players[1][1] = j;
                players[1][2] = i;
            }
            
            /* What color? */
            c = 0;
            if ( pixels[5] ) c += 1;
            if ( pixels[4] ) c += 2;
            if ( pixels[3] ) c += 4;
            c %= 7;
            
            /* Is it a seed or a cell? */
            if ( (pixels[3] || pixels[4] || pixels[5]) &&
                 (pixels[6] || pixels[7] || pixels[8]) )
            {
                scores[(int) c] += 1;
                c += 10;
            }
            lgrid[ el(j,i) ] = c;
        }
        fseek( ifile, gw*4*3*2, SEEK_CUR );
    }
    
    fclose( ifile );
    return 1;
}

