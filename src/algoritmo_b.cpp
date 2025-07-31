#include "algoritmo_b.hpp"
#include "common_constants.hpp"

#include "csv_reader.hpp"
#include "franja_horaria.hpp"
#include "municipio_mapper.hpp"
#include "umbral.hpp"
#include "mpi_datatypes.hpp"    

#include <mpi.h>
#include <omp.h>
#include <vector>
#include <unordered_map>

std::vector<ResultadoEstadistico> analizar_bloque_parallel(
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

    int num_threads = omp_get_max_threads();
    // info("MAX {}", num_threads);
    std::vector<std::unordered_map<uint8_t, std::array<Accumulador, MAX_UMBRAL_ID>>> accumuladores_threads(num_threads);

    std::vector<Register> registers;
    registers.reserve(200000);

    Register reg;
    while (csv_reader.get(reg))
    {
        registers.push_back(reg);
    }

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        auto &accumuladores = accumuladores_threads[tid];
        // info("TID {}", tid);

        #pragma omp for
        for (size_t i = 0; i < registers.size(); i++)
        {
            Register &reg = registers[i];
            // Asignar municipio
            reg.municipio_id = mapper.codificar(Punto { reg.latitud, reg.longitud });
            // Asignar franja horaria
            reg.franja_horaria = std::to_underlying(get_franja_horaria(reg.hora));
            uint8_t dia_semana = day_of_week(reg.fecha.day, reg.fecha.month, reg.fecha.year);
            uint8_t umbral_id = get_umbral_id(reg.municipio_id, reg.franja_horaria, dia_semana);
            auto &[suma, cantidad_registros, cantidad_anomalias]
                = accumuladores[reg.fecha.day][umbral_id];
            suma += reg.velocidad;
            cantidad_registros++;
            if (reg.velocidad < umbrales.at(umbral_id))
                cantidad_anomalias++;
        }
    }

    for (const auto &accumuladores : accumuladores_threads)
    {
        for (const auto &[day, aux2] : accumuladores)
        {
            for (int umbral_id = 0; umbral_id < MAX_UMBRAL_ID; umbral_id++)
            {
                auto &[suma, cantidad_registros, cantidad_anomalias] = aux2[umbral_id];
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
    }

    return resultados;
}

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

void procesar_b(const std::string &shapefile_path, std::vector<const char*> files) {
    int world_rank; //world_rank almacena el ID de este proceso dentro del mundo MPI.
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    MunicipioMapper mapper(shapefile_path);
    const char *file = nullptr;

    std::array<float, MAX_UMBRAL_ID> umbrales;
    //imprimir_umbrales(umbrales);
    
    for (;;)
    {
        MPI_Status status;
        MPI_Probe(MASTER_RANK, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == BLOCKS_TAG) {
            size_t range[2];
            MPI_Recv(range, 2, MPI_UINT64_T, MASTER_RANK, BLOCKS_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            CsvReader csv_reader(file, range[0], range[1]);
            
            // info("Esclavo {} recibió {} registros", world_rank, buffer.size());

            std::vector<ResultadoEstadistico> resultados
                = analizar_bloque(csv_reader, mapper, umbrales);
                // = analizar_bloque_parallel(csv_reader, mapper, umbrales);

            MPI_Send(resultados.data(), resultados.size(), MPI_ResultadoEstadistico, MASTER_RANK, BLOCKS_TAG, MPI_COMM_WORLD);
        }

        if (status.MPI_TAG == UMBRALES_TAG)
        {
            MPI_Recv(umbrales.data(), umbrales.size(), MPI_FLOAT, MASTER_RANK, UMBRALES_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        if (status.MPI_TAG == CHANGE_FILE_TAG)
        {
            int file_index = -1;
            MPI_Recv(&file_index, 1, MPI_INT, MASTER_RANK, CHANGE_FILE_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            file = files[file_index];
        }

        if (status.MPI_TAG == EXIT_MESSAGE_TAG)
        {
            MPI_Recv(nullptr, 0, MPI_Register, MASTER_RANK, EXIT_MESSAGE_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            return;
        }
    }
}
