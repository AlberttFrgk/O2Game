#include "GameResources.hpp"

#include "../Engine/SkinManager.hpp"
#include "Configuration.h"
#include "Exception/SDLException.h"
#include "Rendering/Renderer.h"

#include "MsgBox.h"
#include <SDL2/SDL_image.h>
#include <algorithm>
#include <filesystem>
#include <fstream>

#if __linux__
#include <unistd.h>
#define MAX_PATH 256
#endif

#if _MSC_VER
#define STRCPY_F strcpy_s
#else
#define STRCPY_F strcpy
#endif

#include "../Engine/SkinConfig.hpp"
#include "../EnvironmentSetup.hpp"

#pragma warning(disable : 26451)

uint8_t OPI_MAGIC_FILE[] = { 0x02, 0x00, 0x00, 0x00 };
uint8_t OPI_FILES_MAGIC[] = { 0x01, 0x00, 0x00, 0x00 };
uint8_t OJS_MAGIC_FILE[] = { 0x01, 0x00, 0x55, 0x05 };
uint8_t BND_MAGIC_FILE[] = { 0xFF, 0xFF, 0xFF, 0xFF };

OJSFrame::OJSFrame(int X, int Y, int Width, int Height, short TransColor, int FrameOffset, int FrameSize)
{
    this->X = X;
    this->Y = Y;
    this->Width = Width;
    this->Height = Height;
    this->TransparencyColor = TransColor;
    this->FrameOffset = FrameOffset;
    this->FrameSize = FrameSize;
    this->Buffer = new uint8_t[FrameSize];
}

OJSFrame::~OJSFrame()
{
    if (Buffer == nullptr)
        return;

    delete[] Buffer;
}

OPIFile *InternalGetFile(std::string fileName, std::vector<OPIFile> &files)
{
    for (auto &f : files) {
        std::string name(f.Name);
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);

        if (name == fileName) {
            return &f;
        }
    }

    return nullptr;
}

ESTHANDLE *InternalLoadFileData(OPIFile *file, std::ifstream *stream)
{
    if (file == nullptr) {
        MsgBox::ShowOut("Error", "InternalLoadFileData::file == nullptr", MsgBoxType::OK, MsgBoxFlags::BTN_ERROR);
        return nullptr;
    }

    stream->seekg(file->FileOffset, std::ios::beg);

    if (!stream->good()) {
        MsgBox::ShowOut("Error", "InternalLoadFileData::stream::good == false", MsgBoxType::OK, MsgBoxFlags::BTN_ERROR);
        return nullptr;
    }

    if (memcmp(file->Name + strlen(file->Name) - 4, ".ojs", 4) == 0 || memcmp(file->Name + strlen(file->Name) - 4, ".oja", 4) == 0 || memcmp(file->Name + strlen(file->Name) - 4, ".oji", 4) == 0 || memcmp(file->Name + strlen(file->Name) - 4, ".ojt", 4) == 0) {

        uint8_t *buffer = new uint8_t[file->FileSize];
        stream->read((char *)buffer, file->FileSize);

        OJS *ojs = new OJS();
        STRCPY_F(ojs->Name, file->Name);

        ojs->RGBFormat = *(uint32_t *)(buffer);
        ojs->FrameCount = *(uint16_t *)(buffer + 4);
        ojs->TransparencyCode = *(uint16_t *)(buffer + 6);

        int headerSize = 2 + 2 + 2 + 2 + 4 + 4 + 4;
        int tmpStartOffset = 8 + (ojs->FrameCount * headerSize);
        for (int i = 0; i < ojs->FrameCount; i++) {
            int offset = 8 + (i * headerSize);

            OJSFrame frame = {
                *(short *)(buffer + offset + 0), // X
                *(short *)(buffer + offset + 2), // Y
                *(short *)(buffer + offset + 4), // Width
                *(short *)(buffer + offset + 6), // Height
                ojs->TransparencyCode,
                *(int *)(buffer + offset + 8),  // FrameOffset
                *(int *)(buffer + offset + 12), // FrameSize
            };

            ojs->Frames.push_back(std::make_unique<OJSFrame>(frame));

            ojs->Frames[i].get()->Buffer = new uint8_t[ojs->Frames[i].get()->FrameSize];
            memcpy(ojs->Frames[i].get()->Buffer, buffer + tmpStartOffset + ojs->Frames[i].get()->FrameOffset, ojs->Frames[i].get()->FrameSize);
        }

        delete[] buffer;
        return (ESTHANDLE *)ojs;
    }

    // check if file->Name ends with .bnd
    if (memcmp(file->Name + strlen(file->Name) - 4, ".bnd", 4) == 0) {
        BND *bnd = new BND();

        int MAGIC_NUMBER = 0;
        stream->read((char *)&MAGIC_NUMBER, 4);
        if (MAGIC_NUMBER != -1) {
            return (ESTHANDLE *)bnd;
        }

        stream->read((char *)&bnd->Count, 2);

        for (int i = 0; i < bnd->Count; i++) {
            Boundary bound = {};

            stream->read((char *)&bound.X, 4);
            stream->read((char *)&bound.Y, 4);
            stream->read((char *)&bound.Width, 4);
            stream->read((char *)&bound.Height, 4);

            bnd->Coordinates.push_back(bound);
        }

        return (ESTHANDLE *)bnd;
    }

    return nullptr;
}

std::tuple<std::vector<OPIFile>, std::ifstream *> LoadOPI(std::filesystem::path &path)
{
    std::vector<OPIFile> files;

    if (std::filesystem::exists(path)) {
        throw std::runtime_error("Missing: " + path.string());
    }

    std::fstream file(path, std::ios::in | std::ios::binary);
    if (!file.good()) {
        throw std::runtime_error("Failed to open: " + path.string());
    }

    size_t bufferSize = 0;
    file.seekg(0, std::ios::end);
    bufferSize = file.tellg();
    file.seekg(0, std::ios::beg);

    char opi_magic[4];
    file.read(opi_magic, 4);

    if (memcmp(opi_magic, "OPI", 3) != 0) {
        throw std::runtime_error("Invalid OPI file: " + path.string());
    }

    int fileCount = 0;
    file.read((char *)&fileCount, 4);

    if ((size_t)(fileCount * 152) > bufferSize) {
        throw std::runtime_error("Invalid OPI file: " + path.string());
    }

    file.seekg((bufferSize - ((size_t)fileCount * 152)), std::ios::beg);

    for (int i = 0; i < fileCount; i++) {
        char file_magic[4] = {};
        file.read(file_magic, 4);

        if (memcmp(file_magic, OPI_FILES_MAGIC, 4) != 0) {
            std::cout << "Unknown file magic: " << std::hex << file_magic << " at index: " << file.tellg() << std::endl;
        }

        OPIFile idx = {};

        char fileName[128] = {};
        file.read(fileName, 128);
        STRCPY_F(idx.Name, fileName);

        file.read((char *)&idx.FileOffset, 4);
        file.read((char *)&idx.FileSize, 4);

        int FileSize2 = 0;
        file.read((char *)&FileSize2, 4);

        idx.FileSize = (std::max)(idx.FileSize, FileSize2);

        file.seekg(8, std::ios::cur);

        files.push_back(idx);
    }

    // not really got idea but whatever
    return { files, new std::ifstream(path, std::ios::binary) };
}

namespace GameAvatarResource {
    std::vector<OPIFile> files;
    std::ifstream       *file;

    bool Load()
    {
        char path[MAX_PATH];
#if _WIN32
        GetModuleFileNameA(NULL, path, MAX_PATH);
#else
        int len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (len != -1) {
            path[len] = '\0';
        }
#endif

        // get path to .opi file
        std::string           opiPath = std::string(path);
        std::filesystem::path _path = opiPath.substr(0, opiPath.find_last_of("\\/")) + "\\Image\\Avatar.opi";

        if (!std::filesystem::exists(_path)) {
            MsgBox::ShowOut("Error", "Missing: " + _path.string(), MsgBoxType::OK, MsgBoxFlags::BTN_ERROR);
            return false;
        }

        try {
            auto res = LoadOPI(_path);

            files = std::move(std::get<0>(res));
            file = std::get<1>(res);
        } catch (std::runtime_error &e) {
            MsgBox::ShowOut("Error", e.what(), MsgBoxType::OK, MsgBoxFlags::BTN_ERROR);
            return false;
        }

        return true;
    }

    bool Dispose()
    {
        file->close();
        files.clear();

        return true;
    }

    OPIFile *GetFile(std::string name)
    {
        return InternalGetFile(name, files);
    }

    ESTHANDLE *LoadFileData(OPIFile *opiFile)
    {
        return InternalLoadFileData(opiFile, file);
    }
} // namespace GameAvatarResource

namespace GameInterfaceResource {
    std::vector<OPIFile> files;
    std::ifstream       *file;

    bool Load()
    {
        char path[MAX_PATH];
#if _WIN32
        GetModuleFileNameA(NULL, path, MAX_PATH);
#else
        int len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (len != -1) {
            path[len] = '\0';
        }
#endif

        // get path to .opi file
        std::string           opiPath = std::string(path);
        std::filesystem::path _path = opiPath.substr(0, opiPath.find_last_of("\\/")) + "\\Image\\Interface.opi";

        if (!std::filesystem::exists(_path)) {
            MsgBox::ShowOut("Error", "Missing: " + _path.string(), MsgBoxType::OK, MsgBoxFlags::BTN_ERROR);
            return false;
        }

        try {
            auto res = LoadOPI(_path);

            files = std::move(std::get<0>(res));
            file = std::get<1>(res);
        } catch (std::runtime_error &e) {
            MsgBox::ShowOut("Error", e.what(), MsgBoxType::OK, MsgBoxFlags::BTN_ERROR);
            return false;
        }

        return true;
    }

    bool Dispose()
    {
        file->close();
        files.clear();

        return true;
    }

    OPIFile *GetFile(std::string name)
    {
        return InternalGetFile(name, files);
    }

    ESTHANDLE *LoadFileData(OPIFile *opiFile)
    {
        return InternalLoadFileData(opiFile, file);
    }
} // namespace GameInterfaceResource

namespace GamePlayingResource {
    std::vector<OPIFile> files;
    std::ifstream       *file;

    bool Load()
    {
        char path[MAX_PATH];
#if _WIN32
        GetModuleFileNameA(NULL, path, MAX_PATH);
#else
        int len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (len != -1) {
            path[len] = '\0';
        }
#endif

        // get path to .opi file
        std::string           opiPath = std::string(path);
        std::filesystem::path _path = opiPath.substr(0, opiPath.find_last_of("\\/")) + "\\Image\\Playing.opi";

        if (!std::filesystem::exists(_path)) {
            MsgBox::ShowOut("Error", "Missing: " + _path.string(), MsgBoxType::OK, MsgBoxFlags::BTN_ERROR);
            return false;
        }

        try {
            auto res = LoadOPI(_path);

            files = std::move(std::get<0>(res));
            file = std::get<1>(res);
        } catch (std::runtime_error &e) {
            MsgBox::ShowOut("Error", e.what(), MsgBoxType::OK, MsgBoxFlags::BTN_ERROR);
            return false;
        }

        return true;
    }

    bool Dispose()
    {
        file->close();
        files.clear();

        return true;
    }

    OPIFile *GetFile(std::string name)
    {
        return InternalGetFile(name, files);
    }

    ESTHANDLE *LoadFileData(OPIFile *opiFile)
    {
        return InternalLoadFileData(opiFile, file);
    }
} // namespace GamePlayingResource

namespace GameNoteResource {
    std::unordered_map<NoteImageType, NoteImage *> noteTextures;
    bool                                           Loaded = false;

    bool Load()
    {
        if (Loaded) {
            throw std::runtime_error("NoteResource already loaded");
        }

        bool IsVulkan = Renderer::GetInstance()->IsVulkan();
        std::filesystem::path skinNotePath;

        if (EnvironmentSetup::GetInt("NoteSkin") == 1) {
            auto path = std::filesystem::current_path() / "Resources";
            skinNotePath = path / "Notes" / "Circle";
        }
        else if (EnvironmentSetup::GetInt("NoteSkin") == 2) {
            auto skinPath = SkinManager::GetInstance()->GetPath();
            skinNotePath = skinPath / "Notes";
        }
        else {
            auto path = std::filesystem::current_path() / "Resources";
            skinNotePath = path / "Notes" / "Square";
        }

        auto manager = SkinManager::GetInstance();

        std::string laneMap[7] = { "Note1", "Note2", "Note1", "NoteS", "Note1", "Note2", "Note1" };

        for (int i = 0; i < 7; i++) {
            std::string noteBase = laneMap[i] + "N";
            std::string holdBase = laneMap[i] + "L";

            std::vector<std::filesystem::path> notePaths;
            std::vector<std::filesystem::path> holdPaths;

            if (std::filesystem::exists(skinNotePath / (noteBase + "-0.png"))) {
                int frame = 0;
                while (std::filesystem::exists(skinNotePath / (noteBase + "-" + std::to_string(frame) + ".png"))) {
                    notePaths.push_back(skinNotePath / (noteBase + "-" + std::to_string(frame) + ".png"));
                    frame++;
                }
            } else if (std::filesystem::exists(skinNotePath / (noteBase + ".png"))) {
                notePaths.push_back(skinNotePath / (noteBase + ".png"));
            } else {
                throw std::runtime_error("Missing note files for lane " + std::to_string(i) + " base: " + noteBase);
            }

            if (std::filesystem::exists(skinNotePath / (holdBase + "-0.png"))) {
                int frame = 0;
                while (std::filesystem::exists(skinNotePath / (holdBase + "-" + std::to_string(frame) + ".png"))) {
                    holdPaths.push_back(skinNotePath / (holdBase + "-" + std::to_string(frame) + ".png"));
                    frame++;
                }
            } else if (std::filesystem::exists(skinNotePath / (holdBase + ".png"))) {
                holdPaths.push_back(skinNotePath / (holdBase + ".png"));
            } else {
                throw std::runtime_error("Missing hold files for lane " + std::to_string(i) + " base: " + holdBase);
            }

            NoteImage *noteImage = new NoteImage();
            NoteImage *holdImage = new NoteImage();

            noteImage->MaxFrames = notePaths.size();
            holdImage->MaxFrames = holdPaths.size();

            noteImage->Surface.resize(notePaths.size());
            holdImage->Surface.resize(holdPaths.size());

            noteImage->Texture.resize(notePaths.size());
            holdImage->Texture.resize(holdPaths.size());

            noteImage->VulkanTexture.resize(notePaths.size());
            holdImage->VulkanTexture.resize(holdPaths.size());

            for (int j = 0; j < notePaths.size(); j++) {
                auto path = notePaths[j];

                if (IsVulkan) {
                    auto tex_data = vkTexture::TexLoadImage(path);
                    if (tex_data == nullptr) {
                        throw std::runtime_error("Failed to load image: " + path.string() + "!");
                    }

                    noteImage->VulkanTexture[j] = tex_data;

                    if (j == 0) {
                        auto &rect = noteImage->TextureRect;
                        rect.left = 0;
                        rect.top = 0;

                        vkTexture::QueryTexture(tex_data, rect.right, rect.bottom);
                    }
                } else {
                    noteImage->Surface[j] = IMG_Load(path.string().c_str());
                    if (noteImage->Surface[j] == nullptr) {
                        throw std::runtime_error("Failed to load image: " + path.string() + "!");
                    }

                    noteImage->Texture[j] = SDL_CreateTextureFromSurface(Renderer::GetInstance()->GetSDLRenderer(), noteImage->Surface[j]);
                    if (noteImage->Texture[j] == nullptr) {
                        throw std::runtime_error("Failed to create texture from image: " + path.string() + "!");
                    }

                    if (j == 0) {
                        // query texture size
                        auto &rect = noteImage->TextureRect;
                        rect.left = 0;
                        rect.top = 0;

                        SDL_QueryTexture(noteImage->Texture[j], NULL, NULL, (int *)&rect.right, (int *)&rect.bottom);
                    }
                }
            }

            for (int j = 0; j < holdPaths.size(); j++) {
                auto path = holdPaths[j];

                if (IsVulkan) {
                    auto tex_data = vkTexture::TexLoadImage(path);
                    if (tex_data == nullptr) {
                        throw std::runtime_error("Failed to load image: " + path.string() + "!");
                    }

                    holdImage->VulkanTexture[j] = tex_data;

                    if (j == 0) {
                        auto &rect = holdImage->TextureRect;
                        rect.left = 0;
                        rect.top = 0;

                        vkTexture::QueryTexture(tex_data, rect.right, rect.bottom);
                    }
                } else {
                    holdImage->Surface[j] = IMG_Load((const char *)path.u8string().c_str());
                    if (holdImage->Surface[j] == nullptr) {
                        throw std::runtime_error("Failed to load image: " + path.string() + "!");
                    }

                    holdImage->Texture[j] = SDL_CreateTextureFromSurface(Renderer::GetInstance()->GetSDLRenderer(), holdImage->Surface[j]);
                    if (holdImage->Texture[j] == nullptr) {
                        throw std::runtime_error("Failed to create texture from image: " + path.string() + "!");
                    }

                    if (j == 0) {
                        // query texture size
                        auto &rect = holdImage->TextureRect;
                        rect.left = 0;
                        rect.top = 0;

                        SDL_QueryTexture(holdImage->Texture[j], NULL, NULL, (int *)&rect.right, (int *)&rect.bottom);
                    }
                }
            }

            noteTextures[(NoteImageType)i] = noteImage;
            noteTextures[(NoteImageType)(i + 7)] = holdImage;
        }

        std::vector<std::filesystem::path> trailUpPaths;
        if (std::filesystem::exists(skinNotePath / "TrailUp0.png")) {
            int frame = 0;
            while (std::filesystem::exists(skinNotePath / ("TrailUp" + std::to_string(frame) + ".png"))) {
                trailUpPaths.push_back(skinNotePath / ("TrailUp" + std::to_string(frame) + ".png"));
                frame++;
            }
        } else if (std::filesystem::exists(skinNotePath / "TrailUp.png")) {
            trailUpPaths.push_back(skinNotePath / "TrailUp.png");
        }

        std::vector<std::filesystem::path> trailDownPaths;
        if (std::filesystem::exists(skinNotePath / "TrailDown0.png")) {
            int frame = 0;
            while (std::filesystem::exists(skinNotePath / ("TrailDown" + std::to_string(frame) + ".png"))) {
                trailDownPaths.push_back(skinNotePath / ("TrailDown" + std::to_string(frame) + ".png"));
                frame++;
            }
        } else if (std::filesystem::exists(skinNotePath / "TrailDown.png")) {
            trailDownPaths.push_back(skinNotePath / "TrailDown.png");
        }

        NoteImage *trailUpImg = new NoteImage();
        trailUpImg->MaxFrames = trailUpPaths.size();
        trailUpImg->Texture.resize(trailUpPaths.size());
        trailUpImg->Surface.resize(trailUpPaths.size());
        trailUpImg->VulkanTexture.resize(trailUpPaths.size());

        NoteImage *trailDownImg = new NoteImage();
        trailDownImg->MaxFrames = trailDownPaths.size();
        trailDownImg->Texture.resize(trailDownPaths.size());
        trailDownImg->Surface.resize(trailDownPaths.size());
        trailDownImg->VulkanTexture.resize(trailDownPaths.size());

        for (int i = 0; i < trailUpPaths.size(); i++) {
            auto path = trailUpPaths[i];

            if (IsVulkan) {
                auto tex_data = vkTexture::TexLoadImage(path);
                if (tex_data == nullptr) {
                    throw std::runtime_error("Failed to load image: " + path.string() + "!");
                }

                trailUpImg->VulkanTexture[i] = tex_data;

                if (i == 0) {
                    auto &rect = trailUpImg->TextureRect;
                    rect.left = 0;
                    rect.top = 0;

                    vkTexture::QueryTexture(tex_data, rect.right, rect.bottom);
                }
            } else {
                trailUpImg->Surface[i] = IMG_Load((const char *)path.u8string().c_str());
                if (trailUpImg->Surface[i] == nullptr) {
                    throw std::runtime_error("Failed to load image: " + path.string() + "!");
                }

                trailUpImg->Texture[i] = SDL_CreateTextureFromSurface(Renderer::GetInstance()->GetSDLRenderer(), trailUpImg->Surface[i]);
                if (trailUpImg->Texture[i] == nullptr) {
                    throw std::runtime_error("Failed to create texture from image: " + path.string() + "!");
                }

                if (i == 0) {
                    // query texture size
                    auto &rect = trailUpImg->TextureRect;
                    rect.left = 0;
                    rect.top = 0;

                    SDL_QueryTexture(trailUpImg->Texture[i], NULL, NULL, (int *)&rect.right, (int *)&rect.bottom);
                }
            }
        }

        for (int i = 0; i < trailDownPaths.size(); i++) {
            auto path = trailDownPaths[i];

            if (IsVulkan) {
                auto tex_data = vkTexture::TexLoadImage(path);
                if (tex_data == nullptr) {
                    throw std::runtime_error("Failed to load image: " + path.string() + "!");
                }

                trailDownImg->VulkanTexture[i] = tex_data;

                if (i == 0) {
                    auto &rect = trailDownImg->TextureRect;
                    rect.left = 0;
                    rect.top = 0;

                    vkTexture::QueryTexture(tex_data, rect.right, rect.bottom);
                }
            } else {
                trailDownImg->Surface[i] = IMG_Load((const char *)path.u8string().c_str());
                if (trailDownImg->Surface[i] == nullptr) {
                    throw std::runtime_error("Failed to load image: " + path.string() + "!");
                }

                trailDownImg->Texture[i] = SDL_CreateTextureFromSurface(Renderer::GetInstance()->GetSDLRenderer(), trailDownImg->Surface[i]);
                if (trailDownImg->Texture[i] == nullptr) {
                    throw std::runtime_error("Failed to create texture from image: " + path.string() + "!");
                }

                if (i == 0) {
                    // query texture size
                    auto &rect = trailDownImg->TextureRect;
                    rect.left = 0;
                    rect.top = 0;

                    SDL_QueryTexture(trailDownImg->Texture[i], NULL, NULL, (int *)&rect.right, (int *)&rect.bottom);
                }
            }
        }

        noteTextures[NoteImageType::TRAIL_UP] = trailUpImg;
        noteTextures[NoteImageType::TRAIL_DOWN] = trailDownImg;

        Loaded = true;
        return true;
    }

    bool Dispose()
    {
        bool IsVulkan = Renderer::GetInstance()->IsVulkan();

        for (auto &it : noteTextures) {
            if (IsVulkan) {
                for (auto &tex : it.second->VulkanTexture) {
                    vkTexture::ReleaseTexture(tex);
                    tex = nullptr;
                }
            } else {
                for (auto &tex : it.second->Texture) {
                    SDL_DestroyTexture(tex);
                    tex = nullptr;
                }

                for (auto &sur : it.second->Surface) {
                    SDL_FreeSurface(sur);
                    sur = nullptr;
                }
            }

            delete it.second;
        }

        noteTextures.clear();
        Loaded = false;
        return true;
    }

    NoteImage *GetNoteTexture(NoteImageType noteType)
    {
        return noteTextures[noteType];
    }
} // namespace GameNoteResource
