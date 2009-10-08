/*
    Copyright (c) 2009 Andrew Caudwell (acaudwell@gmail.com)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. The name of the author may not be used to endorse or promote products
       derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

//#define SDLAPP_DEBUG_LOG "debug.log"
#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

#include "logger.h"

bool firstEntry = true;

void debugLog(const char *str, ...) {
#ifdef SDLAPP_DEBUG_LOG

    FILE *log;

    if (firstEntry) {

        log = fopen(SDLAPP_DEBUG_LOG, "w");

        if(log==0) {
            printf("could not create %s\n", SDLAPP_DEBUG_LOG);
            exit(1);
        }

        firstEntry=false;
    } else {

        log = fopen(SDLAPP_DEBUG_LOG, "a");

        if(log==0) {
            printf("could not append to %s\n", SDLAPP_DEBUG_LOG);
            exit(1);
        }
    }

    va_list vl;

    va_start(vl, str);
        vfprintf(log, str, vl);
    va_end(vl);

    fclose(log);

#endif

#ifdef _MSC_VER
    static const size_t BUFFER_SIZE = 128;
    va_list ap;	
    char msg_buffer[BUFFER_SIZE];	
    va_start(ap, str);
    vsnprintf( msg_buffer, BUFFER_SIZE - 1, str, ap);
    va_end(ap);
    msg_buffer[BUFFER_SIZE-1] = 0;
    OutputDebugStringA(msg_buffer);
#endif
}
