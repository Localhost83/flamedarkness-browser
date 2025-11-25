/* Premise: this WebView2 browser was mostly vibecoded by Copilot, with a little of help from Gemini and ChatGPT too. Afterwards, I added a bit of my customizations.
NEW additions: the annoying status bar has been removed, the built-in Adblocker has been updated to gain more filter lists, Quad9 DNS DoH has been added through browser flags, and Chromium's Enhanced Browsing has been activated through browser flags. The changes were meant to be easy but I had to fix issues with the previous source code which mixed Unicode with ANSI.*/
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>
#if defined(__has_include)
#  if __has_include("WebView2EnvironmentOptions.h")
#    include "WebView2EnvironmentOptions.h"
#    define HAVE_WEBVIEW2_ENV_OPTIONS 1
#  endif
#endif
#include <commctrl.h>
#include <commdlg.h> // For GetSaveFileName
#include <string>
#include <shlwapi.h>
#include <stdio.h> 
#include <algorithm> 
#include <fstream>   
#include <sstream>   
#include <vector>
#include <utility> 
#include <set>
#include <shellapi.h>
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")

// *** REQUIRED FOR COM/WEBVIEW2 INITIALIZATION ***
#include <objbase.h> 

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib") // For GetSaveFileName
#pragma comment(lib, "WebView2LoaderStatic.lib") 
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib") // Required for CoInitializeEx

// Enable Visual Styles
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace Microsoft::WRL;
using namespace std;

// --- HELPER: Convert WebView2 Wide Strings to ANSI ---
std::string WideToAnsi(LPWSTR wstr) {
    if (!wstr) return "";
    int size_needed = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, &str[0], size_needed, NULL, NULL);
    if (!str.empty() && str.back() == '\0') str.pop_back(); 
    return str;
}

// --- HELPER: Convert ANSI Strings to WebView2 Wide Strings ---
std::wstring AnsiToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

// --- 1. Constants and IDs ---
#define WINDOW_WIDTH 1300 
#define WINDOW_HEIGHT 700 
#define PADDING 5
#define BUTTON_WIDTH 60
#define BUTTON_HEIGHT 24
#define GO_BUTTON_WIDTH 60
#define TOOLBAR_HEIGHT (BUTTON_HEIGHT + 2 * PADDING) 
#define STATUSBAR_HEIGHT 24
#define STATUS_PARTS 3
#define DNS_BUTTON_WIDTH 120

// Control IDs (IDs are fixed)
#define IDC_URL_BAR 101
#define IDC_GO_BUTTON 102
#define IDC_BACK_BUTTON 103
#define IDC_FORWARD_BUTTON 104
#define IDC_REFRESH_BUTTON 105
#define IDC_HOME_BUTTON 106
#define IDC_STOP_BUTTON 107
#define IDC_HISTORY_BUTTON 109
#define IDC_ADBLOCK_BUTTON 110
#define IDC_SETTINGS_BUTTON 111

// Settings Window Control IDs
#define IDD_SETTINGS_DIALOG 300 
#define IDC_SETTINGS_HEADER 301
#define IDC_CHECK_BLOCK_POPUPS 302
#define IDC_CHECK_DNT 303
#define IDC_CHECK_CUSTOM_UA 304       
#define IDC_UA_LABEL 305              
#define IDC_UA_EDIT_BOX 306           
#define IDC_CHECK_TRACKING_PREVENTION 307  
#define IDC_STATIC_HISTORY_DISABLED 308    
// #define IDC_HELP_ABOUT 320

// Additional feature buttons
#define IDC_HELP_BUTTON 321
#define IDC_DNS_TEST_BUTTON 322

// NEW CHECKBOX IDs for togglable settings

#define IDC_CHECK_SCRIPT_ENABLED 309

#define IDC_CHECK_SCRIPT_DIALOGS 310

#define IDC_CHECK_PASSWORD_SAVE 311

#define IDC_CHECK_AUTOFILL 312



// NEW PERMISSION CHECKBOX IDs

#define IDC_CHECK_BLOCK_NOTIFICATIONS 313

#define IDC_CHECK_BLOCK_LOCATION 314

#define IDC_CHECK_BLOCK_WEBCAM 315

#define IDC_CHECK_BLOCK_MICROPHONE 316

#define IDC_CHECK_QUAD9_DOH 317

// NEW DOWNLOAD RADIO BUTTON IDs

#define IDC_RADIO_DOWNLOAD_ALLOW 318
#define IDC_RADIO_DOWNLOAD_BLOCK 319
#define IDC_RADIO_DOWNLOAD_PROMPT 320

// New: Force HTTPS option
#define IDC_CHECK_FORCE_HTTPS 324





// File Name for Persistence

#define SETTINGS_FILE_NAME "settings.cfg" 



// Menu IDs (IDs are fixed)

#define IDM_FILE_EXIT 204

#define IDM_HELP_ABOUT 207

// Ad-block menu commands
#define IDM_ADBLOCK_TOGGLE 400
#define IDM_ADBLOCK_RELOAD 401
#define IDM_ADBLOCK_LICENSE 402



// --- 2. Global Variables ---

HWND g_hwndMain = nullptr;

HWND g_hwndUrlBar = nullptr;



HWND g_hwndBackButton = nullptr;

HWND g_hwndForwardButton = nullptr;

HWND g_hwndGoButton = nullptr; 

HWND g_hwndStopButton = nullptr;

HWND g_hwndRefreshButton = nullptr;

HWND g_hwndHomeButton = nullptr;

HWND g_hwndHistoryButton = nullptr;

HWND g_hwndAdblockButton = nullptr;

HWND g_hwndSettingsButton = nullptr;


// Help and DNS buttons
HWND g_hwndHelpButton = nullptr;
HWND g_hwndDNSButton = nullptr;



ComPtr<ICoreWebView2> g_webView;

ComPtr<ICoreWebView2Controller> g_controller;



HBRUSH g_hbrWhiteBackground = nullptr;

HWND g_hwndSettings = nullptr;

HWND g_hwndHistory = nullptr; 

HWND g_hwndFavorites = nullptr; 



// Browser State Flags (Persisted in settings.cfg)

bool g_blockPopups = true; 

bool g_trackingPrevention = true; 

bool g_customUserAgentEnabled = true; 

bool g_enableTrackingPrevention = true;

bool g_DNS_DoH = true;

// Third-party cookie blocking toggle (requires newer WebView2 SDK/runtime)
// bool g_blockThirdPartyCookies = true;



// NEW TOGGLE STATES (Defaulting to current secure settings)

bool g_isScriptEnabled = true; // Scripts are enabled by default

bool g_areDefaultScriptDialogsEnabled = false; // Dialogs are disabled by default

bool g_isPasswordAutosaveEnabled = false; // Password save is disabled by default

bool g_isGeneralAutofillEnabled = false; // General autofill is disabled by default

// Force HTTPS navigation
bool g_forceHTTPS = true;



// NEW PERMISSION STATES

bool g_blockNotifications = true;

bool g_blockLocation = true;

bool g_blockWebcam = true;

bool g_blockMicrophone = true;



// NEW DOWNLOAD BEHAVIOR






string g_customUserAgentString = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) FlameDarkness/0.91";

// Track current navigation context
string g_currentUrl = "";
string g_currentTitle = "";

// Ad-blocker state and filters
bool g_adBlockEnabled = true;
std::vector<std::string> g_adblockSubstringFilters;
std::set<std::string> g_adblockHostFilters;

// --- 3. Function Declarations ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam); 

void CreateBrowserControls(HWND hwnd);
void LayoutToolbarControls(HWND hwnd, int windowWidth); 
void ResizeWebView();
void NavigateToUrl(const char* url);
void InitializeWebView2(HWND hwnd);
void ShowSettingsWindow(HWND hParent); 
void ShowHistoryWindow(HWND hParent);
void ShowFavoritesWindow(HWND hParent);
void ApplySettingsToWebView();
bool DownloadAndAggregateFilters(HWND hwnd);
void LoadAdBlockFilters();

void InitializeBrowserState(); 
void SaveFileSettings();    
void LoadFileSettings();    


// --- 4. Persistence Functions (File I/O for Settings) ---

void SaveFileSettings() {
    ofstream outFile(SETTINGS_FILE_NAME);
    if (outFile.is_open()) {
        outFile << "Popups_Blocked=" << (g_blockPopups ? 1 : 0) << "\n";
        outFile << "DNT_Enabled=" << (g_trackingPrevention ? 1 : 0) << "\n";
        outFile << "UA_Enabled=" << (g_customUserAgentEnabled ? 1 : 0) << "\n";
        outFile << "TrackingPrevention_Active=" << (g_enableTrackingPrevention ? 1 : 0) << "\n"; 
        outFile << "Custom_UserAgent=" << g_customUserAgentString << "\n";
        // NEW SETTINGS SAVED
        outFile << "Script_Enabled=" << (g_isScriptEnabled ? 1 : 0) << "\n";
        outFile << "ScriptDialogs_Enabled=" << (g_areDefaultScriptDialogsEnabled ? 1 : 0) << "\n";
        outFile << "PasswordAutosave_Enabled=" << (g_isPasswordAutosaveEnabled ? 1 : 0) << "\n";
        outFile << "GeneralAutofill_Enabled=" << (g_isGeneralAutofillEnabled ? 1 : 0) << "\n";
		// NEW PERMISSIONS
		outFile << "Block_Notifications=" << (g_blockNotifications ? 1 : 0) << "\n";
		outFile << "Block_Location=" << (g_blockLocation ? 1 : 0) << "\n";
		outFile << "Block_Webcam=" << (g_blockWebcam ? 1 : 0) << "\n";
		outFile << "Block_Microphone=" << (g_blockMicrophone ? 1 : 0) << "\n";
        outFile << "DNS_DoH=" << (g_DNS_DoH ? 1 : 0) << "\n";
        // NEW: Force HTTPS setting
        outFile << "Force_HTTPS=" << (g_forceHTTPS ? 1 : 0) << "\n";
    // Note: download behavior UI removed when download interception is not supported by current WebView2 headers
        // AD-BLOCKER SETTING
        outFile << "AdBlock_Enabled=" << (g_adBlockEnabled ? 1 : 0) << "\n";
        outFile.close();
    }
}

void LoadFileSettings() {
    ifstream inFile(SETTINGS_FILE_NAME);
    if (inFile.is_open()) {
        string line;
        while (getline(inFile, line)) {
            size_t pos = line.find('=');
            if (pos != string::npos) {
                string key = line.substr(0, pos);
                string value = line.substr(pos + 1);

                if (key == "Popups_Blocked") {
                    g_blockPopups = (value == "1");
                } else if (key == "DNT_Enabled") {
                    g_trackingPrevention = (value == "1");
                } else if (key == "UA_Enabled") {
                    g_customUserAgentEnabled = (value == "1");
                } else if (key == "TrackingPrevention_Active") {
                    g_enableTrackingPrevention = (value == "1");
                } else if (key == "Custom_UserAgent") {
                    g_customUserAgentString = value;
                }
                // NEW SETTINGS LOADED
                else if (key == "Script_Enabled") {
                    g_isScriptEnabled = (value == "1");
                } else if (key == "ScriptDialogs_Enabled") {
                    g_areDefaultScriptDialogsEnabled = (value == "1");
                } else if (key == "PasswordAutosave_Enabled") {
                    g_isPasswordAutosaveEnabled = (value == "1");
                } else if (key == "GeneralAutofill_Enabled") {
                    g_isGeneralAutofillEnabled = (value == "1");
                } else if (key == "AdBlock_Enabled") {
                    g_adBlockEnabled = (value == "1");
                }
				// NEW PERMISSIONS
				else if (key == "Block_Notifications") {
					g_blockNotifications = (value == "1");
				} else if (key == "Block_Location") {
					g_blockLocation = (value == "1");
				} else if (key == "Block_Webcam") {
					g_blockWebcam = (value == "1");
				} else if (key == "Block_Microphone") {
					g_blockMicrophone = (value == "1");
				}
                // NEW: Force HTTPS setting
                else if (key == "Force_HTTPS") {
                    g_forceHTTPS = (value == "1");
                }
            }
        }
        inFile.close();
    } 
}

void DeleteCookiesOnExit()
{
    if (g_webView) {
        // Use DevTools Protocol command to clear all browser cookies.
        // This is sent as a 'fire-and-forget' command, bypassing the need 
        // for an asynchronous ICoreWebView2CompletedHandler on exit.
        LPCWSTR command = L"Network.clearBrowserCookies";
        LPCWSTR parameters = L"{}"; 
        
        // This method is available on the base ICoreWebView2 interface.
        // The last argument is the completion handler, which we pass as nullptr.
        g_webView->CallDevToolsProtocolMethod(command, parameters, nullptr);
        ApplySettingsToWebView();
    }
}

void InitializeBrowserState() {
    LoadFileSettings();
    // Load ad-block filters (from disk if present otherwise use built-in set)
    LoadAdBlockFilters();
}

// Load ad-block filters into memory. Called at startup and after reload.
void LoadAdBlockFilters() {
    g_adblockHostFilters.clear();
    g_adblockSubstringFilters.clear();

    // Built-in host list (small built-in list)
    const char* builtin_hosts[] = {
        "doubleclick.net",
        "googlesyndication.com",
        "adservice.google.com",
        "ads.youtube.com",
        "ads.google.com",
        "adnxs.com",
        "adsafeprotected.com",
        "pagead2.googlesyndication.com",
        "track.adform.net",
        "tpc.googlesyndication.com"
    };
    for (const char* h : builtin_hosts) g_adblockHostFilters.insert(std::string(h));

    // Try to load a file named adblock_filters.txt next to the executable
    char modulePath[MAX_PATH] = {0};
    if (GetModuleFileNameA(NULL, modulePath, ARRAYSIZE(modulePath)) > 0) {
        PathRemoveFileSpecA(modulePath);
        char filterPath[MAX_PATH];
        PathCombineA(filterPath, modulePath, "adblock_filters.txt");
        if (!PathFileExistsA(filterPath)) {
            // If the file is missing, offer to download well-known filter lists
            DownloadAndAggregateFilters(g_hwndMain);
        }
        if (PathFileExistsA(filterPath)) {
            std::ifstream in(filterPath);
            if (in.is_open()) {
                std::string line;
                while (std::getline(in, line)) {
                    // trim
                    auto l = line.find_first_not_of(" \t\r\n");
                    if (l == std::string::npos) continue;
                    auto r = line.find_last_not_of(" \t\r\n");
                    std::string s = line.substr(l, r - l + 1);
                    if (s.empty()) continue;
                    if (s[0] == L'!' || s[0] == L'#') continue; // comments
                    // very simple heuristic: if contains only [a-z0-9.-] treat as host
                    bool hostLike = true;
                    for (char c : s) {
                        if (!(isalnum(c) || c == '.' || c == '-')) { hostLike = false; break; }
                    }
                    if (hostLike) {
                        // lower-case
                        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
                        g_adblockHostFilters.insert(s);
                    } else {
                        g_adblockSubstringFilters.push_back(s);
                    }
                }
                in.close();
            }
        }
    }
}

// Download and aggregate common filter lists into adblock_filters.txt
// Returns true on success, false on failure or user cancel.
bool DownloadAndAggregateFilters(HWND hwnd) {
    // Ask user for consent (lists are copyrighted by their authors)
    const char* msg = "This will download publicly-available ad/filter lists and aggregate them into a local filter file. These lists are maintained by third parties and FlameDarkness Browser does not claim a license over them. Continue?";
    int resp = MessageBoxA(hwnd, msg, "Download filter lists?", MB_YESNO | MB_ICONQUESTION);
    if (resp != IDYES) return false;

    // URLs to download - can be modified for selecting lists
    std::vector<std::string> urls = {
        "https://easylist-downloads.adblockplus.org/easylist/easylist.txt",
        "https://easylist-downloads.adblockplus.org/easylist/easyprivacy.txt",
        "https://easylist-downloads.adblockplus.org/easylist/malwaredomains_full.txt",
        "https://easylist.to/easylist/fanboy-annoyance.txt",
        "https://easylist.to/easylist/fanboy-social.txt",
        "https://filters.adtidy.org/extension/ublock/filters/2.txt"
    };

    // Prepare paths
    char modulePath[MAX_PATH] = {0};
    if (GetModuleFileNameA(NULL, modulePath, ARRAYSIZE(modulePath)) == 0) return false;
    PathRemoveFileSpecA(modulePath);
    char outPath[MAX_PATH];
    PathCombineA(outPath, modulePath, "adblock_filters.txt");

    // Write header with attribution
    {
        std::ofstream out(outPath, std::ios::out | std::ios::trunc);
        if (!out.is_open()) return false;
        out << "! Aggregated filter list\n";
        out << "! Aggregated from: publicly available filter lists\n";
        // out << "! Lines starting with '!' or '#' are comments and ignored.\n\n";
        out.close();
    }

    // Temporary download file
    char tempPath[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempPath) == 0) return false;
    char tempFile[MAX_PATH];

    for (const auto& u : urls) {
        // create temp filename
        if (GetTempFileNameA(tempPath, "abk", 0, tempFile) == 0) continue;
        HRESULT hr = URLDownloadToFileA(NULL, u.c_str(), tempFile, 0, NULL);
        if (SUCCEEDED(hr)) {
            // Append non-comment lines to outPath
            std::ifstream in(tempFile);
            std::ofstream out(outPath, std::ios::out | std::ios::app);
            if (in.is_open() && out.is_open()) {
                std::string line;
                while (std::getline(in, line)) {
                    // trim whitespace
                    size_t l = line.find_first_not_of(" \t\r\n");
                    if (l == std::string::npos) continue;
                    size_t r = line.find_last_not_of(" \t\r\n");
                    std::string s = line.substr(l, r - l + 1);
                    if (s.empty()) continue;
                    if (s[0] == L'!' || s[0] == L'#') continue;
                    out << s << "\n";
                }
                in.close(); out.close();
            }
        }
        // remove temp file
        DeleteFileA(tempFile);
    }

    return true;
}

// --- 5. WebView2 Initialization and Handlers ---

void ApplySettingsToWebView() {
    if (!g_webView) return;

    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(g_webView->get_Settings(&settings))) {
        
        // --- Core Settings (Script/Host) ---
        settings->put_IsScriptEnabled(g_isScriptEnabled ? TRUE : FALSE); 
        settings->put_AreDefaultScriptDialogsEnabled(g_areDefaultScriptDialogsEnabled ? TRUE : FALSE);         
        settings->put_AreHostObjectsAllowed(FALSE);         

        // --- Settings2 (User Agent) ---
        ComPtr<ICoreWebView2Settings2> settings2;
        if (SUCCEEDED(settings.As(&settings2))) {
            if (g_customUserAgentEnabled) {
                // FIX: Convert std::string (ANSI) to std::wstring (Wide)
                std::wstring wUserAgent = AnsiToWide(g_customUserAgentString);
                settings2->put_UserAgent(wUserAgent.c_str());
            } else {
                settings2->put_UserAgent(L"");
            }
        }

        // NOTE: Setting tracking-prevention on the environment requires using
        // the environment options when creating the WebView2 environment.
        // That logic is non-trivial and depends on the WebView2 SDK version.
        // The previous malformed attempt has been removed. Tracking prevention
        // state (g_enableTrackingPrevention) is still stored and can be applied
        // when the environment is created with proper ICoreWebView2EnvironmentOptions
        // support.

       // --- Settings3 (Third-Party Cookies Blocking) ---
       /*ComPtr<ICoreWebView2Settings3> settings3;
       if (SUCCEEDED(settings.As(&settings3))) {
           cout << "Third-party cookies: controlled via environment flags.");
       } else {
           cout << "Third-party cookie control not available in this WebView build.");
       }*/
        
        // --- Settings4 (Autofill/Password Security) ---
        ComPtr<ICoreWebView2Settings4> settings4;
        if (SUCCEEDED(settings.As(&settings4))) {
            settings4->put_IsPasswordAutosaveEnabled(g_isPasswordAutosaveEnabled ? TRUE : FALSE); 
            settings4->put_IsGeneralAutofillEnabled(g_isGeneralAutofillEnabled ? TRUE : FALSE);  
        }
    }
}


void InitializeWebView2(HWND hwnd) {
    // Try to create concrete environment-options if the SDK header is present
    // (WebView2EnvironmentOptions.h). This lets us enable tracking prevention
    // at environment creation time when supported. If the header/class isn't
    // available, fall back to nullptr options for maximum compatibility.
    ComPtr<ICoreWebView2EnvironmentOptions> envOptions;
#if defined(HAVE_WEBVIEW2_ENV_OPTIONS)
    // Instantiate the provided CoreWebView2EnvironmentOptions runtime class
    // (this type is only available in newer WebView2 SDK headers).
    {
        ComPtr<CoreWebView2EnvironmentOptions> concreteOptions = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
        if (concreteOptions) {
            // If the environment-options interface supports the tracking prevention
            // property (ICoreWebView2EnvironmentOptions5), set it according to
            // the user's saved preference.
            ComPtr<ICoreWebView2EnvironmentOptions5> envOptions5;
            if (SUCCEEDED(concreteOptions.As(&envOptions5))) {
                envOptions5->put_EnableTrackingPrevention(g_enableTrackingPrevention ? TRUE : FALSE);
            }
            // Disable browser extensions for security reasons.
            concreteOptions->put_AreBrowserExtensionsEnabled(FALSE);
            // Always enable privacy-related Chromium flags for partitioning and
            // a robust third-party cookie fallback.
            std::string enableFeatures =
                "ThirdPartyStoragePartitioning,"
                "PartitionedCookies,"
                "SameSiteByDefaultCookies,"
                "CookiesWithoutSameSiteMustBeSecure,"
                "EnhancedSafeBrowsing";

            std::string featureArgs;
                featureArgs += "--disable-third-party-cookies ";
                if (g_DNS_DoH) {
                    enableFeatures +=
                        ",DnsOverHttps<DoHTrial,"
                        "StrictOriginIsolation,"
                        "HeavyAdIntervention,"
                        "HeavyAdPrivacyMitigations";

                    featureArgs += "--enable-features=" + enableFeatures + " ";
                    featureArgs += "--force-fieldtrials=DoHTrial/Group1 ";
                    featureArgs += "--force-fieldtrial-params=DoHTrial.Group1:server/https%3A%2F%2Fdns.quad9.net%2Fdns-query/method/POST ";
                } else {
                    featureArgs += "--enable-features=" + enableFeatures + " ";
                }
            std::wstring wFeatureArgs = AnsiToWide(featureArgs);
            concreteOptions->put_AdditionalBrowserArguments(wFeatureArgs.c_str());
            // Use the concrete options as the ICoreWebView2EnvironmentOptions pointer
            concreteOptions.As(&envOptions);
        }
    }
#endif

    CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, envOptions.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || env == nullptr) {
                    char buf[256];
                    sprintf_s(buf, 256, "Failed to create WebView2 environment (HRESULT=0x%08X). Ensure WebView2 Runtime is installed.", (unsigned)result);
                    MessageBoxA(hwnd, buf, "WebView2 Error", MB_OK | MB_ICONERROR);
                    return S_OK;
                }

                env->CreateCoreWebView2Controller(hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [env, hwnd](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            if (controller != nullptr) {
                                g_controller = controller;
                                g_controller->get_CoreWebView2(&g_webView);
                                
                                EventRegistrationToken token;
                                
                                ApplySettingsToWebView();

                                // Source Changed Handler (Updates URL Bar and global URL)
                                g_webView->add_SourceChanged(Callback<ICoreWebView2SourceChangedEventHandler>(
                                    [](ICoreWebView2* webview, ICoreWebView2SourceChangedEventArgs* args) -> HRESULT {
                                        LPWSTR wide_uri = nullptr; // Changed to LPWSTR
                                        webview->get_Source(&wide_uri);
                                        if (wide_uri != nullptr) {
                                            std::string ansi_uri = WideToAnsi(wide_uri); // Convert to ANSI
                                            SetWindowTextA(g_hwndUrlBar, ansi_uri.c_str());
                                            g_currentUrl = ansi_uri; 
                                            CoTaskMemFree(wide_uri);
                                        }
                                        return S_OK;
                                    }).Get(), &token);

                                // Title Changed Handler (Updates Main Window Title). I absolutely do not want my browser title bar to change, so I commented out the code.
                                /*g_webView->add_DocumentTitleChanged(Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
                                    [](ICoreWebView2* webview, IUnknown* args) -> HRESULT {
                                        LPWSTR wide_title = nullptr; // Changed to LPWSTR
                                        webview->get_DocumentTitle(&wide_title);
                                        if (wide_title != nullptr) {
                                            std::string ansi_title = WideToAnsi(wide_title); // Convert to ANSI
                                            char windowTitle[256];
                                            sprintf_s(windowTitle, 256, "FlameDarkness - %s", ansi_title.c_str());
                                            SetWindowTextA(g_hwndMain, windowTitle);
                                            g_currentTitle = ansi_title; 
                                            CoTaskMemFree(wide_title);
                                        }
                                        return S_OK;
                                    }).Get(), &token);*/

                                // Navigation Complete Handler (Updates Status Bar)
                                g_webView->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [](ICoreWebView2* webview, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                                        BOOL success;
                                        args->get_IsSuccess(&success);
                                        
                                        if (success) {
                                            // After navigation completes, proactively remove third-party
                                            // cookies to improve privacy. We do this by enumerating all
                                            // cookies in the current profile and deleting those whose
                                            // domain does not match the top-level host.
                                            // This uses the CookieManager available via ICoreWebView2_2.
                                            // If the API isn't available this is a no-op.
                                            char mainUrlBuf[2048] = {0};
                                            if (g_currentUrl.size() > 0) {
                                                strncpy_s(mainUrlBuf, g_currentUrl.c_str(), _TRUNCATE);
                                            } else {
                                                LPWSTR wide_src = nullptr; // <--- FIX: Must use LPWSTR
                                                if (SUCCEEDED(webview->get_Source(&wide_src)) && wide_src) {
                                                    // Convert Wide string to ANSI for use with mainUrlBuf (char buffer)
                                                    // WideCharToMultiByte(CP_ACP, 0, wide_src, -1, mainUrlBuf, sizeof(mainUrlBuf), NULL, NULL);
                                                    std::string ansi_src = WideToAnsi(wide_src);
                                                    strncpy_s(mainUrlBuf, ansi_src.c_str(), _TRUNCATE); // Use converted string
                                                    CoTaskMemFree(wide_src);
                                                }
                                            }
                                            // Extract host from main URL
                                            std::string mainHost;
                                            auto GetHostFromUrl = [](const std::string& url) -> std::string {
                                                if (url.empty()) return "";
                                                size_t p = url.find("://");
                                                size_t start = 0;
                                                if (p != std::string::npos) start = p + 3;
                                                size_t slash = url.find('/', start);
                                                std::string host = (slash == std::string::npos) ? url.substr(start) : url.substr(start, slash - start);
                                                // Strip potential credentials and port
                                                size_t at = host.rfind('@');
                                                if (at != std::string::npos) host = host.substr(at + 1);
                                                size_t colon = host.find(':');
                                                if (colon != std::string::npos) host = host.substr(0, colon);
                                                // Lowercase for comparison
                                                std::transform(host.begin(), host.end(), host.begin(), ::tolower);
                                                return host;
                                            };
                                            mainHost = GetHostFromUrl(mainUrlBuf);

if (!mainHost.empty()) {
                                              ComPtr<ICoreWebView2_2> webview2;
                                              if (SUCCEEDED(g_webView.As(&webview2))) {
                                                  ComPtr<ICoreWebView2CookieManager> cookieManager;
                                                  if (SUCCEEDED(webview2->get_CookieManager(&cookieManager)) && cookieManager) {
                                                       // Request all cookies in the profile (empty uri -> all cookies)
                                                       // Note: This is an asynchronous operation, so cleanup is delayed.
                                                       ComPtr<ICoreWebView2CookieManager> cmCopy = cookieManager;
                                                       std::string hostCopy = mainHost;
                                                       // Remove leading "www." for robust comparison if it exists
                                                       if (hostCopy.rfind("www.", 0) == 0) hostCopy = hostCopy.substr(4);
                                                       cmCopy->GetCookies(nullptr, Callback<ICoreWebView2GetCookiesCompletedHandler>(
                                                           [cmCopy, hostCopy](HRESULT hr, ICoreWebView2CookieList* cookieList) -> HRESULT {
                                                               if (FAILED(hr) || !cookieList) return S_OK;
                                                               UINT32 count = 0;
                                                               if (FAILED(cookieList->get_Count(&count))) return S_OK;
                                                               for (UINT32 i = 0; i < count; ++i) {
                                                                   ComPtr<ICoreWebView2Cookie> cookie;
                                                                   if (SUCCEEDED(cookieList->GetValueAtIndex(i, &cookie)) && cookie) {
                                                                       LPWSTR domainW = nullptr;
                                                                       if (SUCCEEDED(cookie->get_Domain(&domainW)) && domainW) {
                                                                           std::string domain = WideToAnsi(domainW);
                                                                           CoTaskMemFree(domainW);
                                                                           
                                                                           // Normalize the cookie domain
                                                                           if (!domain.empty() && domain[0] == '.') domain = domain.substr(1);
                                                                           std::transform(domain.begin(), domain.end(), domain.begin(), tolower);
                                                                           
                                                                           // Remove leading "www." for robust comparison
                                                                           if (domain.rfind("www.", 0) == 0) domain = domain.substr(4);

                                                                           // Check for third-party cookie
                                                                           bool isThirdParty = true;
                                                                           
                                                                           // Check if the cookie domain is the host or a subdomain of the host
                                                                           if (domain == hostCopy) {
                                                                               isThirdParty = false;
                                                                           } else if (domain.size() > hostCopy.size() && domain.rfind("." + hostCopy) == domain.size() - (hostCopy.size() + 1)) {
                                                                               // e.g. cookie domain 'sub.example.com' vs host 'example.com'
                                                                               isThirdParty = false;
                                                                           }

                                                                           if (isThirdParty) {
                                                                               // Delete this cookie (third-party)
                                                                               LPWSTR nameW = nullptr;
                                                                               LPWSTR domainW2 = nullptr;
                                                                               LPWSTR pathW = nullptr;
                                                                               if (SUCCEEDED(cookie->get_Name(&nameW)) && SUCCEEDED(cookie->get_Domain(&domainW2)) && SUCCEEDED(cookie->get_Path(&pathW))) {
                                                                                    // To delete a cookie, you need the name, domain, and path.
                                                                                    cmCopy->DeleteCookiesWithDomainAndPath(nameW, domainW2, pathW);
                                                                               }
                                                                               CoTaskMemFree(nameW);
                                                                               CoTaskMemFree(domainW2);
                                                                               CoTaskMemFree(pathW);
                                                                           }
                                                                       }
                                                                   }
                                                               }

                                                                return S_OK;
                                                            }).Get());
                                                    }
                                                }
                                            }
                                        } else {
                                            // Navigation failed.
                                        }
                                        return S_OK;
                                    }).Get(), &token);

                                    // Navigation Starting Handler (Force HTTPS upgrade)
                                    g_webView->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
                                        [](ICoreWebView2* webview, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                            LPWSTR wide_uri = nullptr;
                                            if (SUCCEEDED(args->get_Uri(&wide_uri)) && wide_uri) {
                                                std::string s = WideToAnsi(wide_uri);
                                                CoTaskMemFree(wide_uri);
                                                if (g_forceHTTPS) {
                                                    // If navigation is explicitly http://, upgrade to https
                                                    if (s.find("http://") == 0) {
                                                        args->put_Cancel(TRUE);
                                                        std::string secure = "https://" + s.substr(7);
                                                        // FIX: Convert the ANSI string 'secure' to Wide before calling Navigate
                                                        std::wstring wSecure = AnsiToWide(secure);
                                                        if (g_webView) g_webView->Navigate(wSecure.c_str());
                                                    }
                                                }
                                            }
                                            return S_OK;
                                        }).Get(), &token);
                                
                                // Pop-up Blocker
                                g_webView->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                                    [](ICoreWebView2* webview, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                                        if (g_blockPopups) {
                                            args->put_Handled(TRUE);
                                        } 
                                        return S_OK;
                                    }).Get(), &token);

								
								// NEW: Permission Handler
								g_webView->add_PermissionRequested(Callback<ICoreWebView2PermissionRequestedEventHandler>(
									[](ICoreWebView2* webview, ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT {
										COREWEBVIEW2_PERMISSION_KIND kind;
										args->get_PermissionKind(&kind);

										bool do_block = false;
										LPCSTR message = "";

										switch (kind) {
											case COREWEBVIEW2_PERMISSION_KIND_NOTIFICATIONS:
												if (g_blockNotifications) {
													do_block = true;
													message = "Notification permission blocked.";
												}
												break;
                                            case COREWEBVIEW2_PERMISSION_KIND_GEOLOCATION:
                                                if (g_blockLocation) {
                                                    do_block = true;
                                                    message = "Location permission blocked.";
                                                }
                                                break;
											case COREWEBVIEW2_PERMISSION_KIND_MICROPHONE:
												if (g_blockMicrophone) {
													do_block = true;
													message = "Microphone permission blocked.";
												}
												break;
											case COREWEBVIEW2_PERMISSION_KIND_CAMERA:
												if (g_blockWebcam) {
													do_block = true;
													message = "Webcam permission blocked.";
												}
												break;
											default:
												// For other permissions, use default behavior.
												args->put_State(COREWEBVIEW2_PERMISSION_STATE_DEFAULT);
												return S_OK;
										}

										if (do_block) {
											args->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY);
										} else {
											// You might want to prompt the user here instead of always allowing
											args->put_State(COREWEBVIEW2_PERMISSION_STATE_ALLOW);
										}
										
										return S_OK;
									}).Get(), &token);
                                
                               // Default start page is a privacy-focused and compatible search engine.
                                HRESULT hrFilter = g_webView->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
                                (void)hrFilter; // ignore if not supported

                                EventRegistrationToken token2;
                                g_webView->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>(
                                    [env](ICoreWebView2* webview, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                                        if (!args) return S_OK;
                                        ComPtr<ICoreWebView2WebResourceRequest> request;
                                        if (FAILED(args->get_Request(&request)) || !request) return S_OK;

                                        // If user enabled Do-Not-Track, add DNT request header
                                        ComPtr<ICoreWebView2HttpRequestHeaders> headers;
                                        if (g_trackingPrevention && SUCCEEDED(request->get_Headers(&headers)) && headers) {
                                            headers->SetHeader(L"DNT", L"1");
                                        }

                                        LPWSTR uriW = nullptr; // Must be Wide
                                        request->get_Uri(&uriW);
                                        if (!uriW) return S_OK;
                                        std::string uri = WideToAnsi(uriW); // Must be ANSI for adblock logic
                                        CoTaskMemFree(uriW);

                                        // Use ad-block filters if enabled
                                        if (g_adBlockEnabled) {
                                            // Extract host from URI
                                            auto GetHost = [](const std::string& url) -> std::string {
                                                if (url.empty()) return "";
                                                size_t p = url.find("://");
                                                size_t start = (p == std::string::npos) ? 0 : (p + 3);
                                                size_t slash = url.find('/', start);
                                                std::string h = (slash == std::string::npos) ? url.substr(start) : url.substr(start, slash - start);
                                                // strip credentials
                                                size_t at = h.rfind(L'@'); if (at != std::string::npos) h = h.substr(at + 1);
                                                // strip port
                                                size_t colon = h.find(L':'); if (colon != std::string::npos) h = h.substr(0, colon);
                                                std::transform(h.begin(), h.end(), h.begin(), ::tolower);
                                                return h;
                                            };

                                            std::string host = GetHost(uri);

                                            bool blocked = false;

                                            if (!host.empty()) {
                                                // Exact or suffix host match
                                                for (const auto& fh : g_adblockHostFilters) {
                                                    if (host == fh) { blocked = true; break; }
                                                    if (host.size() > fh.size() && host.compare(host.size() - fh.size(), fh.size(), fh) == 0) {
                                                        // e.g. host 'sub.example.com' ends with 'example.com'
                                                        if (host[host.size() - fh.size() - 1] == L'.') { blocked = true; break; }
                                                    }
                                                }
                                            }

                                            // Substring-based filters
                                            if (!blocked) {
                                                for (const auto& pat : g_adblockSubstringFilters) {
                                                    if (uri.find(pat) != std::string::npos) { blocked = true; break; }
                                                }
                                            }

                                            if (blocked) {
                                                IStream* emptyStream = SHCreateMemStream(nullptr, 0);
                                                if (env && emptyStream) {
                                                    ComPtr<ICoreWebView2WebResourceResponse> response;
                                                    HRESULT hr = env->CreateWebResourceResponse(emptyStream, 204, L"No Content", L"Content-Type: text/plain", &response);
                                                    if (SUCCEEDED(hr) && response) {
                                                        args->put_Response(response.Get());
                                                    }
                                                    emptyStream->Release();
                                                }
                                            }
                                        }
                                        return S_OK;
                                    }).Get(), &token2);

                                g_webView->Navigate(L"https://duckduckgo.com");
                                ResizeWebView();
                                g_controller->put_IsVisible(TRUE);
                            }
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
}


// --- 6. Settings Application, Creation, and Procedures ---

LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CLOSE: {
            SaveFileSettings(); 
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
            
        case WM_COMMAND: {
            if (HIWORD(wParam) == BN_CLICKED) {
                HWND hCheckbox = (HWND)lParam;
                int controlId = LOWORD(wParam);
                bool isChecked = SendMessageA(hCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;

                if (controlId == IDC_CHECK_BLOCK_POPUPS) {
                    g_blockPopups = isChecked;
                    ApplySettingsToWebView();
                }
                else if (controlId == IDC_CHECK_DNT) {
                            g_trackingPrevention = isChecked;
                            ApplySettingsToWebView();
                            // DNT is a best-effort header; mark as optional in the status
                }
                else if (controlId == IDC_CHECK_CUSTOM_UA) {
                    g_customUserAgentEnabled = isChecked;
                    EnableWindow(GetDlgItem(hwnd, IDC_UA_EDIT_BOX), g_customUserAgentEnabled);
                    ApplySettingsToWebView();
                }
                else if (controlId == IDC_CHECK_TRACKING_PREVENTION) { 
                    g_enableTrackingPrevention = isChecked;
                    // Environment-level tracking prevention is work-in-progress depending on WebView2 runtime
                }
                else if (controlId == IDC_CHECK_FORCE_HTTPS) {
                    g_forceHTTPS = isChecked;
                    ApplySettingsToWebView();
                }
                else if (controlId == IDC_CHECK_SCRIPT_ENABLED) {
                    g_isScriptEnabled = isChecked;
                    ApplySettingsToWebView();
                }
                else if (controlId == IDC_CHECK_SCRIPT_DIALOGS) {
                    g_areDefaultScriptDialogsEnabled = isChecked;
                    ApplySettingsToWebView();
                }
                else if (controlId == IDC_CHECK_PASSWORD_SAVE) {
                    g_isPasswordAutosaveEnabled = isChecked;
                    ApplySettingsToWebView();
                }
                else if (controlId == IDC_CHECK_AUTOFILL) {
                    g_isGeneralAutofillEnabled = isChecked;
                    ApplySettingsToWebView();
                }
				
				// NEW PERMISSION HANDLERS
				else if (controlId == IDC_CHECK_BLOCK_NOTIFICATIONS) {
					g_blockNotifications = isChecked;
                    ApplySettingsToWebView();
				}
				else if (controlId == IDC_CHECK_BLOCK_LOCATION) {
					g_blockLocation = isChecked;
                    ApplySettingsToWebView();
				}
				else if (controlId == IDC_CHECK_BLOCK_WEBCAM) {
					g_blockWebcam = isChecked;
                    ApplySettingsToWebView();
				}
				else if (controlId == IDC_CHECK_BLOCK_MICROPHONE) {
					g_blockMicrophone = isChecked;
                    ApplySettingsToWebView();
				}
				else if (controlId == IDC_CHECK_QUAD9_DOH) {
					g_DNS_DoH = isChecked;
                    ApplySettingsToWebView();
				}
				
            } else if (HIWORD(wParam) == EN_CHANGE) {
                if (LOWORD(wParam) == IDC_UA_EDIT_BOX) {
                    char buffer[1024];
                    GetWindowTextA(GetDlgItem(hwnd, IDC_UA_EDIT_BOX), buffer, 1024);
                    g_customUserAgentString = buffer;
                    
                    if (g_customUserAgentEnabled) {
                        ApplySettingsToWebView();
                    }
                }
            }
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void ShowSettingsWindow(HWND hParent) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE);
    const char SETTINGS_CLASS_NAME[] = "BrowserSettingsWindow";
    
    // Register the Settings Window Class once
    WNDCLASSA wc = { 
        0, SettingsWindowProc, 0, 0, hInst, LoadIcon(NULL, IDI_APPLICATION), 
        LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1), nullptr, SETTINGS_CLASS_NAME
    };
    if (!GetClassInfoA(hInst, SETTINGS_CLASS_NAME, &wc)) RegisterClassA(&wc);

    if (!g_hwndSettings) {
        g_hwndSettings = CreateWindowExA(
            0, SETTINGS_CLASS_NAME, "Settings - Security & Privacy", 
            WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            CW_USEDEFAULT, CW_USEDEFAULT, 450, 660, // This comment lets me resize the window height more easily, so all privacy options are visible.
            hParent, nullptr, hInst, nullptr
        );
        if (!g_hwndSettings) return;

        int y = PADDING * 3;

        // --- 1. General Header ---
        CreateWindowA("STATIC", "Security and Privacy Features", WS_CHILD | WS_VISIBLE | SS_LEFT, 
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_SETTINGS_HEADER, hInst, nullptr);
        y += 30;
        CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, 20, y, 400, 1, g_hwndSettings, nullptr, hInst, nullptr);
        y += PADDING * 3;

        // --- 2. Checkboxes ---
        
        // Pop-ups
        CreateWindowA("BUTTON", "Block pop-up windows (Recommended)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_CHECK_BLOCK_POPUPS, hInst, nullptr);
        y += 30;

        // DNT
        CreateWindowA("BUTTON", "Send 'Do Not Track' (DNT) header", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_CHECK_DNT, hInst, nullptr);
        y += 30;

        CreateWindowA("BUTTON", "Force HTTPS (Upgrade insecure requests)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_CHECK_FORCE_HTTPS, hInst, nullptr);
        y += 30;

        // Tracking Prevention Button
        CreateWindowA("BUTTON", "Enable Tracking Prevention (Work-in-Progress)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_CHECK_TRACKING_PREVENTION, hInst, nullptr);
        y += 40; 
        
        // --- 3. Script Control Settings (NEW) ---
        CreateWindowA("STATIC", "Script and User Input Contro", WS_CHILD | WS_VISIBLE | SS_LEFT, 
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_UA_LABEL, hInst, nullptr);
        y += 20;

        // NEW: Script Enabled
        CreateWindowA("BUTTON", "Enable JavaScript", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_CHECK_SCRIPT_ENABLED, hInst, nullptr);
        y += 30;

        // NEW: Script Dialogs Enabled
        CreateWindowA("BUTTON", "Allow JavaScript Dialogs (Alert, Confirm, Prompt)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_CHECK_SCRIPT_DIALOGS, hInst, nullptr);
        y += 30;

        // NEW: Password Autosave Enabled
        CreateWindowA("BUTTON", "Allow Password Autosave", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_CHECK_PASSWORD_SAVE, hInst, nullptr);
        y += 30;

        // NEW: General Autofill Enabled
        CreateWindowA("BUTTON", "Allow General Autofill (Address, Credit Card)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_CHECK_AUTOFILL, hInst, nullptr);
        y += 40;

		
		// --- NEW: Permissions Section ---
        CreateWindowA("STATIC", "Permission Contro", WS_CHILD | WS_VISIBLE | SS_LEFT, 
            20, y, 400, 20, g_hwndSettings, nullptr, hInst, nullptr);
        y += 30;

        CreateWindowA("BUTTON", "Block Notifications", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_CHECK_BLOCK_NOTIFICATIONS, hInst, nullptr);
        y += 30;

        CreateWindowA("BUTTON", "Block Location Access", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_CHECK_BLOCK_LOCATION, hInst, nullptr);
        y += 30;

        CreateWindowA("BUTTON", "Block Webcam Access", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_CHECK_BLOCK_WEBCAM, hInst, nullptr);
        y += 30;

        CreateWindowA("BUTTON", "Block Microphone Access", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_CHECK_BLOCK_MICROPHONE, hInst, nullptr);
        y += 30;
        CreateWindowA("BUTTON", "Enable Quad9 DNS DoH", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_CHECK_QUAD9_DOH, hInst, nullptr);
        y += 40;		

        // --- 4. User Agent Section ---
        CreateWindowA("STATIC", "Custom Browser Identity (User Agent)", WS_CHILD | WS_VISIBLE | SS_LEFT, 
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_UA_LABEL, hInst, nullptr);
        y += 20;

        // Checkbox: Custom User Agent Enable
        CreateWindowA("BUTTON", "Override User Agent String", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 
            20, y, 400, 20, g_hwndSettings, (HMENU)IDC_CHECK_CUSTOM_UA, hInst, nullptr);
        y += 30;

        // Edit Box: User Agent String
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_customUserAgentString.c_str(),
            WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL, 20, y, 400, 24, g_hwndSettings, (HMENU)IDC_UA_EDIT_BOX, hInst, nullptr);
        y += 30; 

        // Apply font
        HFONT hFont = (HFONT)GetStockObject(SYSTEM_FONT);
        EnumChildWindows(g_hwndSettings, [](HWND hwndChild, LPARAM lParam) -> BOOL {
            SendMessageA(hwndChild, WM_SETFONT, (WPARAM)lParam, TRUE);
            return TRUE;
        }, (LPARAM)hFont);
    }
    
    // Set toggle and permission states
    SendMessageA(GetDlgItem(g_hwndSettings, IDC_CHECK_BLOCK_POPUPS), BM_SETCHECK, g_blockPopups ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(GetDlgItem(g_hwndSettings, IDC_CHECK_DNT), BM_SETCHECK, g_trackingPrevention ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(GetDlgItem(g_hwndSettings, IDC_CHECK_CUSTOM_UA), BM_SETCHECK, g_customUserAgentEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(GetDlgItem(g_hwndSettings, IDC_CHECK_TRACKING_PREVENTION), BM_SETCHECK, g_enableTrackingPrevention ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(GetDlgItem(g_hwndSettings, IDC_CHECK_SCRIPT_ENABLED), BM_SETCHECK, g_isScriptEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(GetDlgItem(g_hwndSettings, IDC_CHECK_SCRIPT_DIALOGS), BM_SETCHECK, g_areDefaultScriptDialogsEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(GetDlgItem(g_hwndSettings, IDC_CHECK_PASSWORD_SAVE), BM_SETCHECK, g_isPasswordAutosaveEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(GetDlgItem(g_hwndSettings, IDC_CHECK_AUTOFILL), BM_SETCHECK, g_isGeneralAutofillEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(GetDlgItem(g_hwndSettings, IDC_CHECK_BLOCK_NOTIFICATIONS), BM_SETCHECK, g_blockNotifications ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(GetDlgItem(g_hwndSettings, IDC_CHECK_BLOCK_LOCATION), BM_SETCHECK, g_blockLocation ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(GetDlgItem(g_hwndSettings, IDC_CHECK_BLOCK_WEBCAM), BM_SETCHECK, g_blockWebcam ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(GetDlgItem(g_hwndSettings, IDC_CHECK_BLOCK_MICROPHONE), BM_SETCHECK, g_blockMicrophone ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(GetDlgItem(g_hwndSettings, IDC_CHECK_QUAD9_DOH), BM_SETCHECK, g_DNS_DoH ? BST_CHECKED : BST_UNCHECKED, 0);

    // Set Force HTTPS checkbox state
    SendMessageA(GetDlgItem(g_hwndSettings, IDC_CHECK_FORCE_HTTPS), BM_SETCHECK, g_forceHTTPS ? BST_CHECKED : BST_UNCHECKED, 0);
	

    SetWindowTextA(GetDlgItem(g_hwndSettings, IDC_UA_EDIT_BOX), g_customUserAgentString.c_str());
    EnableWindow(GetDlgItem(g_hwndSettings, IDC_UA_EDIT_BOX), g_customUserAgentEnabled);

    ShowWindow(g_hwndSettings, SW_SHOW);
    SetForegroundWindow(g_hwndSettings);
}


// --- 7. Disabled Features ---

void ShowHistoryWindow(HWND hParent) {
    MessageBoxA(hParent, "History logging is disabled for security and privacy reasons.", "Feature Disabled", MB_OK | MB_ICONINFORMATION);
}


// --- 8. Main Window Creation and Procedures ---

void NavigateToUrl(const char* url) {
    string s_url(url);
    // Default to HTTPS.
    if (s_url.find("://") == string::npos) {
        s_url = "https://" + s_url;
    }
    if (g_webView) {
        // Convert to Wide before sending to WebView2
        std::wstring wUrl = AnsiToWide(s_url);
        g_webView->Navigate(wUrl.c_str());
    }
}

void CreateBrowserControls(HWND hwnd) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    
    // Create Toolbar buttons
    int x = PADDING;
    g_hwndBackButton = CreateWindowA("BUTTON", "<--", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, x, PADDING, BUTTON_WIDTH, BUTTON_HEIGHT, hwnd, (HMENU)IDC_BACK_BUTTON, hInst, 0);
    x += BUTTON_WIDTH + PADDING;
    g_hwndForwardButton = CreateWindowA("BUTTON", "-->", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, x, PADDING, BUTTON_WIDTH, BUTTON_HEIGHT, hwnd, (HMENU)IDC_FORWARD_BUTTON, hInst, 0);
    x += BUTTON_WIDTH + PADDING;
    g_hwndRefreshButton = CreateWindowA("BUTTON", "Refresh", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, x, PADDING, BUTTON_WIDTH, BUTTON_HEIGHT, hwnd, (HMENU)IDC_REFRESH_BUTTON, hInst, 0);
    x += BUTTON_WIDTH + PADDING;
    g_hwndHomeButton = CreateWindowA("BUTTON", "Home", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, x, PADDING, BUTTON_WIDTH, BUTTON_HEIGHT, hwnd, (HMENU)IDC_HOME_BUTTON, hInst, 0);
    x += BUTTON_WIDTH + PADDING;

    // Create URL Bar (Edit Control)
    g_hwndUrlBar = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "https://duckduckgo.com", 
                                  WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL, 
                                  x, PADDING, 1, BUTTON_HEIGHT, hwnd, (HMENU)IDC_URL_BAR, hInst, 0);

    // Create GO Button
    g_hwndGoButton = CreateWindowA("BUTTON", "Go", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, x, PADDING, GO_BUTTON_WIDTH, BUTTON_HEIGHT, hwnd, (HMENU)IDC_GO_BUTTON, hInst, 0);

    // Create Feature Buttons
    int right_x = GetSystemMetrics(SM_CXSCREEN); 
    g_hwndSettingsButton = CreateWindowA("BUTTON", "Settings", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, right_x - 80, PADDING, 80, BUTTON_HEIGHT, hwnd, (HMENU)IDC_SETTINGS_BUTTON, hInst, 0);
    g_hwndAdblockButton = CreateWindowA("BUTTON", "Ad Blocker", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, right_x - 80 * 2 - PADDING, PADDING, 80, BUTTON_HEIGHT, hwnd, (HMENU)IDC_ADBLOCK_BUTTON, hInst, 0);
    g_hwndHistoryButton = CreateWindowA("BUTTON", "History", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, right_x - 80 * 3 - PADDING * 2, PADDING, 80, BUTTON_HEIGHT, hwnd, (HMENU)IDC_HISTORY_BUTTON, hInst, 0);
    g_hwndHelpButton = CreateWindowA("BUTTON", "Help", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, right_x - 80 * 4 - PADDING * 3, PADDING, 80, BUTTON_HEIGHT, hwnd, (HMENU)IDC_HELP_BUTTON, hInst, 0);
    // Make DNS Test button wider so the label fits
    g_hwndDNSButton = CreateWindowA("BUTTON", "Quad9 DoH Test", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, right_x - 80 * 4 - BUTTON_WIDTH - PADDING * 4, PADDING, BUTTON_WIDTH, BUTTON_HEIGHT, hwnd, (HMENU)IDC_DNS_TEST_BUTTON, hInst, 0);


    // Create Status Bar
/*    g_hwndStatusBar = CreateWindowExA(0, STATUSCLASSNAMEW, nullptr, 
                                     WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 
                                     0, 0, 0, 0, hwnd, nullptr, hInst, nullptr);
    int parts[STATUS_PARTS] = {100, 250, -1};
    SendMessageA(g_hwndStatusBar, SB_SETPARTS, STATUS_PARTS, (LPARAM)parts);
    SendMessageA(g_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Ready");
    SendMessageA(g_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)"Security Status: OK");
    SendMessageA(g_hwndStatusBar, SB_SETTEXT, 2, (LPARAM)"FlameDarkness Browser");*/

    // Apply font to controls
    // Use SYSTEM_FONT which is generally better for Unicode/Emoji support
    HFONT hFont = (HFONT)GetStockObject(SYSTEM_FONT);
    EnumChildWindows(hwnd, [](HWND hwndChild, LPARAM lParam) -> BOOL {
        SendMessageA(hwndChild, WM_SETFONT, (WPARAM)lParam, TRUE);
        return TRUE;
    }, (LPARAM)hFont);
}

void LayoutToolbarControls(HWND hwnd, int windowWidth) {
    // Calculate total fixed width for buttons (4 Nav buttons + 5 Feature buttons)
    const int navButtonWidth = BUTTON_WIDTH * 4;
    const int navButtonPadding = PADDING * 5; 
    // Adjusted to 4 buttons of 80px width plus a wider DNS Test button
    const int featureButtonTotalWidth = 80 * 4 + DNS_BUTTON_WIDTH + PADDING * 5; 

    const int urlBarStart = navButtonWidth + navButtonPadding;
    const int urlBarEnd = windowWidth - GO_BUTTON_WIDTH - PADDING - featureButtonTotalWidth;
    const int urlBarWidth = urlBarEnd - urlBarStart;

    // Reposition URL Bar and GO Button
    MoveWindow(g_hwndUrlBar, urlBarStart, PADDING, urlBarWidth, BUTTON_HEIGHT, TRUE);
    MoveWindow(g_hwndGoButton, urlBarStart + urlBarWidth + PADDING, PADDING, GO_BUTTON_WIDTH, BUTTON_HEIGHT, TRUE);

    // Reposition Feature Buttons (Fixed width 80px)
    int current_x = windowWidth - PADDING - 80;
    MoveWindow(g_hwndSettingsButton, current_x, PADDING, 80, BUTTON_HEIGHT, TRUE);
    
    current_x -= (80 + PADDING);
    MoveWindow(g_hwndAdblockButton, current_x, PADDING, 80, BUTTON_HEIGHT, TRUE);

    current_x -= (80 + PADDING);
    MoveWindow(g_hwndHistoryButton, current_x, PADDING, 80, BUTTON_HEIGHT, TRUE);

    current_x -= (80 + PADDING);
    MoveWindow(g_hwndHelpButton, current_x, PADDING, 80, BUTTON_HEIGHT, TRUE);

    current_x -= (DNS_BUTTON_WIDTH + PADDING);
    MoveWindow(g_hwndDNSButton, current_x, PADDING, DNS_BUTTON_WIDTH, BUTTON_HEIGHT, TRUE);

}

void ResizeWebView() {
    if (g_controller) {
        RECT bounds;
        GetClientRect(g_hwndMain, &bounds);
        
        // Exclude Toolbar and Status Bar areas
        bounds.top += TOOLBAR_HEIGHT;
        bounds.bottom -= STATUSBAR_HEIGHT;
        
        g_controller->put_Bounds(bounds);
        
        // Move Status Bar to bottom
        // MoveWindow(g_hwndStatusBar, 0, bounds.bottom, bounds.right, STATUSBAR_HEIGHT, TRUE);
    }
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            InitializeBrowserState();
            CreateBrowserControls(hwnd);
            InitializeWebView2(hwnd);
            break;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            LayoutToolbarControls(hwnd, width);
            ResizeWebView();
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            
            // Process button clicks
            switch (wmId) {
                case IDC_GO_BUTTON: {
                    char urlBuffer[2048];
                    GetWindowTextA(g_hwndUrlBar, urlBuffer, 2048);
                    NavigateToUrl(urlBuffer);
                    break;
                }
                case IDC_BACK_BUTTON:
                    if (g_webView) ApplySettingsToWebView || g_webView->GoBack();
                    break;
                case IDC_FORWARD_BUTTON:
                    if (g_webView) ApplySettingsToWebView || g_webView->GoForward();
                    break;
                case IDC_REFRESH_BUTTON:
                    if (g_webView) ApplySettingsToWebView || g_webView->Reload();
                    break;
                case IDC_HOME_BUTTON:
                    ApplySettingsToWebView;
                    NavigateToUrl("https://duckduckgo.com");
                    break;
                case IDC_HISTORY_BUTTON:
                    ShowHistoryWindow(hwnd); 
                    break;
                case IDC_ADBLOCK_BUTTON: {
                    // Show popup menu for Ad Blocker with Toggle / Reload / License
                    HMENU hMenu = CreatePopupMenu();
                    if (hMenu) {
                        // Informational (disabled) menu item to show list provenance
                        std::string toggleLabel = g_adBlockEnabled ? "Disable Ad Blocker" : "Enable Ad Blocker";
                        AppendMenuA(hMenu, MF_STRING, IDM_ADBLOCK_TOGGLE, toggleLabel.c_str());
                        AppendMenuA(hMenu, MF_STRING, IDM_ADBLOCK_RELOAD, "Reload filters");
                        AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
                        AppendMenuA(hMenu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, "Relying on publicly available filter lists.");

                        // Position popup under the button
                        RECT rc;
                        GetWindowRect(g_hwndAdblockButton, &rc);
                        POINT pt;
                        pt.x = rc.left;
                        pt.y = rc.bottom;

                        // Use TPM_RETURNCMD so we get the selected command ID synchronously
                        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, NULL);
                        DestroyMenu(hMenu);

                        if (cmd == IDM_ADBLOCK_TOGGLE) {
                            g_adBlockEnabled = !g_adBlockEnabled;
                            SaveFileSettings();
                        } else if (cmd == IDM_ADBLOCK_RELOAD) {
                            // Try download+aggregate; if user declines or fails, still attempt to load existing filters
                            bool ok = DownloadAndAggregateFilters(hwnd);
                            LoadAdBlockFilters();
                            /*if (ok) {
                                // Ad-block filters successfully reloaded.
                            } else {
                                // Ad-block filters reload failed.
                            }*/
                            // Reload the current page so new rules take effect immediately
                            if (g_webView) g_webView->Reload();
                        }
                    }
                    break;
                }
                case IDC_SETTINGS_BUTTON:
                    ShowSettingsWindow(hwnd);
                    break;
                case IDC_HELP_BUTTON: {
                    // Simple About/Help dialog (include the update date)
                    const char* aboutText = "FlameDarkness Browser Beta 0.92.\nMade by Local83 on 7 November 2025.\nUpdated on 25 November 2025.\nTheoretically compatible with Windows 11, 10, 8.1, and 7.\n\nReport issues: see the project repository.";
                    MessageBoxA(hwnd, aboutText, "About / Help", MB_OK | MB_ICONINFORMATION);
                    break;
                }
                case IDC_DNS_TEST_BUTTON: {
                    // Navigate to the DNS DoH test page inside the browser
                    NavigateToUrl("https://on.quad9.net/");
                    break;
                }
                case IDM_FILE_EXIT:
                    DestroyWindow(hwnd);
                    break;
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            if (g_trackingPrevention) {
                DeleteCookiesOnExit();
            }
            g_controller = nullptr;
            g_webView = nullptr;
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// --- 9. WinMain Entry Point ---

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    // Initialize common controls (status bar and others)
    INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    // *** FIX: Initialize COM (Critical for WebView2) ***
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return 0; 
    }

    const char CLASS_NAME[] = "BrowserWindowClass";

    // Register the main window class
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    
    if (!RegisterClassA(&wc)) {
        CoUninitialize(); // Uninitialize before returning
        return 0;
    }

    // Create the main window
    g_hwndMain = CreateWindowExA(
        0, CLASS_NAME, "FlameDarkness Browser Beta 0.92",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!g_hwndMain) {
        CoUninitialize(); // Uninitialize before returning
        return 0;
    }

    ShowWindow(g_hwndMain, nCmdShow);
    UpdateWindow(g_hwndMain);

    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // FIX: Uninitialize COM
    CoUninitialize();

    return 0;
}