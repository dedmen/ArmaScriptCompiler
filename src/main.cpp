#include <zstd.h>

#include <fstream>
#include <algorithm>
#include <thread>

#include <iostream>

#include <charconv>

#include <stack>
#include "compiledCode.hpp"
#include "scriptCompiler.hpp"
#include "scriptSerializer.hpp"
#include <mutex>
#include <queue>
#include <base64.h>

#include <json.hpp>
#include "luaHandler.hpp"
#include "logger.hpp"

std::queue<std::filesystem::path> tasks;
std::mutex taskMutex;
bool threadsShouldRun = true;
std::filesystem::path outputDir;


// Paths that should be considered the root of a P-drive, even if its not directly a drive letter
// Basically they just get stripped off the front

// rootPathMappings = {};
// inputPath = "P:/test/temp/thing.sqf"
// results in virtualPath = "\\test\\temp\\thing.sqf"

// rootPathMappings = { {"P:/test", "/"} }
// inputPath = "P:/test/temp/thing.sqf"
// results in virtualPath = "\\temp\\thing.sqf"

std::vector<std::pair<std::filesystem::path, std::filesystem::path>> rootPathMappings;

std::filesystem::path ApplyRootPathMapping(const std::filesystem::path& inputPath)
{
    // Performance of this is meh, but I don't care rn

    for (auto& it : rootPathMappings)
    {
        auto deltaPos = std::mismatch(it.first.begin(), it.first.end(), inputPath.begin(), inputPath.end());

        // If the first delta, is the end of our mapping, that means the whole mapping matched
        if (deltaPos.first != it.first.end())
            continue;

        auto resultPath = it.second / inputPath.lexically_relative(it.first);
        return resultPath;
    }


    auto rootDir = inputPath.root_path();
    auto pathRelative = inputPath.lexically_relative(rootDir);

    return pathRelative;
}


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

        if (i->path().extension() == ".sqf"sv) {
            if (i->path().filename() == "fnc_zeusAttributes.sqf") continue; //Hard ignore for missing include file
            //if (i->path().filename() != "fnc_viewdir.sqf") continue;
            //if (i->path().filename() != "test.sqf") continue; //Hard ignore for missing include file
            //if (i->path().string().find("keybinding") == std::string::npos) continue; //CBA trying to format a code piece
            //if (i->path().filename().string().find("XEH_preStart") == std::string::npos) continue; //Hard ignore unit tests
            tasks.emplace(i->path());
        }
    }
}

void processFile(ScriptCompiler& comp, std::filesystem::path path) {
    try {
        auto pathRelative = ApplyRootPathMapping(path);

        auto outputPath = outputDir / pathRelative.parent_path() / (path.stem().string() + ".sqfc");
        
        //if sqfc exists, check if the sqf file has been updated (is newer). if not, skip this sqf file
        if (std::filesystem::exists(outputPath)) { 
            auto sqfcWriteTime = std::filesystem::last_write_time(outputPath);
            auto sqfWriteTime = std::filesystem::last_write_time(path);
            if (sqfWriteTime <= sqfcWriteTime) //sqf file is older than sqfc
                return;
        }

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

void DecompressSQFC(std::filesystem::path inputPath, std::filesystem::path outputPath)
{
    // To use this, also need to edit ScriptSerializer::compiledToBinary and disable compressed serialization

    std::ifstream inputFile(inputPath, std::ifstream::binary);
    auto compiledData = ScriptSerializer::binaryToCompiled(inputFile);



    std::stringstream output(std::stringstream::binary | std::stringstream::out);
    ScriptSerializer::compiledToBinary(compiledData, output);

    auto data = output.str();
    auto encoded = data; //base64_encode(data);
    std::ofstream outputFile(outputPath, std::ofstream::binary);

    outputFile.write(encoded.data(), encoded.length());
    outputFile.flush();
}


int main(int argc, char* argv[]) {

    if (std::filesystem::exists("sqfc.lua")) {
        std::cout << "Using LUA for config" << "\n";

        GLuaHandler.LoadFromFile("sqfc.lua");
        return 0; //#TODO return real error state if any script failed
    }

    if (!std::filesystem::exists("sqfc.json")) {
        std::cout << "Missing sqfc.json in current working directory" << "\n";
        return 1;
    }

    std::ifstream inputFile("sqfc.json");
    auto json = nlohmann::json::parse(inputFile);

    std::vector<std::string> checkConfigKeys = { "excludeList", "inputDirs", "includePaths", "outputDir", "workerThreads"};
    for (const std::string& key : checkConfigKeys) {
        if (!json.contains(key)) {
            std::cout << "Missing \"" << key << "\" in sqfc.json" << "\n";
            return 1;
        }
    }

    std::vector<std::string> excludeList = json["excludeList"].get<std::vector<std::string>>();

    std::transform(excludeList.begin(), excludeList.end(), excludeList.begin(), [](std::string inp) {
        std::transform(inp.begin(), inp.end(), inp.begin(), ::tolower);
        return inp;
    });


    std::vector<std::filesystem::path> inputDirs;
    for (const std::string& inputDir : json["inputDirs"].get<std::vector<std::string>>()) {
        inputDirs.push_back(std::filesystem::path(inputDir));
    }

    std::vector<std::filesystem::path> includePaths;
    for (const std::string& includePath : json["includePaths"].get<std::vector<std::string>>()) {
        includePaths.push_back(std::filesystem::path(includePath));
    }

    outputDir = std::filesystem::path(json["outputDir"].get<std::string>());
    int numberOfWorkerThreads = json["workerThreads"].get<int>();

    if (!json["rootPathMappings"].is_null() && !json["rootPathMappings"].is_array())
    {
        std::cout << "sqfc.json error, rootPathMappings has to be array of arrays";
    }
    else
    {
        for (const auto& includePath : json["rootPathMappings"]) {
            auto x = includePath.get<std::array<std::string, 2>>();
            auto virtualPath = x[1];
            // Strip leading slash if there is one, we add that back later
            if (virtualPath.front() == '/' || virtualPath.front() == '\\')
                virtualPath.erase(0, 1);
            
            rootPathMappings.push_back({std::filesystem::path(x[0]).lexically_normal(), std::filesystem::path(virtualPath).lexically_normal()});
        }
    }

    // Logging


    if (auto loggingConfig = json["logging"]; !loggingConfig.is_null())
    {
        if (auto verboseCfg = loggingConfig["verbose"]; verboseCfg.is_boolean())
            GLogger.m_EnableVerbose = verboseCfg;
    }

 

    // Setup workers

    std::mutex workWait;
    workWait.lock();
    auto workerFunc = [&]() {
        ScriptCompiler compiler(includePaths);
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
    //compileRecursive("T:/x/");
    //compileRecursive("T:/z/ace/");
    //compileRecursive("T:/z/acex/");
    //compileRecursive("T:/a3");

    //compileRecursive("P:/test/");
    for (std::filesystem::path &inputDir : inputDirs) {
        compileRecursive(inputDir);
    }

    workWait.unlock();

    std::vector<std::thread> workerThreads;
    for (int i = 0; i < numberOfWorkerThreads; i++) {
        workerThreads.push_back(std::thread(workerFunc));
    }

    workerFunc();

    for (std::thread &thread : workerThreads) {
        thread.join();
    }

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

    return 0;  //#TODO return real error state if any script failed
}
