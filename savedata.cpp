/*
Abstract:
    Implementation of MSVAD data saving class.

    To save the playback data to disk, this class maintains a circular data
    buffer, associated frame structures and worker items to save frames to disk.
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
ULONG CSaveData::streamId_ = 0;

#pragma code_seg("PAGE")
//=============================================================================
// CSaveData
//=============================================================================

//=============================================================================
CSaveData::CSaveData()
:   dataBuffer_(nullptr),
    fileHandle_(nullptr),
    frameCount_(DEFAULT_FRAME_COUNT),
    bufferSize_(DEFAULT_BUFFER_SIZE),
    frameSize_(DEFAULT_FRAME_SIZE),
    bufferPtr_(0),
    framePtr_(0),
    frameUsed_(nullptr),
    filePtr_(nullptr),
    writeDisabled_(FALSE),
    initialized_(FALSE)
{

    PAGED_CODE();

    waveFormat_ = nullptr;
    fileHeader_.dwRiff           = RIFF_TAG;
    fileHeader_.dwFileSize       = 0;
    fileHeader_.dwWave           = WAVE_TAG;
    fileHeader_.dwFormat         = FMT__TAG;
    fileHeader_.dwFormatLength   = sizeof(WAVEFORMATEX);

    dataHeader_.dwData           = DATA_TAG;
    dataHeader_.dwDataLength     = 0;

    RtlZeroMemory(&objectAttributes_, sizeof(objectAttributes_));

    streamId_++;
    initializeWorkItems(getDeviceObject());
}

//=============================================================================
CSaveData::~CSaveData()
{
    PAGED_CODE();

    DPF_ENTER(("[CSaveData::~CSaveData]"));

    // Update the wave header in data file with real file size.
    //
    if(filePtr_)
    {
        fileHeader_.dwFileSize   = (DWORD) filePtr_->QuadPart - 2 * sizeof(DWORD);
        dataHeader_.dwDataLength = (DWORD) filePtr_->QuadPart - sizeof(fileHeader_)
                                 - fileHeader_.dwFormatLength - sizeof(dataHeader_);

        if (STATUS_SUCCESS == KeWaitForSingleObject(&fileSync_, Executive, KernelMode, FALSE, nullptr))
        {
            if (NT_SUCCESS(fileOpen(FALSE)))
            {
                fileWriteHeader();
                fileClose();
            }

            KeReleaseMutex(&fileSync_, FALSE);
        }
    }

   // frees the work items
   for (int i = 0; i < MAX_WORKER_ITEM_COUNT; i++)
   {
    
       if (workItems_[i].WorkItem!=nullptr)
       {
           IoFreeWorkItem(workItems_[i].WorkItem);
           workItems_[i].WorkItem = nullptr;
       }
   }

    if (waveFormat_)
    {
        ExFreePoolWithTag(waveFormat_, MSVAD_POOLTAG);
    }

    if (frameUsed_)
    {
        ExFreePoolWithTag(frameUsed_, MSVAD_POOLTAG);

        // NOTE : Do not release m_pFilePtr.
    }

    if (fileName_.Buffer)
    {
        ExFreePoolWithTag(fileName_.Buffer, MSVAD_POOLTAG);
    }

    if (dataBuffer_)
    {
        ExFreePoolWithTag(dataBuffer_, MSVAD_POOLTAG);
    }
}

//=============================================================================
void CSaveData::destroyWorkItems()
{
    PAGED_CODE();

    if (workItems_)
    {
        ExFreePoolWithTag(workItems_, MSVAD_POOLTAG);
        workItems_ = nullptr;
    }
}

//=============================================================================
void CSaveData::disable(BOOL fDisable)
{
    PAGED_CODE();

    writeDisabled_ = fDisable;
}

//=============================================================================
NTSTATUS CSaveData::fileClose()
{
    PAGED_CODE();

    NTSTATUS ntStatus = STATUS_SUCCESS;

    if (fileHandle_)
    {
        ntStatus = ZwClose(fileHandle_);
        fileHandle_ = nullptr;
    }

    return ntStatus;
}

//=============================================================================
NTSTATUS CSaveData::fileOpen(IN  BOOL fOverWrite)
{
    PAGED_CODE();

    NTSTATUS        ntStatus = STATUS_SUCCESS;
    IO_STATUS_BLOCK ioStatusBlock;

    if( FALSE == initialized_ )
    {
        return STATUS_UNSUCCESSFUL;
    }

    if(!fileHandle_)
    {
        ntStatus = ZwCreateFile(&fileHandle_,
                                GENERIC_WRITE | SYNCHRONIZE,
                                &objectAttributes_,
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
NTSTATUS CSaveData::fileWrite
(
    _In_reads_bytes_(ulDataSize)    PBYTE   pData,
    _In_                            ULONG   ulDataSize
)
{
    PAGED_CODE();

    ASSERT(pData);
    ASSERT(filePtr_);

    NTSTATUS ntStatus;

    if (fileHandle_)
    {
        IO_STATUS_BLOCK ioStatusBlock;

        ntStatus = ZwWriteFile(fileHandle_, nullptr, nullptr, nullptr, &ioStatusBlock, pData, ulDataSize, filePtr_, nullptr);

        if (NT_SUCCESS(ntStatus))
        {
            ASSERT(ioStatusBlock.Information == ulDataSize);

            filePtr_->QuadPart += ulDataSize;
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
NTSTATUS CSaveData::fileWriteHeader()
{
    PAGED_CODE();

    NTSTATUS ntStatus;

    if (fileHandle_ && waveFormat_)
    {
        IO_STATUS_BLOCK ioStatusBlock;

        filePtr_->QuadPart = 0;

        fileHeader_.dwFormatLength = (waveFormat_->wFormatTag == WAVE_FORMAT_PCM)
                                      ?  sizeof( PCMWAVEFORMAT )
                                      :  sizeof( WAVEFORMATEX ) + waveFormat_->cbSize;

        ntStatus = ZwWriteFile(fileHandle_, nullptr, nullptr, nullptr,
                               &ioStatusBlock,
                               &fileHeader_,
                               sizeof(fileHeader_),
                               filePtr_,
                               nullptr);
        if (!NT_SUCCESS(ntStatus))
        {
            DPF(D_TERSE, ("[CSaveData::FileWriteHeader : Write File Header Error]"));
        }

        filePtr_->QuadPart += sizeof(fileHeader_);

        ntStatus = ZwWriteFile(fileHandle_, nullptr, nullptr, nullptr,
                                &ioStatusBlock,
                                waveFormat_,
                                fileHeader_.dwFormatLength,
                                filePtr_,
                                nullptr);
        if (!NT_SUCCESS(ntStatus))
        {
            DPF(D_TERSE, ("[CSaveData::FileWriteHeader : Write Format Error]"));
        }

        filePtr_->QuadPart += fileHeader_.dwFormatLength;

        ntStatus = ZwWriteFile(fileHandle_, nullptr, nullptr, nullptr,
                                &ioStatusBlock,
                                &dataHeader_,
                                sizeof(dataHeader_),
                                filePtr_,
                                nullptr);
        if (!NT_SUCCESS(ntStatus))
        {
            DPF(D_TERSE, ("[CSaveData::FileWriteHeader : Write Data Header Error]"));
        }

        filePtr_->QuadPart += sizeof(dataHeader_);
    }
    else
    {
        DPF(D_TERSE, ("[CSaveData::FileWriteHeader : File not open]"));
        ntStatus = STATUS_INVALID_HANDLE;
    }

    return ntStatus;
}
NTSTATUS CSaveData::setDeviceObject(IN  PDEVICE_OBJECT deviceObject)
{
    PAGED_CODE();

    ASSERT(deviceObject);

    NTSTATUS ntStatus = STATUS_SUCCESS;
    
    deviceObject_ = deviceObject;
    return ntStatus;
}

PDEVICE_OBJECT
CSaveData::getDeviceObject()
{
    PAGED_CODE();

    return deviceObject_;
}

#pragma code_seg()
//=============================================================================
PSAVEWORKER_PARAM
CSaveData::getNewWorkItem()
{
    LARGE_INTEGER timeOut = { 0 };

    for (int i = 0; i < MAX_WORKER_ITEM_COUNT; i++)
    {
        NTSTATUS ntStatus = KeWaitForSingleObject(&workItems_[i].EventDone, Executive, KernelMode, FALSE, &timeOut);
        if (STATUS_SUCCESS == ntStatus)
        {
            if (workItems_[i].WorkItem)
                return &(workItems_[i]);
            else
                return nullptr;
        }
    }

    return nullptr;
}
#pragma code_seg("PAGE")

//=============================================================================
NTSTATUS CSaveData::initialize()
{
    PAGED_CODE();

    WCHAR       szTemp[MAX_PATH];
    size_t      cLen;

    DPF_ENTER(("[CSaveData::Initialize]"));

    // Allocate data file name.
    //
    RtlStringCchPrintfW(szTemp, MAX_PATH, L"%s_%d.wav", DEFAULT_FILE_NAME, streamId_);
    fileName_.Length = 0;
    NTSTATUS ntStatus = RtlStringCchLengthW(szTemp, sizeof(szTemp)/sizeof(szTemp[0]), &cLen);
    if (NT_SUCCESS(ntStatus))
    {
        fileName_.MaximumLength = (USHORT)((cLen * sizeof(WCHAR)) + sizeof(WCHAR));//convert to wchar and add room for nullptr
        fileName_.Buffer = (PWSTR)ExAllocatePoolWithTag(PagedPool, fileName_.MaximumLength, MSVAD_POOLTAG);
        if (!fileName_.Buffer)
        {
            DPF(D_TERSE, ("[Could not allocate memory for FileName]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    // Allocate memory for data buffer.
    //
    if (NT_SUCCESS(ntStatus))
    {
        RtlStringCbCopyW(fileName_.Buffer, fileName_.MaximumLength, szTemp);
        fileName_.Length = (USHORT)wcslen(fileName_.Buffer) * sizeof(WCHAR);
        DPF(D_BLAB, ("[New DataFile -- %S", fileName_.Buffer));

        dataBuffer_ = (PBYTE)ExAllocatePoolWithTag(NonPagedPool, bufferSize_, MSVAD_POOLTAG);
        if (!dataBuffer_)
        {
            DPF(D_TERSE, ("[Could not allocate memory for Saving Data]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    // Allocate memory for frame usage flags and m_pFilePtr.
    //
    if (NT_SUCCESS(ntStatus))
    {
        frameUsed_ = (PBOOL)ExAllocatePoolWithTag(NonPagedPool, frameCount_ * sizeof(BOOL) + sizeof(LARGE_INTEGER), MSVAD_POOLTAG);
        if (!frameUsed_)
        {
            DPF(D_TERSE, ("[Could not allocate memory for frame flags]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    // Initialize the spinlock to synchronize access to the frames
    //
    KeInitializeSpinLock ( &frameInUseSpinLock_ ) ;

    // Initialize the file mutex
    //
    KeInitializeMutex( &fileSync_, 1 ) ;

    // Open the data file.
    //
    if (NT_SUCCESS(ntStatus))
    {
        // m_fFrameUsed has additional memory to hold m_pFilePtr
        //
        filePtr_ = (PLARGE_INTEGER)(((PBYTE) frameUsed_) + frameCount_ * sizeof(BOOL));
        RtlZeroMemory(frameUsed_, frameCount_ * sizeof(BOOL) + sizeof(LARGE_INTEGER));

        // Create data file.
        InitializeObjectAttributes(&objectAttributes_, &fileName_, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr);

        initialized_ = TRUE;

        // Write wave header information to data file.
        ntStatus = KeWaitForSingleObject(&fileSync_, Executive, KernelMode, FALSE, nullptr);

        if (STATUS_SUCCESS == ntStatus)
        {
            ntStatus = fileOpen(TRUE);
            if (NT_SUCCESS(ntStatus))
            {
                ntStatus = fileWriteHeader();

                fileClose();
            }

            KeReleaseMutex( &fileSync_, FALSE );
        }
    }

    return ntStatus;
}

//=============================================================================
NTSTATUS CSaveData::initializeWorkItems(IN  PDEVICE_OBJECT deviceObject)
{
    PAGED_CODE();
    ASSERT(deviceObject);
    NTSTATUS ntStatus = STATUS_SUCCESS;

    DPF_ENTER(("[CSaveData::InitializeWorkItems]"));

    if (workItems_)
    {
        return ntStatus;
    }

    workItems_ = (PSAVEWORKER_PARAM)ExAllocatePoolWithTag(NonPagedPool, sizeof(SAVEWORKER_PARAM) * MAX_WORKER_ITEM_COUNT, MSVAD_POOLTAG);
    if (workItems_)
    {
        for (int i = 0; i < MAX_WORKER_ITEM_COUNT; i++)
        {
            workItems_[i].WorkItem = IoAllocateWorkItem(deviceObject);
            if (workItems_[i].WorkItem == nullptr)
            {
              return STATUS_INSUFFICIENT_RESOURCES;
            }
            KeInitializeEvent(&workItems_[i].EventDone, NotificationEvent, TRUE);
        }
    }
    else
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    return ntStatus;
}

//=============================================================================

IO_WORKITEM_ROUTINE saveFrameWorkerCallback;

VOID saveFrameWorkerCallback(PDEVICE_OBJECT deviceObject, IN  PVOID  context)
{
    UNREFERENCED_PARAMETER(deviceObject);
    PAGED_CODE();
    ASSERT(context);

    PSAVEWORKER_PARAM pParam = (PSAVEWORKER_PARAM) context;

    if (!pParam)
    {
        // This is completely unexpected, assert here.
        //
        ASSERT(pParam);
        return;
    }

    DPF(D_VERBOSE, ("[SaveFrameWorkerCallback], %d", pParam->ulFrameNo));

    ASSERT(pParam->pSaveData);
    ASSERT(pParam->pSaveData->frameUsed_);

    if (pParam->WorkItem)
    {
        PCSaveData saveData = pParam->pSaveData;

        if (STATUS_SUCCESS == KeWaitForSingleObject(&saveData->fileSync_, Executive, KernelMode, FALSE, nullptr))
        {
            if (NT_SUCCESS(saveData->fileOpen(FALSE)))
            { 
                saveData->fileWrite(pParam->pData, pParam->ulDataSize);
                saveData->fileClose();
            }
            InterlockedExchange( (LONG *)&(saveData->frameUsed_[pParam->ulFrameNo]), FALSE );

            KeReleaseMutex( &saveData->fileSync_, FALSE );
        }
    }

    KeSetEvent(&pParam->EventDone, 0, FALSE);
}

//=============================================================================
NTSTATUS CSaveData::setDataFormat(IN PKSDATAFORMAT dataFormat)
{
    PAGED_CODE();
    NTSTATUS ntStatus = STATUS_SUCCESS; 
    DPF_ENTER(("[CSaveData::SetDataFormat]"));
    ASSERT(dataFormat);

    PWAVEFORMATEX wfx = nullptr;

    if (IsEqualGUIDAligned(dataFormat->Specifier, KSDATAFORMAT_SPECIFIER_DSOUND))
    {
        wfx = &(((PKSDATAFORMAT_DSOUND) dataFormat)->BufferDesc.WaveFormatEx);
    }
    else if (IsEqualGUIDAligned(dataFormat->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))
    {
        wfx = &((PKSDATAFORMAT_WAVEFORMATEX) dataFormat)->WaveFormatEx;
    }

    if (wfx)
    {
        // Free the previously allocated waveformat
        if (waveFormat_)
        {
            ExFreePoolWithTag(waveFormat_, MSVAD_POOLTAG);
        }

        const SIZE_T numberOfBytes = (wfx->wFormatTag == WAVE_FORMAT_PCM) ? sizeof(PCMWAVEFORMAT) : sizeof(WAVEFORMATEX) + wfx->cbSize;
        waveFormat_ = (PWAVEFORMATEX)ExAllocatePoolWithTag(NonPagedPool, numberOfBytes, MSVAD_POOLTAG);

        if(waveFormat_)
        {
            const size_t length = (wfx->wFormatTag == WAVE_FORMAT_PCM) ? sizeof(PCMWAVEFORMAT) : sizeof(WAVEFORMATEX) + wfx->cbSize;
            RtlCopyMemory(waveFormat_, wfx, length);
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
CSaveData::readData
(
    _Inout_updates_bytes_all_(byteCount) PBYTE buffer,
    _In_                                 ULONG byteCount
)
{
    UNREFERENCED_PARAMETER(buffer);
    UNREFERENCED_PARAMETER(byteCount);

    PAGED_CODE();

    // Not implemented yet.
}

//=============================================================================
#pragma code_seg()
void CSaveData::saveFrame(IN ULONG frameNo, IN ULONG dataSize)
{
    DPF_ENTER(("[CSaveData::SaveFrame]"));

    PSAVEWORKER_PARAM pParam = getNewWorkItem();
    if (pParam)
    {
        pParam->pSaveData  = this;
        pParam->ulFrameNo  = frameNo;
        pParam->ulDataSize = dataSize;
        pParam->pData      = dataBuffer_ + frameNo * frameSize_;
        KeResetEvent(&pParam->EventDone);
        IoQueueWorkItem(pParam->WorkItem, saveFrameWorkerCallback, CriticalWorkQueue, (PVOID)pParam);
    }
}
#pragma code_seg("PAGE")
//=============================================================================
void CSaveData::waitAllWorkItems()
{
    PAGED_CODE();
    DPF_ENTER(("[CSaveData::WaitAllWorkItems]"));

    // Save the last partially-filled frame
    saveFrame(framePtr_, bufferPtr_ - (framePtr_ * frameSize_));

    for (int i = 0; i < MAX_WORKER_ITEM_COUNT; i++)
    {
        DPF(D_VERBOSE, ("[Waiting for WorkItem] %d", i));
        KeWaitForSingleObject(&(workItems_[i].EventDone), Executive, KernelMode, FALSE, nullptr);
    }
}

#pragma code_seg()
//=============================================================================
void
CSaveData::writeData
(
    _In_reads_bytes_(byteCount) PBYTE buffer,
    _In_                        ULONG byteCount
)
{
    ASSERT(buffer);

    BOOL fSaveFrame = FALSE;
    ULONG ulSaveFramePtr = 0;

    // If stream writing is disabled, then exit.
    //
    if (writeDisabled_)
    {
        return;
    }

    DPF_ENTER(("[CSaveData::WriteData ulByteCount=%lu]", byteCount));

    if ( 0 == byteCount )
    {
        return;
    }

    // Check to see if this frame is available.
    KeAcquireSpinLockAtDpcLevel( &frameInUseSpinLock_ );
    if (!frameUsed_[framePtr_])
    {
        KeReleaseSpinLockFromDpcLevel( &frameInUseSpinLock_ );

        ULONG writeBytes = byteCount;

        if( (bufferSize_ - bufferPtr_) < writeBytes )
        {
            writeBytes = bufferSize_ - bufferPtr_;
        }

        RtlCopyMemory(dataBuffer_ + bufferPtr_, buffer, writeBytes); 
        bufferPtr_ += writeBytes;

        // Check to see if we need to save this frame
        if (bufferPtr_ >= ((framePtr_ + 1) * frameSize_))
        {
            fSaveFrame = TRUE;
        }

        // Loop the buffer, if we reached the end.
        if (bufferPtr_ == bufferSize_)
        {
            fSaveFrame = TRUE;
            bufferPtr_ = 0;
        }

        if (fSaveFrame)
        {
            InterlockedExchange( (LONG *)&(frameUsed_[framePtr_]), TRUE );
            ulSaveFramePtr = framePtr_;
            framePtr_ = (framePtr_ + 1) % frameCount_;
        }

        // Write the left over if the next frame is available.
        if (writeBytes != byteCount)
        {
            KeAcquireSpinLockAtDpcLevel( &frameInUseSpinLock_ );
            if (!frameUsed_[framePtr_])
            {
                KeReleaseSpinLockFromDpcLevel( &frameInUseSpinLock_ );
                RtlCopyMemory
                (
                    dataBuffer_ + bufferPtr_,
                    buffer + writeBytes,
                    byteCount - writeBytes
                );
                bufferPtr_ += byteCount - writeBytes;
            }
            else
            {
                KeReleaseSpinLockFromDpcLevel( &frameInUseSpinLock_ );
                DPF(D_BLAB, ("[Frame overflow, next frame is in use]"));
            }
        }

        if (fSaveFrame)
        {
            saveFrame(ulSaveFramePtr, frameSize_);
        }
    }
    else
    {
        KeReleaseSpinLockFromDpcLevel( &frameInUseSpinLock_ );
        DPF(D_BLAB, ("[Frame %d is in use]", framePtr_));
    }
}