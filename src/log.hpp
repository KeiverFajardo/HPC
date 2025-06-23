#pragma once

#include <format>
#include <print>

#include <mpi.h>

template <typename ...Args>
void info(std::format_string<Args...> fmt, Args &&...args)
{
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    std::print("[{}]: ", rank);
    std::println(fmt, std::forward<Args>(args)...);
}
