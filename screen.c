/*
 * screen.c - screen management
 *
 * Copyright © 2007 Julien Danjou <julien@danjou.info>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <X11/extensions/Xinerama.h>

#include "util.h"
#include "screen.h"
#include "tag.h"
#include "focus.h"
#include "statusbar.h"
#include "client.h"

extern AwesomeConf globalconf;

/** Get screens info
 * \param screen Screen number
 * \param statusbar statusbar
 * \param padding Padding
 * \return Area
 */
Area
get_screen_area(int screen, Statusbar *statusbar, Padding *padding)
{
    int screen_number = 0;
    XineramaScreenInfo *si;
    Area area;
    Statusbar *sb;

    if(XineramaIsActive(globalconf.display))
    {
        si = XineramaQueryScreens(globalconf.display, &screen_number);
        if (screen_number < screen)
            eprint("Info request for unknown screen.");
        area.x = si[screen].x_org;
        area.y = si[screen].y_org;
        area.width = si[screen].width;
        area.height = si[screen].height;
        XFree(si);
    }
    else
    {
        /* emulate Xinerama info but only fill the screen we want */
        area.x = 0;
        area.y = 0;
        area.width = DisplayWidth(globalconf.display, screen);
        area.height = DisplayHeight(globalconf.display, screen);
    }

     /* make padding corrections */
    if(padding)
    {
        area.x += padding->left;
        area.y += padding->top;
        area.width -= padding->left + padding->right;
        area.height -= padding->top + padding->bottom;
    }

    for(sb = statusbar; sb; sb = sb->next)
        switch(sb->position)
        {
          case Top:
            area.y += sb->height;
          case Bottom:
            area.height -= sb->height;
            break;
          case Left:
            area.x += sb->height;
          case Right:
            area.width -= sb->height;
            break;
          case Off:
            break;
        }

    return area;
}

/** Get display info
 * \param screen Screen number
 * \param statusbar the statusbar
 * \param padding Padding
 * \return Area
 */
Area
get_display_area(int screen, Statusbar *statusbar, Padding *padding)
{
    Area area;
    Statusbar *sb;

    area.x = 0;
    area.y = 0;
    area.width = DisplayWidth(globalconf.display, screen);
    area.height = DisplayHeight(globalconf.display, screen);

    for(sb = statusbar; sb; sb = sb->next)
    {
        area.y += sb->position == Top ? sb->height : 0;
        area.height -= (sb->position == Top || sb->position == Bottom) ? sb->height : 0;
    }

    /* make padding corrections */
    if(padding)
    {
            area.x += padding->left;
            area.y += padding->top;
            area.width -= padding->left + padding->right;
            area.height -= padding->top + padding->bottom;
    }
    return area;
}

/** Return the Xinerama screen number where the coordinates belongs to
 * \param x x coordinate of the window
 * \param y y coordinate of the window
 * \return screen number or DefaultScreen of disp on no match
 */
int
get_screen_bycoord(int x, int y)
{
    int i;
    Area area;

    /* don't waste our time */
    if(!XineramaIsActive(globalconf.display))
        return DefaultScreen(globalconf.display);


    for(i = 0; i < get_screen_count(); i++)
    {
        area = get_screen_area(i, NULL, NULL);
        if((x < 0 || (x >= area.x && x < area.x + area.width))
           && (y < 0 || (y >= area.y && y < area.y + area.height)))
            return i;
    }
    return DefaultScreen(globalconf.display);
}

/** Return the actual screen count
 * \return the number of screen available
 */
int
get_screen_count(void)
{
    int screen_number;

    if(XineramaIsActive(globalconf.display))
        XineramaQueryScreens(globalconf.display, &screen_number);
    else
        return ScreenCount(globalconf.display);

    return screen_number;
}

/** This returns the real X screen number for a logical
 * screen if Xinerama is active.
 * \param screen the logical screen
 * \return the X screen
 */
int
get_phys_screen(int screen)
{
    if(XineramaIsActive(globalconf.display))
        return DefaultScreen(globalconf.display);
    return screen;
}

/** Move a client to a virtual screen
 * \param c the client
 * \param new_screen The destinatiuon screen
 * \param doresize set to True if we also move the client to the new x and
 *        y of the new screen
 */
void
move_client_to_screen(Client *c, int new_screen, Bool doresize)
{
    Tag *tag;
    int old_screen = c->screen;
    Area from, to;

    for(tag = globalconf.screens[old_screen].tags; tag; tag = tag->next)
        untag_client(c, tag);

    c->screen = new_screen;

    /* tag client with new screen tags */
    tag_client_with_current_selected(c);

    if(doresize && old_screen != c->screen)
    {
        to = get_screen_area(c->screen, NULL, NULL);
        from = get_screen_area(old_screen, NULL, NULL);

        /* compute new coords in new screen */
        c->f_geometry.x = (c->f_geometry.x - from.x) + to.x;
        c->f_geometry.y = (c->f_geometry.y - from.y) + to.y;
        /* check that new coords are still in the screen */
        if(c->f_geometry.width > to.width)
            c->f_geometry.width = to.width;
        if(c->f_geometry.height > to.height)
            c->f_geometry.height = to.height;
        if(c->f_geometry.x + c->f_geometry.width >= to.x + to.width)
            c->f_geometry.x = to.x + to.width - c->f_geometry.width - 2 * c->border;
        if(c->f_geometry.y + c->f_geometry.height >= to.y + to.height)
            c->f_geometry.y = to.y + to.height - c->f_geometry.height - 2 * c->border;

        client_resize(c, c->f_geometry.x, c->f_geometry.y,
                      c->f_geometry.width, c->f_geometry.height, True, False);
    }

    focus(c, True, c->screen);

    /* redraw statusbar on all screens */
    statusbar_draw_all(old_screen);
    statusbar_draw_all(new_screen);
}

/** Move mouse pointer to x_org and y_xorg of specified screen
 * \param screen screen number
 */
static void
move_mouse_pointer_to_screen(int screen)
{
    if(XineramaIsActive(globalconf.display))
    {
        Area area = get_screen_area(screen, NULL, NULL);
        XWarpPointer(globalconf.display,
                     None,
                     DefaultRootWindow(globalconf.display),
                     0, 0, 0, 0, area.x, area.y);
    }
    else
        XWarpPointer(globalconf.display,
                     None,
                     RootWindow(globalconf.display, screen),
                     0, 0, 0, 0, 0, 0);
}


/** Switch focus to a specified screen
 * \param screen Screen ID
 * \param arg screen number
 * \ingroup ui_callback
 */
void
uicb_screen_focus(int screen, char *arg)
{
    int new_screen, numscreens = get_screen_count();

    if(arg)
        new_screen = compute_new_value_from_arg(arg, screen);
    else
        new_screen = screen + 1;

    if (new_screen < 0)
        new_screen = numscreens - 1;
    if (new_screen > (numscreens - 1))
        new_screen = 0;

    focus(focus_get_current_client(new_screen), True, new_screen);

    move_mouse_pointer_to_screen(new_screen);
}

/** Move client to a virtual screen (if Xinerama is active)
 * \param screen Screen ID
 * \param arg screen number
 * \ingroup ui_callback
 */
void
uicb_client_movetoscreen(int screen __attribute__ ((unused)), char *arg)
{
    int new_screen, prev_screen;
    Client *sel = globalconf.focus->client;

    if(!sel || !XineramaIsActive(globalconf.display))
        return;

    if(arg)
        new_screen = compute_new_value_from_arg(arg, sel->screen);
    else
        new_screen = sel->screen + 1;

    if(new_screen >= get_screen_count())
        new_screen = 0;
    else if(new_screen < 0)
        new_screen = get_screen_count() - 1;

    prev_screen = sel->screen;
    move_client_to_screen(sel, new_screen, True);
    move_mouse_pointer_to_screen(new_screen);
    arrange(prev_screen);
    arrange(new_screen);
}
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80
