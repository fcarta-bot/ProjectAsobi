// Implementazione Windows: usa l'API moderna di Windows (WinRT) per cercare i dispositivi
// Bluetooth vicini, sia classici (PlayStation, Switch Pro) sia LE (Xbox Series), e abbinarli.
// Questo file non include raylib: windows.h e raylib usano alcuni nomi uguali.
#ifdef _WIN32
#include "ControllerLink.hpp"

#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <bluetoothapis.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cwchar>
#ifdef ASOBI_LINK_TEST
#include <cstdio>
#endif

// ---------------------------------------------------------------------------
// Interfacce WinRT dichiarate a mano: le intestazioni WinRT di MinGW non compilano in C++
// (boolean e BYTE coincidono e alcuni template risultano definiti due volte).
// GUID e ordine dei metodi verificati sui metadati di Windows (Windows.Devices.Enumeration).
// ---------------------------------------------------------------------------

struct IAsyncInfoRt : public IInspectable { // Windows.Foundation.IAsyncInfo
    virtual HRESULT STDMETHODCALLTYPE get_Id(UINT32* id) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Status(INT32* status) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ErrorCode(HRESULT* code) = 0;
    virtual HRESULT STDMETHODCALLTYPE Cancel() = 0;
    virtual HRESULT STDMETHODCALLTYPE Close() = 0;
};

// IAsyncOperation<T>: ci serve solo GetResults, lo stato si legge da IAsyncInfo
template <typename T>
struct IAsyncOperationRt : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE put_Completed(IUnknown* handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Completed(IUnknown** handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResults(T* result) = 0;
};

template <typename T>
struct IVectorViewRt : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE GetAt(UINT32 index, T* item) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Size(UINT32* size) = 0;
    virtual HRESULT STDMETHODCALLTYPE IndexOf(T item, UINT32* index, boolean* found) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMany(UINT32 start, UINT32 capacity, T* items, UINT32* actual) = 0;
};

struct IDeviceInfo : public IInspectable { // IDeviceInformation (usiamo solo i primi due metodi)
    virtual HRESULT STDMETHODCALLTYPE get_Id(HSTRING* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Name(HSTRING* value) = 0;
};

using DeviceList = IVectorViewRt<IDeviceInfo*>;
using FindAllOperation = IAsyncOperationRt<DeviceList*>;

struct IDeviceInfoStatics2 : public IInspectable { // IDeviceInformationStatics2
    virtual HRESULT STDMETHODCALLTYPE GetAqsFilterFromDeviceClass(INT32 deviceClass, HSTRING* filter) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateFromIdAsyncWithKindAndAdditionalProperties(
        HSTRING id, IUnknown* properties, INT32 kind, IUnknown** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindAllAsyncWithKindAqsFilterAndAdditionalProperties(
        HSTRING filter, IUnknown* properties, INT32 kind, FindAllOperation** operation) = 0;
};

struct IPairingResult : public IInspectable { // IDevicePairingResult
    virtual HRESULT STDMETHODCALLTYPE get_Status(INT32* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ProtectionLevelUsed(INT32* value) = 0;
};

// IAsyncOperation<DevicePairingResult>: ci serve solo GetResults, lo stato si legge da IAsyncInfo
struct IPairingOperation : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE put_Completed(IUnknown* handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Completed(IUnknown** handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResults(IPairingResult** result) = 0;
};

struct IDevicePairing : public IInspectable { // IDeviceInformationPairing
    virtual HRESULT STDMETHODCALLTYPE get_IsPaired(boolean* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CanPair(boolean* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE PairAsync(IPairingOperation** operation) = 0;
    virtual HRESULT STDMETHODCALLTYPE PairWithProtectionLevelAsync(INT32 level, IPairingOperation** operation) = 0;
};

struct IDeviceInfo2 : public IInspectable { // IDeviceInformation2
    virtual HRESULT STDMETHODCALLTYPE get_Kind(INT32* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Pairing(IDevicePairing** value) = 0;
};

static const GUID IID_DeviceInfo2 = { 0xf156a638, 0x7997, 0x48d9, { 0xa1, 0x0c, 0x26, 0x9d, 0x46, 0x53, 0x3f, 0x48 } };
static const GUID IID_DeviceInfoStatics2 = { 0x493b4f34, 0xa84f, 0x45fd, { 0x91, 0x67, 0x15, 0xd1, 0xcb, 0x1b, 0xd1, 0xf9 } };
static const GUID IID_AsyncInfo = { 0x00000036, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

enum { ASYNC_STARTED = 0, ASYNC_COMPLETED = 1, ASYNC_CANCELED = 2, ASYNC_ERROR = 3 };
constexpr int KIND_ASSOCIATION_ENDPOINT = 5; // DeviceInformationKind: dispositivi "vicini", non ancora driver

// Filtri di ricerca: dispositivi Bluetooth classici / LE non ancora abbinati
static const wchar_t* CLASSIC_FILTER =
    L"System.Devices.Aep.ProtocolId:=\"{e0cbf06c-cd8b-4647-bb8a-263b43f0f974}\" AND "
    L"System.Devices.Aep.IsPaired:=System.StructuredQueryType.Boolean#False";
static const wchar_t* LE_FILTER =
    L"System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\" AND "
    L"System.Devices.Aep.IsPaired:=System.StructuredQueryType.Boolean#False";

template <typename T>
static void SafeRelease(T*& p) {
    if (p) { p->Release(); p = nullptr; }
}

static HSTRING MakeHString(const wchar_t* text) {
    HSTRING s = nullptr;
    WindowsCreateString(text, (UINT32)wcslen(text), &s);
    return s;
}

static std::string ToUtf8(HSTRING s) {
    UINT32 len = 0;
    const wchar_t* raw = WindowsGetStringRawBuffer(s, &len);
    if (!raw || len == 0) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, raw, (int)len, nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, raw, (int)len, out.data(), size, nullptr, nullptr);
    return out;
}

// Le operazioni WinRT sono asincrone: ne leggiamo lo stato ogni frame senza bloccare il gioco
static int AsyncState(IUnknown* op) {
    int state = ASYNC_ERROR;
    IAsyncInfoRt* info = nullptr;
    if (SUCCEEDED(op->QueryInterface(IID_AsyncInfo, (void**)&info))) {
        INT32 s;
        if (SUCCEEDED(info->get_Status(&s))) state = (int)s;
        info->Release();
    }
    return state;
}

static void CancelAsync(IUnknown* op) {
    IAsyncInfoRt* info = nullptr;
    if (SUCCEEDED(op->QueryInterface(IID_AsyncInfo, (void**)&info))) {
        info->Cancel();
        info->Release();
    }
}

static bool LooksLikeGamepad(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    static const char* words[] = { "controller", "gamepad", "joy", "pad", "xbox", "dualsense", "dualshock", "8bitdo" };
    for (const char* w : words)
        if (name.find(w) != std::string::npos) return true;
    return false;
}

// Messaggi per i codici di DevicePairingResultStatus piu' comuni
static const char* PairingError(int code) {
    switch (code) {
        case 1:  return "Il controller non e' pronto: rimettilo in modalita' abbinamento e riprova";
        case 4:  return "Il controller ha rifiutato la connessione";
        case 5:  return "Troppi dispositivi collegati al Bluetooth";
        case 7:  return "Tempo scaduto: riprova tenendo il controller vicino al PC";
        case 8:
        case 12: return "Windows non ha consentito l'abbinamento";
        case 14: return "Abbinamento annullato";
        case 16: return "Questo controller chiede un codice: abbinalo dalle impostazioni di Windows";
        default: return nullptr;
    }
}

static bool DetectBluetoothRadio() {
    BLUETOOTH_FIND_RADIO_PARAMS params = { sizeof(params) };
    HANDLE radio = nullptr;
    HBLUETOOTH_RADIO_FIND find = BluetoothFindFirstRadio(&params, &radio);
    if (!find) return false;
    CloseHandle(radio);
    BluetoothFindRadioClose(find);
    return true;
}

class WindowsLink : public ControllerLink {
public:
    WindowsLink() {
        ownsRuntime = SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED));
        HSTRING cls = MakeHString(L"Windows.Devices.Enumeration.DeviceInformation");
        if (cls) {
            RoGetActivationFactory(cls, IID_DeviceInfoStatics2, (void**)&statics);
            WindowsDeleteString(cls);
        }
        bluetooth = DetectBluetoothRadio();
        lastRadioCheck = std::chrono::steady_clock::now();
    }

    ~WindowsLink() override {
        Cancel();
        ReleaseDevices();
        SafeRelease(statics);
        if (ownsRuntime) RoUninitialize();
    }

    bool PairingSupported() const override { return statics != nullptr; }
    bool BluetoothAvailable() const override { return bluetooth; }

    void StartScan() override {
        Cancel();
        ReleaseDevices();
        if (!statics) { Fail("La ricerca Bluetooth non e' disponibile su questa versione di Windows"); return; }
#ifndef ASOBI_LINK_TEST // nei test si cerca anche senza adattatore, per provare le chiamate a Windows
        if (!bluetooth) { Fail("Bluetooth non disponibile su questo dispositivo"); return; }
#endif
        status = LinkStatus::Scanning;
        message = "Ricerca in corso...";
        StartQueries();
    }

    void Pair(int index) override {
        if (index < 0 || index >= (int)devices.size()) return;
        StopQueries();

        IDeviceInfo2* info2 = nullptr;
        IDevicePairing* pairing = nullptr;
        bool started = SUCCEEDED(devices[index]->QueryInterface(IID_DeviceInfo2, (void**)&info2)) &&
                       SUCCEEDED(info2->get_Pairing(&pairing)) && pairing &&
                       SUCCEEDED(pairing->PairAsync(&pairOp)) && pairOp;
        SafeRelease(pairing);
        SafeRelease(info2);
        if (!started) { Fail("Impossibile avviare l'abbinamento con questo dispositivo"); return; }

        status = LinkStatus::Pairing;
        message = "Abbinamento con " + found[index].name + "...";
    }

    void Cancel() override {
        StopQueries();
        if (pairOp) {
            CancelAsync(pairOp);
            SafeRelease(pairOp);
        }
        if (status == LinkStatus::Scanning || status == LinkStatus::Pairing) status = LinkStatus::Idle;
    }

    void Update() override {
        // Ogni tanto ricontrolla l'adattatore: una pennetta Bluetooth puo' essere inserita a gioco aperto
        auto now = std::chrono::steady_clock::now();
        if (now - lastRadioCheck > std::chrono::seconds(2)) {
            bluetooth = DetectBluetoothRadio();
            lastRadioCheck = now;
        }

        if (status == LinkStatus::Scanning) {
            for (Query& q : queries) PollQuery(q);
            // Finito un giro di ricerca ne parte un altro: i controller possono comparire in ritardo
            if (!queries[0].op && !queries[1].op && now - lastQueryStart > std::chrono::seconds(3)) StartQueries();
            message = found.empty() ? "Ricerca in corso..."
                                    : std::to_string(found.size()) + " dispositivi trovati: scegli il tuo controller";
        }

        if (status == LinkStatus::Pairing && pairOp) {
            int state = AsyncState(pairOp);
            if (state == ASYNC_STARTED) return;
            INT32 result = -1;
            IPairingResult* r = nullptr;
            if (state == ASYNC_COMPLETED && SUCCEEDED(pairOp->GetResults(&r)) && r) {
                r->get_Status(&result);
                r->Release();
            }
            SafeRelease(pairOp);

            if (result == 0 || result == 3) { // Paired / AlreadyPaired
                status = LinkStatus::Paired;
                message = "Abbinato! Il gioco lo collega appena Windows lo rende disponibile";
            } else {
                const char* e = PairingError(result);
                Fail(e ? e : "Abbinamento non riuscito (codice " + std::to_string(result) + ")");
            }
        }
    }

private:
    struct Query {
        FindAllOperation* op = nullptr;
        bool lowEnergy = false;
    };

    void Fail(const std::string& text) {
        status = LinkStatus::Failed;
        message = text;
    }

    void StartQueries() {
        lastQueryStart = std::chrono::steady_clock::now();
        for (Query& q : queries) {
            if (q.op) continue;
            HSTRING filter = MakeHString(q.lowEnergy ? LE_FILTER : CLASSIC_FILTER);
            statics->FindAllAsyncWithKindAqsFilterAndAdditionalProperties(
                filter, nullptr, KIND_ASSOCIATION_ENDPOINT, &q.op);
            WindowsDeleteString(filter);
        }
    }

    void StopQueries() {
        for (Query& q : queries) {
            if (!q.op) continue;
            CancelAsync(q.op);
            SafeRelease(q.op);
        }
    }

    void PollQuery(Query& q) {
        if (!q.op) return;
        int state = AsyncState(q.op);
        if (state == ASYNC_STARTED) return;
        if (state == ASYNC_COMPLETED) {
            DeviceList* list = nullptr;
            if (SUCCEEDED(q.op->GetResults(&list)) && list) {
                UINT32 count = 0;
                list->get_Size(&count);
#ifdef ASOBI_LINK_TEST
                std::printf("ricerca %s completata: %u dispositivi\n", q.lowEnergy ? "LE" : "classica", count);
#endif
                for (UINT32 i = 0; i < count; ++i) {
                    IDeviceInfo* d = nullptr;
                    if (SUCCEEDED(list->GetAt(i, &d)) && d) {
                        AddDevice(d, q.lowEnergy);
                        d->Release();
                    }
                }
                list->Release();
            }
        }
        SafeRelease(q.op);
    }

    void AddDevice(IDeviceInfo* d, bool lowEnergy) {
        HSTRING hid = nullptr, hname = nullptr;
        d->get_Id(&hid);
        d->get_Name(&hname);
        NearbyController c{ ToUtf8(hid), ToUtf8(hname), lowEnergy, false };
        WindowsDeleteString(hid);
        WindowsDeleteString(hname);
        if (c.name.empty()) return; // i dispositivi LE senza nome sono quasi sempre altro (sensori, beacon...)
        for (const NearbyController& f : found)
            if (f.id == c.id) return;

        c.looksLikeGamepad = LooksLikeGamepad(c.name);
        // I controller vanno in cima alla lista, prima degli altri dispositivi
        size_t pos = found.size();
        if (c.looksLikeGamepad)
            pos = std::find_if(found.begin(), found.end(), [](const NearbyController& f) { return !f.looksLikeGamepad; }) - found.begin();
        d->AddRef();
        found.insert(found.begin() + pos, c);
        devices.insert(devices.begin() + pos, d);
    }

    void ReleaseDevices() {
        for (IDeviceInfo*& d : devices) SafeRelease(d);
        devices.clear();
        found.clear();
    }

    IDeviceInfoStatics2* statics = nullptr;
    Query queries[2] = { { nullptr, false }, { nullptr, true } };
    IPairingOperation* pairOp = nullptr;
    std::vector<IDeviceInfo*> devices; // stesso ordine di "found"
    bool ownsRuntime = false;
    bool bluetooth = false;
    std::chrono::steady_clock::time_point lastRadioCheck, lastQueryStart;
};

std::unique_ptr<ControllerLink> ControllerLink::Create() {
    return std::make_unique<WindowsLink>();
}
#endif
