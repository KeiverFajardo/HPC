#pragma once

#include "municipio_mapper.hpp"
#include <boost/container_hash/hash.hpp>
#include <unordered_map>
#include <vector>
#include <cstddef>

// Clase que encapsula la lógica del Proceso A (maestro)
class AlgoritmoA {
public:
    AlgoritmoA(const std::string &shapefile_path);

    void procesar(std::vector<std::string> files);
    
    using Clave = std::pair<uint8_t, uint8_t>;

private:
    // Cargar un bloque de tamaño `block_size` con registros extendidos
    void enviar_umbrales();
    void recalcular_umbrales();

    std::unordered_map<Clave, float, boost::hash<Clave>> m_umbrales;

    struct DatosEstadisticos {
        float suma_velocidades;
        size_t cantidad_registros;
        size_t cantidad_anomalias;
    };

    std::unordered_map<Clave, DatosEstadisticos, boost::hash<Clave>> m_datos_estadisticos;
    MunicipioMapper m_mapper;
};
