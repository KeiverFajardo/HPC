#include <mpi.h>

#include "algoritmo_a.hpp"
#include "algoritmo_b.hpp"
#include "mpi_datatypes.hpp"


#include <vector>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <set>

#include "csv_reader.hpp"
#include "franja_horaria.hpp"
#include "municipio_mapper.hpp"
#include "common_constants.hpp"
#include "umbral.hpp"
#include "log.hpp"

#include "gnuplot-iostream.h"

constexpr int BLOCK_SIZE = 10000000;

std::unordered_map<Clave, float, boost::hash<Clave>> m_umbrales;

struct DatosEstadisticos {
    float suma_velocidades;
    size_t cantidad_registros;
    size_t cantidad_anomalias;
};

std::unordered_map<Clave, DatosEstadisticos, boost::hash<Clave>> m_datos_estadisticos;

std::unordered_map<Clave, float, boost::hash<Clave>> umbrales;
std::unordered_map<Clave, std::tuple<float, size_t, size_t>, boost::hash<Clave>> estadisticas;


void plot(const std::vector<float> &barras)
{
    Gnuplot gp;
    
    info("BARRAS BEGIN");
    for (auto barra : barras)
        info("{}", barra);
    info("BARRAS END");

	// Don't forget to put "\n" at the end of each line!
    gp << "set terminal gif size 300,200 animate delay 2\n";
    gp << "set output 'image.gif'\n";
    gp << "set style fill solid\n";
    gp << "set boxwidth 0.5\n";
	gp << "plot '-' with boxes title 'pts_A'\n";
	gp.send1d(barras);
    const_cast<std::vector<float>&>(barras)[1] = 3.0f;
	gp << "plot '-' with boxes title 'pts_A'\n";
	gp.send1d(barras);
}

std::vector<ResultadoEstadistico> analizar_bloque(
    const std::vector<Register> &bloque,
    const std::unordered_map<Clave, float, boost::hash<Clave>> &umbrales
) {
    std::vector<ResultadoEstadistico> resultados;
    std::array<std::array<std::tuple<float, size_t, size_t>, 3>, 8> aux;

    for (int m = 0; m < 8; m++)
        for (int f = 0; f < 3; f++)
            aux[m][f] = {0.0f, 0, 0};

    for (const auto &reg : bloque) {
        auto &[suma, cant, anomalias] = aux[reg.municipio_id][reg.franja_horaria];
        suma += reg.velocidad;
        cant++;
        if (reg.velocidad < umbrales.at({reg.municipio_id, reg.franja_horaria}))
            anomalias++;
    }

    for (int m = 0; m < 8; m++) {
        for (int f = 0; f < 3; f++) {
            auto &[suma, cant, anomalias] = aux[m][f];
            resultados.push_back({static_cast<uint8_t>(m), static_cast<uint8_t>(f), suma, cant, anomalias});
        }
    }

    return resultados;
}

bool cargar_bloque(CsvReader &csv_reader, MunicipioMapper &mapper, std::vector<Register> &bloque)
{
    bloque.clear();
    Register reg;

    while (csv_reader.get(reg))
    {
        // Asignar municipio
        reg.municipio_id = mapper.codificar(Punto { reg.latitud, reg.longitud });

        // Asignar franja horaria
        reg.franja_horaria = std::to_underlying(get_franja_horaria(reg.hora));

        bloque.push_back(reg);
    }

    return !bloque.empty();
}

void imprimir_umbrales(const std::unordered_map<Clave, float, boost::hash<Clave>> &umbrales)
{
    for (const auto &[clave, valor] : umbrales)
    {
        info("  Municipio {} - Franja {} => Umbral = {:.2f}", clave.first, clave.second, valor);
    }
}

void recalcular_umbrales()
{
    for (auto &[clave, datos] : m_datos_estadisticos)
    {
        if (datos.cantidad_registros != 0)
            m_umbrales[clave] = (datos.suma_velocidades / datos.cantidad_registros) * 0.5;
        datos.suma_velocidades = 0.0f;
        datos.cantidad_registros = 0;
    }
}

void procesar_main(const std::string &shapefile_path, std::vector<std::string> files)
{
     MunicipioMapper mapper(shapefile_path);

    
    info("MUNICIPIOS {}", m_mapper.cantidad());
    for (uint8_t municipio = 0; municipio < 8; ++municipio) {
        for (uint8_t franja_horaria = 0; franja_horaria < 3; ++franja_horaria) {
            m_umbrales[{municipio, franja_horaria}] = 15.0f;
            m_datos_estadisticos[{municipio, franja_horaria}] = { 0.0f, 0, 0 };
        }
    }

    Gnuplot gp;
    gp << "set terminal gif size 640,480 animate delay 16.6\n";
    gp << "set output 'image.gif'\n";
    gp << "set style fill solid\n";
    gp << "set boxwidth 0.5\n";
    gp << "set xlabel 'Municipios'\n";
    gp << "set ylabel 'Velocidad media'\n";

    std::vector<ResultadoEstadistico> resultados;

    for (size_t i = 0; i < files.size(); i++)
    {
        int bloque = 0;
        auto &filename = files[i];
        filename.reserve(128);

        std::ifstream ifs(filename.c_str());
        ifs.seekg(0, std::ios::end);
        size_t filesize = ifs.tellg();
        ifs.close();
        size_t cursor = 0;
        info("Comienzo archivo {} ({} bytes)", filename, filesize);

        //enviar_umbrales();---------------------
        std::vector<UmbralPorPar> umbrales_buffer;
        for (auto &[clave, umbral] : m_umbrales)
            umbrales_buffer.emplace_back(UmbralPorPar {
                clave.first,
                clave.second,
                umbral,
            });

        info("UMBRASLES_BUFFER {}", umbrales_buffer.size());
        //---------------------------------
        
        bool file_ended = false;
        while (!file_ended)
        {
            if (cursor < filesize)
            {
                uint64_t range[2];
                range[0] = cursor;
                cursor += BLOCK_SIZE;
                range[1] = cursor;
                info("BLOQUE {} ({}:{})", ++bloque, range[0], range[1]);

                //Proceso B
                CsvReader csv_reader(file.c_str(), range[0], range[1]);
                
                cargar_bloque(csv_reader, mapper, buffer);
                std::vector<ResultadoEstadistico> resultados = analizar_bloque(buffer, umbrales);

                //A de vuelta
                for (const auto &r : resultados)
                {
                    Clave clave = {r.municipio_id, r.franja_horaria};
                    DatosEstadisticos &datos = m_datos_estadisticos[clave];

                    datos.suma_velocidades += r.suma_velocidades;
                    datos.cantidad_registros += r.cantidad_registros;
                    datos.cantidad_anomalias += r.cantidad_anomalias;
                }

                std::vector<std::pair<std::string, int>> barras(8);
                for (int i = 0; i < 8; i++)
                {
                    barras[i] = std::make_pair('"' + m_mapper.decodificar(i) + '"', 0);
                }

                for (auto &[clave, datos] : m_datos_estadisticos)
                {
                    barras.at(clave.first).second = datos.cantidad_anomalias;
                }
                gp << "plot '-' using 2:xtic(1) with boxes notitle\n";
                gp.send1d(barras);

                recalcular_umbrales();
            }
            else
            {
                file_ended = true;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    std::vector<std::string> files = {
        "../autoscope_04_2025_velocidad.csv",
        "../autoscope_05_2025_velocidad.csv",
    };

    procesar_main("../shapefiles/procesado.shp", std::move(files));
    return 0;
}
