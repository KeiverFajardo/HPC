add_executable(${PROJECT_NAME}
    main.cpp
    csv_reader.cpp
    mpi_datatypes.cpp
    algoritmo_a.cpp
    algoritmo_b.cpp
    estadistica.cpp
    franja_horaria.cpp
    shapefile_loader.cpp
    municipio_mapper.cpp
)

find_package(OpenMP REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC OpenMP::OpenMP_CXX)

find_package(MPI REQUIRED)
message(STATUS "Run: ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} ${MPIEXEC_MAX_NUMPROCS} ${MPIEXEC_PREFLAGS} EXECUTABLE ${MPIEXEC_POSTFLAGS} ARGS")
target_link_libraries(${PROJECT_NAME} PUBLIC MPI::MPI_CXX)

find_package(GDAL REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${GDAL_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC ${GDAL_LIBRARY})

find_package(Boost REQUIRED COMPONENTS iostreams)
target_include_directories(${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::iostreams)

add_executable(hpc_project_secuencial
    main2.cpp
    csv_reader.cpp
    estadistica.cpp
    franja_horaria.cpp
    shapefile_loader.cpp
    municipio_mapper.cpp
)

find_package(GDAL REQUIRED)
target_include_directories(hpc_project_secuencial PUBLIC ${GDAL_INCLUDE_DIR})
target_link_libraries(hpc_project_secuencial PUBLIC ${GDAL_LIBRARY})

find_package(Boost REQUIRED COMPONENTS iostreams)
target_include_directories(hpc_project_secuencial PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(hpc_project_secuencial PRIVATE Boost::iostreams)
