/*++

Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:

    savedata.h

Abstract:

    Declaration of MSVAD data saving class. This class supplies services
to save data to disk.


--*/

#ifndef _MSVAD_SAVEDATA_H
#define _MSVAD_SAVEDATA_H

//-----------------------------------------------------------------------------
//  Forward declaration
//-----------------------------------------------------------------------------
class  CSaveData;
using PCSaveData = CSaveData*;

//-----------------------------------------------------------------------------
//  Structs
//-----------------------------------------------------------------------------

// Parameter to workitem.
#include <pshpack1.h>
typedef struct _SAVEWORKER_PARAM {
    PIO_WORKITEM     WorkItem;
    ULONG            ulFrameNo;
    ULONG            ulDataSize;
    PBYTE            pData;
    PCSaveData       pSaveData;
    KEVENT           EventDone;
} SAVEWORKER_PARAM;

using PSAVEWORKER_PARAM = SAVEWORKER_PARAM*;

#include <poppack.h>

// wave file header.
#include <pshpack1.h>

typedef struct _OUTPUT_FILE_HEADER
{
    DWORD           dwRiff;
    DWORD           dwFileSize;
    DWORD           dwWave;
    DWORD           dwFormat;
    DWORD           dwFormatLength;
} OUTPUT_FILE_HEADER;

using POUTPUT_FILE_HEADER = OUTPUT_FILE_HEADER*;

typedef struct _OUTPUT_DATA_HEADER
{
    DWORD           dwData;
    DWORD           dwDataLength;
} OUTPUT_DATA_HEADER;

using POUTPUT_DATA_HEADER = OUTPUT_DATA_HEADER*;

#include <poppack.h>

//-----------------------------------------------------------------------------
//  Classes
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
// CSaveData
//   Saves the wave data to disk.
//
IO_WORKITEM_ROUTINE saveFrameWorkerCallback;

class CSaveData
{
protected:
    UNICODE_STRING              fileName_;              // DataFile name.
    HANDLE                      fileHandle_;            // DataFile handle.
    PBYTE                       dataBuffer_;            // Data buffer.
    ULONG                       bufferSize_;            // Total buffer size.

    ULONG                       framePtr_;              // Current Frame.
    ULONG                       frameCount_;            // Frame count.
    ULONG                       frameSize_;
    ULONG                       bufferPtr_;             // Pointer in buffer.
    PBOOL                       frameUsed_;             // Frame usage table.
    KSPIN_LOCK                  frameInUseSpinLock_;    // Spinlock for sync
    KMUTEX                      fileSync_;              // Synchronizes file access

    OBJECT_ATTRIBUTES           objectAttributes_;      // Used for opening file.

    OUTPUT_FILE_HEADER          fileHeader_;
    PWAVEFORMATEX               waveFormat_;
    OUTPUT_DATA_HEADER          dataHeader_;
    PLARGE_INTEGER              filePtr_;

    static PDEVICE_OBJECT       deviceObject_;
    static ULONG                streamId_;
    static PSAVEWORKER_PARAM    workItems_;

    BOOL                        writeDisabled_;

    BOOL                        initialized_;

public:
    CSaveData();
    ~CSaveData();

    static void                 destroyWorkItems();
    void                        disable(BOOL fDisable);
    static PSAVEWORKER_PARAM    getNewWorkItem();
    NTSTATUS                    initialize();
    static NTSTATUS             setDeviceObject(IN  PDEVICE_OBJECT DeviceObject);
    static PDEVICE_OBJECT       getDeviceObject();

    void                        readData(_Inout_updates_bytes_all_(ulByteCount)  PBYTE pBuffer,
                                         _In_                                    ULONG ulByteCount);

    NTSTATUS                    setDataFormat(IN  PKSDATAFORMAT       pDataFormat);
    void                        waitAllWorkItems();
    void                        writeData(_In_reads_bytes_(ulByteCount)   PBYTE   pBuffer,
                                          _In_                            ULONG   ulByteCount);
private:
    static NTSTATUS             initializeWorkItems(IN  PDEVICE_OBJECT DeviceObject);

    NTSTATUS                    fileClose(void);
    NTSTATUS                    fileOpen(IN  BOOL fOverWrite);
    NTSTATUS                    fileWrite(_In_reads_bytes_(ulDataSize) PBYTE   pData,
                                          _In_                         ULONG   ulDataSize);

    NTSTATUS                    fileWriteHeader();
    void                        saveFrame(IN  ULONG ulFrameNo, IN  ULONG ulDataSize);
    friend VOID                 saveFrameWorkerCallback(PDEVICE_OBJECT pDeviceObject, IN  PVOID  Context);
};

using PCSaveData = CSaveData*;

#endif
