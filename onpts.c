/*
 * onpts is a too to securely access the input buffer of other pseudo terminals
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

// Idea by Pratik Sinha (2010)
// http://www.humbug.in/2010/utility-to-send-commands-or-data-to-other-terminals-ttypts/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>
#include "sec.h"

#define VERSION "1.0.1"
/*
 * CHANGELOG
 *
 * 1.0.1
 *  - Stops appeding an unwanted trailing space to the string that gets send to the remote PTS
 */

int GetPTSPath(char *ptsnum, const char **ptspath);
int CheckPTS(const char *ptspath);
int CheckPermissions(const char *ptspath);
int OpenPTS(const char *ptspath, int *ptshandler);
int SendCommand(int ptshandler, const char *command);
int SendStdin(int ptshandler);
int SendChar(int ptshandler, char byte);

#define MAX_PTS_PATH_LENGTH (sizeof("/dev/pts/XXXX")+1)

void PrintHelp(char *pname)
{
    fprintf(stderr, "onpts  Copyright (C) 2017  Ralf Stemmer <ralf.stemmer@gmx.net>\n");
    fprintf(stderr, "This program comes with \e[0;33mABSOLUTELY NO WARRANTY\e[0m.\n");
    fprintf(stderr, "This is free software, and you are welcome to redistribute it\n");
    fprintf(stderr, "under certain conditions.\n\n");
    fprintf(stderr, "\e[1;31monpts [\e[1;34m%s\e[1;31m]\e[0m\n", VERSION);
    fprintf(stderr, "\e[1;37mUsage: \e[1;36m%s\e[1;34m [-h|-n] PTS COMMAND\e[0m\n", pname);
    fprintf(stderr, "\t\e[1;36m-h\t\e[1;34mPrint this Help\e[0m\n");
    fprintf(stderr, "\t\e[1;36m-n\t\e[1;34mNO line break after command (like -n for echo)\e[0m\n");
    fprintf(stderr, "If data gets piped to stdin, they get send to the other PTS after the strings on the parameter list.\n");
}



int main(int argc, char *argv[])
{
    // Handle Arguments
    int  argi   = 0;
    char *pname = argv[argi++];
    if(argc == 2)
    {
        if(strncmp(argv[1], "-h", 10) == 0 || strncmp(argv[1], "--help", 10) == 0)
        {
            PrintHelp(pname);
            exit(EXIT_SUCCESS);
        }
    }
    if(argc < 3)
    {
        fprintf(stderr, "\e[1;31mNot enough arguments!\e[0m\n");
        PrintHelp(pname);
        exit(EXIT_FAILURE);
    }

    // Handle optional arguments
    bool opt_nolinebreak   = false;
    bool opt_readfromstdin = false;

    for(; argi < argc; argi++)
    {
        if(argv[argi][0] == '-')
        {
            if(strncmp(argv[argi], "-n", 10) == 0)
                opt_nolinebreak = true;
        }
        else
            break;
    }

    // Do I get data from stdin?
    if(!isatty(fileno(stdin)))
        opt_readfromstdin = true;

    // Handle PTS argument
    char *arg_ptsnum = argv[argi++];
    // Check if it is a valid number
    for(int i=0; arg_ptsnum[i]; i++)
    {
        if(!isdigit(arg_ptsnum[i]) || i >= 4)
        {
            fprintf(stderr, "\e[1;31mPTYNUM must be a decimal number between 0 and 9999!\e[0m\n");
            exit(EXIT_FAILURE);
        }
    }

    // Handle Command
    char  *arg_command   = NULL;
    size_t commandlength = 1; // offset of one for "\0"

    // concatinate cmd and its args
    for(; argi < argc; argi++)
    {
        commandlength += strlen(argv[argi]) + 1; // + " "
        arg_command    = (char*)realloc((void*)arg_command, commandlength);
        if(arg_command == NULL)
        {
            fprintf(stderr, "\e[1;31mAllocating memory for command argument failed with error: ");
            fprintf(stderr, "%s\e[0m\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        strcat(arg_command, argv[argi]);
        strcat(arg_command, " ");
    }

    if(!opt_nolinebreak)
        arg_command[strlen(arg_command) - 1] = '\n';    // relace last " " with a "\n"
    else
        arg_command[strlen(arg_command) - 1] = '\0';    // remove last " "

#ifdef DEBUG
    printf("\e[1;34mpname:     \e[0;36m%s\n", pname);
    printf("\e[1;34mptynum:    \e[0;36m%s\n", arg_ptsnum);
    printf("\e[1;34mthispty:   \e[0;36m%s\n", ttyname(0));
    printf("\e[1;34mcommand:   \e[0;36m%s\n", arg_command);
#endif

    // Get the path to the pseudo terminal
    const char *ptspath;
    if(GetPTSPath(arg_ptsnum, &ptspath))
        exit(EXIT_FAILURE);

    // Check if the PTS is valid, or the same pts onpts was executed on (this is forbidden)
    if(CheckPTS(ptspath))
        exit(EXIT_FAILURE);

    // Check security
    if(CheckPermissions(ptspath))
        exit(EXIT_FAILURE);

    // Open PTY
    int ptshandler;
    if(OpenPTS(ptspath, &ptshandler))
        exit(EXIT_FAILURE);
    free((void*)ptspath);
    ptspath = NULL;

    // send Command
    int retval;
    retval = SendCommand(ptshandler, arg_command);
    free(arg_command);
    arg_command = NULL;

    // if command was successfull and there is data waiting on stdin, process it
    if(opt_readfromstdin && retval == 0)
        retval = SendStdin(ptshandler);

    // clean up
    close(ptshandler);
    if(retval)
        exit(EXIT_FAILURE);

    return EXIT_SUCCESS;
}



int GetPTSPath(char *ptsnum, const char **ptspath)
{
#ifdef DEBUG
    printf("\e[1;34mGenerating path to PTS \e[0;36m%s\e[0m\n", ptsnum);
#endif
    if(ptsnum == NULL || ptspath == NULL)
        return -1;

    char *path;
    path = (char*) malloc(MAX_PTS_PATH_LENGTH);
    if(path == NULL)
    {
        fprintf(stderr, "\e[1;31mAllocating memory for PTS-path failed with error: ");
        fprintf(stderr, "%s\e[0m\n", strerror(errno));
        return -1;
    }

    strcpy(path, "/dev/pts/");
    strncat(path, ptsnum, 4);
    *ptspath = path;
    return 0;
}



int CheckPTS(const char *ptspath)
{
    if(ptspath == NULL)
        return -1;

    // Check if "the other" PTS ist the same onpts was executed on
    // FIXME THIS IS A HACK - If stdin/out/err are pipes or files, this breaks
    for(int i=0; i<3; i++) // for stdin, stdout, stderr:
    {
        if(!isatty(i))
            continue;

        if(strncmp(ptspath, ttyname(i), MAX_PTS_PATH_LENGTH) != 0)
            return 0;
    }
    fprintf(stderr, "\e[1;31monpts runs on the same terminal it shall access!\e[0m\n");
    return -1;
}



int CheckPermissions(const char *ptspath)
{
    if(ptspath == NULL)
        return -1;

    uid_t uid = getuid();
    gid_t gid = getgid();
#ifdef DEBUG
    printf("\e[1;34mChecking \e[0;36m%s\e[0m\n", ptspath);
    printf("\e[1;34m uid: \e[0;36m%d\e[0m\n", uid);
    printf("\e[1;34m gid: \e[0;36m%d\e[0m\n", gid);
#endif

    // root can do whatever root wants to do
    if(uid == 0)
        return 0;

    if(CheckPrivileges(ptspath, uid, gid) != 0)
        return -1;

    return 0;
}



int OpenPTS(const char *ptspath, int *ptshandler)
{
#ifdef DEBUG
    printf("\e[1;34mOpening \e[0;36m%s\e[0m\n", ptspath);
#endif
    if(ptspath == NULL || ptshandler == NULL)
        return -1;

    int pts_fd;
    pts_fd = open(ptspath, O_RDWR);
    if(pts_fd == -1)
    {
        fprintf(stderr, "\e[1;31mOpening %s failed with error: ", ptspath);
        fprintf(stderr, "%s\e[0m\n", strerror(errno));
        return -1;
    }

    *ptshandler = pts_fd;
    return 0;
}



int SendCommand(int ptshandler, const char *command)
{
#ifdef DEBUG
    printf("\e[1;34mSending \e[0;36m%s\e[0m\n", command);
#endif
    if(command == NULL)
        return -1;

    while(*command)
    {
        if(SendChar(ptshandler, *command))
            return -1;
        command++;
    }
    return 0;
} 



int SendStdin(int ptshandler)
{
    int chr;
    while((chr = getchar()) != EOF)
    {
        if(SendChar(ptshandler, chr))
            return -1;
    }
    return 0;
}



int SendChar(int ptshandler, char byte)
{
    int retval;
    retval = ioctl(ptshandler, TIOCSTI, &byte);
    if(retval == -1)
    {
        fprintf(stderr, "\e[1;31mioctl failed with error: ");
        fprintf(stderr, "%s\e[0m\n", strerror(errno));
        return -1;
    }
    return 0;
}

// vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4

