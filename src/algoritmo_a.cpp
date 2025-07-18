#include "algoritmo_a.hpp"

#include <utility>

#include "algoritmo_b.hpp"
#include "log.hpp"
#include "mpi_datatypes.hpp"
#include "common_constants.hpp"

#include <unordered_map>
#include <boost/functional/hash.hpp>

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

    using Clave = std::pair<uint8_t, uint8_t>;
    using UmbralData = std::pair<float, std::size_t>;  // <umbral, cantidad acumulada>

    std::unordered_map<Clave, UmbralData, boost::hash<Clave>> umbrales_map;

    for (uint8_t m = 0; m < 10; ++m) {
        for (uint8_t f = 0; f < 4; ++f) {
             umbrales_map[{m, f}] = {15.0f, 1}; // umbral inicial, cantidad mínima 1 para evitar división por 0
        }
    }

    // Enviar umbrales iniciales
    std::vector<UmbralPorPar> umbral_vector;
    for (const auto &[clave, val] : umbrales_map)
        umbral_vector.push_back({clave.first, clave.second, val.first});

    int umbral_count = umbral_vector.size();
    MPI_Bcast(&umbral_count, 1, MPI_INT, MASTER_RANK, MPI_COMM_WORLD);
    MPI_Bcast(umbral_vector.data(), umbral_count, MPI_UmbralPorPar, MASTER_RANK, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD); // Todos sincronizados antes de procesar

    std::vector<ResultadoEstadistico> resultados;
    int flag = 0;

    // ---- Envío de bloques a esclavos
    while (!exit || blocks_sent > 0)
    {
        if (!exit)
        {
            buffer.clear();
            if (cargar_bloque(buffer, BLOCK_SIZE))
            {
                MPI_Send(buffer.data(), buffer.size(), MPI_Register, next_slave, TAG_DATA, MPI_COMM_WORLD);
                blocks_sent++;
                next_slave++;
                if (next_slave >= world_size) next_slave = 1;
            }
            else
            {
                exit = true;
            }
        }

        MPI_Status status;
        MPI_Iprobe(MPI_ANY_SOURCE, TAG_DATA, MPI_COMM_WORLD, &flag, &status);
        
        if (flag) {
            // ---- Recepción de resultados estadísticos
            int count;
            MPI_Get_count(&status, MPI_ResultadoEstadistico, &count);
            resultados.clear();
            resultados.resize(count);
            MPI_Recv(resultados.data(), count, MPI_ResultadoEstadistico, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            //Actualizar umbrales y enviarlos a los hijos Bs y ademas pintar la grafica

            // Actualizar umbrales con promedio ponderado
            for (const auto &r : resultados)
            {
                Clave clave = {r.municipio_id, r.franja_horaria};
                auto &[umbral, n_acum] = umbrales_map[clave];
                float nuevo = r.promedio;
                std::size_t cantidad = r.cantidad;

                float actualizado = (umbral * n_acum + nuevo * cantidad) / (n_acum + cantidad);
                n_acum += cantidad;
                umbral = actualizado;
            }

            // Volver a enviar umbrales actualizados
            umbral_vector.clear();
            for (const auto &[clave, val] : umbrales_map)
                umbral_vector.push_back({clave.first, clave.second, val.first});

            int umbral_count = umbral_vector.size();
            MPI_Bcast(&umbral_count, 1, MPI_INT, MASTER_RANK, MPI_COMM_WORLD);
            MPI_Bcast(umbral_vector.data(), umbral_count, MPI_UmbralPorPar, MASTER_RANK, MPI_COMM_WORLD);
            MPI_Barrier(MPI_COMM_WORLD);

            blocks_sent--;

            //Mostrar resultados
            for (const auto& r : resultados)
            {
                std::string nombre_municipio = mapper.decodificar(r.municipio_id);
                info("Municipio {}, Franja {} => Prom: {:.2f}, Desv: {:.2f}, N: {}, Rank origen: {}",
                    nombre_municipio, (int)r.franja_horaria, r.promedio, r.desvio, r.cantidad, status.MPI_SOURCE);
            }
        }
    }

    for (int i = 1; i < world_size; ++i)
    {
        MPI_Send(nullptr, 0, MPI_Register, i, EXIT_MESSAGE_TAG, MPI_COMM_WORLD);
    }
}
