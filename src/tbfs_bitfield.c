/*
 * Copyright (C) 2012-2013 Paul Ionkin <paul.ionkin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#include "tbfs_bitfield.h"

struct _Bitfield {
    guint8 *bits;
    guint32 len;
    guint32 bit_count;
    guint32 set_count;
};

#define BF_LOG "bf"

static size_t get_bytes_needed (guint32 bit_count);

/*{{{ create / destroy */
Bitfield *tbfs_bitfield_create (guint32 bit_count)
{
    Bitfield *bf;

    bf = g_new0 (Bitfield, 1);
    bf->bit_count = bit_count;
    bf->len = get_bytes_needed (bit_count);
    bf->bits = g_new0 (guint8, bf->len);
    bf->set_count = 0;

    LOG_debug (BF_LOG, "For %u bits len: %u", bit_count, bf->len);

    return bf;
}

void tbfs_bitfield_destroy (Bitfield *bf)
{
    g_free (bf->bits);
    g_free (bf);
}
/*}}}*/

static size_t get_bytes_needed (guint32 bit_count)
{
    return (bit_count + 7u) / 8u;
}

void tbfs_bitfield_set_bit (Bitfield *bf, guint32 bit)
{
    bf->bits[bit >> 3u] |= (0x80 >> (bit & 7u));
    bf->set_count++;
    
    LOG_debug (BF_LOG, "Set %u bit, %x", bit, bf->bits);
}

guint32 tbfs_bitfield_get_length (Bitfield *bf)
{
    return bf->len;
}

void *tbfs_bitfield_get_bits (Bitfield *bf)
{
    uint8_t * bits;
    
    bits = g_new0 (uint8_t, bf->len);
    memcpy (bits, bf->bits, bf->len);

    return bits;
}

guint32 tbfs_bitfield_get_set_bits (Bitfield *bf)
{
    return bf->set_count;
}
