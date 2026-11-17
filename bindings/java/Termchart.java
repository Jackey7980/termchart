import java.nio.charset.StandardCharsets;

/**
 * termchart 高层出图桥（基于 termchart_capi C ABI）。
 *
 * <p>与 plugins/gui_jni 的分工：gui_jni 是"把图画进你给的 int[] 窗口表面"的 GUI 注入后端；
 * 本类是"给数据拿 ANSI / SVG 字符串"的通用出图入口——两者都通过 JNI 加载各自的 native 库，
 * 互不依赖。native 库名：termchart_jni。</p>
 *
 * <p>错误一律以 TermchartException 抛出（native 侧读 tc_error_message），绝不静默降级。</p>
 */
public final class Termchart {
    static {
        System.loadLibrary("termchart_jni");
    }

    /** 库版本字符串（如 "1.0.0"）。 */
    public static native String version();

    /** 幂等初始化；返回核心是否已初始化。 */
    public static native boolean init();

    /** 折线：给 x/y 点集，返回 ANSI 真彩字符串（UTF-8/ANSI SGR）。 */
    public static native String plot(double[] x, double[] y) throws TermchartException;

    /** 散点：同 plot，点类型不同。 */
    public static native String scatter(double[] x, double[] y) throws TermchartException;

    /** 柱状：给 values（类别按序号），返回 ANSI 字符串。 */
    public static native String bar(double[] values) throws TermchartException;

    /** 柱状：带类别标签。 */
    public static native String barWithLabels(double[] values, String[] labels)
            throws TermchartException;

    /** 一段文本渲染（对齐 tc_render_line）。 */
    public static native String renderLine(String text) throws TermchartException;

    /** 查询可用 kind 名（空格分隔，字典序）。 */
    public static native String kinds() throws TermchartException;

    private Termchart() {}
}
