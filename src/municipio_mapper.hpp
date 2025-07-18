#pragma once

#include "shapefile_loader.hpp"
#include <string>
#include <cstdint>

class MunicipioMapper {
public:
    MunicipioMapper(const std::string& shapefile_path);

    // Devuelve el ID numérico del municipio para un punto geográfico
    uint8_t codificar(const Punto& punto) const;

    // Devuelve el nombre del municipio asociado a un ID numérico
    std::string decodificar(uint8_t id) const;

    // Devuelve el número total de municipios conocidos
    size_t cantidad() const;

private:
    ShapefileLoader loader_;
};
