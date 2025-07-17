#include "shapefile_loader.hpp"

#include <gdal/ogrsf_frmts.h>
#include <stdexcept>
#include <iostream>

MunicipioLoader::MunicipioLoader(const std::string &shapefile_path)
{
    GDALAllRegister();
    GDALDataset *dataset = static_cast<GDALDataset *>(GDALOpenEx(
        shapefile_path.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));

    if (!dataset)
        throw std::runtime_error("Error al abrir el shapefile: " + shapefile_path);

    OGRLayer *layer = dataset->GetLayer(0);
    if (!layer)
        throw std::runtime_error("No se pudo obtener la capa del shapefile");

    layer->ResetReading();

    OGRFeature *feature;
    uint8_t next_id = 0;

    while ((feature = layer->GetNextFeature()) != nullptr)
    {
        OGRGeometry *geometry = feature->GetGeometryRef();
        if (!geometry)
        {
            OGRFeature::DestroyFeature(feature);
            continue;
        }

        OGRGeometry *clone = geometry->clone();
        geometries.push_back(clone);

        std::string nombre = feature->GetFieldAsString("GID"); // <- campo nombre municipio si no es GID es MUNICIPIO

        // Asignar código si no existe aún
        if (name_to_id.find(nombre) == name_to_id.end())
        {
            name_to_id[nombre] = next_id;
            id_to_name[next_id] = nombre;
            next_id++;
        }

        municipio_ids.push_back(name_to_id[nombre]);

        OGRFeature::DestroyFeature(feature);
    }

    GDALClose(dataset);
}

uint8_t MunicipioLoader::get_municipio(double lon, double lat) const
{
    OGRPoint punto(lon, lat);
    for (size_t i = 0; i < geometries.size(); ++i)
    {
        if (geometries[i]->Contains(&punto))
            return municipio_ids[i];
    }

    return 255; // código para "desconocido" o "fuera de Montevideo"
}

std::string MunicipioLoader::decode_municipio(uint8_t id) const
{
    auto it = id_to_name.find(id);
    return it != id_to_name.end() ? it->second : "Desconocido";
}
