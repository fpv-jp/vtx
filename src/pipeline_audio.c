#include "headers/pipeline.h"

// --- vtx_opus_pipeline ----------------------------------
gchar *
vtx_opus_pipeline (const MediaParams * params)
{
  return g_strdup_printf ("%s ! "
      "audioconvert ! "
      "audioresample ! "
      COMMON_QUEUE
      "opusenc perfect-timestamp=true ! "
      "rtpopuspay pt=%d ! "
      "application/x-rtp,media=audio,encoding-name=OPUS,payload=%d",
      params->audio_sampling,
      params->audio_payload_type, params->audio_payload_type);
}

// --- vtx_pcmu_pipeline ----------------------------------
gchar *
vtx_pcmu_pipeline (const MediaParams * params)
{
  return g_strdup_printf ("%s ! "
      "audioconvert ! "
      "audioresample ! "
      COMMON_QUEUE
      "mulawenc ! "
      "rtppcmupay pt=%d ! "
      "application/x-rtp,media=audio,encoding-name=PCMU,payload=%d",
      params->audio_sampling,
      params->audio_payload_type, params->audio_payload_type);
}
