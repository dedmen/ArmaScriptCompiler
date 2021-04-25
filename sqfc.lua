local inspect = require 'inspect' -- https://github.com/kikito/inspect.lua
print("Config loading")


function ScanDirectory(directory)
    local dirIterator = DirectoryIterator.new(directory);

    local currentFile = dirIterator:begin();
    local sqfExt = path.new(".sqf")
    local gitExt = path.new(".git")
    local svnExt = path.new(".svn")

    while not currentFile:is_end() do
        local file = currentFile:get();

        if file:is_directory() and (
            file:path():filename() == gitExt
            or file:path():filename() == svnExt
            or string.sub(tostring(file:path():filename()), 0,3) == "map" -- skip all the map layer files on full pdrive
        ) then
            print("skip dir", file)
            currentFile:disable_recursion_pending(); -- Don't recurse into that directory
        end

        if file:is_regular_file() and file:path():filename():extension() == sqfExt then

            -- Here filter our SQF files any way we want

            ASC:AddCompileTask(file:path())
        end

        currentFile:next();
    end
end

-- Called when a compiler is created, usually done once per worker thread
function SetupCompiler(scriptCompiler)
    print("Setup compiler")


    -- init include file paths
    scriptCompiler:InitIncludePaths({path.new("T:/")});

    -- example of how to define a simple macro
    -- #define MY_MACRO_EMPTY
    scriptCompiler:AddMacro(Macro.new("MY_MACRO_EMPTY"));

    -- #define MY_MACRO blabla
    scriptCompiler:AddMacro(Macro.new("MY_MACRO", blabla));
    -- #define MY_MACRO_ARGS(arg1, arg2) arg1##_##arg2
    scriptCompiler:AddMacro(Macro.new("MY_MACRO_ARGS", {"arg1", "arg2"}, "arg1##_##arg2"));
    -- custom handler function, returns the final macro replacement text
    scriptCompiler:AddMacro(Macro.new("MY_MACRO_CUSTOM", 
        function(diagInfo)
            return "testy"
        end
    ));
    -- custom handler function, returns the final macro replacement text, with parameters
    -- In this example equivalent to 
    -- #define MY_MACRO_CUSTOM_ARGS(arg1,arg2) arg1 is at __LINE__ with arg2
    scriptCompiler:AddMacro(Macro.new("MY_MACRO_CUSTOM_ARGS", {"arg1", "arg2"}, 
        function(parameters, diagInfo)
            return parameters[1] .. " is at " ..  diagInfo.line .. " with " .. parameters[2]
        end
    ));
    -- custom pragma handler
    -- #pragma myPragma ...
    scriptCompiler:AddPragma(Pragma.new("myPragma", 
    function(data, diagInfo)
        return ""
    end
));


end

--print("ASC meta:", inspect(getmetatable(ASC)))
--ScanDirectory("P:/test");
--ASC:SetOutputDir(path.new("P:/"))
--ASC:RunCompileTasks(1)
print("Config loaded")


local compiler = ScriptCompiler.new()

-- custom handler function, returns the final macro replacement text, with parameters
-- In this example equivalent to 
-- #define MY_MACRO_CUSTOM_ARGS(arg1,arg2) arg1 is at __LINE__ with arg2
compiler:AddMacro(Macro.new("MY_MACRO_CUSTOM_ARGS", {"arg1", "arg2"}, 
    function(parameters, diagInfo)
        --print("parameters:", inspect(parameters))
        --print("parameters:", inspect(getmetatable(parameters)))
        --print("diagInfo:", inspect(getmetatable(diagInfo)))
        return parameters[1] .. " is at " ..  diagInfo.line .. " with " .. parameters[2]
    end
));

print("Preprocessed Script: \n", compiler:PreprocessFile(path.new("P:/test/test.sqf")));



