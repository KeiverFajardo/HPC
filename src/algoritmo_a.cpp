#include "algoritmo_a.hpp"

#include <mpi.h>
#include <set>
#include <utility>

#include "algoritmo_b.hpp"
#include "log.hpp"
#include "mpi_datatypes.hpp"
#include "common_constants.hpp"
#include "umbral.hpp"

#include "gnuplot-iostream.h"

#include <unordered_map>
#include <boost/functional/hash.hpp>

constexpr int BLOCK_SIZE = 1000000;

AlgoritmoA::AlgoritmoA(const std::string &shapefile_path)
    : m_mapper(shapefile_path)
{
    info("MUNICIPIOS {}", m_mapper.cantidad());
    for (uint8_t municipio = 0; municipio < 8; ++municipio) {
        for (uint8_t franja_horaria = 0; franja_horaria < 3; ++franja_horaria) {
             m_umbrales[{municipio, franja_horaria}] = 15.0f;
             m_datos_estadisticos[{municipio, franja_horaria}] = { 0.0f, 0, 0 };
        }
    }
}

void AlgoritmoA::enviar_umbrales()
{
    std::vector<UmbralPorPar> umbrales_buffer;
    for (auto &[clave, umbral] : m_umbrales)
        umbrales_buffer.emplace_back(UmbralPorPar {
            clave.first,
            clave.second,
            umbral,
        });

    info("UMBRASLES_BUFFER {}", umbrales_buffer.size());
    MPI_Bcast(umbrales_buffer.data(), umbrales_buffer.size(), MPI_Umbral, MASTER_RANK, MPI_COMM_WORLD);
}

void AlgoritmoA::recalcular_umbrales()
{
    for (auto &[clave, datos] : m_datos_estadisticos)
    {
        if (datos.cantidad_registros != 0)
            m_umbrales[clave] = (datos.suma_velocidades / datos.cantidad_registros) * 0.5;
        datos.suma_velocidades = 0.0f;
        datos.cantidad_registros = 0;
    }
}

void AlgoritmoA::procesar(std::vector<std::string> files)
{
    Gnuplot gp;
    gp << "set terminal gif size 640,480 animate delay 16.6\n";
    gp << "set output 'image.gif'\n";
    gp << "set style fill solid\n";
    gp << "set boxwidth 0.5\n";
    gp << "set yrange [0:1000]\n";
    gp << "set xlabel 'Municipios'\n";
    gp << "set ylabel 'Velocidad media'\n";

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    std::set<int> free_slaves;
    for (int i = 1; i < world_size; i++)
        free_slaves.emplace(i);

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

        enviar_umbrales();
        // ---- Envío de bloques a esclavos
        bool file_ended = false;
        while (!file_ended || free_slaves.size() < static_cast<size_t>(world_size - 1))
        {
            if (!file_ended && !free_slaves.empty())
            {
                if (cursor < filesize)
                {
                    int slave = free_slaves.extract(free_slaves.begin()).value();
                    uint64_t range[2];
                    range[0] = cursor;
                    cursor += BLOCK_SIZE;
                    range[1] = cursor;
                    info("BLOQUE {} ({}:{}) for {}", ++bloque, range[0], range[1], slave);
                    MPI_Send(range, 2, MPI_UINT64_T, slave, TAG_DATA, MPI_COMM_WORLD);
                }
                else
                {
                    file_ended = true;
                }
            }

            int flag = 0;
            MPI_Status status;
            MPI_Iprobe(MPI_ANY_SOURCE, TAG_DATA, MPI_COMM_WORLD, &flag, &status);
            while (flag) { 
                // ---- Recepción de resultados estadísticos
                int count;
                MPI_Get_count(&status, MPI_ResultadoEstadistico, &count);
                resultados.clear();
                resultados.resize(count);
                MPI_Recv(resultados.data(), count, MPI_ResultadoEstadistico, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                //Actualizar umbrales y enviarlos a los hijos Bs y ademas pintar la grafica

                // Actualizar umbrales con promedio ponderado
                for (const auto &r : resultados)
                {
                    Clave clave = {r.municipio_id, r.franja_horaria};
                    DatosEstadisticos &datos = m_datos_estadisticos[clave];

                    datos.suma_velocidades += r.suma_velocidades;
                    datos.cantidad_registros += r.cantidad_registros;
                    datos.cantidad_anomalias += r.cantidad_anomalias;
                }

                free_slaves.emplace(status.MPI_SOURCE);

                //Mostrar resultados
                // for (const auto& r : resultados)
                // {
                //     std::string nombre_municipio = m_mapper.decodificar(r.municipio_id);
                //     info("Municipio {}, Franja {} => Suma: {:.2f}, N: {}, Rank origen: {}",
                //         nombre_municipio, (int)r.franja_horaria, r.suma_velocidades, r.cantidad, status.MPI_SOURCE);
                // }

                MPI_Iprobe(MPI_ANY_SOURCE, TAG_DATA, MPI_COMM_WORLD, &flag, &status);
            }
        }

        if (static_cast<size_t>(i) != files.size() - 1)
        {
            for (int i = 1; i < world_size; ++i)
            {
                MPI_Send(nullptr, 0, MPI_Register, i, END_OF_FILE_TAG, MPI_COMM_WORLD);
            }
        }
        else
        {
            info("ACAAAAA");
        }

        {
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
        }

        recalcular_umbrales();
    }

    for (int i = 1; i < world_size; ++i)
    {
        MPI_Send(nullptr, 0, MPI_Register, i, EXIT_MESSAGE_TAG, MPI_COMM_WORLD);
    }
}
