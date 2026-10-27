#ifndef CERTIFICADO_H
#define CERTIFICADO_H

#include <iostream>
#include <cstdlib>
#include <sys/stat.h>

// certificado y llave (en el mismo archivo) de los programas que escuchan por TLS:
// el intermediario y los servidores de productos. Se crea una vez con "make certificado"
static const char* const ARCHIVO_CERTIFICADO = "certificados/ci0123.pem";

// sin el certificado no se puede atender por TLS: se explica como crearlo y se termina
inline void exigirCertificado() {
    struct stat info;
    if (stat(ARCHIVO_CERTIFICADO, &info) == 0) return;

    std::cerr << "no se encontro " << ARCHIVO_CERTIFICADO << ". Se crea con:\n"
              << "  make certificado\n"
              << "que hace lo mismo que:\n"
              << "  mkdir -p certificados\n"
              << "  openssl req -x509 -newkey rsa:2048 -keyout certificados/key.pem -out certificados/cert.pem"
                 " -days 365 -nodes -subj \"/C=CR/O=UCR/CN=ci0123\"\n"
              << "  cat certificados/key.pem certificados/cert.pem > " << ARCHIVO_CERTIFICADO << "\n";
    std::exit(1);
}

#endif
