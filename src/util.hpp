#pragma once

#include <mpi.h>

#include <cstdio>
#include <stdlib.h>
#include <print>

constexpr const char *ANSI_COLOR_RED     = "\x1b[31m";
constexpr const char *ANSI_COLOR_GREEN   = "\x1b[32m";
constexpr const char *ANSI_COLOR_YELLOW  = "\x1b[33m";
constexpr const char *ANSI_COLOR_BLUE    = "\x1b[34m";
constexpr const char *ANSI_COLOR_MAGENTA = "\x1b[35m";
constexpr const char *ANSI_COLOR_CYAN    = "\x1b[36m";
constexpr const char *ANSI_COLOR_RESET   = "\x1b[0m";

inline void print_error(
    int error_code,
    const char *function_call_text,
    const char *filename,
    unsigned int line
) {
    int error_name_len = -1;
    char error_name_buf[MPI_MAX_ERROR_STRING];
    MPI_Error_string(error_code, error_name_buf, &error_name_len);
    std::println(
        stderr,
        "Error {} while running {} on {}:{}",
        error_name_buf,
        function_call_text,
        filename,
        line
    );
    exit(error_code);
}

#define MPI_CHK(function_to_run)                                                                   \
    do                                                                                             \
    {                                                                                              \
        /*{ \
            int rank; \
            MPI_Comm_rank(MPI_COMM_WORLD, &rank); \
            printf("[%d]: %s:%d\n", rank, #function_to_run, __LINE__); \
        }*/ \
                                                                                                   \
        int error_code = function_to_run;                                                          \
        if (error_code) {                                                                          \
            print_error(error_code, #function_to_run, __FILE__, __LINE__);                         \
        }                                                                                          \
    }                                                                                              \
    while (0)
