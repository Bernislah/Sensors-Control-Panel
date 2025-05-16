#include "framework.h"
#include "Sensors Control Panel.h"

#include <fstream>
#include <string>
#include <iostream>
#include <sstream>

#include <commdlg.h>

#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#include <windows.h>
#include <vector>
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")
#include <commctrl.h>  // Para SetWindowSubclass
#pragma comment(lib, "comctl32.lib")

#include <wininet.h>
#pragma comment(lib, "wininet.lib")


#define MAX_LOADSTRING 100

// Variables globales:
HINSTANCE hInst;                                // instancia actual
WCHAR szTitle[MAX_LOADSTRING];                  // Texto de la barra de título
WCHAR szWindowClass[MAX_LOADSTRING];            // nombre de clase de la ventana principal
HWND hWndButton;                                // Variable global para el HWND del botón
WCHAR sMultiModal[MAX_LOADSTRING];                  // Texto de la barra de título

wchar_t rutaConfig[MAX_PATH];
wchar_t rutaPrograma[MAX_PATH] = { 0 };

bool control = 0;
HFONT hFont = NULL;
HFONT hFont2 = NULL;
HWND hPanelConexiones;
HWND hPanelControles;

//Variables globales para barra de progreso
// Para el diálogo de progreso
HWND   hProgressDlg = nullptr;
HWND   hProgressBar = nullptr;


//Reutilizable de mensajes de texto:
wchar_t questionText[256];
wchar_t questionText2[256];

//Variables para reposicionar las ventanas.
HWND hLabel, hBtnStart, hBtnStop, hBtnRestart, hBtnUpload, hBtnChildStart, hBtnChildStop, hBtnChildSRestart, hBtnChildSStatus;
HWND hWnd = NULL; // ventana principal
bool g_enSizing = false;

//= = = = = = = = = = = = = = =

// Declaraciones de funciones adelantadas incluidas en este módulo de código:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


void AplicarTemaAControles(HWND hWndParent) {
    HWND hChild = GetWindow(hWndParent, GW_CHILD);
    while (hChild) {
        SetWindowTheme(hChild, L"", L"");
        hChild = GetNextWindow(hChild, GW_HWNDNEXT);
    }
}

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
// = = = = = = = = = = FUNCIONES TRONCALES = = = = = = = = = =
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

// Elimina espacios, tabs y retornos de carro al principio y al final
static std::wstring trim(const std::wstring& s) {
    size_t start = s.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";
    size_t end = s.find_last_not_of(L" \t\r\n");
    return s.substr(start, end - start + 1);
}


//Función para contar lineas del fichero y decir las conexiones
int ContarLineasConfig()
{
    std::wifstream file(rutaConfig);
    if (!file.is_open())
        return 0;

    int contador = 0;
    std::wstring linea;
    while (std::getline(file, linea)) {
        if (!linea.empty()) // Opcional: ignorar líneas vacías
            contador++;
    }

    file.close();
    return contador;
}

// Estructura para almacenar una conexión
struct Connection {
    std::wstring nombre;
    std::wstring servidor;
    std::wstring usuario;
    std::wstring contrasena;
    bool         old_server;
    std::wstring hostkey;
};

// Estructura para pasar datos al diálogo
struct DialogData {
    bool editMode;
    int connectionIndex; // índice en el archivo
};

// Función para leer una conexión del archivo por índice
Connection GetConnectionFromFile(int index) {
    Connection conn;
    std::wifstream file(rutaConfig);
    if (!file.is_open()) return conn;

    std::wstring line;
    int currentLine = 0;
    while (std::getline(file, line)) {
        if (currentLine++ == index) {
            // 1) Encuentra la posición del 5º colon
            size_t pos = 0;
            for (int i = 0; i < 5; ++i) {
                pos = line.find(L':', pos);
                if (pos == std::wstring::npos) break;
                ++pos; // saltamos el colon
            }
            // 2) Extrae los primeros 5 campos con stringstream
            std::wistringstream ss(line.substr(0, pos));
            std::wstring token;
            std::vector<std::wstring> parts;
            while (std::getline(ss, token, L':'))
                parts.push_back(trim(token)); // usa tu función trim

            if (parts.size() >= 5) {
                conn.nombre = parts[0];
                conn.servidor = parts[1];
                conn.usuario = parts[2];
                conn.contrasena = parts[3];
                conn.old_server = (parts[4] == L"1");
            }

            // 3) El resto de la línea, desde pos hasta final, es la huella
            if (pos != std::wstring::npos && pos < line.size()) {
                conn.hostkey = trim(line.substr(pos));
            }
            break;
        }
    }
    return conn;
}

// Muestra un diálogo “Abrir fichero” y devuelve true si se seleccionó uno.
// La ruta completa queda en outPath (debe reservar como wchar_t[N_MAX_PATH])
bool SelectLocalFile(wchar_t* outPath, DWORD maxLen) {
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;            // tu ventana principal
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = maxLen;
    ofn.lpstrFilter =
        L"Todos los archivos\0*.*\0"
        L"Ficheros de texto\0*.txt\0"
        L"\0";   // doble null final
    ofn.lpstrTitle = L"Selecciona el fichero a subir";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    outPath[0] = L'\0';

    if (!GetOpenFileNameW(&ofn)) {
        //DWORD err = CommDlgExtendedError();
        //// Por depuración temporal:
        //std::wstring msg = L"Dialogo cancelado o error, código: " + std::to_wstring(err);
        //MessageBoxW(hWnd, msg.c_str(), L"Error GetOpenFileName", MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
// = = = = = = = = = = FUNCIONES DE PENDIENTES = = = = = = = =
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =


//Función dedicada a crear paneles de conexión, espera el valor del panel que es el "padre"
void CrearPanelesConexiones(HWND hParentPanel) {
    int numConexiones = ContarLineasConfig();
    int yOffset = 10;
    int xOffset = 10;
    const int panelWidth = 380;
    const int panelHeight = 80;
    const int verticalSpacing = 90;
    const int horizontalSpacing = 400; // Espacio entre columnas
    const int maxPorColumna = 5;

    // Eliminar solo hijos del panel de conexiones
    EnumChildWindows(hParentPanel, [](HWND hwnd, LPARAM) -> BOOL {
        DestroyWindow(hwnd);
        return TRUE;
        }, 0);

    for (int i = 0; i < numConexiones; ++i) {
        if (i % maxPorColumna == 0 && i != 0) {
            // Cada 5 conexiones, pasa a la siguiente columna
            yOffset = 10;
            xOffset += horizontalSpacing;
        }

        Connection conn = GetConnectionFromFile(i);

        HWND hPanel = CreateWindowEx(
            0, L"STATIC", NULL,
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | SS_WHITERECT,
            xOffset, yOffset, panelWidth, panelHeight,
            hParentPanel, NULL, hInst, NULL
        );

        HWND hNombre = CreateWindowEx(
            0, L"STATIC", conn.nombre.c_str(),
            WS_VISIBLE | WS_CHILD | SS_CENTER | WS_CLIPSIBLINGS,
            10, 10, 360, 20,
            hPanel, NULL, hInst, NULL
        );
        SendMessage(hNombre, WM_SETFONT, WPARAM(hFont), TRUE);


        hBtnChildStart = CreateWindowW(L"BUTTON", L"Start", WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            10, 40, 60, 25, hPanel, (HMENU)(1000 + i * 10), hInst, NULL);
        SendMessage(hBtnChildStart, WM_SETFONT, WPARAM(hFont), TRUE);
        hBtnChildStop = CreateWindowW(L"BUTTON", L"Stop", WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            80, 40, 60, 25, hPanel, (HMENU)(1001 + i * 10), hInst, NULL);
        SendMessage(hBtnChildStop, WM_SETFONT, WPARAM(hFont), TRUE);
        hBtnChildSRestart = CreateWindowW(L"BUTTON", L"Restart", WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
            150, 40, 70, 25, hPanel, (HMENU)(1002 + i * 10), hInst, NULL);
        SendMessage(hBtnChildSRestart, WM_SETFONT, WPARAM(hFont), TRUE);

        hBtnChildSStatus = CreateWindowEx(
            0, L"STATIC", L"ON", WS_VISIBLE | WS_CHILD | SS_CENTER,
            320, 45, 40, 20, hPanel, NULL, hInst, NULL
        );
        SendMessage(hBtnChildSStatus, WM_SETFONT, WPARAM(hFont2), TRUE);


        yOffset += verticalSpacing;

    }

    // Estimar el ancho total necesario para el scroll
    int numColumnas = (numConexiones + maxPorColumna - 1) / maxPorColumna;
    int anchoTotal = 10 + numColumnas * horizontalSpacing;

    RECT rcClient;
    GetClientRect(hParentPanel, &rcClient);

    // Ancho total real: por cada columna, espacio horizontal; 
    // al final quitamos el espacio sobrante de la última
    int anchoContenido = numColumnas * horizontalSpacing - (horizontalSpacing - panelWidth);

    // Configurar scrollinfo correctamente
    SCROLLINFO si = { sizeof(si) };
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = anchoContenido - 1;                   // [0…anchoContenido-1]
    si.nPage = min(rcClient.right, si.nMax);         // ancho visible (no mayor que nMax)
    si.nPos = 0;

    SetScrollInfo(hParentPanel, SB_HORZ, &si, TRUE);

    InvalidateRect(hParentPanel, NULL, TRUE);
    UpdateWindow(hParentPanel);
}

//Función dedicada a editar una linea en un fichero, requiere el index de conexión y los datos del struct connection para añadirlos
void ReplaceConnectionInFile(int index, const Connection& conn) {
    // 1) Leemos todas las líneas
    std::wifstream inFile(rutaConfig);
    if (!inFile.is_open()) return;
    std::vector<std::wstring> lines;
    std::wstring line;
    while (std::getline(inFile, line)) {
        lines.push_back(line);
    }
    inFile.close();

    // 2) Verificamos índice válido
    if (index < 0 || index >= (int)lines.size()) return;

    // 3) Reensamblamos la línea con 6 campos
    std::wstringstream ss;
    ss << conn.nombre << L":"
        << conn.servidor << L":"
        << conn.usuario << L":"
        << conn.contrasena << L":"
        << conn.old_server << L":"
        << conn.hostkey;  // el valor que queramos grabar
    lines[index] = ss.str();

    // 4) Reescribimos todo el fichero
    std::wofstream outFile(rutaConfig, std::ios::trunc);
    if (!outFile.is_open()) return;
    for (auto& l : lines) {
        outFile << l << L"\n";
    }
    outFile.close();

    // 5) Refrescamos paneles
    CrearPanelesConexiones(hPanelConexiones);
}

// Una sola vez al arranque:
static std::wstring g_plinkPath;
void InitSSH() {
    g_plinkPath = std::wstring(rutaPrograma) + L"\\plink.exe";
}

enum class SSHCommand {
    FREE = 0,
    START = 1,
    STOP = 2,
    RESTART = 3
};

struct SSHResult {
    int exitCode;
    std::wstring output;
};

SSHResult RunSSHCommand(
    int connIndex,
    SSHCommand mode,
    const std::wstring& freeCmd /* definido en header */
) {
    // 1. Leer conexión
    Connection conn = GetConnectionFromFile(connIndex);
    if (conn.servidor.empty() || conn.usuario.empty()) {
        return { -1, L"Conexión incompleta" };
    }

    // 2. Preparar datos
    std::wstring host = conn.usuario + L"@" + conn.servidor;
    std::wstring pass = conn.contrasena;
    bool bOld = conn.old_server;
    int im = static_cast<int>(mode);

    // 3. Seleccionar comando
    std::wstring cmd;
    if (bOld) {
        // Comandos para servidores antiguos
        const std::wstring cmdso[4] = {
            freeCmd,
            L"ls",
            L"service sensors stop",
            L"service sensors restart"
        };
        cmd = (im >= 1 && im <= 3) ? cmdso[im] : freeCmd;
    }
    else {
        // Comandos para servidores nuevos
        const std::wstring cmds[4] = {
            freeCmd,
            L"systemctl sensors start",
            L"systemctl sensors stop",
            L"systemctl sensors restart"
        };
        cmd = (im >= 1 && im <= 3) ? cmds[im] : freeCmd;
    }

    // 4. Lambda para invocar plink
    auto invoke = [&](const std::wstring& opts) -> SSHResult {
        // Construir línea de comando
        std::wstringstream ss;
        ss << L"\"" << g_plinkPath << L"\" -ssh -batch";
        if (!opts.empty()) ss << opts;
        ss << L" -pw \"" << pass << L"\" " << host;
        if (!cmd.empty()) ss << L" \"" << cmd << L"\"";

        std::wstring line = ss.str();
        OutputDebugStringW((L"Comando a ejecutar: " + line + L"\n").c_str());

        // Crear pipe para capturar salida
        SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
        HANDLE rd = nullptr, wr = nullptr;
        if (!CreatePipe(&rd, &wr, &sa, 0))
            return { -1, L"ERROR: CreatePipe" };
        SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

        // Configurar STARTUPINFO
        PROCESS_INFORMATION pi{};
        STARTUPINFOW si{ sizeof(si) };
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = si.hStdError = wr;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

        // Línea mutable para CreateProcessW
        std::vector<wchar_t> cmdLine(line.begin(), line.end());
        cmdLine.push_back(L'\0');

        // Lanzar proceso
        if (!CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr,
            TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        {
            DWORD err = GetLastError();
            CloseHandle(rd);
            CloseHandle(wr);
            std::wstringstream serr;
            serr << L"ERROR: plink (Windows error " << err << L")";
            return { -1, serr.str() };
        }

        // Cerrar extremo de escritura en padre
        CloseHandle(wr);

        // Leer la salida
        std::string utf8;
        char buffer[4096];
        DWORD bytesRead = 0;
        while (ReadFile(rd, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
            utf8.append(buffer, bytesRead);
        }

        // Esperar a que termine
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        // Cerrar handles restantes
        CloseHandle(rd);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        // Convertir salida a UTF-16
        std::wstring output;
        if (!utf8.empty()) {
            int wsize = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
            if (wsize > 0) {
                output.resize(wsize);
                MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), &output[0], wsize);
            }
        }

        return { (int)exitCode, output };
    };

    // 5. Obtener y guardar hostkey si falta
    if (conn.hostkey.empty()) {
        SSHResult first = invoke(L"");
        const std::wstring marker = L"The server's rsa2 key fingerprint is:";
        size_t pos = first.output.find(marker);
        if (pos != std::wstring::npos) {
            std::wstringstream ss2(first.output.substr(pos + marker.size()));
            std::wstring dummy, fingerprint;
            std::getline(ss2, dummy);
            std::getline(ss2, fingerprint);
            fingerprint = trim(fingerprint);

            std::wstring ask = L"Se ha detectado esta huella:\n\n" + fingerprint + L"\n\n¿Confiar y guardar?";
            if (MessageBoxW(nullptr, ask.c_str(), L"Aceptar hostkey", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                conn.hostkey = fingerprint;
                ReplaceConnectionInFile(connIndex, conn);
            }
            else {
                return { -2, L"Huella rechazada por el usuario" };
            }
        }
        else {
            return first;
        }
    }

    // 6. Ejecutar con hostkey guardada
    std::wstring opts = L" -hostkey \"" + conn.hostkey + L"\"";
    return invoke(opts);
}

bool GeneralSSHFunc(SSHCommand mode)
{
    // 1) Cuenta conexiones
    int total = ContarLineasConfig();
    if (total <= 0) {
        MessageBoxW(hWnd, L"No hay conexiones definidas", L"Atención", MB_OK | MB_ICONWARNING);
        return false;
    }

    // 2) Carga textos OK/ERR
    wchar_t okTxt[128], errTxt[128];
    LoadStringW(hInst, IDS_STRING_OK, okTxt, _countof(okTxt));
    LoadStringW(hInst, IDS_STRING_ERR, errTxt, _countof(errTxt));

    // 3) Ejecuta SSH en cada servidor (por índice) y construye el resumen
    std::wstring resumen;
    bool anyError = false;
    for (int i = 0; i < total; ++i) {
        // Llama a la función con el índice y el modo
        SSHResult r = RunSSHCommand(i, mode, L"");

        // Recupera los datos para mostrar nombre en el resumen
        Connection c = GetConnectionFromFile(i);

        resumen += (r.exitCode == 0 ? okTxt : errTxt);
        anyError |= (r.exitCode != 0);
        resumen += c.nombre + L"\n"
            + L"  Exit: " + std::to_wstring(r.exitCode) + L"\n"
            + L"  Out:  " + L"\n\n" + L"= = = = = = = DUMP = = = = = = =" + L"\n\n" + r.output + L"\n\n" + L"= = = = = = = FIN = = = = = = =" + L"\n\n" + L"\n\n";
    }

    // 4) Genera la ruta del log (wide)
    std::wstring logPathW = rutaPrograma;
    if (!logPathW.empty() && logPathW.back() != L'\\')
        logPathW += L'\\';
    logPathW += L"ssh_resultado.log";

    // 5) Prepara UTF-8 del resumen
    int utf8Len = WideCharToMultiByte(
        CP_UTF8, 0,
        resumen.data(), (int)resumen.size(),
        nullptr, 0, nullptr, nullptr
    );
    std::string resumenUtf8(utf8Len, '\0');
    WideCharToMultiByte(
        CP_UTF8, 0,
        resumen.data(), (int)resumen.size(),
        &resumenUtf8[0], utf8Len, nullptr, nullptr
    );

    // 6) Convierte logPathW → narrow UTF-8 para std::ofstream
    int pathLen = WideCharToMultiByte(
        CP_UTF8, 0,
        logPathW.data(), (int)logPathW.size(),
        nullptr, 0, nullptr, nullptr
    );
    std::string logPath(pathLen, '\0');
    WideCharToMultiByte(
        CP_UTF8, 0,
        logPathW.data(), (int)logPathW.size(),
        &logPath[0], pathLen, nullptr, nullptr
    );

    // 7) Escribe en binario
    std::ofstream ofs(logPath, std::ios::binary | std::ios::trunc);
    if (!ofs) {
        MessageBoxW(hWnd, L"No se pudo escribir ssh_resultado.log", L"Error", MB_OK | MB_ICONERROR);
        // mostramos resumen en pantalla aunque falle el fichero
        MessageBoxW(hWnd, resumen.c_str(), L"SSH Todos", MB_OK | MB_ICONERROR);
        return false;
    }
    // BOM UTF-8
    constexpr unsigned char bom[] = { 0xEF,0xBB,0xBF };
    ofs.write(reinterpret_cast<const char*>(bom), sizeof(bom));
    ofs.write(resumenUtf8.data(), resumenUtf8.size());
    ofs.close();

    // 7) Muestra resultado solo si hubo errores
    if (anyError) {
        MessageBoxW(hWnd, resumen.c_str(), L"SSH Todos - Errores detectados", MB_OK | MB_ICONERROR);
        return false;
    }
    else {
        MessageBoxW(hWnd, L"Todos los servidores respondieron correctamente.", L"SSH Todos", MB_OK | MB_ICONINFORMATION);
        return true;
    }
}


//----------------------------------------------------------------------
// Hace login por FTP y sube un archivo local al servidor remoto
//----------------------------------------------------------------------
// Parámetros:
//   server: nombre o IP del servidor FTP (sin “ftp://”)
//   port:   normalmente INTERNET_DEFAULT_FTP_PORT (21)
//   user/pass: credenciales
//   localFile: ruta completa al fichero en tu disco
//   remoteFile: nombre (o ruta) destino en el servidor
//----------------------------------------------------------------------

//Dialogo de barra de progreso
INT_PTR CALLBACK ProgressDlgProc(HWND dlg, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_INITDIALOG) {
        hProgressBar = GetDlgItem(dlg, IDC_PROGRESS_BAR);
        return TRUE;
    }
    return FALSE;
}

struct FtpResult {
    bool        ok;          // fue FtpPutFileW exitoso?
    DWORD       errCode;     // GetLastError() si ok==false
    std::wstring response;   // respuesta del servidor (o texto del error)
};
FtpResult UploadFileFTP(
    const wchar_t* server,
    INTERNET_PORT   port,
    const wchar_t* user,
    const wchar_t* pass,
    const wchar_t* localFile,
    const wchar_t* remoteFile
) {
    FtpResult result{ false, 0, L"" };

    // Handles y recursos
    HINTERNET hInet = nullptr;
    HINTERNET hFtp = nullptr;
    HINTERNET hFtpFile = nullptr;
    HANDLE    hFile = INVALID_HANDLE_VALUE;

    // 1) Medir fichero
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(localFile, GetFileExInfoStandard, &fad)) {
        result.errCode = GetLastError();
        return result;
    }
    ULONGLONG totalBytes = (ULONGLONG(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;

    // 2) Mostrar diálogo de progreso
    hProgressDlg = CreateDialogParamW(
        hInst, MAKEINTRESOURCEW(IDD_PROGRESS_DLG),
        hWnd, ProgressDlgProc, 0
    );
    ShowWindow(hProgressDlg, SW_SHOW);
    SendMessageW(hProgressBar, PBM_SETRANGE, 0,
        MAKELPARAM(0, (int)(totalBytes / 1024)));
    SendMessageW(hProgressBar, PBM_SETSTEP, (WPARAM)64, 0);

    // 3) InternetOpen
    hInet = InternetOpenW(L"SensorsFTPClient",
        INTERNET_OPEN_TYPE_PRECONFIG,
        nullptr, nullptr, 0);
    if (!hInet) {
        result.errCode = GetLastError();
        DestroyWindow(hProgressDlg);
        return result;
    }

    // 4) InternetConnect
    hFtp = InternetConnectW(
        hInet, server, port,
        user, pass,
        INTERNET_SERVICE_FTP,
        INTERNET_FLAG_PASSIVE,
        0
    );
    if (!hFtp) {
        result.errCode = GetLastError();
        InternetCloseHandle(hInet);
        DestroyWindow(hProgressDlg);
        return result;
    }

    // 5) FtpOpenFile
    hFtpFile = FtpOpenFileW(
        hFtp, remoteFile,
        GENERIC_WRITE,
        FTP_TRANSFER_TYPE_BINARY,
        0
    );
    if (!hFtpFile) {
        result.errCode = GetLastError();
        InternetCloseHandle(hFtp);
        InternetCloseHandle(hInet);
        DestroyWindow(hProgressDlg);
        return result;
    }

    // 6) CreateFile local
    hFile = CreateFileW(
        localFile,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        result.errCode = GetLastError();
        InternetCloseHandle(hFtpFile);
        InternetCloseHandle(hFtp);
        InternetCloseHandle(hInet);
        DestroyWindow(hProgressDlg);
        return result;
    }

    // 7) Loop de envío
    {
        BYTE      buffer[64 * 1024];
        DWORD     readBytes = 0;
        ULONGLONG sentBytes = 0;
        bool      allSent = true;

        while (ReadFile(hFile, buffer, sizeof(buffer), &readBytes, nullptr) && readBytes) {
            DWORD written = 0;
            if (!InternetWriteFile(hFtpFile, buffer, readBytes, &written)) {
                allSent = false;
                result.errCode = GetLastError();
                // Recuperar respuesta FTP
                DWORD dwErr = 0, dwLen = 0;
                InternetGetLastResponseInfoW(&dwErr, nullptr, &dwLen);
                if (dwLen) {
                    std::wstring buf;
                    buf.resize(dwLen);
                    if (InternetGetLastResponseInfoW(&dwErr, &buf[0], &dwLen)) {
                        if (!buf.empty() && buf.back() == L'\0')
                            buf.pop_back();
                        result.response = buf;
                    }
                }
                break;
            }
            sentBytes += written;
            SendMessageW(hProgressBar,
                PBM_SETPOS,
                (WPARAM)(sentBytes / 1024),
                0);

            // Procesa mensajes del diálogo
            MSG msg;
            while (PeekMessageW(&msg, hProgressDlg, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        result.ok = allSent && sentBytes == totalBytes;
    }

    // 8) Limpieza final
    CloseHandle(hFile);
    InternetCloseHandle(hFtpFile);
    InternetCloseHandle(hFtp);
    InternetCloseHandle(hInet);
    DestroyWindow(hProgressDlg);

    return result;
}

// Función para crear el archivo config.txt si no existe
void ConfigFileIfNotExists()
{
    wchar_t rutaFinal[MAX_PATH];
    PathCombine(rutaFinal, rutaPrograma, L"config.txt");

    std::ifstream file(rutaFinal);

    // Si el archivo no existe, lo creamos con datos predeterminados
    if (!file.is_open())
    {
        std::ofstream newFile(rutaFinal);
        if (newFile.is_open())
        {
            newFile.close();
        }
        wcscpy_s(rutaConfig, MAX_PATH, rutaFinal);
    }
    else
    {
        file.close();
    }
}

void ActualizarListaConexiones(HWND hDlg)
{
    // Actualizar contador de conexiones
    int numLineas = ContarLineasConfig();
    wchar_t buffer[16];
    swprintf(buffer, 16, L"%d", numLineas);
    SetDlgItemTextW(hDlg, IDC_CONFIG_CONEXIONES, buffer);

    // Obtener y limpiar el ComboBox
    HWND hCombo = GetDlgItem(hDlg, IDC_CONFI_CONECTIONS);
    if (!hCombo)
    {
        MessageBox(hDlg, L"No se encontró el ComboBox", L"Error", MB_ICONERROR);
        return;
    }

    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);

    std::wifstream file(rutaConfig);
    if (file.is_open())
    {
        std::wstring line;
        while (std::getline(file, line))
        {
            size_t pos = line.find(L":");
            if (pos != std::wstring::npos && pos > 0)
            {
                std::wstring nombre = line.substr(0, pos);
                SendMessage(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(nombre.c_str()));
            }
        }
        file.close();
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);
    }
    else
    {
        MessageBox(hDlg, L"No se pudo abrir el archivo de configuración para el combo", L"Error", MB_ICONERROR);
    }
}


void EliminarLineaSeleccionada(HWND hComboBox, const wchar_t* rutaConfig) {
    // Obtener índice seleccionado
    int selectedIndex = static_cast<int>(SendMessage(hComboBox, CB_GETCURSEL, 0, 0));
    if (selectedIndex == CB_ERR) {
        MessageBox(NULL, L"Selecciona un registro para borrar.", L"Atención", MB_OK | MB_ICONWARNING);
        return;
    }

    // Obtener el nombre seleccionado del ComboBox
    wchar_t selectedName[256];
    SendMessage(hComboBox, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedName);

    // Leer todas las líneas del archivo
    std::wifstream archivoIn(rutaConfig);
    if (!archivoIn.is_open()) {
        MessageBox(NULL, L"No se pudo abrir el archivo.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    std::vector<std::wstring> lineas;
    std::wstring linea;
    while (std::getline(archivoIn, linea)) {
        if (!linea.empty()) {
            lineas.push_back(linea);
        }
    }
    archivoIn.close();

    // Buscar y eliminar la línea correspondiente al nombre
    bool found = false;
    for (auto it = lineas.begin(); it != lineas.end(); ++it) {
        size_t pos = it->find(L":");
        std::wstring nombre = (pos != std::wstring::npos) ? it->substr(0, pos) : *it;
        if (nombre == selectedName) {
            lineas.erase(it);
            found = true;
            break;
        }
    }

    if (!found) {
        MessageBox(NULL, L"No se encontró la conexión para eliminar.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Reescribir el archivo con las líneas restantes
    std::wofstream archivoOut(rutaConfig, std::ios::trunc);
    if (!archivoOut.is_open()) {
        MessageBox(NULL, L"No se pudo escribir en el archivo.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    for (const auto& l : lineas) {
        archivoOut << l << L"\n";
    }
    archivoOut.close();


    // Ajustar la nueva selección
    if (!lineas.empty()) {
        int newIndex = min(selectedIndex, static_cast<int>(lineas.size()) - 1);
        SendMessage(hComboBox, CB_SETCURSEL, newIndex, 0);
    }

    CrearPanelesConexiones(hPanelConexiones);

    MessageBox(NULL, L"Registro eliminado correctamente.", L"Éxito", MB_OK | MB_ICONINFORMATION);
}

//EDITAR UNA CONEXIÓN


//Función para añadir los datos mandados al void

// Cambia la firma para aceptar también el hostkey (por defecto vacío)
void AgregarNuevaConexion(
    const wchar_t* name,
    const wchar_t* host,
    const wchar_t* user,
    const wchar_t* pass,
    bool           server,
    const std::wstring& hostkey = L""
) {
    std::wofstream file(rutaConfig, std::ios_base::app);
    if (file.is_open()) {
        // Ahora escribimos 6 campos separados por ':'
        file << name << L":"
            << host << L":"
            << user << L":"
            << pass << L":"
            << (server ? L"1" : L"0") << L":"
            << hostkey   // puede estar vacío la primera vez
            << L"\n";
        file.close();
        CrearPanelesConexiones(hPanelConexiones);
    }
    else {
        MessageBox(NULL, L"No se pudo abrir el archivo para agregar la conexión.", L"Error", MB_ICONERROR);
    }
}

// FUNCIÓN PARA REESTRUCTURAR LA VENTANA PRINCIPAL
void ResizeControls(HWND hWnd)
{
    RECT rect;
    GetClientRect(hWnd, &rect);

    int anchoTotal = rect.right - rect.left;
    int altoTotal = rect.bottom - rect.top;

    // Altura del panel de controles inferior
    int alturaControles = 130;
    int separacion = 10;

    // Redimensionar panel de conexiones (parte superior)
    MoveWindow(hPanelConexiones, separacion, separacion, anchoTotal - 2 * separacion, altoTotal - alturaControles - 2 * separacion, TRUE);

    // Redimensionar panel de controles (parte inferior)
    int anchoPanel = anchoTotal - 2 * separacion;
    MoveWindow(hPanelControles, separacion, altoTotal - alturaControles - separacion, anchoPanel, alturaControles, TRUE);

    // Centrar el label en el panel inferior
    int anchoLabel = 200;
    // Label centrado dentro del ancho total del panel inferior
    MoveWindow(hLabel, 0, 10, anchoPanel, 20, TRUE);

    // Botones Start, Stop, Restart alineados a la izquierda
    int anchoBoton = 100;
    int altoBoton = 30;
    int espacioBoton = 10;

    int xStart = 20;
    MoveWindow(hBtnStart, xStart, 50, anchoBoton, altoBoton, TRUE);
    MoveWindow(hBtnStop, xStart + (anchoBoton + espacioBoton), 50, anchoBoton, altoBoton, TRUE);
    MoveWindow(hBtnRestart, xStart + 2 * (anchoBoton + espacioBoton), 50, anchoBoton, altoBoton, TRUE);

    // Botón "Subir fichero" alineado a la derecha
    int anchoUpload = 140;
    MoveWindow(hBtnUpload, anchoPanel - anchoUpload - 20, 50, anchoUpload, altoBoton, TRUE);
}


// ===========================================================
// = = = = = = = = = = = = EJECUCIÓN = = = = = = = = = = = = =
// ===========================================================



int APIENTRY wWinMain(_In_ HINSTANCE hInstance,  //LO que se inicia antes de nada
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Inicializar cadenas globales
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_SENSORSCONTROLPANEL, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    WNDCLASSEXW wcexScroll = {};
    wcexScroll.cbSize = sizeof(wcexScroll);
    wcexScroll.style = CS_HREDRAW | CS_VREDRAW;
    wcexScroll.lpfnWndProc = DefWindowProcW;       // usa el proc por defecto
    wcexScroll.hInstance = hInstance;
    wcexScroll.lpszClassName = L"SCROLL_PANEL";
    RegisterClassExW(&wcexScroll);


    //CUSTOM
    GetModuleFileName(NULL, rutaPrograma, MAX_PATH);
    PathRemoveFileSpec(rutaPrograma);

    //Para la barra de progreso
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    //Inicio el guardar la ruta de plink
    InitSSH();

    // Suponiendo que ya tienes rutaPrograma cargada con GetModuleFileName
    wchar_t rutaFinalIni[MAX_PATH] = { 0 };
    PathCombine(rutaFinalIni, rutaPrograma, L"config.txt");

    wcscpy_s(rutaConfig, MAX_PATH, rutaFinalIni); // Actualizar la ruta del archivo


    // Realizar la inicialización de la aplicación:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SENSORSCONTROLPANEL));

    MSG msg;

    // Bucle principal de mensajes:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}


// - - - - - - CONTROLADORES DE DIALOGOS - - - - - -


// Controlador de mensajes del cuadro Acerca de.




INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    }
    return (INT_PTR)FALSE;
}

//Controlador de el config
INT_PTR CALLBACK NewConection(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hDlg, GWLP_HINSTANCE);

    switch (message)
    {
    case WM_INITDIALOG:
    {
        //Añado Tooltip


        AplicarTemaAControles(hDlg);
        DialogData* data = reinterpret_cast<DialogData*>(lParam);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);

        if (data && data->editMode)
        {
            Connection conn = GetConnectionFromFile(data->connectionIndex);

            SetDlgItemText(hDlg, IDC_EDIT_NAME, conn.nombre.c_str());
            SetDlgItemText(hDlg, IDC_EDIT_HOST, conn.servidor.c_str());
            SetDlgItemText(hDlg, IDC_EDIT_USER, conn.usuario.c_str());
            SetDlgItemText(hDlg, IDC_EDIT_PASS, conn.contrasena.c_str());
            CheckDlgButton(hDlg, IDC_EDIT_OLD, conn.old_server ? BST_CHECKED : BST_UNCHECKED);

            SetWindowText(hDlg, L"Editar conexión");
        }
        return (INT_PTR)TRUE;
    }
    case WM_COMMAND:
    {
        wchar_t nombre[100], host[100], user[100], pass[100];
        bool old;

        switch (LOWORD(wParam))
        {
        case IDC_EDIT_SAVE:
        {
            // 1) Recoge y valida
            wchar_t nombre[100], host[100], user[100], pass[100];
            GetDlgItemText(hDlg, IDC_EDIT_NAME, nombre, 100);
            GetDlgItemText(hDlg, IDC_EDIT_HOST, host, 100);
            GetDlgItemText(hDlg, IDC_EDIT_USER, user, 100);
            GetDlgItemText(hDlg, IDC_EDIT_PASS, pass, 100);
            bool old = (IsDlgButtonChecked(hDlg, IDC_EDIT_OLD) == BST_CHECKED);

            if (!*nombre || !*host || !*user || !*pass) {
                MessageBox(hDlg, L"Todos los campos son obligatorios.", L"Advertencia", MB_ICONWARNING);
                break;
            }

            // 2) Guarda o reemplaza la conexión (sin hostkey aún)
            Connection conn{ nombre, host, user, pass, old, L"" };
            DialogData* data = (DialogData*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            int idx = (data && data->editMode)
                ? (data->connectionIndex, ReplaceConnectionInFile(data->connectionIndex, conn), data->connectionIndex)
                : (AgregarNuevaConexion(nombre, host, user, pass, old, L""), ContarLineasConfig() - 1);

            // 3) Lanza SSH FREE para capturar y grabar la huella automáticamente
            RunSSHCommand(idx, SSHCommand::FREE, L"ls");

            EndDialog(hDlg, IDOK);
            return (INT_PTR)TRUE;
        }
        case IDOK:
        {
            EndDialog(hDlg, IDOK);
            return (INT_PTR)TRUE;
        }
        case IDCANCEL:
        {
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
            break;
        }
        }
    }
    }
    return (INT_PTR)FALSE;
}

//Controlador de el config
INT_PTR CALLBACK ConfigProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hDlg, GWLP_HINSTANCE);

    static int indiceSeleccionado = -1; // Se mantiene entre llamadas
    static HWND hComboBox = NULL;

    switch (message)
    {
    case WM_INITDIALOG:
    {
        if (!PathFileExists(rutaConfig))
        {
            LoadStringW(hInst, IDS_Config_Preset, sMultiModal, MAX_LOADSTRING);
            SetDlgItemTextW(hDlg, IDC_ConfigFile, sMultiModal);
            MessageBox(hDlg, L"No se encontró fichero de configuración", L"Error", MB_ICONERROR);

        }
        else {
            hComboBox = GetDlgItem(hDlg, IDC_CONFI_CONECTIONS);

            SetDlgItemTextW(hDlg, IDC_ConfigFile, rutaConfig);

            ActualizarListaConexiones(hDlg);
        }
    }
    return (INT_PTR)TRUE;

    case WM_COMMAND:
    {
        int indexSeleccionado;
        switch (LOWORD(wParam))
        {
            //CONTROLAR UN CAMBIO EN EL COMBO
            if (HIWORD(wParam) == CBN_SELCHANGE && (HWND)lParam == hComboBox)
            {
                indexSeleccionado = (int)SendMessage(hComboBox, CB_GETCURSEL, 0, 0);

                // Opcional: mostrar info para depuración
                // wchar_t texto[256];
                // SendMessage(hComboBox, CB_GETLBTEXT, indiceSeleccionado, (LPARAM)texto);
                // MessageBox(hDlg, texto, L"Seleccionado", MB_OK);
            }

        case IDC_Explorer_Config:
        {
            OPENFILENAME ofn = { 0 };
            WCHAR szFile[MAX_PATH] = { 0 };

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hDlg;
            ofn.lpstrFilter = L"Archivos de config (*.txt)\0*.txt\0Todos los archivos (*.*)\0*.*\0";
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            ofn.lpstrTitle = L"Selecciona un archivo";

            if (GetOpenFileName(&ofn))
            {
                // Aquí puedes guardar la ruta seleccionada, mostrarla o escribirla en el cuadro
                SetDlgItemText(hDlg, IDC_ConfigFile, szFile);

                // Obtener la ruta de destino (por ejemplo, la misma carpeta donde se ejecuta el programa)
                wchar_t rutaFinal[MAX_PATH] = { 0 };
                PathCombine(rutaFinal, rutaPrograma, L"config.txt");

                // Copiar el archivo seleccionado a la ruta de destino
                if (CopyFile(szFile, rutaFinal, FALSE)) {
                    MessageBox(hDlg, L"Archivo copiado correctamente.", L"Éxito", MB_OK);
                    wcscpy_s(rutaConfig, MAX_PATH, rutaFinal); // Actualizar la ruta del archivo
                    SetDlgItemText(hDlg, IDC_ConfigFile, rutaConfig); //Pongo visualmente la ruta del archivo
                }
                else {
                    MessageBox(hDlg, L"No se pudo copiar el archivo.", L"Error", MB_OK | MB_ICONERROR);
                }
            }
            return (INT_PTR)TRUE;
        }
        case IDC_CREAR_CONFIG:
        {
            if (!PathFileExists(rutaConfig))
            {
                ConfigFileIfNotExists();
                MessageBox(hDlg, L"Archivo creado correctamente.", L"Éxito", MB_OK);
                SetDlgItemText(hDlg, IDC_ConfigFile, rutaConfig);
            }
            else {
                MessageBox(hDlg, L"El fichero ya existe.", L"Alerta", MB_HELP);
            }
            break;
        }
        case IDC_CONFIG_NUEVA_CONEXION:
        {
            if (PathFileExists(rutaConfig)) {
                if (DialogBox(hInst, MAKEINTRESOURCE(IDD_EDIT_MENU), hDlg, NewConection) == IDOK) {
                    ActualizarListaConexiones(hDlg);
                }
            }
            else { MessageBox(hDlg, L"Error", L"Alerta", MB_HELP); }
            break;
        }
        case IDC_CONFIG_BORRAR_CONEXION:
        {
            // Obtener la selección del ComboBox
            HWND hComboBox = GetDlgItem(hDlg, IDC_CONFI_CONECTIONS);
            if (hComboBox) {
                // Llamar a la función para eliminar la conexión seleccionada
                EliminarLineaSeleccionada(hComboBox, rutaConfig);
                ActualizarListaConexiones(hDlg);
            }
            break;
        }
        case IDC_CONFIG_EDIT_CONEXION:
        {
            int indexSeleccionado = SendDlgItemMessage(hDlg, IDC_CONFI_CONECTIONS, CB_GETCURSEL, 0, 0);
            if (indexSeleccionado != CB_ERR) {
                DialogData data;
                data.editMode = true;
                data.connectionIndex = indexSeleccionado;

                DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_EDIT_MENU), hDlg, NewConection, (LPARAM)&data);
            }
            else {
                MessageBox(hDlg, L"Selecciona una conexión para editar.", L"Error", MB_ICONERROR);
            }
            break;
        }
        case IDOK:
        case IDCANCEL:
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
        }
    }
    }
    return (INT_PTR)FALSE;
}


LRESULT CALLBACK PanelScrollProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    static int scrollPos = 0;

    switch (msg) {
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
        return 1;   // dices “yo ya lo he pintado”
    }
    case WM_MOUSEWHEEL: {
        // Delta positivo → rueda hacia arriba → scroll a la izquierda
        // Delta negativo → scroll a la derecha
        int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (zDelta > 0)
            SendMessage(hwnd, WM_HSCROLL, SB_LINELEFT, 0);
        else
            SendMessage(hwnd, WM_HSCROLL, SB_LINERIGHT, 0);
        return 0;  // ya lo hemos gestionado
    }
    case WM_HSCROLL: {
        SCROLLINFO si = { sizeof(si) };
        si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_HORZ, &si);

        int oldPos = si.nPos;

        switch (LOWORD(wParam)) {
        case SB_LINELEFT:   si.nPos -= 20; break;
        case SB_LINERIGHT:  si.nPos += 20; break;
        case SB_PAGELEFT:   si.nPos -= 100; break;
        case SB_PAGERIGHT:  si.nPos += 100; break;
        case SB_THUMBTRACK: si.nPos = HIWORD(wParam); break;
        }

        // Limitar el rango
        if (si.nPos < si.nMin) si.nPos = si.nMin;
        if (si.nPos > si.nMax - (int)si.nPage + 1) si.nPos = si.nMax - (int)si.nPage + 1;

        SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
        GetScrollInfo(hwnd, SB_HORZ, &si);

        int dx = oldPos - si.nPos;
        if (dx != 0) {
            ScrollWindowEx(hwnd, dx, 0, NULL, NULL, NULL, NULL, SW_INVALIDATE);

            InvalidateRect(hwnd, NULL, TRUE);   // marca todo el cliente para repintar
            UpdateWindow(hwnd);                // refresca enseguida
            // Mover controles hijos manualmente
            HWND hChild = GetWindow(hwnd, GW_CHILD);
            while (hChild) {
                RECT rc;
                GetWindowRect(hChild, &rc);
                ScreenToClient(hwnd, (LPPOINT)&rc.left);
                ScreenToClient(hwnd, (LPPOINT)&rc.right);

                SetWindowPos(hChild, NULL, rc.left + dx, rc.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                hChild = GetNextWindow(hChild, GW_HWNDNEXT);
            }

            UpdateWindow(hwnd);
        }
        return 0;
    }

    case WM_DESTROY:
        RemoveWindowSubclass(hwnd, PanelScrollProc, 1);
        break;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// = = = = = = = = = = MENÚ INFERIOR = = = = = = = == = = = 
// Subclase para el panel de controles (hPanelControles)
LRESULT CALLBACK PanelControlesProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{

    switch (msg) {
        case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId)
            {
            case IDC_MAIN_BUTTON_START:
                GeneralSSHFunc(SSHCommand::START);
                break;
            case IDC_MAIN_BUTTON_STOP:
                GeneralSSHFunc(SSHCommand::STOP);
                break;
            case IDC_MAIN_BUTTON_RESTART:
                GeneralSSHFunc(SSHCommand::RESTART);
                break;
            case IDC_MAIN_BUTTON_UPLOAD:
            {   // “Subir fichero”
                // 1) Selección de fichero
                wchar_t localPath[MAX_PATH];
                if (!SelectLocalFile(localPath, MAX_PATH))
                    break;
                const wchar_t* remotePath = PathFindFileNameW(localPath);

                // 1b) Validamos el nombre esperado
                const wchar_t* expectedName = L"sensors.sh";
                if (_wcsicmp(remotePath, expectedName) != 0) {
                    std::wstring msg = L"El fichero debe llamarse \"";
                    msg += expectedName;
                    msg += L"\" para poder subirlo.";
                    MessageBoxW(hWnd, msg.c_str(), L"Nombre invalido", MB_OK | MB_ICONERROR);
                    break;  // interrumpe la subida
                }

                // 2) Configuración básica
                WIN32_FILE_ATTRIBUTE_DATA fad;
                if (!GetFileAttributesExW(localPath, GetFileExInfoStandard, &fad)) {
                    MessageBoxW(hWnd, L"No se puede acceder al fichero", L"Error", MB_OK | MB_ICONERROR);
                    break;
                }
                ULONGLONG fileBytes = (ULONGLONG(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;

                int totalConn = ContarLineasConfig();
                if (totalConn <= 0) {
                    MessageBoxW(hWnd, L"No hay conexiones definidas", L"Atención", MB_OK | MB_ICONWARNING);
                    break;
                }

                // 3) Barra de progreso global en BYTES
                ULONGLONG totalAllBytes = fileBytes * (ULONGLONG)totalConn;
                int       maxBytes = (totalAllBytes > INT_MAX) ? INT_MAX : (int)totalAllBytes;

                hProgressDlg = CreateDialogParamW(
                    hInst,
                    MAKEINTRESOURCEW(IDD_PROGRESS_DLG),
                    hWnd,
                    ProgressDlgProc,
                    0
                );
                ShowWindow(hProgressDlg, SW_SHOW);

                // Rango completo en BYTES (32-bit)
                SendMessageW(hProgressBar, PBM_SETRANGE32, 0, maxBytes);
                // Paso de 64 bytes
                SendMessageW(hProgressBar, PBM_SETSTEP, (WPARAM)64, 0);

                ULONGLONG sentAllBytes = 0;
                std::wstring resumen;

                // 4) Iteración sobre cada conexión
                for (int i = 0; i < totalConn; ++i) {
                    Connection conn = GetConnectionFromFile(i);
                    if (conn.servidor.empty() || conn.usuario.empty())
                        continue;

                    FtpResult res{ false, 0, L"" };

                    // Abre WinINet
                    HINTERNET hInet = InternetOpenW(L"SensorsFTPClient",
                        INTERNET_OPEN_TYPE_PRECONFIG,
                        nullptr, nullptr, 0);
                    if (!hInet) {
                        res.errCode = GetLastError();
                    }
                    else {
                        HINTERNET hFtp = InternetConnectW(
                            hInet,
                            conn.servidor.c_str(),
                            INTERNET_DEFAULT_FTP_PORT,
                            conn.usuario.c_str(),
                            conn.contrasena.c_str(),
                            INTERNET_SERVICE_FTP,
                            INTERNET_FLAG_PASSIVE,
                            0
                        );
                        if (!hFtp) {
                            res.errCode = GetLastError();
                        }
                        else {
                            HINTERNET hFtpFile = FtpOpenFileW(
                                hFtp, remotePath,
                                GENERIC_WRITE,
                                FTP_TRANSFER_TYPE_BINARY,
                                0
                            );
                            if (!hFtpFile) {
                                res.errCode = GetLastError();
                            }
                            else {
                                HANDLE hFile = CreateFileW(
                                    localPath,
                                    GENERIC_READ,
                                    FILE_SHARE_READ,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr
                                );
                                if (hFile == INVALID_HANDLE_VALUE) {
                                    res.errCode = GetLastError();
                                }
                                else {
                                    // Envío en bloques y avance de barra en BYTES
                                    BYTE      buffer[64 * 1024];
                                    DWORD     readBytes = 0, written = 0;
                                    ULONGLONG sentThis = 0;
                                    bool      allSent = true;

                                    while (ReadFile(hFile, buffer, sizeof(buffer), &readBytes, nullptr) && readBytes) {
                                        written = 0;
                                        if (!InternetWriteFile(hFtpFile, buffer, readBytes, &written)) {
                                            allSent = false;
                                            res.errCode = GetLastError();
                                            // lee respuesta FTP
                                            DWORD dwErr = 0, dwLen = 0;
                                            InternetGetLastResponseInfoW(&dwErr, nullptr, &dwLen);
                                            if (dwLen) {
                                                std::wstring buf; buf.resize(dwLen);
                                                if (InternetGetLastResponseInfoW(&dwErr, &buf[0], &dwLen)) {
                                                    if (!buf.empty() && buf.back() == L'\0') buf.pop_back();
                                                    res.response = buf;
                                                }
                                            }
                                            break;
                                        }
                                        sentThis += written;
                                        sentAllBytes += written;

                                        // **Actualización de la barra en BYTES**
                                        int pos = (sentAllBytes > INT_MAX) ? INT_MAX : (int)sentAllBytes;
                                        SendMessageW(hProgressBar, PBM_SETPOS, (WPARAM)pos, 0);

                                        // UI responsive
                                        MSG msg;
                                        while (PeekMessageW(&msg, hProgressDlg, 0, 0, PM_REMOVE)) {
                                            TranslateMessage(&msg);
                                            DispatchMessageW(&msg);
                                        }
                                    }
                                    res.ok = allSent && (sentThis == fileBytes);
                                    CloseHandle(hFile);
                                }
                                InternetCloseHandle(hFtpFile);
                            }
                            InternetCloseHandle(hFtp);
                        }
                        InternetCloseHandle(hInet);
                    }

                    // Acumula en el resumen
                    wchar_t okTxt[128], errTxt[128];
                    LoadStringW(hInst, IDS_STRING_OK, okTxt, _countof(okTxt));
                    LoadStringW(hInst, IDS_STRING_ERR, errTxt, _countof(errTxt));
                    resumen += res.ok ? okTxt : errTxt;
                    resumen += L" " + conn.nombre + L" (" + conn.servidor + L")\n";
                    if (!res.ok) {
                        resumen += L"    Win32 Err: " + std::to_wstring(res.errCode) + L"\n";
                        if (!res.response.empty())
                            resumen += L"    FTP Resp: " + res.response + L"\n";
                    }
                    resumen += L"\n";
                }

                // 5) Cierra diálogo de progreso
                DestroyWindow(hProgressDlg);
                hProgressDlg = nullptr;

                // 6) Mensaje final + OK/Cancel + Yes/No
                int respuesta = MessageBoxW(
                    hWnd,
                    resumen.c_str(),
                    L"Resultado de la subida FTP",
                    MB_OK | MB_ICONINFORMATION
                );
                if (respuesta == IDOK) {
                    wchar_t question[256];
                    LoadStringW(hInst, IDS_STRING_QUESTION, question, _countof(question));
                    int opcion = MessageBoxW(hWnd, question, L"Reinicio", MB_YESNO | MB_ICONQUESTION);
                    if (opcion == IDYES) {
                        MessageBoxW(hWnd, L"Sensors reiniciado en todos los servidores", L"Éxito", MB_OK | MB_ICONINFORMATION);

                        /*if (DeleteFileW(localPath))
                            MessageBoxW(hWnd, L"Fichero borrado.", L"Éxito", MB_OK | MB_ICONINFORMATION);
                        else
                            MessageBoxW(hWnd, L"No se pudo borrar el fichero.", L"Error", MB_OK | MB_ICONERROR);*/
                    }
                    else {
                        //Por si queremos poner algo en el no reniciar
                    }
                }
            }
            break;

            // Reenvía al padre (la ventana principal)
            //return SendMessage(
            //    GetParent(hwnd),
            //    WM_COMMAND,
            //    wParam,
            //    lParam
            //);
            }
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}


//=========== MENÚ PRINCIPAL ==================

// FUNCIÓN: MyRegisterClass()
//
// PROPÓSITO: Registra la clase de ventana.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SENSORSCONTROLPANEL));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_SENSORSCONTROLPANEL);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

// FUNCIÓN: InitInstance(HINSTANCE, int)
//
// PROPÓSITO: Guarda el identificador de instancia y crea la ventana principal
//

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    wcscpy_s(szTitle, L"Sensors");
    int altoControles = 100;
    int anchoVentana = 820;
    int altoVentana = 690;

    // Permite cambiar tamaño con el ratón:
    DWORD estiloVentana = WS_OVERLAPPED
        | WS_CAPTION
        | WS_SYSMENU
        | WS_THICKFRAME;    // o WS_SIZEBOX, mismo efecto

    hWnd = CreateWindowW(szWindowClass, szTitle, estiloVentana,
        CW_USEDEFAULT, CW_USEDEFAULT, anchoVentana, altoVentana,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
        return FALSE;

    // Crea la fuente con CreateFontW
    hFont = CreateFontW(
        16,                  // Altura de la fuente
        0,                   // Ancho (0 ajusta automáticamente)
        0,                   // Ángulo de inclinación
        0,                   // Ángulo de orientación
        FW_NORMAL,           // Grosor normal
        0,                   // No es cursiva
        0,                   // No subrayado
        0,                   // No tachado
        DEFAULT_CHARSET,     // Juego de caracteres predeterminado
        OUT_DEFAULT_PRECIS,  // Precisión de salida
        CLIP_DEFAULT_PRECIS, // Precisión de recorte
        CLEARTYPE_QUALITY,   // Calidad de la fuente
        DEFAULT_PITCH | FF_SWISS, // Estilo de fuente y familia
        L"Segoe UI"          // Nombre de la fuente
    );

    hFont2 = CreateFontW(
        16,                  // Altura de la fuente
        0,                   // Ancho (0 ajusta automáticamente)
        0,                   // Ángulo de inclinación
        0,                   // Ángulo de orientación
        FW_NORMAL,           // Grosor normal
        0,                   // No es cursiva
        0,                   // No subrayado
        0,                   // No tachado
        DEFAULT_CHARSET,     // Juego de caracteres predeterminado
        OUT_DEFAULT_PRECIS,  // Precisión de salida
        CLIP_DEFAULT_PRECIS, // Precisión de recorte
        CLEARTYPE_QUALITY,   // Calidad de la fuente
        DEFAULT_PITCH | FF_SWISS, // Estilo de fuente y familia
        L"Berlin Sans FB Demi"          // Nombre de la fuente
    );

    if (!hFont || !hFont2)
        return FALSE;

    // Panel de conexiones (parte superior)
    hPanelConexiones = CreateWindowW(
        L"SCROLL_PANEL",         // <-- Usa tu nueva clase
        NULL,
        WS_VISIBLE | WS_CHILD
        | WS_HSCROLL           // barra horizontal
        | WS_BORDER
        | WS_CLIPCHILDREN,     // evita que los hijos se pinten fuera
        10, 10, 600, 300,        // posición y tamaño inicial
        hWnd, NULL, hInstance, nullptr
    );
    SetWindowSubclass(hPanelConexiones, PanelScrollProc, 1, 0);

    // Panel inferior para controles generales
    hPanelControles = CreateWindowW(
        L"STATIC", NULL,
        WS_VISIBLE | WS_CHILD | SS_WHITERECT,
        10, 320, 600, 150,  // Posición provisional
        hWnd, NULL, hInstance, nullptr
    );
    // tras tu CreateWindowW(...) de hPanelControles:
    SetWindowSubclass(hPanelControles,PanelControlesProc, 3, 0);


    // Crear controles generales dentro del panel inferior
    hLabel = CreateWindowW(L"STATIC", L"Funciones generales", WS_VISIBLE | WS_CHILD | SS_CENTER,
        10, 10, 200, 20, hPanelControles, nullptr, hInstance, nullptr);
    SendMessage(hLabel, WM_SETFONT, WPARAM(hFont), TRUE);

    hBtnStart = CreateWindowW(L"BUTTON", L"Start", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        10, 40, 80, 30, hPanelControles, (HMENU)IDC_MAIN_BUTTON_START, hInstance, nullptr);
    SendMessage(hBtnStart, WM_SETFONT, WPARAM(hFont), TRUE);

    hBtnStop = CreateWindowW(L"BUTTON", L"Stop", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        100, 40, 80, 30, hPanelControles, (HMENU)IDC_MAIN_BUTTON_STOP, hInstance, nullptr);
    SendMessage(hBtnStop, WM_SETFONT, WPARAM(hFont), TRUE);

    hBtnRestart = CreateWindowW(L"BUTTON", L"Restart", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        190, 40, 80, 30, hPanelControles, (HMENU)IDC_MAIN_BUTTON_RESTART, hInstance, nullptr);
    SendMessage(hBtnRestart, WM_SETFONT, WPARAM(hFont), TRUE);

    hBtnUpload = CreateWindowW(L"BUTTON", L"Subir fichero", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        280, 40, 120, 30, hPanelControles, (HMENU)IDC_MAIN_BUTTON_UPLOAD, hInstance, nullptr);
    SendMessage(hBtnUpload, WM_SETFONT, WPARAM(hFont), TRUE);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Crear los paneles de conexión dentro del panel superior
    CrearPanelesConexiones(hPanelConexiones);

    return TRUE;
}


// FUNCIÓN: WndProc(HWND, UINT, WPARAM, LPARAM)
//
// PROPÓSITO: Procesa mensajes de la ventana principal.
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ENTERSIZEMOVE:
        g_enSizing = true;
        break;
    case WM_EXITSIZEMOVE:
        g_enSizing = false;
        break;
    case WM_HSCROLL:
    {
        SCROLLINFO si = { sizeof(si) };
        si.fMask = SIF_ALL;
        GetScrollInfo(hWnd, SB_HORZ, &si);
        int xOld = si.nPos;
        // Ajustar si.nPos según código de scroll
        switch (LOWORD(wParam)) {
        case SB_LINELEFT:   si.nPos -= 10; break;    // desplazar 10 px (un "paso")
        case SB_LINERIGHT:  si.nPos += 10; break;
        case SB_PAGELEFT:   si.nPos -= si.nPage; break;
        case SB_PAGERIGHT:  si.nPos += si.nPage; break;
        case SB_THUMBTRACK: si.nPos = si.nTrackPos; break;
            // ...otros casos SB_LEFT/SB_RIGHT si se desean...
        default: break;
        }
        // Asegurar rango [nMin,nMax]
        si.nPos = max(si.nMin, min(si.nMax, si.nPos));
        // Actualizar scroll bar y desplazar si cambió la posición
        si.fMask = SIF_POS;
        SetScrollInfo(hWnd, SB_HORZ, &si, TRUE);
        if (si.nPos != xOld) {
            // Desplaza el contenido horizontalmente
            ScrollWindow(hWnd, xOld - si.nPos, 0, NULL, NULL);
            UpdateWindow(hWnd); // opcional para repintar inmediatamente
        }
        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        // si Shift está pulsado, simular scroll horizontal por línea
        if (wParam & MK_SHIFT) {
            int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (zDelta > 0)
                SendMessage(hWnd, WM_HSCROLL, SB_LINELEFT, 0);
            else
                SendMessage(hWnd, WM_HSCROLL, SB_LINERIGHT, 0);
        }
        break;
    }
    case WM_GETMINMAXINFO :
    {
        auto mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        // Establece la altura mínima y máxima idéntica a la altura inicial:
        int altoInicial = 690;  // pon aquí tu altoVentana original
        mmi->ptMinTrackSize.y = altoInicial;
        mmi->ptMaxTrackSize.y = altoInicial;
        return 0;
    }
    case WM_SIZE:
        ResizeControls(hWnd);

        if (!g_enSizing) {
            // Al terminar de redimensionar, rehacemos todo
            CrearPanelesConexiones(hPanelConexiones);
        }
        else {
            // Mientras arrastro, solo actualizo el tamaño de página
            RECT rc;
            GetClientRect(hPanelConexiones, &rc);

            SCROLLINFO si;
            ZeroMemory(&si, sizeof(si));
            si.cbSize = sizeof(si);
            si.fMask = SIF_PAGE;
            si.nPage = rc.right;   // ancho visible
            SetScrollInfo(hPanelConexiones, SB_HORZ, &si, TRUE);
        }
    break;    
    case WM_COMMAND: // == ACCIONES DEL PANEL PRINCIPAL ==
    {
        int wmId = LOWORD(wParam);

        // Analizar las selecciones de menú:
        switch (wmId)
        {
        case IDM_CONFIG:
            // Llamamos a una función que muestre la configuración
            DialogBox(hInst, MAKEINTRESOURCE(IDD_CONFIG_WINDOW), hWnd, ConfigProc);
            break;
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: Agregar cualquier código de dibujo que use hDC aquí...
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
