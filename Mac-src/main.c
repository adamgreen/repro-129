/*  Copyright (C) 2023  Adam Green (https://github.com/adamgreen)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
// This program sends a fake GDB 'G', set registers, command to an
// Arduino over the serial port and reports the number of bytes that
// actually made it over the serial connection as expected.
#include <ctype.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>


typedef struct SerialIComm
{
    struct termios origTerm;
    int            file;
    int            origTermValid;
    uint32_t       secTimeout;
    uint32_t       usecTimeout;
} SerialIComm;


static int SerialIComm_Init(SerialIComm* pThis, const char* pDevicePath, uint32_t baudRate, uint32_t msecTimeout);
static int openAndConfigureSerialPort(SerialIComm* pThis, const char* pDevicePath, uint32_t baudRate, uint32_t msecTimeout);
static void SerialIComm_Uninit(SerialIComm* pComm);
static int receiveChar(SerialIComm* pComm);
static int receiveBytes(SerialIComm* pComm, void* pvBuffer, size_t bufferSize);
static int sendChar(SerialIComm* pComm, int character);
static int sendBytes(SerialIComm* pComm, const void* pvBuffer, size_t bufferSize);


static void displayUsage(void)
{
    fprintf(stderr, "Usage: repro-129 deviceName\n"
                    "Where: deviceName is serial connection to use for\n"
                    "                  communicating with Nano 33 BLE device.\n"
                    "                  Examples: /dev/tty.usbmodem11401\n");
}


int main(int argc, const char** argv)
{
    int         returnValue = 0;
    const char* pDeviceName = NULL;
    SerialIComm serial;

    if (argc < 2)
    {
        fprintf(stderr, "error: deviceName parameter must be specified.\n");
        displayUsage();
        return -1;
    }
    pDeviceName = argv[1];

    printf("Opening connection to %s...\n", pDeviceName);
    int result = SerialIComm_Init(&serial, pDeviceName, 230400, 2000);
    if (!result)
    {
        return -1;
    }

    size_t i;
    for (i = 0 ; i < 1000 ; i++)
    {
        printf("Sending...\n");
        const uint8_t testData[] = "$G000000000000000001000000adc10020b4b90020d032002000000000010000001300800000000000000000000000000000000000a0b9002091ae010020e6010000000f61000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000#f8";
        result = sendBytes(&serial, testData, sizeof(testData)-1);
        if (!result)
        {
            fprintf(stderr, "error: Failed to send bytes to serial port.\n");
            goto Error;
        }

        printf("Receiving...\n");
        uint32_t count = 0;
        result = receiveBytes(&serial, &count, sizeof(count));
        if (!result)
        {
            fprintf(stderr, "error: Failed while reading bytes from serial port.\n");
            goto Error;
        }
        printf("Count - actual: %u  expected: %lu)\n", count, sizeof(testData)-1);
    }
    returnValue = 0;

Error:
    SerialIComm_Uninit(&serial);

    return returnValue;
}

static int SerialIComm_Init(SerialIComm* pThis, const char* pDevicePath, uint32_t baudRate, uint32_t msecTimeout)
{
    memset(pThis, 0, sizeof(*pThis));
    pThis->file = -1;
    int result = openAndConfigureSerialPort(pThis, pDevicePath, baudRate, msecTimeout);
    if (!result)
    {
        SerialIComm_Uninit(pThis);
        return 0;
    }

    return 1;
}

static int openAndConfigureSerialPort(SerialIComm* pThis, const char* pDevicePath, uint32_t baudRate, uint32_t msecTimeout)
{
    int            result = -1;
    struct termios newTerm;

    pThis->file = open(pDevicePath, O_RDWR | O_NONBLOCK);
    if (pThis->file == -1)
    {
        perror("error: Failed to open serial port");
        return 0;
    }

    /* Remember the desired timeout for reads/writes */
    pThis->secTimeout = msecTimeout / 1000;
    msecTimeout = msecTimeout % 1000;
    pThis->usecTimeout = msecTimeout * 1000;

    /* Configure serial port settings. */
    result = tcgetattr(pThis->file, &pThis->origTerm);
    if (result == -1)
        return 0;
    pThis->origTermValid = 1;
    newTerm = pThis->origTerm;

    /* Set the baud rate. */
    result = cfsetspeed(&newTerm, baudRate);
    if (result == -1)
    {
        perror("error: Failed to set baud rate");
        return 0;
    }

    /* Use Non-canonical mode. */
    cfmakeraw(&newTerm);

    /* No input or output mapping. */
    newTerm.c_iflag = 0;
    newTerm.c_oflag = 0;


    /* Configure for 8n1 format and disable modem flow control. */
    newTerm.c_cflag = CS8 | CREAD | CLOCAL;

    /* Set MIN characters and TIMEout for non-canonical mode. */
    newTerm.c_cc[VMIN] = 0;
    newTerm.c_cc[VTIME] = 0;

    result = tcflush(pThis->file, TCIOFLUSH);
    if (result != 0)
        return 0;

    result = tcsetattr(pThis->file, TCSANOW, &newTerm);
    if (result == -1)
    {
        perror("error: Failed to configure serial port");
        return 0;
    }

    /* This might only be required due to the mbed CDC firmware. */
    usleep(100000);
    return 1;
}

static void SerialIComm_Uninit(SerialIComm* pComm)
{
    SerialIComm* pThis = (SerialIComm*)pComm;

    if (!pThis)
        return;

    if (pThis->origTermValid)
        tcsetattr(pThis->file, TCSAFLUSH, &pThis->origTerm);
    if (pThis->file != -1)
    {
        close(pThis->file);
        pThis->file = -1;
    }
}

#ifdef UNDONE
static int hasReceiveData(SerialIComm* pComm)
{
    SerialIComm*   pThis = (SerialIComm*)pComm;
    int            selectResult = -1;
    struct timeval timeOut = {0, 0};
    fd_set         readfds;
    fd_set         errorfds;

    FD_ZERO(&readfds);
    FD_ZERO(&errorfds);
    FD_SET(pThis->file, &readfds);

    selectResult = select(pThis->file+1, &readfds, NULL, &errorfds, &timeOut);
    if (selectResult == -1)
        __throws(serialException);
    if (selectResult == 0)
        return 0;
    else
        return 1;
}
#endif // UNDONE

static int receiveChar(SerialIComm* pComm)
{
    SerialIComm*   pThis = (SerialIComm*)pComm;
    ssize_t        bytesRead = 0;
    uint8_t        byte = 0;
    int            selectResult = -1;
    fd_set         readfds;
    fd_set         errorfds;
    struct timeval timeOut = {pThis->secTimeout, pThis->usecTimeout};

    FD_ZERO(&readfds);
    FD_ZERO(&errorfds);
    FD_SET(pThis->file, &readfds);

    selectResult = select(pThis->file+1, &readfds, NULL, &errorfds, &timeOut);
    if (selectResult == -1)
        return -1;
    if (selectResult == 0)
        return -1;

    bytesRead = read(pThis->file, &byte, sizeof(byte));
    if (bytesRead == -1)
        return -1;

    return (int)byte;
}

static int receiveBytes(SerialIComm* pComm, void* pvBuffer, size_t bufferSize)
{
    uint8_t* pBuffer = (uint8_t*)pvBuffer;
    while (bufferSize-- > 0)
    {
        int b = receiveChar(pComm);
        if (b < 0)
        {
            return 0;
        }
        *pBuffer++ = b;
    }
    return 1;
}

static int sendChar(SerialIComm* pComm, int character)
{
    SerialIComm*   pThis = (SerialIComm*)pComm;
    uint8_t        byte = (uint8_t)character;
    ssize_t        bytesWritten = 0;
    int            selectResult = -1;
    fd_set         writefds;
    fd_set         errorfds;
    struct timeval timeOut = {pThis->secTimeout, pThis->usecTimeout};

    FD_ZERO(&writefds);
    FD_ZERO(&errorfds);
    FD_SET(pThis->file, &writefds);

    selectResult = select(pThis->file+1, NULL, &writefds, &errorfds, &timeOut);
    if (selectResult == -1)
        return 0;
    if (selectResult == 0)
        return 0;

    bytesWritten = write(pThis->file, &byte, sizeof(byte));
    if (bytesWritten != sizeof(byte))
        return 0;

    return 1;
}

static int sendBytes(SerialIComm* pComm, const void* pvBuffer, size_t bufferSize)
{
    const uint8_t* pBuffer = (const uint8_t*)pvBuffer;
    while (bufferSize-- > 0)
    {
        int result = sendChar(pComm, *pBuffer++);
        if (!result)
        {
            return 0;
        }
    }

    return 1;
}
