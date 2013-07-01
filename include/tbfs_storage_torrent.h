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
#ifndef _TBFS_STORAGE_TORRENT_H_
#define _TBFS_STORAGE_TORRENT_H_

#include "global.h"
#include "tbfs_storage_mng.h"

typedef struct _StorageTorrent StorageTorrent;

StorageTorrent *tbfs_storage_torrent_create (StorageMng *mng, const gchar *info_hash);
void tbfs_storage_torrent_destroy (StorageTorrent *storage);

const gchar *tbfs_storage_torrent_get_info_hash (StorageTorrent *storage);

void tbfs_storage_torrent_write_piece_buf (StorageTorrent *storage, guint32 piece_idx, guint32 offset, struct evbuffer *in_buf);

#endif
