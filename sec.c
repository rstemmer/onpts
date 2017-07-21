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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>
#include <fein.h>
#include "sec.h"

static uid_t global_uid;
static gid_t global_gid;

#define RETVAL_ERROR    -1  // Never change this value, it is related to the error-behavior of libfein
#define RETVAL_OK        0
#define RETVAL_UNSECURE  RETVAL_ERROR


static int ForEachPIDCallback(const char *str, const char *delimiters, const char *parent_pid);
static int ForEachTaskCallback(const char *dirpath, struct dirent *entry);
static int ForEachPIDLineCallback(const char *filename, const char *line, size_t linelength, size_t linenumber);
static int CheckStatus(char *statusfilepath);
static int ForEachStatusLineCallback(const char *filename, const char *line, size_t linelength, size_t linenumber);


/* 
 * Structure of the recursive construct of callback functions.
 *
 *       ┌───────────────────────────┐
 *       │                           │
 *       │      CheckPrivileges      │
 *       │                           │
 *       └─────────────┬─────────────┘
 *                     │
 *                     │ For each PID given by
 *                     │ "fuser $PTY"
 *                     ▼
 *       ┌───────────────────────────┐   ┌───────────────────────────┐
 *       │                           │   │                           │
 *   ┌──▶│    ForEachPIDCallback     ├──▶│        CheckStatus        │
 *   │   │                           │   │                           │
 *   │   └─────────────┬─────────────┘   └──────────────┬────────────┘
 *   │                 │                                │
 *   │                 │ For each TID in                │ For each line in
 *   │                 │ /proc/$PID/task                │ /proc/$PID/status
 *   │                 ▼                                ▼
 *   │   ┌───────────────────────────┐   ┌───────────────────────────┐
 *   │   │                           │   │                           │
 *   │   │    ForEachTaskCallback    │   │ ForEachStatusLineCallback │
 *   │   │                           │   │                           │
 *   │   └─────────────┬─────────────┘   └───────────────────────────┘
 *   │                 │
 *   │                 │ For each child PID in
 *   │                 │ /proc/$PID/task/$TID/children
 *   │                 ▼
 *   │   ┌───────────────────────────┐
 *   │   │                           │
 *   └───┤  ForEachPIDLineCallback   │
 *       │                           │
 *       └───────────────────────────┘
 */


/*
 * This function checks for all processes running on the given terminal that may be accessed
 * by onpts, if they have different permissions than onpts.
 *
 * Args:
 *  pts_path:   path to the pseudo terminal slave /dev/pts/X
 *  uid:        The UID of the onpts caller
 *  gid:        The GID of the onpts caller
 *
 * Returns:
 *  RETVAL_OK:  On the PTS addressed by pty_path, there is NO process/task/child 
 *              running with other permissions than the onpts has.
 *  otherwise:  It is not secure to send input data to the other terminal! 
 *              There may be a process running as root.
 */
int CheckPrivileges(const char* pts_path, uid_t uid, gid_t gid)
{
    global_uid = uid;
    global_gid = gid;

    // Get PIDs that access PTY
    FILE *pp;
    char command[128];
    strncpy(command, "fuser ",        7);
    strncat(command, pts_path,      100);
    strncat(command, " 2>/dev/null", 20);

#ifdef DEBUG
    printf("\e[1;34m\tExecuting \e[0;36m%s\e[0m\n", command);
#endif
    pp = popen(command, "r");
    if(!pp)
    {
        fprintf(stderr, "\e[1;31mpopen(\"%s\", \"r\") failed with error: ", command);
        fprintf(stderr, "%s\e[0m\n", strerror(errno));
        return RETVAL_ERROR;
    }

#ifdef DEBUG
    printf("\e[1;34m\tReading PIDs: ");
#endif
    char pidlist[1024];
    fgets(pidlist, 1024, pp);
    pclose(pp);
#ifdef DEBUG
    printf("\e[0;36m%s\e[0m\n", pidlist);
#endif

    int retval;
    retval = ForEachTokenInString(pidlist, " ", ForEachPIDCallback);
    return retval;
}



/*
 * This function checks the permissions of a process with the given PID.
 * If the processes permission are OK, its tasks and their child process
 * permissions will be checked.
 *
 * Args:
 *  str:        (not used) - DO NOT USE THIS ARGUMENT
 *  delimiters: (not used) - DO NOT USE THIS ARGUMENT
 *  parent_pid: the PID thats permission and child permissions shall be checked
 *
 * Returns:
 *  The security status of the process and its child processes
 */
int ForEachPIDCallback(const char *str, const char *delimiters, const char *parent_pid)
{
    int retval;

    // Check status
#ifdef DEBUG
    printf("\e[1;34m\tCheck status of pid \e[0;36m%s\e[0m\n", parent_pid);
#endif
    char *statuspath;
    asprintf(&statuspath, "/proc/%s/status", parent_pid);
    retval = CheckStatus(statuspath);
    free(statuspath);
    if(retval != RETVAL_OK)
        return retval;

    // For each task the child processes must be checked
    char *taskdirpath;
    asprintf(&taskdirpath, "/proc/%s/task", parent_pid);
    retval = ForEachFileInDir(taskdirpath, ForEachTaskCallback);
    free(taskdirpath);
    return retval;
}



/*
 * This function gets called for each task of a process.
 * And for each task the child processes (if there are any) permissions
 * will be checked later on in the ForEachPIDLineCallback function.
 * The PIDs of the child processes were read from the file
 * /proc/$PID/task/$TID/children
 *
 * Args:
 *  dirpath:    path to the task directory: /proc/$PID/task
 *  entry:      One entry inside the task directory except "." and ".."
 *
 * Returns:
 *  The security status of the tasks child processes
 */
int ForEachTaskCallback(const char *dirpath, struct dirent *entry)
{
    // check children
    char *childlistpath;
    int retval;
    asprintf(&childlistpath, "%s/%s/children", dirpath, entry->d_name);
    retval = ForEachLineInFile(childlistpath, ForEachPIDLineCallback);
    free(childlistpath);
    return retval;
}



/*
 * This is a wrapper function to ForEachPIDCallback.
 * This callback gets called for each PID in the /proc/PARENTPID/task/PARENTTID/children
 * file.
 * The line containing a child PID gets trimmed (removing trailing space).
 * Then ForEachPIDCallback gets called with str=NULL, delimiters=NULL and parent_pid as the
 * child PID in the children file to recursively check those permissions 
 * and the permissions of those child processes
 *
 * Args:
 *  filename:   (not used) - path to the children file of a task
 *  line:       One line of the children file containing one PID of a child
 *  linelength: (not used) - length of the line in bytes
 *  linenumber: (not used) - number of the line starting by 1
 *
 * Returns:
 *  The security status of the child processes
 */
int ForEachPIDLineCallback(const char *filename, const char *line, size_t linelength, size_t linenumber)
{
    // remove trailing spaces
    char *pid;
    pid = (char*)malloc(sizeof(char)*linelength);
    if(pid == NULL)
    {
        fprintf(stderr, "\e[1;31mmalloc(%lu); failed with error: ", sizeof(char)*linelength);
        fprintf(stderr, "\e[1;31m%s\e[0m\n", strerror(errno));
        return -1;
    }

    strncpy(pid, line, linelength);

    while(pid[linelength-1] == ' ')
        pid[--linelength] = '\0';

    // check the childs permissions
    int retval;
    retval = ForEachPIDCallback(NULL, NULL, (const char*)pid);
    free(pid);
    return retval;
}



/*
 * This function reads the status file of a process: /proc/$PID/status.
 * This file contains the UID and GID of the process. Those have to match
 * the UID and GID of the user who called onpts (stored in global_{uid,gid}).
 *
 * Args:
 *  statusfilepath: Path to the status file of a process
 *
 * Returns:
 *  Result of the ForEachStatusLineCallback:
 *      RETVAL_OK:       if the UID and GID match
 *      RETVAL_UNSECURE: if the UID or GID are different of the callers one
 */
int CheckStatus(char *statusfilepath)
{
#ifdef DEBUG
    printf("\e[1;34m\t\tCheckin status in \e[0;36m%s\e[0m\n", statusfilepath);
#endif

    int retval;
    retval = ForEachLineInFile(statusfilepath, ForEachStatusLineCallback);
    return retval;
}



/*
 * This function gets applied to each line in /proc/$PID/status.
 * If a line starts with "Uid:" or "Gid:", the listed ids will be compared
 * to the IDs of the onpts callers user/group IDs.
 * If one of the IDs don't match, the function returns RETVAL_USECURE and the
 * whole security check gets stop.
 * Furthermore an error messages gets printed to stderr telling the user that
 * he has no permission to run onpts.
 *
 * Args:
 *  filename:   (not used) - path to the statusfile
 *  line:       One line of the status file
 *  linelength: (not used) - length of the line in bytes
 *  linenumber: (not used) - number of the line starting by 1
 *
 * Returns:
 */
int ForEachStatusLineCallback(const char *filename, const char *line, size_t linelength, size_t linenumber)
{
    if(strncmp(line, "Uid:", 4) == 0 || strncmp(line, "Gid:", 4) == 0)
    {
        uid_t real, eff, saved, fs;
        char tmp[128];
        int n;

        n = sscanf(line, "%127s\t%d\t%d\t%d\t%d", tmp, &real, &eff, &saved, &fs);
#ifdef DEBUG
        printf("\e[1;34m\t\t\t\"\e[1;36m%s\e[1;34m\": ", tmp);
        printf("\e[0;36m%d\e[1;34m, ", real);
        printf("\e[0;36m%d\e[1;34m, ", eff);
        printf("\e[0;36m%d\e[1;34m, ", saved);
        printf("\e[0;36m%d\e[0m\n",    fs);
#endif
        if(n != 5)
        {
            fprintf(stderr, "\e[1;31mScanning %s failed!\e[0m\n", line);
            fprintf(stderr, "\e[1;31mError: %s\e[0m\n", strerror(errno));
            return RETVAL_ERROR;
        }

        uid_t id;
        if(tmp[0] == 'U') id = global_uid;
        if(tmp[0] == 'G') id = global_gid;
        if(real != id || eff != id || saved != id || fs != id)
        {
            fprintf(stderr, "\e[1;31mPermission denied - One process on destination PTS has different privileges!\e[0m\n");
            return RETVAL_UNSECURE;
        }

    }
    return RETVAL_OK;
}

// vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4

