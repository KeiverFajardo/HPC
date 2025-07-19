#include <mpi.h>
#include <vector>

#include "algoritmo_a.hpp"
#include "algoritmo_b.hpp"
#include "log.hpp"
#include "mpi_datatypes.hpp"

#include "gnuplot-iostream.h"

#include "common_constants.hpp"

void plot(const std::vector<float> &barras)
{
    Gnuplot gp;
	// Create a script which can be manually fed into gnuplot later:
	//    Gnuplot gp(">script.gp");
	// Create script and also feed to gnuplot:
	//    Gnuplot gp("tee plot.gp | gnuplot -persist");
	// Or choose any of those options at runtime by setting the GNUPLOT_IOSTREAM_CMD
	// environment variable.
    
    //const_cast<std::vector<float>&>(barras)[1] = 3.0f;
    
    info("BARRAS BEGIN");
    for (auto barra : barras)
        info("{}", barra);
    info("BARRAS END");

	// Don't forget to put "\n" at the end of each line!
    gp << "set terminal gif size 300,200 animate delay 2\n";
    gp << "set output 'image.gif'\n";
    gp << "set style fill solid\n";
    gp << "set boxwidth 0.5\n";
	gp << "plot '-' with boxes title 'pts_A'\n";
	gp.send1d(barras);
    const_cast<std::vector<float>&>(barras)[1] = 3.0f;
	gp << "plot '-' with boxes title 'pts_A'\n";
	gp.send1d(barras);
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    init_mpi_datatypes();

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    
    std::vector<std::string> files = {
        "../04_2025.csv",
        "../05_2025.csv",
    };

    if (world_rank == MASTER_RANK)
    {
        AlgoritmoA algoritmo("../shapefiles/procesado.shp");
        
        algoritmo.procesar(std::move(files));

        // std::vector<float> barras;
        // for (auto &res_estadistico : resultados)
        // {
        //     if (barras.size() < res_estadistico.municipio_id)
        //         barras.resize(res_estadistico.municipio_id);
        //     barras[res_estadistico.municipio_id] = res_estadistico.promedio;
        // }
        // plot(barras);
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
