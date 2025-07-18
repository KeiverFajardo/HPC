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

    // Inicializar umbrales con valores por defecto y enviar a cada esclavo
    std::unordered_map<std::pair<uint8_t, uint8_t>, float, boost::hash<std::pair<uint8_t, uint8_t>>> umbrales;
    for (uint8_t m = 0; m < 10; ++m) {
        for (uint8_t f = 0; f < 4; ++f) {
            umbrales[{m, f}] = 15.0f; // valor por defecto
        }
    }

    for (int i = 1; i < world_size; ++i) {
        MPI_Send(umbrales.data(), umbrales.size() * sizeof(std::pair<std::pair<uint8_t, uint8_t>, float>), MPI_BYTE, i, TAG_UMBRAL, MPI_COMM_WORLD);
    }

    std::vector<ResultadoEstadistico> resultados;
    int flag = 0;

    // ---- Envío de bloques a esclavos
    while (!exit || blocks_sent > 0)
    {
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
            MPI_Send(buffer.data(), buffer.size(), MPI_Register, next_slave, 0, MPI_COMM_WORLD);
            blocks_sent++;
            next_slave++;
            if (next_slave >= world_size) next_slave = 1;
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

            // Actualizar umbrales por promedio ponderado y reenviarlos
            /* for (const auto &res : resultados) {
                Clave clave = {res.municipio_id, res.franja_horaria};
                auto &[umbral_actual, n_acum] = umbrales_map[clave];

                float nuevo = res.promedio;
                std::size_t cantidad = res.cantidad;

                // promedio ponderado
                float actualizado = (umbral_actual * n_acum + nuevo * cantidad) / (n_acum + cantidad);
                n_acum += cantidad;
                umbral_actual = actualizado;
            }
            // Reenviar nuevos umbrales actualizados
            umbrales.clear();
            for (const auto &[clave, val] : umbrales_map) {
                umbrales.push_back({clave.first, clave.second, val.first});
            }

            for (int i = 1; i < world_size; ++i) {
                MPI_Send(umbrales.data(), umbrales.size(), MPI_UmbralPorPar, i, TAG_UMBRAL, MPI_COMM_WORLD);
            } */


            for (const auto& r : resultados)
            {
                std::string nombre_municipio = mapper.decodificar(r.municipio_id);
                info("Municipio {}, Franja {} => Prom: {:.2f}, Desv: {:.2f}, N: {}, Rank origen: {}",
                    nombre_municipio, (int)r.franja_horaria, r.promedio, r.desvio, r.cantidad, status.MPI_SOURCE);
            }
            blocks_sent--;
        }
    }

    for (int i = 1; i < world_size; ++i)
    {
        MPI_Send(nullptr, 0, MPI_Register, i, EXIT_MESSAGE_TAG, MPI_COMM_WORLD);
    }
}
