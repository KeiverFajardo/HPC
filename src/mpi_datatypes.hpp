#pragma once

#include <mpi.h>

extern MPI_Datatype MPI_Date;
extern MPI_Datatype MPI_Hour;
extern MPI_Datatype MPI_Register;
extern MPI_Datatype MPI_ResultadoEstadistico;
extern MPI_Datatype MPI_Umbral;

void init_mpi_datatypes();
void free_mpi_datatypes();
