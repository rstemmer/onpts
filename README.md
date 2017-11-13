# onpts

This tool writes into the input buffer of another pseudo terminal slave (PTS).

[Idea by Pratik Sinha (2010)](http://www.humbug.in/2010/utility-to-send-commands-or-data-to-other-terminals-ttypts/)

## Example

This section shows the state of two terminals __after__ `onpts` was executed.
The user acts on the left terminal.
User action takes place on PTS2 only if explicitly mentioned.
The right terminal is the one that was remote controlled using running `onpts` on the left.

### Execute a Command on a different PTS

The user on PTS1 executes a command on PTS2.
The first argument to `onpts` is the number of the pseudo terminal slave (PTS).
Any arguments after the PTS number will be send to the input buffer of that terminal.
In this example "whoami" followed by a line break (Enter-Key) will be send.
The shell on PTS2 gets the pseudo key strokes and executes the command `whoami`.

```
[pts01] onpts 2 whoami   │ [pts02] whoami
[pts01]                  │ user
[pts01]                  │ [pts02]
```

### Exit a forgotten vim session

A more common real problem is the forgotten vim session.
The user on PTS1 closes a vim session that blocks a file.

If the foreground process on a terminal is not the shell, than the simulated key strokes were read by
the process running in the foreground.
In this example it is the `vim` text editor.
To close this editor you have to enter the necessary commands.
This can be done as simple as executing commands on PTS2 - just send the necessary key strokes to close the session.

```
[pts01] onpts 2 $'\e:wqa'        │ [pts02] vim notes.txt
[pts01]                          │ [pts02]
```

The `$''` construct makes the shell to replace the "\e" by the escape character.

### Both shells in the same directory

A further real problem is, that you work on two (or more) PTS and you want to change your
current work directory.
(This was the main problem [Pratik Sinha](http://www.humbug.in/2010/utility-to-send-commands-or-data-to-other-terminals-ttypts/) wanted to solve.)
Just navigate in one shell to the new directory and make the other shells follow you.

Here the `pwd` commands output, the current working directory, gets piped to `onpts`.
onpts writes "cd " to PTS2 but without a line break because of the "-n" argument.
After the "cd " string, the characters that were piped to the standard input of `onpts` follow.
The shell on PTS2 gets the input as if you enter the `cd` command line to the directory you want.

```
[pts01] pwd                      │ [pts02] pwd    # entered by a user
/home/user                       │ /tmp
[pts01] pwd | onpts -n 2 "cd "   │ [pts02] cd /home/user
[pts01] onpts 2 pwd              │ [pts02] pwd
[pts01]                          │ /home/user
```

The "-n" makes `onpts` to not append a line break after "cd ".
This is important so that the path piped from `pwd` gets appended to the `cd` command.

### Try something evil

The user on PTS1 tries to run a command on PTS2.
This is not allowed because the shell running on PTS2 has
other permissions than the user on PTS1.

The shell does not care about permissions if the input comes from the user.
If a user enters a root shell, than the user is root.
So if `onpts` simply writes characters from a user shell into the input buffer of a PTS
that runs a root shell, the user can execute commands with root permissions without entering a password.
To prevent this, `onpts` does a check if every process hast exact the same permissions on the other PTS has,
the `onpts` has.
If this is not the case, `onpts` quits with an error message.

```
[pts01] whoami                   │ [pts02] whoami    # entered by a user
user                             │ root
[pts01] onpts 2 shutdown -h now  │ [pts02]
Permission denied …              │ [pts02]
```

### Lets try to hack

The user on PTS1 tries to run a command on PTS2.
The shell itself being active on PTS2 has the same permission the
shell on PTS1 has.
Because a simple `exit` command would give access to a tool with other permissions,
_onpts_ does not allow accessing PTS2.

Here the foreground process on PTS2 has the same permissions as `onpts` on PTS1.
But with a simple exit, the user on PTS1 would be able to enter commands that get executed with root permissions.
(Imagine the parameter "$'exit\nshutdown -h'" - exit user shell, and run shutdown in the root shell).
As mentioned in the section above, `onpts` checks _ALL_ processes running in context of the other PTS.
And _every_ process/task/child has to match the permissions `onpts` has.

```
[pts01] whoami           │ [pts02] whoami     # entered by a user
user                     │ root
[pts01] onpts 2 exit     │ [pts02] su user    # entered by a user
Permission denied …      │ [pts02] whoami     # entered by a user
[pts01]                  │ user
                         │ [pts02]
```

### Lets get insane

Just a very complicated way to create a file with "Hello World!" in it.

The following line opens vim, writes a string, saves the file and exit vim.
All this happens on PTS2 while the user is on PTS1.
I don't show PTS2 here because its output is not relevant.

```bash
echo "Hello World!" | onpts -n 2 $'vim test.txt\ni' && onpts 2 $'\e:wq'
```

The string "Hello World! gets piped to the first `onpts` call.
The first time `onpts` gets executed, it detects that there is something waiting on _stdin_.
onpts sends the data "vim test.txt\ni" where "\n" is a line break.
After the last character, the "i" onpts does not append a further line break because the option `-n` is set.
Next, onpts writes the data from _stdin_ to PTS2 what ends up in the vim editor that is in insert-mode (because of the trailing "i").
If everything went well, the second `onpts` call closes the vim session by leaving the insert mode and sending the write and quit command.

onpts does not handle escape sequences, so `onpts 2 \n` would send a "\" followed by an "n" instead of a line break.
Thats why the `$''` construct is used to make the shell do replace the "\n" by a line break character.

## Installation

You should check the `install.sh` script before executing.
The default installation path is _/usr/local/bin_

```bash
# download
git clone https://github.com/rstemmer/onpts.git
cd onpts

# build
./build.sh

# install
sudo ./install.sh
```

## Usage

onpts [-h|-n] PTSNUM COMMAND…

 * -h: Print help and version number
 * -n: Do not append a line break after the command that will be send to PTSx
 * PTSNUM: Number of the pseudo terminal the command shall be sent to
 * COMMAND…: A string that will be send to PTSx

Every argument after _PTSNUM_ will be concatenated to the one string with a single space in between.

It is possible to pipe data to _stdin_.
Those bytes get send to the PTS after the strings from the parameter list were send.

## Hints

Some hints to figure out which pseudo terminal slave number a terminal has and other usefull things
to know when using `onpts`.

### tty command

```bash
tty
# output: /dev/pts/42
```

### zsh PROMPT

To get a prompt as shown in the example
you can use the `%l` variable.
The code below shows the line you have to write into ~/.zshrc to get the following prompt:
`[pts/xx]` with _xx_ for the PTS number.

```bash
PROMPT=$'[%l] '
```

### escape and newline

Write the string in `$''` to let the shell preprocess the string and replace "\n" to a newline character.

```bash
# -E disables escape sequence handling in echo
echo -E $'This\nis\na\ntest'            # new line: \n
echo -E $'\e[1;32mThis is green!\e[0m'  # escape:   \e
```

### list pts a program like vim runs on

If there is a vim session somewhere running on your system you want to exit with `ontps $PTS $'\e:wqa'`, you need to figure out on which pseudo terminal it runs.
This can be done using `ps`.
The exampe shows how to call `ps` showing all `vim` sessions, the pseudo terminal it runs on, and the username that executed `vim`.

```bash
ps -C vim -o tty,user,fname
```

