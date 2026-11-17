// TermchartSmoke —— Java 桥无头冒烟（无 AWT/Swing，纯内存验证 native 出图链路）
public final class TermchartSmoke {
    public static void main(String[] args) throws Exception {
        final String version = Termchart.version();
        if (version == null || version.isEmpty()) {
            throw new AssertionError("empty version");
        }

        final String kinds = Termchart.kinds();
        if (kinds == null || !kinds.contains("line")) {
            throw new AssertionError("kinds missing line: " + kinds);
        }

        final double[] x = {0, 1, 2, 3, 4};
        final double[] y = {1, 3, 2, 5, 4};
        final String plot = Termchart.plot(x, y);
        if (plot == null || !plot.contains("\u001b[")) {
            throw new AssertionError("plot has no ANSI SGR");
        }

        final String bar = Termchart.bar(new double[] {3, 7, 5});
        if (bar == null || bar.isEmpty()) {
            throw new AssertionError("empty bar");
        }

        final String line = Termchart.renderLine("hello java bridge");
        if (line == null || line.isEmpty()) {
            throw new AssertionError("empty renderLine");
        }

        // 错误通道：空输入应抛 TermchartException（fail-loud，不静默返回空图）。
        boolean threw = false;
        try {
            Termchart.plot(new double[0], new double[0]);
        } catch (TermchartException e) {
            threw = true;
        }
        if (!threw) {
            throw new AssertionError("empty plot did not throw TermchartException");
        }

        System.out.println("java smoke: OK version=" + version + " kinds.len=" + kinds.length()
                + " plot.bytes=" + plot.length());
    }
}
