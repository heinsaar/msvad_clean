/*++

Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:

    Common.h

Abstract:
    
    CAdapterCommon class declaration.

--*/

#ifndef _MSVAD_COMMON_H_
#define _MSVAD_COMMON_H_

//=============================================================================
// Defines
//=============================================================================

DEFINE_GUID(IID_IAdapterCommon,
0x7eda2950, 0xbf9f, 0x11d0, 0x87, 0x1f, 0x0, 0xa0, 0xc9, 0x11, 0xb5, 0x44);

//=============================================================================
// Interfaces
//=============================================================================

///////////////////////////////////////////////////////////////////////////////
// IAdapterCommon
//
DECLARE_INTERFACE_(IAdapterCommon, IUnknown)
{
    STDMETHOD_(NTSTATUS,        Init) 
    ( 
        THIS_
        IN  PDEVICE_OBJECT      DeviceObject 
    ) PURE;

    STDMETHOD_(PDEVICE_OBJECT,  getDeviceObject)
    (
        THIS
    ) PURE;

    STDMETHOD_(VOID,            setWaveServiceGroup) 
    ( 
        THIS_
        IN PSERVICEGROUP        ServiceGroup 
    ) PURE;

    STDMETHOD_(NTSTATUS,        instantiateDevices)
    (
        THIS
    ) PURE;

    STDMETHOD_(NTSTATUS,        uninstantiateDevices)
    (
        THIS
    ) PURE;

    STDMETHOD_(NTSTATUS,        plugin)
    (
        THIS
    ) PURE;

    STDMETHOD_(NTSTATUS,        unplug)
    (
        THIS
    ) PURE;

    STDMETHOD_(PUNKNOWN *,      wavePortDriverDest) 
    ( 
        THIS 
    ) PURE;

    STDMETHOD_(BOOL,            bDevSpecificRead)
    (
        THIS_
    ) PURE;

    STDMETHOD_(VOID,            bDevSpecificWrite)
    (
        THIS_
        IN  BOOL                bDevSpecific
    );

    STDMETHOD_(INT,             iDevSpecificRead)
    (
        THIS_
    ) PURE;

    STDMETHOD_(VOID,            iDevSpecificWrite)
    (
        THIS_
        IN  INT                 iDevSpecific
    );

    STDMETHOD_(UINT,            uiDevSpecificRead)
    (
        THIS_
    ) PURE;

    STDMETHOD_(VOID,            uiDevSpecificWrite)
    (
        THIS_
        IN  UINT                uiDevSpecific
    );

    STDMETHOD_(BOOL,            mixerMuteRead)
    (
        THIS_
        IN  ULONG               Index
    ) PURE;

    STDMETHOD_(VOID,            mixerMuteWrite)
    (
        THIS_
        IN  ULONG               Index,
        IN  BOOL                Value
    );

    STDMETHOD_(ULONG,           mixerMuxRead)
    (
        THIS
    );

    STDMETHOD_(VOID,            mixerMuxWrite)
    (
        THIS_
        IN  ULONG               Index
    );

    STDMETHOD_(LONG,            mixerVolumeRead) 
    ( 
        THIS_
        IN  ULONG               Index,
        IN  LONG                Channel
    ) PURE;

    STDMETHOD_(VOID,            mixerVolumeWrite) 
    ( 
        THIS_
        IN  ULONG               Index,
        IN  LONG                Channel,
        IN  LONG                Value 
    ) PURE;

    STDMETHOD_(VOID,            mixerReset) 
    ( 
        THIS 
    ) PURE;

    STDMETHOD_(BOOL,            isInstantiated) 
    ( 
        THIS 
    ) PURE;

    STDMETHOD_(BOOL,            isPluggedIn) 
    ( 
        THIS 
    ) PURE;

    STDMETHOD_(NTSTATUS,        setInstantiateWorkItem)
    (
        THIS_
        _In_ __drv_aliasesMem   PIO_WORKITEM    WorkItem
    ) PURE;

    STDMETHOD_(NTSTATUS,        freeInstantiateWorkItem)
    (
        THIS_
    ) PURE;
};
typedef IAdapterCommon *PADAPTERCOMMON;

//=============================================================================
// Function Prototypes
//=============================================================================
NTSTATUS
newAdapterCommon
( 
    OUT PUNKNOWN *              Unknown,
    IN  REFCLSID,
    IN  PUNKNOWN                UnknownOuter OPTIONAL,
    _When_((PoolType & NonPagedPoolMustSucceed) != 0,
       __drv_reportError("Must succeed pool allocations are forbidden. "
			 "Allocation failures cause a system crash"))
    IN  POOL_TYPE               PoolType 
);

#endif  //_COMMON_H_

