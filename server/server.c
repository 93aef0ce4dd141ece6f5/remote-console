/*
 * 
 * Author: 93aef0ce4dd141ece6f5
 * Description: Sends data to remote host for command line
 *              execution and then receives output.
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

#define DTOR __attribute__((destructor))

#define BACKLOG_SIZE 10

#define MINUTE 60*1000

enum {
    WINDOWS,
    POSIX
};

SOCKET s;
SOCKET client_s;
HANDLE hThreadBeat;
HANDLE hThreadRecv;

DTOR void dStructor (void) {
    TerminateThread (hThreadBeat, 0);
    TerminateThread (hThreadRecv, 0);
    shutdown (s, SD_BOTH);
    closesocket (s);
    shutdown (client_s, SD_BOTH);
    closesocket (client_s);
    WSACleanup();
}

void nonFatal (char *str, int type) {
    #ifdef DEBUG
    if (type == WINDOWS) {
        int err = 0;
        if ((err = (int)GetLastError())) {
            fprintf (stderr, "[!] error: %s: %d\n", str, err);
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

void servInit (char *port) {
    int gai;
    WSADATA wsa;
    struct addrinfo hints, *res;
    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;

    puts ("[*] Initialising sockets...");
    if (WSAStartup (MAKEWORD (2, 2), &wsa) != 0) {
        fatal ("WSA Startup", WINDOWS);
    }
    
    puts ("[*] Getting information...");
    if ((gai = getaddrinfo (NULL, port, &hints, &res)) != 0) {
        fprintf (stderr, "[!] Get Info Error: %s\n", gai_strerror (gai));
        exit (EXIT_FAILURE);
    }
    
    if ((s = socket (res->ai_family, res->ai_socktype, res->ai_protocol)) == INVALID_SOCKET) {
        fatal ("Socket", WINDOWS);
    }
    
    puts ("[*] Attempting to bind...");
    if (bind (s, res->ai_addr, res->ai_addrlen) != 0) {
        fatal ("Bind", WINDOWS);
    }
}

void receiver (void) {
    char recvBuf[MAX_MSG_SIZE];
    int recvStat;

    puts ("[*] Receiver started");
    while (TRUE) {
        memset (recvBuf, 0, sizeof (recvBuf));

        recvStat = recv (client_s, recvBuf, sizeof (recvBuf)-1, 0);
        if (recvStat == SOCKET_ERROR) {
            int err = WSAGetLastError();
            //        aborted connexion             timed out             connexion reset
            // restart connexion
            if (err == WSAECONNABORTED || err == WSAETIMEDOUT || err == WSAECONNRESET) {
                printf ("\n[*] Disconnected\n");
                exit (EXIT_SUCCESS);
            } else {
                fatal ("Receive", WINDOWS);
            }

        }
        printf ("%s", recvBuf);
    }
}

void beat (void) {
    while (TRUE) {
        Sleep (5*MINUTE);
        if (send (client_s, "PING", 5, 0) == SOCKET_ERROR) {
            nonFatal ("Ping", WINDOWS);
        }
    }
}

void startServer (void) {
    struct sockaddr_storage client_addr;
    memset (&client_addr, 0, sizeof (client_addr));
    int client_size = sizeof (client_addr);

    // listen
    puts ("[*] Listening for client...");
    if (listen (s, BACKLOG_SIZE) != 0) {
        fatal ("Listen", WINDOWS);
    }

    // accept
    if ((client_s = accept (s, (struct sockaddr *)&client_addr, &client_size)) == INVALID_SOCKET) {
        fatal ("Accept", WINDOWS);
    }

    /*
    MessageBox (NULL, "Client connected!", "Connected!", 
        MB_OK|MB_ICONINFORMATION|MB_DEFBUTTON1|MB_SETFOREGROUND);
    */
    puts ("[*] Client connected!");

    char sendBuf[MAX_MSG_SIZE];
    
    // receive from client
    hThreadRecv = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)receiver, NULL, 0, NULL);
    if (!hThreadRecv) {
        fatal ("Receive Thread", WINDOWS);
    }

    // heartbeat
    hThreadBeat = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)beat, NULL, 0, NULL);
    if (!hThreadBeat) {
        fatal ("Heartbeat Thread", WINDOWS);
    }

    // send commands
    puts ("[*] Ready");
    while (TRUE) {
        memset (sendBuf, 0, sizeof (sendBuf));

        while (strlen (sendBuf) <= 0) {
            if (fgets (sendBuf, MAX_MSG_SIZE, stdin) == NULL) {
                fatal ("[!] Get message", POSIX);
            }
        }
        /*
        int len;
        for (len = 0; len < strlen (sendBuf); len++) {
            if (sendBuf[len] == '\r' || sendBuf[len] == '\n') {
                break;
            }
        }
        */
        if (send (client_s, sendBuf, strlen (sendBuf), 0) == SOCKET_ERROR) {
            fatal ("Send", WINDOWS);
        }
    }
}

int main (int argc, char *argv[]) {
    char title[14] = "Server - ";

    if (argc < 2) {
        fprintf (stderr, "Syntax: %s [PORT]\n", argv[0]);
        exit (EXIT_FAILURE);
    }

    strncat (title, argv[1], 4);

    //SetWindowText (NULL, title);

    servInit (argv[1]);
    startServer();

    return EXIT_SUCCESS;

}
