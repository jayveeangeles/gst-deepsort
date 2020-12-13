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

class Stopwatch {
private:
  std::chrono::time_point<std::chrono::steady_clock> _start, _end;
public:
  Stopwatch() { start(); }
  void start() { _start = _end = std::chrono::steady_clock::now(); }
  double stop() { _end = std::chrono::steady_clock::now(); return elapsed();}
  double elapsed() { 
      auto delta = std::chrono::duration_cast<std::chrono::microseconds>(_end-_start);
      return delta.count(); 
  }
};

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
};

// Initialize library context
DeepSortPluginCtx* DeepSortPluginCtxInit(DeepSortPluginInitParams*);

// Helper
DETECTIONS convertToDetections( GstDetectionMetas*, gchar* );

// Deinitialize library context
void DeepSortPluginCtxDeinit(DeepSortPluginCtx* ctx);

#ifdef __cplusplus
}
#endif

#endif