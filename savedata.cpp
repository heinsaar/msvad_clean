/*

Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:

    savedata.cpp

Abstract:

    Implementation of MSVAD data saving class.

    To save the playback data to disk, this class maintains a circular data
    buffer, associated frame structures and worker items to save frames to
    disk.
    Each frame structure represents a portion of buffer. When that portion
    of frame is full, a workitem is scheduled to save it to disk.
*/
#pragma warning (disable : 4127)
#pragma warning (disable : 26165)

#include <msvad.h>
#include "savedata.h"
#include <ntstrsafe.h>   // This is for using RtlStringcbPrintf

//=============================================================================
// Defines
//=============================================================================
#define RIFF_TAG                    0x46464952;
#define WAVE_TAG                    0x45564157;
#define FMT__TAG                    0x20746D66;
#define DATA_TAG                    0x61746164;

#define DEFAULT_FRAME_COUNT         2
#define DEFAULT_FRAME_SIZE          PAGE_SIZE * 4
#define DEFAULT_BUFFER_SIZE         DEFAULT_FRAME_SIZE * DEFAULT_FRAME_COUNT

#define DEFAULT_FILE_NAME           L"\\DosDevices\\C:\\STREAM"

#define MAX_WORKER_ITEM_COUNT       15

//=============================================================================
// Statics
//=============================================================================
ULONG CSaveData::m_ulStreamId = 0;

#pragma code_seg("PAGE")
//=============================================================================
// CSaveData
//=============================================================================

//=============================================================================
CSaveData::CSaveData()
:   m_pDataBuffer(nullptr),
    m_FileHandle(nullptr),
    m_ulFrameCount(DEFAULT_FRAME_COUNT),
    m_ulBufferSize(DEFAULT_BUFFER_SIZE),
    m_ulFrameSize(DEFAULT_FRAME_SIZE),
    m_ulBufferPtr(0),
    m_ulFramePtr(0),
    m_fFrameUsed(nullptr),
    m_pFilePtr(nullptr),
    m_fWriteDisabled(FALSE),
    m_bInitialized(FALSE)
{

    PAGED_CODE();

    m_waveFormat = nullptr;
    m_FileHeader.dwRiff           = RIFF_TAG;
    m_FileHeader.dwFileSize       = 0;
    m_FileHeader.dwWave           = WAVE_TAG;
    m_FileHeader.dwFormat         = FMT__TAG;
    m_FileHeader.dwFormatLength   = sizeof(WAVEFORMATEX);

    m_DataHeader.dwData           = DATA_TAG;
    m_DataHeader.dwDataLength     = 0;

    RtlZeroMemory(&m_objectAttributes, sizeof(m_objectAttributes));

    m_ulStreamId++;
    InitializeWorkItems(GetDeviceObject());
}

//=============================================================================
CSaveData::~CSaveData()
{
    PAGED_CODE();

    DPF_ENTER(("[CSaveData::~CSaveData]"));

    // Update the wave header in data file with real file size.
    //
    if(m_pFilePtr)
    {
        m_FileHeader.dwFileSize   = (DWORD) m_pFilePtr->QuadPart - 2 * sizeof(DWORD);
        m_DataHeader.dwDataLength = (DWORD) m_pFilePtr->QuadPart - sizeof(m_FileHeader) -
                                     m_FileHeader.dwFormatLength - sizeof(m_DataHeader);

        if (STATUS_SUCCESS == KeWaitForSingleObject(&m_FileSync, Executive, KernelMode, FALSE, nullptr))
        {
            if (NT_SUCCESS(FileOpen(FALSE)))
            {
                FileWriteHeader();
                FileClose();
            }

            KeReleaseMutex(&m_FileSync, FALSE);
        }
    }

   // frees the work items
   for (int i = 0; i < MAX_WORKER_ITEM_COUNT; i++)
   {
    
       if (m_pWorkItems[i].WorkItem!=nullptr)
       {
           IoFreeWorkItem(m_pWorkItems[i].WorkItem);
           m_pWorkItems[i].WorkItem = nullptr;
       }
   }

    if (m_waveFormat)
    {
        ExFreePoolWithTag(m_waveFormat, MSVAD_POOLTAG);
    }

    if (m_fFrameUsed)
    {
        ExFreePoolWithTag(m_fFrameUsed, MSVAD_POOLTAG);

        // NOTE : Do not release m_pFilePtr.
    }

    if (m_FileName.Buffer)
    {
        ExFreePoolWithTag(m_FileName.Buffer, MSVAD_POOLTAG);
    }

    if (m_pDataBuffer)
    {
        ExFreePoolWithTag(m_pDataBuffer, MSVAD_POOLTAG);
    }
}

//=============================================================================
void CSaveData::DestroyWorkItems()
{
    PAGED_CODE();

    if (m_pWorkItems)
    {
        ExFreePoolWithTag(m_pWorkItems, MSVAD_POOLTAG);
        m_pWorkItems = nullptr;
    }
}

//=============================================================================
void CSaveData::Disable(BOOL fDisable)
{
    PAGED_CODE();

    m_fWriteDisabled = fDisable;
}

//=============================================================================
NTSTATUS CSaveData::FileClose()
{
    PAGED_CODE();

    NTSTATUS ntStatus = STATUS_SUCCESS;

    if (m_FileHandle)
    {
        ntStatus = ZwClose(m_FileHandle);
        m_FileHandle = nullptr;
    }

    return ntStatus;
}

//=============================================================================
NTSTATUS CSaveData::FileOpen(IN  BOOL fOverWrite)
{
    PAGED_CODE();

    NTSTATUS        ntStatus = STATUS_SUCCESS;
    IO_STATUS_BLOCK ioStatusBlock;

    if( FALSE == m_bInitialized )
    {
        return STATUS_UNSUCCESSFUL;
    }

    if(!m_FileHandle)
    {
        ntStatus = ZwCreateFile(&m_FileHandle,
                                GENERIC_WRITE | SYNCHRONIZE,
                                &m_objectAttributes,
                                &ioStatusBlock,
                                nullptr,
                                FILE_ATTRIBUTE_NORMAL,
                                0,
                                fOverWrite ? FILE_OVERWRITE_IF : FILE_OPEN_IF,
                                FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
                                nullptr,
                                0);
        if (!NT_SUCCESS(ntStatus))
        {
            DPF(D_TERSE, ("[CSaveData::FileOpen : Error opening data file]"));
        }
    }

    return ntStatus;
}

//=============================================================================
NTSTATUS CSaveData::FileWrite
(
    _In_reads_bytes_(ulDataSize)    PBYTE   pData,
    _In_                            ULONG   ulDataSize
)
{
    PAGED_CODE();

    ASSERT(pData);
    ASSERT(m_pFilePtr);

    NTSTATUS ntStatus;

    if (m_FileHandle)
    {
        IO_STATUS_BLOCK ioStatusBlock;

        ntStatus = ZwWriteFile(m_FileHandle, nullptr, nullptr, nullptr, &ioStatusBlock, pData, ulDataSize, m_pFilePtr, nullptr);

        if (NT_SUCCESS(ntStatus))
        {
            ASSERT(ioStatusBlock.Information == ulDataSize);

            m_pFilePtr->QuadPart += ulDataSize;
        }
        else
        {
            DPF(D_TERSE, ("[CSaveData::FileWrite : WriteFileError]"));
        }
    }
    else
    {
        DPF(D_TERSE, ("[CSaveData::FileWrite : File not open]"));
        ntStatus = STATUS_INVALID_HANDLE;
    }

    return ntStatus;
}

//=============================================================================
NTSTATUS CSaveData::FileWriteHeader()
{
    PAGED_CODE();

    NTSTATUS ntStatus;

    if (m_FileHandle && m_waveFormat)
    {
        IO_STATUS_BLOCK ioStatusBlock;

        m_pFilePtr->QuadPart = 0;

        m_FileHeader.dwFormatLength = (m_waveFormat->wFormatTag == WAVE_FORMAT_PCM)
                                      ?  sizeof( PCMWAVEFORMAT )
                                      :  sizeof( WAVEFORMATEX ) + m_waveFormat->cbSize;

        ntStatus = ZwWriteFile(m_FileHandle, nullptr, nullptr, nullptr,
                               &ioStatusBlock,
                               &m_FileHeader,
                               sizeof(m_FileHeader),
                               m_pFilePtr,
                               nullptr);
        if (!NT_SUCCESS(ntStatus))
        {
            DPF(D_TERSE, ("[CSaveData::FileWriteHeader : Write File Header Error]"));
        }

        m_pFilePtr->QuadPart += sizeof(m_FileHeader);

        ntStatus = ZwWriteFile(m_FileHandle, nullptr, nullptr, nullptr,
                                &ioStatusBlock,
                                m_waveFormat,
                                m_FileHeader.dwFormatLength,
                                m_pFilePtr,
                                nullptr);
        if (!NT_SUCCESS(ntStatus))
        {
            DPF(D_TERSE, ("[CSaveData::FileWriteHeader : Write Format Error]"));
        }

        m_pFilePtr->QuadPart += m_FileHeader.dwFormatLength;

        ntStatus = ZwWriteFile(m_FileHandle, nullptr, nullptr, nullptr,
                                &ioStatusBlock,
                                &m_DataHeader,
                                sizeof(m_DataHeader),
                                m_pFilePtr,
                                nullptr);
        if (!NT_SUCCESS(ntStatus))
        {
            DPF(D_TERSE, ("[CSaveData::FileWriteHeader : Write Data Header Error]"));
        }

        m_pFilePtr->QuadPart += sizeof(m_DataHeader);
    }
    else
    {
        DPF(D_TERSE, ("[CSaveData::FileWriteHeader : File not open]"));
        ntStatus = STATUS_INVALID_HANDLE;
    }

    return ntStatus;
}
NTSTATUS CSaveData::SetDeviceObject(IN  PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    ASSERT(DeviceObject);

    NTSTATUS ntStatus = STATUS_SUCCESS;
    
    m_pDeviceObject = DeviceObject;
    return ntStatus;
}

PDEVICE_OBJECT
CSaveData::GetDeviceObject()
{
    PAGED_CODE();

    return m_pDeviceObject;
}

#pragma code_seg()
//=============================================================================
PSAVEWORKER_PARAM
CSaveData::GetNewWorkItem()
{
    LARGE_INTEGER timeOut = { 0 };

    for (int i = 0; i < MAX_WORKER_ITEM_COUNT; i++)
    {
        NTSTATUS ntStatus = KeWaitForSingleObject(&m_pWorkItems[i].EventDone, Executive, KernelMode, FALSE, &timeOut);
        if (STATUS_SUCCESS == ntStatus)
        {
            if (m_pWorkItems[i].WorkItem)
                return &(m_pWorkItems[i]);
            else
                return nullptr;
        }
    }

    return nullptr;
}
#pragma code_seg("PAGE")

//=============================================================================
NTSTATUS CSaveData::Initialize()
{
    PAGED_CODE();

    NTSTATUS    ntStatus = STATUS_SUCCESS;
    WCHAR       szTemp[MAX_PATH];
    size_t      cLen;

    DPF_ENTER(("[CSaveData::Initialize]"));

    // Allocate data file name.
    //
    RtlStringCchPrintfW(szTemp, MAX_PATH, L"%s_%d.wav", DEFAULT_FILE_NAME, m_ulStreamId);
    m_FileName.Length = 0;
    ntStatus = RtlStringCchLengthW(szTemp, sizeof(szTemp)/sizeof(szTemp[0]), &cLen);
    if (NT_SUCCESS(ntStatus))
    {
        m_FileName.MaximumLength = (USHORT)((cLen * sizeof(WCHAR)) + sizeof(WCHAR));//convert to wchar and add room for nullptr
        m_FileName.Buffer = (PWSTR)ExAllocatePoolWithTag(PagedPool, m_FileName.MaximumLength, MSVAD_POOLTAG);
        if (!m_FileName.Buffer)
        {
            DPF(D_TERSE, ("[Could not allocate memory for FileName]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    // Allocate memory for data buffer.
    //
    if (NT_SUCCESS(ntStatus))
    {
        RtlStringCbCopyW(m_FileName.Buffer, m_FileName.MaximumLength, szTemp);
        m_FileName.Length = (USHORT)wcslen(m_FileName.Buffer) * sizeof(WCHAR);
        DPF(D_BLAB, ("[New DataFile -- %S", m_FileName.Buffer));

        m_pDataBuffer = (PBYTE)ExAllocatePoolWithTag(NonPagedPool, m_ulBufferSize, MSVAD_POOLTAG);
        if (!m_pDataBuffer)
        {
            DPF(D_TERSE, ("[Could not allocate memory for Saving Data]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    // Allocate memory for frame usage flags and m_pFilePtr.
    //
    if (NT_SUCCESS(ntStatus))
    {
        m_fFrameUsed = (PBOOL)ExAllocatePoolWithTag(NonPagedPool, m_ulFrameCount * sizeof(BOOL) + sizeof(LARGE_INTEGER), MSVAD_POOLTAG);
        if (!m_fFrameUsed)
        {
            DPF(D_TERSE, ("[Could not allocate memory for frame flags]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    // Initialize the spinlock to synchronize access to the frames
    //
    KeInitializeSpinLock ( &m_FrameInUseSpinLock ) ;

    // Initialize the file mutex
    //
    KeInitializeMutex( &m_FileSync, 1 ) ;

    // Open the data file.
    //
    if (NT_SUCCESS(ntStatus))
    {
        // m_fFrameUsed has additional memory to hold m_pFilePtr
        //
        m_pFilePtr = (PLARGE_INTEGER)(((PBYTE) m_fFrameUsed) + m_ulFrameCount * sizeof(BOOL));
        RtlZeroMemory(m_fFrameUsed, m_ulFrameCount * sizeof(BOOL) + sizeof(LARGE_INTEGER));

        // Create data file.
        InitializeObjectAttributes(&m_objectAttributes, &m_FileName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr);

        m_bInitialized = TRUE;

        // Write wave header information to data file.
        ntStatus = KeWaitForSingleObject(&m_FileSync, Executive, KernelMode, FALSE, nullptr);

        if (STATUS_SUCCESS == ntStatus)
        {
            ntStatus = FileOpen(TRUE);
            if (NT_SUCCESS(ntStatus))
            {
                ntStatus = FileWriteHeader();

                FileClose();
            }

            KeReleaseMutex( &m_FileSync, FALSE );
        }
    }

    return ntStatus;
}

//=============================================================================
NTSTATUS CSaveData::InitializeWorkItems
(
    IN  PDEVICE_OBJECT DeviceObject
)
{
    PAGED_CODE();
    ASSERT(DeviceObject);
    NTSTATUS ntStatus = STATUS_SUCCESS;

    DPF_ENTER(("[CSaveData::InitializeWorkItems]"));

    if (m_pWorkItems)
    {
        return ntStatus;
    }

    m_pWorkItems = (PSAVEWORKER_PARAM)ExAllocatePoolWithTag(NonPagedPool, sizeof(SAVEWORKER_PARAM) * MAX_WORKER_ITEM_COUNT, MSVAD_POOLTAG);
    if (m_pWorkItems)
    {
        for (int i = 0; i < MAX_WORKER_ITEM_COUNT; i++)
        {
            m_pWorkItems[i].WorkItem = IoAllocateWorkItem(DeviceObject);
            if (m_pWorkItems[i].WorkItem == nullptr)
            {
              return STATUS_INSUFFICIENT_RESOURCES;
            }
            KeInitializeEvent(&m_pWorkItems[i].EventDone, NotificationEvent, TRUE);
        }
    }
    else
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    return ntStatus;
}

//=============================================================================

IO_WORKITEM_ROUTINE SaveFrameWorkerCallback;

VOID
SaveFrameWorkerCallback
(
    PDEVICE_OBJECT pDeviceObject, IN  PVOID  Context
)
{
    UNREFERENCED_PARAMETER(pDeviceObject);
    PAGED_CODE();
    ASSERT(Context);

    PSAVEWORKER_PARAM pParam = (PSAVEWORKER_PARAM) Context;

    if (!pParam)
    {
        // This is completely unexpected, assert here.
        //
        ASSERT(pParam);
        return;
    }

    DPF(D_VERBOSE, ("[SaveFrameWorkerCallback], %d", pParam->ulFrameNo));

    ASSERT(pParam->pSaveData);
    ASSERT(pParam->pSaveData->m_fFrameUsed);

    if (pParam->WorkItem)
    {
        PCSaveData pSaveData = pParam->pSaveData;

        if (STATUS_SUCCESS == KeWaitForSingleObject(&pSaveData->m_FileSync, Executive, KernelMode, FALSE, nullptr))
        {
            if (NT_SUCCESS(pSaveData->FileOpen(FALSE)))
            { 
                pSaveData->FileWrite(pParam->pData, pParam->ulDataSize);
                pSaveData->FileClose();
            }
            InterlockedExchange( (LONG *)&(pSaveData->m_fFrameUsed[pParam->ulFrameNo]), FALSE );

            KeReleaseMutex( &pSaveData->m_FileSync, FALSE );
        }
    }

    KeSetEvent(&pParam->EventDone, 0, FALSE);
}

//=============================================================================
NTSTATUS CSaveData::SetDataFormat(IN PKSDATAFORMAT pDataFormat)
{
    PAGED_CODE();
    NTSTATUS ntStatus = STATUS_SUCCESS; 
    DPF_ENTER(("[CSaveData::SetDataFormat]"));
    ASSERT(pDataFormat);

    PWAVEFORMATEX pwfx = nullptr;

    if (IsEqualGUIDAligned(pDataFormat->Specifier, KSDATAFORMAT_SPECIFIER_DSOUND))
    {
        pwfx = &(((PKSDATAFORMAT_DSOUND) pDataFormat)->BufferDesc.WaveFormatEx);
    }
    else if (IsEqualGUIDAligned(pDataFormat->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))
    {
        pwfx = &((PKSDATAFORMAT_WAVEFORMATEX) pDataFormat)->WaveFormatEx;
    }

    if (pwfx)
    {
        // Free the previously allocated waveformat
        if (m_waveFormat)
        {
            ExFreePoolWithTag(m_waveFormat, MSVAD_POOLTAG);
        }

        const SIZE_T numberOfBytes = (pwfx->wFormatTag == WAVE_FORMAT_PCM) ? sizeof(PCMWAVEFORMAT) : sizeof(WAVEFORMATEX) + pwfx->cbSize;
        m_waveFormat = (PWAVEFORMATEX)ExAllocatePoolWithTag(NonPagedPool, numberOfBytes, MSVAD_POOLTAG);

        if(m_waveFormat)
        {
            const size_t length = (pwfx->wFormatTag == WAVE_FORMAT_PCM) ? sizeof(PCMWAVEFORMAT) : sizeof(WAVEFORMATEX) + pwfx->cbSize;
            RtlCopyMemory(m_waveFormat, pwfx, length);
        }
        else
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }
    return ntStatus;
}

//=============================================================================
void
CSaveData::ReadData
(
    _Inout_updates_bytes_all_(ulByteCount)  PBYTE   pBuffer,
    _In_                                    ULONG   ulByteCount
)
{
    UNREFERENCED_PARAMETER(pBuffer);
    UNREFERENCED_PARAMETER(ulByteCount);

    PAGED_CODE();

    // Not implemented yet.
}

//=============================================================================
#pragma code_seg()
void CSaveData::SaveFrame(IN ULONG frameNo, IN ULONG dataSize)
{
    DPF_ENTER(("[CSaveData::SaveFrame]"));

    PSAVEWORKER_PARAM pParam = GetNewWorkItem();
    if (pParam)
    {
        pParam->pSaveData  = this;
        pParam->ulFrameNo  = frameNo;
        pParam->ulDataSize = dataSize;
        pParam->pData      = m_pDataBuffer + frameNo * m_ulFrameSize;
        KeResetEvent(&pParam->EventDone);
        IoQueueWorkItem(pParam->WorkItem, SaveFrameWorkerCallback, CriticalWorkQueue, (PVOID)pParam);
    }
}
#pragma code_seg("PAGE")
//=============================================================================
void CSaveData::WaitAllWorkItems()
{
    PAGED_CODE();
    DPF_ENTER(("[CSaveData::WaitAllWorkItems]"));

    // Save the last partially-filled frame
    SaveFrame(m_ulFramePtr, m_ulBufferPtr - (m_ulFramePtr * m_ulFrameSize));

    for (int i = 0; i < MAX_WORKER_ITEM_COUNT; i++)
    {
        DPF(D_VERBOSE, ("[Waiting for WorkItem] %d", i));
        KeWaitForSingleObject(&(m_pWorkItems[i].EventDone), Executive, KernelMode, FALSE, nullptr);
    }
}

#pragma code_seg()
//=============================================================================
void
CSaveData::WriteData
(
    _In_reads_bytes_(byteCount)   PBYTE   pBuffer,
    _In_                          ULONG   byteCount
)
{
    ASSERT(pBuffer);

    BOOL fSaveFrame = FALSE;
    ULONG ulSaveFramePtr = 0;

    // If stream writing is disabled, then exit.
    //
    if (m_fWriteDisabled)
    {
        return;
    }

    DPF_ENTER(("[CSaveData::WriteData ulByteCount=%lu]", byteCount));

    if ( 0 == byteCount )
    {
        return;
    }

    // Check to see if this frame is available.
    KeAcquireSpinLockAtDpcLevel( &m_FrameInUseSpinLock );
    if (!m_fFrameUsed[m_ulFramePtr])
    {
        KeReleaseSpinLockFromDpcLevel( &m_FrameInUseSpinLock );

        ULONG writeBytes = byteCount;

        if( (m_ulBufferSize - m_ulBufferPtr) < writeBytes )
        {
            writeBytes = m_ulBufferSize - m_ulBufferPtr;
        }

        RtlCopyMemory(m_pDataBuffer + m_ulBufferPtr, pBuffer, writeBytes);
        m_ulBufferPtr += writeBytes;

        // Check to see if we need to save this frame
        if (m_ulBufferPtr >= ((m_ulFramePtr + 1) * m_ulFrameSize))
        {
            fSaveFrame = TRUE;
        }

        // Loop the buffer, if we reached the end.
        if (m_ulBufferPtr == m_ulBufferSize)
        {
            fSaveFrame = TRUE;
            m_ulBufferPtr = 0;
        }

        if (fSaveFrame)
        {
            InterlockedExchange( (LONG *)&(m_fFrameUsed[m_ulFramePtr]), TRUE );
            ulSaveFramePtr = m_ulFramePtr;
            m_ulFramePtr = (m_ulFramePtr + 1) % m_ulFrameCount;
        }

        // Write the left over if the next frame is available.
        if (writeBytes != byteCount)
        {
            KeAcquireSpinLockAtDpcLevel( &m_FrameInUseSpinLock );
            if (!m_fFrameUsed[m_ulFramePtr])
            {
                KeReleaseSpinLockFromDpcLevel( &m_FrameInUseSpinLock );
                RtlCopyMemory
                (
                    m_pDataBuffer + m_ulBufferPtr,
                    pBuffer + writeBytes,
                    byteCount - writeBytes
                );
                m_ulBufferPtr += byteCount - writeBytes;
            }
            else
            {
                KeReleaseSpinLockFromDpcLevel( &m_FrameInUseSpinLock );
                DPF(D_BLAB, ("[Frame overflow, next frame is in use]"));
            }
        }

        if (fSaveFrame)
        {
            SaveFrame(ulSaveFramePtr, m_ulFrameSize);
        }
    }
    else
    {
        KeReleaseSpinLockFromDpcLevel( &m_FrameInUseSpinLock );
        DPF(D_BLAB, ("[Frame %d is in use]", m_ulFramePtr));
    }
}


