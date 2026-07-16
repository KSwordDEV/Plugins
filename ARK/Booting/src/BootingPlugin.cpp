#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <shellapi.h>
#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr wchar_t kRootClass[] = L"KSwordBootingPluginRoot";
constexpr wchar_t kContentClass[] = L"KSwordBootingPluginContent";
constexpr wchar_t kExpectedSetupHash[] =
    L"EC429816372B68C890371CB22F4A453F79D165701C98579A9528155C2B422A76";
constexpr int kVirtualContentHeight = 785;
constexpr UINT kRefreshPreflight = WM_APP + 1;

enum ControlId : int {
    IDC_PREFLIGHT = 1001,
    IDC_REFRESH,
    IDC_PATH,
    IDC_BROWSE,
    IDC_PREVIEW,
    IDC_X,
    IDC_Y,
    IDC_ORIENTATION,
    IDC_RESOLUTION,
    IDC_LOG_ENABLED,
    IDC_ACK_RECOVERY,
    IDC_ACK_TPM,
    IDC_ACK_SECURE_BOOT,
    IDC_PHRASE,
    IDC_DRY_RUN,
    IDC_INSTALL,
    IDC_UNINSTALL,
    IDC_BOOT_LOG,
    IDC_OPEN_WORKDIR,
    IDC_OPEN_DOCS,
    IDC_OPEN_SHIM,
    IDC_OUTPUT,
};

struct Ui {
    HWND warning{};
    HWND content{};
    HWND preflightGroup{};
    HWND preflight{};
    HWND refresh{};
    HWND imageGroup{};
    HWND pathLabel{};
    HWND path{};
    HWND browse{};
    HWND preview{};
    HWND xLabel{};
    HWND xEdit{};
    HWND yLabel{};
    HWND yEdit{};
    HWND orientationLabel{};
    HWND orientation{};
    HWND resolutionLabel{};
    HWND resolution{};
    HWND logEnabled{};
    HWND ackGroup{};
    HWND ackRecovery{};
    HWND ackTpm{};
    HWND ackSecureBoot{};
    HWND phraseLabel{};
    HWND phrase{};
    HWND dryRun{};
    HWND install{};
    HWND uninstall{};
    HWND bootLog{};
    HWND openWorkDir{};
    HWND openDocs{};
    HWND openShim{};
    HWND outputGroup{};
    HWND output{};
};

struct Preflight {
    bool uefi{};
    bool admin{};
    bool secureBootKnown{};
    bool secureBoot{};
    bool bitLockerKnown{};
    bool bitLocker{};
    bool payloadOk{};
};

struct ProcessResult {
    bool started{};
    DWORD exitCode{static_cast<DWORD>(-1)};
    std::string output;
    std::wstring error;
};

HINSTANCE g_instance{};
HWND g_root{};
HWND g_hostParent{};
DWORD g_hostPid{};
Ui g_ui;
Preflight g_preflight;
HFONT g_font{};
HFONT g_warningFont{};
HBRUSH g_warningBrush{};
ULONG_PTR g_gdiplusToken{};
std::unique_ptr<Gdiplus::Image> g_previewImage;
fs::path g_payloadDir;
fs::path g_workDir;
fs::path g_selectedImage;
int g_scrollPos{};
bool g_busy{};

struct PluginTheme {
    bool dark{};
    COLORREF window{RGB(243, 246, 250)};
    COLORREF surface{RGB(255, 255, 255)};
    COLORREF surfaceAlt{RGB(238, 243, 248)};
    COLORREF textPrimary{RGB(24, 34, 46)};
    COLORREF textSecondary{RGB(77, 91, 108)};
    COLORREF border{RGB(190, 201, 214)};
    COLORREF accent{RGB(37, 139, 230)};
    COLORREF onAccent{RGB(255, 255, 255)};
    HBRUSH windowBrush{};
    HBRUSH surfaceBrush{};
    HBRUSH surfaceAltBrush{};
};

PluginTheme g_theme;

std::wstring FormatWin32Error(DWORD code) {
    wchar_t* raw = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    FormatMessageW(flags, nullptr, code, 0, reinterpret_cast<wchar_t*>(&raw), 0, nullptr);
    std::wstring text = raw ? raw : L"unknown error";
    if (raw) {
        LocalFree(raw);
    }
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n')) {
        text.pop_back();
    }
    return text;
}

fs::path ModuleDirectory() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    return fs::path(std::wstring(buffer.data(), length)).parent_path();
}

fs::path LocalAppDataDirectory() {
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &raw))) {
        return {};
    }
    fs::path result(raw);
    CoTaskMemFree(raw);
    return result;
}

std::wstring Lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return value;
}

std::wstring EnvironmentValue(const wchar_t* name) {
    const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
    if (required <= 1) return {};
    std::wstring value(required - 1, L'\0');
    GetEnvironmentVariableW(name, value.data(), required);
    return value;
}

COLORREF ParseEnvironmentColor(const wchar_t* name, COLORREF fallback) {
    const std::wstring value = EnvironmentValue(name);
    if (value.size() != 7 || value.front() != L'#') return fallback;
    wchar_t* end = nullptr;
    const unsigned long rgb = wcstoul(value.c_str() + 1, &end, 16);
    if (!end || *end != L'\0') return fallback;
    return RGB((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
}

void InitializeTheme() {
    g_theme.dark = Lower(EnvironmentValue(L"KSWORD_PLUGIN_THEME")) == L"dark";
    if (g_theme.dark) {
        g_theme.window = RGB(13, 23, 34);
        g_theme.surface = RGB(16, 28, 41);
        g_theme.surfaceAlt = RGB(20, 36, 52);
        g_theme.textPrimary = RGB(232, 240, 248);
        g_theme.textSecondary = RGB(167, 184, 201);
        g_theme.border = RGB(54, 79, 103);
        g_theme.accent = RGB(54, 151, 241);
    }
    g_theme.window = ParseEnvironmentColor(L"KSWORD_PLUGIN_COLOR_WINDOW", g_theme.window);
    g_theme.surface = ParseEnvironmentColor(L"KSWORD_PLUGIN_COLOR_SURFACE", g_theme.surface);
    g_theme.surfaceAlt =
        ParseEnvironmentColor(L"KSWORD_PLUGIN_COLOR_SURFACE_ALT", g_theme.surfaceAlt);
    g_theme.textPrimary =
        ParseEnvironmentColor(L"KSWORD_PLUGIN_COLOR_TEXT_PRIMARY", g_theme.textPrimary);
    g_theme.textSecondary =
        ParseEnvironmentColor(L"KSWORD_PLUGIN_COLOR_TEXT_SECONDARY", g_theme.textSecondary);
    g_theme.border = ParseEnvironmentColor(L"KSWORD_PLUGIN_COLOR_BORDER", g_theme.border);
    g_theme.accent = ParseEnvironmentColor(L"KSWORD_PLUGIN_COLOR_ACCENT", g_theme.accent);
    g_theme.onAccent =
        ParseEnvironmentColor(L"KSWORD_PLUGIN_COLOR_ON_ACCENT", g_theme.onAccent);
    g_theme.windowBrush = CreateSolidBrush(g_theme.window);
    g_theme.surfaceBrush = CreateSolidBrush(g_theme.surface);
    g_theme.surfaceAltBrush = CreateSolidBrush(g_theme.surfaceAlt);
}

void DestroyTheme() {
    if (g_theme.windowBrush) DeleteObject(g_theme.windowBrush);
    if (g_theme.surfaceBrush) DeleteObject(g_theme.surfaceBrush);
    if (g_theme.surfaceAltBrush) DeleteObject(g_theme.surfaceAltBrush);
}

std::wstring ToWide(const std::string& bytes) {
    if (bytes.empty()) {
        return {};
    }
    auto convert = [&](UINT codePage, DWORD flags) -> std::wstring {
        const int count = MultiByteToWideChar(codePage, flags, bytes.data(),
                                               static_cast<int>(bytes.size()), nullptr, 0);
        if (count <= 0) {
            return {};
        }
        std::wstring result(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(codePage, flags, bytes.data(), static_cast<int>(bytes.size()),
                            result.data(), count);
        return result;
    };
    std::wstring result = convert(CP_UTF8, MB_ERR_INVALID_CHARS);
    if (result.empty()) {
        result = convert(CP_OEMCP, 0);
    }
    return result;
}

void SetControlFont(HWND control, HFONT font = nullptr) {
    if (control) {
        SendMessageW(control, WM_SETFONT,
                     reinterpret_cast<WPARAM>(font ? font : g_font), TRUE);
        SetWindowTheme(control, g_theme.dark ? L"" : L"Explorer", nullptr);
    }
}

LRESULT ControlColor(UINT message, WPARAM wParam, LPARAM lParam, HWND warning = nullptr) {
    HDC dc = reinterpret_cast<HDC>(wParam);
    HWND control = reinterpret_cast<HWND>(lParam);
    if (control == warning) {
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkColor(dc, RGB(150, 18, 24));
        return reinterpret_cast<LRESULT>(g_warningBrush);
    }

    SetTextColor(dc, IsWindowEnabled(control) ? g_theme.textPrimary : g_theme.textSecondary);
    if (message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX) {
        SetBkMode(dc, OPAQUE);
        SetBkColor(dc, g_theme.surfaceAlt);
        return reinterpret_cast<LRESULT>(g_theme.surfaceAltBrush);
    }
    SetBkMode(dc, TRANSPARENT);
    SetBkColor(dc, g_theme.surface);
    return reinterpret_cast<LRESULT>(g_theme.surfaceBrush);
}

HWND CreateControl(HWND parent, const wchar_t* className, const wchar_t* text,
                   DWORD style, int id, DWORD exStyle = 0) {
    wchar_t parentClass[64]{};
    GetClassNameW(parent, parentClass, static_cast<int>(std::size(parentClass)));
    if (wcscmp(parentClass, kContentClass) == 0) {
        if (wcscmp(className, L"STATIC") == 0 && (style & SS_TYPEMASK) != SS_OWNERDRAW) {
            exStyle |= WS_EX_TRANSPARENT;
        } else if (wcscmp(className, L"BUTTON") == 0) {
            const DWORD buttonType = style & BS_TYPEMASK;
            if (buttonType == BS_GROUPBOX || buttonType == BS_AUTOCHECKBOX) {
                exStyle |= WS_EX_TRANSPARENT;
            }
        }
    }
    HWND control = CreateWindowExW(exStyle, className, text, style | WS_CHILD | WS_VISIBLE,
                                   0, 0, 10, 10, parent,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   g_instance, nullptr);
    SetControlFont(control);
    return control;
}

void Move(HWND control, int x, int y, int width, int height, int scroll = 0) {
    if (control) {
        MoveWindow(control, x, y - scroll, std::max(1, width), std::max(1, height), TRUE);
    }
}

void AppendOutput(const std::wstring& text) {
    if (!g_ui.output) {
        return;
    }
    const int length = GetWindowTextLengthW(g_ui.output);
    SendMessageW(g_ui.output, EM_SETSEL, length, length);
    SendMessageW(g_ui.output, EM_REPLACESEL, FALSE,
                 reinterpret_cast<LPARAM>(text.c_str()));
    SendMessageW(g_ui.output, EM_SCROLLCARET, 0, 0);
}

bool IsAdministrator() {
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = nullptr;
    BOOL member = FALSE;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                 &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &member);
        FreeSid(adminGroup);
    }
    return member == TRUE;
}

bool ReadSecureBoot(bool& enabled) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State",
        L"UEFISecureBootEnabled", RRF_RT_REG_DWORD, nullptr, &value, &size);
    if (status != ERROR_SUCCESS) {
        return false;
    }
    enabled = value != 0;
    return true;
}

std::wstring Sha256(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectLength = 0;
    DWORD hashLength = 0;
    DWORD received = 0;
    std::vector<UCHAR> object;
    std::vector<UCHAR> digest;
    std::wstring result;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                          reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength),
                          &received, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
                          reinterpret_cast<PUCHAR>(&hashLength), sizeof(hashLength),
                          &received, 0) < 0) {
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    object.resize(objectLength);
    digest.resize(hashLength);
    if (BCryptCreateHash(algorithm, &hash, object.data(), objectLength,
                         nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    std::array<char, 64 * 1024> buffer{};
    while (stream) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = stream.gcount();
        if (count > 0 && BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()),
                                        static_cast<ULONG>(count), 0) < 0) {
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return {};
        }
    }

    if (BCryptFinishHash(hash, digest.data(), hashLength, 0) >= 0) {
        std::wostringstream formatted;
        formatted << std::uppercase << std::hex << std::setfill(L'0');
        for (const UCHAR byte : digest) {
            formatted << std::setw(2) << static_cast<unsigned>(byte);
        }
        result = formatted.str();
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return result;
}

void PumpMessages() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            PostQuitMessage(static_cast<int>(message.wParam));
            return;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

ProcessResult RunProcess(const fs::path& executable, const std::wstring& arguments,
                         const fs::path& workingDirectory) {
    ProcessResult result;
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
        result.error = FormatWin32Error(GetLastError());
        return result;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    HANDLE nullInput = CreateFileW(L"NUL", GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (nullInput == INVALID_HANDLE_VALUE) {
        result.error = FormatWin32Error(GetLastError());
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        return result;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    startup.hStdInput = nullInput;

    PROCESS_INFORMATION process{};
    std::wstring commandLine = L"\"" + executable.wstring() + L"\" " + arguments;
    const std::wstring cwd = workingDirectory.wstring();
    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, cwd.empty() ? nullptr : cwd.c_str(),
                        &startup, &process)) {
        result.error = FormatWin32Error(GetLastError());
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        CloseHandle(nullInput);
        return result;
    }
    result.started = true;
    CloseHandle(writePipe);
    CloseHandle(nullInput);
    writePipe = nullptr;

    auto readAvailable = [&]() {
        for (;;) {
            DWORD available = 0;
            if (!PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr) ||
                available == 0) {
                break;
            }
            std::vector<char> chunk(std::min<DWORD>(available, 32 * 1024));
            DWORD read = 0;
            if (!ReadFile(readPipe, chunk.data(), static_cast<DWORD>(chunk.size()), &read,
                          nullptr) || read == 0) {
                break;
            }
            result.output.append(chunk.data(), read);
        }
    };

    while (WaitForSingleObject(process.hProcess, 50) == WAIT_TIMEOUT) {
        readAvailable();
        PumpMessages();
    }
    readAvailable();
    GetExitCodeProcess(process.hProcess, &result.exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(readPipe);
    return result;
}

std::wstring SystemDrive() {
    wchar_t windowsDirectory[MAX_PATH]{};
    const UINT count = GetWindowsDirectoryW(windowsDirectory, MAX_PATH);
    if (count >= 2 && windowsDirectory[1] == L':') {
        return std::wstring(windowsDirectory, 2);
    }
    return L"C:";
}

void SetBusy(bool busy) {
    g_busy = busy;
    const std::array<HWND, 8> actions = {
        g_ui.refresh, g_ui.browse, g_ui.dryRun, g_ui.install,
        g_ui.uninstall, g_ui.bootLog, g_ui.openWorkDir, g_ui.phrase,
    };
    for (HWND action : actions) {
        if (action) EnableWindow(action, !busy);
    }
}

bool ButtonChecked(HWND button) {
    return SendMessageW(button, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

std::wstring ControlText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    if (length > 0) {
        GetWindowTextW(control, text.data(), length + 1);
    }
    text.resize(static_cast<size_t>(length));
    return text;
}

bool ParseInteger(const std::wstring& value, int& parsed) {
    if (value.empty()) return false;
    wchar_t* end = nullptr;
    const long number = wcstol(value.c_str(), &end, 10);
    if (!end || *end != L'\0' || number < -1 || number > 16384) return false;
    parsed = static_cast<int>(number);
    return true;
}

bool ValidPosition(const std::wstring& value) {
    if (value.empty() || value.size() > 24) return false;
    wchar_t* end = nullptr;
    const double number = wcstod(value.c_str(), &end);
    return end && *end == L'\0' && number >= -999999.0 && number <= 999999.0;
}

bool ValidResolution(const std::wstring& value) {
    const size_t delimiter = value.find(L'x');
    if (delimiter == std::wstring::npos || value.find(L'x', delimiter + 1) != std::wstring::npos) {
        return false;
    }
    int width = 0;
    int height = 0;
    if (!ParseInteger(value.substr(0, delimiter), width) ||
        !ParseInteger(value.substr(delimiter + 1), height)) {
        return false;
    }
    return (width == 0 && height == 0) || (width == -1 && height == -1) ||
           (width > 0 && height > 0);
}

void UpdatePreview(const fs::path& path) {
    g_previewImage.reset();
    if (!path.empty() && fs::exists(path)) {
        auto image = std::make_unique<Gdiplus::Image>(path.c_str(), FALSE);
        if (image->GetLastStatus() == Gdiplus::Ok) {
            g_previewImage = std::move(image);
        }
    }
    if (g_ui.preview) InvalidateRect(g_ui.preview, nullptr, TRUE);
}

bool SelectImage() {
    wchar_t path[32768]{};
    if (!g_selectedImage.empty()) {
        wcsncpy_s(path, g_selectedImage.c_str(), _TRUNCATE);
    }
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_root;
    dialog.lpstrFilter = L"Supported images (*.bmp;*.png;*.jpg;*.jpeg;*.gif)\0"
                         L"*.bmp;*.png;*.jpg;*.jpeg;*.gif\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = static_cast<DWORD>(std::size(path));
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) {
        return false;
    }
    const std::wstring extension = Lower(fs::path(path).extension().wstring());
    if (extension != L".bmp" && extension != L".png" && extension != L".jpg" &&
        extension != L".jpeg" && extension != L".gif") {
        MessageBoxW(g_root, L"仅支持 BMP、PNG、JPEG、GIF。", L"Booting", MB_ICONWARNING);
        return false;
    }
    g_selectedImage = path;
    SetWindowTextW(g_ui.path, g_selectedImage.c_str());
    UpdatePreview(g_selectedImage);
    return true;
}

bool CopyPayloadToWorkDir(std::wstring& error) {
    if (!g_preflight.payloadOk) {
        error = L"官方 HackBGRT payload 未通过 SHA-256 校验。";
        return false;
    }
    const fs::path local = LocalAppDataDirectory();
    if (local.empty()) {
        error = L"无法定位 LocalAppData。";
        return false;
    }
    g_workDir = local / L"KSword" / L"Plugins" / L"booting" / L"HackBGRT-2.6.0";
    std::error_code ec;
    fs::create_directories(g_workDir, ec);
    if (ec) {
        error = L"无法创建工作目录：" + ToWide(ec.message());
        return false;
    }
    fs::copy(g_payloadDir, g_workDir,
             fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) {
        error = L"无法复制 HackBGRT 工作副本。";
        return false;
    }
    if (Sha256(g_workDir / L"setup.exe") != kExpectedSetupHash) {
        error = L"工作副本 setup.exe 的 SHA-256 不匹配；已拒绝执行。";
        return false;
    }
    return true;
}

bool WriteConfiguration(std::wstring& error) {
    const std::wstring x = ControlText(g_ui.xEdit);
    const std::wstring y = ControlText(g_ui.yEdit);
    const std::wstring resolution = ControlText(g_ui.resolution);
    if (!ValidPosition(x) || !ValidPosition(y)) {
        error = L"X/Y 必须是 -999999 到 999999 之间的数字。";
        return false;
    }
    if (!ValidResolution(resolution)) {
        error = L"分辨率必须为 0x0、-1x-1 或正整数 WIDTHxHEIGHT。";
        return false;
    }
    if (g_selectedImage.empty() || !fs::is_regular_file(g_selectedImage)) {
        error = L"请选择一个存在的启动图片。";
        return false;
    }

    std::wstring orientation(16, L'\0');
    const LRESULT selected = SendMessageW(g_ui.orientation, CB_GETCURSEL, 0, 0);
    if (selected == CB_ERR) {
        error = L"请选择屏幕方向。";
        return false;
    }
    const LRESULT copied = SendMessageW(g_ui.orientation, CB_GETLBTEXT,
                                        static_cast<WPARAM>(selected),
                                        reinterpret_cast<LPARAM>(orientation.data()));
    if (copied == CB_ERR) {
        error = L"无法读取屏幕方向。";
        return false;
    }
    orientation.resize(static_cast<size_t>(copied));

    std::wstring extension = Lower(g_selectedImage.extension().wstring());
    if (extension == L".jpeg") extension = L".jpg";
    const fs::path stagedImage = g_workDir / (L"ksword-logo" + extension);
    std::error_code ec;
    fs::copy_file(g_selectedImage, stagedImage, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        error = L"无法复制选定图片到隔离工作目录。";
        return false;
    }

    auto ascii = [](const std::wstring& value) {
        std::string result;
        result.reserve(value.size());
        for (const wchar_t ch : value) {
            result.push_back(static_cast<char>(ch));
        }
        return result;
    };
    std::ofstream config(g_workDir / L"config.txt", std::ios::binary | std::ios::trunc);
    if (!config) {
        error = L"无法写入 config.txt。";
        return false;
    }
    config << "# Generated by KSword Booting Tab Plugin.\r\n"
           << "# Uses the standard bcdedit entry path; never overwrites Windows Boot Manager.\r\n"
           << "boot=MS\r\n"
           << "image= x=" << ascii(x) << " y=" << ascii(y)
           << " o=" << ascii(orientation) << " path=" << stagedImage.filename().string() << "\r\n"
           << "resolution=" << ascii(resolution) << "\r\n"
           << "log=" << (ButtonChecked(g_ui.logEnabled) ? "1" : "0") << "\r\n"
           << "debug=0\r\n";
    if (!config) {
        error = L"写入 config.txt 失败。";
        return false;
    }
    return true;
}

bool PrepareWorkspace(std::wstring& error) {
    return CopyPayloadToWorkDir(error) && WriteConfiguration(error);
}

void LogProcessResult(const std::wstring& title, const ProcessResult& result) {
    AppendOutput(L"\r\n=== " + title + L" ===\r\n");
    if (!result.started) {
        AppendOutput(L"启动失败：" + result.error + L"\r\n");
        return;
    }
    AppendOutput(ToWide(result.output));
    AppendOutput(L"\r\nExit code: " + std::to_wstring(result.exitCode) + L"\r\n");
}

std::wstring RiskOverrideArguments() {
    std::wstring result;
    if (g_preflight.secureBootKnown && g_preflight.secureBoot) {
        result += L" allow-secure-boot";
    }
    if (g_preflight.bitLockerKnown && g_preflight.bitLocker) {
        result += L" allow-bitlocker";
    }
    return result;
}

bool PreconditionsForMutation() {
    if (!g_preflight.uefi || !g_preflight.admin || !g_preflight.payloadOk) {
        MessageBoxW(g_root,
                    L"拒绝执行：必须同时满足 UEFI、管理员权限和官方 setup.exe SHA-256 校验。\n"
                    L"请刷新预检并处理红色项目。",
                    L"Booting 安全门", MB_ICONERROR);
        return false;
    }
    return true;
}

bool AllAcknowledgementsChecked() {
    return ButtonChecked(g_ui.ackRecovery) && ButtonChecked(g_ui.ackTpm) &&
           ButtonChecked(g_ui.ackSecureBoot);
}

void ExecuteDryRun() {
    std::wstring error;
    if (!g_preflight.uefi || !g_preflight.payloadOk) {
        MessageBoxW(g_root, L"Dry-run 仍要求 UEFI 和官方 payload 校验通过。",
                    L"Booting", MB_ICONWARNING);
        return;
    }
    SetBusy(true);
    if (!PrepareWorkspace(error)) {
        SetBusy(false);
        MessageBoxW(g_root, error.c_str(), L"Booting", MB_ICONERROR);
        return;
    }
    const std::wstring args = L"batch dry-run" + RiskOverrideArguments() +
                              L" disable install enable-bcdedit";
    const ProcessResult result = RunProcess(g_workDir / L"setup.exe", args, g_workDir);
    LogProcessResult(L"Dry-run（不写入 EFI/NVRAM）", result);
    SetBusy(false);
}

void ExecuteInstall() {
    if (!PreconditionsForMutation()) return;
    if (!AllAcknowledgementsChecked()) {
        MessageBoxW(g_root, L"必须逐项勾选三项风险确认。", L"Booting 安全门", MB_ICONWARNING);
        return;
    }
    if (ControlText(g_ui.phrase) != L"HACKBGRT") {
        MessageBoxW(g_root, L"要安装/更新，请在确认框准确输入 HACKBGRT。",
                    L"Booting 安全门", MB_ICONWARNING);
        return;
    }
    const int confirmation = MessageBoxW(
        g_root,
        L"最后确认：即将修改 UEFI 启动项。错误或中断可能导致系统无法启动。\n\n"
        L"已选择安全的 bcdedit 启动项方案；不会覆盖 Windows Boot Manager。\n"
        L"Secure Boot 开启时，重启后仍必须手动完成 shim/MOK 信任。\n\n"
        L"是否继续？",
        L"危险操作：安装 HackBGRT", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
    if (confirmation != IDYES) return;

    std::wstring error;
    SetBusy(true);
    if (!PrepareWorkspace(error)) {
        SetBusy(false);
        MessageBoxW(g_root, error.c_str(), L"Booting", MB_ICONERROR);
        return;
    }
    const std::wstring args = L"batch" + RiskOverrideArguments() +
                              L" disable install enable-bcdedit";
    AppendOutput(L"\r\n注意：请勿关机或强制结束进程。\r\n");
    const ProcessResult result = RunProcess(g_workDir / L"setup.exe", args, g_workDir);
    LogProcessResult(L"Install / Update", result);
    SetBusy(false);
    if (result.started && result.exitCode == 0 && g_preflight.secureBoot) {
        MessageBoxW(g_root,
                    L"安装命令已完成。Secure Boot 已开启：下次启动必须按照 shim.md 手动完成 MOK 信任。\n"
                    L"请勿忽略 Verification failed / Security Violation 页面。",
                    L"重启前必读", MB_ICONWARNING);
    }
}

void ExecuteUninstall() {
    if (!PreconditionsForMutation()) return;
    if (ControlText(g_ui.phrase) != L"UNINSTALL") {
        MessageBoxW(g_root, L"要卸载，请在确认框准确输入 UNINSTALL。",
                    L"Booting 安全门", MB_ICONWARNING);
        return;
    }
    if (MessageBoxW(g_root,
                    L"将调用官方 setup.exe batch uninstall，删除 HackBGRT 启动项和文件。\n"
                    L"操作期间请勿关机。是否继续？",
                    L"卸载 HackBGRT", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES) {
        return;
    }
    std::wstring error;
    SetBusy(true);
    if (!CopyPayloadToWorkDir(error)) {
        SetBusy(false);
        MessageBoxW(g_root, error.c_str(), L"Booting", MB_ICONERROR);
        return;
    }
    const std::wstring args = L"batch" + RiskOverrideArguments() + L" uninstall";
    const ProcessResult result = RunProcess(g_workDir / L"setup.exe", args, g_workDir);
    LogProcessResult(L"Uninstall", result);
    SetBusy(false);
}

void ShowBootLog() {
    std::wstring error;
    SetBusy(true);
    if (!CopyPayloadToWorkDir(error)) {
        SetBusy(false);
        MessageBoxW(g_root, error.c_str(), L"Booting", MB_ICONERROR);
        return;
    }
    const ProcessResult result = RunProcess(g_workDir / L"setup.exe",
                                            L"batch show-boot-log", g_workDir);
    LogProcessResult(L"HackBGRT boot log", result);
    SetBusy(false);
}

void RefreshPreflight() {
    SetBusy(true);
    g_preflight = {};
    FIRMWARE_TYPE firmware = FirmwareTypeUnknown;
    g_preflight.uefi = GetFirmwareType(&firmware) && firmware == FirmwareTypeUefi;
    g_preflight.admin = IsAdministrator();
    g_preflight.secureBootKnown = ReadSecureBoot(g_preflight.secureBoot);
    g_preflight.payloadOk =
        Sha256(g_payloadDir / L"setup.exe") == kExpectedSetupHash;

    wchar_t systemRoot[MAX_PATH]{};
    GetSystemDirectoryW(systemRoot, MAX_PATH);
    const fs::path manageBde = fs::path(systemRoot) / L"manage-bde.exe";
    if (fs::exists(manageBde)) {
        const ProcessResult bitLocker = RunProcess(manageBde,
                                                   L"-status " + SystemDrive(),
                                                   manageBde.parent_path());
        if (bitLocker.started && bitLocker.exitCode == 0) {
            const std::wstring output = Lower(ToWide(bitLocker.output));
            g_preflight.bitLockerKnown = true;
            if (output.find(L"protection off") != std::wstring::npos ||
                output.find(L"保护已关闭") != std::wstring::npos) {
                g_preflight.bitLocker = false;
            } else if (output.find(L"protection on") != std::wstring::npos ||
                       output.find(L"保护已启用") != std::wstring::npos) {
                g_preflight.bitLocker = true;
            } else {
                g_preflight.bitLockerKnown = false;
            }
        }
    }

    auto mark = [](bool ok) { return ok ? L"[OK] " : L"[BLOCK] "; };
    std::wostringstream status;
    status << mark(g_preflight.uefi) << L"固件模式："
           << (g_preflight.uefi ? L"UEFI" : L"非 UEFI / 无法识别") << L"\r\n";
    status << mark(g_preflight.admin) << L"管理员权限："
           << (g_preflight.admin ? L"已具备" : L"缺失（安装/卸载将被拒绝）") << L"\r\n";
    status << (g_preflight.secureBootKnown ? L"[INFO] " : L"[WARN] ")
           << L"Secure Boot："
           << (g_preflight.secureBootKnown
                   ? (g_preflight.secureBoot ? L"开启；重启后必须手动完成 shim/MOK 信任"
                                              : L"关闭")
                   : L"无法读取") << L"\r\n";
    status << (g_preflight.bitLockerKnown ? L"[INFO] " : L"[WARN] ")
           << L"BitLocker："
           << (g_preflight.bitLockerKnown
                   ? (g_preflight.bitLocker ? L"保护已开启；先备份恢复密钥并暂停保护"
                                             : L"未检测到已开启的保护")
                   : L"无法可靠识别；官方安装器仍会复核") << L"\r\n";
    status << mark(g_preflight.payloadOk) << L"官方 HackBGRT 2.6.0 setup.exe SHA-256："
           << (g_preflight.payloadOk ? L"匹配" : L"不匹配或文件缺失");
    SetWindowTextW(g_ui.preflight, status.str().c_str());
    AppendOutput(L"预检已刷新。任何 [BLOCK] 项都会阻止修改。\r\n");
    SetBusy(false);
}

void OpenPath(const fs::path& path) {
    std::error_code ec;
    fs::create_directories(path, ec);
    ShellExecuteW(g_root, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void LayoutContent(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = std::max(360L, client.right - client.left);
    const int height = std::max(1L, client.bottom - client.top);
    const int s = g_scrollPos;
    const int margin = 10;

    SendMessageW(window, WM_SETREDRAW, FALSE, 0);
    Move(g_ui.preflightGroup, margin, 5, width - 2 * margin, 108, s);
    Move(g_ui.preflight, 24, 25, width - 176, 76, s);
    Move(g_ui.refresh, width - 142, 45, 118, 34, s);

    Move(g_ui.imageGroup, margin, 120, width - 2 * margin, 217, s);
    Move(g_ui.pathLabel, 24, 143, 58, 22, s);
    Move(g_ui.path, 82, 140, width - 212, 25, s);
    Move(g_ui.browse, width - 120, 139, 96, 27, s);
    const int previewWidth = std::min(260, std::max(190, width / 3));
    Move(g_ui.preview, 24, 174, previewWidth, 145, s);
    const int configX = 42 + previewWidth;
    const int editX = configX + 85;
    const int editWidth = std::max(90, width - editX - 26);
    Move(g_ui.xLabel, configX, 177, 80, 22, s);
    Move(g_ui.xEdit, editX, 174, editWidth, 25, s);
    Move(g_ui.yLabel, configX, 207, 80, 22, s);
    Move(g_ui.yEdit, editX, 204, editWidth, 25, s);
    Move(g_ui.orientationLabel, configX, 237, 80, 22, s);
    Move(g_ui.orientation, editX, 234, editWidth, 120, s);
    Move(g_ui.resolutionLabel, configX, 267, 80, 22, s);
    Move(g_ui.resolution, editX, 264, editWidth, 25, s);
    Move(g_ui.logEnabled, configX, 297, editWidth + 85, 22, s);

    Move(g_ui.ackGroup, margin, 344, width - 2 * margin, 202, s);
    Move(g_ui.ackRecovery, 24, 367, width - 48, 37, s);
    Move(g_ui.ackTpm, 24, 407, width - 48, 37, s);
    Move(g_ui.ackSecureBoot, 24, 447, width - 48, 40, s);
    Move(g_ui.phraseLabel, 24, 500, width - 255, 25, s);
    Move(g_ui.phrase, width - 220, 496, 196, 27, s);

    const int gap = 7;
    const int buttonWidth = std::max(88, (width - 2 * margin - 3 * gap) / 4);
    Move(g_ui.dryRun, margin, 555, buttonWidth, 30, s);
    Move(g_ui.install, margin + buttonWidth + gap, 555, buttonWidth, 30, s);
    Move(g_ui.uninstall, margin + 2 * (buttonWidth + gap), 555, buttonWidth, 30, s);
    Move(g_ui.bootLog, margin + 3 * (buttonWidth + gap), 555, buttonWidth, 30, s);
    Move(g_ui.openWorkDir, margin, 592, buttonWidth, 30, s);
    Move(g_ui.openDocs, margin + buttonWidth + gap, 592, buttonWidth, 30, s);
    Move(g_ui.openShim, margin + 2 * (buttonWidth + gap), 592, buttonWidth, 30, s);

    Move(g_ui.outputGroup, margin, 631, width - 2 * margin, 145, s);
    Move(g_ui.output, 24, 652, width - 48, 111, s);

    SCROLLINFO info{sizeof(info), SIF_PAGE | SIF_RANGE | SIF_POS};
    info.nMin = 0;
    info.nMax = kVirtualContentHeight - 1;
    info.nPage = static_cast<UINT>(height);
    info.nPos = g_scrollPos;
    SetScrollInfo(window, SB_VERT, &info, TRUE);
    SendMessageW(window, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(window, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN |
                     RDW_UPDATENOW);
}

void ScrollContentTo(HWND window, int requestedPosition) {
    RECT client{};
    GetClientRect(window, &client);
    const int height = std::max(1L, client.bottom - client.top);
    const int maximum = std::max(0, kVirtualContentHeight - height);
    const int nextPosition = std::clamp(requestedPosition, 0, maximum);
    if (nextPosition == g_scrollPos) return;

    const int delta = g_scrollPos - nextPosition;
    g_scrollPos = nextPosition;
    SetScrollPos(window, SB_VERT, g_scrollPos, TRUE);
    ScrollWindowEx(window, 0, delta, nullptr, nullptr, nullptr, nullptr,
                   SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
    RedrawWindow(window, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN |
                     RDW_UPDATENOW);
}

void CreateContentControls(HWND window) {
    g_ui.preflightGroup = CreateControl(window, L"BUTTON", L"启动环境预检 / Preflight",
                                         BS_GROUPBOX, 0);
    g_ui.preflight = CreateControl(window, L"EDIT", L"正在等待预检……",
                                   ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL |
                                       WS_VSCROLL | WS_BORDER,
                                   IDC_PREFLIGHT, WS_EX_CLIENTEDGE);
    g_ui.refresh = CreateControl(window, L"BUTTON", L"刷新预检", BS_PUSHBUTTON, IDC_REFRESH);

    g_ui.imageGroup = CreateControl(window, L"BUTTON", L"启动图片和官方配置",
                                    BS_GROUPBOX, 0);
    g_ui.pathLabel = CreateControl(window, L"STATIC", L"图片：", SS_LEFT, 0);
    g_ui.path = CreateControl(window, L"EDIT", L"", ES_AUTOHSCROLL | ES_READONLY | WS_BORDER,
                              IDC_PATH, WS_EX_CLIENTEDGE);
    g_ui.browse = CreateControl(window, L"BUTTON", L"选择图片…", BS_PUSHBUTTON, IDC_BROWSE);
    g_ui.preview = CreateControl(window, L"STATIC", L"", SS_OWNERDRAW | WS_BORDER, IDC_PREVIEW,
                                 WS_EX_CLIENTEDGE);
    g_ui.xLabel = CreateControl(window, L"STATIC", L"X 位置：", SS_LEFT, 0);
    g_ui.xEdit = CreateControl(window, L"EDIT", L".5", ES_AUTOHSCROLL | WS_BORDER, IDC_X,
                               WS_EX_CLIENTEDGE);
    g_ui.yLabel = CreateControl(window, L"STATIC", L"Y 位置：", SS_LEFT, 0);
    g_ui.yEdit = CreateControl(window, L"EDIT", L".382", ES_AUTOHSCROLL | WS_BORDER, IDC_Y,
                               WS_EX_CLIENTEDGE);
    g_ui.orientationLabel = CreateControl(window, L"STATIC", L"方向：", SS_LEFT, 0);
    g_ui.orientation = CreateControl(window, L"COMBOBOX", L"",
                                     CBS_DROPDOWNLIST | WS_VSCROLL, IDC_ORIENTATION);
    for (const wchar_t* item : {L"keep", L"0", L"90", L"180", L"270"}) {
        SendMessageW(g_ui.orientation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
    }
    SendMessageW(g_ui.orientation, CB_SETCURSEL, 0, 0);
    g_ui.resolutionLabel = CreateControl(window, L"STATIC", L"分辨率：", SS_LEFT, 0);
    g_ui.resolution = CreateControl(window, L"EDIT", L"0x0", ES_AUTOHSCROLL | WS_BORDER,
                                    IDC_RESOLUTION, WS_EX_CLIENTEDGE);
    g_ui.logEnabled = CreateControl(window, L"BUTTON", L"启用 HackBGRT 启动日志（占用少量 RAM）",
                                    BS_AUTOCHECKBOX, IDC_LOG_ENABLED);
    SendMessageW(g_ui.logEnabled, BM_SETCHECK, BST_CHECKED, 0);

    g_ui.ackGroup = CreateControl(window, L"BUTTON", L"强制风险确认（安装/更新前必须全部勾选）",
                                  BS_GROUPBOX, 0);
    g_ui.ackRecovery = CreateControl(
        window, L"BUTTON",
        L"我已创建可启动恢复介质，并知道如何从固件启动菜单选择 Windows Boot Manager 恢复。",
        BS_AUTOCHECKBOX | BS_MULTILINE, IDC_ACK_RECOVERY);
    g_ui.ackTpm = CreateControl(
        window, L"BUTTON",
        L"我已备份 BitLocker 恢复密钥并暂停保护；理解 TPM/PIN/反作弊/磁盘加密可能失效。",
        BS_AUTOCHECKBOX | BS_MULTILINE, IDC_ACK_TPM);
    g_ui.ackSecureBoot = CreateControl(
        window, L"BUTTON",
        L"我理解 Secure Boot 步骤不能自动化；重启后会按 shim.md 手动完成 MOK 信任。",
        BS_AUTOCHECKBOX | BS_MULTILINE, IDC_ACK_SECURE_BOOT);
    g_ui.phraseLabel = CreateControl(
        window, L"STATIC",
        L"安装输入 HACKBGRT；卸载输入 UNINSTALL：", SS_LEFT, 0);
    g_ui.phrase = CreateControl(window, L"EDIT", L"", ES_AUTOHSCROLL | WS_BORDER, IDC_PHRASE,
                                WS_EX_CLIENTEDGE);

    g_ui.dryRun = CreateControl(window, L"BUTTON", L"Dry-run（不写入）", BS_PUSHBUTTON,
                                IDC_DRY_RUN);
    g_ui.install = CreateControl(window, L"BUTTON", L"安装 / 更新", BS_DEFPUSHBUTTON,
                                 IDC_INSTALL);
    g_ui.uninstall = CreateControl(window, L"BUTTON", L"卸载", BS_PUSHBUTTON, IDC_UNINSTALL);
    g_ui.bootLog = CreateControl(window, L"BUTTON", L"读取启动日志", BS_PUSHBUTTON,
                                 IDC_BOOT_LOG);
    g_ui.openWorkDir = CreateControl(window, L"BUTTON", L"打开隔离工作目录", BS_PUSHBUTTON,
                                     IDC_OPEN_WORKDIR);
    g_ui.openDocs = CreateControl(window, L"BUTTON", L"官方 README", BS_PUSHBUTTON,
                                  IDC_OPEN_DOCS);
    g_ui.openShim = CreateControl(window, L"BUTTON", L"Secure Boot / shim 指引", BS_PUSHBUTTON,
                                  IDC_OPEN_SHIM);
    g_ui.outputGroup = CreateControl(window, L"BUTTON", L"执行输出", BS_GROUPBOX, 0);
    g_ui.output = CreateControl(window, L"EDIT",
                                L"插件只调用官方 HackBGRT setup.exe batch 命令；禁用 overwrite/legacy 路径。\r\n",
                                ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL |
                                    WS_HSCROLL | WS_BORDER,
                                IDC_OUTPUT, WS_EX_CLIENTEDGE);

    g_selectedImage = g_payloadDir / L"splash.bmp";
    if (fs::exists(g_selectedImage)) {
        SetWindowTextW(g_ui.path, g_selectedImage.c_str());
        UpdatePreview(g_selectedImage);
    }
}

void DrawPreview(const DRAWITEMSTRUCT* item) {
    HBRUSH background = CreateSolidBrush(g_theme.surfaceAlt);
    FillRect(item->hDC, &item->rcItem, background);
    DeleteObject(background);
    if (!g_previewImage) {
        SetBkMode(item->hDC, TRANSPARENT);
        SetTextColor(item->hDC, g_theme.textSecondary);
        RECT textRect = item->rcItem;
        DrawTextW(item->hDC, L"无有效预览\nNo valid preview", -1, &textRect,
                  DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        return;
    }
    const UINT imageWidth = g_previewImage->GetWidth();
    const UINT imageHeight = g_previewImage->GetHeight();
    if (imageWidth == 0 || imageHeight == 0) return;
    const int availableWidth = std::max(1L, item->rcItem.right - item->rcItem.left - 8);
    const int availableHeight = std::max(1L, item->rcItem.bottom - item->rcItem.top - 8);
    const double scale = std::min(static_cast<double>(availableWidth) / imageWidth,
                                  static_cast<double>(availableHeight) / imageHeight);
    const int width = static_cast<int>(imageWidth * scale);
    const int height = static_cast<int>(imageHeight * scale);
    const int x = item->rcItem.left + (availableWidth - width) / 2 + 4;
    const int y = item->rcItem.top + (availableHeight - height) / 2 + 4;
    Gdiplus::Graphics graphics(item->hDC);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.DrawImage(g_previewImage.get(), x, y, width, height);
}

LRESULT CALLBACK ContentWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        SetWindowTheme(window, g_theme.dark ? L"" : L"Explorer", nullptr);
        CreateContentControls(window);
        return 0;
    case WM_SIZE:
        LayoutContent(window);
        return 0;
    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(window, &client);
        FillRect(reinterpret_cast<HDC>(wParam), &client, g_theme.surfaceBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORLISTBOX:
        return ControlColor(message, wParam, lParam);
    case WM_VSCROLL: {
        SCROLLINFO info{sizeof(info), SIF_ALL};
        GetScrollInfo(window, SB_VERT, &info);
        int position = info.nPos;
        switch (LOWORD(wParam)) {
        case SB_LINEUP: position -= 24; break;
        case SB_LINEDOWN: position += 24; break;
        case SB_PAGEUP: position -= static_cast<int>(info.nPage); break;
        case SB_PAGEDOWN: position += static_cast<int>(info.nPage); break;
        case SB_THUMBTRACK: position = info.nTrackPos; break;
        case SB_TOP: position = info.nMin; break;
        case SB_BOTTOM: position = info.nMax; break;
        default: return 0;
        }
        ScrollContentTo(window, position);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        SendMessageW(window, WM_VSCROLL,
                     MAKEWPARAM(delta > 0 ? SB_LINEUP : SB_LINEDOWN, 0), 0);
        return 0;
    }
    case kRefreshPreflight:
        RefreshPreflight();
        return 0;
    case WM_DRAWITEM:
        if (wParam == IDC_PREVIEW) {
            DrawPreview(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
            return TRUE;
        }
        break;
    case WM_COMMAND:
        if (HIWORD(wParam) != BN_CLICKED) break;
        switch (LOWORD(wParam)) {
        case IDC_REFRESH: RefreshPreflight(); return 0;
        case IDC_BROWSE: SelectImage(); return 0;
        case IDC_DRY_RUN: ExecuteDryRun(); return 0;
        case IDC_INSTALL: ExecuteInstall(); return 0;
        case IDC_UNINSTALL: ExecuteUninstall(); return 0;
        case IDC_BOOT_LOG: ShowBootLog(); return 0;
        case IDC_OPEN_WORKDIR:
            if (g_workDir.empty()) {
                g_workDir = LocalAppDataDirectory() / L"KSword" / L"Plugins" /
                            L"booting" / L"HackBGRT-2.6.0";
            }
            OpenPath(g_workDir);
            return 0;
        case IDC_OPEN_DOCS:
            ShellExecuteW(g_root, L"open",
                          L"https://github.com/Metabolix/HackBGRT/blob/master/README.md",
                          nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        case IDC_OPEN_SHIM:
            ShellExecuteW(g_root, L"open",
                          L"https://github.com/Metabolix/HackBGRT/blob/master/shim.md",
                          nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        default: break;
        }
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void LayoutRoot(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    MoveWindow(g_ui.warning, 8, 8, std::max(1, width - 16), 101, TRUE);
    MoveWindow(g_ui.content, 0, 116, std::max(1, width), std::max(1, height - 116), TRUE);
}

LRESULT CALLBACK RootWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        SetWindowTheme(window, g_theme.dark ? L"" : L"Explorer", nullptr);
        g_ui.warning = CreateControl(
            window, L"STATIC",
            L"危险 / DANGER：HackBGRT 会修改 UEFI 启动链；配置错误、断电或错误磁盘选择可能导致系统无法启动。\n"
            L"先创建恢复介质并备份 BitLocker 恢复密钥。TPM/PIN/反作弊/磁盘加密可能受影响；Secure Boot 需要重启后手动完成 shim/MOK 信任。\n"
            L"仅支持 UEFI，建议只连接一个可启动磁盘。原厂 Logo 短暂闪现属于预期现象。软件无担保，风险由使用者承担。",
            SS_LEFT | SS_EDITCONTROL, 0);
        SetControlFont(g_ui.warning, g_warningFont);
        SetWindowTheme(g_ui.warning, L"Explorer", nullptr);
        g_ui.content = CreateWindowExW(WS_EX_CONTROLPARENT, kContentClass, L"",
                                       WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL,
                                       0, 0, 10, 10, window, nullptr, g_instance, nullptr);
        SetTimer(window, 1, 1500, nullptr);
        return 0;
    case WM_SIZE:
        LayoutRoot(window);
        return 0;
    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(window, &client);
        FillRect(reinterpret_cast<HDC>(wParam), &client, g_theme.windowBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORLISTBOX:
        return ControlColor(message, wParam, lParam, g_ui.warning);
    case WM_TIMER:
        if (g_hostParent && (!IsWindow(g_hostParent) ||
                            GetWindowThreadProcessId(g_hostParent, nullptr) == 0)) {
            DestroyWindow(window);
        } else if (g_hostPid) {
            HANDLE host = OpenProcess(SYNCHRONIZE, FALSE, g_hostPid);
            if (!host || WaitForSingleObject(host, 0) == WAIT_OBJECT_0) {
                if (host) CloseHandle(host);
                DestroyWindow(window);
                return 0;
            }
            CloseHandle(host);
        }
        return 0;
    case WM_DESTROY:
        KillTimer(window, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool RegisterWindowClasses() {
    WNDCLASSEXW content{sizeof(content)};
    content.hInstance = g_instance;
    content.lpfnWndProc = ContentWindowProc;
    content.lpszClassName = kContentClass;
    content.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    content.hbrBackground = g_theme.surfaceBrush;
    if (!RegisterClassExW(&content)) return false;

    WNDCLASSEXW root{sizeof(root)};
    root.hInstance = g_instance;
    root.lpfnWndProc = RootWindowProc;
    root.lpszClassName = kRootClass;
    root.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    root.hbrBackground = g_theme.windowBrush;
    root.hIcon = LoadIconW(nullptr, IDI_WARNING);
    return RegisterClassExW(&root) != 0;
}

void WriteProtocolEvent(const std::string& event, HWND hwnd = nullptr,
                        const std::string& message = {}) {
    std::ostringstream json;
    json << "{\"protocol\":\"ksword-plugin/1\",\"plugin_id\":\"booting\",\"event\":\""
         << event << "\"";
    if (hwnd) {
        json << ",\"hwnd\":\"" << reinterpret_cast<uintptr_t>(hwnd) << "\"";
    }
    if (!message.empty()) {
        json << ",\"message\":\"" << message << "\"";
    }
    json << "}\n";
    const std::string line = json.str();
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output && output != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(output, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
        FlushFileBuffers(output);
    }
}

struct LaunchArguments {
    HWND parent{};
    DWORD hostPid{};
    bool standalone{};
    bool valid{};
};

LaunchArguments ParseArguments() {
    LaunchArguments result;
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) return result;
    if (count == 1) {
        result.standalone = true;
        result.valid = true;
        LocalFree(arguments);
        return result;
    }
    bool pluginCommand = false;
    for (int i = 1; i < count; ++i) {
        const std::wstring argument = arguments[i];
        if (argument == L"--ksword-plugin" && i + 1 < count) {
            pluginCommand = std::wstring(arguments[++i]) == L"tab";
        } else if (argument == L"--parent-hwnd" && i + 1 < count) {
            result.parent = reinterpret_cast<HWND>(
                static_cast<uintptr_t>(_wcstoui64(arguments[++i], nullptr, 10)));
        } else if (argument == L"--host-pid" && i + 1 < count) {
            result.hostPid = wcstoul(arguments[++i], nullptr, 10);
        } else if (argument == L"--standalone") {
            result.standalone = true;
        }
    }
    LocalFree(arguments);

    if (result.standalone) {
        result.valid = true;
        return result;
    }
    if (!pluginCommand || !result.parent || !IsWindow(result.parent) || result.hostPid == 0) {
        return result;
    }
    DWORD ownerPid = 0;
    GetWindowThreadProcessId(result.parent, &ownerPid);
    result.valid = ownerPid == result.hostPid;
    return result;
}

void CreateFonts() {
    NONCLIENTMETRICSW metrics{sizeof(metrics)};
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
        g_font = CreateFontIndirectW(&metrics.lfMessageFont);
        LOGFONTW warning = metrics.lfMessageFont;
        warning.lfWeight = FW_SEMIBOLD;
        g_warningFont = CreateFontIndirectW(&warning);
    }
    if (!g_font) g_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    if (!g_warningFont) g_warningFont = g_font;
}

} // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int showCommand) {
    g_instance = instance;
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    Gdiplus::GdiplusStartupInput gdiplusInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusInput, nullptr);
    InitializeTheme();
    CreateFonts();
    g_warningBrush = CreateSolidBrush(RGB(150, 18, 24));
    g_payloadDir = ModuleDirectory() / L"HackBGRT";

    const LaunchArguments launch = ParseArguments();
    if (!launch.valid) {
        WriteProtocolEvent("error", nullptr, "invalid parent HWND or host PID");
        return 2;
    }
    g_hostParent = launch.parent;
    g_hostPid = launch.hostPid;
    if (!RegisterWindowClasses()) {
        WriteProtocolEvent("error", nullptr, "window class registration failed");
        return 3;
    }

    DWORD style = launch.standalone
                      ? WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN
                      : WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
    g_root = CreateWindowExW(WS_EX_CONTROLPARENT, kRootClass, L"KSword Booting Tab Plugin",
                             style, CW_USEDEFAULT, CW_USEDEFAULT, 1080, 820,
                             launch.parent, nullptr, instance, nullptr);
    if (!g_root) {
        WriteProtocolEvent("error", nullptr, "root window creation failed");
        return 4;
    }

    if (launch.standalone) {
        ShowWindow(g_root, showCommand == 0 ? SW_SHOWNORMAL : showCommand);
        UpdateWindow(g_root);
    } else {
        WriteProtocolEvent("tab_ready", g_root);
    }
    PostMessageW(g_ui.content, kRefreshPreflight, 0, 0);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(g_root, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    g_previewImage.reset();
    if (g_warningBrush) DeleteObject(g_warningBrush);
    if (g_font && g_font != GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(g_font);
    if (g_warningFont && g_warningFont != g_font &&
        g_warningFont != GetStockObject(DEFAULT_GUI_FONT)) {
        DeleteObject(g_warningFont);
    }
    DestroyTheme();
    if (g_gdiplusToken) Gdiplus::GdiplusShutdown(g_gdiplusToken);
    return static_cast<int>(message.wParam);
}
