// ReCarbon's own (GPL-3.0-or-later). Tests for why the installer turns a disc
// image or folder down (install/installer.h CheckGameVersion/CheckSource,
// install/disc_source.h OpenDiscSource): another platform's disc, another
// game, another region or version of Carbon, each with its own message that
// names the edition ReCarbon needs, while a damaged dump of the right edition
// keeps its own message. The disc files are synthetic: a 64-byte XEX2 header
// with a planted title ID, served from memory, against a three-file manifest;
// then the folder and image cases through the disk in a scratch folder. From
// the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/disc_check_test.cpp runtime/install/{placement,installer,locate,manifest,result,disc_source,sha256}.cpp runtime/user/portable.cpp -framework CoreFoundation -o build/disc_check_test
//   build/disc_check_test [folder]
// The disk checks run in a new folder made inside [folder] (default: the
// current folder), and only that one is deleted afterwards.
#include <install/installer.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

namespace fs = std::filesystem;
using namespace install;

static bool Has(const std::string& text, const char* part)
{
    return text.find(part) != std::string::npos;
}

constexpr uint32_t kCarbon = 0x454107EC;
constexpr const char* kNeeds = "ReCarbon needs your own Need for Speed: Carbon (USA) disc for Xbox 360.";

// A minimal XEX2 header: one optional header, the execution info (key
// 0x00040006) at 0x20, whose title ID is at +12. `salt` makes another
// executable of the same title.
static std::string Xex(uint32_t titleId, uint8_t salt = 0)
{
    std::string x(64, '\0');
    auto be32 = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; i++) x[at + i] = char(v >> (24 - 8 * i)); };
    memcpy(x.data(), "XEX2", 4);
    be32(0x14, 1);
    be32(0x18, 0x00040006);
    be32(0x1C, 0x20);
    be32(0x20 + 12, titleId);
    x[0x3F] = char(salt);
    return x;
}

// A disc served from memory: every CheckGameVersion branch without a disc image.
class MemorySource : public DiscSource
{
public:
    MemorySource(SourceKind k, std::map<std::string, std::string> contents) : data(std::move(contents))
    {
        kind = k;
        path = k == SourceKind::Image ? "Carbon.iso" : "Carbon";
        for (const auto& [name, bytes] : data)
            files.push_back({ name, bytes.size(), 0 });
    }

    std::unique_ptr<FileReader> Open(const DiscFile& file, std::string&) const override
    {
        return std::make_unique<Reader>(data.at(file.path));
    }

private:
    struct Reader : FileReader
    {
        explicit Reader(const std::string& b) : bytes(b) {}
        bool Read(uint64_t offset, void* buffer, size_t size, std::string& error) override
        {
            if (offset + size > bytes.size())
            {
                error = "past the end";
                return false;
            }
            memcpy(buffer, bytes.data() + offset, size);
            return true;
        }
        const std::string& bytes;
    };
    std::map<std::string, std::string> data;
};

// The supported disc: default.xex = Xex(kCarbon), and two data files.
struct TestManifest
{
    std::string xexHash = ToHex(Sha256Of(Xex(kCarbon).data(), 64));
    std::vector<ManifestFile> files{ { "NFS/GLOBAL.BUN", 4, "00" }, { "Movies/intro.wmv", 3, "00" }, { "default.xex", 64, xexHash } };
    Manifest manifest{ "test", kCarbon, files, 71 };
};

static void VersionChecks()
{
    TestManifest t;
    const Manifest& m = t.manifest;
    std::map<std::string, std::string> rest{ { "NFS/GLOBAL.BUN", "data" }, { "Movies/intro.wmv", "mov" } };
    auto with = [&](std::map<std::string, std::string> extra)
    {
        extra.insert(rest.begin(), rest.end());
        return extra;
    };

    // The supported disc passes both checks.
    MemorySource good(SourceKind::Image, with({ { "default.xex", Xex(kCarbon) } }));
    CHECK(CheckGameVersion(good, m).Ok(), "good: %s", CheckGameVersion(good, m).message.c_str());
    CHECK(CheckSource(good, m).Ok(), "good source: %s", CheckSource(good, m).message.c_str());

    // An original Xbox disc boots default.xbe.
    MemorySource xbox(SourceKind::Image, with({ { "default.xbe", "XBEH" } }));
    Result r = CheckGameVersion(xbox, m);
    CHECK(r.error == Error::WrongGame, "xbe: %s", ErrorName(r.error));
    CHECK(Has(r.message, "original Xbox") && Has(r.message, "doesn't look like the Xbox 360 version of Need for Speed: Carbon")
        && Has(r.message, kNeeds), "xbe: %s", r.message.c_str());

    // No executable at all (another console's or a PC disc's files).
    MemorySource none(SourceKind::Folder, with({ { "setup.exe", "MZ" } }));
    r = CheckGameVersion(none, m);
    CHECK(r.error == Error::NotADiscImage, "no xex: %s", ErrorName(r.error));
    CHECK(Has(r.message, "This folder has no default.xex") && Has(r.message, "doesn't look like the Xbox 360 version")
        && Has(r.message, kNeeds) && !Has(r.message, "original Xbox"), "no xex: %s", r.message.c_str());

    // A default.xex that isn't an Xbox 360 executable.
    MemorySource junk(SourceKind::Image, with({ { "default.xex", std::string(64, 'x') } }));
    r = CheckGameVersion(junk, m);
    CHECK(r.error == Error::WrongGame, "not XEX2: %s", ErrorName(r.error));
    CHECK(Has(r.message, "not an Xbox 360 executable") && Has(r.message, "doesn't look like the Xbox 360 version")
        && Has(r.message, kNeeds), "not XEX2: %s", r.message.c_str());

    // Another Xbox 360 game.
    MemorySource other(SourceKind::Image, with({ { "default.xex", Xex(0x4D5307E6) } }));
    r = CheckGameVersion(other, m);
    CHECK(r.error == Error::WrongGame, "other title: %s", ErrorName(r.error));
    CHECK(Has(r.message, "another game, not Need for Speed: Carbon") && Has(r.message, "4D5307E6")
        && Has(r.message, "454107EC") && Has(r.message, kNeeds), "other title: %s", r.message.c_str());
    CHECK(!Has(r.message, "region"), "other title: %s", r.message.c_str());

    // Carbon, another executable (another region or revision; same size, so
    // also possibly a damaged dump).
    MemorySource region(SourceKind::Image, with({ { "default.xex", Xex(kCarbon, 1) } }));
    r = CheckGameVersion(region, m);
    CHECK(r.error == Error::UnsupportedVersion, "region: %s", ErrorName(r.error));
    CHECK(Has(r.message, "a different region or version of Need for Speed: Carbon")
        && Has(r.message, "ReCarbon supports the USA edition (Xbox 360) only") && Has(r.message, "damaged")
        && Has(r.message, t.xexHash.c_str()), "region: %s", r.message.c_str());
    CHECK(r.foundHash == ToHex(Sha256Of(Xex(kCarbon, 1).data(), 64)), "region: found hash");
    // A different size can't be a damaged copy of the same file.
    MemorySource longer(SourceKind::Image, with({ { "default.xex", Xex(kCarbon) + "more" } }));
    r = CheckGameVersion(longer, m);
    CHECK(r.error == Error::UnsupportedVersion && Has(r.message, "different region or version") && !Has(r.message, "damaged"),
        "longer: %s", r.message.c_str());

    // The right executable with a bad data file: a damaged dump, as before.
    MemorySource missing(SourceKind::Image, { { "default.xex", Xex(kCarbon) }, { "NFS/GLOBAL.BUN", "data" } });
    r = CheckSource(missing, m);
    CHECK(r.error == Error::MissingFile && Has(r.message, "is incomplete: Movies/intro.wmv is missing")
        && !Has(r.message, "region"), "missing: %s", r.message.c_str());
    MemorySource cut(SourceKind::Image, with({ { "default.xex", Xex(kCarbon) }, { "NFS/GLOBAL.BUN", "dat" } }));
    r = CheckSource(cut, m);
    CHECK(r.error == Error::CorruptFile && Has(r.message, "is damaged: NFS/GLOBAL.BUN is 3 bytes")
        && !Has(r.message, "region"), "cut: %s", r.message.c_str());
}

static void Write(const fs::path& file, const std::string& bytes)
{
    fs::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << bytes;
}

static void DiskChecks(const fs::path& root)
{
    std::unique_ptr<DiscSource> source;

    // An extracted original Xbox disc.
    Write(root / "xbox" / "default.xbe", "XBEH");
    Result r = OpenDiscSource(root / "xbox", source);
    CHECK(r.error == Error::NotADiscImage, "xbe folder: %s", ErrorName(r.error));
    CHECK(Has(r.message, "original Xbox") && Has(r.message, kNeeds), "xbe folder: %s", r.message.c_str());
    // Also one folder down, where the picker looks for default.xex.
    Write(root / "xbox-up" / "files" / "DEFAULT.XBE", "XBEH");
    r = OpenDiscSource(root / "xbox-up", source);
    CHECK(Has(r.message, "original Xbox") && Has(r.message, kNeeds), "xbe one down: %s", r.message.c_str());

    // A folder with no game executable (a PC game's files).
    Write(root / "pc" / "setup.exe", "MZ");
    r = OpenDiscSource(root / "pc", source);
    CHECK(r.error == Error::NotADiscImage, "pc folder: %s", ErrorName(r.error));
    CHECK(Has(r.message, "holds no game") && Has(r.message, kNeeds) && !Has(r.message, "original Xbox"),
        "pc folder: %s", r.message.c_str());

    // A file that is no disc image any console uses here (a Wii image's start).
    Write(root / "wii.iso", std::string(0x18, '\0') + "\x5D\x1C\x9E\xA3" + std::string(4096, '\0'));
    r = OpenDiscSource(root / "wii.iso", source);
    CHECK(r.error == Error::NotADiscImage, "wii: %s", ErrorName(r.error));
    CHECK(Has(r.message, "is not an Xbox 360 disc image") && Has(r.message, kNeeds), "wii: %s", r.message.c_str());

    // A PC or PlayStation DVD: ISO 9660, larger than an XGD2's video
    // partition, and no Xbox game partition (sparse: nothing is written).
    fs::path dvd = root / "pc-dvd.iso";
    Write(dvd, std::string(0x8001, '\0') + "CD001");
    CHECK(truncate(dvd.c_str(), 0x10000000) == 0, "truncate: %s", strerror(errno));
    r = OpenDiscSource(dvd, source);
    CHECK(r.error == Error::UnsupportedFormat, "dvd: %s", ErrorName(r.error));
    CHECK(Has(r.message, "without an Xbox 360 game partition") && Has(r.message, kNeeds), "dvd: %s", r.message.c_str());

    // Messages about what the picked file is stay as they were.
    Write(root / "default.xex.zip", std::string("PK\x03\x04", 4) + std::string(64, '\0'));
    r = OpenDiscSource(root / "default.xex.zip", source);
    CHECK(r.error == Error::NotADiscImage && Has(r.message, "is a compressed archive") && !Has(r.message, kNeeds),
        "zip: %s", r.message.c_str());
    Write(root / "game.god", "LIVE" + std::string(64, '\0'));
    r = OpenDiscSource(root / "game.god", source);
    CHECK(r.error == Error::UnsupportedFormat && Has(r.message, "content package") && !Has(r.message, kNeeds),
        "package: %s", r.message.c_str());
}

int main(int argc, char** argv)
{
    VersionChecks();

    std::error_code ec;
    fs::path base = argc > 1 ? fs::path(argv[1]) : fs::current_path();
    fs::create_directories(base, ec);
    std::string pattern = (base / "disc_check_test.XXXXXX").string();
    if (!mkdtemp(pattern.data()))
    {
        printf("can't make a folder in %s: %s\n", base.c_str(), strerror(errno));
        return 2;
    }
    fs::path root = fs::canonical(pattern);
    DiskChecks(root);
    fs::remove_all(root, ec);

    printf("%s (%d failures)\n", g_failures ? "FAILED" : "passed", g_failures);
    return g_failures ? 1 : 0;
}
