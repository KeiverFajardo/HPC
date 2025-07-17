#include <vector>
#include <mpi.h>

#include "csv_reader.hpp"
#include "log.hpp"
#include "mpi_datatypes.hpp"
#include "shapefile_loader.hpp"

constexpr int MASTER_RANK = 0;

constexpr int EXIT_MESSAGE_TAG = 1;

constexpr int BLOCK_SIZE = 100;

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    init_mpi_datatypes();
    
    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    if (world_rank == MASTER_RANK)
    {
        bool exit = false;
        float total_speed_sum = 0.0f;
        unsigned int blocks_pending = 0;
        int next_slave_rank_to_work = 0;

        info("world_size: {}", world_size);

        // ←---- Cargar shapefile
        MunicipioLoader municipio_loader("../shapefiles/sig_municipios.shp");

        CsvReader csv_reader("../only1000.csv");
        std::vector<Register> registers;
        registers.reserve(1000);
        Register r;

        int flag;
        MPI_Status status;
        while (!exit || blocks_pending > 0)
        {
            MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);
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
            }

            for (int i = 0; i < BLOCK_SIZE; i++)
            {
                if (!csv_reader.get(r))
                {
                    exit = true;
                    info("ENDED");
                    break;
                }

                // ←---- Asignar municipio basado en coordenadas
                r.municipio_id = municipio_loader.get_municipio(r.longitud, r.latitud);
                info("Municipio ID: {}, Nombre: {}", r.municipio_id, municipio_loader.decode_municipio(r.municipio_id));
                
                registers.emplace_back(r);
            }

            if (!registers.empty())
            {
                MPI_Send(
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
            }
        }

        info("AVG = {}", total_speed_sum / 1000.0f);

        for (int slave_rank = 1; slave_rank < world_size; slave_rank++)
            MPI_Send(nullptr, 0, MPI_INT, slave_rank, EXIT_MESSAGE_TAG, MPI_COMM_WORLD);

    }
    else
    {
        std::vector<Register> registers(100);
        while (true)
        {
            MPI_Status status;
            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            if (status.MPI_TAG == EXIT_MESSAGE_TAG) break;

            MPI_Recv(registers.data(), registers.size(), MPI_Register, MASTER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            float sum = 0.0f;
            for (Register &reg : registers)
                sum += reg.velocidad;

            MPI_Send(&sum, 1, MPI_FLOAT, MASTER_RANK, 0, MPI_COMM_WORLD);
        }
    }

    free_mpi_datatypes();
    MPI_Finalize();
    return 0;
}
