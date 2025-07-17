#pragma once

#include <string>
#include <vector>
#include <map>
#include <gdal/ogrsf_frmts.h>

class MunicipioLoader
{
public:
    // Constructor: carga los datos del shapefile
    explicit MunicipioLoader(const std::string &shapefile_path);

    // Devuelve el código de municipio (uint8_t) para una coordenada (lon, lat)
    uint8_t get_municipio(double lon, double lat) const;

    // Devuelve el nombre del municipio a partir del ID
    std::string decode_municipio(uint8_t id) const;

private:
    std::vector<OGRGeometry *> geometries;     // Geometrías de los municipios
    std::vector<uint8_t> municipio_ids;        // ID codificado para cada geometría

    std::map<std::string, uint8_t> name_to_id; // Mapeo nombre → ID
    std::map<uint8_t, std::string> id_to_name; // Mapeo ID → nombre
};
