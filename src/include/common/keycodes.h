/************************************************************************
 * Copyright (C) 2010 by Hanna Reitz                                    *
 * xanclic@googlemail.com                                               *
 *                                                                      *
 * This program is free software; you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation;  either version 2 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program  is  distributed  in the hope  that it will  be useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied  warranty of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the  GNU  General  Public License *
 * along with this program; if not, write to the                        *
 * Free Software Foundation, Inc.,                                      *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.            *
 ************************************************************************/

#ifndef _KEYCODES_H
#define _KEYCODES_H

#define KEY_BACKSPACE '\b'
#define KEY_ENTER     '\n'
#define KEY_ESCAPE    '\33'
#define KEY_TAB       '\t'

#define KEY_TYPE     0xFFFE
#define KEY_LOCATION 0x0001
#define KEY_IS_LEFT  0x0000
#define KEY_IS_RIGHT 0x0001

#define KEY_CONTROL  0xE000
#define KEY_LCONTROL (KEY_CONTROL | KEY_IS_LEFT )
#define KEY_RCONTROL (KEY_CONTROL | KEY_IS_RIGHT)
#define KEY_MENU     0xE002
#define KEY_LMENU    (KEY_MENU    | KEY_IS_LEFT )
#define KEY_RMENU    (KEY_MENU    | KEY_IS_RIGHT)
#define KEY_ALT_GR   KEY_RMENU
#define KEY_META     0xE004
#define KEY_LMETA    (KEY_META    | KEY_IS_LEFT )
#define KEY_RMETA    (KEY_META    | KEY_IS_RIGHT)
#define KEY_SHIFT    0xE006
#define KEY_LSHIFT   (KEY_SHIFT   | KEY_IS_LEFT )
#define KEY_RSHIFT   (KEY_SHIFT   | KEY_IS_RIGHT)

#define KEY_CAPSLOCK   0xE008
#define KEY_NUMLOCK    0xE009
#define KEY_SCROLLLOCK 0xE00A
#define KEY_DELETE     0xE00B
#define KEY_INSERT     0xE00C
#define KEY_HOME       0xE00D
#define KEY_END        0xE00E
#define KEY_UP         0xE010
#define KEY_DOWN       0xE011
#define KEY_LEFT       0xE012
#define KEY_RIGHT      0xE013
#define KEY_PGUP       0xE014
#define KEY_PGDOWN     0xE015

#define KEY_F1  0xE030
#define KEY_F2  0xE031
#define KEY_F3  0xE032
#define KEY_F4  0xE033
#define KEY_F5  0xE034
#define KEY_F6  0xE035
#define KEY_F7  0xE036
#define KEY_F8  0xE037
#define KEY_F9  0xE038
#define KEY_F10 0xE039
#define KEY_F11 0xE03A
#define KEY_F12 0xE03B

#endif
