#include <gst/webrtc/nice/nice.h>

#include "webrtc.h"

struct _CustomICEAgent
{
  GstWebRTCICE parent;
  GstWebRTCNice *nice_agent;
  gchar *network_interface;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE(CustomICEAgent, customice_agent, GST_TYPE_WEBRTC_ICE)
/* *INDENT-ON* */

GstWebRTCICEStream *customice_agent_add_stream(GstWebRTCICE *ice, guint session_id)
{
  CustomICEAgent *agent = CUSTOMICE_AGENT(ice);
  if (!agent->nice_agent)
  {
    gst_printerrln("CustomICEAgent: nice_agent is NULL in add_stream");
    return NULL;
  }
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(agent->nice_agent);
  return gst_webrtc_ice_add_stream(c_ice, session_id);
}

GstWebRTCICETransport *customice_agent_find_transport(GstWebRTCICE *ice, GstWebRTCICEStream *stream, GstWebRTCICEComponent component)
{
  CustomICEAgent *agent = CUSTOMICE_AGENT(ice);
  if (!agent->nice_agent)
  {
    gst_printerrln("CustomICEAgent: nice_agent is NULL in find_transport");
    return NULL;
  }
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(agent->nice_agent);
  return gst_webrtc_ice_find_transport(c_ice, stream, component);
}

/* Determine if GStreamer uses the new 3-parameter API or old 4-parameter API.
 * The API change is inconsistent across distributions and builds.
 * Embedded ARM platforms (RPI4, Jetson, Radxa, Rock 5B) typically use newer 3-param API.
 * Desktop platforms (Mac, x86 Linux) may still use older 4-param API with GstPromise.
 */
#if (GST_VERSION_MINOR < 23)
/* Embedded platforms: 3-parameter API (no promise) */
void customice_agent_add_candidate(GstWebRTCICE *ice, GstWebRTCICEStream *stream, const gchar *candidate)
{
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(CUSTOMICE_AGENT(ice)->nice_agent);
  gst_webrtc_ice_add_candidate(c_ice, stream, candidate);
}
#else
/* Desktop platforms: 4-parameter API (with promise) */
void customice_agent_add_candidate(GstWebRTCICE *ice, GstWebRTCICEStream *stream, const gchar *candidate, GstPromise *promise)
{
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(CUSTOMICE_AGENT(ice)->nice_agent);
  gst_webrtc_ice_add_candidate(c_ice, stream, candidate, promise);
}
#endif

gboolean customice_agent_set_remote_credentials(GstWebRTCICE *ice, GstWebRTCICEStream *stream, const gchar *ufrag, const gchar *pwd)
{
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(CUSTOMICE_AGENT(ice)->nice_agent);
  return gst_webrtc_ice_set_remote_credentials(c_ice, stream, ufrag, pwd);
}

gboolean customice_agent_add_turn_server(GstWebRTCICE *ice, const gchar *uri)
{
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(CUSTOMICE_AGENT(ice)->nice_agent);
  return gst_webrtc_ice_add_turn_server(c_ice, uri);
}

gboolean customice_agent_set_local_credentials(GstWebRTCICE *ice, GstWebRTCICEStream *stream, const gchar *ufrag, const gchar *pwd)
{
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(CUSTOMICE_AGENT(ice)->nice_agent);
  return gst_webrtc_ice_set_local_credentials(c_ice, stream, ufrag, pwd);
}

gboolean customice_agent_gather_candidates(GstWebRTCICE *ice, GstWebRTCICEStream *stream)
{
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(CUSTOMICE_AGENT(ice)->nice_agent);
  return gst_webrtc_ice_gather_candidates(c_ice, stream);
}

void customice_agent_set_is_controller(GstWebRTCICE *ice, gboolean controller)
{
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(CUSTOMICE_AGENT(ice)->nice_agent);
  gst_webrtc_ice_set_is_controller(c_ice, controller);
}

gboolean customice_agent_get_is_controller(GstWebRTCICE *ice)
{
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(CUSTOMICE_AGENT(ice)->nice_agent);
  return gst_webrtc_ice_get_is_controller(c_ice);
}

void customice_agent_set_force_relay(GstWebRTCICE *ice, gboolean force_relay)
{
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(CUSTOMICE_AGENT(ice)->nice_agent);
  gst_webrtc_ice_set_force_relay(c_ice, force_relay);
}

void customice_agent_set_tos(GstWebRTCICE *ice, GstWebRTCICEStream *stream, guint tos)
{
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(CUSTOMICE_AGENT(ice)->nice_agent);
  gst_webrtc_ice_set_tos(c_ice, stream, tos);
}

void customice_agent_set_on_ice_candidate(GstWebRTCICE *ice, GstWebRTCICEOnCandidateFunc func, gpointer user_data, GDestroyNotify notify)
{
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(CUSTOMICE_AGENT(ice)->nice_agent);
  gst_webrtc_ice_set_on_ice_candidate(c_ice, func, user_data, notify);
}

void customice_agent_set_stun_server(GstWebRTCICE *ice, const gchar *uri_s)
{
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(CUSTOMICE_AGENT(ice)->nice_agent);
  gst_webrtc_ice_set_stun_server(c_ice, uri_s);
}

gchar *customice_agent_get_stun_server(GstWebRTCICE *ice)
{
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(CUSTOMICE_AGENT(ice)->nice_agent);
  return gst_webrtc_ice_get_stun_server(c_ice);
}

void customice_agent_set_turn_server(GstWebRTCICE *ice, const gchar *uri_s)
{
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(CUSTOMICE_AGENT(ice)->nice_agent);
  gst_webrtc_ice_set_turn_server(c_ice, uri_s);
}

gchar *customice_agent_get_turn_server(GstWebRTCICE *ice)
{
  GstWebRTCICE *c_ice = GST_WEBRTC_ICE(CUSTOMICE_AGENT(ice)->nice_agent);
  return gst_webrtc_ice_get_turn_server(c_ice);
}

static void customice_agent_finalize(GObject *object)
{
  CustomICEAgent *ice = CUSTOMICE_AGENT(object);
  g_free(ice->network_interface);
  if (ice->nice_agent)
  {
    g_object_unref(ice->nice_agent);
    ice->nice_agent = NULL;
  }
  G_OBJECT_CLASS(customice_agent_parent_class)->finalize(object);
}

static void customice_agent_class_init(CustomICEAgentClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GstWebRTCICEClass *gst_webrtc_ice_class = GST_WEBRTC_ICE_CLASS(klass);

  gobject_class->finalize = customice_agent_finalize;

  // override virtual functions
#if (GST_VERSION_MINOR < 23)
  gst_webrtc_ice_class->add_candidate = (void (*)(GstWebRTCICE *, GstWebRTCICEStream *, const gchar *)) customice_agent_add_candidate;
#else
  gst_webrtc_ice_class->add_candidate = (void (*)(GstWebRTCICE *, GstWebRTCICEStream *, const gchar *, GstPromise *)) customice_agent_add_candidate;
#endif
  gst_webrtc_ice_class->add_stream = customice_agent_add_stream;
  gst_webrtc_ice_class->add_turn_server = customice_agent_add_turn_server;
  gst_webrtc_ice_class->find_transport = customice_agent_find_transport;
  gst_webrtc_ice_class->gather_candidates = customice_agent_gather_candidates;
  gst_webrtc_ice_class->get_is_controller = customice_agent_get_is_controller;
  gst_webrtc_ice_class->get_stun_server = customice_agent_get_stun_server;
  gst_webrtc_ice_class->get_turn_server = customice_agent_get_turn_server;
  gst_webrtc_ice_class->set_force_relay = customice_agent_set_force_relay;
  gst_webrtc_ice_class->set_is_controller = customice_agent_set_is_controller;
  gst_webrtc_ice_class->set_local_credentials = customice_agent_set_local_credentials;
  gst_webrtc_ice_class->set_remote_credentials = customice_agent_set_remote_credentials;
  gst_webrtc_ice_class->set_stun_server = customice_agent_set_stun_server;
  gst_webrtc_ice_class->set_tos = customice_agent_set_tos;
  gst_webrtc_ice_class->set_turn_server = customice_agent_set_turn_server;
  gst_webrtc_ice_class->set_on_ice_candidate = customice_agent_set_on_ice_candidate;
}

static void customice_agent_init(CustomICEAgent *ice)
{
  ice->nice_agent = gst_webrtc_nice_new("nice_agent");
  ice->network_interface = NULL;

  if (!ice->nice_agent)
  {
    gst_printerrln("Failed to create GstWebRTCNice agent");
  }
  else if (!GST_IS_WEBRTC_ICE(ice->nice_agent))
  {
    gst_printerrln("Created nice_agent is not a valid GstWebRTCICE object");
  }
  else
  {
    // Take ownership of the nice_agent by sinking the floating reference
    g_object_ref_sink(ice->nice_agent);
    gst_println("CustomICEAgent: Successfully created nice_agent (refcount after sink: %d)", G_OBJECT(ice->nice_agent)->ref_count);
  }
}

CustomICEAgent *customice_agent_new(const gchar *name, const gchar *network_interface)
{
  CustomICEAgent *agent = g_object_new(CUSTOMICE_TYPE_AGENT, "name", name, NULL);

  // Validate that the agent is properly registered as a GstWebRTCICE
  gst_println("CustomICEAgent type check: %s", GST_IS_WEBRTC_ICE(agent) ? "VALID GstWebRTCICE" : "NOT A VALID GstWebRTCICE");
  gst_println("CustomICEAgent GType: %s (parent: %s)", g_type_name(G_OBJECT_TYPE(agent)), g_type_name(g_type_parent(G_OBJECT_TYPE(agent))));

  if (network_interface)
  {
    agent->network_interface = g_strdup(network_interface);

    // Get the internal NiceAgent from GstWebRTCNice
    GObject *nice_agent_obj = NULL;
    g_object_get(agent->nice_agent, "agent", &nice_agent_obj, NULL);

    if (nice_agent_obj)
    {
      // Parse network_interface for IP address (format: "interface:ip" or just "interface")
      gchar *interface_name = g_strdup(network_interface);
      gchar *ip_address = NULL;
      gchar *colon = strchr(interface_name, ':');

      if (colon)
      {
        *colon = '\0';           // Terminate interface name at colon
        ip_address = colon + 1;  // IP address follows the colon
      }

      // If IP address is specified, use nice_agent_add_local_address() to restrict to that IP
      if (ip_address && strlen(ip_address) > 0)
      {
        NiceAgent *nice_agent = NICE_AGENT(nice_agent_obj);
        NiceAddress addr;

        if (nice_address_set_from_string(&addr, ip_address))
        {
          if (nice_agent_add_local_address(nice_agent, &addr))
          {
            gst_println("CustomICEAgent: Successfully restricted to local address: %s", ip_address);
          }
          else
          {
            gst_printerrln("CustomICEAgent: Failed to add local address: %s", ip_address);
          }
        }
        else
        {
          gst_printerrln("CustomICEAgent: Invalid IP address format: %s", ip_address);
        }
      }

      // Also set the interfaces property as fallback
      GParamSpec *pspec = g_object_class_find_property(G_OBJECT_GET_CLASS(nice_agent_obj), "interfaces");

      if (pspec)
      {
        GSList *interfaces = NULL;
        interfaces = g_slist_append(interfaces, g_strdup(interface_name));
        g_object_set(nice_agent_obj, "interfaces", interfaces, NULL);
        g_slist_free_full(interfaces, g_free);
        gst_println("CustomICEAgent: Restricting to network interface: %s\n", interface_name);
      }
      else
      {
        gst_printerrln(
            "CustomICEAgent: 'interfaces' property not available on NiceAgent. "
            "Network interface restriction may not work. Consider upgrading libnice or using "
            "system-level network configuration.");
      }

      g_free(interface_name);
      g_object_unref(nice_agent_obj);
    }
  }

  return agent;
}
