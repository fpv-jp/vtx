#pragma once

#include <gst/gst.h>
#include <gst/webrtc/ice.h>

#ifndef __CUSTOM_AGENT_H__
#define __CUSTOM_AGENT_H__

G_BEGIN_DECLS
#define CUSTOMICE_TYPE_AGENT (customice_agent_get_type())
G_DECLARE_FINAL_TYPE(CustomICEAgent, customice_agent, CUSTOMICE, AGENT, GstWebRTCICE)
CustomICEAgent *customice_agent_new(const gchar *name, const gchar *network_interface);

void vtx_webrtc_on_ice_candidate(GstElement *webrtc, guint mlineindex, gchar *candidate, gpointer user_data);

void vtx_webrtc_notify_ice_gathering_state(GstElement *webrtc, GParamSpec *pspec, gpointer user_data);

void vtx_webrtc_notify_ice_connection_state(GstElement *webrtc, GParamSpec *pspec, gpointer user_data);

void vtx_webrtc_on_negotiation_needed(GstElement *element, gpointer user_data);

G_END_DECLS
#endif /* __CUSTOM_AGENT_H__ */
