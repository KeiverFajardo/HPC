#include "algoritmo_b.hpp"
#include "common_constants.hpp"
#include "estadistica.hpp"

#include "csv_reader.hpp"
#include "log.hpp"
#include "umbral.hpp"
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

void imprimir_umbrales(int world_rank, const UmbralMap &umbrales)
{
    info(">>> UMBRALES ACTUALIZADOS en esclavo {}", world_rank);
    for (const auto &[clave, valor] : umbrales)
    {
        info("  Municipio {} - Franja {} => Umbral = {:.2f}", clave.municipio_id, clave.franja_horaria, valor);
    }
}

void procesar_b() {
    int world_rank; //world_rank almacena el ID de este proceso dentro del mundo MPI.
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    std::vector<Register> buffer;
    UmbralMap umbrales_actuales;

    while (true)
    {
        //  Esperar broadcast de umbrales (conteo primero)
        int umbral_count = 0;
        MPI_Bcast(&umbral_count, 1, MPI_INT, MASTER_RANK, MPI_COMM_WORLD); //Todos los procesos B lo reciben al mismo tiempo

        if (umbral_count > 0) { //Si hay umbrales para recibir, se crea un vector recibidos para almacenarlos.
            std::vector<UmbralPorPar> recibidos(umbral_count);
            MPI_Bcast(recibidos.data(), umbral_count, MPI_Umbral, MASTER_RANK, MPI_COMM_WORLD);

            umbrales_actuales.clear();
            for (const auto &u : recibidos) {
                ClaveAgrupacion clave = {u.municipio_id, u.franja_horaria};
                umbrales_actuales[clave] = u.umbral;
            }
            imprimir_umbrales(world_rank, umbrales_actuales);
        }
        //Hace que todos los procesos esperen hasta que todos hayan terminado de recibir y actualizar sus umbrales.
        MPI_Barrier(MPI_COMM_WORLD); // sincronización


        MPI_Status status;
        /* MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status); */
        MPI_Probe(MASTER_RANK, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == EXIT_MESSAGE_TAG)
            break;

        if (status.MPI_TAG == TAG_DATA) {
            // Recibir bloque de registros
            int count;
            MPI_Get_count(&status, MPI_Register, &count);
            buffer.resize(count);
            
            //MPI_Recv(buffer.data(), count, MPI_Register, MASTER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(buffer.data(), count, MPI_Register, MASTER_RANK, TAG_DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            info("Esclavo {} recibió {} registros", world_rank, buffer.size());
            
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
