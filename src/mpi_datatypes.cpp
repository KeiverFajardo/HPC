#include "mpi_datatypes.hpp"

#include "csv_reader.hpp"

MPI_Datatype MPI_Date;
MPI_Datatype MPI_Hour;
MPI_Datatype MPI_Register;

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
    int blocklengths[7] = {1, 1, 1, 1, 1, 1, 1};
    MPI_Aint displacements[7];
    MPI_Datatype types[7] = {
        MPI_UNSIGNED,       // cod_detector
        MPI_UNSIGNED,       // id_carril
        MPI_Date,           // fecha
        MPI_Hour,           // hora
        MPI_FLOAT,          // latitud
        MPI_FLOAT,          // longitud
        MPI_FLOAT           // velocidad
    };

    displacements[0] = offsetof(Register, cod_detector);
    displacements[1] = offsetof(Register, id_carril);
    displacements[2] = offsetof(Register, fecha);
    displacements[3] = offsetof(Register, hora);
    displacements[4] = offsetof(Register, latitud);
    displacements[5] = offsetof(Register, longitud);
    displacements[6] = offsetof(Register, velocidad);

    MPI_Type_create_struct(7, blocklengths, displacements, types, &MPI_Register);
    MPI_Type_commit(&MPI_Register);
}

void init_mpi_datatypes()
{
    init_mpi_date();
    init_mpi_hour();
    init_mpi_register();
}

void free_mpi_datatypes()
{
    MPI_Type_free(&MPI_Date);
    MPI_Type_free(&MPI_Hour);
    MPI_Type_free(&MPI_Register);
}
