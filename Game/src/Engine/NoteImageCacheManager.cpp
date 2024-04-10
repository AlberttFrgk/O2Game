#include "NoteImageCacheManager.hpp"
#include <algorithm>

constexpr int MAX_OBJECTS = 64;

NoteImageCacheManager::NoteImageCacheManager() {
    m_totalCount = 0;
    m_totalNoteCount = 0;
    m_totalHoldCount = 0;
    m_totalTrailCount = 0;
}

NoteImageCacheManager::~NoteImageCacheManager() {

}

NoteImageCacheManager* NoteImageCacheManager::s_instance = nullptr;

NoteImageCacheManager* NoteImageCacheManager::GetInstance() {
    if (s_instance == nullptr) {
        s_instance = new NoteImageCacheManager();
    }
    return s_instance;
}

void NoteImageCacheManager::Release() {
    if (s_instance) {
        // Clean up all note textures
        for (auto& pair : s_instance->m_noteTextures) {
            for (auto& note : pair.second) {
                delete note;
            }
        }
        s_instance->m_noteTextures.clear();

        // Clean up all hold textures
        for (auto& pair : s_instance->m_holdTextures) {
            for (auto& note : pair.second) {
                delete note;
            }
        }
        s_instance->m_holdTextures.clear();

        // Clean up all trail textures
        for (auto& pair : s_instance->m_trailTextures) {
            for (auto& note : pair.second) {
                delete note;
            }
        }
        s_instance->m_trailTextures.clear();

        delete s_instance;
        s_instance = nullptr;
    }
}

void NoteImageCacheManager::Repool(DrawableNote* image, NoteImageType noteType) {
    if (image == nullptr || m_noteTextures[noteType].size() >= MAX_OBJECTS)
        return;

    m_noteTextures[noteType].push_back(image);
    m_totalNoteCount++;
}

void NoteImageCacheManager::RepoolHold(DrawableNote* image, NoteImageType noteType) {
    if (image == nullptr || m_holdTextures[noteType].size() >= MAX_OBJECTS)
        return;

    m_holdTextures[noteType].push_back(image);
    m_totalHoldCount++;
}

void NoteImageCacheManager::RepoolTrail(DrawableNote* image, NoteImageType noteType) {
    if (image == nullptr || m_trailTextures[noteType].size() >= MAX_OBJECTS)
        return;

    m_trailTextures[noteType].push_back(image);
    m_totalTrailCount++;
}

DrawableNote* NoteImageCacheManager::Depool(NoteImageType noteType) {
    auto& textureMap = m_noteTextures[noteType];
    if (!textureMap.empty()) {
        DrawableNote* note = textureMap.back();
        textureMap.pop_back();
        return note;
    } else {
        // Create a new note if the pool is empty
        DrawableNote* note = new DrawableNote(GameNoteResource::GetNoteTexture(noteType));
        note->Repeat = true;
        return note;
    }
}

DrawableNote* NoteImageCacheManager::DepoolHold(NoteImageType noteType) {
    auto& textureMap = m_holdTextures[noteType];
    if (!textureMap.empty()) {
        DrawableNote* note = textureMap.back();
        textureMap.pop_back();
        return note;
    } else {
        // Create a new hold note if the pool is empty
        DrawableNote* note = new DrawableNote(GameNoteResource::GetNoteTexture(noteType));
        note->Repeat = true;
        return note;
    }
}

DrawableNote* NoteImageCacheManager::DepoolTrail(NoteImageType noteType) {
    auto& textureMap = m_trailTextures[noteType];
    if (!textureMap.empty()) {
        DrawableNote* note = textureMap.back();
        textureMap.pop_back();
        return note;
    } else {
        // Create a new trail note if the pool is empty
        DrawableNote* note = new DrawableNote(GameNoteResource::GetNoteTexture(noteType));
        note->Repeat = true;
        return note;
    }
}
