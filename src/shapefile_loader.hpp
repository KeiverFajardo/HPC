#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <ogrsf_frmts.h>

struct Punto {
    float lat;
    float lon;
};

class ShapefileLoader {
public:
    ShapefileLoader(const std::string& shapefile_path);

    // Devuelve el ID de municipio asociado al punto (o 255 si no pertenece a ninguno)
    uint8_t get_municipio(const Punto& punto) const;

    // Devuelve el nombre textual del municipio, dado el ID
    std::string nombre_municipio(uint8_t id) const;

private:
    struct Municipio {
        std::string nombre;
        void* geometria;  // OGRGeometry*
    };

    std::vector<Municipio> municipios_;
    std::unordered_map<std::string, uint8_t> nombre_to_id_;
};
