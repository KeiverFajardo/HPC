#include "algoritmo_b.hpp"
#include "common_constants.hpp"
#include "estadistica.hpp"

#include "csv_reader.hpp"
#include "log.hpp"
#include "mpi_datatypes.hpp"

#include <mpi.h>
#include <vector>
#include <unordered_map>
#include <tuple>

struct ClaveAgrupacion {
    uint8_t municipio_id;
    uint8_t franja_horaria;

    bool operator==(const ClaveAgrupacion &other) const = default;
};

template <>
struct std::hash<ClaveAgrupacion> {
    std::size_t operator()(const ClaveAgrupacion &clave) const noexcept {
        return std::hash<uint16_t>()(clave.franja_horaria | (clave.municipio_id << 8));
    }
};

using UmbralMap = std::unordered_map<ClaveAgrupacion, float>;

std::vector<ResultadoEstadistico> analizar_bloque(const std::vector<Register> &bloque, const UmbralMap &umbrales)
{
    std::unordered_map<ClaveAgrupacion, std::vector<float>> grupos;

    // Agrupar velocidades por (municipio, franja horaria)
    for (const auto &reg : bloque)
    {
        ClaveAgrupacion clave = {reg.municipio_id, reg.franja_horaria};
        grupos[clave].push_back(reg.velocidad);
    }

    // Calcular estadística por grupo
    std::vector<ResultadoEstadistico> resultados;

    for (const auto &[clave, velocidades] : grupos)
    {
        ResultadoEstadistico resultado;
        resultado.municipio_id = clave.municipio_id;
        resultado.franja_horaria = clave.franja_horaria;
        resultado.promedio = calcular_media(velocidades);
        resultado.desvio = calcular_desvio(velocidades, resultado.promedio);
        resultado.cantidad = velocidades.size();
        auto it = umbrales.find(clave);
        resultado.es_anomalia = (it != umbrales.end()) ? (resultado.promedio < it->second) : false;  //false;
        resultados.push_back(resultado);
    }

    return resultados;
}

void procesar_b() {
    std::vector<Register> buffer;
    UmbralMap umbrales_actuales;

    while (true)
    {
        MPI_Status status;
        /* MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status); */
        MPI_Probe(MASTER_RANK, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == EXIT_MESSAGE_TAG)
            break;

        if (status.MPI_TAG == TAG_UMBRAL)
        {
            int count;
            MPI_Get_count(&status, MPI_UmbralPorPar, &count);

            std::vector<UmbralPorPar> umbrales_recv(count);
            MPI_Recv(umbrales_recv.data(), count, MPI_UmbralPorPar, MASTER_RANK, TAG_UMBRAL, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            for (const auto &u : umbrales_recv)
            {
                ClaveAgrupacion clave = {u.municipio_id, u.franja_horaria};
                umbrales_actuales[clave] = u.umbral;
            }
            continue;
        }    
        
        if (status.MPI_TAG == TAG_DATA) {
            // Recibir bloque de registros
            int count;
            MPI_Get_count(&status, MPI_Register, &count);
            buffer.resize(count);
            
            //MPI_Recv(buffer.data(), count, MPI_Register, MASTER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(buffer.data(), count, MPI_Register, MASTER_RANK, TAG_DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            info("BUFFER INICIO");
            for (auto reg : buffer)
                info("{}", reg);
            info("BUFFER FIN");

            /* float sum = 0.0f;
            for (Register &reg : registers)
                sum += reg.velocidad; */

            std::vector<ResultadoEstadistico> resultados = analizar_bloque(buffer, umbrales_actuales);
            //MPI_Send(resultados.data(), resultados.size(), MPI_ResultadoEstadistico, MASTER_RANK, 0, MPI_COMM_WORLD);
            MPI_Send(resultados.data(), resultados.size(), MPI_ResultadoEstadistico, MASTER_RANK, TAG_DATA, MPI_COMM_WORLD);
        }
        
    }
}
