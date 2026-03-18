#include "types.h"
#include "stat.h"
#include "user.h"

int 
main(int argc, char * argv[])
{
    printf(1, "\nBYEBYE XV6~\n\n");
    shutdown();

    exit(); //return 0;
}