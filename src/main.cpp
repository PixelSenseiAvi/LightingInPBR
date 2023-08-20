#include <cstdio>
#include <string>
#include <memory>

#include "application.hpp"

#include "openglUtility.hpp"


int main(int argc, char* argv[])
{
    RendererInterface* renderer = new Renderer;

    try {
        Application().run(std::unique_ptr<RendererInterface>{renderer});
    }
    catch(const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}
