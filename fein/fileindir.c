/*
 * A collection of functions that iterate over data using callback functions
 * Copyright (C) 2017  Ralf Stemmer <ralf.stemmer@gmx.net>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * CHANGELOG
 *
 * 18.07.17 - Ralf Stemmer <ralf.stemmer@gmx.net>
 *  - Do not call callback for "." and ".."
 */

#include <fein.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int ForEachFileInDir(const char *path, FileInDirCallback_t FileInDirCallback)
{
    DIR *dp;
    dp = opendir(path);
    if(!dp)
    {
        fprintf(stderr, "\e[1;31mopendir(\"%s\"); failed with error: ", path);
        fprintf(stderr, "\e[1;31m%s\e[0m\n", strerror(errno));
        return -1;
    }

    int retval = 0;
    while(1)
    {
        struct dirent *entry;
        entry = readdir(dp);
        if(entry == NULL)
            break;

        if(strcmp(entry->d_name, ".") == 0)
            continue;
        if(strcmp(entry->d_name, "..") == 0)
            continue;

        retval = FileInDirCallback(path, entry);
        if(retval < 0)
            break;
    }

    closedir(dp);
    return retval;
}

// vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4

