#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <iostream>
#include <chrono>
#include <boost/program_options.hpp>
#ifdef _OPENACC
#include <openacc.h>
#endif
namespace po = boost::program_options;

#define EPS 0.001
#define MAX_ITER 1000000
#define SIZE 10
#define IND(i, j) ((i) * nx + (j))


int main(int argc, char* argv[])
{
    int N = SIZE;
    double eps = EPS;
    int max_iter = MAX_ITER;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("size", po::value<int>(&N), "Grid size N (NxN)")
        ("eps", po::value<double>(&eps), "Tolerance")
        ("iters", po::value<int>(&max_iter), "Maximum iterations");

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
    double max_error = 0;
    
    int ny = N;
    int nx = N;
    int left_top = 10;
    int left_bottom = 20;
    int right_top = 20;
    int right_bottom = 30;
    std::vector<double> local_grid(ny * nx, 0.0);
    std::vector<double> local_newgrid(ny * nx, 0.0);

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

        for (;;) {
            double maxdiff = 0.0;   
            iter++;
            if (iter > max_iter) break;
            #pragma acc kernels loop reduction(max:maxdiff)
            for (int i = 1; i < ny - 1; i++) { // Update interior points
                #pragma acc loop reduction(max:maxdiff)
                for (int j = 1; j < nx - 1; j++) {
                    local_newgrid[i * nx + j] =
                    (local_grid[(i-1) * nx + j] + local_grid[(i+1) * nx + j] +
                    local_grid[i * nx + j - 1] + local_grid[i * nx + j + 1]) * 0.25;
                    int ind = i * nx + j;
                    maxdiff = fmax(maxdiff, fabs(local_grid[ind] - local_newgrid[ind]));
                    

                }
            }

            #pragma acc kernels loop
            for (int i = 0; i < ny; i++) {
                #pragma acc loop
                for (int j = 0; j < nx; j++) {
                    local_grid[i * nx + j] = local_newgrid[i * nx + j];
                }
            }
            max_error = maxdiff;
            if (maxdiff < eps) break;
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
    
    return 0;
    }