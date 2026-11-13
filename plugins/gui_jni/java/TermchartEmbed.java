// TermchartEmbed.java —— JNI 后端宿主示例（桌面 Java；Android 复用同一 native 库）
//
// 约定：int[] 当"窗口表面"（ARGB）。真实宿主里，Swing 用
//   BufferedImage.TYPE_INT_ARGB + setRGB + drawImage
// Android 用 Bitmap.createBitmap(pixels,...) + Canvas.drawBitmap
// main(--smoke) 不碰 AWT/Swing，纯内存验证：渲染后 int[] 必须非空。
public final class TermchartEmbed {
    static {
        System.loadLibrary("termchart_gui_jni");
    }

    private static native long nativeCreate(int[] pixels, int width, int height);
    private static native void nativeRender(long handle);
    private static native void nativeMouse(long handle, int x, int y, int button);
    private static native void nativeDestroy(long handle);

    public static void main(String[] args) {
        final int w = 320;
        final int h = 200;
        int[] pixels = new int[w * h];
        long handle = nativeCreate(pixels, w, h);
        if (handle == 0L) {
            System.err.println("jni smoke: nativeCreate failed");
            System.exit(2);
        }
        nativeRender(handle);
        int nonzero = 0;
        for (int v : pixels) {
            if (v != 0) {
                nonzero++;
            }
        }
        nativeMouse(handle, w / 2, h / 2, 0);
        nativeDestroy(handle);
        System.out.println("jni smoke: nonzero=" + nonzero + " / " + (w * h));
        if (nonzero == 0) {
            System.err.println("jni smoke: surface stayed empty");
            System.exit(1);
        }
    }
}
