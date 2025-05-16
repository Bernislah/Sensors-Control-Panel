// libssh2_client.h
#pragma once

#include <string>
#include <libssh2.h>
#include "Sensors Control Panel.cpp" // ContarLineasConfig, GetConnectionFromFile, ReplaceConnectionInFile

struct SSHResult {
    int exitCode;
    std::wstring output;
};

enum class SSHCommand {
    FREE,
    START,
    STOP,
    RESTART
};

// Inicializar/Cerrar libssh2
bool InitSSH();
void CleanupSSH();

// Ejecuta el comando SSH usando libssh2, gestiona hostkey internamente
SSHResult RunSSHCommandLibssh2(
    int connIndex,
    SSHCommand mode,
    const std::wstring& freeCmd = L""
);


// libssh2_client.cpp
#include "libssh2_client.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>

static bool g_sshInitialized = false;

bool InitSSH() {
    if (g_sshInitialized) return true;
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
        return false;
    if (libssh2_init(0) != 0) {
        WSACleanup();
        return false;
    }
    g_sshInitialized = true;
    return true;
}

void CleanupSSH() {
    if (!g_sshInitialized) return;
    libssh2_exit();
    WSACleanup();
    g_sshInitialized = false;
}

static std::string WideToUtf8(const std::wstring& w) {
    int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], len, nullptr, nullptr);
    return s;
}

static std::wstring Utf8ToWide(const std::string& s) {
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], len);
    return w;
}

SSHResult RunSSHCommandLibssh2(
    int connIndex,
    SSHCommand mode,
    const std::wstring& freeCmd
) {
    // Asegurar init
    if (!InitSSH()) return { -1, L"SSH init failed" };

    // 0) Leer conexión y preparar
    Connection conn = GetConnectionFromFile(connIndex);
    std::string user = WideToUtf8(conn.usuario);
    std::string host = WideToUtf8(conn.servidor);
    std::string pass = WideToUtf8(conn.contrasena);

    const std::string cmds[] = {
        WideToUtf8(freeCmd),
        "service sensors start",
        "service sensors stop",
        "service sensors restart"
    };
    int im = static_cast<int>(mode);
    std::string cmd = (im >= 1 && im <= 3) ? cmds[im] : cmds[0];

    // 1) Abrir socket
    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res;
    getaddrinfo(host.c_str(), "22", &hints, &res);
    SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    connect(sock, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);

    // 2) Sesión SSH
    LIBSSH2_SESSION* sess = libssh2_session_init();
    libssh2_session_handshake(sess, sock);
    libssh2_userauth_password(sess, user.c_str(), pass.c_str());

    // 3) Ejecutar comando
    LIBSSH2_CHANNEL* channel = libssh2_channel_open_session(sess);
    libssh2_channel_exec(channel, cmd.c_str());

    // 4) Leer salida
    std::ostringstream oss;
    char buffer[4096];
    for (;;) {
        ssize_t n = libssh2_channel_read(channel, buffer, sizeof(buffer));
        if (n <= 0) break;
        oss.write(buffer, n);
    }
    int exitcode = libssh2_channel_get_exit_status(channel);
    libssh2_channel_close(channel);
    libssh2_channel_free(channel);

    // 5) Cerrar sesión
    libssh2_session_disconnect(sess, "Bye");
    libssh2_session_free(sess);
    closesocket(sock);

    // 6) Construir resultado
    std::string out8 = oss.str();
    std::wstring out16 = Utf8ToWide(out8);
    return { exitcode, out16 };
}
