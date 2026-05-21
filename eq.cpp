#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <iostream>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <algorithm>
#include <cstdlib>
#include <boost/program_options.hpp>
#ifdef _OPENACC
#include <openacc.h>
#endif
namespace po = boost::program_options;

#define EPS 0.001
#define MAX_ITER 1000000
#define SIZE 10
#define IND(i, j) ((i) * nx + (j))

static bool write_matrix_text(const std::string& path, const double* data, int n)
{
    std::ofstream out(path);
    if (!out) return false;
    out << n << '\n';
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            out << data[i * n + j];
            if (j + 1 < n) out << ' ';
        }
        out << '\n';
    }
    return out.good();
}

static void configure_multicore_env_defaults(int n, int requested_cores)
{
    // NVHPC OpenACC multicore uses runtime-controlled thread counts.
    // Setting these here avoids requiring the user to export env vars manually.
    // Only set defaults if user didn't already set them.
    const bool env_has_acc = (std::getenv("ACC_NUM_CORES") != nullptr);
    const bool env_has_omp = (std::getenv("OMP_NUM_THREADS") != nullptr);
    if (requested_cores <= 0 && (env_has_acc || env_has_omp)) {
        return;
    }

    int hw_threads = static_cast<int>(std::thread::hardware_concurrency());
    if (hw_threads <= 0) hw_threads = 1;

    int cores = requested_cores;
    if (cores <= 0) {
        // Heuristic: small grids often lose to overhead on many threads.
        // Tune conservatively; user can override via --cores or env vars.
        if (n <= 512) cores = 1;
        else if (n <= 1536) cores = 4;
        else cores = 8;
    }
    cores = std::max(1, std::min(cores, hw_threads));

    const std::string cores_str = std::to_string(cores);
    // overwrite=1: if user passed --cores we treat it as an explicit override.
    setenv("ACC_NUM_CORES", cores_str.c_str(), 1);
    setenv("OMP_NUM_THREADS", cores_str.c_str(), 1);

    // Affinity defaults: help avoid oversubscription/poor pinning on shared nodes.
    setenv("OMP_PROC_BIND", "true", 0);
    setenv("OMP_PLACES", "cores", 0);

    // Reduce runtime variability (some OpenMP runtimes change thread count dynamically).
    setenv("OMP_DYNAMIC", "false", 0);
}


int main(int argc, char* argv[])
{
    int N = SIZE;
    double eps = EPS;
    int max_iter = MAX_ITER;
    std::string output_path = "out.txt";
    int cores = 0;
    bool verbose = false;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("size", po::value<int>(&N), "Grid size N (NxN)")
        ("eps", po::value<double>(&eps), "Tolerance")
        ("iters", po::value<int>(&max_iter), "Maximum iterations")
        ("cores", po::value<int>(&cores)->default_value(0), "CPU threads for -acc=multicore (0=auto default)")
        ("verbose", po::bool_switch(&verbose), "Print effective ACC/OMP runtime settings")
        ("output", po::value<std::string>(&output_path)->default_value(output_path), "Output file for resulting matrix (text)");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (std::exception& e) {
        std::cerr << "Error parsing command line: " << e.what() << std::endl;
        return 1;
    }

    if (N <= 9) {
        std::cerr << "Grid size must be greater than 9." << std::endl;
        return 1;
    }
    if (eps <= 0) {
        std::cerr << "Tolerance must be positive." << std::endl;
        return 1;
    }
    if (max_iter <= 0) {
        std::cerr << "Maximum iterations must be positive." << std::endl;
        return 1;
    }
    if (vm.count("help")) {
        std::cout << desc << std::endl;
    return 0;
}

    if (verbose) {
        const char* acc_before = std::getenv("ACC_NUM_CORES");
        const char* omp_before = std::getenv("OMP_NUM_THREADS");
        std::cerr << "env_before: ACC_NUM_CORES=" << (acc_before ? acc_before : "<unset>")
                  << " OMP_NUM_THREADS=" << (omp_before ? omp_before : "<unset>")
                  << " hw_concurrency=" << std::thread::hardware_concurrency() << "\n";
    }

    // Apply runtime defaults early (before first OpenACC region).
    #if defined(ACC_MULTICORE_DEFAULTS)
    configure_multicore_env_defaults(N, cores);
    #endif

    if (verbose) {
        const char* acc_after = std::getenv("ACC_NUM_CORES");
        const char* omp_after = std::getenv("OMP_NUM_THREADS");
        const char* bind_after = std::getenv("OMP_PROC_BIND");
        const char* places_after = std::getenv("OMP_PLACES");
        const char* dyn_after = std::getenv("OMP_DYNAMIC");
        #if !defined(ACC_MULTICORE_DEFAULTS)
        std::cerr << "note: ACC_MULTICORE_DEFAULTS is not enabled in this build\n";
        #endif
        std::cerr << "env_after:  ACC_NUM_CORES=" << (acc_after ? acc_after : "<unset>")
                  << " OMP_NUM_THREADS=" << (omp_after ? omp_after : "<unset>")
                  << " OMP_PROC_BIND=" << (bind_after ? bind_after : "<unset>")
                  << " OMP_PLACES=" << (places_after ? places_after : "<unset>")
                  << " OMP_DYNAMIC=" << (dyn_after ? dyn_after : "<unset>")
                  << "\n";
    }
    double max_error = 0;
    
    int ny = N;
    int nx = N;
    int left_top = 10;
    int left_bottom = 20;
    int right_top = 20;
    int right_bottom = 30;
    double* __restrict__ local_grid = new double[ny * nx]();
    double* __restrict__ local_newgrid = new double[ny * nx]();

    // Initialize top border
    double interpolation_value_top = (double)(right_top - left_top) / (double)(nx-1);
    double interpolation_value_botton = (right_bottom - left_bottom) / (double)(nx-1);
    for (int j = 0; j < nx - 1; j++) {
        int ind = IND(ny - 1, j);
       
        local_newgrid[j] = local_grid[j] = left_top + j * interpolation_value_top;
        local_newgrid[ind] = local_grid[ind] = left_bottom + j * interpolation_value_botton;
    }
    // Initialize bottom border
    // for (int j = 0; j < nx - 1; j++) {
    //     int ind = IND(ny - 1, j);
    //     double interpolation_value = (right_bottom - left_bottom + 1) / (double)ny;
    //     local_newgrid[ind] = local_grid[ind] = left_bottom + j * interpolation_value;
    // }

    // initialize sides
    double interpolation_value_l = (left_bottom - left_top) / (double)(ny-1);
    double interpolation_value_r = (right_bottom - right_top) / (double)(ny-1);
    for (int j = 0; j < ny - 1; j++) {
        int ind = IND(j, 0);
        int ind2 = IND(j, nx - 1);

        local_newgrid[ind] = local_grid[ind] = left_top + j * interpolation_value_l;
        local_newgrid[ind2] = local_grid[ind2] = right_top + j * interpolation_value_r;
    }
    local_grid[ny * nx - 1] = 30;
    local_newgrid[ny * nx - 1] = 30;
    // std::cout<<interpolation_value_top<<" "<<interpolation_value_botton<<std::endl;

    int iter = 0;
    auto start = std::chrono::steady_clock::now();

    #pragma acc data copy(local_grid[0:ny * nx]) create(local_newgrid[0:ny * nx])
    {
        for (;;) {
            double maxdiff = 0.0;
            iter++;
            if (iter > max_iter) break;

            #pragma acc parallel loop collapse(2) gang vector present(local_grid, local_newgrid) reduction(max:maxdiff)
            for (int i = 1; i < ny - 1; i++) {
                for (int j = 1; j < nx - 1; j++) {
                    int ind = i * nx + j;
                    local_newgrid[ind] = (local_grid[ind - nx] + local_grid[ind + nx] +
                                          local_grid[ind - 1] + local_grid[ind + 1]) * 0.25;
                    double diff = local_grid[ind] - local_newgrid[ind];
                    if (diff < 0) diff = -diff;
                    if (diff > maxdiff) maxdiff = diff;
                }
            }

            #pragma acc parallel loop collapse(2) gang vector present(local_grid, local_newgrid)
            for (int i = 1; i < ny - 1; i++) {
                for (int j = 1; j < nx - 1; j++) {
                    local_grid[i * nx + j] = local_newgrid[i * nx + j];
                }
            }

            max_error = maxdiff;
            if (maxdiff < eps) break;
        }
    }

    auto end = std::chrono::steady_clock::now();
    std::cout<<"error: "<<max_error<<std::endl;
    std::chrono::duration<double> elapsed = end - start;
    std::cout<<"time: "<<elapsed.count()<<"\niterations: "<<iter<<std::endl;

    if (N == 10 || N == 13) {
        std::cout<<"\nFinal grid: (" <<N<<"x"<<N<<"):\n";
        for (int i = 0; i < nx; i++){
            for (int j = 0; j < ny; j++)
            {
                std::cout<<local_grid[i * nx + j]<<' ';
            }
            std::cout<<std::endl;
        }
    }

    if (!write_matrix_text(output_path, local_grid, N)) {
        std::cerr << "Failed to write matrix to: " << output_path << std::endl;
        delete[] local_grid;
        delete[] local_newgrid;
        return 2;
    }
    std::cout << "matrix_file: " << output_path << std::endl;
    
    delete[] local_grid;
    delete[] local_newgrid;
    return 0;
    }