#include "mpi_datatypes.hpp"

#include "algoritmo_b.hpp"
#include "csv_reader.hpp"

MPI_Datatype MPI_Date;
MPI_Datatype MPI_Hour;
MPI_Datatype MPI_Register;
MPI_Datatype MPI_ResultadoEstadistico;

void init_mpi_date()
{
    int blocklengths[3] = {1, 1, 1};
    MPI_Aint displacements[3];
    MPI_Datatype types[3] = {MPI_UINT16_T, MPI_UINT8_T, MPI_UINT8_T};

    displacements[0] = offsetof(Date, year);
    displacements[1] = offsetof(Date, month);
    displacements[2] = offsetof(Date, day);

    MPI_Type_create_struct(3, blocklengths, displacements, types, &MPI_Date);
    MPI_Type_commit(&MPI_Date);
}

void init_mpi_hour()
{
    int blocklengths[3] = {1, 1, 1};
    MPI_Aint displacements[3];
    MPI_Datatype types[3] = {MPI_UINT8_T, MPI_UINT8_T, MPI_UINT8_T};

    displacements[0] = offsetof(Hour, hour);
    displacements[1] = offsetof(Hour, minute);
    displacements[2] = offsetof(Hour, second);

    MPI_Type_create_struct(3, blocklengths, displacements, types, &MPI_Hour);
    MPI_Type_commit(&MPI_Hour);
}

void init_mpi_register()
{
    int blocklengths[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
    MPI_Aint displacements[9];
    MPI_Datatype types[9] = {
        MPI_UNSIGNED,       // cod_detector
        MPI_UNSIGNED,       // id_carril
        MPI_Date,           // fecha
        MPI_Hour,           // hora
        MPI_FLOAT,          // latitud
        MPI_FLOAT,          // longitud
        MPI_FLOAT,          // velocidad
        MPI_UINT8_T,        // municipio_id
        MPI_UINT8_T         // franja_horaria
    };

    displacements[0] = offsetof(Register, cod_detector);
    displacements[1] = offsetof(Register, id_carril);
    displacements[2] = offsetof(Register, fecha);
    displacements[3] = offsetof(Register, hora);
    displacements[4] = offsetof(Register, latitud);
    displacements[5] = offsetof(Register, longitud);
    displacements[6] = offsetof(Register, velocidad);
    displacements[7] = offsetof(Register, municipio_id);
    displacements[8] = offsetof(Register, franja_horaria);

    MPI_Type_create_struct(9, blocklengths, displacements, types, &MPI_Register);
    MPI_Type_commit(&MPI_Register);
}

void init_mpi_resultado_estadistico()
{
    int blocklengths[5] = {1, 1, 1, 1, 1};
    MPI_Aint displacements[5];
    MPI_Datatype types[5] = {
        MPI_UINT8_T,  // uint8_t municipio_id;
        MPI_UINT8_T,  // uint8_t franja_horaria;
        MPI_FLOAT,    // float promedio;
        MPI_FLOAT,    // float desvio;
        MPI_UINT64_T, // std::size_t cantidad;
    };

    displacements[0] = offsetof(ResultadoEstadistico, municipio_id);
    displacements[1] = offsetof(ResultadoEstadistico, franja_horaria);
    displacements[2] = offsetof(ResultadoEstadistico, promedio);
    displacements[3] = offsetof(ResultadoEstadistico, desvio);
    displacements[4] = offsetof(ResultadoEstadistico, cantidad);

    MPI_Type_create_struct(5, blocklengths, displacements, types, &MPI_ResultadoEstadistico);
    MPI_Type_commit(&MPI_ResultadoEstadistico);
}

void init_mpi_datatypes()
{
    init_mpi_date();
    init_mpi_hour();
    init_mpi_register();
    init_mpi_resultado_estadistico();
}

void free_mpi_datatypes()
{
    MPI_Type_free(&MPI_Date);
    MPI_Type_free(&MPI_Hour);
    MPI_Type_free(&MPI_Register);
    MPI_Type_free(&MPI_ResultadoEstadistico);
}
