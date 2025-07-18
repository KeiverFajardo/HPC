#include "algoritmo_b.hpp"
#include "estadistica.hpp"

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
        resultados.push_back(resultado);
    }

    return resultados;
}
