#include "algoritmo_b.hpp"
#include "common_constants.hpp"

#include "csv_reader.hpp"
#include "log.hpp"
#include "umbral.hpp"
#include "mpi_datatypes.hpp"    

#include <mpi.h>
#include <vector>
#include <unordered_map>

using Clave = std::pair<uint8_t, uint8_t>;

std::pair<
    std::vector<ResultadoEstadistico>,
    std::vector<Register>
> analizar_bloque(
    const std::vector<Register> &bloque,
    const std::unordered_map<Clave, float, boost::hash<Clave>> &umbrales
) {
    std::unordered_map<Clave, std::vector<float>> grupos;
    std::vector<Register> anomalias;

    // Agrupar velocidades por (municipio, franja horaria)
    for (const auto &reg : bloque)
    {
        Clave clave = {reg.municipio_id, reg.franja_horaria};
        grupos[clave].push_back(reg.velocidad);
        auto it = umbrales.find({reg.municipio_id, reg.franja_horaria});
        assert(it != umbrales.end());
        if (reg.velocidad < it->second)
        {
            anomalias.emplace_back(reg);
        }
    }

    // Calcular estadística por grupo
    std::vector<ResultadoEstadistico> resultados;

    for (const auto &[clave, velocidades] : grupos)
    {
        float suma = 0.0f;
        for (auto &vel : velocidades) suma += vel;
        ResultadoEstadistico resultado;
        resultado.municipio_id = clave.first;
        resultado.franja_horaria = clave.second;
        resultado.suma_velocidades = suma;
        resultado.cantidad = velocidades.size();
        //resultado.es_anomalia = (it != umbrales.end()) ? (resultado.promedio < it->second) : false;  //false;
        resultados.push_back(resultado);
    }

    return std::make_pair(resultados, anomalias);
}

void imprimir_umbrales(
    int world_rank,
    const std::unordered_map<Clave, float, boost::hash<Clave>> &umbrales
) {
    info(">>> UMBRALES ACTUALIZADOS en esclavo {}", world_rank);
    for (const auto &[clave, valor] : umbrales)
    {
        info("  Municipio {} - Franja {} => Umbral = {:.2f}", clave.first, clave.second, valor);
    }
}

std::unordered_map<Clave, float, boost::hash<Clave>> recibir_umbrales()
{
    std::vector<UmbralPorPar> umbrales_buffer(8*3);
    MPI_Bcast(umbrales_buffer.data(), umbrales_buffer.size(), MPI_Umbral, MASTER_RANK, MPI_COMM_WORLD);
    std::unordered_map<Clave, float, boost::hash<Clave>> umbrales;
    for (UmbralPorPar &umbral : umbrales_buffer)
    {
        umbrales[{umbral.municipio_id, umbral.franja_horaria}] = umbral.umbral;
    }
    return std::move(umbrales);
}

void procesar_b() {
    int world_rank; //world_rank almacena el ID de este proceso dentro del mundo MPI.
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    info("IM {}", world_rank);

    std::vector<Register> buffer;

    while (true)
    {
        std::unordered_map<Clave, float, boost::hash<Clave>> umbrales = recibir_umbrales();
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
                // Recibir bloque de registros
                int count;
                MPI_Get_count(&status, MPI_Register, &count);
                buffer.resize(count);
                
                //MPI_Recv(buffer.data(), count, MPI_Register, MASTER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(buffer.data(), count, MPI_Register, MASTER_RANK, TAG_DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                // info("Esclavo {} recibió {} registros", world_rank, buffer.size());
                
                // info("BUFFER INICIO");
                // for (auto reg : buffer)
                //     info("{}", reg);
                // info("BUFFER FIN");

                /* float sum = 0.0f;
                for (Register &reg : registers)
                    sum += reg.velocidad; */

                std::pair<std::vector<ResultadoEstadistico>, std::vector<Register>> par
                    = analizar_bloque(buffer, umbrales);

                std::vector<ResultadoEstadistico> resultados = par.first;
                std::vector<Register> anomalias = par.second;


                MPI_Send(anomalias.data(), anomalias.size(), MPI_Register, MASTER_RANK, ANOMALIAS_TAG, MPI_COMM_WORLD);
                MPI_Send(resultados.data(), resultados.size(), MPI_ResultadoEstadistico, MASTER_RANK, TAG_DATA, MPI_COMM_WORLD);
            }
        }
    }
}
