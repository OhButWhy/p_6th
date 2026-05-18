#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <iostream>
#include <chrono>

#define EPS 0.001
#define IND(i, j) ((i) * nx + (j))


int main() //int argc, char **argv
{
    // if (argc < 3) {
    //     return 1;
    // }
    // int m = atoi(argv[1]); 
    // int p = atoi(argv[2]);
    int k = 0;
    double max_error = 0;
    auto start = std::chrono::steady_clock::now();
    int rows = 21;
    int cols = 21;
    int ny = rows;
    int nx = cols;
    int left_top = 10;
    int left_bottom = 20;
    int right_top = 20;
    int right_bottom = 30;
    std::vector<double> local_grid(ny * nx, 0.0);
    std::vector<double> local_newgrid(ny * nx, 0.0);

    double dx = 1.0 / (nx - 1.0);

    // Initialize top border
    double interpolation_value_top = (double)(right_top - left_top) / (double)nx;
    double interpolation_value_botton = (right_bottom - left_bottom) / (double)nx;
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
    double interpolation_value_l = (left_bottom - left_top) / (double)ny;
    double interpolation_value_r = (right_bottom - right_top) / (double)ny;
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
    for (;;) {
        double maxdiff = 0.0;
        iter++;
        for (int i = 1; i < ny - 1; i++) { // Update interior points
            for (int j = 1; j < nx - 1; j++) {
                local_newgrid[IND(i, j)] =
                (local_grid[IND(i - 1, j)] + local_grid[IND(i + 1, j)] +
                local_grid[IND(i, j - 1)] + local_grid[IND(i, j + 1)]) * 0.25;
                int ind = IND(i, j);
                maxdiff = fmax(maxdiff, fabs(local_grid[ind] - local_newgrid[ind]));
                

            }
        }

        local_grid = local_newgrid;
        max_error = maxdiff;
        if (maxdiff < EPS)
        break;
    }
    std::cout<<"error: "<<max_error<<std::endl;
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout<<"time: "<<elapsed.count()<<"\niterations: "<<iter<<std::endl;
    // for (int i = 0; i < nx; i++){
    //     for (int j = 0; j < ny; j++)
    //     {
    //         std::cout<<local_grid[i * nx + j]<<' ';
    //     }
    //     std::cout<<std::endl;
    // }

    
    return 0;
    }