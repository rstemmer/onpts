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

#ifndef LIBFEIN_H
#define LIBFEIN_H

#define _GNU_SOURCE
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

/*
 * int LineInFileCallback(
 *  const char *filename, 
 *  const char *line, 
 *  size_t linelength, 
 *  size_t linenumber)
 */
typedef int (*LineInFileCallback_t)(const char*, const char*, size_t, size_t);
int ForEachLineInFile(const char *filename, LineInFileCallback_t LineInFileCallback);

/*
 * int FileInDirCallback(
 *  const char *dirpath,
 *  struct dirent *entry
 *  )
 */
typedef int (*FileInDirCallback_t)(const char*, struct dirent*);
int ForEachFileInDir(const char *path, FileInDirCallback_t FileInDirCallback);

/*
 * int TokenInStringCallback(
 *  const char *string,
 *  const char *delimiters,
 *  const char *token
 *  )
 */
typedef int (*TokenInStringCallback_t)(const char*, const char*, const char*);
int ForEachTokenInString(const char *str, const char *delimiters, TokenInStringCallback_t TokenInStringCallback);

#endif

