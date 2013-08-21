glmDir     = os.matchdirs("./deps/glm*")[1]
jsoncppDir = os.matchdirs("./deps/json*")[1]
assimpDir  = os.matchdirs("./deps/assimp*")[1]

if ( assimpDir and jsoncppDir and assimpDir ) then
    print("\nAll libraries found.");
else
    print("\nLibraries not found:");
    if not glmDir        then print("\nglm") end;
    if not jsoncppDir    then print("\njson-cpp") end;
    if not assimpDir     then print("\nassimp") end;
    print("");
    os.exit();
end

glmDir     = "./" .. glmDir;
jsoncppDir = "./" .. jsoncppDir;
assimpDir  = "./" .. assimpDir;

solution "assimp-to-json"
    configurations { "Debug", "Release" }
    includedirs    { glmDir, jsoncppDir .. "/dist", assimpDir .. "/include" }
    configuration "Debug"
        defines { "DEBUG" }
	flags   { "Symbols" }
	libdirs { assimpDir .. "/lib/Debug" }
	links   { "assimpD" }
	targetdir "./bin/Debug"

    configuration "Release"
        defines { "NDEBUG" }
	flags   { "Optimize" }
	libdirs { assimpDir .. "/lib/Release" }
	links   { "assimp" }
	targetdir "./bin/Release"

    project "assimp-to-json"
        kind "ConsoleApp"
	language "C++"
	files { "./src/**.cpp", "./src/**.h" }
	location "./proj"
