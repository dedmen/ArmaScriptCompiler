---@diagnostic disable: lowercase-global
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

-- print("Preprocessed Script: \n", compiler:PreprocessFile(path.new("P:\\test.sqf")));


function optimizerNodeHandler(node)


    print("node meta:", inspect(getmetatable(node)))
    print("node:", inspect(node))
    print("nodef:", node.file)
    print("nodel:", node.line)
    print("nodet:", node.type)
    print("nodec:", node.constant)
    print("nodev:", node.value)

    if node.type == InstructionType.callUnary and node:areChildrenConstant() and node.value=="params" then
        print("Params!")
        -- We are calling params script command and the array argument consists of only constants, we can safely optimize the array to a push instruction
       
        function resolveMakeArray(node)
            print("resolve", node.type)
            if (node.type ~= InstructionType.makeArray) then return end
            
            for i,v in ipairs(node.children) do 
                resolveMakeArray(v)
            end
            node.value = ScriptConstantArray.new(); --dummy. Children are the contents
            node.type = InstructionType.push;
        end


        print("preresolve", #node.children, resolveMakeArray)
        resolveMakeArray(node.children[1])

        --#TODO verify again that they are all push or makeArray instructions

        node.children[1].value = ScriptConstantArray.new() --dummy. Children are the contents
        node.children[1].type = InstructionType.push
        print("Params optimized!")
    end


    -- We need to figure out what things we consider as constants
    -- Push and end statement are always constants
    if node.type == InstructionType.push or node.type == InstructionType.endStatement then
        node.constant = true
    end


    function isArrayConst(node)
        for i,v in ipairs(node.children) do 
            if not v.constant or v.type ~= InstructionType.push then
                return false
            end
        end
        return true
    end

    -- makeArray is only const if all children are makeArray or push
    if node.type == InstructionType.makeArray then
        node.constant = isArrayConst(node);
    end

end

local optimizer = OptimizerModuleLua.new(optimizerNodeHandler)


--compiler:CompileScriptToFile(path.new("P:\\test.sqf"), path.new("P:/test.asm"), optimizer)
compiler:CompileScriptToFile(path.new("T:\\z\\ace\\addons\\common\\functions\\fnc_cbaSettings_loadFromConfig.sqf"), path.new("P:/test.asm"), optimizer)

