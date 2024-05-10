#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/float.h"			// Required for using single-precision variables.
#include "pico/double.h"		// Required for using double-precision variables.

#define MAX_ITR 100000
#define BASIS_PI 3.14159265359

// Function for calculating the value of PI in single-precision floating-point representation
float singlePrecision (int n){
  float ans = 1.0;
  for (int i = 1; i <= MAX_ITR; i++){
    ans *= (4.0 * i * i) / ((4.0 * i * i) - 1);
  }
  return ans;
}

// Function for calculating the value of PI in double-precision floating-point representation
double doublePrecision (int n){
  double ans = 1.0;
  for (int i = 1; i <= MAX_ITR; i++){
    ans *= (4.0 * i * i) / ((4.0 * i * i) - 1);
  }
  return ans;
}

int main (){
  #ifndef WOKWI
  // Initialise the IO as we will be using the UART
  // Only required for hardware and not needed for Wokwi
  stdio_init_all ();
  #endif

  //Results for the first function i.e. single-precision value of PI and its approx error.
  float ans1 = singlePrecision(1);
  float error1 = BASIS_PI - ans1 * 2;
  printf ("Single precition value: %f\n", (ans1 * 2));
  printf ("Single precision approx error: %f\n\n", error1);
  
  //Results for the second function i.e. double-precision value of PI and its approx error.
  double ans2 =  doublePrecision(1);
  double error2 = BASIS_PI - ans2 * 2;
  printf ("Double precision value: %lf\n", ans2 * 2);
  printf ("Double precision approx error: %lf\n\n", error2);
  
  // Returning zero indicates everything went okay.
  return 0;
}