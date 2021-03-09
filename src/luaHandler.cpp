#include "luaHandler.hpp"

#include <queue>
#include <thread>



#include "scriptCompiler.hpp"

#include "sol/sol.hpp"
extern std::queue<std::filesystem::path> tasks;
extern void processFile(ScriptCompiler& comp, std::filesystem::path path);
extern std::filesystem::path outputDir;





class DirectoryIterator {

    std::filesystem::path directory;
public:

    typedef std::filesystem::recursive_directory_iterator iterator;
    typedef typename std::filesystem::recursive_directory_iterator::value_type value_type;


    DirectoryIterator(std::string path) {
        directory = path;
    }
    std::filesystem::recursive_directory_iterator begin() const {
        return std::filesystem::recursive_directory_iterator(directory, std::filesystem::directory_options::follow_directory_symlink);
    }

    std::filesystem::recursive_directory_iterator end() const {
        return std::filesystem::recursive_directory_iterator();
    }
};


namespace sol {
    template <>
    struct is_container<DirectoryIterator> : std::true_type {};
    template <>
    struct is_container<std::filesystem::path> : std::false_type {};

    template <>
    struct is_automagical<std::filesystem::path> : std::false_type {};

    template <>
    struct is_automagical<OptimizerModuleLua> : std::false_type {};

    template <>
    struct is_automagical<OptimizerModuleBase::Node> : std::false_type {};
}


struct LuaASC {

    void AddCompileTask(std::filesystem::path path) {
        tasks.emplace(path);
    }

    void SetOutputDir(std::filesystem::path path) {
        outputDir = path;
    }

    auto IterateDirectory(std::string path) {
        return sol::as_container(DirectoryIterator(path));
    }

    void RunCompileTasks(int numberOfWorkerThreads) {
        std::mutex taskMutex;
        bool threadsShouldRun = true;

        auto workerFunc = [&]() {
            ScriptCompiler compiler;
            GLuaHandler.SetupCompiler(compiler);

            while (threadsShouldRun) {
                std::unique_lock<std::mutex> lock(taskMutex);
                if (tasks.empty()) return;
                const auto task(std::move(tasks.front()));
                tasks.pop();
                if (tasks.empty())
                    threadsShouldRun = false;
                lock.unlock();


                //auto foundExclude = std::find_if(excludeList.begin(), excludeList.end(), [&task](const std::string& excludeItem)
                //    {
                //        auto taskString = task.string();
                //        std::transform(taskString.begin(), taskString.end(), taskString.begin(), ::tolower);
                //        return taskString.find(excludeItem) != std::string::npos;
                //    });
                //
                //if (foundExclude == excludeList.end())
                processFile(compiler, task);
            }

        };
        std::vector<std::thread> workerThreads;
        for (int i = 0; i < numberOfWorkerThreads-1; i++) {
            workerThreads.push_back(std::thread(workerFunc));
        }

        workerFunc();

        for (std::thread& thread : workerThreads) {
            thread.join();
        }
    }
};



LuaHandler::LuaHandler() {

    lua.open_libraries(
        // print, assert, and other base functions
        sol::lib::base,
        // require and other package functions
        sol::lib::package,
        // coroutine functions and utilities
        sol::lib::coroutine,
        // string library
        sol::lib::string,
        // functionality from the OS
        sol::lib::os,
        // all things math
        sol::lib::math,
        // the table manipulator and observer functions
        sol::lib::table,
        // the debug library
        sol::lib::debug,
        // the bit library: different based on which you're using
        sol::lib::bit32,
        // input/output library
        sol::lib::io,
        // library for handling utf8: new to Lua
        sol::lib::utf8);


    auto DirIterType = lua.new_usertype<DirectoryIterator>(
        "DirectoryIterator", sol::constructors<DirectoryIterator(std::string)>(),
        "begin", &DirectoryIterator::begin
        );

    lua.new_usertype<std::filesystem::recursive_directory_iterator>(
        "recursive_directory_iterator", sol::no_constructor,
        "disable_recursion_pending", &std::filesystem::recursive_directory_iterator::disable_recursion_pending,
        "next", [](std::filesystem::recursive_directory_iterator& iter)
        {
            ++iter;
        },
        "is_end", [](std::filesystem::recursive_directory_iterator& iter)
        {
            return iter == std::filesystem::recursive_directory_iterator();
        },
            "get", &std::filesystem::recursive_directory_iterator::operator*,
            "depth", &std::filesystem::recursive_directory_iterator::depth,
            "options", &std::filesystem::recursive_directory_iterator::options


            );

    lua.new_usertype<std::filesystem::directory_entry>(
        "directory_entry", sol::no_constructor,
        "is_block_file", static_cast<bool(std::filesystem::directory_entry::*)() const>(&std::filesystem::directory_entry::is_block_file),
        "is_directory", static_cast<bool(std::filesystem::directory_entry::*)() const>(&std::filesystem::directory_entry::is_directory),
        "is_regular_file", static_cast<bool(std::filesystem::directory_entry::*)() const>(&std::filesystem::directory_entry::is_regular_file),
        "is_character_file", static_cast<bool(std::filesystem::directory_entry::*)() const>(&std::filesystem::directory_entry::is_character_file),
        "is_symlink", static_cast<bool(std::filesystem::directory_entry::*)() const>(&std::filesystem::directory_entry::is_symlink),
        "last_write_time", static_cast<std::filesystem::file_time_type(std::filesystem::directory_entry::*)() const>(&std::filesystem::directory_entry::last_write_time),
        "path", &std::filesystem::directory_entry::path
        );

    sol::automagic_enrollments enrollments;

    enrollments.pairs_operator = false;
    //{
    //    false, false, false, true, true, true, true, true, true
    //};

    auto pathType = lua.new_usertype<std::filesystem::path>(
        "path", sol::constructors<std::filesystem::path(), std::filesystem::path(const std::string&)>(),
        "generic_string", [](const std::filesystem::path& path) -> std::string
        {
            return path.generic_string();
        },
        "__tostring", static_cast<std::string(std::filesystem::path::*)() const>(&std::filesystem::path::generic_string),
        "__eq", &sol::detail::comparsion_operator_wrap<std::filesystem::path, std::equal_to<>>,
        "__le", &sol::detail::comparsion_operator_wrap<std::filesystem::path, std::less_equal<>>,
        "__lt", &sol::detail::comparsion_operator_wrap<std::filesystem::path, std::less<>>,

        //"__eq", sol::overload([](const std::filesystem::path& l, const std::filesystem::path& r)
        //{
        //    return l == r;
        //}, [](const std::filesystem::path& l, const std::string& r)
        //{
        //    return l == r;
        //}),
        "filename", &std::filesystem::path::filename,
        "extension", &std::filesystem::path::extension,
        "lexically_normal", &std::filesystem::path::lexically_normal,
        "parent_path", &std::filesystem::path::parent_path,
        "root_directory", &std::filesystem::path::root_directory,
        "root_name", &std::filesystem::path::root_name,
        "root_path", &std::filesystem::path::root_path
    );

    lua.new_usertype<LuaASC>(
        "ASC", sol::default_constructor,

        "AddCompileTask", &LuaASC::AddCompileTask,
        "IterateDirectory", &LuaASC::IterateDirectory,
        "RunCompileTasks", &LuaASC::RunCompileTasks,
        "SetOutputDir", &LuaASC::SetOutputDir 
    );
    

    lua.new_usertype<ScriptCompiler>(
        "ScriptCompiler", sol::no_constructor,
        "new", []()
        {
            ScriptCompiler compiler;
            GLuaHandler.SetupCompiler(compiler);
            return compiler;
        },

        "InitIncludePaths", [](ScriptCompiler& comp, sol::table paths)
        {
            std::vector<std::filesystem::path> paths2;

            for (const auto& it : paths) {
                auto t3 = it.second.as<std::filesystem::path>();
                paths2.emplace_back(t3);
            }

           comp.initIncludePaths(paths2);
        },
        "AddMacro", & ScriptCompiler::addMacro,
        "AddPragma", & ScriptCompiler::addPragma,
        // #TODO ability to register SQF commands

        "PreprocessFile", [](ScriptCompiler& comp, const std::filesystem::path& path)
        {
            auto rootDir = path.root_path();
            auto pathRelative = path.lexically_relative(rootDir);

            auto result = comp.preprocessScript(path.generic_string(), ("\\" / pathRelative).generic_string());
            return result;
        },

        "CompileScriptToFile", [](ScriptCompiler& comp, const std::filesystem::path& path, const std::filesystem::path& outputFile, OptimizerModuleLua& luaOptimizer) {
            auto rootDir = path.root_path();
            auto pathRelative = path.lexically_relative(rootDir);

            comp.compileScriptLua(path.generic_string(), ("\\" / pathRelative).generic_string(), luaOptimizer, outputFile);
        }
    );

    lua.new_usertype<OptimizerModuleLua>(
        "OptimizerModuleLua", sol::no_constructor,
        "new", [](sol::protected_function func) {
            OptimizerModuleLua opt;
            opt.nodeHandler = func;
            return opt;
        }
    );


    lua.new_usertype<sqf::runtime::parser::macro>(
        "Macro", sol::no_constructor,
        "new", 
        sol::overload(
            [](std::string name)
            {
                return sqf::runtime::parser::macro(std::move(name));
            },
            [](std::string name, std::string content)
            {
                return sqf::runtime::parser::macro(std::move(name), std::move(content));
            },
            [](std::string name, sol::table args, std::string content)
            {
                std::vector<std::string> args2;
                for (const auto& it : args) {
                    auto t3 = it.second.as<std::string>();
                    args2.emplace_back(t3);
                }

                return sqf::runtime::parser::macro(std::move(name), std::move(args2), std::move(content));
            },
            [](std::string name, sol::protected_function handler)
            {
                return sqf::runtime::parser::macro(std::move(name), [handler](
                    const sqf::runtime::parser::macro& m,
                    const ::sqf::runtime::diagnostics::diag_info dinf,
                    const ::sqf::runtime::fileio::pathinfo location,
                    const std::vector<std::string>& params,
                    ::sqf::runtime::runtime& runtime) -> std::string
                {
                        return handler(dinf);
                });
            },
            [](std::string name, sol::table args, sol::protected_function handler)
            {
                std::vector<std::string> args2;
                for (const auto& it : args) {
                    auto t3 = it.second.as<std::string>();
                    args2.emplace_back(t3);
                }

                return sqf::runtime::parser::macro(std::move(name), std::move(args2), [handler](
                    const sqf::runtime::parser::macro& m,
                    const ::sqf::runtime::diagnostics::diag_info dinf,
                    const ::sqf::runtime::fileio::pathinfo location,
                    const std::vector<std::string>& params,
                    ::sqf::runtime::runtime& runtime) -> std::string
                    {
                        return handler(sol::as_table(params), dinf);
                    });
            }
        )
    );

    lua.new_usertype<sqf::runtime::parser::pragma>(
        "Pragma", sol::no_constructor,
        "new",
            [](std::string name, sol::protected_function handler)
            {
                return sqf::runtime::parser::pragma(std::move(name), [handler](
                    const sqf::runtime::parser::pragma& m,
                    ::sqf::runtime::runtime& runtime,
                    const ::sqf::runtime::diagnostics::diag_info dinf,
                    const ::sqf::runtime::fileio::pathinfo location,
                    const std::string& data) -> std::string
                    {
                        return handler(data, dinf);
                    });
            }
                
        );


    lua.new_usertype<sqf::runtime::fileio::pathinfo>(
        "PathInfo", sol::no_constructor,
        "additional", sol::readonly(&sqf::runtime::fileio::pathinfo::additional),
        "physical", sol::readonly(&sqf::runtime::fileio::pathinfo::physical),
        "virtual", sol::readonly(&sqf::runtime::fileio::pathinfo::virtual_)
    );

    lua.new_usertype<sqf::runtime::diagnostics::diag_info>(
        "DiagInfo", sol::no_constructor,
        "path", sol::readonly(&sqf::runtime::diagnostics::diag_info::path),
        "length", sol::readonly(&sqf::runtime::diagnostics::diag_info::length),
        "adjusted_offset", sol::readonly(&sqf::runtime::diagnostics::diag_info::adjusted_offset),
        "code_segment", sol::readonly(&sqf::runtime::diagnostics::diag_info::code_segment),
        "line", sol::readonly(&sqf::runtime::diagnostics::diag_info::line),
        "column", sol::readonly(&sqf::runtime::diagnostics::diag_info::column),
        "file_offset", sol::readonly(&sqf::runtime::diagnostics::diag_info::file_offset)
    );

    lua.new_usertype<OptimizerModuleBase::Node>(
        "OptimizerNode", sol::no_constructor,
        "type", &OptimizerModuleBase::Node::type,
        "file", &OptimizerModuleBase::Node::file,
        "line", &OptimizerModuleBase::Node::line,
        "offset", &OptimizerModuleBase::Node::offset,
        "children", &OptimizerModuleBase::Node::children,
        "constant", &OptimizerModuleBase::Node::constant,
        "value", &OptimizerModuleBase::Node::value,
        "areChildrenConstant", &OptimizerModuleBase::Node::areChildrenConstant
    );

    lua.new_enum("InstructionType",
        "endStatement", InstructionType::endStatement,
        "push", InstructionType::push,
        "callUnary", InstructionType::callUnary,
        "callBinary", InstructionType::callBinary,
        "callNular", InstructionType::callNular,
        "assignTo", InstructionType::assignTo,
        "assignToLocal", InstructionType::assignToLocal,
        "getVariable", InstructionType::getVariable,
        "makeArray" , InstructionType::makeArray
    );

    lua.new_usertype<ScriptConstantArray>(
        "ScriptConstantArray", sol::default_constructor
    );
    

    lua["ASC"] = LuaASC{};
}

void LuaHandler::LoadFromFile(std::filesystem::path filePath) {
    isActive = true;

    lua.safe_script_file(filePath.string());
}


void LuaHandler::SetupCompiler(ScriptCompiler& compiler) {
    if (!isActive)
        return;
    std::lock_guard accessGuard(luaAccess);
    if (!lua.get<sol::function>("SetupCompiler"))
        return;


    lua.get<sol::function>("SetupCompiler")(compiler);

}
