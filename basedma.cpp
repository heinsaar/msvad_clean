/*

Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:

    basedma.cpp

Abstract:

    IDmaChannel implementation. Does nothing HW related.

*/

#pragma warning (disable : 4127)

#include <msvad.h>
#include "common.h"
#include "basewave.h"

#pragma code_seg("PAGE")
//=============================================================================
/*
Routine Description:
  The AllocateBuffer function allocates a buffer associated with the DMA object.
  The buffer is nonPaged.
  Callers of AllocateBuffer should run at a passive IRQL.

Arguments:
  BufferSize - Size in bytes of the buffer to be allocated.
  PhysicalAddressConstraint - Optional constraint to place on the physical
                              address of the buffer. If supplied, only the bits
                              that are set in the constraint address may vary
                              from the beginning to the end of the buffer.
                              For example, if the desired buffer should not
                              cross a 64k boundary, the physical address
                              constraint 0x000000000000ffff should be specified
*/
_Use_decl_annotations_
STDMETHODIMP_(NTSTATUS)
MiniportWaveCyclicStreamMSVAD::AllocateBuffer(ULONG BufferSize, PPHYSICAL_ADDRESS PhysicalAddressConstraint OPTIONAL)
{
    UNREFERENCED_PARAMETER(PhysicalAddressConstraint);
    PAGED_CODE();
    DPF_ENTER(("[CMiniportWaveCyclicStreamMSVAD::AllocateBuffer]"));

    NTSTATUS ntStatus = STATUS_SUCCESS;

    // Adjust this cap as needed...
    ASSERT (BufferSize <= DMA_BUFFER_SIZE);

    dmaBuffer_ = (PVOID)ExAllocatePoolWithTag(NonPagedPool, BufferSize, MSVAD_POOLTAG);
    if (!dmaBuffer_)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }
    else
    {
        dmaBufferSize_ = BufferSize;
    }

    return ntStatus;
}
#pragma code_seg()

//=============================================================================
/*
Routine Description:
  AllocatedBufferSize returns the size of the allocated buffer.
  Callers of AllocatedBufferSize can run at any IRQL.
*/
STDMETHODIMP_(ULONG)
MiniportWaveCyclicStreamMSVAD::AllocatedBufferSize()
{
    DPF_ENTER(("[CMiniportWaveCyclicStreamMSVAD::AllocatedBufferSize]"));

    return dmaBufferSize_;
}

//=============================================================================
/*
Routine Description:

  BufferSize returns the size set by SetBufferSize or the allocated buffer size
  if the buffer size has not been set. The DMA object does not actually use
  this value internally. This value is maintained by the object to allow its
  various clients to communicate the intended size of the buffer. This call
  is often used to obtain the map size parameter to the Start member
  function. Callers of BufferSize can run at any IRQL
*/
STDMETHODIMP_(ULONG)
MiniportWaveCyclicStreamMSVAD::BufferSize()
{
    return dmaBufferSize_;
}

//=============================================================================
/*
Routine Description:
  The CopyFrom function copies sample data from the DMA buffer.
  Callers of CopyFrom can run at any IRQL

Arguments:
  Destination - Points to the destination buffer.
  Source - Points to the source buffer.
  ByteCount - Points to the source buffer.
*/
_Use_decl_annotations_
STDMETHODIMP_(void)
MiniportWaveCyclicStreamMSVAD::CopyFrom(PVOID Destination, PVOID Source, ULONG ByteCount)
{
    UNREFERENCED_PARAMETER(Destination);
    UNREFERENCED_PARAMETER(Source);
    UNREFERENCED_PARAMETER(ByteCount);
}

//=============================================================================
/*
Routine Description:

  The CopyTo function copies sample data to the DMA buffer.
  Callers of CopyTo can run at any IRQL.

Arguments:

  Destination - Points to the destination buffer.
  Source - Points to the source buffer
  ByteCount - Number of bytes to be copied
*/
_Use_decl_annotations_
STDMETHODIMP_(void)
MiniportWaveCyclicStreamMSVAD::CopyTo(PVOID Destination, PVOID Source, ULONG ByteCount)
{
    UNREFERENCED_PARAMETER(Destination);

    saveData_.writeData((PBYTE) Source, ByteCount);
}

//=============================================================================
/*
Routine Description:

  The FreeBuffer function frees the buffer allocated by AllocateBuffer. Because
  the buffer is automatically freed when the DMA object is deleted, this
  function is not normally used. Callers of FreeBuffer should run at IRQL PASSIVE_LEVEL.
*/
#pragma code_seg("PAGE")
STDMETHODIMP_(void)
MiniportWaveCyclicStreamMSVAD::FreeBuffer()
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveCyclicStreamMSVAD::FreeBuffer]"));

    if ( dmaBuffer_ )
    {
        ExFreePoolWithTag( dmaBuffer_, MSVAD_POOLTAG );
        dmaBufferSize_ = 0;
    }
}
#pragma code_seg()

//=============================================================================
/*
Routine Description:
  The GetAdapterObject function returns the DMA object's internal adapter
  object. Callers of GetAdapterObject can run at any IRQL.

Arguments:
  PADAPTER_OBJECT - The return value is the object's internal adapter object.
*/
STDMETHODIMP_(PADAPTER_OBJECT)
MiniportWaveCyclicStreamMSVAD::GetAdapterObject()
{
    DPF_ENTER(("[CMiniportWaveCyclicStreamMSVAD::GetAdapterObject]"));

    // MSVAD does not have need a physical DMA channel. Therefore it
    // does not have physical DMA structure.

    return nullptr;
}

//=============================================================================
STDMETHODIMP_(ULONG)
MiniportWaveCyclicStreamMSVAD::MaximumBufferSize()
{
    DPF_ENTER(("[CMiniportWaveCyclicStreamMSVAD::MaximumBufferSize]"));

    return miniport_->maxDmaBufferSize_;
}

//=============================================================================
/*
Routine Description:

  MaximumBufferSize returns the size in bytes of the largest buffer this DMA
  object is configured to support. Callers of MaximumBufferSize can run at any IRQL
   
  PHYSICAL_ADDRESS - The return value is the size in bytes of the largest
                     buffer this DMA object is configured to support.
*/
STDMETHODIMP_(PHYSICAL_ADDRESS)
MiniportWaveCyclicStreamMSVAD::PhysicalAddress()
{
    DPF_ENTER(("[CMiniportWaveCyclicStreamMSVAD::PhysicalAddress]"));

    PHYSICAL_ADDRESS pAddress;

    pAddress.QuadPart = (LONGLONG) dmaBuffer_;

    return pAddress;
}

//=============================================================================
/*
Routine Description:

  The SetBufferSize function sets the current buffer size. This value is set to
  the allocated buffer size when AllocateBuffer is called. The DMA object does
  not actually use this value internally. This value is maintained by the object
  to allow its various clients to communicate the intended size of the buffer.
  Callers of SetBufferSize can run at any IRQL.

Arguments:
  BufferSize - Current size in bytes.
*/
STDMETHODIMP_(void)
MiniportWaveCyclicStreamMSVAD::SetBufferSize(_In_ ULONG BufferSize)
{
    DPF_ENTER(("[CMiniportWaveCyclicStreamMSVAD::SetBufferSize]"));

    if ( BufferSize <= dmaBufferSize_ )
    {
        dmaBufferSize_ = BufferSize;
    }
    else
    {
        DPF(D_ERROR, ("Tried to enlarge dma buffer size"));
    }
}

//=============================================================================
/*
Routine Description:
  The SystemAddress function returns the virtual system address of the
  allocated buffer. Callers of SystemAddress can run at any IRQL.

  PVOID - The return value is the virtual system address of the allocated buffer.
*/
STDMETHODIMP_(PVOID)
MiniportWaveCyclicStreamMSVAD::SystemAddress()
{
    return dmaBuffer_;
}

//=============================================================================
/*
Routine Description:

  The TransferCount function returns the size in bytes of the buffer currently
  being transferred by a DMA object. Callers of TransferCount can run  at any IRQL.

  ULONG - The return value is the size in bytes of the buffer currently being transferred.
*/
STDMETHODIMP_(ULONG)
MiniportWaveCyclicStreamMSVAD::TransferCount()
{
    DPF_ENTER(("[CMiniportWaveCyclicStreamMSVAD::TransferCount]"));

    return dmaBufferSize_;
}

