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
PWAVEFORMATEX GetWaveFormatEx(IN PKSDATAFORMAT dataFormat)
{
    PAGED_CODE();

    PWAVEFORMATEX wfx = nullptr;
    
    // If this is a known dataformat extract the waveformat info.
    //
    if (dataFormat &&
        ( IsEqualGUIDAligned(dataFormat->MajorFormat, KSDATAFORMAT_TYPE_AUDIO)             &&
        ( IsEqualGUIDAligned(dataFormat->Specifier,   KSDATAFORMAT_SPECIFIER_WAVEFORMATEX) ||
          IsEqualGUIDAligned(dataFormat->Specifier,   KSDATAFORMAT_SPECIFIER_DSOUND))))
    {
        wfx = PWAVEFORMATEX(dataFormat + 1);

        if (IsEqualGUIDAligned(dataFormat->Specifier, KSDATAFORMAT_SPECIFIER_DSOUND))
        {
            PKSDSOUND_BUFFERDESC pwfxds = PKSDSOUND_BUFFERDESC(dataFormat + 1);
            wfx = &pwfxds->WaveFormatEx;
        }
    }

    return wfx;
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
    IN PPCPROPERTY_REQUEST propertyRequest,
    IN ULONG               flags,
    IN DWORD               propTypeSetId
)
{
    PAGED_CODE();

    ASSERT(flags & KSPROPERTY_TYPE_BASICSUPPORT);

    NTSTATUS ntStatus = STATUS_INVALID_PARAMETER;

    if (propertyRequest->ValueSize >= sizeof(KSPROPERTY_DESCRIPTION))
    {
        // if return buffer can hold a KSPROPERTY_DESCRIPTION, return it
        //
        PKSPROPERTY_DESCRIPTION propDesc = PKSPROPERTY_DESCRIPTION(propertyRequest->Value);

        propDesc->AccessFlags       = flags;
        propDesc->DescriptionSize   = sizeof(KSPROPERTY_DESCRIPTION);
        if  (VT_ILLEGAL != propTypeSetId)
        {
            propDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
            propDesc->PropTypeSet.Id    = propTypeSetId;
        }
        else
        {
            propDesc->PropTypeSet.Set   = GUID_NULL;
            propDesc->PropTypeSet.Id    = 0;
        }
        propDesc->PropTypeSet.Flags = 0;
        propDesc->MembersListCount  = 0;
        propDesc->Reserved          = 0;

        propertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
        ntStatus = STATUS_SUCCESS;
    } 
    else if (propertyRequest->ValueSize >= sizeof(ULONG))
    {
        // if return buffer can hold a ULONG, return the access flags
        //
        *(PULONG(propertyRequest->Value)) = flags;

        propertyRequest->ValueSize = sizeof(ULONG);
        ntStatus = STATUS_SUCCESS;                    
    }
    else
    {
        propertyRequest->ValueSize = 0;
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
    IN PPCPROPERTY_REQUEST propertyRequest, 
    IN ULONG               cbSize,
    IN ULONG               cbInstanceSize // = 0
)
{
    PAGED_CODE();

    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

    if (propertyRequest && cbSize)
    {
        // If the caller is asking for ValueSize.
        //
        if (0 == propertyRequest->ValueSize) 
        {
            propertyRequest->ValueSize = cbSize;
            ntStatus = STATUS_BUFFER_OVERFLOW;
        }
        // If the caller passed an invalid ValueSize.
        //
        else if (propertyRequest->ValueSize < cbSize)
        {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
        }
        else if (propertyRequest->InstanceSize < cbInstanceSize)
        {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
        }
        // If all parameters are OK.
        // 
        else if (propertyRequest->ValueSize == cbSize)
        {
            if (propertyRequest->Value)
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
    if (propertyRequest            &&
        ntStatus != STATUS_SUCCESS &&
        ntStatus != STATUS_BUFFER_OVERFLOW)
    {
        propertyRequest->ValueSize = 0;
    }

    return ntStatus;
}