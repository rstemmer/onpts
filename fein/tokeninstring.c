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

#include <fein.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int ForEachTokenInString(const char *str, const char *delimiters, TokenInStringCallback_t TokenInStringCallback)
{
    // Make a copy of the string because strtok changes its content
    char *tokens;
    tokens = (char*)malloc(strlen(str) + 1);
    if(tokens == NULL)
    {
        fprintf(stderr, "\e[1;31mmalloc(%d); failed with error: ", strlen(str)+1);
        fprintf(stderr, "\e[1;31m%s\e[0m\n", strerror(errno));
        return -1;
    }

    strcpy(tokens, str);

    char *token;
    int retval = 0;
    token = strtok(tokens, delimiters);
    while(token != NULL)
    {
        retval = TokenInStringCallback(str, delimiters, (const char*)token);
        if(retval < 0)
            break;

        token = strtok(NULL, delimiters);
    }

    free(tokens);
    return retval;
}

// vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4

