#include "./util.h"
#include "./kernel.h"
#include "./parse.h"

#ifdef USE_GUROBI
#include <gurobi_c++.h>
#endif


/** The main function.
 *  Observations is read from stdin or text file. */
int main(int argc, char* argv[])
{
    using namespace dav;

#ifndef _DEBUG
    try
#endif
    {
        setup(argc, argv);

        if (kernel() != nullptr)
        {
            kernel()->read();
            kernel()->run();
        }
    }
#ifndef _DEBUG
    catch (const exception_t &exception)
    {
        console()->error(exception.what());
        if (exception.do_print_usage())
            console()->print_help();
        abort();
    }
#ifdef USE_GUROBI
    catch (const GRBException &exception)
    {
        console()->error("gurobi-exception : " + exception.getMessage());
        abort();
    }
#endif      
#endif
}
