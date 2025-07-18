#include "algoritmo_b.hpp"
#include "estadistica.hpp"

#include <unordered_map>
#include <tuple>

using ClaveAgrupacion = std::pair<uint8_t, uint8_t>;

std::vector<ResultadoEstadistico> analizar_bloque(const std::vector<RegisterExt> &bloque)
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
        resultado.municipio_id = clave.first;
        resultado.franja_horaria = clave.second;
        resultado.promedio = calcular_media(velocidades);
        resultado.desvio = calcular_desvio(velocidades, resultado.promedio);
        resultado.cantidad = velocidades.size();
        resultados.push_back(resultado);
    }

    return resultados;
}
