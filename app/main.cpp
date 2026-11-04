#include "App.h"
#include "pipeline/Pipeline.h"
int main(int argc, char** argv) {
    return coreci::app::run(argc, argv, coreci::productionPipeline());
}
