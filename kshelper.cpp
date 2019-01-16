/*
Abstract:
    Helper functions for msvad
*/

#include "kshelper.h"

#pragma code_seg("PAGE")

//-----------------------------------------------------------------------------
/*
Routine Description:
  Returns the waveformatex for known formats. 

Arguments:
    pDataFormat - data format.
    waveformatex in DataFormat.
    nullptr for unknown data formats.
*/
PWAVEFORMATEX GetWaveFormatEx(IN  PKSDATAFORMAT pDataFormat)
{
    PAGED_CODE();

    PWAVEFORMATEX pWfx = nullptr;
    
    // If this is a known dataformat extract the waveformat info.
    //
    if (pDataFormat &&
        ( IsEqualGUIDAligned(pDataFormat->MajorFormat, KSDATAFORMAT_TYPE_AUDIO)             &&
        ( IsEqualGUIDAligned(pDataFormat->Specifier,   KSDATAFORMAT_SPECIFIER_WAVEFORMATEX) ||
          IsEqualGUIDAligned(pDataFormat->Specifier,   KSDATAFORMAT_SPECIFIER_DSOUND))))
    {
        pWfx = PWAVEFORMATEX(pDataFormat + 1);

        if (IsEqualGUIDAligned(pDataFormat->Specifier, KSDATAFORMAT_SPECIFIER_DSOUND))
        {
            PKSDSOUND_BUFFERDESC pwfxds = PKSDSOUND_BUFFERDESC(pDataFormat + 1);
            pWfx = &pwfxds->WaveFormatEx;
        }
    }

    return pWfx;
}

//-----------------------------------------------------------------------------
/*
Routine Description:
  Default basic support handler. Basic processing depends on the size of data.
  For ULONG it only returns Flags. For KSPROPERTY_DESCRIPTION, the structure is filled.

Arguments:
  Flags         - Support flags.
  PropTypeSetId - PropTypeSetId
*/
NTSTATUS
PropertyHandler_BasicSupport
(
    IN PPCPROPERTY_REQUEST PropertyRequest,
    IN ULONG               Flags,
    IN DWORD               PropTypeSetId
)
{
    PAGED_CODE();

    ASSERT(Flags & KSPROPERTY_TYPE_BASICSUPPORT);

    NTSTATUS ntStatus = STATUS_INVALID_PARAMETER;

    if (PropertyRequest->ValueSize >= sizeof(KSPROPERTY_DESCRIPTION))
    {
        // if return buffer can hold a KSPROPERTY_DESCRIPTION, return it
        //
        PKSPROPERTY_DESCRIPTION PropDesc = PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);

        PropDesc->AccessFlags       = Flags;
        PropDesc->DescriptionSize   = sizeof(KSPROPERTY_DESCRIPTION);
        if  (VT_ILLEGAL != PropTypeSetId)
        {
            PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
            PropDesc->PropTypeSet.Id    = PropTypeSetId;
        }
        else
        {
            PropDesc->PropTypeSet.Set   = GUID_NULL;
            PropDesc->PropTypeSet.Id    = 0;
        }
        PropDesc->PropTypeSet.Flags = 0;
        PropDesc->MembersListCount  = 0;
        PropDesc->Reserved          = 0;

        PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
        ntStatus = STATUS_SUCCESS;
    } 
    else if (PropertyRequest->ValueSize >= sizeof(ULONG))
    {
        // if return buffer can hold a ULONG, return the access flags
        //
        *(PULONG(PropertyRequest->Value)) = Flags;

        PropertyRequest->ValueSize = sizeof(ULONG);
        ntStatus = STATUS_SUCCESS;                    
    }
    else
    {
        PropertyRequest->ValueSize = 0;
        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
}

//-----------------------------------------------------------------------------
/*
Routine Description:
  Validates property parameters.
*/
NTSTATUS
ValidatePropertyParams
(
    IN PPCPROPERTY_REQUEST PropertyRequest, 
    IN ULONG               cbSize,
    IN ULONG               cbInstanceSize // = 0
)
{
    PAGED_CODE();

    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

    if (PropertyRequest && cbSize)
    {
        // If the caller is asking for ValueSize.
        //
        if (0 == PropertyRequest->ValueSize) 
        {
            PropertyRequest->ValueSize = cbSize;
            ntStatus = STATUS_BUFFER_OVERFLOW;
        }
        // If the caller passed an invalid ValueSize.
        //
        else if (PropertyRequest->ValueSize < cbSize)
        {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
        }
        else if (PropertyRequest->InstanceSize < cbInstanceSize)
        {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
        }
        // If all parameters are OK.
        // 
        else if (PropertyRequest->ValueSize == cbSize)
        {
            if (PropertyRequest->Value)
            {
                ntStatus = STATUS_SUCCESS;
                //
                // Caller should set ValueSize, if the property 
                // call is successful.
                //
            }
        }
    }
    else
    {
        ntStatus = STATUS_INVALID_PARAMETER;
    }
    
    // Clear the ValueSize if unsuccessful.
    //
    if (PropertyRequest            &&
        ntStatus != STATUS_SUCCESS &&
        ntStatus != STATUS_BUFFER_OVERFLOW)
    {
        PropertyRequest->ValueSize = 0;
    }

    return ntStatus;
}