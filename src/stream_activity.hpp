#pragma once
#include <obs.h>
#include <obs-frontend-api.h>

// Aggregate real streaming outputs, including outputs owned by Aitum.
// Recording/replay/virtual-camera outputs do not have OBS_OUTPUT_SERVICE.
// obs_enum_outputs holds the output list lock while the callback reads flags.
inline bool anyStreamingOutputActive()
{
    bool live=obs_frontend_streaming_active();
    obs_enum_outputs([](void* context,obs_output_t* output) {
        auto* result=static_cast<bool*>(context);
        if((obs_output_get_flags(output)&OBS_OUTPUT_SERVICE) && obs_output_active(output))
            *result=true;
        return true;
    },&live);
    return live;
}
