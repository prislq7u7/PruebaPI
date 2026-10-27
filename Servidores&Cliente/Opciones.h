#ifndef OPCIONES_H
#define OPCIONES_H

#include <string>

// lectura de opciones con nombre de la linea de comandos: ./programa --intermediario 10.1.35.20 --puerto 8080

// valor de "--nombre valor"; porDefecto si no viene
std::string opcion(int argc, char* argv[], const std::string& nombre, const std::string& porDefecto);

bool tieneOpcion(int argc, char* argv[], const std::string& nombre);

// primer argumento que no es una opcion (--algo) ni el valor de una opcion
std::string argumentoSuelto(int argc, char* argv[], int cual = 0);

// primera IPv4 de la maquina que no es 127.x; asi no hay que escribirla para probar en una sola computadora
std::string ipLocal();

#endif
