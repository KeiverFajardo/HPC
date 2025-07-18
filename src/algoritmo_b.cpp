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

std::vector<ResultadoEstadistico> analizar_bloque(const std::vector<Register> &bloque)
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
        resultado.es_anomalia = false;
        resultados.push_back(resultado);
    }

    return resultados;
}

void procesar_b() {
    std::vector<Register> buffer;

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
