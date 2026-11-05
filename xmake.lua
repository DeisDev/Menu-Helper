includes("lib/commonlibf4")

set_project("menu-helper")
set_version("0.1.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

option("flex_sdk")
    set_default("")
    set_showmenu(true)
    set_description("Apache Flex SDK directory for the MenuHelperSWF target (defaults to FLEX_HOME)")
option_end()

option("playerglobal")
    set_default("")
    set_showmenu(true)
    set_description("playerglobal.swc for the MenuHelperSWF target")
option_end()

option("papyrus_compiler")
    set_default("")
    set_showmenu(true)
    set_description("Creation Kit PapyrusCompiler.exe for the MenuHelperPapyrus target")
option_end()

option("papyrus_imports")
    set_default("")
    set_showmenu(true)
    set_description("Papyrus import folders separated by ';': F4SE script sources first, then the extracted Base.zip")
option_end()

target("MenuHelper")
    add_rules("commonlibf4.plugin", {
        name = "MenuHelper",
        author = "aiden",
        description = "Runtime API for adding entries and submenus to the Fallout 4 main and pause menus"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src", "include")
    set_pcxxheader("src/pch.h")
    add_installfiles("swf/MenuHelper.swf", { prefixdir = "Interface/MenuHelper" })
    add_installfiles("papyrus/MenuHelper.pex", { prefixdir = "Scripts" })
    add_installfiles("papyrus/MenuHelper.psc", { prefixdir = "Scripts/Source/User" })

target("MenuHelperExample")
    add_rules("commonlibf4.plugin", {
        name = "MenuHelperExample",
        author = "aiden",
        description = "Example consumer of the MenuHelper API"
    })

    add_files("examples/basic/**.cpp")
    add_includedirs("include")

-- Rebuilds swf/MenuHelper.swf from swf/src. Not built by default so C++-only builds need no Flex SDK.
target("MenuHelperSWF")
    set_kind("phony")
    set_default(false)

    on_build(function (target)
        import("lib.detect.find_tool")

        local sdk = get_config("flex_sdk")
        if not sdk or sdk == "" then
            sdk = os.getenv("FLEX_HOME")
        end
        if not sdk or not os.isdir(sdk) then
            raise("set --flex_sdk=<dir> or FLEX_HOME to an Apache Flex SDK")
        end
        local playerglobal = get_config("playerglobal")
        if not playerglobal or not os.isfile(playerglobal) then
            raise("set --playerglobal=<file> to a playerglobal.swc")
        end
        local java = find_tool("java")
        if not java then
            raise("java not found")
        end

        local srcdir = path.join(os.projectdir(), "swf", "src")
        os.vrunv(java.program, {
            "-jar", path.join(sdk, "lib", "mxmlc.jar"),
            "+flexlib=" .. path.join(sdk, "frameworks"),
            "-load-config=",
            "-external-library-path=" .. playerglobal,
            "-target-player=11.2",
            "-swf-version=15",
            "-default-size=1280,720",
            "-default-frame-rate=30",
            "-source-path=" .. srcdir,
            "-output", path.join(os.projectdir(), "swf", "MenuHelper.swf"),
            path.join(srcdir, "MenuHelper.as")
        })
    end)

-- Rebuilds papyrus/MenuHelper.pex from papyrus/MenuHelper.psc with the Creation Kit compiler.
target("MenuHelperPapyrus")
    set_kind("phony")
    set_default(false)

    on_build(function (target)
        local compiler = get_config("papyrus_compiler")
        if not compiler or not os.isfile(compiler) then
            raise("set --papyrus_compiler=<path to PapyrusCompiler.exe>")
        end
        local imports = get_config("papyrus_imports")
        if not imports or imports == "" then
            raise("set --papyrus_imports=<F4SE script sources>;<extracted Base.zip>")
        end

        -- The compiler derives the script namespace from the working directory.
        local srcdir = path.join(os.projectdir(), "papyrus")
        local olddir = os.cd(srcdir)
        os.vrunv(compiler, {
            "MenuHelper.psc",
            "-f=Institute_Papyrus_Flags.flg",
            "-i=" .. srcdir .. ";" .. imports,
            "-o=" .. srcdir,
            "-op", "-release", "-final"
        })
        os.cd(olddir)
    end)
