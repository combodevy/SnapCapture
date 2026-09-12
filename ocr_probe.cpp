#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <activation.h>
#include <cstdio>

namespace {

const wchar_t* HResultName(HRESULT hr) {
    switch (hr) {
    case S_OK: return L"S_OK";
    case REGDB_E_CLASSNOTREG: return L"REGDB_E_CLASSNOTREG";
    case E_NOINTERFACE: return L"E_NOINTERFACE";
    default: return L"(unknown)";
    }
}

void PrintResult(const wchar_t* step, HRESULT hr) {
    wprintf(L"[probe] %-28ls -> hr=0x%08lX (%ls)\n", step, static_cast<unsigned long>(hr), HResultName(hr));
}

} // namespace

int wmain() {
    wprintf(L"=== SnapCapture OCR Feasibility Probe (Win10+) ===\n");

    const HRESULT initHr = RoInitialize(RO_INIT_MULTITHREADED);
    PrintResult(L"RoInitialize(MTA)", initHr);
    if (FAILED(initHr) && initHr != S_FALSE) {
        wprintf(L"[result] FAIL: WinRT runtime init failed.\n");
        return 2;
    }

    const wchar_t* className = L"Windows.Media.Ocr.OcrEngine";
    HSTRING classId = nullptr;
    HRESULT hr = WindowsCreateString(className, static_cast<UINT32>(wcslen(className)), &classId);
    PrintResult(L"WindowsCreateString", hr);
    if (FAILED(hr)) {
        if (SUCCEEDED(initHr) || initHr == S_FALSE) {
            RoUninitialize();
        }
        wprintf(L"[result] FAIL: Cannot build WinRT class name.\n");
        return 3;
    }

    IActivationFactory* activationFactory = nullptr;
    hr = RoGetActivationFactory(classId, __uuidof(IActivationFactory), reinterpret_cast<void**>(&activationFactory));
    PrintResult(L"RoGetActivationFactory", hr);

    if (activationFactory) {
        activationFactory->Release();
        wprintf(L"[result] PASS: Windows.Media.Ocr.OcrEngine activation factory is available.\n");
    } else {
        wprintf(L"[result] FAIL: OcrEngine activation factory unavailable.\n");
    }

    WindowsDeleteString(classId);

    if (SUCCEEDED(initHr) || initHr == S_FALSE) {
        RoUninitialize();
    }

    return activationFactory ? 0 : 4;
}
