#pragma once

#include "csv_reader.hpp"
#include "municipio_mapper.hpp"

#include <boost/container_hash/hash.hpp>
#include <vector>
#include <cstddef>

// Clase que encapsula la lógica del Proceso A (maestro)
class AlgoritmoA {
public:
    AlgoritmoA(const std::string &shapefile_path);

    void procesar();
    
    using Clave = std::pair<uint8_t, uint8_t>;

private:
    // Cargar un bloque de tamaño `block_size` con registros extendidos
    bool cargar_bloque(CsvReader &csv_reader, std::vector<Register> &bloque, std::size_t block_size);
    void enviar_umbrales();
    void recalcular_umbrales();

    std::unordered_map<Clave, float, boost::hash<Clave>> m_umbrales;
    std::unordered_map<Clave, std::pair<float, size_t>, boost::hash<Clave>> m_suma_velocidades;
    MunicipioMapper m_mapper;
};
