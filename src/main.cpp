#include <zstd.h>

#include <fstream>
#include <algorithm>

#include <iostream>

#include <charconv>

#include <stack>
#include "compiledCode.hpp"
#include "scriptCompiler.hpp"
#include "scriptSerializer.hpp"


void compileRecursive(ScriptCompiler& comp, std::filesystem::path inputDir) {

    const std::filesystem::path ignoreGit(".git");
    const std::filesystem::path ignoreSvn(".svn");

    //recursively search for pboprefix
    for (auto i = std::filesystem::recursive_directory_iterator(inputDir, std::filesystem::directory_options::follow_directory_symlink);
        i != std::filesystem::recursive_directory_iterator();
        ++i) {
        if (i->is_directory() && (i->path().filename() == ignoreGit || i->path().filename() == ignoreSvn)) {
            i.disable_recursion_pending(); //Don't recurse into that directory
            continue;
        }
        if (!i->is_regular_file()) continue;

        if (i->path().filename().extension() == ".sqf"sv) {

            try {

                auto outputPath = i->path().parent_path() / (i->path().stem().string() + ".sqfc");
                std::cout << "compile " << outputPath.generic_string() << "\n";

                auto compiledData = comp.compileScript(i->path().generic_string());
                std::ofstream output(outputPath);
                ScriptSerializer::compiledToBinaryCompressed(compiledData, output);
                output.clear();
            } catch (std::domain_error&) {

            }
        }
    }
}

int main(int argc, char* argv[]) {


    std::ifstream inputFile("I:/ACE3/addons/advanced_ballistics/functions/fnc_readWeaponDataFromConfig.sqf");

    ScriptCompiler compiler({ static_cast<std::filesystem::path>("I:\\ACE3") });
    compileRecursive(compiler, "I:/ACE3/addons");

    /*
    auto compiledScript = compiler.compileScript("I:/ACE3/addons/advanced_ballistics/functions/fnc_readWeaponDataFromConfig.sqf");

    std::ofstream hr("P:\\human.sqfa");
    ScriptSerializer::compiledToHumanReadable(compiledScript, hr);
    hr.close();

    std::ofstream bin("P:\\binary.sqfc", std::ofstream::binary);
    ScriptSerializer::compiledToBinary(compiledScript, bin);
    bin.close();

    std::ifstream bini("P:\\binary.sqfc", std::ifstream::binary);
    auto compData = ScriptSerializer::binaryToCompiled(bini);
    std::ofstream hr2("P:\\humanpostbin.sqfa");
    ScriptSerializer::compiledToHumanReadable(compData, hr2);
    hr2.close();



    std::ofstream biCn("P:\\binaryCompressed.sqfc", std::ofstream::binary);
    ScriptSerializer::compiledToBinaryCompressed(compiledScript, biCn);
    biCn.close();

    std::ifstream biniC("P:\\binaryCompressed.sqfc", std::ifstream::binary);
    auto compDataC = ScriptSerializer::binaryToCompiledCompressed(biniC);
    std::ofstream hr2C("P:\\humanpostbincompressed.sqfa");
    ScriptSerializer::compiledToHumanReadable(compDataC, hr2C);
    hr2C.close();
    */


    return 1;
}
