#include "ShallowWater.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <fstream> 
#include <cblas.h>
#include <cstdlib>

#include <omp.h>

#define g 9.81

// constructor definition
ShallowWater::ShallowWater(){
}   // Default Constructor

ShallowWater::ShallowWater(double dtt, double Tt, int Nxx, int Nyy, int icc, double dxx, double dyy, int typeAnalysis) : dt(dtt), T(Tt), Nx(Nxx), Ny(Nyy), ic(icc), dx(dxx), dy(dyy), analysis(typeAnalysis){
}   // Constructor using initialization list to avoid calling default class constructor and then over writting

ShallowWater::~ShallowWater(){
std::cout << "Class destroyed" << std::endl;

delete[] h;
delete[] u;
delete[] v;
}   // Custom destructor definition

// Method definition

void ShallowWater::SetInitialCondition(){// ARRAY OF POINTER TO POINTER WILL BE GENERATED. 
    // Output[i] will contain the ith column of the initial condition, corresponding
    // to the points [x0,y0], [x0,y1], ... [x0,yn].

    h = new double[Nx*Ny];
    u = new double[Nx*Ny];
    v = new double[Nx*Ny];
    
    for (int i = 0; i<Nx*Ny; i++){
        u[i] =  0;
        v[i] =  0;
    }
    // Populate 2 dimensional array depending on Index of initial condition
    
    switch (ic){
        case 1:
            for (int i = 0; i<Nx; i++){
                for (int j = 0; j<Ny; j++){
//                    g[i][j] = (double) (std::exp(-(i*dx-50)*(i*dx-50)/25));
//                    g[i][j] = i+1 + (j+1)*10;
                    h[i*Ny + j] = (double) (10 + std::exp(-(i*dx-Nx/2.)*(i*dx-Nx/2.)/(Nx/4.)));
                }
            }
            break;
        case 2:
            for (int i = 0; i<Nx; i++){
                for (int j = 0; j<Ny; j++){
//                    h[i*Nx + j] = (double) ( std::exp(-(j*dy-50)*(j*dy-50)/25));
                    h[i*Ny + j] = (double) (10+ std::exp(-(j*dy-Ny/2.)*(j*dy-Ny/2.)/(Nx/4.)));
                }
            }
            break;
        case 3: 
            for (int i = 0; i<Nx; i++){
                for (int j = 0; j<Ny; j++){
                    h[i*Ny + j] = (double) (10 + std::exp(-((i*dx-50)*(i*dx-50) + (j*dy-50)*(j*dy-50))/25.));
                }
            }
            break;
        case 4: 
            for (int i = 0; i<Nx; i++){
                for (int j = 0; j<Ny; j++){
                    h[i*Ny + j] = (double) (10 + std::exp(-((i*dx-25)*(i*dx-25) + (j*dy-25)*(j*dy-25))/25.) + std::exp(-((i*dx-75)*(i*dx-75) + (j*dy-75)*(j*dy-75))/25.));
                }
            }
            break;
    }
}


void ShallowWater::TimeIntegrate(){
    std::cout << std::setprecision(16) << std::fixed;
    std::string str;
    
    int threadid;
    int NumThreads;
    
    int remainder_col =0;
    int remainder_row = 0;
    int local_col, local_row;
    
    int* cumsum_col = nullptr;
    int* cumsum_row = nullptr;
    
    int* additional_col = nullptr;
    int* additional_row = nullptr;
    
    int dim = Nx*Ny;
    
    double* ku = new double[dim];
    double* kv = new double[dim];
    double* kh = new double[dim];
    
    double* kutemp = new double[dim];
    double* kvtemp = new double[dim];
    double* khtemp = new double[dim];    
    
    double* unew = new double[dim];
    double* vnew = new double[dim];
    double* hnew = new double[dim];
    
    double* dhdx = new double[dim];
    double* dudx = new double[dim];
    double* dvdx = new double[dim];

    double* dhdy = new double[dim];
    double* dudy = new double[dim];
    double* dvdy = new double[dim];
    
    double coeffs[6] = {-0.016667, 0.15, -0.75, 0.75, -0.15, 0.016667};
    double RKcoeffs[4] = {dt/6, dt/3, dt/3, dt/6};
    double kcoeffs[3] = {dt/2, dt/2, dt};

    // Open branch of threads
    #pragma omp parallel default(shared) private(threadid) 
    {
        threadid = omp_get_thread_num(); // Get number of threads
        
        // Perform calculatrions to divide computations betwwen threads.
        // Instead of splitting up the nodes evenly betwwen threads, 
        // problem will be splitted into columns and rows. Even though the
        // approach proposed will cause work to be splitted unevenly betwwen 
        // threads, other benefits can be seen by using cache firendly operations
        if (threadid == 0){
            NumThreads = omp_get_num_threads();
                        
            remainder_col = Nx%NumThreads;
            local_col = (Nx - remainder_col)/NumThreads;
            
            remainder_row = Ny%NumThreads;
            local_row = (Ny - remainder_row)/NumThreads;
            
            std::cout << "\t" << "Number of threads:\t" <<"\t" << NumThreads << "\n" << std::endl;
            
            // Populating initial pointer shifting array due to reaminder of columns
            cumsum_col = new int[NumThreads];
            additional_col = new int[NumThreads];
            for (int i =0; i< NumThreads; i++){
                cumsum_col[i] = i*local_col;
                additional_col[i] = 0;
            }
            if ( remainder_col != 0) {
                for (int i = 1 ; i < remainder_col+1; i++){
                    additional_col[i-1] = 1;
                    cumsum_col[i] = cumsum_col[i-1] + local_col + 1;
                }
                for (int i = remainder_col+1; i<NumThreads; i++){
                    cumsum_col[i] = cumsum_col[i-1] + local_col;
                }
            }
            
            // Same for rows
            cumsum_row = new int[NumThreads];
            additional_row = new int[NumThreads];
            for (int i =0; i< NumThreads; i++){
                cumsum_row[i] = i*local_row;
                additional_row[i] = 0;
            }
            if ( remainder_row != 0) {
                for (int i = 1 ; i < remainder_row+1; i++){
                    additional_row[i-1] = 1;
                    cumsum_row[i] = cumsum_row[i-1] + local_row + 1;
                }
                for (int i = remainder_row+1; i<NumThreads; i++){
                    cumsum_row[i] = cumsum_row[i-1] + local_row;
                }
            }
        }
        #pragma omp barrier
        
        // Start integration loop 
        double t = dt;
        while (t < T + dt/2){
            // Calculate k1 and propagate Snew
            GetDerivativesParallel(local_row+additional_row[threadid], local_col + additional_col[threadid], u + (cumsum_row[threadid]),u + (cumsum_col[threadid])*Ny,  dudx + (cumsum_row[threadid]), dudy + (cumsum_col[threadid])*Ny, coeffs);
            GetDerivativesParallel(local_row+additional_row[threadid], local_col + additional_col[threadid], v + (cumsum_row[threadid]),v + (cumsum_col[threadid])*Ny,  dvdx + (cumsum_row[threadid]), dvdy + (cumsum_col[threadid])*Ny, coeffs);
            GetDerivativesParallel(local_row+additional_row[threadid], local_col + additional_col[threadid], h + (cumsum_row[threadid]),h + (cumsum_col[threadid])*Ny,  dhdx + (cumsum_row[threadid]), dhdy + (cumsum_col[threadid])*Ny, coeffs);
            
            #pragma omp barrier
            
            for (int node = (cumsum_col[threadid])*Ny; node < (cumsum_col[threadid] + local_col + additional_col[threadid])*Ny; node++){
                ku[node] = -u[node]*dudx[node] - v[node]*dudy[node] - g*dhdx[node];
                unew[node] = u[node] + RKcoeffs[0] * ku[node];
                
                kv[node] =  -u[node]*dvdx[node] - v[node]*dvdy[node] - g*dhdy[node];
                vnew[node] = v[node] + RKcoeffs[0] * kv[node];
                
                kh[node] = -h[node]*dudx[node] - u[node]*dhdx[node] - h[node]*dvdy[node] - v[node]*dhdy[node];
                hnew[node] = h[node] + RKcoeffs[0] * kh[node];
                
                u[node] += kcoeffs[0]*ku[node];
                v[node] += kcoeffs[0]*kv[node];
                h[node] += kcoeffs[0]*kh[node];
            }            
            
            #pragma omp barrier
            
            // Calculate k2 and propagate Snew
            GetDerivativesParallel(local_row+additional_row[threadid], local_col + additional_col[threadid], u + (cumsum_row[threadid]),u + (cumsum_col[threadid])*Ny,  dudx + (cumsum_row[threadid]), dudy + (cumsum_col[threadid])*Ny, coeffs);
            GetDerivativesParallel(local_row+additional_row[threadid], local_col + additional_col[threadid], v + (cumsum_row[threadid]),v + (cumsum_col[threadid])*Ny,  dvdx + (cumsum_row[threadid]), dvdy + (cumsum_col[threadid])*Ny, coeffs);
            GetDerivativesParallel(local_row+additional_row[threadid], local_col + additional_col[threadid], h + (cumsum_row[threadid]),h + (cumsum_col[threadid])*Ny,  dhdx + (cumsum_row[threadid]), dhdy + (cumsum_col[threadid])*Ny, coeffs);
            
            #pragma omp barrier
            
            for (int node = (cumsum_col[threadid])*Ny; node < (cumsum_col[threadid] + local_col + additional_col[threadid])*Ny; node++){
                kutemp[node] = ku[node];
                ku[node] = -u[node]*dudx[node] - v[node]*dudy[node] - g*dhdx[node];
                unew[node] += RKcoeffs[1] * ku[node];
                
                kvtemp[node] = kv[node];
                kv[node] =  -u[node]*dvdx[node] - v[node]*dvdy[node] - g*dhdy[node];
                vnew[node] += RKcoeffs[1] * kv[node];
                
                khtemp[node] = kh[node];
                kh[node] = -h[node]*dudx[node] - u[node]*dhdx[node] - h[node]*dvdy[node] - v[node]*dhdy[node];
                hnew[node] += RKcoeffs[1] * kh[node];
                
                u[node] += kcoeffs[1]*ku[node] - kcoeffs[0]*kutemp[node];
                v[node] += kcoeffs[1]*kv[node] - kcoeffs[0]*kvtemp[node];
                h[node] += kcoeffs[1]*kh[node] - kcoeffs[0]*khtemp[node];
            }
            #pragma omp barrier
            
            // Calculate k3 and propagate Snew
            GetDerivativesParallel(local_row+additional_row[threadid], local_col + additional_col[threadid], u + (cumsum_row[threadid]),u + (cumsum_col[threadid])*Ny,  dudx + (cumsum_row[threadid]), dudy + (cumsum_col[threadid])*Ny, coeffs);
            GetDerivativesParallel(local_row+additional_row[threadid], local_col + additional_col[threadid], v + (cumsum_row[threadid]),v + (cumsum_col[threadid])*Ny,  dvdx + (cumsum_row[threadid]), dvdy + (cumsum_col[threadid])*Ny, coeffs);
            GetDerivativesParallel(local_row+additional_row[threadid], local_col + additional_col[threadid], h + (cumsum_row[threadid]),h + (cumsum_col[threadid])*Ny,  dhdx + (cumsum_row[threadid]), dhdy + (cumsum_col[threadid])*Ny, coeffs);
            
            #pragma omp barrier
            
            for (int node = (cumsum_col[threadid])*Ny; node < (cumsum_col[threadid] + local_col + additional_col[threadid])*Ny; node++){
                kutemp[node] = ku[node];
                ku[node] = -u[node]*dudx[node] - v[node]*dudy[node] - g*dhdx[node];
                unew[node] += RKcoeffs[2] * ku[node];
                
                kvtemp[node] = kv[node];
                kv[node] =  -u[node]*dvdx[node] - v[node]*dvdy[node] - g*dhdy[node];
                vnew[node] += RKcoeffs[2] * kv[node];
                
                khtemp[node] = kh[node];
                kh[node] = -h[node]*dudx[node] - u[node]*dhdx[node] - h[node]*dvdy[node] - v[node]*dhdy[node];
                hnew[node] += RKcoeffs[2] * kh[node];
                
                u[node] += kcoeffs[2]*ku[node] - kcoeffs[1]*kutemp[node];
                v[node] += kcoeffs[2]*kv[node] - kcoeffs[1]*kvtemp[node];
                h[node] += kcoeffs[2]*kh[node] - kcoeffs[1]*khtemp[node];
            }
            #pragma omp barrier
            
            // Calculate k3 and propagate Snew
            GetDerivativesParallel(local_row+additional_row[threadid], local_col + additional_col[threadid], u + (cumsum_row[threadid]),u + (cumsum_col[threadid])*Ny,  dudx + (cumsum_row[threadid]), dudy + (cumsum_col[threadid])*Ny, coeffs);
            GetDerivativesParallel(local_row+additional_row[threadid], local_col + additional_col[threadid], v + (cumsum_row[threadid]),v + (cumsum_col[threadid])*Ny,  dvdx + (cumsum_row[threadid]), dvdy + (cumsum_col[threadid])*Ny, coeffs);
            GetDerivativesParallel(local_row+additional_row[threadid], local_col + additional_col[threadid], h + (cumsum_row[threadid]),h + (cumsum_col[threadid])*Ny,  dhdx + (cumsum_row[threadid]), dhdy + (cumsum_col[threadid])*Ny, coeffs);
            
            #pragma omp barrier
            
            for (int node = (cumsum_col[threadid])*Ny; node < (cumsum_col[threadid] + local_col + additional_col[threadid])*Ny; node++){
                ku[node] = -u[node]*dudx[node] - v[node]*dudy[node] - g*dhdx[node];
                kv[node] =  -u[node]*dvdx[node] - v[node]*dvdy[node] - g*dhdy[node];
                kh[node] = -h[node]*dudx[node] - u[node]*dhdx[node] - h[node]*dvdy[node] - v[node]*dhdy[node];
                
                u[node] = unew[node] + RKcoeffs[3] * ku[node];
                v[node] = vnew[node] + RKcoeffs[3] * kv[node];
                h[node] = hnew[node] + RKcoeffs[3] * kh[node];
            }
            #pragma omp barrier
            
//            #pragma omp critical
//            if (threadid == 0){
//                std::cout << std::string(str.length(),'\b');
//                str = "Time: " + std::to_string(t) + ". " + std::to_string((int) ((t)/dt)) + " time steps done out of " + std::to_string((int) (T/dt)) + ".";
//                std::cout << str;
//            }
            t+=dt;
        }
    }
    
    delete[] cumsum_col;
    delete[] additional_col;
    delete[] cumsum_row;
    delete[] additional_row;
            
    delete[] kutemp;
    delete[] kvtemp;
    delete[] khtemp;
    
    delete[] unew;
    delete[] vnew;
    delete[] hnew;
    
    delete[] dhdx;
    delete[] dudx;
    delete[] dvdx;

    delete[] dhdy;
    delete[] dudy;
    delete[] dvdy;

    delete[] ku;
    delete[] kv;
    delete[] kh;
            
}

void ShallowWater::TimeIntegrateBLAS(){ 
    
    std::string str;
    
    // Populate Differentiation matrix (Only Required by BLAS implementation)
    int ldsy = 3*Ny;
    int dimS = ldsy*Nx;
    int kl = 3; 
    int ku = 3;
    int lday = 1+ kl + ku;
    
    // Initialize variables
    double* B = new double[5*3*Ny*Nx];
    double* C = new double[3*3*Ny*Nx];
    
    double* S = new double[dimS];
    double* Snew = new double[dimS];
    double* k2 = new double[dimS];
    double* k1 = new double[dimS];
    double* dSdx = new double[dimS];
    double* dSdy = new double[dimS];
    
    double coeffs[6] = {-0.016667, 0.15, -0.75, 0.75, -0.15, 0.016667};
    double RK4coeffs[4] = {dt/6, dt/3, dt/3, dt/6};
    double kcoeffs[3] = {dt/2, dt/2, dt};
    
    // Construct state vector S = [u, v, h]^T. [u,v,h] for all nodes stack on yop of each other in a column major way
    ShallowWater::ConstructSVector(S);
    
    // Start integration loop 
    double t = dt;
    while (t < T + dt/2){
        
        // Calculate k1 and propagate Snew
//        EvaluateFuncBlasV2(kl, ku, A, lday, S, ldsy, coeffs, k1, B, C);
        EvaluateFuncBlasV3(kl, ku, lday, S, ldsy, coeffs, k1, dSdx, dSdy, B, C);

        for (int i = 0; i<dimS; i++){
            Snew[i] = S[i] + RK4coeffs[0]*k1[i];
            S[i] += kcoeffs[0]*k1[i];
        }
        // Calculate k2 and propagate Snew
//        EvaluateFuncBlasV2(kl, ku, A, lday, S, ldsy, coeffs, k2, B, C);
        EvaluateFuncBlasV3(kl, ku, lday, S, ldsy, coeffs, k2, dSdx, dSdy, B, C);
    

        for (int i = 0; i<dimS; i++){
            Snew[i] += RK4coeffs[1]*k2[i];
            S[i] += kcoeffs[1]*k2[i] -kcoeffs[0]*k1[i];
        }
       
        // Calculate k3 and propagate Snew
//        EvaluateFuncBlasV2(kl, ku, A, lday, S, ldsy, coeffs, k1, B, C);
        EvaluateFuncBlasV3(kl, ku, lday, S, ldsy, coeffs, k1, dSdx, dSdy, B, C);

        for (int i = 0; i<dimS; i++){
            Snew[i] += RK4coeffs[2]*k1[i];
            S[i] +=  kcoeffs[2]*k1[i]- kcoeffs[1]*k2[i];
        }

            
        
        // Calculate k4 and update S for next iteration
//        EvaluateFuncBlasV2(kl, ku, A, lday, S, ldsy, coeffs, k2, B, C);
         EvaluateFuncBlasV3(kl, ku, lday, S, ldsy, coeffs, k2, dSdx, dSdy, B, C);
         
        for (int i = 0; i<dimS; i++){
            S[i] = Snew[i] + RK4coeffs[3]*k2[i];
        }
        

        std::cout << std::string(str.length(),'\b');
        str = "Time: " + std::to_string(t) + ". " + std::to_string((int) ((t)/dt)) + " time steps done out of " + std::to_string((int) (T/dt)) + ".";
        std::cout << str;
        t += dt;
    }  
    
    for (int i = 0; i<dimS; i+=3){
        u[i/3] = S[i];
        v[i/3] = S[i+1];
        h[i/3] = S[i+2];
    }    
    
    delete[] B;
    delete[] C;
    delete[] S;
    delete[] Snew;
    delete[] k2;
    delete[] k1;
    delete[] dSdx;
    delete[] dSdy;
    
    
}



void ShallowWater::GetDerivativesParallel(const int& rows, const int& cols, const double* varx, const double* vary,  double* dvardx, double* dvardy, const double* coeffs){
     // Calculate derivatives in direction x and y (ASSUME SQUARE)
    int ldy = Ny;    
    int ldy2 = 2*ldy;
    int ldy3 = 3*ldy;
    int ldx = Nx*ldy;
    // X - DERIVATIVES
    for (int iy = 0; iy < rows; iy++){
        // Boundary points for ix <3 and ix > Nx-3   
        // Left most points
        dvardx[iy] = coeffs[0]*varx[iy + ldx - (ldy3 + ldy)] + coeffs[1]*varx[iy + ldx - ldy3] + coeffs[2]*varx[iy + ldx - ldy2] + coeffs[3]*varx[iy + ldy] + coeffs[4]*varx[iy + ldy2] + coeffs[5]*varx[iy + ldy3];
        dvardx[iy + ldy] = coeffs[0]*varx[iy + ldx -ldy3] + coeffs[1]*varx[iy + ldx - ldy2] + coeffs[2]*varx[iy] + coeffs[3]*varx[iy +ldy2] + coeffs[4]*varx[iy + ldy3] + coeffs[5]*varx[iy + ldy3 + ldy];
        dvardx[iy + ldy2] = coeffs[0]*varx[iy +  ldx - ldy2] + coeffs[1]*varx[iy] + coeffs[2]*varx[iy + ldy] + coeffs[3]*varx[iy + ldy3] + coeffs[4]*varx[iy + ldy3 + ldy] + coeffs[5]*varx[iy + ldy3 + ldy2];
        
        // Right most points
        dvardx[iy+ldx-ldy] = coeffs[0]*varx[iy + ldx - (ldy3 + ldy)] + coeffs[1]*varx[iy + ldx - ldy3] + coeffs[2]*varx[iy + ldx - ldy2] + coeffs[3]*varx[iy + ldy] + coeffs[4]*varx[iy + ldy2] + coeffs[5]*varx[iy + ldy3];
        dvardx[iy+ldx-ldy2] = coeffs[0]*varx[iy + ldx - (ldy3 + ldy2)] + coeffs[1]*varx[iy + ldx - (ldy3 + ldy)] + coeffs[2]*varx[iy + ldx - ldy3] + coeffs[3]*varx[iy] + coeffs[4]*varx[iy + ldy] + coeffs[5]*varx[iy + ldy2];
        dvardx[iy+ldx-ldy3] = coeffs[0]*varx[iy + ldx - (ldy3 + ldy3)] + coeffs[1]*varx[iy + ldx - (ldy3 + ldy2)] + coeffs[2]*varx[iy + ldx - (ldy3 + ldy)] + coeffs[3]*varx[iy + ldx - ldy2] + coeffs[4]*varx[iy] + coeffs[5]*varx[iy + ldy];
    
        // Inner points
        int counter = ldy3 + iy;
        for (int ix = 3; ix<Nx-3; ix++){
            dvardx[counter] = coeffs[0]*varx[counter - ldy3] + coeffs[1]*varx[counter - ldy2] + coeffs[2]*varx[counter - ldy] + coeffs[3]*varx[counter + ldy] + coeffs[4]*varx[counter + ldy2] + coeffs[5]*varx[counter + ldy3];
            counter += ldy;
        }
    }
        
    // Y - DERIVATVES
    
    for (int ix = 0; ix < cols; ix++){
        // Boundary points for iy <3 and iy > Nx-3
        int counter = ldy*ix;
        // Top points
        dvardy[counter] = coeffs[0]*vary[counter + Ny-4] + coeffs[1]*vary[counter + Ny -3] + coeffs[2]*vary[counter + Ny-2] + coeffs[3]*vary[counter+1] + coeffs[4]*vary[counter+2] + coeffs[5]*vary[counter+3];
        dvardy[counter + 1] = coeffs[0]*vary[counter + Ny-3] + coeffs[1]*vary[counter + Ny -2] + coeffs[2]*vary[counter + 0] + coeffs[3]*vary[counter+2] + coeffs[4]*vary[counter+3] + coeffs[5]*vary[counter+4];
        dvardy[counter + 2] = coeffs[0]*vary[counter + Ny-2] + coeffs[1]*vary[counter + 0] + coeffs[2]*vary[counter + 1] + coeffs[3]*vary[counter+3] + coeffs[4]*vary[counter+4] + coeffs[5]*vary[counter+5];
        
        
        // Bottom points
        dvardy[Ny-1+counter] = coeffs[0]*vary[counter + Ny-4] + coeffs[1]*vary[counter + Ny -3] + coeffs[2]*vary[counter + Ny-2] + coeffs[3]*vary[counter+1] + coeffs[4]*vary[counter +2] + coeffs[5]*vary[counter +3];
        dvardy[Ny-2+counter] = coeffs[0]*vary[counter + Ny-5] + coeffs[1]*vary[counter + Ny -4] + coeffs[2]*vary[counter + Ny-3] + coeffs[3]*vary[counter+Ny-1] + coeffs[4]*vary[counter +1] + coeffs[5]*vary[counter +2];
        dvardy[Ny-3+counter] = coeffs[0]*vary[counter + Ny-6] + coeffs[1]*vary[counter + Ny -5] + coeffs[2]*vary[counter + Ny-4] + coeffs[3]*vary[counter+Ny-2] + coeffs[4]*vary[counter + Ny-1] + coeffs[5]*vary[counter + 1];
        
        // Inner points
        for (int iy = 3; iy<Ny-3; iy++){
            dvardy[iy+counter] = coeffs[0]*vary[iy-3 + counter] + coeffs[1]*vary[iy -2 + counter] + coeffs[2]*vary[iy -1 + counter] + coeffs[3]*vary[iy + 1 + counter] + coeffs[4]*vary[iy + 2 + counter] + coeffs[5]*vary[iy + 3 + counter];
        }
    }
}

void ShallowWater::GetDerivativesBLASV2(const double* S, double* dSdx, double* dSdy, const double* coeffs){
    // Calculate derivatives in direction x and y (ASSUME SQUARE)
    int ldy = 3*Ny;    
    int ldy2 = 2*ldy;
    int ldy3 = 3*ldy;   
    int ldx = ldy*Nx;
    
    // X - DERIVATIVES
    for (int iy = 0; iy < ldy; iy++){
        // Boundary points for ix <3 and ix > Nx-3
     // Left most points
        dSdx[iy] = coeffs[0]*S[iy + ldx - (ldy3 + ldy)] + coeffs[1]*S[iy + ldx - ldy3] + coeffs[2]*S[iy + ldx - ldy2] + coeffs[3]*S[iy + ldy] + coeffs[4]*S[iy + ldy2] + coeffs[5]*S[iy + ldy3];
        dSdx[iy + ldy] = coeffs[0]*S[iy + ldx -ldy3] + coeffs[1]*S[iy + ldx - ldy2] + coeffs[2]*S[iy] + coeffs[3]*S[iy +ldy2] + coeffs[4]*S[iy + ldy3] + coeffs[5]*S[iy + ldy3 + ldy];
        dSdx[iy + ldy2] = coeffs[0]*S[iy +  ldx - ldy2] + coeffs[1]*S[iy] + coeffs[2]*S[iy + ldy] + coeffs[3]*S[iy + ldy3] + coeffs[4]*S[iy + ldy3 + ldy] + coeffs[5]*S[iy + ldy3 + ldy2];
        
        // Right most points
        dSdx[iy+ldx-ldy] = coeffs[0]*S[iy + ldx - (ldy3 + ldy)] + coeffs[1]*S[iy + ldx - ldy3] + coeffs[2]*S[iy + ldx - ldy2] + coeffs[3]*S[iy + ldy] + coeffs[4]*S[iy + ldy2] + coeffs[5]*S[iy + ldy3];
        dSdx[iy+ldx-ldy2] = coeffs[0]*S[iy + ldx - (ldy3 + ldy2)] + coeffs[1]*S[iy + ldx - (ldy3 + ldy)] + coeffs[2]*S[iy + ldx - ldy3] + coeffs[3]*S[iy] + coeffs[4]*S[iy + ldy] + coeffs[5]*S[iy + ldy2];
        dSdx[iy+ldx-ldy3] = coeffs[0]*S[iy + ldx - (ldy3 + ldy3)] + coeffs[1]*S[iy + ldx - (ldy3 + ldy2)] + coeffs[2]*S[iy + ldx - (ldy3 + ldy)] + coeffs[3]*S[iy + ldx - ldy2] + coeffs[4]*S[iy] + coeffs[5]*S[iy + ldy];
    
        // Inner points
        int counter = 3*ldy+iy;
        for (int ix = 3; ix<Nx-3; ix++){
            dSdx[counter] = coeffs[0]*S[counter - ldy3] + coeffs[1]*S[counter - ldy2] + coeffs[2]*S[counter -ldy] + coeffs[3]*S[counter +ldy] + coeffs[4]*S[counter + ldy2] + coeffs[5]*S[counter +ldy3];
            counter += ldy;
        }
    }
            
    // Y - DERIVATVES
    for (int ix = 0; ix < Nx; ix++){
        // Boundary points for iy <3 and iy > Nx-3
        for (int i =  0; i < 3; i++){
            int counter =  ix*ldy;
            // Top points
            dSdy[0 + i + counter] = coeffs[0]*S[counter + ldy - 12 + i] + coeffs[1]*S[counter + ldy -9 + i] + coeffs[2]*S[counter + ldy -6 + i] + coeffs[3]*S[counter + 3 + i] + coeffs[4]*S[counter + 6 + i] + coeffs[5]*S[counter + 9 + i];
            dSdy[3 + i + counter] = coeffs[0]*S[counter + ldy - 9 + i] + coeffs[1]*S[counter + ldy - 6 + i] + coeffs[2]*S[counter + i] + coeffs[3]*S[counter + 6 + i] + coeffs[4]*S[counter + 9 + i] + coeffs[5]*S[counter + 12 + i];
            dSdy[6 + i + counter] = coeffs[0]*S[counter + ldy - 6 + i] + coeffs[1]*S[counter + i] + coeffs[2]*S[counter + 3 + i] + coeffs[3]*S[counter + 9 + i] + coeffs[4]*S[counter + 12 + i] + coeffs[5]*S[counter + 15 + i];
            
            // Bottom points
            dSdy[ldy - 3 + i+counter] = coeffs[0]*S[counter + ldy - 12 + i] + coeffs[1]*S[counter + ldy - 9 + i] + coeffs[2]*S[counter + ldy - 6 + i] + coeffs[3]*S[counter + 3 + i] + coeffs[4]*S[counter + 6 + i] + coeffs[5]*S[counter + 9 + i];
            dSdy[ldy - 6 + i+counter] = coeffs[0]*S[counter + ldy - 15 + i] + coeffs[1]*S[counter + ldy - 12 + i] + coeffs[2]*S[counter + ldy - 9 + i] + coeffs[3]*S[counter + ldy - 3 + i] + coeffs[4]*S[counter + 3 + i] + coeffs[5]*S[counter + 6 + i];
            dSdy[ldy - 9 + i+counter] = coeffs[0]*S[counter + ldy - 18 + i] + coeffs[1]*S[counter + ldy - 15 + i] + coeffs[2]*S[counter + ldy - 12 + i] + coeffs[3]*S[counter + ldy - 6 + i] + coeffs[4]*S[counter + ldy  - 3 + i] + coeffs[5]*S[counter + 3 + i];
            
            // Inner points
            for (int iy = 9; iy<3*Ny-9; iy+=3){
                dSdy[iy + i + counter] = coeffs[0]*S[iy + i - 9 + counter] + coeffs[1]*S[iy + i - 6 + counter] + coeffs[2]*S[iy + i - 3 + counter] + coeffs[3]*S[iy + i + 3+  counter] + coeffs[4]*S[iy + i + 6+ counter] + coeffs[5]*S[iy + i + 9 + counter];
            }
        }
    }
    
}



void ShallowWater::EvaluateFuncBlasV3(const int& kla, const int& kua, const int& lday, double* S, const int& ldsy, const double* coeffs, double* k, double* dSdx, double* dSdy, double* B, double* C){
    // k = F(S) = - B*d(S)/dx - C*d(S)/dy
    int dimS = ldsy*Nx;
            
    // Step 1: Evaluate derivatives of State S
    GetDerivativesBLASV2(S, dSdx, dSdy, coeffs);
            
    // Step 2: Construct banded matrix B
    int kl = 2;
    int ku = 2;
    int ldy = 1 + kl + ku;
    
    for (int i = 0; i < dimS; i+=3){ 
        if (i != 0) {B[(i-1)*ldy] = g;}
        B[i*ldy+ku] = B[(i+1)*ldy+ku] = B[(i+2)*ldy+ku] = S[i];
        if (i!=dimS-1){B[(i+1)*ldy-1] = S[i+2];}
    }
    B[(dimS-1)*ldy] = g;
     
    // Step 3: Evaluate b*d(S)/dx
    cblas_dgbmv(CblasColMajor, CblasNoTrans, dimS, dimS, kl, ku, -1.0, B, ldy, dSdx, 1, 0.0, k, 1);
    
    // Step 4: Construct banded matrix C
    kl = 1;
    ku = 1;
    ldy = 1 + kl + ku;
    
    for (int i = 0; i < dimS; i+=3){
        C[i*ldy+ku] = C[(i+1)*ldy+ku] = C[(i+2)*ldy+ku] = S[i+1];
        if (i != 0) {C[(i-1)*ldy] = g;}
        if (i<dimS-1){C[(i+2)*ldy-1] = S[i+2];}
    } 
    
    // Step 5: Evaluate value of function f(S)
    cblas_dgbmv(CblasColMajor, CblasNoTrans, dimS, dimS, kl, ku, -1.0, C, ldy, dSdy, 1, 1.0, k, 1);
    k[dimS - 2] = - (S[dimS-3+1]* dSdy[dimS-3+1] + g * dSdy[dimS-1]);
}



void ShallowWater::sayHello(){
std::cout << "Hello from class 'ShallowWater'!" << std::endl;
}

void ShallowWater::PrintVector(const int& N, const double* x, const int& inc){
    for (int i = 0; i<N; i+=inc){
        std::cout << x[i] << std::endl;
    }
}

void ShallowWater::PrintMatrix(const int& N, const double* A, const int& lday, const int& inc){ 
    for (int i = 0; i<lday; i+=inc){
        for (int j = 0; j<N; j++) {
            if(std::abs(A[j*lday + (i)]) < 1e-13){
                std::cout << 0 << ",\t";
            }
            else{
                std::cout << A[j*lday + (i)] << ",\t";
            }
        }
    std::cout << "\n" << std::endl;
    }
}

void ShallowWater::ConstructSVector(double* S){
    for (int i = 0; i<3*Nx*Ny; i+=3){
        S[i] =  u[i/3];
        S[i+1] = v[i/3];
        S[i+2] = h[i/3];
    }
}

void ShallowWater::WriteFile(){
    std::ofstream myfile;
    myfile.open("Output.txt");
    for (int iy = 0; iy< Ny; iy++){
        for (int ix = 0; ix<Nx; ix++){
            myfile << ix*dx << "\t" << iy*dy << "\t" << u[iy+ix*Ny] << "\t" << v[iy+ix*Ny] << "\t" << h[iy+ix*Ny] << "\n"; 
        }
    }
    std::cout << "\n\nWriting output to file." << std::endl;
    myfile.close();
    
}

// 'Getter' function definition
double ShallowWater::getTimeStep(){ return dt;}
double ShallowWater::getIntegrationTime(){ return T;}
int ShallowWater::getNx(){return Nx;}
int ShallowWater::getNy(){return Ny;}
int ShallowWater::getIc(){return ic;}
double ShallowWater::getdx(){ return dx;}
double ShallowWater::getdy(){return dy;}
double* ShallowWater::geth(){return h;}
double* ShallowWater::getu(){return u;}
double* ShallowWater::getv(){return v;}