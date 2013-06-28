/*
 *  HackOfLife - game.c
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
 *  Handles the entire game. Everything begins and ends here.
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
#include "world.h"
#include "network.h"
#include "random.h"


static int running;                 /*< True while the game is running. */
static char sbuffer[1024];          /*< Input string buffer. */
static char cfgfile[50];            /*< Filename of the .lifecf file. */

/* Our sets of menus. */
static const char *mmenu[] =
    {"CHOOSE A GAME MODE:"," CONQUEST"," CHALLENGE"," HOTSEAT"," NETPLAY",
     " CONFIG"," QUIT"};
static const char *pmenu[] =
    {"ENTER NUMBER OF PLAYERS"," 1"," 2"," 3"," 4"," 5"," 6"," BACK"};
static const char *nmenu[] =
    {"NETWORK GAME:"," HOST"," JOIN"," BACK"};
static const char *omenu[] =
    {"CONFIGURATION:"," PLAYER COLOR"," SERIAL GLYPHS"," SPACED GRID",
     " TERMINAL HEIGHT"," TERMINAL WIDTH"," STARTING SEEDS"," MAX SEEDS",
     " GRID WIDTH"," GRID HEIGHT"," GRID_WRAP"," NOISE"," OTHER CELLS",
     " TIMEOUT"," NET HANG"," GENERATIONS"," RULES"," RULESTRING S(0)",
     " RULESTRING S(1)"," RULESTRING S(2)"," RULESTRING S(3)",
     " RULESTRING S(4)"," RULESTRING S(5)"," RULESTRING S(6)",
     " RULESTRING S(7)"," RULESTRING S(8)"," RULESTRING B(0)",
     " RULESTRING B(1)"," RULESTRING B(2)"," RULESTRING B(3)",
     " RULESTRING B(4)"," RULESTRING B(5)"," RULESTRING B(6)",
     " RULESTRING B(7)"," RULESTRING B(8)"," SAVE OPTIONS"," EXIT"};
static const char *cmenu[] =
    {"COLOR:"," RED"," YELLOW"," GREEN"," CYAN"," BLUE"," MAGENTA"};
static const char *bmenu[] =
    {"OTHER CELLS:"," PASSIVE"," SOLID"," DEADLY"};
static const char *rmenu[] =
    {"RULES:"," SURVIVAL"," EXTERMINATION"," PROLIFERATION"," SANDBOX"};


/* The main method. Program execution begins here. */
int main( int argc, char **argv )
{
    int i, c, n;        /*< Iterator and temp vals. */
    char *m;            /*< Standard issue char pointer. */
    
    /* Load our options. If no conf file exists, then we load the default
       parameters. */
    load_options();
    
    /* Load command line parameters now. These override ALL options. */
    for ( i = 0; i < argc; i++ )
    {
        if ( strncmp( argv[i], "-w=", 3 ) == 0 )
            sscanf( argv[i]+3, "%d", life_opts()+OP_TERMINAL_WIDTH );
        else if ( strncmp( argv[i], "-h=", 3 ) == 0 )
            sscanf( argv[i]+3, "%d", life_opts()+OP_TERMINAL_HEIGHT );
    }
    
    /* Prepare the environment. */
    set_size(life_opts()[OP_TERMINAL_WIDTH], life_opts()[OP_TERMINAL_HEIGHT]);
    start_ui();                     /* Start curses. */
    init_genrand( time(NULL) );     /* Load the Mersenne Twister. */
    
    /* Woohoo! Main loop. Fun stuff. */
    running = 1;
    while ( ( running = go_menu( 1, 1, 30, 7, 6, mmenu, 0 ) ) != 5 )
    {
        erase();
        switch ( running )
        {
            /* Conquest mode - play through random rounds, trying to clear as
               many as possible. */
            case 0: i = go_menu( 1, 1, 30, 8, 7, pmenu, 0 );
                    if ( i != 6 )
                        play_game( G_CONQUEST, i );
                    break;
            
            /* Challenge mode - try to beat a pre-configured map. */
            case 1: play_game( G_CHALLENGE, 5 );
                    break;
            
            /* Hotseat mode - Play life with your friends on the same
               computer. */
            case 2: i = go_menu( 1, 1, 30, 8, 7, pmenu, 0 );
                    if ( i != 6 )
                        play_game( G_HOTSEAT, i );
                    break;
            
            /* Network mode - Same as hotseat, but everyone gets his own
               computer! */
            case 3: i = go_menu( 1, 1, 30, 4, 3, nmenu, 0 );
                    erase();
                    if ( i == 0 )
                    {
                        net_start();
                        get_string( 1, 1, 10, "Port Number", sbuffer );
                        
                        /* Start the server and wait for some folks to join.
                           Then send everyone the rules and the number of
                           players in plaintext. */
                        if ( ( n = net_host( sbuffer ) ) )
                        {
                            /* Collect the rules and the number of players. */
                            memset( sbuffer, 0, sizeof(char)*1024 );
                            m = sbuffer;
                            for ( i = 5; i < OP_N; i++ )
                                m += sprintf( m, "%d ", life_opts()[i] );
                            sprintf( m, "%d", n );
                            
                            /* Send them to everyone. */
                            for ( i = 0; i < n; i++ )
                                send_message( sbuffer, i );
                            
                            erase();
                            move( 1, 1 );
                            printw( "Waiting for players to respond. This" );
                            move( 2, 1 );
                            printw( "should not take long. If a player fails");
                            move( 3, 1 );
                            printw( "to respond, kill their connection by");
                            move( 4, 1 );
                            printw( "pressing 'X': " );
                            refresh();
                            
                            /* Wait for everyone to respond. */
                            for ( i = 0; i < n; i++ )
                            {
                                m = wait_message( i, -1 );
                                
                                if ( m == NULL )
                                    kill_client( i );
                                
                                printw( "O " );
                                refresh();
                            }
                            
                            /* Now start the game. */
                            play_game( G_NETPLAY, n );
                        }
                        
                        net_end();
                    }
                    else if ( i == 1 )
                    {
                        net_start();
                        get_string( 1, 1, 50, "Host and Port", sbuffer );
                        for ( i = 0; i < 50 && sbuffer[i] != ' '; i++ ) {};
                        sbuffer[i] = 0;
                        if ( net_connect( sbuffer, sbuffer+i+1 ) )
                        {
                            erase();
                            move( 1,1 );
                            printw( "Connected with the server. Waiting for" );
                            move( 2,1 );
                            printw( "the game to start... this might take a" );
                            move( 3,1 );
                            printw( "while. Press 'X' to cancel.");
                            refresh();
                            
                            /* Now wait for the server. */
                            m = wait_message( 0, -1 );
                            
                            /* Get the rules from 'm' */
                            for ( i = 5; i < OP_N && m != NULL; i++ )
                            {
                                if ( sscanf( m, "%d", life_opts()+i ) == 1 )
                                {
                                    m = strchr( m, ' ' );
                                    if ( m ) m++;
                                }
                                else
                                    m = NULL;
                            }
                            
                            /* Get the number of players. */
                            if ( !( m && sscanf( m, "%d", &n ) == 1 ) )
                                m = NULL;
                            
                            /* Let the server know we're ready. */
                            if ( m != NULL )
                            {
                                send_message( "OK", 0 );
                                play_game( G_NETPLAY, n );
                            }
                        }
                        net_end();
                    }
                    break;
            
            /* Change the game's options. */
            case 4:
                    while ( ( i = go_menu(1,1,tw()-2,th()-2,OP_N+2,omenu,1 ) )
                              != OP_N + 1 )
                    {
                        erase();
                        if ( i == OP_N )
                            save_options();
                        else if ( i == OP_PLAYER_COLOR )
                            life_opts()[i] = go_menu(1,1,20,7,6,cmenu,0)+1;
                        else if ( i == OP_OTHER_CELLS )
                            life_opts()[i] = go_menu(1, 1, 20, 4, 3, bmenu, 0);
                        else if ( i == OP_RULES )
                            life_opts()[i] = go_menu(1, 1, 20, 5, 4, rmenu, 0);
                        else if ( i == OP_SERIAL_GLYPHS || i == OP_SPACED_GRID
                                  || i == OP_GRID_WRAP || i == OP_NOISE )
                            life_opts()[i] = (life_opts()[i]) ? 0: 1;
                        else
                        {
                            get_string( 1, 1, 30, omenu[i+1], sbuffer );
                            (void) sscanf( sbuffer, "%d", &c );
                            life_opts()[i] = c;
                        }
                        erase();
                        validate_options();
                    }
                    break;
            
            /* You can't go here...? */
            default:
                    break;
        }
        erase();
    }
    
    /* Now we're done. Time to clean up. */
    end_ui();               /* Deactivate curses. */
    return EXIT_SUCCESS;    /* Return successfully and say good-bye. */
}


/* Load options from the configuration file. If, for some reason there is no
   configuration file, load the defaults, and make a configuration file. */
void load_options( void )
{
    FILE *ifile;                    /*< The option file. */
    int i;                          /*< Iterator. */
    int *opts = life_opts();        /*< Option array! */
    
    if ( getenv("HOME") != NULL )
        strncpy( cfgfile, getenv("HOME"), 35);
    else
        strcpy( cfgfile, "." );
    
    #ifdef WIN32
        strcat( cfgfile, "\\.lifecf" );
    #else
        strcat( cfgfile, "/.lifecf" );
    #endif
    
    /* Try to load from a file. */
    ifile = fopen( cfgfile, "r" );
    if ( ifile )
    {
        i = 0;
        while ( i < OP_N && fscanf( ifile, "%d", opts+i ) == 1 )
            i++;
        fclose( ifile );
        
        if ( i == OP_N && validate_options() )
            return;
    }
    
    /* If we can't do that, then we set the default values. */
    opts[OP_PLAYER_COLOR]       = 1;
    opts[OP_SERIAL_GLYPHS]      = 0;
    opts[OP_SPACED_GRID]        = 0;
    opts[OP_TERMINAL_HEIGHT]    = 24;
    opts[OP_TERMINAL_WIDTH]     = 80;
    opts[OP_STARTING_SEEDS]     = 100;
    opts[OP_MAX_SEEDS]          = 999;
    opts[OP_GRID_WIDTH]         = 20;
    opts[OP_GRID_HEIGHT]        = 20;
    opts[OP_GRID_WRAP]          = 1;
    opts[OP_NOISE]              = 0;
    opts[OP_OTHER_CELLS]        = OPx_PASSIVE;
    opts[OP_TIMEOUT]            = 0;
    opts[OP_NET_HANG]           = 500;
    opts[OP_GENERATIONS]        = 10000;
    opts[OP_RULES]              = OPx_SURVIVAL;
    
    /* Set the default S rulestring. */
    opts[OP_RULESTRING_S(0)]    = 0;
    opts[OP_RULESTRING_S(1)]    = 0;
    opts[OP_RULESTRING_S(2)]    = 100;
    opts[OP_RULESTRING_S(3)]    = 100;
    opts[OP_RULESTRING_S(4)]    = 0;
    opts[OP_RULESTRING_S(5)]    = 0;
    opts[OP_RULESTRING_S(6)]    = 0;
    opts[OP_RULESTRING_S(7)]    = 0;
    opts[OP_RULESTRING_S(8)]    = 0;
    
    /* Set the default B rulestring. */
    opts[OP_RULESTRING_B(0)]    = 0;
    opts[OP_RULESTRING_B(1)]    = 0;
    opts[OP_RULESTRING_B(2)]    = 0;
    opts[OP_RULESTRING_B(3)]    = 100;
    opts[OP_RULESTRING_B(4)]    = 0;
    opts[OP_RULESTRING_B(5)]    = 0;
    opts[OP_RULESTRING_B(6)]    = 0;
    opts[OP_RULESTRING_B(7)]    = 0;
    opts[OP_RULESTRING_B(8)]    = 0;
}


/* Save the options to a configuration file. */
void save_options( void )
{
    FILE *ofile;                    /*< The option file. */
    int i;                          /*< Iterator. */
    int *opts = life_opts();        /*< Option array! */
    
    ofile = fopen( cfgfile, "w" );
    if ( ofile )
    {
        for ( i = 0; i < OP_N; i++ )
            fprintf( ofile, "%d ", opts[i] );
        fclose( ofile );
    }
}


/* Make sure our options are sane. Returns 0 if invalid options were found.
   If invalid options are found, the game will abort, and the options in
   question will be reset to their original values. */
int validate_options( void )
{
    int i, val, report = 1;         /*< Our report variable. */
    int *opts = life_opts();        /*< Option array! */
    
    val = opts[OP_PLAYER_COLOR];
    if ( val < 1 || val > 6 )
    {
        report = 0;
        opts[OP_PLAYER_COLOR] = 1;
    }
    
    val = opts[OP_SERIAL_GLYPHS];
    if ( val < 0 || val > 1 )
    {
        report = 0;
        opts[OP_SERIAL_GLYPHS] = 0;
    }
    
    val = opts[OP_SPACED_GRID];
    if ( val < 0 || val > 1 )
    {
        report = 0;
        opts[OP_SPACED_GRID] = 0;
    }
    
    val = opts[OP_TERMINAL_HEIGHT];
    if ( val < 22 )
    {
        report = 0;
        opts[OP_TERMINAL_HEIGHT] = 24;
    }
    
    val = opts[OP_TERMINAL_WIDTH];
    if ( val < 30 )
    {
        report = 0;
        opts[OP_TERMINAL_WIDTH] = 80;
    }
    
    val = opts[OP_STARTING_SEEDS];
    if ( val < 0 || val > 1000000000 )
    {
        report = 0;
        opts[OP_STARTING_SEEDS] = 100;
    }
    
    val = opts[OP_MAX_SEEDS];
    if ( val < 1 || val > 1000000000 )
    {
        report = 0;
        opts[OP_MAX_SEEDS] = 999;
    }
    
    val = opts[OP_GRID_WIDTH];
    if ( val < 5 || val > 10000 )
    {
        report = 0;
        opts[OP_GRID_WIDTH] = 20;
    }
    
    val = opts[OP_GRID_HEIGHT];
    if ( val < 5 || val > 10000 )
    {
        report = 0;
        opts[OP_GRID_HEIGHT] = 20;
    }
    
    val = opts[OP_GRID_WRAP];
    if ( val < 0 || val > 1 )
    {
        report = 0;
        opts[OP_GRID_WRAP] = 1;
    }
    
    val = opts[OP_NOISE];
    if ( val < 0 || val > 1 )
    {
        report = 0;
        opts[OP_NOISE] = 0;
    }
    
    val = opts[OP_OTHER_CELLS];
    if ( val < OPx_PASSIVE || val > OPx_DEADLY )
    {
        report = 0;
        opts[OP_GRID_WRAP] = OPx_PASSIVE;
    }
    
    val = opts[OP_TIMEOUT];
    if ( val < 0 || val > 60000 )
    {
        report = 0;
        opts[OP_TIMEOUT] = 0;
    }
    
    val = opts[OP_NET_HANG];
    if ( val < 1 || val > 60000 )
    {
        report = 0;
        opts[OP_NET_HANG] = 500;
    }
    
    val = opts[OP_GENERATIONS];
    if ( val < 0 || val > 1000000000 )
    {
        report = 0;
        opts[OP_GENERATIONS] = 10000;
    }
    
    val = opts[OP_RULES];
    if ( val < OPx_SURVIVAL || val > OPx_SANDBOX )
    {
        report = 0;
        opts[OP_RULES] = OPx_SURVIVAL;
    }
    
    for ( i = 0; i < 9; i++ )
    {
        val = opts[OP_RULESTRING_S(i)];
        if ( val < 0 || val > 100 )
        {
            report = 0;
            opts[OP_RULESTRING_S(i)] = 0;
        }
        
        val = opts[OP_RULESTRING_B(i)];
        if ( val < 0 || val > 100 )
        {
            report = 0;
            opts[OP_RULESTRING_B(i)] = 0;
        }
    }
    
    set_size( opts[OP_TERMINAL_WIDTH], opts[OP_TERMINAL_HEIGHT] );
    return report;
}


/* Open a menu with a list of options. This traps input until the player
   activates one of the selections. If 'go' is true, then this menu will
   display the game options beside their names for the option menu editor,
   since I'm too lazy to implement that somewhere else. */
int go_menu( int x, int y, int w, int h, int s, const char *options[], int go )
{
    int i, c, selection;        /*< Iterator and input */
    int top;                    /*< The top index, for scrolling menus. */
    int started;                /*< Drawing flag. */
    
    /* Prepare default values. */
    top = 0;
    i = 0;
    selection = -1;
    started = 0;
    c = ERR;
    
    /* Loop until we get an answer. */
    while ( selection == -1 )
    {
        napms(10);
        c = getch();
        
        /* If we didn't click anything, then restart the loop. */
        if ( started && c == ERR )
            continue;
        
        /* You can go up or go down. */
        if ( c == KEY_DOWN || c == 'j' )
        {
            i++;
            if ( i == s )
                i = 0;
        }
        else if ( c == KEY_UP || c == 'k' )
        {
            i--;
            if ( i == -1 )
                i = s-1;
        }
        else if ( c == '\n' || c == '\r' )
        {
            selection = i;
        }
        
        /* Scroll the menu if it's necessary. */
        if ( i < top )
            top = i;
        if ( i > top+h-2 )
            top = i-h+2;
        
        /* Draw the updated screen. */
        colorize( life_opts()[OP_PLAYER_COLOR], 0 );
        erasea(x,y,x+w,y+h);
        
        /* Draw the header. */
        move( y, x );
        addnstr( options[0], w );
        
        /* Draw the options. */
        for ( c = 0; c < h-1 && c < s; c++ )
        {
            move( y+c+1, x );
            if ( i == top+c )
                colorize( 0, life_opts()[OP_PLAYER_COLOR] );
                
            addnstr( options[ top+c+1 ], w );
            if ( go && top+c < OP_N )
            {
                move( y+c+1, 20 );
                if ( top+c == OP_PLAYER_COLOR )
                {
                    switch ( life_opts()[ top+c ] )
                    {
                        case 1: printw( "Red" );
                                break;
                        case 2: printw( "Yellow" );
                                break;
                        case 3: printw( "Green" );
                                break;
                        case 4: printw( "Cyan" );
                                break;
                        case 5: printw( "Blue" );
                                break;
                        case 6: printw( "Magenta" );
                                break;
                        default:printw( "INVALID" );
                                break;
                    }
                }
                else if ( top+c == OP_OTHER_CELLS )
                {
                    switch ( life_opts()[ top+c ] )
                    {
                        case OPx_PASSIVE: printw( "Passive" );
                                          break;
                        case OPx_SOLID  : printw( "Solid" );
                                          break;
                        case OPx_DEADLY : printw( "Deadly" );
                                          break;
                        default         : printw( "INVALID" );
                                          break;
                    }
                }
                else if ( top+c == OP_RULES )
                {
                    switch ( life_opts()[ top+c ] )
                    {
                        case OPx_SURVIVAL       : printw( "Survival" );
                                                  break;
                        case OPx_EXTERMINATION  : printw( "Extermination" );
                                                  break;
                        case OPx_PROLIFERATION  : printw( "Proliferation" );
                                                  break;
                        case OPx_SANDBOX        : printw( "Sandbox" );
                                                  break;
                        default                 : printw( "INVALID" );
                                                  break;
                    }
                }
                else if ( top+c == OP_SERIAL_GLYPHS || top+c == OP_SPACED_GRID
                          || top+c == OP_GRID_WRAP || top+c == OP_NOISE )
                {
                    if ( life_opts()[ top+c ] ) printw( "ON" );
                    else                          printw( "OFF" );
                }
                else
                {
                    printw( "%d", life_opts()[ top+c ] );
                }
            }
            
            if ( i == top+c )
                colorize( life_opts()[OP_PLAYER_COLOR], 0 );
        }
        
        /* Refresh and mark! */
        refresh();
        started = 1;
    }
    
    return selection;
}


/* Get a string from the player. Used to get option parameters and the hostname
   when trying to connect to a server. */
void get_string( int x, int y, int w, const char *header, char *input )
{
    int i, c, working;          /*< Iterator and flag variables. */
    
    /* Set up default values. */
    memset( input, 0, w );
    colorize( life_opts()[OP_PLAYER_COLOR], 0 );
    erasea( x, y, x+w, y+2 );
    working = 1;
    i = 0;
    
    move( y, x );
    printw( "%s", header );
    move( y+1, x );
    colorize( 0, life_opts()[OP_PLAYER_COLOR] );
    addch( ' ' );
    refresh();
    
    while ( working )
    {
        napms(10);
        c = getch();
        
        if ( c == '\n' || c == '\r' )
            working = 0;
        else if ( c > 0 && c < 0x100 && isprint(c) )
        {
            if ( i < w-1 )
            {
                input[i] = c;
                i++;
            }
        }
        else if ( c == KEY_BACKSPACE || c == '\b' || c == 127 )
        {
            if ( i > 0 )
            {
                i--;
                input[i] = 0;
            }
        }
        
        /* Print the string! */
        colorize( life_opts()[OP_PLAYER_COLOR], 0 );
        erasea( x, y+1, x+w, y+1 );
        move( y+1, x );
        printw( "%s", input );
        colorize( 0, life_opts()[OP_PLAYER_COLOR] );
        addch( ' ' );
        refresh();
    }
    colorize( 7, 0 );
}


/* Block the game until a key is pressed. */
void blocker( void )
{
    flushinp();
    while ( getch() == ERR )
        napms(10);
}


/* Writes an integer to a file in 32-bit little-endian form, without relying on
   cutesy bit-flipping tricks. */
void writeint( unsigned int i, FILE *ofile )
{
    unsigned char c[4];     /*< Our pure data. */
    
    /* Use pure maths to build it. */
    c[0] = i % 0x100;
    c[1] = (i / 0x100)%100;
    c[2] = (i / 0x10000)%100;
    c[3] = (i / 0x1000000)%100;
    
    fwrite( c, sizeof(unsigned char), 4, ofile );
}

/* Reads a 32-bit, little-endian integer out of a file in a platform-
   independent manner. */
unsigned int readint( FILE *ifile )
{
    unsigned int report;        /*< The value we're reporting. */
    unsigned char c[4];         /*< Our pure data. */
    
    /* We're really ignoring the return value. But don't tell the compiler. */
    report = fread( c, sizeof(unsigned char), 4, ifile );
    
    /* Calculate the report value with pure maths. */
    report  = c[3];
    report *= 0x100;
    report += c[2];
    report *= 0x100;
    report += c[1];
    report *= 0x100;
    report += c[0];
    
    return report;
}

