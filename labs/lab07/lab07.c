#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/float.h"     // for using single-precision variables.
#include "pico/double.h"    // for using double-precision variables.
#include "pico/multicore.h" // to use multiple cores on the RP2040.

/**
 * @brief This function acts as the main entry-point for core #1.
 *        A function pointer is passed in via the FIFO with one
 *        incoming int32_t used as a parameter. The function will
 *        provide an int32_t return value by pushing it back on 
 *        the FIFO, which also indicates that the result is ready.
 */

void core1_entry() {
    while (1) {
        // 
        int32_t (*func)() = (int32_t(*)()) multicore_fifo_pop_blocking();
        int32_t p = multicore_fifo_pop_blocking();
        int32_t result = (*func)(p);
        multicore_fifo_push_blocking(result);
    }
}

//using float to calculate the number in single precision method for Wallis-approximation
float singlePrecisionPI(int precision){                                           
    float pi = 1;                                                                                         
    float x = 1;                                                                                           
    for(float i = 2; i <= precision; i+=2){                                                                                       
        pi = pi * ( (i*i) / ( x * (x + 2) ) );                                                                                        
        x += 2;                                                                                      
    }
    pi *= 2;
    return pi;
}

//using double to calculate the number in double precision method for Wallis-approximation
double doublePrecisionPI(int precision){                                                                                               
    double pi = 1;           
    double x = 1;      
    for(double i = 2; i <= precision; i+=2){                                      
        pi = pi * ( (i*i) / ( x * (x + 2) ) );                                                                                            
        x += 2;                                                                                           
    }
    pi *= 2;
    return pi;
}


bool get_xip_cache_en(){
    const uint8_t *flash_target_contents = (const uint8_t *) (XIP_CTRL_BASE);         //Get flash memory adress
    
    //Read contents of memory address
    if(flash_target_contents[0] == 1){
        return true;
    }
    return false;
}

bool set_xip_cache_en(bool cache_en){
    bool *a = (bool*) XIP_CTRL_BASE;                                        //Get flash memory address
    *a = cache_en;                                                          //Write cache_en value to XIP_CTRL_BASE EN bit
    return true;
}

bool flush_xip_cache_en(){
    bool *a = (bool*) XIP_CTRL_BASE + 0x04;                                 //Get flash memory flush address
    *a = 1;                                                                 //Write to XIP_CTRL_BASE Flush bit 
    return true;
}


// Main code entry point for core0.
int main() {
    const int    ITER_MAX   = 100000;
    
    stdio_init_all();
    
    set_xip_cache_en(0);                                                     //Setting XIP Status to be 1
    bool xip_stat = get_xip_cache_en();
    printf("Setting XIP Status to be (0 or 1): %d\n",xip_stat); 
    printf("Running time when cache is off:\n");

    int startingTime = time_us_32();                                         //Record the starting stage of single-core starting stage
    int startSinglePrecision = time_us_32();                                 //Take snapshot of the starting stage of timer and store                                                                                                     
    singlePrecisionPI(ITER_MAX);                                             //Run the single-precision Wallis approximation                             
    int endSinglePrecision = time_us_32();                                   //Take snapshot of the ending stage of timer and store                           
    int doublePrecisionSnapshot = time_us_32();                              //Take snapshot of the starting stage of timer and store
    doublePrecisionPI(ITER_MAX);                                             //Run the double-precision Wallis approximation
    int endDoublePrecision = time_us_32();                                   //Take snapshot of the ending stage of timer and store
    int endingTime = time_us_32();                                           //Record the ending stage of single-core starting stage
    
    int singleTime = endingTime - startingTime;
    printf("Running time of single-core: %lld\n",singleTime);
    
    int singlePrecisionTime = endSinglePrecision - startSinglePrecision;
    printf("Running time of single-precision Wallis on single-core: %lld\n",singlePrecisionTime); 
    
    int doublePrecisionTime = endDoublePrecision - doublePrecisionSnapshot;  
    printf("Running time of double-precision Wallis on single-core: %lld\n",doublePrecisionTime); 
      

    // Code for parallel run goes here…
    multicore_launch_core1(core1_entry);

    int startingTimeMulti = time_us_32();                                    //Take a snapshot of multicore starting time and store
    int startSinglePrecisionMulti = time_us_32();                            //Take a snapshot of the first core starting time and store
    multicore_fifo_push_blocking((uintptr_t) &singlePrecisionPI);
    multicore_fifo_push_blocking(ITER_MAX);                                  //Run the single-precision Wallis approximation on one core
    int startDoublePrecisionMulti = time_us_32();                            //Take a snapshot of the second core starting time and store
    doublePrecisionPI(ITER_MAX);                                             //Run the double-precision Wallis approximation on the other core
    int endDoublePrecisionMulti = time_us_32();                              //Take a snapshot of the second core ending time and store
    int endSinglePresicionMulti = time_us_32();                              //Take a snapshot of the first core ending time and store
    int endingTimeMulti = time_us_32();                                      //Take a snapshot of multicore ending time and store

    int multiTime = endingTimeMulti - startingTimeMulti;
    printf("Running time of multicore: %lld\n",multiTime);

    int singlePrecisionTimeMulti = endSinglePresicionMulti - startSinglePrecisionMulti;
    printf("Running time of single-precision Wallis on multicore: %lld\n",singlePrecisionTimeMulti); 

    int doublePrecisionTimeMulti = endDoublePrecisionMulti - startDoublePrecisionMulti;   
    printf("Running time of double-precision Wallis on multicore: %lld\n",doublePrecisionTimeMulti); 
            

    //Enable cache and re-run tests
    set_xip_cache_en(1);                                                      //Setting XIP Status to be 1
    xip_stat = get_xip_cache_en();
    printf("Setting XIP Status to be (0 or 1): %d\n",xip_stat); 
    printf("Running time when cache is on:\n");



    int startingTime1 = time_us_32();                                                               //Record the starting stage of single-core starting stage
    int startSinglePrecision1 = time_us_32();                                                       //Take snapshot of the starting stage of timer and store                                                                                                  
    singlePrecisionPI(ITER_MAX);                                                                    //Run the single-precision Wallis approximation                               
    int endSinglePrecision1 = time_us_32();                                                         //Take snapshot of the ending stage of timer and store                         
    int doublePrecisionSnapshot1 = time_us_32();                                                    //Take snapshot of the starting stage of timer and store
    doublePrecisionPI(ITER_MAX);                                                                    //Run the double-precision Wallis approximation
    int endDoublePrecision1 = time_us_32();                                                         //Take snapshot of the ending stage of timer and store
    int endingTime1 = time_us_32();                                                                 //Record the ending stage of single-core starting stage

    int singleTime1 = endingTime1 - startingTime1;
    printf("Running time of single-core: %lld\n",singleTime1);

    int singlePrecisionTime1 = endSinglePrecision1 - startSinglePrecision1;
    printf("Running time of single-precision Wallis on single-core: %lld\n",singlePrecisionTime1); 

    int doublePrecisionTime1 = endDoublePrecision1 - doublePrecisionSnapshot1;  
    printf("Running time of double-precision Wallis on single-core: %lld\n",doublePrecisionTime1); 
      

    flush_xip_cache_en(); //Flush the cache
    
    // Code for parallel run goes here…
    multicore_reset_core1();
    multicore_launch_core1(core1_entry);

    int startingTimeMulti1 = time_us_32();                                                          //Take a snapshot of multicore starting time and store
    int startSinglePrecisionMulti1 = time_us_32();                                                  //Take a snapshot of the first core starting time and store
    multicore_fifo_push_blocking((uintptr_t) &singlePrecisionPI);
    multicore_fifo_push_blocking(ITER_MAX);                                                        //Run the single-precision Wallis approximation on one core
    int startDoublePrecisionMulti1 = time_us_32();                                                  //Take a snapshot of the second core starting time and store
    doublePrecisionPI(ITER_MAX);                                                                   //Run the double-precision Wallis approximation on the other core
    int endDoublePrecisionMulti1 = time_us_32();                                                    //Take a snapshot of the second core ending time and store
    int endSinglePresicionMulti1 = time_us_32();                                                    //Take a snapshot of the first core ending time and store
    int endingTimeMulti1 = time_us_32();                                                            //Take a snapshot of multicore ending time and store

    int multiTime1 = endingTimeMulti1 - startingTimeMulti1;
    printf("Running time of multicore: %lld\n",multiTime1);

    int singlePrecisionTimeMulti1 = endSinglePresicionMulti1 - startSinglePrecisionMulti1;
    printf("Running time of single-precision Wallis on multicore: %lld\n",singlePrecisionTimeMulti1); 

    int doublePrecisionTimeMulti1 = endDoublePrecisionMulti1 - startDoublePrecisionMulti1;   
    printf("Running time of double-precision Wallis on multicore: %lld\n",doublePrecisionTimeMulti1);      

    return 0;
}