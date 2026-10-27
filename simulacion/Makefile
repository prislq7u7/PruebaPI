FUENTES = Buzon.cc Validaciones.cc Bitacora.cc Bodega.cc Intermediario.cc Cliente.cc simulacion.cc

simulacion: $(FUENTES)
	g++ -std=c++17 -pthread -Wall $(FUENTES) -o simulacion

clean:
	rm -f simulacion
	-ipcrm -M 0xB54321 2>/dev/null