/*
Abstract:
    Implementation of topology miniport. This the base class for all MSVAD samples
*/

#pragma warning (disable : 4127)

#include <msvad.h>
#include "common.h"
#include "basetopo.h"

//=============================================================================
#pragma code_seg("PAGE")
MiniportTopologyMSVAD::MiniportTopologyMSVAD()
{
    PAGED_CODE();

    DPF_ENTER(("[%s]", __FUNCTION__));

    adapterCommon_    = nullptr;
    filterDescriptor_ = nullptr;
}

MiniportTopologyMSVAD::~MiniportTopologyMSVAD()
{
    PAGED_CODE();

    DPF_ENTER(("[%s]",__FUNCTION__));

    if (adapterCommon_)
    {
        adapterCommon_->Release();
    }
}

//=============================================================================
/*
Routine Description:

  The DataRangeIntersection function determines the highest 
  quality intersection of two data ranges. Topology miniport does nothing.

Arguments:

  PinId                 - Pin for which data intersection is being determined. 

  ClientDataRange       - Pointer to KSDATARANGE structure which contains the data range 
                          submitted by client in the data range intersection property request

  MyDataRange           - Pin's data range to be compared with client's data range

  OutputBufferLength    - Size of the buffer pointed to by the resultant format parameter
  
  ResultantFormat       - Pointer to value where the resultant format should be returned

  ResultantFormatLength - Actual length of the resultant format that is placed 
                          at ResultantFormat. This should be less than or equal 
                          to OutputBufferLength
*/
NTSTATUS MiniportTopologyMSVAD::DataRangeIntersection
(
    _In_        ULONG        pinId,
    _In_        PKSDATARANGE clientDataRange,
    _In_        PKSDATARANGE myDataRange,
    _In_        ULONG        outputBufferLength,
    _Out_writes_bytes_to_opt_(outputBufferLength, *resultantFormatLength)
    PVOID                    resultantFormat,
    _Out_       PULONG       resultantFormatLength
)
{
    UNREFERENCED_PARAMETER(pinId);
    UNREFERENCED_PARAMETER(clientDataRange);
    UNREFERENCED_PARAMETER(myDataRange);
    UNREFERENCED_PARAMETER(outputBufferLength);
    UNREFERENCED_PARAMETER(resultantFormat);
    UNREFERENCED_PARAMETER(resultantFormatLength);

    PAGED_CODE();
    DPF_ENTER(("[%s]",__FUNCTION__));

    return (STATUS_NOT_IMPLEMENTED);
}

//=============================================================================
/*
Routine Description:
  The GetDescription function gets a pointer to a filter description. 
  It provides a location to deposit a pointer in miniport's description 
  structure. This is the placeholder for the FromNode or ToNode fields in 
  connections which describe connections to the filter's pins

Arguments:
  OutFilterDescriptor - Pointer to the filter description. 
*/
NTSTATUS MiniportTopologyMSVAD::GetDescription(_Out_ PPCFILTER_DESCRIPTOR* outFilterDescriptor)
{
    PAGED_CODE();
    ASSERT(outFilterDescriptor);
    DPF_ENTER(("[%s]",__FUNCTION__));
    *outFilterDescriptor = filterDescriptor_;
    return STATUS_SUCCESS;
}

//=============================================================================
/*
Routine Description:
  Initializes the topology miniport.

Arguments:
  Port_ - Pointer to topology port
*/
NTSTATUS MiniportTopologyMSVAD::Init(IN PUNKNOWN unknownAdapter, IN PPORTTOPOLOGY port)
{
    UNREFERENCED_PARAMETER(port);

    PAGED_CODE();

    ASSERT(unknownAdapter);
    ASSERT(port);

    DPF_ENTER(("[CMiniportTopologyMSVAD::Init]"));

    NTSTATUS ntStatus = unknownAdapter->QueryInterface(IID_IAdapterCommon, (PVOID *) &adapterCommon_);
    if (NT_SUCCESS(ntStatus))
    {
        adapterCommon_->mixerReset();
    }

    if (!NT_SUCCESS(ntStatus))
    {
        // clean up AdapterCommon
        if (adapterCommon_)
        {
            adapterCommon_->Release();
            adapterCommon_ = nullptr;
        }
    }

    return ntStatus;
}

//=============================================================================
/*

Routine Description:

  Handles BasicSupport for Volume nodes.

Arguments:
    
  PropertyRequest - property request structure

*/
NTSTATUS MiniportTopologyMSVAD::propertyHandlerBasicSupportVolume(IN  PPCPROPERTY_REQUEST propertyRequest)
{
    PAGED_CODE();

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG cbFullProperty = sizeof(KSPROPERTY_DESCRIPTION) +
                           sizeof(KSPROPERTY_MEMBERSHEADER) +
                           sizeof(KSPROPERTY_STEPPING_LONG);

    if (propertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION)))
    {
        PKSPROPERTY_DESCRIPTION PropDesc = PKSPROPERTY_DESCRIPTION(propertyRequest->Value);

        PropDesc->AccessFlags       = KSPROPERTY_TYPE_ALL;
        PropDesc->DescriptionSize   = cbFullProperty;
        PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
        PropDesc->PropTypeSet.Id    = VT_I4;
        PropDesc->PropTypeSet.Flags = 0;
        PropDesc->MembersListCount  = 1;
        PropDesc->Reserved          = 0;

        // if return buffer can also hold a range description, return it too
        if(propertyRequest->ValueSize >= cbFullProperty)
        {
            // fill in the members header
            PKSPROPERTY_MEMBERSHEADER members = PKSPROPERTY_MEMBERSHEADER(PropDesc + 1);

            members->MembersFlags   = KSPROPERTY_MEMBER_STEPPEDRANGES;
            members->MembersSize    = sizeof(KSPROPERTY_STEPPING_LONG);
            members->MembersCount   = 1;
            members->Flags          = KSPROPERTY_MEMBER_FLAG_BASICSUPPORT_MULTICHANNEL;

            // fill in the stepped range
            PKSPROPERTY_STEPPING_LONG range = PKSPROPERTY_STEPPING_LONG(members + 1);

            range->Bounds.SignedMaximum = 0x00000000;      //   0 dB
            range->Bounds.SignedMinimum = -96 * 0x10000;   // -96 dB
            range->SteppingDelta        = 0x08000;         //  .5 dB
            range->Reserved             = 0;

            // set the return value size
            propertyRequest->ValueSize = cbFullProperty;
        } 
        else
        {
            propertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
        }
    } 
    else if(propertyRequest->ValueSize >= sizeof(ULONG))
    {
        // if return buffer can hold a ULONG, return the access flags
        PULONG accessFlags = PULONG(propertyRequest->Value);

        propertyRequest->ValueSize = sizeof(ULONG);
        *accessFlags = KSPROPERTY_TYPE_ALL;
    }
    else
    {
        propertyRequest->ValueSize = 0;
        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
}

//=============================================================================
/*

Routine Description:

  Processes KSPROPERTY_AUDIO_CPURESOURCES

Arguments:
    
  PropertyRequest - property request structure
*/
NTSTATUS MiniportTopologyMSVAD::propertyHandlerCpuResources(IN  PPCPROPERTY_REQUEST propertyRequest)
{
    PAGED_CODE();

    DPF_ENTER(("[%s]",__FUNCTION__));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    if (propertyRequest->Verb & KSPROPERTY_TYPE_GET)
    {
        ntStatus = ValidatePropertyParams(propertyRequest, sizeof(ULONG));
        if (NT_SUCCESS(ntStatus))
        {
            *(PLONG(propertyRequest->Value)) = KSAUDIO_CPU_RESOURCES_NOT_HOST_CPU;
            propertyRequest->ValueSize = sizeof(LONG);
        }
    }
    else if (propertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ntStatus = PropertyHandler_BasicSupport(propertyRequest, KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT, VT_ILLEGAL);
    }

    return ntStatus;
}

//=============================================================================
/*

Routine Description:

  Handles all properties for this miniport.

Arguments:

  PropertyRequest - property request structure
*/
NTSTATUS
MiniportTopologyMSVAD::propertyHandlerGeneric(IN  PPCPROPERTY_REQUEST propertyRequest)
{
    PAGED_CODE();

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    switch (propertyRequest->PropertyItem->Id)
    {
        case KSPROPERTY_AUDIO_VOLUMELEVEL:
            ntStatus = propertyHandlerVolume(propertyRequest);
            break;
        
        case KSPROPERTY_AUDIO_CPU_RESOURCES:
            ntStatus = propertyHandlerCpuResources(propertyRequest);
            break;

        case KSPROPERTY_AUDIO_MUTE:
            ntStatus = propertyHandlerMute(propertyRequest);
            break;

        case KSPROPERTY_AUDIO_MUX_SOURCE:
            ntStatus = propertyHandlerMuxSource(propertyRequest);
            break;

        case KSPROPERTY_AUDIO_DEV_SPECIFIC:
            ntStatus = propertyHandlerDevSpecific(propertyRequest);
            break;

        default:
            DPF(D_TERSE, ("[PropertyHandlerGeneric: Invalid Device Request]"));
    }

    return ntStatus;
}

//=============================================================================
/*
Routine Description:

  Property handler for KSPROPERTY_AUDIO_MUTE

Arguments:

  PropertyRequest - property request structure
*/
NTSTATUS
MiniportTopologyMSVAD::propertyHandlerMute(IN  PPCPROPERTY_REQUEST propertyRequest)
{
    PAGED_CODE();
    DPF_ENTER(("[%s]",__FUNCTION__));

    NTSTATUS ntStatus;

    if (propertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ntStatus = PropertyHandler_BasicSupport(propertyRequest, KSPROPERTY_TYPE_ALL, VT_BOOL);
    }
    else
    {
        ntStatus = ValidatePropertyParams(propertyRequest, sizeof(BOOL), sizeof(LONG));
        if (NT_SUCCESS(ntStatus))
        {
            // If the channel index is needed, it is supplied in the Instance parameter
            // LONG lChannel = * PLONG (PropertyRequest->Instance);
            //
            PBOOL pfMute = PBOOL (propertyRequest->Value);

            if (propertyRequest->Verb & KSPROPERTY_TYPE_GET)
            {
                *pfMute = adapterCommon_->mixerMuteRead(propertyRequest->Node);
                propertyRequest->ValueSize = sizeof(BOOL);
            }
            else if (propertyRequest->Verb & KSPROPERTY_TYPE_SET)
            {
                adapterCommon_->mixerMuteWrite(propertyRequest->Node, *pfMute);
            }
        }
        else
        {
            DPF(D_TERSE, ("[%s - ntStatus=0x%08x]",__FUNCTION__,ntStatus));
        }
    }

    return ntStatus;
}

//=============================================================================
/*
Routine Description:

  PropertyHandler for KSPROPERTY_AUDIO_MUX_SOURCE.

Arguments:

  PropertyRequest - property request structure
*/
NTSTATUS
MiniportTopologyMSVAD::propertyHandlerMuxSource(IN  PPCPROPERTY_REQUEST propertyRequest)
{
    PAGED_CODE();
    DPF_ENTER(("[%s]",__FUNCTION__));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    //
    // Validate node
    // This property is only valid for WAVEIN_MUX node.
    //
    // TODO if (WAVEIN_MUX == PropertyRequest->Node)
    {
        if (propertyRequest->ValueSize >= sizeof(ULONG))
        {
            PULONG pulMuxValue = PULONG(propertyRequest->Value);
            
            if (propertyRequest->Verb & KSPROPERTY_TYPE_GET)
            {
                *pulMuxValue = adapterCommon_->mixerMuxRead();
                propertyRequest->ValueSize = sizeof(ULONG);
                ntStatus = STATUS_SUCCESS;
            }
            else if (propertyRequest->Verb & KSPROPERTY_TYPE_SET)
            {
                adapterCommon_->mixerMuxWrite(*pulMuxValue);
                ntStatus = STATUS_SUCCESS;
            }
            else if (propertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
            {
                ntStatus = PropertyHandler_BasicSupport(propertyRequest, KSPROPERTY_TYPE_ALL, VT_I4);
            }
        }
        else
        {
            DPF(D_TERSE, ("[PropertyHandlerMuxSource - Invalid parameter]"));
            ntStatus = STATUS_INVALID_PARAMETER;
        }
    }

    return ntStatus;
}

//=============================================================================
/*
Routine Description:
  Property handler for KSPROPERTY_AUDIO_VOLUMELEVEL
Arguments:
  PropertyRequest - property request structure
*/
NTSTATUS MiniportTopologyMSVAD::propertyHandlerVolume(IN  PPCPROPERTY_REQUEST propertyRequest)
{
    PAGED_CODE();

    DPF_ENTER(("[%s]",__FUNCTION__));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    if (propertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ntStatus = propertyHandlerBasicSupportVolume(propertyRequest);
    }
    else
    {
        ntStatus = ValidatePropertyParams(propertyRequest, sizeof(ULONG) /* volume value is a ULONG */, sizeof(LONG) /* instance is the channel number */);
        if (NT_SUCCESS(ntStatus))
        {
            const LONG   channel = *(PLONG (propertyRequest->Instance));
            const PULONG volume  =  PULONG (propertyRequest->Value);

            if (propertyRequest->Verb & KSPROPERTY_TYPE_GET)
            {
                *volume = adapterCommon_->mixerVolumeRead(propertyRequest->Node, channel);
                propertyRequest->ValueSize = sizeof(ULONG);                
            }
            else if (propertyRequest->Verb & KSPROPERTY_TYPE_SET)
            {
                adapterCommon_->mixerVolumeWrite(propertyRequest->Node, channel, *volume);
            }
        }
        else
        {
            DPF(D_TERSE, ("[%s - ntStatus=0x%08x]",__FUNCTION__,ntStatus));
        }
    }

    return ntStatus;
}

//=============================================================================
/*
Routine Description:
  Property handler for KSPROPERTY_AUDIO_DEV_SPECIFIC

Arguments:
  PropertyRequest - property request structure
*/
NTSTATUS
MiniportTopologyMSVAD::propertyHandlerDevSpecific(IN  PPCPROPERTY_REQUEST propertyRequest)
{
    PAGED_CODE();
    DPF_ENTER(("[%s]",__FUNCTION__));

    NTSTATUS ntStatus = STATUS_SUCCESS;

    if (propertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        if ( DEV_SPECIFIC_VT_BOOL == propertyRequest->Node )
        {
            ntStatus = PropertyHandler_BasicSupport(propertyRequest,KSPROPERTY_TYPE_ALL,VT_BOOL);
        }
        else
        {
            ULONG expectedSize = sizeof( KSPROPERTY_DESCRIPTION ) + 
                                 sizeof( KSPROPERTY_MEMBERSHEADER ) + 
                                 sizeof( KSPROPERTY_BOUNDS_LONG );
            DWORD propTypeSetId;

            if( DEV_SPECIFIC_VT_I4 == propertyRequest->Node )
            {
                propTypeSetId = VT_I4;
            }
            else if ( DEV_SPECIFIC_VT_UI4 == propertyRequest->Node )
            {
                propTypeSetId = VT_UI4;
            }
            else
            {
                propTypeSetId = VT_ILLEGAL;
                ntStatus = STATUS_INVALID_PARAMETER;
            }

            if( NT_SUCCESS(ntStatus))
            {
                if ( !propertyRequest->ValueSize )
                {
                    propertyRequest->ValueSize = expectedSize;
                    ntStatus = STATUS_BUFFER_OVERFLOW;
                } 
                else if (propertyRequest->ValueSize >= sizeof(KSPROPERTY_DESCRIPTION))
                {
                    // if return buffer can hold a KSPROPERTY_DESCRIPTION, return it
                    //
                    PKSPROPERTY_DESCRIPTION propDesc = PKSPROPERTY_DESCRIPTION(propertyRequest->Value);

                    propDesc->AccessFlags       = KSPROPERTY_TYPE_ALL;
                    propDesc->DescriptionSize   = expectedSize;
                    propDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
                    propDesc->PropTypeSet.Id    = propTypeSetId;
                    propDesc->PropTypeSet.Flags = 0;
                    propDesc->MembersListCount  = 0;
                    propDesc->Reserved          = 0;

                    if ( propertyRequest->ValueSize >= expectedSize )
                    {
                        // Extra information to return
                        propDesc->MembersListCount  = 1;

                        PKSPROPERTY_MEMBERSHEADER membersHeader = ( PKSPROPERTY_MEMBERSHEADER )( propDesc + 1);

                        membersHeader->MembersFlags = KSPROPERTY_MEMBER_RANGES;
                        membersHeader->MembersCount = 1;
                        membersHeader->MembersSize  = sizeof( KSPROPERTY_BOUNDS_LONG );
                        membersHeader->Flags        = 0;

                        PKSPROPERTY_BOUNDS_LONG PeakMeterBounds = (PKSPROPERTY_BOUNDS_LONG)( membersHeader + 1);
                        if(VT_I4 == propTypeSetId )
                        {
                            PeakMeterBounds->SignedMinimum = 0;
                            PeakMeterBounds->SignedMaximum = 0x7fffffff;
                        }
                        else
                        {
                            PeakMeterBounds->UnsignedMinimum = 0;
                            PeakMeterBounds->UnsignedMaximum = 0xffffffff;
                        }

                        // set the return value size
                        propertyRequest->ValueSize = expectedSize;
                    }
                    else
                    {
                        // No extra information to return.
                        propertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
                    }

                    ntStatus = STATUS_SUCCESS;
                } 
                else if (propertyRequest->ValueSize >= sizeof(ULONG))
                {
                    // if return buffer can hold a ULONG, return the access flags
                    //
                    *(PULONG(propertyRequest->Value)) = KSPROPERTY_TYPE_ALL;

                    propertyRequest->ValueSize = sizeof(ULONG);
                    ntStatus = STATUS_SUCCESS;                    
                }
                else
                {
                    propertyRequest->ValueSize = 0;
                    ntStatus = STATUS_BUFFER_TOO_SMALL;
                }
            }
        }
    }
    else
    {
        // switch on node id
        switch( propertyRequest->Node )
        {
        case DEV_SPECIFIC_VT_BOOL:
            {
                PBOOL bDevSpecific;

                ntStatus = ValidatePropertyParams(propertyRequest, sizeof(BOOL), 0);

                if (NT_SUCCESS(ntStatus))
                {
                    bDevSpecific   = PBOOL (propertyRequest->Value);

                    if (propertyRequest->Verb & KSPROPERTY_TYPE_GET)
                    {
                        *bDevSpecific = adapterCommon_->bDevSpecificRead();
                        propertyRequest->ValueSize = sizeof(BOOL);
                    }
                    else if (propertyRequest->Verb & KSPROPERTY_TYPE_SET)
                    {
                        adapterCommon_->bDevSpecificWrite(*bDevSpecific);
                    }
                    else
                    {
                        ntStatus = STATUS_INVALID_PARAMETER;
                    }
                }
            }
            break;
        case DEV_SPECIFIC_VT_I4:
            {
                ntStatus = ValidatePropertyParams(propertyRequest, sizeof(int), 0);

                if (NT_SUCCESS(ntStatus))
                {
                    INT* iDevSpecific = PINT(propertyRequest->Value);

                    if (propertyRequest->Verb & KSPROPERTY_TYPE_GET)
                    {
                        *iDevSpecific = adapterCommon_->iDevSpecificRead();
                        propertyRequest->ValueSize = sizeof(int);
                    }
                    else if (propertyRequest->Verb & KSPROPERTY_TYPE_SET)
                    {
                        adapterCommon_->iDevSpecificWrite(*iDevSpecific);
                    }
                    else
                    {
                        ntStatus = STATUS_INVALID_PARAMETER;
                    }
                }
            }
            break;
        case DEV_SPECIFIC_VT_UI4:
            {
                ntStatus = ValidatePropertyParams(propertyRequest, sizeof(UINT), 0);

                if (NT_SUCCESS(ntStatus))
                {
                    UINT* uiDevSpecific = PUINT(propertyRequest->Value);

                    if (propertyRequest->Verb & KSPROPERTY_TYPE_GET)
                    {
                        *uiDevSpecific = adapterCommon_->uiDevSpecificRead();
                        propertyRequest->ValueSize = sizeof(UINT);
                    }
                    else if (propertyRequest->Verb & KSPROPERTY_TYPE_SET)
                    {
                        adapterCommon_->uiDevSpecificWrite(*uiDevSpecific);
                    }
                    else
                    {
                        ntStatus = STATUS_INVALID_PARAMETER;
                    }
                }
            }
            break;
        default:
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }

        if( !NT_SUCCESS(ntStatus))
        {
            DPF(D_TERSE, ("[%s - ntStatus=0x%08x]",__FUNCTION__,ntStatus));
        }
    }

    return ntStatus;
}

#pragma code_seg()