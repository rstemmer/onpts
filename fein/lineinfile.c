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
 *  - Of by 1 in linelength when line does not end with \n
 */
#include <fein.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int ForEachLineInFile(const char *path, LineInFileCallback_t LineInFileCallback)
{
    FILE *fp;
    fp = fopen(path, "r");
    if(!fp)
    {
        fprintf(stderr, "\e[1;31mfopen(\"%s\", \"r\"); failed with error: ", path);
        fprintf(stderr, "\e[1;31m%s\e[0m\n", strerror(errno));
        return -1;
    }

    size_t linelength   = 0;
    size_t linenumber   = 0;
    char   *linebuffer  = NULL;
    size_t buffersize   = 0;
    int    retval       = 0;
    while(1)
    {

        // Get line from file
        linelength = getline(&linebuffer, &buffersize, fp);
        if(linelength == -1)
            break;

        // Update and clean data
        linenumber++;
        if(linebuffer[linelength-1] == '\n')
        {
            linebuffer[linelength-1] = '\0';    // No \n at the end
            linelength--;
        }

        // Call callback function
        retval = LineInFileCallback(path, linebuffer, linelength, linenumber);
        if(retval < 0)
            break;
    }

    free(linebuffer);
    fclose(fp);
    return retval;
}

// vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4

