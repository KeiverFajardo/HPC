#include <mpi.h>
#include <vector>

#include "algoritmo_a.hpp"
#include "algoritmo_b.hpp"
#include "mpi_datatypes.hpp"

#include "gnuplot-iostream.h"

#include "common_constants.hpp"

int main(int argc, char *argv[])
{
    GDALAllRegister();
    MPI_Init(&argc, &argv);
    init_mpi_datatypes();

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
 
    std::vector<const char *> files;

    for (int i = 1; i < argc; i++)
        files.emplace_back(argv[i]);

    if (world_rank == MASTER_RANK)
    {
        AlgoritmoA algoritmo("../shapefiles/procesado.shp", std::move(files));
        
        algoritmo.procesar();
    }
    else
    {
        // --- Procesos B: Esclavos ---
        procesar_b("../shapefiles/procesado.shp", std::move(files));
    }

    free_mpi_datatypes();
    MPI_Finalize();
    return 0;
}
