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
#ifndef _TBFS_STORAGE_MNG_H_
#define _TBFS_STORAGE_MNG_H_

#include "global.h"

typedef struct _StorageMng StorageMng;

StorageMng *tbfs_storage_mng_create (Application *app);
void tbfs_storage_mng_destroy (StorageMng *mng);

Application *tbfs_storage_mng_get_app (StorageMng *mng);
#endif
