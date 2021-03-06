/**
 * rofi
 *
 * MIT/X11 License
 * Copyright 2013-2015 Qball Cow <qball@gmpclient.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "rofi.h"
#include "dialogs/dmenu.h"
#include "helper.h"

struct range_pair
{
    unsigned int start;
    unsigned int stop;
};
typedef struct _DmenuModePrivateData
{
    char              *prompt;
    unsigned int      selected_line;
    char              *message;
    char              *format;
    struct range_pair * urgent_list;
    unsigned int      num_urgent_list;
    struct range_pair * active_list;
    unsigned int      num_active_list;
    // List with entries.
    char              **cmd_list;
    unsigned int      cmd_list_length;
} DmenuModePrivateData;

static char **get_dmenu ( unsigned int *length )
{
    char buffer[1024];
    char **retv = NULL;

    *length = 0;

    while ( fgets_s ( buffer, 1024, stdin, (char) config.separator ) != NULL ) {
        retv                  = g_realloc ( retv, ( ( *length ) + 2 ) * sizeof ( char* ) );
        retv[( *length )]     = g_strdup ( buffer );
        retv[( *length ) + 1] = NULL;

        // Filter out line-end.
        if ( retv[( *length )][strlen ( buffer ) - 1] == '\n' ) {
            retv[( *length )][strlen ( buffer ) - 1] = '\0';
        }

        ( *length )++;
        // Stop when we hit 2³¹ entries.
        if ( ( *length ) == INT_MAX ) {
            return retv;
        }
    }

    return retv;
}

static char ** dmenu_mode_get_data ( unsigned int *length, Switcher *sw )
{
    DmenuModePrivateData *rmpd = (DmenuModePrivateData *) sw->private_data;
    if ( rmpd->cmd_list == NULL ) {
        rmpd->cmd_list_length = 0;
        rmpd->cmd_list        = get_dmenu ( &( rmpd->cmd_list_length ) );
    }
    *length = rmpd->cmd_list_length;
    return rmpd->cmd_list;
}

static void parse_pair ( char  *input, struct range_pair  *item )
{
    int index = 0;
    for ( char *token = strsep ( &input, "-" ); token != NULL; token = strsep ( &input, "-" ) ) {
        if ( index == 0 ) {
            item->start = item->stop = (unsigned int) strtoul ( token, NULL, 10 );
            index++;
        }
        else {
            if ( token[0] == '\0' ) {
                item->stop = 0xFFFFFFFF;
            }
            else{
                item->stop = (unsigned int) strtoul ( token, NULL, 10 );
            }
        }
    }
}

static void parse_ranges ( char *input, struct range_pair **list, unsigned int *length )
{
    char *endp;
    for ( char *token = strtok_r ( input, ",", &endp ); token != NULL; token = strtok_r ( NULL, ",", &endp ) ) {
        // Make space.
        *list = g_realloc ( ( *list ), ( ( *length ) + 1 ) * sizeof ( struct range_pair ) );
        // Parse a single pair.
        parse_pair ( token, &( ( *list )[*length] ) );

        ( *length )++;
    }
}

static const char *get_display_data ( unsigned int index, void *data, G_GNUC_UNUSED int *state )
{
    Switcher             *sw    = (Switcher *) data;
    DmenuModePrivateData *pd    = (DmenuModePrivateData *) sw->private_data;
    char                 **retv = (char * *) pd->cmd_list;
    for ( unsigned int i = 0; i < pd->num_active_list; i++ ) {
        if ( index >= pd->active_list[i].start && index <= pd->active_list[i].stop ) {
            *state |= ACTIVE;
        }
    }
    for ( unsigned int i = 0; i < pd->num_urgent_list; i++ ) {
        if ( index >= pd->urgent_list[i].start && index <= pd->urgent_list[i].stop ) {
            *state |= URGENT;
        }
    }
    return retv[index];
}

/**
 * @param format The format string used. See below for possible syntax.
 * @param string The selected entry.
 * @param selected_line The selected line index.
 * @param filter The entered filter.
 *
 * Function that outputs the selected line in the user-specified format.
 * Currently the following formats are supported:
 *   * i: Print the index (0-(N-1))
 *   * d: Print the index (1-N)
 *   * s: Print input string.
 *   * q: Print quoted input string.
 *   * f: Print the entered filter.
 *   * F: Print the entered filter, quoted
 *
 * This functions outputs the formatted string to stdout, appends a newline (\n) character and
 * calls flush on the file descriptor.
 */
static void dmenu_output_formatted_line ( const char *format, const char *string, int selected_line,
                                          const char *filter )
{
    for ( int i = 0; format && format[i]; i++ ) {
        if ( format[i] == 'i' ) {
            fprintf ( stdout, "%d", selected_line );
        }
        else if ( format[i] == 'd' ) {
            fprintf ( stdout, "%d", ( selected_line + 1 ) );
        }
        else if ( format[i] == 's' ) {
            fputs ( string, stdout );
        }
        else if ( format[i] == 'q' ) {
            char *quote = g_shell_quote ( string );
            fputs ( quote, stdout );
            g_free ( quote );
        }
        else if ( format[i] == 'f' ) {
            fputs ( filter, stdout );
        }
        else if ( format[i] == 'F' ) {
            char *quote = g_shell_quote ( filter );
            fputs ( quote, stdout );
            g_free ( quote );
        }
        else {
            fputc ( format[i], stdout );
        }
    }
    fputc ( '\n', stdout );
    fflush ( stdout );
}
static void dmenu_mode_free ( Switcher *sw )
{
    if ( sw->private_data == NULL ) {
        return;
    }
    DmenuModePrivateData *pd = (DmenuModePrivateData *) sw->private_data;

    g_strfreev ( pd->cmd_list );
    g_free ( pd->urgent_list );
    g_free ( pd->active_list );

    g_free ( pd );
    sw->private_data = NULL;
}

static void dmenu_mode_init ( Switcher *sw )
{
    if ( sw->private_data != NULL ) {
        return;
    }
    sw->private_data = g_malloc0 ( sizeof ( DmenuModePrivateData ) );
    DmenuModePrivateData *pd = (DmenuModePrivateData *) sw->private_data;

    pd->prompt        = "dmenu ";
    pd->selected_line = UINT32_MAX;

    find_arg_str ( "-mesg", &( pd->message ) );

    // Check prompt
    find_arg_str (  "-p", &( pd->prompt ) );
    find_arg_uint (  "-selected-row", &( pd->selected_line ) );
    // By default we print the unescaped line back.
    pd->format = "s";

    // Allow user to override the output format.
    find_arg_str ( "-format", &( pd->format ) );
    // Urgent.
    char *str = NULL;
    find_arg_str (  "-u", &str );
    if ( str != NULL ) {
        parse_ranges ( str, &( pd->urgent_list ), &( pd->num_urgent_list ) );
    }
    // Active
    str = NULL;
    find_arg_str (  "-a", &str );
    if ( str != NULL ) {
        parse_ranges ( str, &( pd->active_list ), &( pd->num_active_list ) );
    }

    // DMENU COMPATIBILITY
    find_arg_uint (  "-l", &( config.menu_lines ) );

    /**
     * Dmenu compatibility.
     * `-b` put on bottom.
     */
    if ( find_arg ( "-b" ) >= 0 ) {
        config.location = 6;
    }
    /* -i case insensitive */
    config.case_sensitive = TRUE;
    if ( find_arg ( "-i" ) >= 0 ) {
        config.case_sensitive = FALSE;
    }
}

Switcher dmenu_mode =
{
    .name         = "dmenu",
    .keycfg       = NULL,
    .keystr       = NULL,
    .modmask      = AnyModifier,
    .init         = dmenu_mode_init,
    .get_data     = dmenu_mode_get_data,
    .result       = NULL,
    .destroy      = dmenu_mode_free,
    .token_match  = token_match,
    .mgrv         = get_display_data,
    .private_data = NULL,
    .free         = NULL
};

int dmenu_switcher_dialog ( void )
{
    dmenu_mode.init ( &dmenu_mode );
    DmenuModePrivateData *pd             = (DmenuModePrivateData *) dmenu_mode.private_data;
    char                 *input          = NULL;
    int                  retv            = FALSE;
    int                  restart         = FALSE;
    unsigned int         cmd_list_length = 0;
    char                 **cmd_list      = dmenu_mode.get_data ( &( cmd_list_length ), &dmenu_mode );

    int                  only_selected = FALSE;
    if ( find_arg ( "-only-match" ) >= 0 || find_arg ( "-no-custom" ) >= 0 ) {
        only_selected = TRUE;
        if ( cmd_list_length == 0 ) {
            return TRUE;
        }
    }
    /* copy filter string */
    input = g_strdup ( config.filter );

    char *select = NULL;
    find_arg_str ( "-select", &select );
    if ( select != NULL ) {
        char         **tokens = tokenize ( select, config.case_sensitive );
        unsigned int i        = 0;
        for ( i = 0; i < cmd_list_length; i++ ) {
            if ( token_match ( tokens, cmd_list[i], config.case_sensitive, 0, NULL ) ) {
                pd->selected_line = i;
                break;
            }
        }
        g_strfreev ( tokens );
    }

    do {
        unsigned int next_pos = pd->selected_line;
        int          mretv    = menu ( &dmenu_mode, &input, pd->prompt, &( pd->selected_line ), &next_pos, pd->message );
        // Special behavior.
        // TODO clean this up!
        if ( only_selected ) {
            /**
             * Select item mode.
             */
            restart = 1;
            if ( ( mretv & ( MENU_OK | MENU_QUICK_SWITCH ) ) && cmd_list[pd->selected_line] != NULL ) {
                dmenu_output_formatted_line ( pd->format, cmd_list[pd->selected_line], pd->selected_line, input );
                retv = TRUE;
                if ( ( mretv & MENU_QUICK_SWITCH ) ) {
                    retv = 10 + ( mretv & MENU_LOWER_MASK );
                }
                return retv;
            }
            else if ( ( mretv & MENU_CANCEL ) == MENU_CANCEL ) {
                // In no custom mode we allow canceling.
                restart = ( find_arg ( "-only-match" ) >= 0 );
            }
            pd->selected_line = next_pos - 1;
            continue;
        }
        // We normally do not want to restart the loop.
        restart = FALSE;
        // Normal mode
        if ( ( mretv & MENU_OK  ) && cmd_list[pd->selected_line] != NULL ) {
            dmenu_output_formatted_line ( pd->format, cmd_list[pd->selected_line], pd->selected_line, input );
            if ( ( mretv & MENU_SHIFT ) ) {
                restart = TRUE;
                // Move to next line.
                pd->selected_line = MIN ( next_pos, cmd_list_length - 1 );
            }
            retv = TRUE;
        }
        // Custom input
        else if ( ( mretv & ( MENU_CUSTOM_INPUT ) ) ) {
            dmenu_output_formatted_line ( pd->format, input, -1, input );
            if ( ( mretv & MENU_SHIFT ) ) {
                restart = TRUE;
                // Move to next line.
                pd->selected_line = MIN ( next_pos, cmd_list_length - 1 );
            }

            retv = TRUE;
        }
        // Quick switch with entry selected.
        else if ( ( mretv & MENU_QUICK_SWITCH ) && pd->selected_line < UINT32_MAX ) {
            dmenu_output_formatted_line ( pd->format, cmd_list[pd->selected_line], pd->selected_line, input );

            restart = FALSE;
            retv    = 10 + ( mretv & MENU_LOWER_MASK );
        }
        // Quick switch without entry selected.
        else if ( ( mretv & MENU_QUICK_SWITCH ) && pd->selected_line == UINT32_MAX ) {
            dmenu_output_formatted_line ( pd->format, input, -1, input );

            restart = FALSE;
            retv    = 10 + ( mretv & MENU_LOWER_MASK );
        }
    } while ( restart );

    g_free ( input );
    dmenu_mode.destroy ( &dmenu_mode );
    return retv;
}

