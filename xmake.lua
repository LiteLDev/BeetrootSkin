add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

set_policy("package.requires_lock", true)

-- BeetrootSkin is a client-only mod. The option exists because the levilamina package and
-- levibuildscript rules read it.
option("target_type")
    set_default("client")
    set_showmenu(true)
    set_values("client")
option_end()

option("api_sample")
    set_default(false)
    set_showmenu(true)
    set_description("Build beetroot-api-sample, a test-only mod using BeetrootSkin's API (e2e scenario 'api'; not shipped)")
option_end()

option("tests")
    set_default(true)
    set_showmenu(true)
    set_description("Build the host-side unit test binary (beetroot-tests)")
option_end()

add_requires("levilamina 26.51.7", {configs = {target_type = "client"}})
add_requires("levibuildscript")
add_requires("nlohmann_json v3.12.0")
if has_config("tests") then
    add_requires("gtest")
end

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

local function beetroot_common_flags()
    set_languages("c++20")
    set_toolchains("clang-cl")
    add_defines("NOMINMAX", "UNICODE", "_HAS_CXX23=1")
    add_cxflags("/utf-8", "/W4")
    add_cxflags(
        "-Wno-microsoft-cast",
        "-Wno-invalid-offsetof",
        "-Wno-c++2b-extensions",
        "-Wno-microsoft-include",
        "-Wno-overloaded-virtual",
        "-Wno-ignored-qualifiers",
        "-Wno-missing-field-initializers",
        "-Wno-potentially-evaluated-expression",
        "-Wno-pragma-system-header-outside-header",
        {tools = {"clang_cl"}}
    )
end

-- Pure, game-independent logic (src/core). No mc/ll headers allowed here.
target("beetroot-core")
    set_kind("static")
    beetroot_common_flags()
    set_exceptions("none")
    add_cxflags("/EHa")
    add_packages("nlohmann_json", {public = true})
    add_includedirs("src", "include", {public = true})
    add_headerfiles("src/core/**.h")
    add_files("src/core/**.cpp")

-- The mod itself. Every src/**.cpp outside src/core is compiled in, so a new
-- src/features/<name>/*.cpp is picked up without touching this file.
target("BeetrootSkin")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    set_kind("shared")
    beetroot_common_flags()
    set_exceptions("none") -- To avoid conflicts with /EHa.
    add_cxflags("/EHa", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
    add_cxflags("/EHs", {tools = {"clang_cl"}})
    set_symbols("debug")
    add_deps("beetroot-core")
    add_packages("levilamina", "nlohmann_json")
    add_syslinks("user32")
    add_includedirs("src", "include")
    -- Public API (include/beetrootskin/api): BEETROOTSKIN_API = dllexport while building the mod.
    add_defines("BEETROOTSKIN_EXPORTS")
    add_headerfiles("src/**.h")
    add_headerfiles("include/(beetrootskin/**.h)")
    add_files("src/**.cpp|core/**.cpp")
    after_build(function (target)
        import("core.base.json")
        local bindir = path.join(os.projectdir(), "bin", target:name())
        -- API SDK for other mods: bin/BeetrootSkin-sdk/{include/beetrootskin/**, lib/BeetrootSkin.lib}.
        local sdkdir = path.join(os.projectdir(), "bin", target:name() .. "-sdk")
        os.tryrm(sdkdir)
        local incsrc = path.join(os.projectdir(), "include")
        for _, file in ipairs(os.files(path.join(incsrc, "beetrootskin", "**.h"))) do
            local dst = path.join(sdkdir, "include", path.relative(file, incsrc))
            os.mkdir(path.directory(dst))
            os.cp(file, dst)
        end
        local implib = path.join(path.directory(target:targetfile()), path.basename(target:targetfile()) .. ".lib")
        if os.isfile(implib) then
            os.mkdir(path.join(sdkdir, "lib"))
            os.cp(implib, path.join(sdkdir, "lib"))
        else
            raise("import library not found: %s", implib)
        end
        -- Merge per-unit language fragments (assets/lang/<locale>/*.json) into
        -- bin/BeetrootSkin/lang/<locale>.json next to the mod. A key defined twice
        -- (with different values) fails the build.
        local outdir = path.join(bindir, "lang")
        os.mkdir(outdir)
        for _, localedir in ipairs(os.dirs(path.join(os.projectdir(), "assets", "lang", "*"))) do
            local merged = {}
            for _, file in ipairs(os.files(path.join(localedir, "*.json"))) do
                for k, v in pairs(json.loadfile(file)) do
                    if merged[k] ~= nil and merged[k] ~= v then
                        raise("duplicate lang key %s in %s", k, file)
                    end
                    merged[k] = v
                end
            end
            json.savefile(path.join(outdir, path.filename(localedir) .. ".json"), merged)
        end
        -- Mod-owned textures: assets/textures/** -> bin/BeetrootSkin/textures/**.
        local texsrc = path.join(os.projectdir(), "assets", "textures")
        local texdst = path.join(bindir, "textures")
        os.tryrm(texdst)
        if os.isdir(texsrc) then
            for _, file in ipairs(os.files(path.join(texsrc, "**"))) do
                local rel = path.relative(file, texsrc)
                local dst = path.join(texdst, rel)
                os.mkdir(path.directory(dst))
                os.cp(file, dst)
            end
        end
    end)

-- Test-only companion mod (xmake f --api_sample=y): subscribes to BeetrootSkin's API events from
-- another DLL, linked against BeetrootSkin's import library. Output: bin/beetroot-api-sample/.
if has_config("api_sample") then
    target("beetroot-api-sample")
        add_rules("@levibuildscript/linkrule")
        set_kind("shared")
        beetroot_common_flags()
        set_exceptions("none")
        add_cxflags("/EHa")
        add_cxflags("/EHs", {tools = {"clang_cl"}})
        add_deps("BeetrootSkin") -- links BeetrootSkin.lib (and builds/links after it)
        add_packages("levilamina")
        add_includedirs("include")
        add_files("samples/api_sample/*.cpp")
        after_build(function (target)
            local outdir = path.join(os.projectdir(), "bin", target:name())
            os.tryrm(outdir)
            os.mkdir(outdir)
            os.cp(target:targetfile(), outdir)
            os.cp(path.join(os.projectdir(), "samples", "api_sample", "manifest.json"), outdir)
        end)
end

if has_config("tests") then
    target("beetroot-tests")
        set_kind("binary")
        set_default(false)
        beetroot_common_flags()
        set_exceptions("cxx")
        add_deps("beetroot-core")
        add_packages("gtest", "nlohmann_json")
        add_files("tests/**.cpp")
        add_tests("default")
end
