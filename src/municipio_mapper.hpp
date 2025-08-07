#pragma once

#include "shapefile_loader.hpp"
#include <boost/container_hash/hash.hpp>
#include <string>
#include <cstdint>

inline std::size_t hash_value(const Punto& p) {
    std::size_t seed = 0;
    boost::hash_combine(seed, p.lat);
    boost::hash_combine(seed, p.lon);
    return seed;
}

class MunicipioMapper {
public:
    MunicipioMapper(const std::string& shapefile_path);

    // Devuelve el ID numérico del municipio para un punto geográfico
    uint8_t codificar(Punto punto) const;

    // Devuelve el nombre del municipio asociado a un ID numérico
    std::string decodificar(uint8_t id) const;

    // Devuelve el número total de municipios conocidos
    size_t cantidad() const;

private:
    ShapefileLoader loader_;
};
