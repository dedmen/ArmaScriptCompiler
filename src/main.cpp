#include <zstd.h>

#include <fstream>
#include <algorithm>

#include <iostream>

#include <charconv>

#include <stack>
#include "compiledCode.hpp"
#include "scriptCompiler.hpp"
#include "scriptSerializer.hpp"
#include <mutex>
#include <queue>
#include <base64.h>

std::queue<std::filesystem::path> tasks;
std::mutex taskMutex;
bool threadsShouldRun = true;

void compileRecursive(std::filesystem::path inputDir) {

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
            if (i->path().filename() == "fnc_zeusAttributes.sqf") continue; //Hard ignore for missing include file
            tasks.emplace(i->path());
        }
    }
}

void processFile(ScriptCompiler& comp, std::filesystem::path path) {
    try {
        auto outputPath = path.parent_path() / (path.stem().string() + ".sqfc");
        std::cout << "compile " << outputPath.generic_string() << "\n";

        auto compiledData = comp.compileScript(path.generic_string());
        std::stringstream output;
        ScriptSerializer::compiledToBinaryCompressed(compiledData, output);

        auto data = output.str();
        auto encoded = base64_encode(data);

        //std::ofstream outputFile(path);

        //outputFile.write(encoded.data(), encoded.length());

        //ScriptSerializer::compiledToBinary(compiledData, output);
        //outputFile.flush();
        //auto outputPath2 = path.parent_path() / (path.stem().string() + ".sqfa");
        //std::ofstream output2(outputPath2);
        //ScriptSerializer::compiledToHumanReadable(compiledData, output2);
        //output2.flush();
    } catch (std::domain_error& err) {

    }
}

int main(int argc, char* argv[]) {


    std::ifstream inputFile("I:/ACE3/addons/advanced_ballistics/functions/fnc_readWeaponDataFromConfig.sqf");

    ScriptCompiler compiler({ static_cast<std::filesystem::path>("I:\\ACE3") });
    compileRecursive("I:/ACE3/addons");
    //compileRecursive("I:/ACE3/addons/nightvision");
    //compileRecursive("I:/ACE3/addons/aircraft");

    auto workerFunc = []() {
        ScriptCompiler compiler({ static_cast<std::filesystem::path>("I:\\ACE3") });


        while (threadsShouldRun) {
            std::unique_lock<std::mutex> lock(taskMutex);
            const auto task(std::move(tasks.front()));
            tasks.pop();
            if (tasks.empty())
                threadsShouldRun = false;
            lock.unlock();
            processFile(compiler, task);
        }

    };

    std::unique_ptr<std::thread> myThread = std::make_unique<std::thread>(workerFunc);
    std::unique_ptr<std::thread> myThread2 = std::make_unique<std::thread>(workerFunc);
    std::unique_ptr<std::thread> myThread3 = std::make_unique<std::thread>(workerFunc);
    std::unique_ptr<std::thread> myThread4 = std::make_unique<std::thread>(workerFunc);
    std::unique_ptr<std::thread> myThread5 = std::make_unique<std::thread>(workerFunc);
    std::unique_ptr<std::thread> myThread6 = std::make_unique<std::thread>(workerFunc);
    std::unique_ptr<std::thread> myThread7 = std::make_unique<std::thread>(workerFunc);

    workerFunc();

    myThread->join();
    myThread2->join();
    myThread3->join();
    myThread4->join();
    myThread5->join();
    myThread6->join();
    myThread7->join();

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
