/*
 * @file GeneratorRunner.cpp
 * @brief TODO
 */

#include <GeneratorRunner.hpp>

using llvm::orc::ThreadSafeModule;

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/**
 * @brief TODO
 * @param pathGenerator Path to the generator to be invoked
 * @return std::vector<std::string>
 */
std::vector<QuantumTask> invokeGenerator(const QuantumTask &parentQuantumTask,
                                         const std::string &nameGenerator)
{
    std::string pathGenerator;
    char buffer[PATH_MAX];

    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1)
    {
        buffer[len] = '\0';
        pathGenerator = std::string(buffer);
        size_t lastSlash = pathGenerator.find_last_of("/\\");
        pathGenerator = pathGenerator.substr(0, lastSlash) + "/lib/";
    }
    pathGenerator.append(nameGenerator);

    std::cout << "   [Generator Runner]....Invoking generator: "
              << nameGenerator << std::endl;

    // Load the generator as a shared library
    pathGenerator = "/home/ubuntu/mqss/qrm.git/build/src/generator_runner/"
                    "generators/libgenerator_cutter.so";
    void *lib_handle = dlopen(pathGenerator.c_str(), RTLD_LAZY);

    if (!lib_handle)
    {
        std::cerr << "   [Generator Runner]....Error loading generator as a "
                     "shared library: "
                  << dlerror() << std::endl;

        return {};
    }

    // Dynamic loading and linking of the shared library
    typedef std::vector<QuantumTask> (*GeneratorFunction)(const QuantumTask &);

    GeneratorFunction generator =
        reinterpret_cast<GeneratorFunction>(dlsym(lib_handle, "generator"));

    if (!generator)
    {
        std::cerr
            << "   [Generator Runner]....Error finding function in shared "
               "library: "
            << dlerror() << std::endl;

        dlclose(lib_handle);
        return {};
    }

    // Call the generator function
    return generator(parentQuantumTask);
}
