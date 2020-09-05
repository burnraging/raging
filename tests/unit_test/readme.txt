Adding a new Test:

Create a test file (c source file )
    * in this file add your function that will call all the tests, something like:
        void ut_run_all_my_tests(void);
Open test_list.h and add the prototype for that function
Open test_main.c and add a call to that function
Open the makefile and add a line to compile your testfile.c

Begin writing tests.
