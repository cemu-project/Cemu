#include "Cafe/HW/Latte/Renderer/Metal/MetalQuery.h"

bool LatteQueryObjectMtl::getResult(uint64& numSamplesPassed)
{
    cemuLog_log(LogType::MetalLogging, "LatteQueryObjectMtl::getResult: occlusion queries are not yet supported on Metal");
    return true;
}

void LatteQueryObjectMtl::begin()
{
    cemuLog_log(LogType::MetalLogging, "LatteQueryObjectMtl::begin: occlusion queries are not yet supported on Metal");
}

void LatteQueryObjectMtl::end()
{
    cemuLog_log(LogType::MetalLogging, "LatteQueryObjectMtl::end: occlusion queries are not yet supported on Metal");
}
