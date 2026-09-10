/*
    ble_scan.c
    Pure C / Win32 + WinRT ABI
    MinGW-w64 GCC

    Scans BLE advertisements WITHOUT pairing.

    Shows:
        - device address
        - RSSI
        - local name
        - advertisement type

    Ctrl+C stops the scanner.

    Build:
        gcc -std=c11 -Wall -Wextra -O2 ble_scan.c -o ble_scan.exe -lole32

    On some MinGW installations, if the linker cannot resolve WinRT
    functions, add:
        -lwindowsapp
*/

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <roapi.h>
#include <winstring.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
   WinRT ABI basics
   ================================================================ */

typedef struct IInspectableC IInspectableC;
typedef struct IUnknownVtblC IUnknownVtblC;

typedef HRESULT(STDMETHODCALLTYPE *QueryInterfaceFn)(
    void *self,
    REFIID riid,
    void **object);

typedef ULONG(STDMETHODCALLTYPE *AddRefFn)(
    void *self);

typedef ULONG(STDMETHODCALLTYPE *ReleaseFn)(
    void *self);

struct IUnknownVtblC
{
    QueryInterfaceFn QueryInterface;
    AddRefFn AddRef;
    ReleaseFn Release;
};

typedef struct
{
    QueryInterfaceFn QueryInterface;
    AddRefFn AddRef;
    ReleaseFn Release;
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
} IInspectableVtblC;

struct IInspectableC
{
    IInspectableVtblC *lpVtbl;
};

/* ================================================================
   EventRegistrationToken
   ================================================================ */

typedef struct
{
    INT64 value;
} EventRegistrationTokenC;

/* ================================================================
   GUIDs
   ================================================================ */

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
        BluetoothLEAdvertisementWatcher,
        BluetoothLEAdvertisementReceivedEventArgs
    >

    Parameterized WinRT GUID.
*/
static const GUID IID_ReceivedHandler =
    {
        0x90EB4ECA,
        0xD465,
        0x5EA0,
        {0xA6, 0x1C, 0x03, 0x3C, 0x8C, 0x5E, 0xCE, 0xF2}};

/*
    Runtime class name.
*/
static const wchar_t WATCHER_CLASS[] =
    L"Windows.Devices.Bluetooth.Advertisement.BluetoothLEAdvertisementWatcher";

/* ================================================================
   Enums
   ================================================================ */

/*
    BluetoothLEScanningMode
*/
enum
{
    BLE_SCANNING_MODE_PASSIVE = 0,
    BLE_SCANNING_MODE_ACTIVE = 1
};

/*
    BluetoothLEAdvertisementType
    Exact values are not important for our test.
*/
static const char *adv_type_name(INT32 type)
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

/* ================================================================
   BLE watcher ABI
   ================================================================ */

typedef struct BLEWatcher BLEWatcher;
typedef struct BLEWatcherVtbl BLEWatcherVtbl;

typedef HRESULT(STDMETHODCALLTYPE *WatcherGetTimeSpanFn)(
    BLEWatcher *self,
    int64_t *value);

typedef HRESULT(STDMETHODCALLTYPE *WatcherGetStatusFn)(
    BLEWatcher *self,
    INT32 *value);

typedef HRESULT(STDMETHODCALLTYPE *WatcherGetScanningModeFn)(
    BLEWatcher *self,
    INT32 *value);

typedef HRESULT(STDMETHODCALLTYPE *WatcherSetScanningModeFn)(
    BLEWatcher *self,
    INT32 value);

typedef HRESULT(STDMETHODCALLTYPE *WatcherGetInterfaceFn)(
    BLEWatcher *self,
    void **value);

typedef HRESULT(STDMETHODCALLTYPE *WatcherSetInterfaceFn)(
    BLEWatcher *self,
    void *value);

typedef HRESULT(STDMETHODCALLTYPE *WatcherAddReceivedFn)(
    BLEWatcher *self,
    void *handler,
    EventRegistrationTokenC *token);

typedef HRESULT(STDMETHODCALLTYPE *WatcherRemoveReceivedFn)(
    BLEWatcher *self,
    EventRegistrationTokenC token);

typedef HRESULT(STDMETHODCALLTYPE *WatcherAddStoppedFn)(
    BLEWatcher *self,
    void *handler,
    EventRegistrationTokenC *token);

typedef HRESULT(STDMETHODCALLTYPE *WatcherRemoveStoppedFn)(
    BLEWatcher *self,
    EventRegistrationTokenC token);

struct BLEWatcherVtbl
{
    /* IUnknown */
    QueryInterfaceFn QueryInterface;
    AddRefFn AddRef;
    ReleaseFn Release;

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

    WatcherGetTimeSpanFn get_MinSamplingInterval;
    WatcherGetTimeSpanFn get_MaxSamplingInterval;
    WatcherGetTimeSpanFn get_MinOutOfRangeTimeout;
    WatcherGetTimeSpanFn get_MaxOutOfRangeTimeout;

    WatcherGetStatusFn get_Status;
    WatcherGetScanningModeFn get_ScanningMode;
    WatcherSetScanningModeFn put_ScanningMode;

    WatcherGetInterfaceFn get_SignalStrengthFilter;
    WatcherSetInterfaceFn put_SignalStrengthFilter;

    WatcherGetInterfaceFn get_AdvertisementFilter;
    WatcherSetInterfaceFn put_AdvertisementFilter;

    HRESULT(STDMETHODCALLTYPE *Start)(
        BLEWatcher *self);

    HRESULT(STDMETHODCALLTYPE *Stop)(
        BLEWatcher *self);

    WatcherAddReceivedFn add_Received;
    WatcherRemoveReceivedFn remove_Received;

    WatcherAddStoppedFn add_Stopped;
    WatcherRemoveStoppedFn remove_Stopped;
};

struct BLEWatcher
{
    BLEWatcherVtbl *lpVtbl;
};

/* ================================================================
   ReceivedEventArgs ABI
   ================================================================ */

typedef struct BLEReceivedArgs BLEReceivedArgs;
typedef struct BLEReceivedArgsVtbl BLEReceivedArgsVtbl;

typedef HRESULT(STDMETHODCALLTYPE *ArgsGetSignalFn)(
    BLEReceivedArgs *self,
    INT16 *value);

typedef HRESULT(STDMETHODCALLTYPE *ArgsGetAddressFn)(
    BLEReceivedArgs *self,
    UINT64 *value);

typedef HRESULT(STDMETHODCALLTYPE *ArgsGetAdvTypeFn)(
    BLEReceivedArgs *self,
    INT32 *value);

typedef HRESULT(STDMETHODCALLTYPE *ArgsGetTimestampFn)(
    BLEReceivedArgs *self,
    int64_t *value);

typedef HRESULT(STDMETHODCALLTYPE *ArgsGetAdvertisementFn)(
    BLEReceivedArgs *self,
    void **value);

struct BLEReceivedArgsVtbl
{
    /* IUnknown */
    QueryInterfaceFn QueryInterface;
    AddRefFn AddRef;
    ReleaseFn Release;

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

    /* IBluetoothLEAdvertisementReceivedEventArgs */

    ArgsGetSignalFn get_RawSignalStrengthInDBm;
    ArgsGetAddressFn get_BluetoothAddress;
    ArgsGetAdvTypeFn get_AdvertisementType;
    ArgsGetTimestampFn get_Timestamp;
    ArgsGetAdvertisementFn get_Advertisement;
};

struct BLEReceivedArgs
{
    BLEReceivedArgsVtbl *lpVtbl;
};

/* ================================================================
   Callback object
   ================================================================ */

typedef struct BLEHandler BLEHandler;
typedef struct BLEHandlerVtbl BLEHandlerVtbl;

typedef HRESULT(STDMETHODCALLTYPE *HandlerQueryInterfaceFn)(
    BLEHandler *self,
    REFIID riid,
    void **object);

typedef ULONG(STDMETHODCALLTYPE *HandlerAddRefFn)(
    BLEHandler *self);

typedef ULONG(STDMETHODCALLTYPE *HandlerReleaseFn)(
    BLEHandler *self);

typedef HRESULT(STDMETHODCALLTYPE *HandlerInvokeFn)(
    BLEHandler *self,
    void *sender,
    void *args);

struct BLEHandlerVtbl
{
    HandlerQueryInterfaceFn QueryInterface;
    HandlerAddRefFn AddRef;
    HandlerReleaseFn Release;
    HandlerInvokeFn Invoke;
};

struct BLEHandler
{
    BLEHandlerVtbl *lpVtbl;
    LONG refs;
};

/* ================================================================
   Globals
   ================================================================ */

static volatile LONG g_running = 1;

static HRESULT IInspectableC_QueryInterface(
    void *object,
    REFIID riid,
    void **result);

/* ================================================================
   Helpers
   ================================================================ */

static void print_hr(const char *where, HRESULT hr)
{
    fprintf(
        stderr,
        "%s failed: HRESULT=0x%08lX\n",
        where,
        (unsigned long)hr);
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

/* ================================================================
   Handler implementation
   ================================================================ */

static HRESULT STDMETHODCALLTYPE handler_QueryInterface(
    BLEHandler *self,
    REFIID riid,
    void **object)
{
    if (!object)
        return E_POINTER;

    *object = NULL;

    /*
        We accept IUnknown and the exact delegate IID.
    */
    if (IsEqualGUID(riid, &IID_IUnknown) ||
        IsEqualGUID(riid, &IID_ReceivedHandler))
    {
        *object = self;

        InterlockedIncrement(&self->refs);

        return S_OK;
    }

    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE handler_AddRef(
    BLEHandler *self)
{
    return (ULONG)InterlockedIncrement(&self->refs);
}

static ULONG STDMETHODCALLTYPE handler_Release(
    BLEHandler *self)
{
    LONG refs = InterlockedDecrement(&self->refs);

    if (refs == 0)
        free(self);

    return (ULONG)refs;
}

static HRESULT STDMETHODCALLTYPE handler_Invoke(
    BLEHandler *self,
    void *sender,
    void *args)
{
    (void)self;
    (void)sender;

    BLEReceivedArgs *event = NULL;

    HRESULT hr = IInspectableC_QueryInterface(
        args,
        &IID_IBluetoothLEAdvertisementReceivedEventArgs,
        (void **)&event);

    if (FAILED(hr))
        return hr;

    INT16 rssi = 0;
    UINT64 address = 0;
    INT32 adv_type = 0;

    event->lpVtbl->get_RawSignalStrengthInDBm(
        event,
        &rssi);

    event->lpVtbl->get_BluetoothAddress(
        event,
        &address);

    event->lpVtbl->get_AdvertisementType(
        event,
        &adv_type);

    printf("\n----------------------------------------\n");

    printf("Address : ");
    print_address(address);
    printf("\n");

    printf("RSSI    : %d dBm\n", (int)rssi);

    printf(
        "Type    : %s (%d)\n",
        adv_type_name(adv_type),
        (int)adv_type);

    /*
        For now we don't inspect the Advertisement object.
        The next stage can extract:
            LocalName
            Service UUIDs
            Manufacturer Data
            Service Data
            raw DataSections
    */

    event->lpVtbl->Release(event);

    return S_OK;
}

static BLEHandlerVtbl g_handler_vtbl =
    {
        handler_QueryInterface,
        handler_AddRef,
        handler_Release,
        handler_Invoke};

static BLEHandler *handler_create(void)
{
    BLEHandler *handler =
        (BLEHandler *)calloc(1, sizeof(*handler));

    if (!handler)
        return NULL;

    handler->lpVtbl = &g_handler_vtbl;
    handler->refs = 1;

    return handler;
}

/* ================================================================
   IInspectable helper
   ================================================================ */

static HRESULT IInspectableC_QueryInterface(
    void *object,
    REFIID riid,
    void **result)
{
    IInspectableC *inspectable =
        (IInspectableC *)object;

    return inspectable->lpVtbl->QueryInterface(
        object,
        riid,
        result);
}

/* ================================================================
   Ctrl+C
   ================================================================ */

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

/* ================================================================
   Main
   ================================================================ */

int main(void)
{
    HRESULT hr;

    printf("========================================\n");
    printf("        Pure C Windows BLE scanner\n");
    printf("========================================\n\n");

    printf("Pairing is NOT required.\n");
    printf("Listening for BLE advertisements...\n");
    printf("Press Ctrl+C to stop.\n\n");

    SetConsoleCtrlHandler(console_handler, TRUE);

    /*
        Initialize Windows Runtime.
    */
    hr = RoInitialize(RO_INIT_MULTITHREADED);

    if (FAILED(hr))
    {
        print_hr("RoInitialize", hr);
        return 1;
    }

    /*
        Create HSTRING containing runtime class name.
    */
    HSTRING class_name = NULL;

    hr = WindowsCreateString(
        WATCHER_CLASS,
        (UINT32)wcslen(WATCHER_CLASS),
        &class_name);

    if (FAILED(hr))
    {
        print_hr("WindowsCreateString", hr);

        RoUninitialize();
        return 1;
    }

    /*
        Activate watcher.
    */
    IInspectableC *inspectable = NULL;

    hr = RoActivateInstance(
        class_name,
        (IInspectable **)&inspectable);

    WindowsDeleteString(class_name);

    if (FAILED(hr))
    {
        print_hr("RoActivateInstance", hr);

        RoUninitialize();
        return 1;
    }

    /*
        Get IBluetoothLEAdvertisementWatcher.
    */
    BLEWatcher *watcher = NULL;

    hr = inspectable->lpVtbl->QueryInterface(
        inspectable,
        &IID_IBluetoothLEAdvertisementWatcher,
        (void **)&watcher);

    inspectable->lpVtbl->Release(inspectable);

    if (FAILED(hr))
    {
        print_hr(
            "QI(IBluetoothLEAdvertisementWatcher)",
            hr);

        RoUninitialize();
        return 1;
    }

    printf("[+] BLE watcher created\n");

    /*
        Use ACTIVE scanning.

        Passive:
            only advertisements

        Active:
            advertisements + scan responses
    */
    hr = watcher->lpVtbl->put_ScanningMode(
        watcher,
        BLE_SCANNING_MODE_ACTIVE);

    if (FAILED(hr))
    {
        print_hr("put_ScanningMode", hr);

        watcher->lpVtbl->Release(watcher);
        RoUninitialize();

        return 1;
    }

    /*
        Create event handler.
    */
    BLEHandler *handler = handler_create();

    if (!handler)
    {
        fprintf(stderr, "Out of memory.\n");

        watcher->lpVtbl->Release(watcher);
        RoUninitialize();

        return 1;
    }

    /*
        Register Received callback.
    */
    EventRegistrationTokenC token;

    memset(&token, 0, sizeof(token));

    hr = watcher->lpVtbl->add_Received(
        watcher,
        handler,
        &token);

    if (FAILED(hr))
    {
        print_hr("add_Received", hr);

        handler->lpVtbl->Release(handler);
        watcher->lpVtbl->Release(watcher);

        RoUninitialize();

        return 1;
    }

    /*
        We can release our reference after registration.
        The watcher retains its own reference to the delegate.
    */
    handler->lpVtbl->Release(handler);

    printf("[+] Received callback registered\n");

    /*
        Start BLE scanning.
    */
    hr = watcher->lpVtbl->Start(watcher);

    if (FAILED(hr))
    {
        print_hr("Start", hr);

        watcher->lpVtbl->remove_Received(
            watcher,
            token);

        watcher->lpVtbl->Release(watcher);

        RoUninitialize();

        return 1;
    }

    printf("[+] BLE scanning started\n\n");

    /*
        Wait.
    */
    while (InterlockedCompareExchange(
        &g_running,
        1,
        1))
    {
        Sleep(100);
    }

    printf("\nStopping scanner...\n");

    /*
        Stop watcher.
    */
    watcher->lpVtbl->Stop(watcher);

    /*
        Remove callback.
    */
    watcher->lpVtbl->remove_Received(
        watcher,
        token);

    watcher->lpVtbl->Release(watcher);

    RoUninitialize();

    printf("Done.\n");

    return 0;
}