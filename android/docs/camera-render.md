cameraOesTex openGL的 input texture
        glGenTextures(1, &s->cameraOesTex);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, s->cameraOesTex);


cameraInputSurfaceTexture & cameraInputWindow 
    camera 的 output.





    现在有个致命的问题，当通话过程中，switchCamera 之后，发送给远端的视频画面依然是之前的 camera 的，最后一帧。 即
  renderThread 里面读取的仍然是老的 camera 的 frame。 但是 本地preview的确实是新 camera 的了。 