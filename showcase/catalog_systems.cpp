// showcase/catalog_systems.cpp —— 后端 / 协议 / 交互 / 图像 / 核心 全功能 catalog。
// 这些能力多为"跑一次 API 并展示结果指标"（字节数/魔数/状态），用文本面板呈现；
// 少数（十字准线/提示框/高亮）直接画进 canvas。
#include <cstdint>
#include <string>
#include <vector>

#include "catalog.hpp"

#include "termchart/backend/vector/pdf_backend.hpp"  // PdfBackend/render_pdf（termchart_full 未含）

namespace cat {
namespace {

const Color kB = Color::rgb(90, 100, 120);
const Color kT = Color::rgb(120, 210, 255);

void put_lines(CellBuffer& b, const std::vector<std::string>& lines) {
    Style st;
    st.set_fg(Color::rgb(200, 210, 225));
    std::uint32_t y = 0;
    for (const std::string& s : lines) {
        if (y + 1 >= b.height()) break;
        draw_text(b, 0, y, s, st);
        ++y;
    }
}

CellBuffer tiny_buf() {
    CellBuffer b(14, 4);
    b.at(2, 1).cp = U'H';
    b.at(2, 1).style.set_fg(Color::ansi16(2)).set_attr(tc::Attr::Bold);
    b.at(4, 1).cp = U'i';
    b.at(4, 1).style.set_fg(Color::rgb(255, 80, 80));
    return b;
}
tc::scene::Scene tiny_scene() {
    tc::scene::Scene s;
    tc::scene::SceneBuilder b(s);
    b.move_to(0, 0).line_to(30, 16).stroke(Color::rgb(255, 60, 120), 2.0);
    b.text(tc::Point{2.0, 2.0}, "scene", Style{}.set_fg(Color::rgb(200, 200, 220)));
    return s;
}
std::string head_of(const std::string& s, std::size_t n) {
    std::string out;
    for (std::size_t i = 0; i < s.size() && i < n; ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x20 || c > 0x7e) {
            out += "\\x";
            const char* hex = "0123456789abcdef";
            out += hex[(c >> 4) & 0xF];
            out += hex[c & 0xF];
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

void register_systems_impl(std::vector<Demo>& d) {
    auto add = [&](const char* c, const char* n, std::function<CellBuffer(std::uint32_t, std::uint32_t)> f) {
        d.push_back(Demo{c, n, std::move(f)});
    };
    // 文本结果面板
    auto sys = [&](const char* c, const char* n, const char* t, auto body) {
        add(c, n, [t, body](std::uint32_t w, std::uint32_t h) {
            return cat::panel_buf(w, h, t, [&body](CellBuffer& inner) { body(inner); }, kB, kT);
        });
    };
    // canvas 结果面板
    auto cvsys = [&](const char* c, const char* n, const char* t, auto body) {
        add(c, n, [t, body](std::uint32_t w, std::uint32_t h) {
            return cat::panel(w, h, t, [&body](tc::canvas::Canvas& cv) { body(cv); }, kB, kT);
        });
    };

    // ═══ 后端 · 快照（输入 CellBuffer）═══════════════════════════════════
    sys("后端·快照", "ANSI", "AnsiBackend", [](CellBuffer& o) {
        const CellBuffer b = tiny_buf();
        tc::backend::snapshot::AnsiBackend be;
        be.set_options(tc::backend::snapshot::SnapshotOptions{tc::ColorDepth::TrueColor, true});
        const std::string s = tc::backend::snapshot::render_snapshot(be, b);
        put_lines(o, {std::to_string(s.size()) + " bytes", "head: " + head_of(s, 16)});
    });
    sys("后端·快照", "纯文本", "TextBackend", [](CellBuffer& o) {
        const CellBuffer b = tiny_buf();
        tc::backend::snapshot::TextBackend be;
        const std::string s = tc::backend::snapshot::render_snapshot(be, b);
        std::string first;
        for (char c : s) {
            if (c == '\n') break;
            first += c;
        }
        put_lines(o, {std::to_string(s.size()) + " bytes", "row0: [" + first + "]"});
    });
    sys("后端·快照", "HTML<pre>", "HtmlPreBackend", [](CellBuffer& o) {
        const CellBuffer b = tiny_buf();
        tc::backend::snapshot::HtmlPreBackend be;
        const std::string s = tc::backend::snapshot::render_snapshot(be, b);
        const bool has = s.find("<pre") != std::string::npos || s.find("<span") != std::string::npos;
        put_lines(o, {std::to_string(s.size()) + " bytes", has ? "含 <pre>/<span> ✓" : "无标记?"});
    });
    sys("后端·快照", "JSON", "JsonBackend", [](CellBuffer& o) {
        const CellBuffer b = tiny_buf();
        tc::backend::snapshot::JsonBackend be;
        const std::string s = tc::backend::snapshot::render_snapshot(be, b);
        put_lines(o, {std::to_string(s.size()) + " bytes", "json: " + head_of(s, 20)});
    });
    sys("后端·快照", "PNG 截图", "render_png_screenshot", [](CellBuffer& o) {
        const CellBuffer b = tiny_buf();
        const std::string s = tc::backend::snapshot::render_png_screenshot(b, 8, 16);
        const bool png = s.size() >= 4 && static_cast<unsigned char>(s[0]) == 0x89 &&
                         static_cast<unsigned char>(s[1]) == 'P';
        put_lines(o, {std::to_string(s.size()) + " bytes", png ? "PNG magic ✓" : "非 PNG?"});
    });

    // ═══ 后端 · 矢量 / LaTeX / 结构化 ════════════════════════════════════
    sys("后端·矢量", "SVG", "SvgBackend", [](CellBuffer& o) {
        const tc::scene::Scene s = tiny_scene();
        tc::backend::vector::SvgBackend svg;
        svg.set_options(tc::backend::vector::VectorOptions{});
        const std::string out = tc::backend::vector::render_vector(svg, s);
        const bool ok = out.find("<svg") != std::string::npos;
        put_lines(o, {std::to_string(out.size()) + " bytes", ok ? "含 <svg> ✓" : "无 <svg>?"});
    });
    sys("后端·矢量", "PDF", "PdfBackend", [](CellBuffer& o) {
        const tc::scene::Scene s = tiny_scene();
        tc::backend::vector::PdfBackend pdf;
        pdf.set_options(tc::backend::vector::PdfOptions{});
        const std::string out = tc::backend::vector::render_pdf(pdf, s);
        const bool ok = out.find("%PDF") != std::string::npos;
        put_lines(o, {std::to_string(out.size()) + " bytes", ok ? "%PDF ✓" : "无 %PDF?"});
    });
    sys("后端·矢量", "LaTeX", "LatexBackend", [](CellBuffer& o) {
        const CellBuffer b = tiny_buf();
        tc::backend::vector::LatexBackend tex;
        const std::string out = tc::backend::vector::render_latex(tex, b);
        put_lines(o, {std::to_string(out.size()) + " bytes", "head: " + head_of(out, 18)});
    });
    sys("后端·结构化", "HTML", "HtmlBackend", [](CellBuffer& o) {
        tc::document::Document doc;
        doc.add_block(tc::document::Block::heading("R", 1));
        tc::document::TableModel t;
        t.set_header({"A", "B"});
        t.add_row({"1", "2"});
        doc.add_block(tc::document::Block::table(t));
        doc.add_block(tc::document::Block::chart(tiny_scene()));
        tc::backend::structured::HtmlBackend html;
        html.set_options(tc::backend::structured::StructuredOptions{"R", "termchart"});
        const std::string out = tc::backend::structured::render_structured(html, doc);
        put_lines(o, {std::to_string(out.size()) + " bytes", "blocks=" + std::to_string(doc.size())});
    });
    sys("后端·结构化", "Markdown", "MarkdownBackend", [](CellBuffer& o) {
        tc::document::Document doc;
        doc.add_block(tc::document::Block::heading("Title", 1));
        doc.add_block(tc::document::Block::paragraph({tc::document::Span{"hi ", Style{}}}));
        tc::backend::structured::MarkdownBackend md;
        const std::string out = tc::backend::structured::render_structured(md, doc);
        const bool ok = out.find('#') != std::string::npos;
        put_lines(o, {std::to_string(out.size()) + " bytes", ok ? "含 # 标题 ✓" : "无 #?"});
    });

    // ═══ 协议 protocol ═══════════════════════════════════════════════════
    sys("协议", "能力探测+选择", "detect/pick", [](CellBuffer& o) {
        const tc::protocol::ProtocolCaps caps = tc::protocol::detect_protocol_caps();
        const tc::protocol::ProtocolKind k = tc::protocol::pick_protocol(caps);
        put_lines(o, {std::string("pick=") + tc::protocol::protocol_name(k),
                      "kitty/sixel/iterm2/tc=" + std::to_string(caps.kitty) +
                          std::to_string(caps.sixel) + std::to_string(caps.iterm2) +
                          std::to_string(caps.truecolor)});
    });
    sys("协议", "Sixel 编码", "SixelProtocol", [](CellBuffer& o) {
        tc::image::Bitmap bmp(8, 8, 4);
        bmp.fill(200, 60, 60, 255);
        tc::protocol::SixelProtocol p;
        const std::string s = p.encode(bmp);
        put_lines(o, {std::to_string(s.size()) + " bytes", "head: " + head_of(s, 12)});
    });
    sys("协议", "Kitty 编码", "KittyProtocol", [](CellBuffer& o) {
        tc::image::Bitmap bmp(8, 8, 4);
        bmp.fill(60, 160, 220, 255);
        tc::protocol::KittyProtocol p;
        const std::string s = p.encode(bmp);
        put_lines(o, {std::to_string(s.size()) + " bytes", "head: " + head_of(s, 12)});
    });
    sys("协议", "iTerm2 编码", "Iterm2Inline", [](CellBuffer& o) {
        tc::image::Bitmap bmp(8, 8, 4);
        bmp.fill(60, 200, 120, 255);
        tc::protocol::Iterm2Inline p;
        const std::string s = p.encode(bmp);
        put_lines(o, {std::to_string(s.size()) + " bytes", "head: " + head_of(s, 12)});
    });
    sys("协议", "半块编码", "HalfBlockProtocol", [](CellBuffer& o) {
        tc::image::Bitmap bmp(8, 4, 4);
        for (std::uint32_t y = 0; y < 4; ++y) {
            for (std::uint32_t x = 0; x < 8; ++x) {
                bmp.set(x, y, 0, static_cast<std::uint8_t>(x * 30));
                bmp.set(x, y, 1, static_cast<std::uint8_t>(y * 60));
                bmp.set(x, y, 2, 128);
                bmp.set(x, y, 3, 255);
            }
        }
        tc::protocol::HalfBlockProtocol p;
        const std::string s = p.encode(bmp);
        put_lines(o, {std::to_string(s.size()) + " bytes", "head: " + head_of(s, 12)});
    });

    // ═══ 交互 interaction ════════════════════════════════════════════════
    sys("交互", "输入解析", "InputParser", [](CellBuffer& o) {
        tc::interaction::InputParser parser;
        const std::string stream = "\x1b[<0;10;5M\x1b[A\r";
        const std::vector<tc::interaction::Event> evs = parser.feed(stream);
        const auto rest = parser.flush();
        std::string kinds;
        for (const auto& e : evs) kinds += std::string(tc::interaction::kind_name(tc::interaction::kind_of(e))) + " ";
        put_lines(o, {"parsed=" + std::to_string(evs.size() + rest.size()), "kinds: " + kinds});
    });
    sys("交互", "命中测试", "topmost_hit", [](CellBuffer& o) {
        std::vector<tc::chart::HitRegion> regions;
        regions.push_back(tc::chart::HitRegion::rect(tc::Rect{0, 0, 10, 10}, 7));
        regions.push_back(tc::chart::HitRegion::circle(tc::Point{5, 5}, 3.0, 9));
        const tc::chart::HitRegion* hit = tc::interaction::topmost_hit(regions, 5.0, 5.0);
        const auto all = tc::interaction::hit_payloads(regions, 5.0, 5.0);
        put_lines(o, {hit ? ("top=" + std::to_string(hit->payload())) : "top=none",
                      "under=" + std::to_string(all.size())});
    });
    sys("交互", "事件分发", "Dispatcher", [](CellBuffer& o) {
        tc::interaction::Dispatcher disp;
        int hits = 0;
        disp.subscribe(tc::interaction::EventKind::Key, [&hits](const tc::interaction::Event&) {
            ++hits;
            return true;
        });
        tc::interaction::InputParser parser;
        for (const auto& e : parser.feed("ab")) {
            (void)disp.dispatch(e);
        }
        put_lines(o, {"handlers=" + std::to_string(disp.handler_count()), "key hits=" + std::to_string(hits)});
    });
    sys("交互", "焦点链", "FocusChain", [](CellBuffer& o) {
        tc::interaction::FocusChain f;
        const std::vector<std::uint32_t> order{1, 2, 3};
        f.set(1);
        f.next(order);
        put_lines(o, {"current=" + std::to_string(f.current())});
    });
    sys("交互", "视口缩放", "Viewport", [](CellBuffer& o) {
        tc::interaction::Viewport vp(tc::interaction::ViewportOptions{tc::Rect{0, 0, 100, 100}, 1e-9, false});
        vp.zoom_at(0.5, tc::Point{50, 50});
        const tc::Rect vis = vp.visible();
        char buf[48];
        std::snprintf(buf, sizeof(buf), "vis %.0f,%.0f %.0fx%.0f", vis.x, vis.y, vis.w, vis.h);
        put_lines(o, {buf});
    });
    sys("交互", "框选", "BoxSelect", [](CellBuffer& o) {
        tc::interaction::BoxSelect sel;
        sel.begin(tc::Point{2, 2});
        sel.update(tc::Point{8, 6});
        const tc::Rect r = sel.rect();
        const std::vector<std::pair<tc::Point, std::uint32_t>> items{{tc::Point{3, 3}, 1}, {tc::Point{20, 20}, 2}};
        const auto inside = sel.payloads_inside(items);
        char buf[48];
        std::snprintf(buf, sizeof(buf), "rect %.0fx%.0f inside=%zu", r.w, r.h, inside.size());
        put_lines(o, {buf});
    });
    sys("交互", "刷选", "Brush", [](CellBuffer& o) {
        tc::interaction::Brush brush;
        brush.set_items({{tc::Point{1, 1}, 1}, {tc::Point{2, 2}, 2}, {tc::Point{9, 9}, 3}});
        brush.begin(tc::Point{1, 1});
        brush.stroke(tc::Point{2, 2});
        brush.end();
        put_lines(o, {"selected=" + std::to_string(brush.selected().size())});
    });
    sys("交互", "图例切换", "LegendToggle", [](CellBuffer& o) {
        tc::interaction::LegendToggle lt;
        lt.register_series(1, "sin");
        lt.register_series(2, "cos");
        (void)lt.click(1);
        put_lines(o, {"hidden=" + std::to_string(lt.hidden().size()), "sin visible=" + std::to_string(lt.visible(1) ? 1 : 0)});
    });
    cvsys("交互", "十字准线", "Crosshair", [](tc::canvas::Canvas& cv) {
        tc::interaction::Crosshair ch(tc::interaction::CrosshairOptions{});
        ch.draw(cv, tc::Point{10, 6}, std::string_view{"x=10 y=6"});
    });
    cvsys("交互", "提示框", "Tooltip", [](tc::canvas::Canvas& cv) {
        tc::interaction::Tooltip tip(tc::interaction::TooltipOptions{});
        tip.draw(cv, tc::Point{8, 4}, std::vector<std::string>{"值: 42", "标签: A"});
    });

    // ═══ 图像 image ══════════════════════════════════════════════════════
    sys("图像", "PNG 往返", "encode/decode_png", [](CellBuffer& o) {
        tc::image::Bitmap bmp(16, 12, 4);
        for (std::uint32_t y = 0; y < 12; ++y) {
            for (std::uint32_t x = 0; x < 16; ++x) {
                bmp.set(x, y, 0, static_cast<std::uint8_t>(x * 16));
                bmp.set(x, y, 1, static_cast<std::uint8_t>(y * 20));
                bmp.set(x, y, 2, 128);
                bmp.set(x, y, 3, 255);
            }
        }
        const std::string png = tc::image::encode_png(bmp);
        const auto r = tc::image::decode_png(png);
        put_lines(o, {"png=" + std::to_string(png.size()) + "B",
                      r ? ("decoded " + std::to_string(r->width()) + "x" + std::to_string(r->height()))
                        : "decode failed"});
    });
    sys("图像", "格式嗅探", "sniff_image_format", [](CellBuffer& o) {
        tc::image::Bitmap bmp(4, 4, 4);
        bmp.fill(1, 2, 3, 255);
        const std::string png = tc::image::encode_png(bmp);
        const tc::image::DetectedFormat f = tc::image::sniff_image_format(
            reinterpret_cast<const std::uint8_t*>(png.data()), png.size());
        put_lines(o, {std::string("format=") + tc::image::image_format_name(f)});
    });
    sys("图像", "亮度场", "luminance_field", [](CellBuffer& o) {
        tc::image::Bitmap bmp(8, 8, 4);
        for (std::uint32_t y = 0; y < 8; ++y) {
            for (std::uint32_t x = 0; x < 8; ++x) {
                const auto v = static_cast<std::uint8_t>((x + y) * 15);
                bmp.set(x, y, 0, v);
                bmp.set(x, y, 1, v);
                bmp.set(x, y, 2, v);
                bmp.set(x, y, 3, 255);
            }
        }
        const tc::image::FloatField field = tc::image::luminance_field(bmp);
        const tc::image::Bitmap bw = tc::image::field_to_bitmap(field, tc::Rgb{255, 255, 255}, tc::Rgb{0, 0, 0}, 0.5, false);
        put_lines(o, {std::string("field ") + std::to_string(field.width()) + "x" + std::to_string(field.height()),
                      "bw " + std::to_string(bw.width()) + "x" + std::to_string(bw.height())});
    });

    // ═══ 核心 core / render / term ═══════════════════════════════════════
    sys("核心", "CellBuffer", "buffer", [](CellBuffer& o) {
        CellBuffer b(20, 5);
        put_lines(o, {std::to_string(b.width()) + "x" + std::to_string(b.height()),
                      "size=" + std::to_string(b.size()), "contains(3,2)=" + std::to_string(b.contains(3, 2) ? 1 : 0)});
    });
    sys("核心", "Renderer 增量", "render vs render_full", [](CellBuffer& o) {
        CellBuffer b(24, 4);
        b.at(1, 1).cp = U'X';
        tc::render::Renderer r(tc::ColorDepth::TrueColor);
        const std::string full = r.render_full(b);
        const std::string inc = r.render(b);  // 第二帧无变化 → 增量应更小/相等
        put_lines(o, {"full=" + std::to_string(full.size()) + "B", "inc=" + std::to_string(inc.size()) + "B",
                      inc.size() <= full.size() ? "增量✓" : "增量异常"});
    });
    sys("核心", "Compositor", "composite", [](CellBuffer& o) {
        CellBuffer a(10, 3);
        CellBuffer b(6, 2);
        a.at(0, 0).cp = U'a';
        b.at(0, 0).cp = U'b';
        const std::vector<tc::render::Layer> layers{{&a, 0, 0, true}, {&b, 2, 1, true}};
        const CellBuffer out = tc::render::Compositor::composite(layers);
        put_lines(o, {std::to_string(out.width()) + "x" + std::to_string(out.height()), "2 层叠加"});
    });
    sys("核心", "wcwidth 中文宽", "display_width", [](CellBuffer& o) {
        const std::string s = "全景A";
        const int w = tc::util::display_width(s);
        put_lines(o, {"\"" + s + "\"", "display_width=" + std::to_string(w)});
    });
    sys("核心", "ColorDepth", "color_depth_count", [](CellBuffer& o) {
        put_lines(o, {"Ansi16=" + std::to_string(tc::color_depth_count(tc::ColorDepth::Ansi16)),
                      "Ansi256=" + std::to_string(tc::color_depth_count(tc::ColorDepth::Ansi256)),
                      "TrueColor=" + std::to_string(tc::color_depth_count(tc::ColorDepth::TrueColor))});
    });
    sys("核心", "终端尺寸", "size_from_env", [](CellBuffer& o) {
        const tc::term::TerminalSize sz = tc::term::size_from_env();
        put_lines(o, {std::to_string(sz.cols) + "x" + std::to_string(sz.rows), "valid=" + std::to_string(sz.valid() ? 1 : 0)});
    });
}

}  // namespace

void register_systems(std::vector<Demo>& d) { register_systems_impl(d); }

}  // namespace cat
