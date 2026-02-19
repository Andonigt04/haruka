#pragma once

#include <AL/al.h>
#include <AL/alc.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace Haruka {

struct AudioSource {
    ALuint sourceId = 0;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
    float gain = 1.0f;
    float pitch = 1.0f;
    bool looping = false;
    bool positional = true;
};

class AudioBuffer {
public:
    AudioBuffer(const std::string& filepath);
    ~AudioBuffer();
    
    ALuint getBufferId() const { return bufferId; }
    bool isLoaded() const { return bufferId != 0; }

private:
    ALuint bufferId = 0;
    bool loadWAV(const std::string& filepath);
    bool loadOGG(const std::string& filepath);
};

class AudioManager {
public:
    static AudioManager& getInstance() {
        static AudioManager instance;
        return instance;
    }
    
    bool init();
    void shutdown();
    
    // Listener (camera)
    void setListenerPosition(const glm::vec3& pos);
    void setListenerVelocity(const glm::vec3& vel);
    void setListenerOrientation(const glm::vec3& forward, const glm::vec3& up);
    
    // Load audio
    bool loadSound(const std::string& name, const std::string& filepath);
    
    // Play audio
    int playSound(const std::string& name, const glm::vec3& position = glm::vec3(0.0f), 
                  bool loop = false, float gain = 1.0f, float pitch = 1.0f);
    int playSound2D(const std::string& name, bool loop = false, float gain = 1.0f);
    
    // Source control
    void stopSource(int sourceId);
    void pauseSource(int sourceId);
    void resumeSource(int sourceId);
    
    void setSourcePosition(int sourceId, const glm::vec3& pos);
    void setSourceVelocity(int sourceId, const glm::vec3& vel);
    void setSourceGain(int sourceId, float gain);
    void setSourcePitch(int sourceId, float pitch);
    
    // Master volume
    void setMasterVolume(float volume);
    float getMasterVolume() const { return masterVolume; }
    
    // Update (for 3D audio)
    void update(float deltaTime);

private:
    AudioManager();
    ~AudioManager();
    
    ALCdevice* device = nullptr;
    ALCcontext* context = nullptr;
    
    std::map<std::string, std::shared_ptr<AudioBuffer>> buffers;
    std::vector<AudioSource> sources;
    std::vector<int> freeSources;
    
    float masterVolume = 1.0f;
    
    int allocateSource();
    void freeSource(int sourceId);
    
    void checkALError(const char* operation);
};

}