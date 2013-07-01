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
#include "wutils.h"

int wutils_daemonize (void)
{
    int fd;

    switch (fork ()) {
        case -1:
            return 0;
        case 0:
            break;
        default:
            exit(0);
    }

    getpid ();

    if (setsid () == -1) {
        return 0;
    }

    umask (0);

    fd = open ("/dev/null", O_RDWR);
    if (fd == -1) {
        return 0;
    }

    if (dup2 (fd, 0) == -1) {
        return 0;
    }

    if (dup2 (fd, 1) == -1) {
        return 0;
    }

    if (fd > 2) {
        if (close (fd) == -1) {
            return 0;
        }
    }

    return 1;

}
