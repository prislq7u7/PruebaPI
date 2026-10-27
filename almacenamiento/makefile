CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread

FS_OBJ = FileSystem.o Inventario.o
FS_H = Formatos.h FileSystem.h Inventario.h

TARGETS = mkfs agregar_bodega extraer_bodega borrar_bodega listar_fs dump_fs

all: $(TARGETS)

%.o: %.cc $(FS_H)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TARGETS): %: %.o $(FS_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# formatea fs1.dat.ci0123 (servidor 1) y fs2.dat.ci0123 (servidor 2) con sus bodegas
discos: all
	./crear_discos.sh

clean:
	rm -f $(TARGETS) *.o

distclean: clean
	rm -f *.dat.ci0123

.PHONY: all discos clean distclean
