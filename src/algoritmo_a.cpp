#include "algoritmo_a.hpp"

#include <mpi.h>
#include <set>
#include <utility>

#include "algoritmo_b.hpp"
#include "franja_horaria.hpp"
#include "log.hpp"
#include "mpi_datatypes.hpp"
#include "common_constants.hpp"
#include "umbral.hpp"

#include "gnuplot-iostream.h"

#include <unordered_map>
#include <boost/functional/hash.hpp>

constexpr int BLOCK_SIZE = 1000;

AlgoritmoA::AlgoritmoA(const std::string &shapefile_path)
    : m_mapper(shapefile_path)
{
    info("MUNICIPIOS {}", m_mapper.cantidad());
    for (uint8_t municipio = 0; municipio < 8; ++municipio) {
        for (uint8_t franja_horaria = 0; franja_horaria < 3; ++franja_horaria) {
             m_umbrales[{municipio, franja_horaria}] = 15.0f;
             m_suma_velocidades[{municipio, franja_horaria}] = std::make_pair(0.0f, 0);
        }
    }
}

bool AlgoritmoA::cargar_bloque(CsvReader &csv_reader, std::vector<Register> &bloque, std::size_t block_size)
{
    bloque.clear();
    Register reg;

    while (bloque.size() < block_size && csv_reader.get(reg))
    {
        // Asignar municipio
        reg.municipio_id = m_mapper.codificar(Punto { reg.latitud, reg.longitud });

        // Asignar franja horaria
        reg.franja_horaria = std::to_underlying(get_franja_horaria(reg.hora));

        bloque.push_back(reg);
    }

    return !bloque.empty();
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
    for (auto &[clave, suma_y_cantidad] : m_suma_velocidades)
    {
        m_umbrales[clave] = suma_y_cantidad.first / suma_y_cantidad.second * 0.5;
        suma_y_cantidad.first = 0.0f;
        suma_y_cantidad.second = 0;
    }
}

void AlgoritmoA::procesar()
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

    std::vector<Register> buffer;
    buffer.reserve(BLOCK_SIZE);

    std::vector<Register> anomalias;

    std::vector<ResultadoEstadistico> resultados;
    int flag = 0;

    std::vector<std::string> files = {
        "../04_2025.csv",
        "../05_2025.csv",
    };

    for (int i = 0; i < files.size(); i++)
    {
        int bloque = 0;
        const auto &filename = files[i];

        info("Comienzo archivo {}", filename);
        CsvReader csv_reader(filename.c_str());
        enviar_umbrales();
        // ---- Envío de bloques a esclavos
        bool file_ended = false;
        while (!file_ended || free_slaves.size() < world_size - 1)
        {
            if (!file_ended && !free_slaves.empty())
            {
                if (cargar_bloque(csv_reader, buffer, BLOCK_SIZE))
                {
                    int slave = free_slaves.extract(free_slaves.begin()).value();
                    info("BLOQUE {} for {}", ++bloque, slave);
                    MPI_Send(buffer.data(), buffer.size(), MPI_Register, slave, TAG_DATA, MPI_COMM_WORLD);
                }
                else
                {
                    file_ended = true;
                }
            }

            MPI_Status status;
            MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);

            if (!flag) continue;
            
            if (status.MPI_TAG == TAG_DATA) {
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
                    auto &[suma, cantidad] = m_suma_velocidades[clave];

                    suma += r.suma_velocidades;
                    cantidad += r.cantidad;
                }

                free_slaves.emplace(status.MPI_SOURCE);

                //Mostrar resultados
                // for (const auto& r : resultados)
                // {
                //     std::string nombre_municipio = m_mapper.decodificar(r.municipio_id);
                //     info("Municipio {}, Franja {} => Suma: {:.2f}, N: {}, Rank origen: {}",
                //         nombre_municipio, (int)r.franja_horaria, r.suma_velocidades, r.cantidad, status.MPI_SOURCE);
                // }
            }

            if (status.MPI_TAG == ANOMALIAS_TAG)
            {
                int count;
                MPI_Get_count(&status, MPI_Register, &count);
                std::vector<Register> anomalias_buf(count);
                MPI_Recv(anomalias_buf.data(), anomalias_buf.size(), MPI_Register, status.MPI_SOURCE, ANOMALIAS_TAG, MPI_COMM_WORLD, &status);
                for (Register &reg : anomalias_buf)
                    anomalias.emplace_back(reg);
            }
        }

        if (i != files.size() - 1)
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
            std::unordered_map<uint8_t, int> cantidad_anomalias_por_municipio;
            for (Register &reg : anomalias)
            {
                cantidad_anomalias_por_municipio[reg.municipio_id] += 1;
            }

            std::vector<std::pair<std::string, int>> barras(8);
            for (int i = 0; i < 8; i++)
            {
                barras[i] = std::make_pair('"' + m_mapper.decodificar(i) + '"', 0);
            }

            for (auto &[municipio_id, cantidad] : cantidad_anomalias_por_municipio)
            {
                barras.at(municipio_id).second = cantidad;
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
