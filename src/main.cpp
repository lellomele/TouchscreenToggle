#include <windows.h>
#include <commctrl.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <dwmapi.h>
#include <setupapi.h>

#include <algorithm>
#include <cwctype>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kWindowClass[] = L"TouchscreenToggleWindow";
constexpr wchar_t kTitle[] = L"Touchscreen Toggle";
constexpr int kAppIconId = 101;
constexpr int kToggleButtonId = 1001;
constexpr int kRefreshButtonId = 1002;

enum class TouchState { Enabled, Disabled, NotFound, Error };

struct TouchDevice {
    std::wstring instanceId;
    std::wstring name;
    TouchState state = TouchState::Error;
};

struct AppState {
    TouchDevice device;
    std::wstring detail;
    HWND toggleButton = nullptr;
    HWND refreshButton = nullptr;
    HFONT titleFont = nullptr;
    HFONT bodyFont = nullptr;
    HFONT buttonFont = nullptr;
    HBRUSH backgroundBrush = nullptr;
    bool busy = false;
};

std::wstring ErrorText(DWORD code) {
    wchar_t* raw = nullptr;
    const DWORD size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                          FORMAT_MESSAGE_FROM_SYSTEM |
                                          FORMAT_MESSAGE_IGNORE_INSERTS,
                                      nullptr, code, 0,
                                      reinterpret_cast<wchar_t*>(&raw), 0, nullptr);
    std::wstring result = size && raw ? std::wstring(raw, size) : L"Errore sconosciuto";
    if (raw) LocalFree(raw);
    while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n' || result.back() == L' ')) {
        result.pop_back();
    }
    return result;
}

std::wstring Lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return value;
}

std::vector<wchar_t> GetRegistryProperty(HDEVINFO set, SP_DEVINFO_DATA& data, DWORD property) {
    DWORD type = 0;
    DWORD needed = 0;
    SetupDiGetDeviceRegistryPropertyW(set, &data, property, &type, nullptr, 0, &needed);
    if (!needed) return {};

    std::vector<wchar_t> buffer((needed + sizeof(wchar_t) - 1) / sizeof(wchar_t) + 2, L'\0');
    if (!SetupDiGetDeviceRegistryPropertyW(set, &data, property, &type,
                                            reinterpret_cast<PBYTE>(buffer.data()),
                                            static_cast<DWORD>(buffer.size() * sizeof(wchar_t)), nullptr)) {
        return {};
    }
    return buffer;
}

std::wstring FirstString(const std::vector<wchar_t>& buffer) {
    return buffer.empty() ? std::wstring{} : std::wstring(buffer.data());
}

bool MultiStringContains(const std::vector<wchar_t>& buffer, const std::wstring& needle) {
    if (buffer.empty()) return false;
    for (const wchar_t* item = buffer.data(); *item; item += wcslen(item) + 1) {
        if (Lower(item).find(Lower(needle)) != std::wstring::npos) return true;
    }
    return false;
}

bool IsTouchscreen(HDEVINFO set, SP_DEVINFO_DATA& data, const std::wstring& instanceId) {
    const auto hardwareIds = GetRegistryProperty(set, data, SPDRP_HARDWAREID);
    const auto compatibleIds = GetRegistryProperty(set, data, SPDRP_COMPATIBLEIDS);
    const auto friendlyName = FirstString(GetRegistryProperty(set, data, SPDRP_FRIENDLYNAME));
    const auto description = FirstString(GetRegistryProperty(set, data, SPDRP_DEVICEDESC));
    const auto searchable = Lower(friendlyName + L" " + description);

    // HID usage page 0x0D / usage 0x04 is the language-independent touchscreen signature.
    const bool standardTouchUsage =
        MultiStringContains(hardwareIds, L"HID_DEVICE_UP:000D_U:0004") ||
        MultiStringContains(compatibleIds, L"HID_DEVICE_UP:000D_U:0004");

    // Exact controller found on this notebook. Restrict Col01 so its vendor-defined
    // sibling and the touchpad on the same I2C bus are never changed.
    const auto id = Lower(instanceId);
    const bool notebookTouch = id.find(L"hid\\gxtp738x&col01\\") == 0;

    // Fallback for standard Windows names, including this notebook's Italian locale.
    const bool namedTouchscreen = searchable.find(L"touch screen") != std::wstring::npos ||
                                  searchable.find(L"touchscreen") != std::wstring::npos;
    return standardTouchUsage || notebookTouch || namedTouchscreen;
}

TouchState ReadState(DEVINST devInst) {
    ULONG status = 0;
    ULONG problem = 0;
    const CONFIGRET result = CM_Get_DevNode_Status(&status, &problem, devInst, 0);
    if (result != CR_SUCCESS) return TouchState::Error;
    if (problem == CM_PROB_DISABLED) return TouchState::Disabled;
    return (status & DN_STARTED) ? TouchState::Enabled : TouchState::Disabled;
}

std::optional<TouchDevice> FindTouchscreen(std::wstring& error) {
    HDEVINFO set = SetupDiGetClassDevsW(&GUID_DEVCLASS_HIDCLASS, nullptr, nullptr, DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE) {
        error = ErrorText(GetLastError());
        return std::nullopt;
    }

    std::optional<TouchDevice> found;
    SP_DEVINFO_DATA data{};
    data.cbSize = sizeof(data);
    for (DWORD index = 0; SetupDiEnumDeviceInfo(set, index, &data); ++index) {
        wchar_t instanceId[MAX_DEVICE_ID_LEN]{};
        if (!SetupDiGetDeviceInstanceIdW(set, &data, instanceId, MAX_DEVICE_ID_LEN, nullptr)) continue;
        if (!IsTouchscreen(set, data, instanceId)) continue;

        auto name = FirstString(GetRegistryProperty(set, data, SPDRP_FRIENDLYNAME));
        if (name.empty()) name = FirstString(GetRegistryProperty(set, data, SPDRP_DEVICEDESC));
        if (name.empty()) name = L"Touchscreen HID";
        found = TouchDevice{instanceId, name, ReadState(data.DevInst)};
        break;
    }
    const DWORD lastError = GetLastError();
    SetupDiDestroyDeviceInfoList(set);

    if (!found && lastError != ERROR_NO_MORE_ITEMS) error = ErrorText(lastError);
    return found;
}

bool ChangeDeviceState(const std::wstring& instanceId, bool enable, bool& rebootRequired,
                       std::wstring& error) {
    rebootRequired = false;
    HDEVINFO set = SetupDiGetClassDevsW(&GUID_DEVCLASS_HIDCLASS, nullptr, nullptr, DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE) {
        error = ErrorText(GetLastError());
        return false;
    }

    SP_DEVINFO_DATA data{};
    data.cbSize = sizeof(data);
    if (!SetupDiOpenDeviceInfoW(set, instanceId.c_str(), nullptr, 0, &data)) {
        error = ErrorText(GetLastError());
        SetupDiDestroyDeviceInfoList(set);
        return false;
    }

    SP_PROPCHANGE_PARAMS params{};
    params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
    params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
    params.StateChange = enable ? DICS_ENABLE : DICS_DISABLE;
    params.Scope = DICS_FLAG_GLOBAL;
    params.HwProfile = 0;

    bool success = SetupDiSetClassInstallParamsW(
                       set, &data, &params.ClassInstallHeader, sizeof(params)) &&
                   SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, set, &data);
    if (!success) {
        error = ErrorText(GetLastError());
    } else {
        SP_DEVINSTALL_PARAMS_W installParams{};
        installParams.cbSize = sizeof(installParams);
        if (SetupDiGetDeviceInstallParamsW(set, &data, &installParams)) {
            rebootRequired = (installParams.Flags & (DI_NEEDRESTART | DI_NEEDREBOOT)) != 0;
        }
    }
    SetupDiDestroyDeviceInfoList(set);
    return success;
}

void Refresh(HWND window) {
    auto* app = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (!app) return;

    std::wstring error;
    const auto found = FindTouchscreen(error);
    if (found) {
        app->device = *found;
        app->detail = found->name;
    } else {
        app->device = TouchDevice{};
        app->device.state = error.empty() ? TouchState::NotFound : TouchState::Error;
        app->detail = error.empty() ? L"Nessun touchscreen compatibile rilevato" : error;
    }

    const bool canToggle = app->device.state == TouchState::Enabled ||
                           app->device.state == TouchState::Disabled;
    EnableWindow(app->toggleButton, canToggle && !app->busy);
    SetWindowTextW(app->toggleButton,
                   app->device.state == TouchState::Enabled ? L"Disattiva touchscreen" :
                   app->device.state == TouchState::Disabled ? L"Attiva touchscreen" : L"Non disponibile");
    InvalidateRect(window, nullptr, TRUE);
}

void DrawCenteredText(HDC dc, const std::wstring& text, RECT rect, HFONT font,
                      COLORREF color, UINT format = DT_CENTER | DT_VCENTER | DT_SINGLELINE) {
    SelectObject(dc, font);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, text.c_str(), -1, &rect, format);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* app = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        auto* state = new AppState();
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        state->backgroundBrush = CreateSolidBrush(RGB(246, 248, 251));
        state->titleFont = CreateFontW(-30, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        state->bodyFont = CreateFontW(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        state->buttonFont = CreateFontW(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        state->toggleButton = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE |
                                                  WS_TABSTOP | BS_OWNERDRAW,
                                              55, 230, 330, 58, window,
                                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToggleButtonId)), nullptr, nullptr);
        state->refreshButton = CreateWindowExW(0, L"BUTTON", L"Aggiorna stato",
                                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                               145, 306, 150, 34, window,
                                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRefreshButtonId)), nullptr, nullptr);
        SendMessageW(state->refreshButton, WM_SETFONT, reinterpret_cast<WPARAM>(state->bodyFont), TRUE);
        Refresh(window);
        return 0;
    }
    case WM_COMMAND:
        if (!app) break;
        if (LOWORD(wParam) == kRefreshButtonId) {
            Refresh(window);
            return 0;
        }
        if (LOWORD(wParam) == kToggleButtonId && !app->busy) {
            const bool enable = app->device.state == TouchState::Disabled;
            app->busy = true;
            EnableWindow(app->toggleButton, FALSE);
            SetWindowTextW(app->toggleButton, L"Operazione in corso...");
            UpdateWindow(window);

            bool rebootRequired = false;
            std::wstring error;
            if (!ChangeDeviceState(app->device.instanceId, enable, rebootRequired, error)) {
                MessageBoxW(window, (L"Impossibile modificare il touchscreen.\n\n" + error).c_str(),
                            kTitle, MB_OK | MB_ICONERROR);
            }
            app->busy = false;
            Refresh(window);
            if (rebootRequired) {
                MessageBoxW(window, L"La modifica richiede il riavvio di Windows.",
                            kTitle, MB_OK | MB_ICONINFORMATION);
            }
            return 0;
        }
        break;
    case WM_DRAWITEM: {
        if (!app) break;
        auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (item->CtlID != kToggleButtonId) break;
        const bool enabled = IsWindowEnabled(item->hwndItem) != FALSE;
        const bool pressed = (item->itemState & ODS_SELECTED) != 0;
        COLORREF color = enabled ? (app->device.state == TouchState::Enabled
                                        ? RGB(194, 53, 53) : RGB(31, 132, 83))
                                 : RGB(165, 171, 181);
        if (pressed && enabled) color = app->device.state == TouchState::Enabled
                                           ? RGB(166, 42, 42) : RGB(24, 108, 67);
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(item->hDC, &item->rcItem, brush);
        DeleteObject(brush);
        DrawCenteredText(item->hDC,
                         app->device.state == TouchState::Enabled ? L"Disattiva touchscreen" :
                         app->device.state == TouchState::Disabled ? L"Attiva touchscreen" : L"Non disponibile",
                         item->rcItem, app->buttonFont, RGB(255, 255, 255));
        if (item->itemState & ODS_FOCUS) {
            RECT focus = item->rcItem;
            InflateRect(&focus, -4, -4);
            DrawFocusRect(item->hDC, &focus);
        }
        return TRUE;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        FillRect(dc, &client, app ? app->backgroundBrush : GetSysColorBrush(COLOR_WINDOW));
        if (app) {
            RECT title{30, 25, client.right - 30, 70};
            DrawCenteredText(dc, L"Controllo touchscreen", title, app->titleFont, RGB(25, 35, 52));

            COLORREF statusColor = RGB(112, 119, 130);
            std::wstring status = L"ERRORE";
            if (app->device.state == TouchState::Enabled) {
                statusColor = RGB(31, 132, 83); status = L"ATTIVO";
            } else if (app->device.state == TouchState::Disabled) {
                statusColor = RGB(194, 53, 53); status = L"DISATTIVATO";
            } else if (app->device.state == TouchState::NotFound) {
                status = L"NON RILEVATO";
            }
            RECT pill{145, 91, 295, 132};
            HBRUSH pillBrush = CreateSolidBrush(statusColor);
            HPEN pen = CreatePen(PS_NULL, 0, statusColor);
            auto oldBrush = SelectObject(dc, pillBrush);
            auto oldPen = SelectObject(dc, pen);
            RoundRect(dc, pill.left, pill.top, pill.right, pill.bottom, 22, 22);
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(pen);
            DeleteObject(pillBrush);
            DrawCenteredText(dc, status, pill, app->buttonFont, RGB(255, 255, 255));

            RECT detail{40, 150, client.right - 40, 212};
            DrawCenteredText(dc, app->detail, detail, app->bodyFont, RGB(80, 89, 102),
                             DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
        }
        EndPaint(window, &paint);
        return 0;
    }
    case WM_CTLCOLORBTN:
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
    case WM_DESTROY:
        if (app) {
            DeleteObject(app->titleFont);
            DeleteObject(app->bodyFont);
            DeleteObject(app->buttonFont);
            DeleteObject(app->backgroundBrush);
            delete app;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(kAppIconId), IMAGE_ICON,
                                             0, 0, LR_DEFAULTSIZE | LR_SHARED));
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&wc)) return 1;

    constexpr int width = 456;
    constexpr int height = 405;
    RECT desktop{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &desktop, 0);
    HWND window = CreateWindowExW(WS_EX_TOPMOST, kWindowClass, kTitle,
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  desktop.left + (desktop.right - desktop.left - width) / 2,
                                  desktop.top + (desktop.bottom - desktop.top - height) / 2,
                                  width, height, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;

    const BOOL dark = TRUE;
    DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return static_cast<int>(message.wParam);
}
