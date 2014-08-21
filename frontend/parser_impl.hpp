// Generated by Bisonc++ V4.05.00 on Sun, 25 May 2014 14:03:00 -0700

    // Include this file in the sources of the class Parser.

// $insert class.h
#include "parser.h"

// $insert namespace-open
namespace stream
{

inline void Parser::error(char const *msg)
{
    std::cerr << "ERROR [line " << d_scanner.lineNr() << "]: "
              << msg << std::endl;;
}

// $insert lex
inline int Parser::lex()
{
    return d_scanner.lex();
}

inline void Parser::print()         
{
    if (d_print_tokens)
        print__();           // displays tokens if --print was specified
}

inline void Parser::exceptionHandler__(std::exception const &exc)         
{
    throw;              // re-implement to handle exceptions thrown by actions
}

// $insert namespace-close
}

    // Add here includes that are only required for the compilation 
    // of Parser's sources.


// $insert namespace-use
    // UN-comment the next using-declaration if you want to use
    // symbols from the namespace stream without specifying stream::
//using namespace stream;

    // UN-comment the next using-declaration if you want to use
    // int Parser's sources symbols from the namespace std without
    // specifying std::

//using namespace std;
