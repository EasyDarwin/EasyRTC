#ifndef EASYRTC_CAMERA_ERROR_HANDLER_H
#define EASYRTC_CAMERA_ERROR_HANDLER_H

#include <string>

struct MediaSession;

void notifyCameraError(MediaSession *s, int error);
std::string queryEncoderForSurfaceInput(MediaSession *s, const std::string &mime);

using ACameraDevice = struct ACameraDevice;
void onCameraDeviceError(void *context, ACameraDevice *device, int error);
void onCameraDeviceDisconnected(void *context, ACameraDevice *device);

#endif // EASYRTC_CAMERA_ERROR_HANDLER_H