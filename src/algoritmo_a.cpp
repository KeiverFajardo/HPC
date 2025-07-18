#include "algoritmo_a.hpp"

#include <utility>

#include "algoritmo_b.hpp"
#include "log.hpp"
#include "mpi_datatypes.hpp"
#include "common_constants.hpp"

constexpr int BLOCK_SIZE = 8;

AlgoritmoA::AlgoritmoA(const char *csv_path, const std::string &shapefile_path)
    : lector_csv(csv_path), mapper(shapefile_path)
{}

bool AlgoritmoA::cargar_bloque(std::vector<Register> &bloque, std::size_t block_size)
{
    bloque.clear();
    Register reg;

    while (bloque.size() < block_size && lector_csv.get(reg))
    {
        // Asignar municipio
        reg.municipio_id = mapper.codificar(Punto { reg.latitud, reg.longitud });

        // Asignar franja horaria
        reg.franja_horaria = std::to_underlying(get_franja_horaria(reg.hora));

        bloque.push_back(reg);
        registros_procesados++;
    }

    return !bloque.empty();
}

void AlgoritmoA::procesar()
{
    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

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

        cargar_bloque(buffer, BLOCK_SIZE);

        for (int i = 0; i < BLOCK_SIZE; ++i)
        {
            if (!lector_csv.get(r))
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

    std::vector<ResultadoEstadistico> resultados;
    // ←---- Recepción de resultados estadísticos
    for (int i = 0; i < blocks_sent; ++i)
    {
        MPI_Status status;
        int count;

        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_ResultadoEstadistico, &count);

        resultados.clear();
        resultados.resize(count);
        MPI_Recv(resultados.data(), count, MPI_ResultadoEstadistico, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        for (const auto& r : resultados)
        {
            std::string nombre_municipio = mapper.decodificar(r.municipio_id);
            info("Municipio {}, Franja {} => Prom: {:.2f}, Desv: {:.2f}, N: {}, Rank origen: {}",
                 nombre_municipio, (int)r.franja_horaria, r.promedio, r.desvio, r.cantidad, status.MPI_SOURCE);
        }
    }
}
