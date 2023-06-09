#include <iostream>
#include <cmath>

#ifndef SHALLOWWATER_H
#define SHALLOWWATER_H

class ShallowWater
{
    // Default initialisation
    double dt = 0.1;
    double T = 25.1;
    int Nx = 100;
    int Ny = 100;
    int ic = 1;
    double dx = 1.;
    double dy = 1.; 
    int analysis = 1;
    \
    double* h = nullptr;
    double* u = nullptr;
    double* v = nullptr;
    
    void ConstructSVector(double* S);
    void GetDerivativesBLASV2(const double* S, double* dSdx, double* dSdy, const double* coeffs);
    void GetDerivativesParallel(const int& rows, const int& cols, const double* varx, const double* vary,  double* dvardx, double* dvardy, const double* coeffs);
    void EvaluateFuncBlasV3(const int& kla, const int& kua, const int& lday, double* S, const int& ldsy, const double* coeffs, double* k, double* dSdx, double* dSdy, double* B, double* C);
    
    
public:
    // Constructors
    ShallowWater(); // Default Constructorn declaration
    ShallowWater(double dtt, double Tt, int Nxx, int Nyy, int icc, double dxx, double dyy, int typeAnalysis); // Constructor declaration
    
    // Methods
    void sayHello();
    void SetInitialCondition();
    void PrintMatrix(const int& N,  const double* A, const int& lda, const int& inc);
    void PrintVector(const int& N, const double* x, const int& inc);
    void TimeIntegrateBLAS();
    void TimeIntegrate();
    void WriteFile();
    
    // 'Getter' functions
    double getTimeStep();
    double getIntegrationTime();
    int getNx();
    int getNy();
    int getIc();
    double getdx();
    double getdy();
    double* geth();
    double* getu();
    double* getv();
    
    ~ShallowWater(); // Destructor
    
};

#endif
