#include "wasm/interpreter.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    try
    {
        const std::string wasm_path =
            (argc > 1) ? argv[1] : "build/generated_wasm/09_print_hello.wasm";

        auto wasm_bytes = wasm::read_file(wasm_path);

        wasm::Interpreter interpreter;
        interpreter.load(wasm_bytes);

        auto result = interpreter.invoke("_start");
        if (result.trapped)
        {
            std::cerr << "execution trapped: " << result.trap_message << '\n';
            return EXIT_FAILURE;
        }

        if (!result.values.empty())
        {
            int size = result.values.size();
            std::cout << "returned " << size << " value(s)\n";
            //for (int i = 0; i<size; ++i){
            //    std::cout << "Value " << i << ": " << result.values[i] << std::endl;
            //}
        }

        return EXIT_SUCCESS;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
