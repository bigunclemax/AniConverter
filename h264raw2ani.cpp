#include "cxxopts/include/cxxopts.hpp"
#include "aniconverter.h"

void parse(int argc, const char* argv[])
{
    try
    {
        cxxopts::Options options(argv[0], " - h264 bitstream to ani files converter");
        options
                .positional_help("[optional args]")
                .show_positional_help();

        options
                .set_width(70)
                .set_tab_expansion()
                .allow_unrecognised_options()
                .add_options()
                        ("i,input", "Input", cxxopts::value<std::string>(), "raw h264 file")
                        ("o,output", "Output file", cxxopts::value<std::string>()
                                ->default_value("out.ani"), "ANI file")
                        ("help", "Print help");

        options.parse_positional({"input", "output"});

        auto result = options.parse(argc, argv);

        if (result.count("help"))
        {
            std::cout << options.help() << std::endl;
            exit(0);
        }

        if (!result.count("input"))
        {
            std::cout << "Specify input file" << std::endl;
            std::cout << options.help() << std::endl;
            exit(0);
        }

        const auto &input = result["input"].as<std::string>();
        const auto &output = result["output"].as<std::string>();

        ani::pack_ani(input, output);

    }
    catch (const cxxopts::OptionException& e)
    {
        std::cout << "error parsing options: " << e.what() << std::endl;
        exit(1);
    }
}

int main(int argc, const char *argv[]) {
    parse(argc, argv);
    return 0;
}
