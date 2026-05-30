#ifndef EASYRTC_CAMERA_ERROR_HANDLER_H
#define EASYRTC_CAMERA_ERROR_HANDLER_H

struct MediaSession;

void notifyCameraError(MediaSession *s, int error);

using ACameraDevice = struct ACameraDevice;
void onCameraDeviceError(void *context, ACameraDevice *device, int error);
void onCameraDeviceDisconnected(void *context, ACameraDevice *device);

#endif // EASYRTC_CAMERA_ERROR_HANDLER_H