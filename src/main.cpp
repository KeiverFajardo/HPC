#include <mpi.h>
#include <utility>
#include <vector>

#include "csv_reader.hpp"
#include "shapefile_loader.hpp"
#include "municipio_mapper.hpp"
#include "franja_horaria.hpp"
#include "algoritmo_b.hpp"
#include "log.hpp"
#include "mpi_datatypes.hpp"

constexpr int TAG_DATA = 0;
constexpr int TAG_FIN = 1;

constexpr int MASTER_RANK = 0;
constexpr int EXIT_MESSAGE_TAG = 1;
constexpr int BLOCK_SIZE = 8;

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    init_mpi_datatypes();

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (world_rank == MASTER_RANK)
    {
        /* bool exit = false;
        float total_speed_sum = 0.0f;
        unsigned int blocks_pending = 0;
        int next_slave_rank_to_work = 0; */

        // --- Proceso A: Maestro ---
        info("world_size: {}", world_size);

        // ←---- Mapper: long/lat -> municipio_id
        MunicipioMapper mapper("../shapefiles/sig_municipios.shp");

        CsvReader csv_reader("../only64.csv");
        Register r;

        std::vector<Register> buffer;
        buffer.reserve(BLOCK_SIZE);

        bool exit = false;
        int next_slave = 1;
        int blocks_sent = 0;

        // ←---- Envío de bloques a esclavos
        /* int flag;
        MPI_Status status;
        while (!exit || blocks_pending > 0) */
        while (!exit)
        {
            /* MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);
            if (flag)
            {
                float sum;
                MPI_Recv(
                    &sum,
                    1,
                    MPI_FLOAT,
                    status.MPI_SOURCE,
                    status.MPI_TAG,
                    MPI_COMM_WORLD,
                    &status
                );

                total_speed_sum += sum;
                blocks_pending--;
            } */

            buffer.clear();

            for (int i = 0; i < BLOCK_SIZE; ++i)
            {
                if (!csv_reader.get(r))
                {
                    exit = true;
                    break;
                }

                r.municipio_id = mapper.codificar(Punto { r.latitud, r.longitud });
                r.franja_horaria = std::to_underlying(get_franja_horaria(r.hora)); // TODO: dividir por franja horaria

                buffer.push_back(r);
            }

            if (!buffer.empty())
            {
                /* MPI_Send(
                    registers.data(),
                    registers.size(),
                    MPI_Register,
                    next_slave_rank_to_work + 1,
                    0,
                    MPI_COMM_WORLD
                ); 
                registers.clear();
                blocks_pending++;
                next_slave_rank_to_work = (next_slave_rank_to_work + 1) % (world_size - 1);
                */
                // info("BUFFER INICIO");
                // for (auto reg : buffer)
                //     info("{}", reg);
                // info("BUFFER FIN");
                MPI_Send(buffer.data(), buffer.size(), MPI_Register, next_slave, 0, MPI_COMM_WORLD);
                blocks_sent++;
                next_slave++;
                if (next_slave >= world_size) next_slave = 1;
            }
        }

        /* for (int slave_rank = 1; slave_rank < world_size; slave_rank++)
            MPI_Send(nullptr, 0, MPI_INT, slave_rank, EXIT_MESSAGE_TAG, MPI_COMM_WORLD); */
        // ←---- Señales de terminación
        for (int i = 1; i < world_size; ++i)
        {
            MPI_Send(nullptr, 0, MPI_Register, i, EXIT_MESSAGE_TAG, MPI_COMM_WORLD);
        }


        // ←---- Recepción de resultados estadísticos
        for (int i = 0; i < blocks_sent; ++i)
        {
            MPI_Status status;
            int count;

            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_ResultadoEstadistico, &count);

            std::vector<ResultadoEstadistico> resultados(count);
            MPI_Recv(resultados.data(), count, MPI_ResultadoEstadistico, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            for (const auto& r : resultados)
            {
                info("Municipio {}, Franja {} => Prom: {:.2f}, Desv: {:.2f}, N: {}, Rank origen: {}",
                     (int)r.municipio_id, (int)r.franja_horaria, r.promedio, r.desvio, r.cantidad, status.MPI_SOURCE);
            }
        }
    }
    else
    {
        // --- Procesos B: Esclavos ---
        std::vector<Register> buffer(BLOCK_SIZE);

        while (true)
        {
            MPI_Status status;
            /* MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status); */
            MPI_Probe(MASTER_RANK, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            if (status.MPI_TAG == EXIT_MESSAGE_TAG)
                break;

            int count;
            MPI_Get_count(&status, MPI_Register, &count);
            buffer.resize(count);
            
            MPI_Recv(buffer.data(), count, MPI_Register, MASTER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            info("BUFFER INICIO");
            for (auto reg : buffer)
                info("{}", reg);
            info("BUFFER FIN");

            /* float sum = 0.0f;
            for (Register &reg : registers)
                sum += reg.velocidad; */

            std::vector<ResultadoEstadistico> resultados = analizar_bloque(buffer);
            MPI_Send(resultados.data(), resultados.size(), MPI_ResultadoEstadistico, MASTER_RANK, 0, MPI_COMM_WORLD);
        }
    }

    free_mpi_datatypes();
    MPI_Finalize();
    return 0;
}
