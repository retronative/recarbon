// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// Settings > Graphics > Internal Resolution's text (video/resolution_label.h),
// word for word: the entries' standard names (HD 720p, Full HD 1080p, QHD
// 1440p, 4K UHD 2160p), the row naming what is drawn when it differs, Auto
// naming what it picked, the 4K cap's help line, and no scale factor shown. And settings.toml's
// tokens unchanged (user/settings.cpp, linked as the game links it): a file
// from v0.1.3 reads as it did, and a choice is saved as before.
// The argument is a scratch folder (settings.toml is written there).
//
// Build by hand (from the repo root):
//   c++ -std=c++20 -O2 -UNDEBUG -I runtime -I tools/XenonRecomp/thirdparty/tomlplusplus/include \
//     tests/resolution_label_test.cpp runtime/user/settings.cpp runtime/user/atomic_file.cpp -o build/resolution_label_test
//   build/resolution_label_test build/resolution_label_scratch
#include <video/resolution_label.h>
#include <user/settings.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using gpu::RenderScale;
using settings::Id;

namespace
{
    int g_failures = 0;

    void Check(bool ok, const char* what, const std::string& got = {})
    {
        if (!ok)
        {
            printf("  FAIL: %s%s%s\n", what, got.empty() ? "" : ": got ", got.c_str());
            g_failures++;
        }
    }

    const RenderScale k1 = RenderScale::Whole(1, 1), k2 = RenderScale::Whole(2, 2), k3 = RenderScale::Whole(3, 3);
    const RenderScale k15 = { gpu::AxisFromHalves(3), gpu::AxisFromHalves(3) };

    void Strings()
    {
        printf("---- the row's text ----\n");
        using video::ResolutionRowText;
        std::string t;
        t = ResolutionRowText("HD (720p)", false, k1, &k1);
        Check(t == "HD (720p)", "HD drawn as named: the label alone", t);
        t = ResolutionRowText("Full HD (1080p)", false, k15, &k15);
        Check(t == "Full HD (1080p)", "Full HD drawn as named", t);
        t = ResolutionRowText("QHD (1440p)", false, k2, &k2);
        Check(t == "QHD (1440p)", "QHD drawn as named", t);
        t = ResolutionRowText("4K UHD (2160p)", false, k3, &k3);
        Check(t == "4K UHD (2160p)", "4K UHD on a 4K display", t);
        t = ResolutionRowText("4K UHD (2160p)", false, k3, &k2);
        Check(t == "4K UHD (QHD on this display)", "4K UHD held at QHD by the cap: never claims 4K", t);
        const RenderScale gpuLimit = { gpu::AxisScale{ 2, 1 }, gpu::AxisScale{ 3, 2 } };
        t = ResolutionRowText("QHD (1440p)", false, k2, &gpuLimit);
        Check(t == "QHD (2560x1080 on this display)", "a size with no standard name is shown in pixels", t);
        t = ResolutionRowText("Auto", true, k1, &k2);
        Check(t == "Auto (QHD, 1440p)", "Auto at QHD", t);
        t = ResolutionRowText("Auto", true, k1, &k15);
        Check(t == "Auto (Full HD, 1080p)", "Auto at Full HD", t);
        const RenderScale wide = RenderScale::Whole(3, 2);
        t = ResolutionRowText("Auto", true, k1, &wide);
        Check(t == "Auto (3840x1440)", "Auto at a size with no standard name: pixels, no scale factor", t);
        t = ResolutionRowText("Auto", true, k1, nullptr);
        Check(t == "Auto", "Auto before a picture: the bare label", t);
        t = ResolutionRowText("QHD (1440p)", false, k2, nullptr);
        Check(t == "QHD (1440p)", "unknown: the bare label", t);
        const RenderScale four = RenderScale::Whole(4, 4);
        t = video::FixedRowText(four);
        Check(t == "5120x2880 (set for this run)", "RECARBON_RENDER_SCALE=4: the size drawn", t);

        printf("---- presets ----\n");
        Check(std::string(video::PresetName(k1)) == "HD" && std::string(video::PresetName(k15)) == "Full HD" &&
                std::string(video::PresetName(k2)) == "QHD" && std::string(video::PresetName(k3)) == "4K UHD",
            "HD, Full HD, QHD, 4K UHD");
        Check(!video::PresetName(wide) && !video::PresetName(four) && !video::PresetName({ { 2, 1 }, { 3, 2 } }), "no name for 3x2, 4x4, 2x1.5");
        Check(video::SizeText(k15) == "1920x1080" && video::SizeText(wide) == "3840x1440", "SizeText", video::SizeText(k15));

        printf("---- the cap's help line ----\n");
        t = video::CapNote(k3, k2, { 2940, 1912 });
        Check(t == "This display has 2940x1912 pixels, fewer than 4K UHD's 3840x2160, so the game draws QHD (2560x1440). Your choice is "
                   "kept and applies on a 4K display.",
            "4K UHD on the M5's panel", t);
        Check(video::CapNote(k3, k3, { 3840, 2160 }).empty() && video::CapNote(k2, k2, { 2940, 1912 }).empty(), "nothing when not capped");

        printf("---- no scale factor in any text a player sees ----\n");
        for (const std::string& shown : { ResolutionRowText("4K UHD (2160p)", false, k3, &k2), ResolutionRowText("Auto", true, k1, &wide),
                 ResolutionRowText("Auto", true, k1, &k2), video::CapNote(k3, k2, { 2940, 1912 }) })
            for (const char* factor : { "1x", "1.5x", "2x", "3x", "3x2", "(capped)" })
                Check(shown.find(factor) == std::string::npos, factor, shown);
    }

    std::string Read(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        std::stringstream text;
        text << in.rdbuf();
        return text.str();
    }

    void Settings(const std::filesystem::path& dir)
    {
        printf("---- the entries and settings.toml ----\n");
        const settings::Option& o = settings::Info(Id::InternalResolution);
        struct Entry { int32_t value; const char* token; const char* label; };
        const Entry want[] = { { 0, "auto", "Auto" }, { 2, "1x", "HD (720p)" }, { 3, "1.5x", "Full HD (1080p)" }, { 4, "2x", "QHD (1440p)" },
            { 6, "3x", "4K UHD (2160p)" } };
        Check(o.entries.size() == 5, "five entries");
        for (size_t i = 0; i < 5 && i < o.entries.size(); i++)
            Check(o.entries[i].value == want[i].value && std::string(o.entries[i].token) == want[i].token &&
                    std::string(o.entries[i].label) == want[i].label,
                want[i].label, std::string(o.entries[i].token) + " " + o.entries[i].label);
        Check(settings::FormatValue(Id::InternalResolution, 6) == "4K UHD (2160p)", "FormatValue(3x)");

        unsetenv("RECARBON_RESOLUTION");
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        const std::filesystem::path file = dir / "settings.toml";
        for (const Entry& e : want)
        {
            std::ofstream(file, std::ios::binary) << "[video]\ninternal_resolution = \"" << e.token << "\"\n";
            Check(settings::Load(file) && settings::GetInt(Id::InternalResolution) == e.value, "a v0.1.3 file's token reads as before", e.token);
        }
        Check(settings::Set(Id::InternalResolution, 4) == settings::SetResult::Applied, "set 2x (from the 3x just read)");
        Check(settings::Set(Id::InternalResolution, 6) == settings::SetResult::Applied && settings::Save(file), "set 3x and save");
        Check(Read(file).find("internal_resolution = \"3x\"") != std::string::npos, "saved as the token 3x", Read(file));
        std::filesystem::remove_all(dir);
    }
}

int main(int argc, char** argv)
{
    Strings();
    Settings(argc > 1 ? argv[1] : "resolution_label_scratch");
    if (g_failures)
    {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
