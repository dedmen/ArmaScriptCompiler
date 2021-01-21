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
            //if (i->path().filename() != "test.sqf") continue; //Hard ignore for missing include file
            //if (i->path().string().find("keybinding") == std::string::npos) continue; //CBA trying to format a code piece
            //if (i->path().filename().string().find("XEH_preStart") == std::string::npos) continue; //Hard ignore unit tests
            tasks.emplace(i->path());
        }
    }
}

void processFile(ScriptCompiler& comp, std::filesystem::path path) {
    try {
        auto rootDir = path.root_path();
        auto pathRelative = path.lexically_relative(rootDir);


        auto outputPath = "P:/" / pathRelative.parent_path() / (path.stem().string() + ".sqfc");

        if (std::filesystem::exists(outputPath)) return;


        std::error_code ec;
        std::filesystem::create_directories(outputPath.parent_path(), ec);
        std::cout << "compile " << outputPath.generic_string() << "\n";

        auto compiledData = comp.compileScript(path.generic_string(), ("\\" / pathRelative).generic_string());

        if (compiledData.constants.empty()) return; // no code or failed to compile
        std::stringstream output(std::stringstream::binary | std::stringstream::out);
        //ScriptSerializer::compiledToBinaryCompressed(compiledData, output);
        ScriptSerializer::compiledToBinary(compiledData, output);

        auto data = output.str();
        auto encoded = data; //base64_encode(data);

        std::ofstream outputFile(outputPath, std::ofstream::binary);

        outputFile.write(encoded.data(), encoded.length());

        //ScriptSerializer::compiledToBinary(compiledData, output);
        outputFile.flush();
        //std::istringstream data2(data, std::istringstream::binary);
        //auto res = ScriptSerializer::binaryToCompiledCompressed(data2);



        //auto outputPath2 = path.parent_path() / (path.stem().string() + ".sqfa");
        //std::ofstream output2(outputPath2, std::ofstream::binary);
        //ScriptSerializer::compiledToHumanReadable(compiledData, output2);
        //output2.flush();
    } catch (std::domain_error& err) {

    }
    catch (std::runtime_error& err) {

    }


}

int main(int argc, char* argv[]) {


    //std::ifstream inputFile("I:/ACE3/addons/advanced_ballistics/functions/fnc_readWeaponDataFromConfig.sqf");

    //ScriptCompiler compiler({ 
    //    //static_cast<std::filesystem::path>("I:\\ACE3"),
    //    //static_cast<std::filesystem::path>("I:\\CBA_A3"),
    //    //static_cast<std::filesystem::path>("F:/Steam/SteamApps/common/Arma 3/Addons/ui_f"),
    //    //static_cast<std::filesystem::path>("F:/Steam/SteamApps/common/Arma 3/Addons/functions_f"),
    //    //static_cast<std::filesystem::path>("F:/Steam/SteamApps/common/Arma 3/Addons/editor_f")
    //    static_cast<std::filesystem::path>("T:/")
    //});



    // if file path contains this, skip it
    std::vector<std::string> excludeList;
    excludeList.push_back("missions_f_contact\\sites");
    excludeList.push_back("missions_f_epa");
    excludeList.push_back("missions_f_oldman");
    excludeList.push_back("missions_f_tank");
    excludeList.push_back("missions_f_beta");

    std::mutex workWait;
    workWait.lock();
    auto workerFunc = [&]() {
        ScriptCompiler compiler({ 
            static_cast<std::filesystem::path>("T:/")
        });
        workWait.lock();
        workWait.unlock();

        while (threadsShouldRun) {
            std::unique_lock<std::mutex> lock(taskMutex);
            if (tasks.empty()) return;
            const auto task(std::move(tasks.front()));
            tasks.pop();
            if (tasks.empty())
                threadsShouldRun = false;
            lock.unlock();


            auto foundExclude = std::find_if(excludeList.begin(), excludeList.end(), [&task](const std::string& excludeItem)
            {
                auto taskString = task.string();
                std::transform(taskString.begin(), taskString.end(), taskString.begin(), ::tolower);
                return taskString.find(excludeItem) != std::string::npos;
            });

            if (foundExclude == excludeList.end())
                processFile(compiler, task);
        }

    };

    //compileRecursive("I:/ACE3/addons");
    //compileRecursive("I:/CBA_A3/addons");
    compileRecursive("T:/x/");
    compileRecursive("T:/z/ace/");
    compileRecursive("T:/z/acex/");
    compileRecursive("T:/a3/");
    //compileRecursive("P:/test/");

    
    //compileRecursive("I:/CBA_A3/addons");
    //compileRecursive("I:/ACE3/addons/nightvision");
    //compileRecursive("I:/ACE3/addons/aircraft");
    workWait.unlock();




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
