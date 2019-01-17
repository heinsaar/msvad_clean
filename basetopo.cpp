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

    adapterCommon_ = nullptr;
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
    _In_        ULONG        PinId,
    _In_        PKSDATARANGE ClientDataRange,
    _In_        PKSDATARANGE MyDataRange,
    _In_        ULONG        OutputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength)
    PVOID                    ResultantFormat,
    _Out_       PULONG       ResultantFormatLength
)
{
    UNREFERENCED_PARAMETER(PinId);
    UNREFERENCED_PARAMETER(ClientDataRange);
    UNREFERENCED_PARAMETER(MyDataRange);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(ResultantFormat);
    UNREFERENCED_PARAMETER(ResultantFormatLength);

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
NTSTATUS MiniportTopologyMSVAD::GetDescription(_Out_ PPCFILTER_DESCRIPTOR* OutFilterDescriptor)
{
    PAGED_CODE();
    ASSERT(OutFilterDescriptor);
    DPF_ENTER(("[%s]",__FUNCTION__));
    *OutFilterDescriptor = filterDescriptor_;
    return STATUS_SUCCESS;
}

//=============================================================================
/*
Routine Description:
  Initializes the topology miniport.

Arguments:
  Port_ - Pointer to topology port
*/
NTSTATUS MiniportTopologyMSVAD::Init(IN PUNKNOWN UnknownAdapter_, IN PPORTTOPOLOGY Port_)
{
    UNREFERENCED_PARAMETER(Port_);

    PAGED_CODE();

    ASSERT(UnknownAdapter_);
    ASSERT(Port_);

    DPF_ENTER(("[CMiniportTopologyMSVAD::Init]"));

    NTSTATUS ntStatus = UnknownAdapter_->QueryInterface(IID_IAdapterCommon, (PVOID *) &adapterCommon_);
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
NTSTATUS MiniportTopologyMSVAD::propertyHandlerBasicSupportVolume(IN  PPCPROPERTY_REQUEST     PropertyRequest)
{
    PAGED_CODE();

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG cbFullProperty = sizeof(KSPROPERTY_DESCRIPTION) +
                           sizeof(KSPROPERTY_MEMBERSHEADER) +
                           sizeof(KSPROPERTY_STEPPING_LONG);

    if (PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION)))
    {
        PKSPROPERTY_DESCRIPTION PropDesc = PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);

        PropDesc->AccessFlags       = KSPROPERTY_TYPE_ALL;
        PropDesc->DescriptionSize   = cbFullProperty;
        PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
        PropDesc->PropTypeSet.Id    = VT_I4;
        PropDesc->PropTypeSet.Flags = 0;
        PropDesc->MembersListCount  = 1;
        PropDesc->Reserved          = 0;

        // if return buffer can also hold a range description, return it too
        if(PropertyRequest->ValueSize >= cbFullProperty)
        {
            // fill in the members header
            PKSPROPERTY_MEMBERSHEADER Members = PKSPROPERTY_MEMBERSHEADER(PropDesc + 1);

            Members->MembersFlags   = KSPROPERTY_MEMBER_STEPPEDRANGES;
            Members->MembersSize    = sizeof(KSPROPERTY_STEPPING_LONG);
            Members->MembersCount   = 1;
            Members->Flags          = KSPROPERTY_MEMBER_FLAG_BASICSUPPORT_MULTICHANNEL;

            // fill in the stepped range
            PKSPROPERTY_STEPPING_LONG Range = PKSPROPERTY_STEPPING_LONG(Members + 1);

            Range->Bounds.SignedMaximum = 0x00000000;      //   0 dB
            Range->Bounds.SignedMinimum = -96 * 0x10000;   // -96 dB
            Range->SteppingDelta        = 0x08000;         //  .5 dB
            Range->Reserved             = 0;

            // set the return value size
            PropertyRequest->ValueSize = cbFullProperty;
        } 
        else
        {
            PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
        }
    } 
    else if(PropertyRequest->ValueSize >= sizeof(ULONG))
    {
        // if return buffer can hold a ULONG, return the access flags
        PULONG AccessFlags = PULONG(PropertyRequest->Value);

        PropertyRequest->ValueSize = sizeof(ULONG);
        *AccessFlags = KSPROPERTY_TYPE_ALL;
    }
    else
    {
        PropertyRequest->ValueSize = 0;
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
NTSTATUS MiniportTopologyMSVAD::propertyHandlerCpuResources(IN  PPCPROPERTY_REQUEST     PropertyRequest)
{
    PAGED_CODE();

    DPF_ENTER(("[%s]",__FUNCTION__));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
    {
        ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(ULONG));
        if (NT_SUCCESS(ntStatus))
        {
            *(PLONG(PropertyRequest->Value)) = KSAUDIO_CPU_RESOURCES_NOT_HOST_CPU;
            PropertyRequest->ValueSize = sizeof(LONG);
        }
    }
    else if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ntStatus = PropertyHandler_BasicSupport(PropertyRequest, KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT, VT_ILLEGAL);
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
MiniportTopologyMSVAD::propertyHandlerGeneric(IN  PPCPROPERTY_REQUEST     PropertyRequest)
{
    PAGED_CODE();

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    switch (PropertyRequest->PropertyItem->Id)
    {
        case KSPROPERTY_AUDIO_VOLUMELEVEL:
            ntStatus = propertyHandlerVolume(PropertyRequest);
            break;
        
        case KSPROPERTY_AUDIO_CPU_RESOURCES:
            ntStatus = propertyHandlerCpuResources(PropertyRequest);
            break;

        case KSPROPERTY_AUDIO_MUTE:
            ntStatus = propertyHandlerMute(PropertyRequest);
            break;

        case KSPROPERTY_AUDIO_MUX_SOURCE:
            ntStatus = propertyHandlerMuxSource(PropertyRequest);
            break;

        case KSPROPERTY_AUDIO_DEV_SPECIFIC:
            ntStatus = propertyHandlerDevSpecific(PropertyRequest);
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
MiniportTopologyMSVAD::propertyHandlerMute(IN  PPCPROPERTY_REQUEST     PropertyRequest)
{
    PAGED_CODE();
    DPF_ENTER(("[%s]",__FUNCTION__));

    NTSTATUS ntStatus;

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ntStatus = PropertyHandler_BasicSupport(PropertyRequest, KSPROPERTY_TYPE_ALL, VT_BOOL);
    }
    else
    {
        ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(BOOL), sizeof(LONG));
        if (NT_SUCCESS(ntStatus))
        {
            // If the channel index is needed, it is supplied in the Instance parameter
            // LONG lChannel = * PLONG (PropertyRequest->Instance);
            //
            PBOOL pfMute = PBOOL (PropertyRequest->Value);

            if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
            {
                *pfMute = adapterCommon_->mixerMuteRead(PropertyRequest->Node);
                PropertyRequest->ValueSize = sizeof(BOOL);
            }
            else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET)
            {
                adapterCommon_->mixerMuteWrite(PropertyRequest->Node, *pfMute);
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
MiniportTopologyMSVAD::propertyHandlerMuxSource(IN  PPCPROPERTY_REQUEST PropertyRequest)
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
        if (PropertyRequest->ValueSize >= sizeof(ULONG))
        {
            PULONG pulMuxValue = PULONG(PropertyRequest->Value);
            
            if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
            {
                *pulMuxValue = adapterCommon_->mixerMuxRead();
                PropertyRequest->ValueSize = sizeof(ULONG);
                ntStatus = STATUS_SUCCESS;
            }
            else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET)
            {
                adapterCommon_->mixerMuxWrite(*pulMuxValue);
                ntStatus = STATUS_SUCCESS;
            }
            else if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
            {
                ntStatus = PropertyHandler_BasicSupport(PropertyRequest, KSPROPERTY_TYPE_ALL, VT_I4);
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
NTSTATUS MiniportTopologyMSVAD::propertyHandlerVolume(IN  PPCPROPERTY_REQUEST PropertyRequest)
{
    PAGED_CODE();

    DPF_ENTER(("[%s]",__FUNCTION__));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ntStatus = propertyHandlerBasicSupportVolume(PropertyRequest);
    }
    else
    {
        ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(ULONG) /* volume value is a ULONG */, sizeof(LONG) /* instance is the channel number */);
        if (NT_SUCCESS(ntStatus))
        {
            LONG channel = *(PLONG (PropertyRequest->Instance));
            PULONG volume  =  PULONG (PropertyRequest->Value);

            if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
            {
                *volume = adapterCommon_->mixerVolumeRead(PropertyRequest->Node, channel);
                PropertyRequest->ValueSize = sizeof(ULONG);                
            }
            else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET)
            {
                adapterCommon_->mixerVolumeWrite(PropertyRequest->Node, channel, *volume);
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
MiniportTopologyMSVAD::propertyHandlerDevSpecific(IN  PPCPROPERTY_REQUEST     PropertyRequest)
{
    PAGED_CODE();
    DPF_ENTER(("[%s]",__FUNCTION__));

    NTSTATUS ntStatus=STATUS_SUCCESS;

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        if( DEV_SPECIFIC_VT_BOOL == PropertyRequest->Node )
        {
            ntStatus = PropertyHandler_BasicSupport(PropertyRequest,KSPROPERTY_TYPE_ALL,VT_BOOL);
        }
        else
        {
            ULONG ExpectedSize = sizeof( KSPROPERTY_DESCRIPTION ) + 
                                 sizeof( KSPROPERTY_MEMBERSHEADER ) + 
                                 sizeof( KSPROPERTY_BOUNDS_LONG );
            DWORD ulPropTypeSetId;

            if( DEV_SPECIFIC_VT_I4 == PropertyRequest->Node )
            {
                ulPropTypeSetId = VT_I4;
            }
            else if ( DEV_SPECIFIC_VT_UI4 == PropertyRequest->Node )
            {
                ulPropTypeSetId = VT_UI4;
            }
            else
            {
                ulPropTypeSetId = VT_ILLEGAL;
                ntStatus = STATUS_INVALID_PARAMETER;
            }

            if( NT_SUCCESS(ntStatus))
            {
                if ( !PropertyRequest->ValueSize )
                {
                    PropertyRequest->ValueSize = ExpectedSize;
                    ntStatus = STATUS_BUFFER_OVERFLOW;
                } 
                else if (PropertyRequest->ValueSize >= sizeof(KSPROPERTY_DESCRIPTION))
                {
                    // if return buffer can hold a KSPROPERTY_DESCRIPTION, return it
                    //
                    PKSPROPERTY_DESCRIPTION PropDesc = PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);

                    PropDesc->AccessFlags       = KSPROPERTY_TYPE_ALL;
                    PropDesc->DescriptionSize   = ExpectedSize;
                    PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
                    PropDesc->PropTypeSet.Id    = ulPropTypeSetId;
                    PropDesc->PropTypeSet.Flags = 0;
                    PropDesc->MembersListCount  = 0;
                    PropDesc->Reserved          = 0;

                    if ( PropertyRequest->ValueSize >= ExpectedSize )
                    {
                        // Extra information to return
                        PropDesc->MembersListCount  = 1;

                        PKSPROPERTY_MEMBERSHEADER MembersHeader = ( PKSPROPERTY_MEMBERSHEADER )( PropDesc + 1);
                        MembersHeader->MembersFlags = KSPROPERTY_MEMBER_RANGES;
                        MembersHeader->MembersCount  = 1;
                        MembersHeader->MembersSize   = sizeof( KSPROPERTY_BOUNDS_LONG );
                        MembersHeader->Flags = 0;

                        PKSPROPERTY_BOUNDS_LONG PeakMeterBounds = (PKSPROPERTY_BOUNDS_LONG)( MembersHeader + 1);
                        if(VT_I4 == ulPropTypeSetId )
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
                        PropertyRequest->ValueSize = ExpectedSize;
                    }
                    else
                    {
                        // No extra information to return.
                        PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
                    }

                    ntStatus = STATUS_SUCCESS;
                } 
                else if (PropertyRequest->ValueSize >= sizeof(ULONG))
                {
                    // if return buffer can hold a ULONG, return the access flags
                    //
                    *(PULONG(PropertyRequest->Value)) = KSPROPERTY_TYPE_ALL;

                    PropertyRequest->ValueSize = sizeof(ULONG);
                    ntStatus = STATUS_SUCCESS;                    
                }
                else
                {
                    PropertyRequest->ValueSize = 0;
                    ntStatus = STATUS_BUFFER_TOO_SMALL;
                }
            }
        }
    }
    else
    {
        // switch on node id
        switch( PropertyRequest->Node )
        {
        case DEV_SPECIFIC_VT_BOOL:
            {
                PBOOL pbDevSpecific;

                ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(BOOL), 0);

                if (NT_SUCCESS(ntStatus))
                {
                    pbDevSpecific   = PBOOL (PropertyRequest->Value);

                    if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
                    {
                        *pbDevSpecific = adapterCommon_->bDevSpecificRead();
                        PropertyRequest->ValueSize = sizeof(BOOL);
                    }
                    else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET)
                    {
                        adapterCommon_->bDevSpecificWrite(*pbDevSpecific);
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
                ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(int), 0);

                if (NT_SUCCESS(ntStatus))
                {
                    INT* devSpecific = PINT(PropertyRequest->Value);

                    if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
                    {
                        *devSpecific = adapterCommon_->iDevSpecificRead();
                        PropertyRequest->ValueSize = sizeof(int);
                    }
                    else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET)
                    {
                        adapterCommon_->iDevSpecificWrite(*devSpecific);
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
                ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(UINT), 0);

                if (NT_SUCCESS(ntStatus))
                {
                    UINT* devSpecific = PUINT(PropertyRequest->Value);

                    if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
                    {
                        *devSpecific = adapterCommon_->uiDevSpecificRead();
                        PropertyRequest->ValueSize = sizeof(UINT);
                    }
                    else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET)
                    {
                        adapterCommon_->uiDevSpecificWrite(*devSpecific);
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