/*
    ble_scan.c
    ------------------------------------------------------------
    Pure C BLE scanner for Windows
    MinGW-w64 GCC
    No C++
    No C++ STL
    No external libraries

    What it does:
        - scans BLE advertisements
        - does NOT require pairing
        - uses active scanning
        - prints:
            * Bluetooth address
            * RSSI
            * advertisement type
            * local name
            * service UUIDs
            * raw advertisement data sections

    Build:

        gcc -std=c11 -Wall -Wextra -O2 ble_scan.c ^
            -o ble_scan.exe -lole32 -lwindowsapp

    If -lwindowsapp is not needed on your MinGW:
        gcc -std=c11 -Wall -Wextra -O2 ble_scan.c ^
            -o ble_scan.exe -lole32

    Ctrl+C stops the scanner.
*/

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <roapi.h>
#include <winstring.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* ============================================================
   Helpers
   ============================================================ */

static volatile LONG g_running = 1;

static void print_hr(const char *where, HRESULT hr)
{
    fprintf(
        stderr,
        "%s failed: HRESULT = 0x%08lX\n",
        where,
        (unsigned long)hr);
}

static BOOL WINAPI console_handler(DWORD signal)
{
    switch (signal)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        InterlockedExchange(&g_running, 0);
        return TRUE;

    default:
        return FALSE;
    }
}

static void print_hex(
    const uint8_t *data,
    UINT32 size)
{
    if (!data || size == 0)
    {
        printf("<empty>");
        return;
    }

    for (UINT32 i = 0; i < size; ++i)
    {
        printf("%02X", data[i]);

        if (i + 1 < size)
            putchar(' ');
    }
}

static void print_address(UINT64 address)
{
    printf(
        "%02llX:%02llX:%02llX:%02llX:%02llX:%02llX",
        (unsigned long long)((address >> 40) & 0xFF),
        (unsigned long long)((address >> 32) & 0xFF),
        (unsigned long long)((address >> 24) & 0xFF),
        (unsigned long long)((address >> 16) & 0xFF),
        (unsigned long long)((address >> 8) & 0xFF),
        (unsigned long long)(address & 0xFF));
}

static const char *advertisement_type_name(INT32 type)
{
    switch (type)
    {
    case 0:
        return "ConnectableUndirected";
    case 1:
        return "ConnectableDirected";
    case 2:
        return "ScannableUndirected";
    case 3:
        return "NonConnectableUndirected";
    case 4:
        return "ScanResponse";
    case 5:
        return "Extended";
    default:
        return "Unknown";
    }
}

/* ============================================================
   COM / WinRT base interfaces
   ============================================================ */

typedef HRESULT(STDMETHODCALLTYPE *QueryInterfaceFn)(
    void *self,
    REFIID riid,
    void **object);

typedef struct
{
    HRESULT(STDMETHODCALLTYPE *QueryInterface)(
        void *self,
        REFIID riid,
        void **object);

    ULONG(STDMETHODCALLTYPE *AddRef)(
        void *self);

    ULONG(STDMETHODCALLTYPE *Release)(
        void *self);

} IUnknownVtbl_C;

/*
    IInspectable:

        IUnknown
        GetIids
        GetRuntimeClassName
        GetTrustLevel
*/
typedef struct
{
    QueryInterfaceFn QueryInterface;
    ULONG(STDMETHODCALLTYPE *AddRef)(void *self);
    ULONG(STDMETHODCALLTYPE *Release)(void *self);

    HRESULT(STDMETHODCALLTYPE *GetIids)(
        void *self,
        ULONG *count,
        IID **iids);

    HRESULT(STDMETHODCALLTYPE *GetRuntimeClassName)(
        void *self,
        HSTRING *name);

    HRESULT(STDMETHODCALLTYPE *GetTrustLevel)(
        void *self,
        int *level);

} IInspectableVtbl_C;

typedef struct
{
    IInspectableVtbl_C *lpVtbl;

} IInspectable_C;

/* ============================================================
   GUIDs
   ============================================================ */

/*
    IBluetoothLEAdvertisementWatcher
*/
static const GUID IID_IBluetoothLEAdvertisementWatcher =
    {
        0xA6AC336F,
        0xF3D3,
        0x4297,
        {0x8D, 0x6C, 0xC8, 0x1E, 0xA6, 0x62, 0x3F, 0x40}};

/*
    IBluetoothLEAdvertisementReceivedEventArgs
*/
static const GUID IID_IBluetoothLEAdvertisementReceivedEventArgs =
    {
        0x27987DDF,
        0xE596,
        0x41BE,
        {0x8D, 0x43, 0x9E, 0x67, 0x31, 0xD4, 0xA9, 0x13}};

/*
    TypedEventHandler<
        BluetoothLEAdvertisementWatcher*,
        BluetoothLEAdvertisementReceivedEventArgs*
    >
*/
static const GUID IID_ReceivedHandler =
    {
        0x90EB4ECA,
        0xD465,
        0x5EA0,
        {0xA6, 0x1C, 0x03, 0x3C, 0x8C, 0x5E, 0xCE, 0xF2}};

/*
    IBufferByteAccess
*/
static const GUID IID_IBufferByteAccess =
    {
        0x905A0FEF,
        0xBC53,
        0x11DF,
        {0x8C, 0x49, 0x00, 0x1E, 0x4F, 0xC6, 0x86, 0xDA}};

/*
    Runtime class:
    Windows.Devices.Bluetooth.Advertisement.BluetoothLEAdvertisementWatcher
*/
static const wchar_t WATCHER_CLASS_NAME[] =
    L"Windows.Devices.Bluetooth.Advertisement.BluetoothLEAdvertisementWatcher";

/* ============================================================
   EventRegistrationToken
   ============================================================ */

typedef struct
{
    INT64 value;

} EventRegistrationToken_C;

/* ============================================================
   BluetoothLEAdvertisementWatcher
   ============================================================ */

typedef struct BLEWatcher BLEWatcher;

typedef HRESULT(STDMETHODCALLTYPE *WatcherGetTimeSpan)(
    BLEWatcher *self,
    INT64 *value);

typedef HRESULT(STDMETHODCALLTYPE *WatcherGetStatus)(
    BLEWatcher *self,
    INT32 *value);

typedef HRESULT(STDMETHODCALLTYPE *WatcherGetScanningMode)(
    BLEWatcher *self,
    INT32 *value);

typedef HRESULT(STDMETHODCALLTYPE *WatcherSetScanningMode)(
    BLEWatcher *self,
    INT32 value);

typedef HRESULT(STDMETHODCALLTYPE *WatcherGetObject)(
    BLEWatcher *self,
    void **value);

typedef HRESULT(STDMETHODCALLTYPE *WatcherSetObject)(
    BLEWatcher *self,
    void *value);

typedef HRESULT(STDMETHODCALLTYPE *WatcherAddEvent)(
    BLEWatcher *self,
    void *handler,
    EventRegistrationToken_C *token);

typedef HRESULT(STDMETHODCALLTYPE *WatcherRemoveEvent)(
    BLEWatcher *self,
    EventRegistrationToken_C token);

typedef struct
{
    /* IUnknown */
    HRESULT(STDMETHODCALLTYPE *QueryInterface)(
        BLEWatcher *self,
        REFIID riid,
        void **object);

    ULONG(STDMETHODCALLTYPE *AddRef)(
        BLEWatcher *self);

    ULONG(STDMETHODCALLTYPE *Release)(
        BLEWatcher *self);

    /* IInspectable */

    HRESULT(STDMETHODCALLTYPE *GetIids)(
        BLEWatcher *self,
        ULONG *count,
        IID **iids);

    HRESULT(STDMETHODCALLTYPE *GetRuntimeClassName)(
        BLEWatcher *self,
        HSTRING *name);

    HRESULT(STDMETHODCALLTYPE *GetTrustLevel)(
        BLEWatcher *self,
        int *level);

    /* IBluetoothLEAdvertisementWatcher */

    WatcherGetTimeSpan GetMinSamplingInterval;
    WatcherGetTimeSpan GetMaxSamplingInterval;
    WatcherGetTimeSpan GetMinOutOfRangeTimeout;
    WatcherGetTimeSpan GetMaxOutOfRangeTimeout;

    WatcherGetStatus GetStatus;

    WatcherGetScanningMode GetScanningMode;
    WatcherSetScanningMode SetScanningMode;

    WatcherGetObject GetSignalStrengthFilter;
    WatcherSetObject SetSignalStrengthFilter;

    WatcherGetObject GetAdvertisementFilter;
    WatcherSetObject SetAdvertisementFilter;

    HRESULT(STDMETHODCALLTYPE *Start)(
        BLEWatcher *self);

    HRESULT(STDMETHODCALLTYPE *Stop)(
        BLEWatcher *self);

    WatcherAddEvent AddReceived;
    WatcherRemoveEvent RemoveReceived;

    WatcherAddEvent AddStopped;
    WatcherRemoveEvent RemoveStopped;

} BLEWatcherVtbl;

struct BLEWatcher
{
    BLEWatcherVtbl *lpVtbl;
};

/* ============================================================
   BluetoothLEAdvertisementReceivedEventArgs
   ============================================================ */

typedef struct BLEReceivedArgs BLEReceivedArgs;

typedef struct
{
    /* IUnknown */
    HRESULT(STDMETHODCALLTYPE *QueryInterface)(
        BLEReceivedArgs *self,
        REFIID riid,
        void **object);

    ULONG(STDMETHODCALLTYPE *AddRef)(
        BLEReceivedArgs *self);

    ULONG(STDMETHODCALLTYPE *Release)(
        BLEReceivedArgs *self);

    /* IInspectable */

    HRESULT(STDMETHODCALLTYPE *GetIids)(
        BLEReceivedArgs *self,
        ULONG *count,
        IID **iids);

    HRESULT(STDMETHODCALLTYPE *GetRuntimeClassName)(
        BLEReceivedArgs *self,
        HSTRING *name);

    HRESULT(STDMETHODCALLTYPE *GetTrustLevel)(
        BLEReceivedArgs *self,
        int *level);

    /* Event args */

    HRESULT(STDMETHODCALLTYPE *GetRawSignalStrength)(
        BLEReceivedArgs *self,
        INT16 *value);

    HRESULT(STDMETHODCALLTYPE *GetBluetoothAddress)(
        BLEReceivedArgs *self,
        UINT64 *value);

    HRESULT(STDMETHODCALLTYPE *GetAdvertisementType)(
        BLEReceivedArgs *self,
        INT32 *value);

    HRESULT(STDMETHODCALLTYPE *GetTimestamp)(
        BLEReceivedArgs *self,
        INT64 *value);

    HRESULT(STDMETHODCALLTYPE *GetAdvertisement)(
        BLEReceivedArgs *self,
        void **value);

} BLEReceivedArgsVtbl;

struct BLEReceivedArgs
{
    BLEReceivedArgsVtbl *lpVtbl;
};

/* ============================================================
   BluetoothLEAdvertisement
   ============================================================ */

typedef struct BLEAdvertisement BLEAdvertisement;

typedef struct
{
    /* IUnknown */
    HRESULT(STDMETHODCALLTYPE *QueryInterface)(
        BLEAdvertisement *self,
        REFIID riid,
        void **object);

    ULONG(STDMETHODCALLTYPE *AddRef)(
        BLEAdvertisement *self);

    ULONG(STDMETHODCALLTYPE *Release)(
        BLEAdvertisement *self);

    /* IInspectable */

    HRESULT(STDMETHODCALLTYPE *GetIids)(
        BLEAdvertisement *self,
        ULONG *count,
        IID **iids);

    HRESULT(STDMETHODCALLTYPE *GetRuntimeClassName)(
        BLEAdvertisement *self,
        HSTRING *name);

    HRESULT(STDMETHODCALLTYPE *GetTrustLevel)(
        BLEAdvertisement *self,
        int *level);

    /* IBluetoothLEAdvertisement */

    HRESULT(STDMETHODCALLTYPE *GetFlags)(
        BLEAdvertisement *self,
        void **value);

    HRESULT(STDMETHODCALLTYPE *SetFlags)(
        BLEAdvertisement *self,
        void *value);

    HRESULT(STDMETHODCALLTYPE *GetLocalName)(
        BLEAdvertisement *self,
        HSTRING *value);

    HRESULT(STDMETHODCALLTYPE *SetLocalName)(
        BLEAdvertisement *self,
        HSTRING value);

    HRESULT(STDMETHODCALLTYPE *GetServiceUuids)(
        BLEAdvertisement *self,
        void **value);

    HRESULT(STDMETHODCALLTYPE *GetManufacturerData)(
        BLEAdvertisement *self,
        void **value);

    HRESULT(STDMETHODCALLTYPE *GetDataSections)(
        BLEAdvertisement *self,
        void **value);

    HRESULT(STDMETHODCALLTYPE *GetManufacturerDataByCompanyId)(
        BLEAdvertisement *self,
        UINT16 company_id,
        void **value);

    HRESULT(STDMETHODCALLTYPE *GetSectionsByType)(
        BLEAdvertisement *self,
        BYTE type,
        void **value);

} BLEAdvertisementVtbl;

struct BLEAdvertisement
{
    BLEAdvertisementVtbl *lpVtbl;
};

/* ============================================================
   IVector<T>
   ============================================================ */

typedef struct IVector_C IVector_C;

typedef struct
{
    HRESULT(STDMETHODCALLTYPE *QueryInterface)(
        IVector_C *self,
        REFIID riid,
        void **object);

    ULONG(STDMETHODCALLTYPE *AddRef)(
        IVector_C *self);

    ULONG(STDMETHODCALLTYPE *Release)(
        IVector_C *self);

    HRESULT(STDMETHODCALLTYPE *GetIids)(
        IVector_C *self,
        ULONG *count,
        IID **iids);

    HRESULT(STDMETHODCALLTYPE *GetRuntimeClassName)(
        IVector_C *self,
        HSTRING *name);

    HRESULT(STDMETHODCALLTYPE *GetTrustLevel)(
        IVector_C *self,
        int *level);

    /*
        IVector<T>
    */

    HRESULT(STDMETHODCALLTYPE *GetAt)(
        IVector_C *self,
        UINT32 index,
        void **value);

    HRESULT(STDMETHODCALLTYPE *GetSize)(
        IVector_C *self,
        UINT32 *value);

} IVectorVtbl_C;

struct IVector_C
{
    IVectorVtbl_C *lpVtbl;
};

/* ============================================================
   BluetoothLEAdvertisementDataSection
   ============================================================ */

typedef struct BLEDataSection BLEDataSection;

typedef struct
{
    /* IUnknown */

    HRESULT(STDMETHODCALLTYPE *QueryInterface)(
        BLEDataSection *self,
        REFIID riid,
        void **object);

    ULONG(STDMETHODCALLTYPE *AddRef)(
        BLEDataSection *self);

    ULONG(STDMETHODCALLTYPE *Release)(
        BLEDataSection *self);

    /* IInspectable */

    HRESULT(STDMETHODCALLTYPE *GetIids)(
        BLEDataSection *self,
        ULONG *count,
        IID **iids);

    HRESULT(STDMETHODCALLTYPE *GetRuntimeClassName)(
        BLEDataSection *self,
        HSTRING *name);

    HRESULT(STDMETHODCALLTYPE *GetTrustLevel)(
        BLEDataSection *self,
        int *level);

    /* DataSection */

    HRESULT(STDMETHODCALLTYPE *GetDataType)(
        BLEDataSection *self,
        BYTE *value);

    HRESULT(STDMETHODCALLTYPE *SetDataType)(
        BLEDataSection *self,
        BYTE value);

    HRESULT(STDMETHODCALLTYPE *GetData)(
        BLEDataSection *self,
        void **value);

    HRESULT(STDMETHODCALLTYPE *SetData)(
        BLEDataSection *self,
        void *value);

} BLEDataSectionVtbl;

struct BLEDataSection
{
    BLEDataSectionVtbl *lpVtbl;
};

/* ============================================================
   IBuffer
   ============================================================ */

typedef struct IBuffer_C IBuffer_C;

typedef struct
{
    /* IUnknown */

    HRESULT(STDMETHODCALLTYPE *QueryInterface)(
        IBuffer_C *self,
        REFIID riid,
        void **object);

    ULONG(STDMETHODCALLTYPE *AddRef)(
        IBuffer_C *self);

    ULONG(STDMETHODCALLTYPE *Release)(
        IBuffer_C *self);

    /* IInspectable */

    HRESULT(STDMETHODCALLTYPE *GetIids)(
        IBuffer_C *self,
        ULONG *count,
        IID **iids);

    HRESULT(STDMETHODCALLTYPE *GetRuntimeClassName)(
        IBuffer_C *self,
        HSTRING *name);

    HRESULT(STDMETHODCALLTYPE *GetTrustLevel)(
        IBuffer_C *self,
        int *level);

    /* IBuffer */

    HRESULT(STDMETHODCALLTYPE *GetCapacity)(
        IBuffer_C *self,
        UINT32 *value);

    HRESULT(STDMETHODCALLTYPE *GetLength)(
        IBuffer_C *self,
        UINT32 *value);

    HRESULT(STDMETHODCALLTYPE *SetCapacity)(
        IBuffer_C *self,
        UINT32 value);

    HRESULT(STDMETHODCALLTYPE *SetLength)(
        IBuffer_C *self,
        UINT32 value);

} IBufferVtbl_C;

struct IBuffer_C
{
    IBufferVtbl_C *lpVtbl;
};

/* ============================================================
   IBufferByteAccess
   ============================================================ */

typedef struct IBufferByteAccess_C IBufferByteAccess_C;

typedef struct
{
    HRESULT(STDMETHODCALLTYPE *QueryInterface)(
        IBufferByteAccess_C *self,
        REFIID riid,
        void **object);

    ULONG(STDMETHODCALLTYPE *AddRef)(
        IBufferByteAccess_C *self);

    ULONG(STDMETHODCALLTYPE *Release)(
        IBufferByteAccess_C *self);

    HRESULT(STDMETHODCALLTYPE *Buffer)(
        IBufferByteAccess_C *self,
        BYTE **value);

} IBufferByteAccessVtbl_C;

struct IBufferByteAccess_C
{
    IBufferByteAccessVtbl_C *lpVtbl;
};

/* ============================================================
   BLE event handler
   ============================================================ */

typedef struct BLEEventHandler BLEEventHandler;

typedef struct
{
    HRESULT(STDMETHODCALLTYPE *QueryInterface)(
        BLEEventHandler *self,
        REFIID riid,
        void **object);

    ULONG(STDMETHODCALLTYPE *AddRef)(
        BLEEventHandler *self);

    ULONG(STDMETHODCALLTYPE *Release)(
        BLEEventHandler *self);

    HRESULT(STDMETHODCALLTYPE *Invoke)(
        BLEEventHandler *self,
        void *sender,
        void *args);

} BLEEventHandlerVtbl;

struct BLEEventHandler
{
    BLEEventHandlerVtbl *lpVtbl;
    LONG refs;
};

/* ============================================================
   Read IBuffer bytes
   ============================================================ */

static int read_buffer(
    IBuffer_C *buffer,
    BYTE **bytes,
    UINT32 *length)
{
    if (!buffer || !bytes || !length)
        return 0;

    *bytes = NULL;
    *length = 0;

    HRESULT hr;

    UINT32 capacity = 0;
    UINT32 len = 0;

    hr = buffer->lpVtbl->GetCapacity(
        buffer,
        &capacity);

    if (FAILED(hr))
    {
        print_hr("IBuffer.get_Capacity", hr);
        return 0;
    }

    hr = buffer->lpVtbl->GetLength(
        buffer,
        &len);

    if (FAILED(hr))
    {
        print_hr("IBuffer.get_Length", hr);
        return 0;
    }

    if (len > capacity)
    {
        fprintf(
            stderr,
            "IBuffer has invalid length %lu (capacity %lu)\n",
            (unsigned long)len,
            (unsigned long)capacity);
        return 0;
    }

    IBufferByteAccess_C *access = NULL;

    hr = buffer->lpVtbl->QueryInterface(
        buffer,
        &IID_IBufferByteAccess,
        (void **)&access);

    if (FAILED(hr))
    {
        print_hr("IBuffer.QueryInterface(IBufferByteAccess)", hr);
        return 0;
    }

    BYTE *data = NULL;

    hr = access->lpVtbl->Buffer(
        access,
        &data);

    access->lpVtbl->Release(access);

    if (FAILED(hr))
    {
        print_hr("IBufferByteAccess.Buffer", hr);
        return 0;
    }

    *bytes = data;
    *length = len;

    return 1;
}

/* ============================================================
   Print advertisement data sections
   ============================================================ */

static void print_data_sections(
    BLEAdvertisement *advertisement)
{
    if (!advertisement)
        return;

    IVector_C *vector = NULL;

    HRESULT hr =
        advertisement->lpVtbl->GetDataSections(
            advertisement,
            (void **)&vector);

    if (FAILED(hr) || !vector)
    {
        printf("DataSections : <unavailable>\n");
        return;
    }

    UINT32 count = 0;

    hr = vector->lpVtbl->GetSize(
        vector,
        &count);

    if (FAILED(hr))
    {
        printf("DataSections : <size unavailable>\n");

        vector->lpVtbl->Release(vector);
        return;
    }

    printf("DataSections : %lu\n",
           (unsigned long)count);

    for (UINT32 i = 0; i < count; ++i)
    {
        BLEDataSection *section = NULL;

        hr = vector->lpVtbl->GetAt(
            vector,
            i,
            (void **)&section);

        if (FAILED(hr) || !section)
            continue;

        BYTE type = 0;

        hr = section->lpVtbl->GetDataType(
            section,
            &type);

        if (FAILED(hr))
            print_hr("DataSection.get_DataType", hr);

        printf("  [%lu] Type 0x%02X",
               (unsigned long)i,
               (unsigned)type);

         printf("\n");

        void *buffer_object = NULL;

        hr = section->lpVtbl->GetData(
            section,
            &buffer_object);

        if (SUCCEEDED(hr) && buffer_object)
        {
            IBuffer_C *buffer =
                (IBuffer_C *)buffer_object;

            BYTE *bytes = NULL;
            UINT32 length = 0;

            if (read_buffer(
                    buffer,
                    &bytes,
                    &length))
            {
                printf(
                    "  Length %lu\n",
                    (unsigned long)length);

                printf("      ");

                print_hex(
                    bytes,
                    length);

                printf("\n");
            }
            else
            {
                printf("  <buffer read failed>\n");
            }

            buffer->lpVtbl->Release(buffer);
        }
        else
        {
            if (FAILED(hr))
                print_hr("DataSection.get_Data", hr);

            printf("  <no data>\n");
        }

        section->lpVtbl->Release(section);
    }

    vector->lpVtbl->Release(vector);
}

/* ============================================================
   Print service UUIDs
   ============================================================ */

static void print_service_uuids(
    BLEAdvertisement *advertisement)
{
    if (!advertisement)
        return;

    IVector_C *vector = NULL;

    HRESULT hr =
        advertisement->lpVtbl->GetServiceUuids(
            advertisement,
            (void **)&vector);

    if (FAILED(hr) || !vector)
    {
        printf("Service UUIDs: <none>\n");
        return;
    }

    UINT32 count = 0;

    hr = vector->lpVtbl->GetSize(
        vector,
        &count);

    if (FAILED(hr))
    {
        printf("Service UUIDs: <size unavailable>\n");

        vector->lpVtbl->Release(vector);
        return;
    }

    if (count == 0)
    {
        printf("Service UUIDs: <none>\n");

        vector->lpVtbl->Release(vector);
        return;
    }

    printf("Service UUIDs: %lu\n",
           (unsigned long)count);

    for (UINT32 i = 0; i < count; ++i)
    {
        /*
            IVector<GUID>::GetAt returns GUID by value
            through an out parameter.
        */
        GUID guid;

        memset(
            &guid,
            0,
            sizeof(guid));

        hr = vector->lpVtbl->GetAt(
            vector,
            i,
            (void **)&guid);

        if (SUCCEEDED(hr))
        {
            printf(
                "  %08lX-%04X-%04X-%02X%02X-"
                "%02X%02X%02X%02X%02X%02X\n",
                (unsigned long)guid.Data1,
                (unsigned)guid.Data2,
                (unsigned)guid.Data3,
                guid.Data4[0],
                guid.Data4[1],
                guid.Data4[2],
                guid.Data4[3],
                guid.Data4[4],
                guid.Data4[5],
                guid.Data4[6],
                guid.Data4[7]);
        }
    }

    vector->lpVtbl->Release(vector);
}

/* ============================================================
   Print BluetoothLEAdvertisement
   ============================================================ */

static void print_advertisement(
    BLEAdvertisement *advertisement)
{
    if (!advertisement)
        return;

    /*
        Local name
    */

    HSTRING local_name = NULL;

    HRESULT hr =
        advertisement->lpVtbl->GetLocalName(
            advertisement,
            &local_name);

    if (SUCCEEDED(hr) && local_name)
    {
        UINT32 length = 0;

        const wchar_t *text =
            WindowsGetStringRawBuffer(
                local_name,
                &length);

        printf("Local Name  : ");

        if (text && length > 0)
        {
            /*
                Keep console output safe even if the
                BLE name contains non-ASCII UTF-16.
            */
            for (UINT32 i = 0; i < length; ++i)
            {
                wchar_t ch = text[i];

                if (ch >= 32 && ch < 127)
                    putchar((char)ch);
                else
                    putchar('?');
            }

            putchar('\n');
        }
        else
        {
            printf("<empty>\n");
        }

        WindowsDeleteString(local_name);
    }
    else
    {
        printf("Local Name  : <none>\n");
    }

    /*
        Service UUIDs
    */

    print_service_uuids(advertisement);

    /*
        Raw data sections
    */

    print_data_sections(advertisement);
}

/* ============================================================
   Event handler implementation
   ============================================================ */

static HRESULT STDMETHODCALLTYPE handler_QueryInterface(
    BLEEventHandler *self,
    REFIID riid,
    void **object)
{
    if (!object)
        return E_POINTER;

    *object = NULL;

    if (IsEqualGUID(
            riid,
            &IID_IUnknown))
    {
        *object = self;

        InterlockedIncrement(
            &self->refs);

        return S_OK;
    }

    if (IsEqualGUID(
            riid,
            &IID_ReceivedHandler))
    {
        *object = self;

        InterlockedIncrement(
            &self->refs);

        return S_OK;
    }

    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE handler_AddRef(
    BLEEventHandler *self)
{
    return (ULONG)InterlockedIncrement(
        &self->refs);
}

static ULONG STDMETHODCALLTYPE handler_Release(
    BLEEventHandler *self)
{
    LONG refs =
        InterlockedDecrement(
            &self->refs);

    if (refs == 0)
        free(self);

    return (ULONG)refs;
}

static HRESULT STDMETHODCALLTYPE handler_Invoke(
    BLEEventHandler *self,
    void *sender,
    void *args)
{
    (void)self;
    (void)sender;

    if (!args)
        return E_INVALIDARG;

    BLEReceivedArgs *event_args = NULL;

    HRESULT hr =
        ((IInspectable_C *)args)->lpVtbl->QueryInterface(args, &IID_IBluetoothLEAdvertisementReceivedEventArgs, (void **)&event_args);

    if (FAILED(hr))
        return hr;

    INT16 rssi = 0;
    UINT64 address = 0;
    INT32 advertisement_type = 0;

    event_args->lpVtbl->GetRawSignalStrength(
        event_args,
        &rssi);

    event_args->lpVtbl->GetBluetoothAddress(
        event_args,
        &address);

    event_args->lpVtbl->GetAdvertisementType(
        event_args,
        &advertisement_type);

    void *advertisement_object = NULL;

    hr =
        event_args->lpVtbl->GetAdvertisement(
            event_args,
            &advertisement_object);

    printf("\n");
    printf("========================================\n");

    printf("Address     : ");
    print_address(address);
    printf("\n");

    printf(
        "RSSI        : %d dBm\n",
        (int)rssi);

    printf(
        "Type        : %s (%d)\n",
        advertisement_type_name(
            advertisement_type),
        (int)advertisement_type);

    if (SUCCEEDED(hr) && advertisement_object)
    {
        BLEAdvertisement *advertisement =
            (BLEAdvertisement *)advertisement_object;

        print_advertisement(
            advertisement);

        advertisement->lpVtbl->Release(
            advertisement);
    }
    else
    {
        printf(
            "Advertisement: <unavailable>\n");
    }

    printf("========================================\n");

    event_args->lpVtbl->Release(
        event_args);

    return S_OK;
}

static BLEEventHandlerVtbl g_handler_vtbl =
    {
        handler_QueryInterface,
        handler_AddRef,
        handler_Release,
        handler_Invoke};

static BLEEventHandler *handler_create(void)
{
    BLEEventHandler *handler =
        (BLEEventHandler *)calloc(
            1,
            sizeof(*handler));

    if (!handler)
        return NULL;

    handler->lpVtbl =
        &g_handler_vtbl;

    handler->refs = 1;

    return handler;
}

/* ============================================================
   Main
   ============================================================ */

int main(void)
{
    HRESULT hr;

    printf("========================================\n");
    printf("        Pure C Windows BLE scanner\n");
    printf("========================================\n\n");

    printf(
        "Pairing is NOT required.\n");

    printf(
        "Active BLE scanning enabled.\n");

    printf(
        "Press Ctrl+C to stop.\n\n");

    SetConsoleCtrlHandler(
        console_handler,
        TRUE);

    /*
        Initialize WinRT.
    */

    hr = RoInitialize(
        RO_INIT_MULTITHREADED);

    if (FAILED(hr))
    {
        print_hr(
            "RoInitialize",
            hr);

        return 1;
    }

    /*
        Create runtime class instance.
    */

    HSTRING class_name = NULL;

    hr = WindowsCreateString(
        WATCHER_CLASS_NAME,
        (UINT32)wcslen(
            WATCHER_CLASS_NAME),
        &class_name);

    if (FAILED(hr))
    {
        print_hr(
            "WindowsCreateString",
            hr);

        RoUninitialize();

        return 1;
    }

    IInspectable_C *instance = NULL;

    hr = RoActivateInstance(
        class_name,
        (IInspectable **)&instance);

    WindowsDeleteString(
        class_name);

    if (FAILED(hr))
    {
        print_hr(
            "RoActivateInstance",
            hr);

        RoUninitialize();

        return 1;
    }

    /*
        Obtain watcher interface.
    */

    BLEWatcher *watcher = NULL;

    hr = instance->lpVtbl->QueryInterface(
        instance,
        &IID_IBluetoothLEAdvertisementWatcher,
        (void **)&watcher);

    instance->lpVtbl->Release(
        instance);

    if (FAILED(hr))
    {
        print_hr(
            "QueryInterface(BluetoothLEAdvertisementWatcher)",
            hr);

        RoUninitialize();

        return 1;
    }

    printf(
        "[+] BLE watcher created\n");

    /*
        Active scanning.

        0 = Passive
        1 = Active
    */

    hr =
        watcher->lpVtbl->SetScanningMode(
            watcher,
            1);

    if (FAILED(hr))
    {
        print_hr(
            "SetScanningMode",
            hr);

        watcher->lpVtbl->Release(
            watcher);

        RoUninitialize();

        return 1;
    }

    /*
        Create callback object.
    */

    BLEEventHandler *handler =
        handler_create();

    if (!handler)
    {
        fprintf(
            stderr,
            "Out of memory.\n");

        watcher->lpVtbl->Release(
            watcher);

        RoUninitialize();

        return 1;
    }

    /*
        Register Received event.
    */

    EventRegistrationToken_C token;

    memset(
        &token,
        0,
        sizeof(token));

    hr =
        watcher->lpVtbl->AddReceived(
            watcher,
            handler,
            &token);

    if (FAILED(hr))
    {
        print_hr(
            "AddReceived",
            hr);

        handler->lpVtbl->Release(
            handler);

        watcher->lpVtbl->Release(
            watcher);

        RoUninitialize();

        return 1;
    }

    /*
        Watcher has retained the event handler.
        We can release our reference.
    */

    handler->lpVtbl->Release(
        handler);

    printf(
        "[+] Received callback registered\n");

    /*
        Start scanner.
    */

    hr =
        watcher->lpVtbl->Start(
            watcher);

    if (FAILED(hr))
    {
        print_hr(
            "Start",
            hr);

        watcher->lpVtbl->RemoveReceived(
            watcher,
            token);

        watcher->lpVtbl->Release(
            watcher);

        RoUninitialize();

        return 1;
    }

    printf(
        "[+] BLE scanning started\n\n");

    /*
        Main loop.
    */

    while (
        InterlockedCompareExchange(
            &g_running,
            1,
            1))
    {
        Sleep(100);
    }

    printf(
        "\nStopping scanner...\n");

    /*
        Stop.
    */

    watcher->lpVtbl->Stop(
        watcher);

    /*
        Unregister event.
    */

    watcher->lpVtbl->RemoveReceived(
        watcher,
        token);

    /*
        Release watcher.
    */

    watcher->lpVtbl->Release(
        watcher);

    RoUninitialize();

    printf(
        "Done.\n");

    return 0;
}