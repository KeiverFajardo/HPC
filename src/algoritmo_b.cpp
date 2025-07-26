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
#include <omp.h>

using Clave = std::pair<uint8_t, uint8_t>;

std::vector<ResultadoEstadistico> analizar_bloque(
    CsvReader &csv_reader,
    MunicipioMapper &mapper,
    const std::unordered_map<Clave, float, boost::hash<Clave>> &umbrales
) {
    // Calcular estadística por grupo
    /* std::vector<ResultadoEstadistico> resultados;

    std::unordered_map<uint8_t, std::array<std::array<std::tuple<float, size_t, size_t>, 3>, 8>> aux;

    Register reg;

    while (csv_reader.get(reg))
    {
        // Asignar municipio
        reg.municipio_id = mapper.codificar(Punto { reg.latitud, reg.longitud });

        // Asignar franja horaria
        reg.franja_horaria = std::to_underlying(get_franja_horaria(reg.hora));
        auto it = aux.find(reg.fecha.day);
        if (it == aux.end())
        {
            std::array<std::array<std::tuple<float, size_t, size_t>, 3>, 8> new_elem;
            for (int municipio_id = 0; municipio_id < 8; municipio_id++)
            {
                for (int franja_horaria = 0; franja_horaria < 3; franja_horaria++)
                {
                    auto &[suma, cantidad_registros, cantidad_anomalias]
                        = new_elem[municipio_id][franja_horaria];
                    suma = 0.0f;
                    cantidad_registros = 0;
                    cantidad_anomalias = 0;
                }
            }

            aux.insert({reg.fecha.day, new_elem});
            it = aux.find(reg.fecha.day);
        }

        auto &[suma, cantidad_registros, cantidad_anomalias]
            = it->second[reg.municipio_id][reg.franja_horaria];
        suma += reg.velocidad;
        cantidad_registros++;
        if (reg.velocidad < umbrales.at({reg.municipio_id, reg.franja_horaria}))
            cantidad_anomalias++;
    } */

    std::vector<Register> registros;
    Register reg;
    while (csv_reader.get(reg)) {
        reg.municipio_id = mapper.codificar(Punto{reg.latitud, reg.longitud});
        reg.franja_horaria = std::to_underlying(get_franja_horaria(reg.hora));
        registros.push_back(reg);
    }

    std::unordered_map<uint8_t, std::array<std::array<std::tuple<float, size_t, size_t>, 3>, 8>> aux;
    omp_lock_t aux_lock;
    omp_init_lock(&aux_lock);

    #pragma omp parallel for
    for (size_t i = 0; i < registros.size(); ++i) {
        const auto &reg = registros[i];
        const Clave clave = {reg.municipio_id, reg.franja_horaria};
        float velocidad = reg.velocidad;
        uint8_t dia = reg.fecha.day;

        omp_set_lock(&aux_lock);
        auto &[suma, cantidad_registros, cantidad_anomalias] = aux[dia][clave.first][clave.second];
        suma += velocidad;
        cantidad_registros++;
        if (velocidad < umbrales.at(clave)) {
            cantidad_anomalias++;
        }
        omp_unset_lock(&aux_lock);
    }

    omp_destroy_lock(&aux_lock);

    std::vector<ResultadoEstadistico> resultados;
    for (const auto &[day, aux2] : aux)
    {
        for (int municipio_id = 0; municipio_id < 8; municipio_id++)
        {
            for (int franja_horaria = 0; franja_horaria < 3; franja_horaria++)
            {
                auto &[suma, cantidad_registros, cantidad_anomalias] = aux2[municipio_id][franja_horaria];
                resultados.push_back(ResultadoEstadistico{
                    dia,
                    static_cast<uint8_t>(municipio_id),
                    static_cast<uint8_t>(franja_horaria),
                    suma,
                    cantidad_registros,
                    cantidad_anomalias
                });
            }
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

std::unordered_map<Clave, float, boost::hash<Clave>> recibir_umbrales()
{
    std::array<UmbralPorPar, 8*3> umbrales_buffer;
    MPI_Bcast(umbrales_buffer.data(), umbrales_buffer.size(), MPI_Umbral, MASTER_RANK, MPI_COMM_WORLD);
    std::unordered_map<Clave, float, boost::hash<Clave>> umbrales;
    for (UmbralPorPar &umbral : umbrales_buffer)
    {
        umbrales[{umbral.municipio_id, umbral.franja_horaria}] = umbral.umbral;
    }
    return umbrales;
}

void procesar_b(const std::string &shapefile_path, std::vector<const char*> files) {
    int world_rank; //world_rank almacena el ID de este proceso dentro del mundo MPI.
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    MunicipioMapper mapper(shapefile_path);

    for (const auto &file : files)
    {
        std::unordered_map<Clave, float, boost::hash<Clave>> umbrales = recibir_umbrales();
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
