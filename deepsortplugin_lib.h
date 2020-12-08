#ifndef __CAFFEPLUGIN_LIB__
#define __CAFFEPLUGIN_LIB__

#include <glib.h>
#include "gstdetectionsmeta.h"
#include "utility.h"
#include "tracker.h"
#include "FeatureTensor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DeepSortPluginCtx DeepSortPluginCtx;

typedef struct
{
  std::string frozenModel;
  float iouDist;
  float cosDist;
  unsigned int maxAlive;

} DeepSortPluginInitParams;

struct DeepSortPluginCtx
{
  DeepSortPluginInitParams initParams;
  FeatureTensor* featureTensor;
  tracker* mTracker;

  // perf vars
  uint64_t imageCount = 0;
};

// Initialize library context
DeepSortPluginCtx* DeepSortPluginCtxInit(DeepSortPluginInitParams*);

// Helper
DETECTIONS convertToDetections( GstDetectionMetas*, gchar* );

// Process Detections
void DeepSortPluginProcess(DeepSortPluginCtx*, const cv::Mat&, GstDetectionMetas*, gchar*);

// Deinitialize library context
void DeepSortPluginCtxDeinit(DeepSortPluginCtx* ctx);

#ifdef __cplusplus
}
#endif

#endif