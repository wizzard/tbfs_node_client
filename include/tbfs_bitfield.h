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
#ifndef _TBFS_BITFIELD_H_
#define _TBFS_BITFIELD_H_

#include "global.h"

typedef struct _Bitfield Bitfield;

Bitfield *tbfs_bitfield_create (guint32 bit_count);
void tbfs_bitfield_destroy (Bitfield *bf);

void tbfs_bitfield_set_bit (Bitfield *bf, guint32 bit);
guint32 tbfs_bitfield_get_length (Bitfield *bf);
void *tbfs_bitfield_get_bits (Bitfield *bf);

guint32 tbfs_bitfield_get_set_bits (Bitfield *bf);

#endif
