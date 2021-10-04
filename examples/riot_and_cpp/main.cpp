/*
 * Copyright (C) 2014 Hamburg University of Applied Sciences (HAW)
 * Copyright (C) 2014 Ho Chi Minh University of Technology (HCMUT)
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief       Demonstration of mixed c++ and c user application with pure c RIOT
 *              - mixing of c and c++ source to test name mangling
 *              - introducing a namespace to declarative block, avoiding to qualify calls, e.g. std::vector
 *              - using private and public member functions, e.g. 'cpp_obj.greet()' cannot be accessed from main.cpp
 *              - overloading of function 'cpp_obj.say_hello(...)' for 'none', 'int' or 'float'
 *              - demonstration of templated c++ container 'std::vector'
 *              - usage of iterator to access elements of the container type
 *
 * @author      Martin Landsmann <martin.landsmann@haw-hamburg.de>
 * @author      DangNhat Pham-Huu <51002279@hcmut.edu.vn>
 */

#include "thread.h"

#include "c_functions.h"

#include <cstdio>
#include <vector>
#include "cpp_class.hpp"
#include "opendsme/DSMEPlatform.h"

dsme::DSMEPlatform m_dsme;

using namespace std;

/* thread's stack */
char threadA_stack [THREAD_STACKSIZE_MAIN];

/* thread's function */
void *threadA_func(void *arg);

/* main */
int main()
{
    printf("\n************ RIOT and C++ demo program ***********\n");
    printf("\n");

    m_dsme.initialize();
    m_dsme.start();

    return 0;
}

/* thread A function implementation */
void *threadA_func(void *)
{
    int day = 13, month = 6, year = 2014;
    int ret_day;

    printf("******** Hello, you're in thread #%" PRIkernel_pid " ********\n",
           thread_getpid());
    printf("We'll test some C functions here!\n");

    printf("\n-= hello function =-\n");
    hello();

    printf("\n-= day_of_week function =-\n");

    printf("day %d, month %d, year %d is ", day, month, year);

    ret_day = day_of_week(day, month, year);
    if (ret_day >= 0){
        char day_of_week_table[][32] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        printf("%s\n", day_of_week_table[ret_day]);
    }

    printf("\nThis demo ends here, press Ctrl-C to exit (if you're on native)!\n");

    return NULL;
}
