#pragma once

#include "csv_reader.hpp"
#include "franja_horaria.hpp"
#include "shapefile_loader.hpp"
#include "municipio_mapper.hpp"

#include <vector>
#include <memory>
#include <cstddef>

// Clase que encapsula la lógica del Proceso A (maestro)
class AlgoritmoA {
public:
    AlgoritmoA(const std::string &csv_path, const std::string &shapefile_path);

    // Cargar un bloque de tamaño `block_size` con registros extendidos
    bool cargar_bloque(std::vector<Register> &bloque, std::size_t block_size);

    // Retornar cantidad total de registros procesados (opcional para estadísticas)
    std::size_t total_procesados() const { return registros_procesados; }

private:
    CsvReader lector_csv;
    ShapefileLoader loader;
    std::size_t registros_procesados = 0;
};
