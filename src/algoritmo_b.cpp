#include "algoritmo_b.hpp"
#include "common_constants.hpp"

#include "csv_reader.hpp"
#include "franja_horaria.hpp"
#include "log.hpp"
#include "municipio_mapper.hpp"
#include "umbral.hpp"
#include "mpi_datatypes.hpp"    

#include <mpi.h>
#include <vector>
#include <unordered_map>

using Clave = std::pair<uint8_t, uint8_t>;

std::vector<ResultadoEstadistico> analizar_bloque(
    CsvReader &csv_reader,
    MunicipioMapper &mapper,
    const std::array<float, MAX_UMBRAL_ID> &umbrales
) {
    // Calcular estadística por grupo
    std::vector<ResultadoEstadistico> resultados;

    struct Accumulador {
        float suma_velocidades = 0.0f;
        size_t cantidad_registros = 0;
        size_t cantidad_anomalias = 0;
    };

    std::unordered_map<uint8_t, std::array<Accumulador, MAX_UMBRAL_ID>> accumuladores;

    Register reg;

    while (csv_reader.get(reg))
    {
        if (reg.velocidad < 0.1f) continue;

        // Asignar municipio
        reg.municipio_id = mapper.codificar(Punto { reg.latitud, reg.longitud });
        // Asignar franja horaria
        reg.franja_horaria = std::to_underlying(get_franja_horaria(reg.hora));

        // int rank;
        // MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        // if (rank == 1) info("REG {}", reg);

        uint8_t dia_semana = day_of_week(reg.fecha.day, reg.fecha.month, reg.fecha.year);
        uint8_t umbral_id = get_umbral_id(reg.municipio_id, reg.franja_horaria, dia_semana);
        auto &[suma, cantidad_registros, cantidad_anomalias]
            = accumuladores[reg.fecha.day][umbral_id];
        suma += reg.velocidad;
        cantidad_registros++;
        if (reg.velocidad < umbrales.at(umbral_id))
            cantidad_anomalias++;
    }

    for (const auto &[day, aux2] : accumuladores)
    {
        for (int umbral_id = 0; umbral_id < MAX_UMBRAL_ID; umbral_id++)
        {
            auto &[suma, cantidad_registros, cantidad_anomalias] = accumuladores[day][umbral_id];
            if (cantidad_registros <= 0) continue;
            ResultadoEstadistico res;
            res.umbral_id = umbral_id;
            res.dia = day;
            res.suma_velocidades = suma;
            res.cantidad_registros = cantidad_registros;
            res.cantidad_anomalias = cantidad_anomalias;
            resultados.push_back(res);
        }
    }

    return resultados;
}

void imprimir_umbrales(const std::unordered_map<Clave, float, boost::hash<Clave>> &umbrales)
{
    for (const auto &[clave, valor] : umbrales)
    {
        info("  Municipio {} - Franja {} => Umbral = {:.2f}", clave.first, clave.second, valor);
    }
}

std::array<float, MAX_UMBRAL_ID> recibir_umbrales()
{
    std::array<UmbralPorPar, MAX_UMBRAL_ID> umbrales_buffer;
    MPI_Bcast(umbrales_buffer.data(), umbrales_buffer.size(), MPI_Umbral, MASTER_RANK, MPI_COMM_WORLD);
    std::array<float, MAX_UMBRAL_ID> umbrales;
    for (UmbralPorPar &umbral : umbrales_buffer)
    {
        umbrales[umbral.id] = umbral.umbral;
    }
    return umbrales;
}

void procesar_b(const std::string &shapefile_path, std::vector<const char*> files) {
    int world_rank; //world_rank almacena el ID de este proceso dentro del mundo MPI.
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    MunicipioMapper mapper(shapefile_path);

    for (const auto &file : files)
    {
        std::array<float, MAX_UMBRAL_ID> umbrales = recibir_umbrales();
        //imprimir_umbrales(umbrales);
        
        while (true)
        {
            MPI_Status status;
            MPI_Probe(MASTER_RANK, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            
            if (status.MPI_TAG == EXIT_MESSAGE_TAG)
            {
                MPI_Recv(nullptr, 0, MPI_Register, MASTER_RANK, EXIT_MESSAGE_TAG, MPI_COMM_WORLD, &status);
                return;
            }

            if (status.MPI_TAG == END_OF_FILE_TAG)
            {
                MPI_Recv(nullptr, 0, MPI_Register, MASTER_RANK, END_OF_FILE_TAG, MPI_COMM_WORLD, &status);
                break;
            }

            if (status.MPI_TAG == TAG_DATA) {
                size_t range[2];
                MPI_Recv(range, 2, MPI_UINT64_T, MASTER_RANK, TAG_DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                CsvReader csv_reader(file, range[0], range[1]);
                
                // info("Esclavo {} recibió {} registros", world_rank, buffer.size());

                std::vector<ResultadoEstadistico> resultados
                    = analizar_bloque(csv_reader, mapper, umbrales);

                MPI_Send(resultados.data(), resultados.size(), MPI_ResultadoEstadistico, MASTER_RANK, TAG_DATA, MPI_COMM_WORLD);
            }
        }
    }
}
