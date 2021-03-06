#include "deepsortplugin_lib.h"

#include <iomanip>
#include <sys/time.h>

DeepSortPluginCtx* DeepSortPluginCtxInit(DeepSortPluginInitParams* initParams)
{
  DeepSortPluginCtx* ctx = new DeepSortPluginCtx;
  ctx->initParams = *initParams;

  // last param is num of classes; set to 1 for now
  ctx->mTracker = new tracker(
    ctx->initParams.cosDist, 100, 
    ctx->initParams.iouDist, 
    ctx->initParams.maxAlive, 3, 1.0);

  ctx->featureTensor = new FeatureTensor(ctx->initParams.frozenModel);

  // warm up tensorflow engine
  if (ctx->featureTensor) {
    gint width = 1280;
    gint height = 720;

    DETECTIONS detections;
    cv::Mat img(height, width, CV_8UC3, cv::Scalar(0,0, 100));

    DETECTION_ROW tmpRow;
    tmpRow.confidence = 0.8;
    tmpRow.tlwh = DETECTBOX(width / 6, height / 6, width / 3, height / 3);

    tmpRow.class_num = 0;
    detections.push_back(tmpRow);

    // initial timeout for 5s
    ctx->featureTensor->getRectsFeature(img, detections, 5000);
  }

  return ctx;
}

DETECTIONS convertToDetections(GstDetectionMetas* metas, gchar* objectToTrack) {

  DETECTIONS detections;
  for (guint i = 0; i < metas->detections_count; i++) {
    if (g_strcmp0(metas->detections[i].label, objectToTrack) == 0) {
      DETECTION_ROW tmpRow;
      
      tmpRow.confidence = metas->detections[i].confidence;

      tmpRow.tlwh       = DETECTBOX(
        metas->detections[i].xmin, 
        metas->detections[i].ymin, 
        metas->detections[i].xmax - metas->detections[i].xmin, 
        metas->detections[i].ymax - metas->detections[i].ymin
      );

      tmpRow.class_num  = 0;

      detections.push_back(tmpRow);
    }
  }

  return detections;
}

void DeepSortPluginProcess(DeepSortPluginCtx* ctx, const cv::Mat& image, \
  GstDetectionMetas* metas, gchar* objectToTrack, guint infTimeout) {
  DETECTIONS detections = convertToDetections(metas, objectToTrack);
  
  bool infResult = \
    ctx->featureTensor->getRectsFeature(image, detections, infTimeout);
  
  if (infResult) {
    ctx->mTracker->predict();
    ctx->mTracker->update(detections);
  }
}

void DeepSortPluginCtxDeinit(DeepSortPluginCtx* ctx) {
  if (ctx->featureTensor)
    delete ctx->featureTensor;
  
  if (ctx->mTracker)
    delete ctx->mTracker;
  
  if (ctx)
    delete ctx;
}