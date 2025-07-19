#include <mpi.h>
#include <vector>

#include "algoritmo_a.hpp"
#include "algoritmo_b.hpp"
#include "mpi_datatypes.hpp"

#include "gnuplot-iostream.h"

#include "common_constants.hpp"

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    init_mpi_datatypes();

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    
    std::vector<std::string> files = {
        "../autoscope_04_2025_velocidad.csv",
        // "../autoscope_05_2025_velocidad.csv",
        // "../04_2025_short.csv",
    };

    if (world_rank == MASTER_RANK)
    {
        AlgoritmoA algoritmo("../shapefiles/procesado.shp");
        
        algoritmo.procesar(std::move(files));
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
