#include "gstdeepsortplugin.h"
#include "gst/gsttrackedmeta.h"
#include "gst/gstdetectionsmeta.h"
#include <fstream>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string.h>
#include <string>
#include <sys/time.h>
GST_DEBUG_CATEGORY_STATIC (gst_deepsortplugin_debug);
#define GST_CAT_DEFAULT gst_deepsortplugin_debug

/* Enum to identify properties */
enum
{
  PROP_0,
  PROP_UNIQUE_ID,
  PROP_MAX_ALIVE,
  PROP_COS_DIST,
  PROP_IOU_DIST,
  PROP_TO_TRACK,
  PROP_FROZEN_MODEL,
  PROP_SKIP_FRAME_INTERVAL,
  PROP_DRAW_RESULTS_ON_FRAME,
  PROP_MAX_TIME_SINCE_UPDATE
};

/* Default values for properties */
#define DEFAULT_UNIQUE_ID 15
#define DEFAULT_EMPTY_STRING ""
#define DEFAULT_COS_DIST 0.3
#define DEFAULT_IOU_DIST 0.9
#define DEFAULT_MAX_ALIVE 120
#define DEFAULT_MAX_TIME_SINCE_UPDATE 1

static GstStaticPadTemplate gst_deepsortplugin_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw,format=BGR"));

static GstStaticPadTemplate gst_deepsortplugin_src_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw,format=BGR"));

/* Define our element type. Standard GObject/GStreamer boilerplate stuff */
#define gst_deepsortplugin_parent_class parent_class
G_DEFINE_TYPE (GstDeepSortPlugin, gst_deepsortplugin, GST_TYPE_BASE_TRANSFORM);

static void gst_deepsortplugin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_deepsortplugin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_deepsortplugin_set_caps (GstBaseTransform * btrans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_deepsortplugin_start (GstBaseTransform * btrans);
static gboolean gst_deepsortplugin_stop (GstBaseTransform * btrans);

static GstFlowReturn gst_deepsortplugin_transform_ip (GstBaseTransform * btrans,
    GstBuffer * inbuf);

/* Install properties, set sink and src pad capabilities, override the required
 * functions of the base class, These are common to all instances of the
 * element.
 */
static void
gst_deepsortplugin_class_init (GstDeepSortPluginClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *gstbasetransform_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;

  /* Overide base class functions */
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_deepsortplugin_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_deepsortplugin_get_property);

  gstbasetransform_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_deepsortplugin_set_caps);
  gstbasetransform_class->start = GST_DEBUG_FUNCPTR (gst_deepsortplugin_start);
  gstbasetransform_class->stop = GST_DEBUG_FUNCPTR (gst_deepsortplugin_stop);

  gstbasetransform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_deepsortplugin_transform_ip);

  /* Install properties */
  g_object_class_install_property (gobject_class, PROP_UNIQUE_ID,
      g_param_spec_uint ("unique-id", "Unique ID",
          "Unique ID for the element. Can be used to identify output of the"
          " element",
          0, G_MAXUINT, DEFAULT_UNIQUE_ID,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_MAX_ALIVE,
      g_param_spec_uint ("max-alive", "Max Alive Time",
          "Max amount of time tracker is kept alive without detection in number"
          " of frames.",
          0, 300, DEFAULT_MAX_ALIVE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_COS_DIST,
      g_param_spec_float ("cos-dist", "Cosine Distance",
          "",
          0, 1.0, DEFAULT_COS_DIST,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_IOU_DIST,
      g_param_spec_float ("iou-dist", "IoU Distance",
          "",
          0, 1.0, DEFAULT_IOU_DIST,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_TO_TRACK,
      g_param_spec_string ("to-track", "What to Track",
          "class name of object to track; one for now",
          DEFAULT_EMPTY_STRING,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_FROZEN_MODEL,
      g_param_spec_string ("model", "Frozen Model Location",
          "absolute path of frozen model",
          DEFAULT_EMPTY_STRING,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_SKIP_FRAME_INTERVAL,
      g_param_spec_uint ("skip-interval", "Skip Frame Interval",
          "Skip frame every X interval.",
          0, 120, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_DRAW_RESULTS_ON_FRAME,
    g_param_spec_boolean ("draw-results", "Draw Results",
        "Draw boxes and label on current frame",
        0,
        (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_MAX_TIME_SINCE_UPDATE,
    g_param_spec_int ("since-update", "Maximum Time Since Update",
        "Number of frames without corresponding detection before tracked item"
        " is hidden from view. Tracker will still continue to live as per max-alive.",
        0, 120, DEFAULT_MAX_TIME_SINCE_UPDATE,
        (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /* Set sink and src pad capabilities */
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_deepsortplugin_src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_deepsortplugin_sink_template));

  /* Set metadata describing the element */
  gst_element_class_set_details_simple (gstelement_class, "DeepSort", "DeepSort",
      "Get object detection details from separate engine and track", "jayveeangeles");
}

static void
gst_deepsortplugin_init (GstDeepSortPlugin * deepsortplugin)
{
  GstBaseTransform *btrans = GST_BASE_TRANSFORM (deepsortplugin);

  /* We will not be generating a new buffer. Just adding / updating
   * metadata. */
  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (btrans), TRUE);
  /* We do not want to change the input caps. Set to passthrough. transform_ip
   * is still called. */
  gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (btrans), TRUE);

  /* Initialize all property variables to default values */
  deepsortplugin->unique_id = DEFAULT_UNIQUE_ID;
  deepsortplugin->frozen_model = g_strdup (DEFAULT_EMPTY_STRING);
  deepsortplugin->to_track = g_strdup (DEFAULT_EMPTY_STRING);
  deepsortplugin->max_alive = DEFAULT_MAX_ALIVE;
  deepsortplugin->cos_dist = DEFAULT_COS_DIST;
  deepsortplugin->iou_dist = DEFAULT_IOU_DIST;
  deepsortplugin->draw_results = 0;
  deepsortplugin->skip_interval = 0;
  deepsortplugin->max_since_update = DEFAULT_MAX_TIME_SINCE_UPDATE;
}

/* Function called when a property of the element is set. Standard boilerplate.
 */
static void
gst_deepsortplugin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDeepSortPlugin *deepsortplugin = GST_DEEPSORTPLUGIN (object);
  switch (prop_id) {
    case PROP_UNIQUE_ID:
      deepsortplugin->unique_id = g_value_get_uint (value);
      break;
    case PROP_TO_TRACK:
      if (g_value_get_string (value)) {
        g_free (deepsortplugin->to_track);
        deepsortplugin->to_track = g_value_dup_string (value);
      }
      break;
    case PROP_FROZEN_MODEL:
      if (g_value_get_string (value)) {
        g_free (deepsortplugin->frozen_model);
        deepsortplugin->frozen_model = g_value_dup_string (value);
      }
      break;
    case PROP_MAX_ALIVE:
      deepsortplugin->max_alive = g_value_get_uint (value);
      break;
    case PROP_COS_DIST:
      deepsortplugin->cos_dist = g_value_get_float (value);
      break;
    case PROP_IOU_DIST:
      deepsortplugin->iou_dist = g_value_get_float (value);
      break;
    case PROP_SKIP_FRAME_INTERVAL:
      deepsortplugin->skip_interval = g_value_get_uint (value);
      break;
    case PROP_DRAW_RESULTS_ON_FRAME:
      deepsortplugin->draw_results = g_value_get_boolean (value);
      break;
    case PROP_MAX_TIME_SINCE_UPDATE:
      deepsortplugin->max_since_update = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Function called when a property of the element is requested. Standard
 * boilerplate.
 */
static void
gst_deepsortplugin_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstDeepSortPlugin *deepsortplugin = GST_DEEPSORTPLUGIN (object);

  switch (prop_id) {
    case PROP_UNIQUE_ID:
      g_value_set_uint (value, deepsortplugin->unique_id);
      break;
    case PROP_TO_TRACK:
      g_value_set_string (value, deepsortplugin->to_track);
      break;
    case PROP_FROZEN_MODEL:
      g_value_set_string (value, deepsortplugin->frozen_model);
      break;
    case PROP_MAX_ALIVE:
      g_value_set_uint (value, deepsortplugin->max_alive);
      break;
    case PROP_COS_DIST:
      g_value_set_float (value, deepsortplugin->cos_dist);
      break;
    case PROP_IOU_DIST:
      g_value_set_float (value, deepsortplugin->iou_dist);
      break;
    case PROP_SKIP_FRAME_INTERVAL:
      g_value_set_uint (value, deepsortplugin->skip_interval);
      break;
    case PROP_DRAW_RESULTS_ON_FRAME:
      g_value_set_boolean (value, deepsortplugin->draw_results);
      break;
    case PROP_MAX_TIME_SINCE_UPDATE:
      g_value_set_int (value, deepsortplugin->max_since_update);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * Initialize all resources and start the output thread
 */
static gboolean
gst_deepsortplugin_start (GstBaseTransform * btrans)
{
  GstDeepSortPlugin *deepsortplugin = GST_DEEPSORTPLUGIN (btrans);
  DeepSortPluginInitParams init_params = { 
    deepsortplugin->frozen_model, 
    deepsortplugin->iou_dist,
    deepsortplugin->cos_dist,
    deepsortplugin->max_alive
  };

  if ((!deepsortplugin->to_track)
      || (strlen (deepsortplugin->to_track) == 0)) {
    GST_ERROR_OBJECT (deepsortplugin, "ERROR: to_track type not set \n");
    goto error;
  }

  if ((!deepsortplugin->frozen_model)
      || (strlen (deepsortplugin->frozen_model) == 0)) {
    GST_ERROR_OBJECT (deepsortplugin, "ERROR: model path not set \n");
    goto error;
  }

  /* Algorithm specific initializations and resource allocation. */
  deepsortplugin->deepsortpluginlib_ctx = DeepSortPluginCtxInit (&init_params);

  g_assert (deepsortplugin->deepsortpluginlib_ctx
      && "Unable to create DeepSort plugin lib ctx \n ");
  GST_DEBUG_OBJECT (deepsortplugin, "ctx lib %p \n", deepsortplugin->deepsortpluginlib_ctx);

  return TRUE;
error:
  if (deepsortplugin->deepsortpluginlib_ctx)
    DeepSortPluginCtxDeinit (deepsortplugin->deepsortpluginlib_ctx);
  return FALSE;
}

/**
 * Stop the output thread and free up all the resources
 */
static gboolean
gst_deepsortplugin_stop (GstBaseTransform * btrans)
{
  GstDeepSortPlugin *deepsortplugin = GST_DEEPSORTPLUGIN (btrans);

  GST_INFO_OBJECT (deepsortplugin,
    "%26s %.2fus", "Infer time (average):", \
      deepsortplugin->infer_time / deepsortplugin->frame_num);

  // Deinit the algorithm library
  DeepSortPluginCtxDeinit (deepsortplugin->deepsortpluginlib_ctx);
  GST_INFO_OBJECT (deepsortplugin, "ctx lib released \n");

  return TRUE;
}

/**
 * Called when source / sink pad capabilities have been negotiated.
 */
static gboolean
gst_deepsortplugin_set_caps (GstBaseTransform * btrans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstDeepSortPlugin *deepsortplugin = GST_DEEPSORTPLUGIN (btrans);

  /* Save the input video information, since this will be required later. */
  gst_video_info_from_caps (&deepsortplugin->video_info, incaps);

  return TRUE;

error:
  return FALSE;
}

/**
 * Called when element recieves an input buffer from upstream element.
 */
static GstFlowReturn
gst_deepsortplugin_transform_ip (GstBaseTransform * btrans, GstBuffer * inbuf)
{
  GstDeepSortPlugin *deepsortplugin = GST_DEEPSORTPLUGIN (btrans);
  GstMapInfo in_map_info;
  GstFlowReturn flow_ret = GST_FLOW_OK;

  GstTrackedMetas *tracked_metas = GST_TRACKEDMETAS_ADD (inbuf);
  tracked_metas->tracked_count = 0;
  
  bool doSkipFrames = deepsortplugin->skip_interval > 0 ? true : false;

  deepsortplugin->frame_num++;

  if ((doSkipFrames) && \
    ((deepsortplugin->frame_num % deepsortplugin->skip_interval) == 0)) {

    GST_DEBUG_OBJECT (deepsortplugin,
      "skipping frame %d due to skip interval[%d] setting\n", \
      deepsortplugin->frame_num, deepsortplugin->skip_interval);
    return flow_ret;
  }

  memset (&in_map_info, 0, sizeof (in_map_info));
  if (!gst_buffer_map (inbuf, &in_map_info, GST_MAP_READ)) {
    GST_ERROR_OBJECT (deepsortplugin, "Error: Failed to map gst buffer\n");
    return GST_FLOW_ERROR;
  }

  g_assert (in_map_info.size == (
    deepsortplugin->video_info.height *
    deepsortplugin->video_info.width * 3
  )
      && "buffer received does not match video resolution \n ");

  GST_DEBUG_OBJECT (deepsortplugin,
      "Processing Frame %" G_GUINT64_FORMAT "\n",
      deepsortplugin->frame_num);
      
  cv::Mat img (
    deepsortplugin->video_info.height, 
    deepsortplugin->video_info.width, CV_8UC3, in_map_info.data
  );

  GstDetectionMetas *det_metas = GST_DETECTIONMETAS_GET (inbuf);
  
  deepsortplugin->timer.start();
  DeepSortPluginProcess(deepsortplugin->deepsortpluginlib_ctx, img, det_metas, deepsortplugin->to_track);
  double infer_elapsed_time = deepsortplugin->timer.stop();
  deepsortplugin->infer_time += infer_elapsed_time;
  GST_DEBUG_OBJECT (deepsortplugin,
    "%16s %.2fus", "Inference time:", infer_elapsed_time);

  gchar id_n_label[64];
  gchar trunc_id[6];

  for (auto& track : deepsortplugin->deepsortpluginlib_ctx->mTracker->tracks) {
    if (!track.is_confirmed() || \
      track.time_since_update > deepsortplugin->max_since_update) continue;

    GstTrackedMeta *meta = &tracked_metas->tracked[tracked_metas->tracked_count++];

    DETECTBOX tmp = track.to_tlwh();

    meta->confidence = 0.8;
    meta->label = g_strdup(deepsortplugin->to_track);
    meta->id = g_strdup(track.uuid_id.c_str());

    float xmin = tmp(0) >= 0 ? tmp(0) : 0.0;
    float ymin = tmp(1) >= 0 ? tmp(1) : 0.0;

    meta->xmin = static_cast<guint>(xmin);
    meta->ymin = static_cast<guint>(ymin);
    meta->xmax = static_cast<guint>(xmin + tmp(2));
    meta->ymax = static_cast<guint>(ymin + tmp(3));

    if (deepsortplugin->draw_results) {
      g_utf8_strncpy(trunc_id, meta->id, 6);
      sprintf(id_n_label, "%s (%s)", trunc_id, meta->label);

      cv::rectangle(img, cv::Point(meta->xmin, meta->ymin), \
        cv::Point(meta->xmax, meta->ymax), cv::Scalar(0, 0, 255), 3);
      cv::putText(img, id_n_label, cv::Point(meta->xmin, meta->ymin), \
        cv::FONT_HERSHEY_DUPLEX, 1.0, cv::Scalar(0, 255, 0), 2);
    }
  }

  gst_buffer_unmap (inbuf, &in_map_info);
  return flow_ret;
}

/**
 * Boiler plate for registering a plugin and an element.
 */
static gboolean
deepsortplugin_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_deepsortplugin_debug, "deepsort", 0, "deepsort plugin");

  return gst_element_register (plugin, "deepsort", GST_RANK_PRIMARY,
      GST_TYPE_DEEPSORTPLUGIN);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, deepsortplugin,
    DESCRIPTION, deepsortplugin_plugin_init, VERSION, LICENSE, BINARY_PACKAGE, URL)
