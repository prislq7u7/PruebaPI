#include "HttpUtil.h"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdlib>

std::string urlEncode(const std::string& s) {
    std::ostringstream enc;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            enc << c;
        } else if (c == ' ') {
            enc << "%20";
        } else {
            enc << '%' << std::uppercase << std::hex << std::setw(2)
                << std::setfill('0') << (int)c << std::nouppercase << std::dec;
        }
    }
    return enc.str();
}

std::string urlDecode(const std::string& s) {
    std::string resultado;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            std::string hex = s.substr(i + 1, 2);
            resultado += (char)strtol(hex.c_str(), nullptr, 16);
            i += 2;
        } else if (s[i] == '+') {
            resultado += ' ';
        } else {
            resultado += s[i];
        }
    }
    return resultado;
}

std::string buildGET(const std::string& path, const std::string& host) {
    return "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
}

int getCodigoEstado(const std::string& resp) {
    if (resp.rfind("HTTP/", 0) != 0) return -1;
    size_t espacio1 = resp.find(' ');
    if (espacio1 == std::string::npos) return -1;
    size_t espacio2 = resp.find(' ', espacio1 + 1);
    if (espacio2 == std::string::npos) return -1;
    try {
        return std::stoi(resp.substr(espacio1 + 1, espacio2 - espacio1 - 1));
    } catch (...) {
        return -1;
    }
}

std::string extraerCuerpo(const std::string& resp) {
    size_t pos = resp.find("\r\n\r\n");
    if (pos == std::string::npos) return resp;
    return resp.substr(pos + 4);
}

// la direccion completa es lo que esta entre el primer y el segundo espacio: "GET /ruta?a=1 HTTP/1.1"
static std::string extraerUrl(const std::string& peticion) {
    size_t inicio = peticion.find(' ');
    if (inicio == std::string::npos) return "";
    size_t fin = peticion.find(' ', inicio + 1);
    if (fin == std::string::npos) return "";
    return peticion.substr(inicio + 1, fin - inicio - 1);
}

std::string extraerRuta(const std::string& peticion) {
    std::string url = extraerUrl(peticion);
    size_t signo = url.find('?');
    return (signo == std::string::npos) ? url : url.substr(0, signo);
}

std::string parametroUrl(const std::string& peticion, const std::string& nombre) {
    std::string url = extraerUrl(peticion);
    size_t signo = url.find('?');
    if (signo == std::string::npos) return "";

    // los parametros van separados por '&', cada uno con la forma clave=valor
    std::stringstream ss(url.substr(signo + 1));
    std::string par;
    while (std::getline(ss, par, '&')) {
        size_t igual = par.find('=');
        if (igual == std::string::npos) continue;
        if (urlDecode(par.substr(0, igual)) == nombre) return urlDecode(par.substr(igual + 1));
    }
    return "";
}

std::string construirRespuestaHttp(int codigo, const std::string& cuerpo, const std::string& tipo) {
    const char* texto = "Bad Request";
    if (codigo == 200) texto = "OK";
    else if (codigo == 404) texto = "Not Found";
    else if (codigo == 405) texto = "Method Not Allowed";
    else if (codigo == 502) texto = "Bad Gateway";

    std::ostringstream oss;
    oss << "HTTP/1.1 " << codigo << " " << texto << "\r\n"
        << "Content-Type: " << tipo << "\r\n"
        << "Content-Length: " << cuerpo.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << cuerpo;
    return oss.str();
}