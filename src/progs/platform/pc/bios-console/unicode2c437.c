/************************************************************************
 * Copyright (C) 2013 by Hanna Reitz                                    *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

#include <stdint.h>

#define t(_1, _2) case (_1): return _2;

uint8_t get_c437_from_unicode(unsigned uni);

uint8_t get_c437_from_unicode(unsigned uni)
{
    if (uni < 0x80)
        return uni;

    switch (uni)
    {
        t(0x0000A0, 0xFF); //NO-BREAK SPACE
        t(0x0000A1, 0xAD); //¡
        t(0x0000A2, 0x9B); //¢
        t(0x0000A3, 0x9C); //£
        t(0x0000A5, 0x9D); //¥
        t(0x0000A7, 0x15); //§
        t(0x0000AA, 0xA6); //ª
        t(0x0000AB, 0xAE); //«
        t(0x0000AC, 0xAA); //¬
        t(0x0000B0, 0xF8); //°
        t(0x0000B1, 0xF1); //±
        t(0x0000B2, 0xFD); //²
        t(0x0000B5, 0xE6); //µ
        t(0x0000B7, 0xFA); //·
        t(0x0000BA, 0xA7); //º
        t(0x0000BB, 0xAF); //»
        t(0x0000BC, 0xAC); //¼
        t(0x0000BD, 0xAB); //½
        t(0x0000BF, 0xA8); //¿
        t(0x0000C4, 0x8E); //Ä
        t(0x0000C5, 0x8F); //Å
        t(0x0000C6, 0x92); //Æ
        t(0x0000C7, 0x80); //Ç
        t(0x0000C9, 0x90); //É
        t(0x0000D1, 0xA5); //Ñ
        t(0x0000D6, 0x99); //Ö
        t(0x0000DC, 0x9A); //Ü
        t(0x0000DF, 0xE1); //ß; Gleichzeitig ein β
        t(0x0000E0, 0x85); //à
        t(0x0000E1, 0xA0); //á
        t(0x0000E2, 0x83); //â
        t(0x0000E4, 0x84); //ä
        t(0x0000E5, 0x86); //å
        t(0x0000E6, 0x91); //æ
        t(0x0000E7, 0x87); //ç
        t(0x0000E8, 0x8A); //è
        t(0x0000E9, 0x82); //é
        t(0x0000EA, 0x88); //ê
        t(0x0000EB, 0x89); //ë
        t(0x0000EC, 0x8D); //ì
        t(0x0000ED, 0xA1); //í
        t(0x0000EE, 0x8C); //î
        t(0x0000EF, 0x8B); //ï
        t(0x0000F1, 0xA4); //ñ
        t(0x0000F2, 0x95); //ò
        t(0x0000F3, 0xA2); //ó
        t(0x0000F4, 0x93); //ô
        t(0x0000F6, 0x94); //ö
        t(0x0000F7, 0xF6); //÷
        t(0x0000F9, 0x97); //ù
        t(0x0000FA, 0xA3); //ú
        t(0x0000FB, 0x96); //û
        t(0x0000FC, 0x81); //ü
        t(0x0000FF, 0x98); //ÿ
        t(0x000192, 0x9F); //ƒ
        t(0x000393, 0xE2); //Γ
        t(0x000398, 0xE9); //Θ
        t(0x0003A3, 0xE4); //Σ
        t(0x0003A6, 0xE8); //Φ
        t(0x0003A9, 0xEA); //Ω
        t(0x0003B1, 0xE0); //α
        t(0x0003B4, 0xEB); //δ
        t(0x0003B5, 0xEE); //ε
        t(0x0003C0, 0xE3); //π
        t(0x0003C3, 0xE5); //σ
        t(0x0003C4, 0xE7); //τ
        t(0x0003C6, 0xED); //φ
        t(0x00201C, 0x22); //“
        t(0x00201D, 0x22); //”
        t(0x00201E, 0x22); //„
        t(0x002022, 0x07); //•
        t(0x00203C, 0x13); //‼
        t(0x00207F, 0xFC); //SUPERSCRIPT LATIN SMALL LETTER N
        t(0x0020A7, 0x9E); //₧
        t(0x002190, 0x1B); //←
        t(0x002191, 0x18); //↑
        t(0x002192, 0x1A); //→
        t(0x002193, 0x19); //↓
        t(0x002194, 0x1D); //↔
        t(0x002195, 0x12); //↕
        t(0x0021A8, 0x17); //↨
        t(0x002219, 0xF9); //∙
        t(0x00221A, 0xFB); //√
        t(0x00221E, 0xEC); //∞
        t(0x00221F, 0x1C); //∟
        t(0x002229, 0xEF); //∩
        t(0x002248, 0xF7); //≈
        t(0x002261, 0xF0); //≡
        t(0x002264, 0xF3); //≤
        t(0x002265, 0xF2); //≥
        t(0x002302, 0x7F); //⌂
        t(0x002310, 0xA9); //⌐
        t(0x002320, 0xF4); //⌠
        t(0x002321, 0xF5); //⌡
        t(0x002500, 0xC4); //─
        t(0x002502, 0xB3); //│
        t(0x00250C, 0xDA); //┌
        t(0x002510, 0xBF); //┐
        t(0x002514, 0xC0); //└
        t(0x002518, 0xD9); //┘
        t(0x00251C, 0xC3); //├
        t(0x002524, 0xB4); //┤
        t(0x00252C, 0xC2); //┬
        t(0x002534, 0xC1); //┴
        t(0x00253C, 0xC5); //┼
        t(0x002550, 0xCD); //═
        t(0x002551, 0xBA); //║
        t(0x002552, 0xD5); //╒
        t(0x002553, 0xD6); //╓
        t(0x002554, 0xC9); //╔
        t(0x002555, 0xB8); //╕
        t(0x002556, 0xB7); //╖
        t(0x002557, 0xBB); //╗
        t(0x002558, 0xD4); //╘
        t(0x002559, 0xD3); //╙
        t(0x00255A, 0xC8); //╚
        t(0x00255B, 0xBE); //╛
        t(0x00255C, 0xBD); //╜
        t(0x00255D, 0xBC); //╝
        t(0x00255E, 0xC6); //╞
        t(0x00255F, 0xC7); //╟
        t(0x002560, 0xCC); //╠
        t(0x002561, 0xB5); //╡
        t(0x002562, 0xB6); //╢
        t(0x002563, 0xB9); //╣
        t(0x002564, 0xD1); //╤
        t(0x002565, 0xD2); //╥
        t(0x002566, 0xCB); //╦
        t(0x002567, 0xCF); //╧
        t(0x002568, 0xD0); //╨
        t(0x002569, 0xCA); //╩
        t(0x00256A, 0xD8); //╪
        t(0x00256B, 0xD7); //╫
        t(0x00256C, 0xCE); //╬
        t(0x002580, 0xDF); //UPPER HALF BLOCK
        t(0x002584, 0xDC); //LOWER HALF BLOCK
        t(0x002588, 0xDB); //FULL BLOCK
        t(0x00258C, 0xDD); //LEFT HALF BLOCK
        t(0x002590, 0xDE); //RIGHT HALF BLOCK
        t(0x002591, 0xB0); //LIGHT SHADE
        t(0x002592, 0xB1); //MEDIUM SHADE
        t(0x002593, 0xB2); //DARK SHADE
        t(0x0025A0, 0xFE); //■
        t(0x0025AC, 0x16); //BLACK RECTANGLE
        t(0x0025B2, 0x1E); //BLACK UP-POINTING TRIANGLE
        t(0x0025BA, 0x10); //BLACK RIGHT-POINTING POINTER
        t(0x0025BC, 0x1F); //BLACK DOWN-POINTING TRIANGLE
        t(0x0025C4, 0x11); //BLACK LEFT-POINTING POINTER
        t(0x0025CB, 0x09); //WHITE CIRCLE
        t(0x0025D8, 0x08); //INVERSE BULLET
        t(0x0025D9, 0x0A); //INVERSE WHITE CIRCLE
        t(0x00263A, 0x01); //☺
        t(0x00263B, 0x02); //☻
        t(0x00263C, 0x0F); //☼
        t(0x002640, 0x0C); //♀
        t(0x002642, 0x0B); //♂
        t(0x002660, 0x06); //♠
        t(0x002663, 0x05); //♣
        t(0x002665, 0x03); //♥
        t(0x002666, 0x04); //♦
        t(0x00266A, 0x0D); //♪
        t(0x00266B, 0x0E); //♫
    }

    return 0xFE; //Ein kleines Rechteck als Platzhalter
}
