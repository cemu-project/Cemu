#include <stdio.h>
#include "./../src/Common/version.h"

// output current Cemu version for CI workflow. Do not modify
int main()
{
   printf("%d.%d", EMULATOR_VERSION_LEAD, EMULATOR_VERSION_MAJOR);
   return 0;
}
