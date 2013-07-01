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
#ifndef _TBFS_MNG_H_
#define _TBFS_MNG_H_

#include "global.h"
#include "tbfs_torrent.h"

TBFSMng *tbfs_mng_create (Application *app);
void tbfs_mng_destroy (TBFSMng *mng);

Torrent *tbfs_mng_torrent_register (TBFSMng *mng, const gchar *info_hash, guint32 total_pieces);
Torrent *tbfs_mng_torrent_get (TBFSMng *mng, const gchar *info_hash);

#endif
