package cn.easyrtc;

public final class NativeLogBridge {
    static {
        System.loadLibrary("easyrtc_media");
    }

    private NativeLogBridge() {}

    public static native void dispatch(String message);
}
