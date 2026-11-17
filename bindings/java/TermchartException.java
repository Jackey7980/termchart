/** termchart C ABI 错误的 Java 侧表示：message 读自 tc_error_message。 */
public final class TermchartException extends Exception {
    private final int code;

    /** JNI 侧 ThrowNew 使用的构造器（只带消息；code 未知记 -1）。 */
    public TermchartException(String message) {
        this(-1, message);
    }

    public TermchartException(int code, String message) {
        super(message);
        this.code = code;
    }

    /** 对应 termchart_capi.h 的 tc_status 码（0 不会走异常）。 */
    public int code() {
        return code;
    }
}
