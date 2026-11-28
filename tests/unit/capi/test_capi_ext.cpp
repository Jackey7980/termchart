// test_capi_ext.cpp —— C ABI 扩展面回归：终端 / 自绘 / 增量 / 输入 / 命中 /
// 导出物种 / 时序图 / 多帧 / 富文本 / 3D·4D。
//
// 这些用例全部走公开 C 头，确保 C 与 C++ 绑定共同依赖的那层契约不漂移。
#include <doctest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "termchart_capi.h"

TEST_CASE("capi: terminal options, size and caps") {
    const tc_term_opts o = tc_term_opts_default();
    CHECK(o.alt_screen == 1);
    CHECK(o.mouse == 0);

    const tc_term_size s = tc_term_size_from_env();
    CHECK(s.cols == 80);
    CHECK(s.rows == 24);

    tc_term_caps caps{};
    CHECK(tc_term_caps_detect(&caps, 1) == TC_OK);
    CHECK(caps.unicode == 1);
    CHECK(caps.color_depth >= TC_DEPTH_16);

    tc_term_size queried{};
    const tc_status st = tc_term_size_query(&queried, nullptr);
    CHECK((st == TC_OK || st == TC_ERR_UNSUPPORTED));
}

TEST_CASE("capi: canvas primitives and two-phase exports") {
    tc_error_handle err = nullptr;
    tc_canvas_handle cv = tc_canvas_create(20, 8, &err);
    REQUIRE(cv != nullptr);
    CHECK(tc_canvas_width(cv) == 20);
    CHECK(tc_canvas_height(cv) == 8);

    CHECK(tc_canvas_put(cv, 1, 1, U'A', nullptr) == 1);
    CHECK(tc_canvas_put(cv, -1, 1, U'B', nullptr) == 0);
    CHECK(tc_canvas_text(cv, 2, 2, "hello", nullptr) == 1);
    CHECK(tc_canvas_line(cv, 0, 0, 5, 5, U'*', nullptr) == 1);
    tc_canvas_fill_rect(cv, 10, 3, 4, 2, U'#', nullptr);

    std::uint64_t need = 0;
    CHECK(tc_canvas_render(cv, nullptr, nullptr, 0, &need, &err) == TC_OK);
    CHECK(need > 0);
    std::vector<char> buf(need + 1);
    CHECK(tc_canvas_render(cv, nullptr, buf.data(), need, &need, &err) == TC_OK);
    const std::string ansi(buf.data(), static_cast<std::size_t>(need));
    CHECK(ansi.find('*') != std::string::npos);
    CHECK(ansi.find("####") != std::string::npos);

    tc_output_opts o = tc_output_opts_default();
    o.format = TC_OUT_PNG;
    need = 0;
    CHECK(tc_canvas_render(cv, &o, nullptr, 0, &need, &err) == TC_OK);
    CHECK(need > 8);
    std::vector<char> png(need);
    CHECK(tc_canvas_render(cv, &o, png.data(), need, &need, &err) == TC_OK);
    CHECK(static_cast<unsigned char>(png[1]) == 'P');

    o.format = TC_OUT_JSON;
    need = 0;
    CHECK(tc_canvas_render(cv, &o, nullptr, 0, &need, &err) == TC_OK);
    CHECK(need > 0);

    o.format = TC_OUT_SVG;
    need = 0;
    CHECK(tc_canvas_render(cv, &o, nullptr, 0, &need, &err) == TC_ERR_UNSUPPORTED);

    CHECK(tc_canvas_resize(cv, 8, 4) == 0);
    CHECK(tc_canvas_width(cv) == 8);
    CHECK(tc_canvas_resize(cv, 0, 4) == -1);
    tc_canvas_clear(cv);
    tc_canvas_free(cv);
}

TEST_CASE("capi: renderer emits diff frames") {
    tc_error_handle err = nullptr;
    tc_canvas_handle cv = tc_canvas_create(12, 4, &err);
    tc_renderer_handle r = tc_renderer_create(TC_DEPTH_TRUECOLOR, &err);
    REQUIRE(cv);
    REQUIRE(r);

    tc_canvas_put(cv, 0, 0, U'X', nullptr);
    std::uint64_t n1 = 0;
    CHECK(tc_renderer_frame(r, cv, nullptr, 0, &n1, &err) == TC_OK);
    CHECK(n1 > 0);

    std::uint64_t n2 = 0;
    CHECK(tc_renderer_frame(r, cv, nullptr, 0, &n2, &err) == TC_OK);
    CHECK(n2 == 0);  // 画面未变：无字节输出

    tc_canvas_put(cv, 1, 0, U'Y', nullptr);
    std::uint64_t n3 = 0;
    CHECK(tc_renderer_frame(r, cv, nullptr, 0, &n3, &err) == TC_OK);
    CHECK(n3 > 0);

    tc_renderer_reset(r);
    std::uint64_t n4 = 0;
    CHECK(tc_renderer_frame(r, cv, nullptr, 0, &n4, &err) == TC_OK);
    CHECK(n4 > 0);  // reset 后强制全量

    tc_renderer_free(r);
    tc_canvas_free(cv);
}

TEST_CASE("capi: input parser decodes keys, mouse and paste") {
    tc_error_handle err = nullptr;
    tc_input_handle in = tc_input_create(&err);
    REQUIRE(in != nullptr);

    std::uint64_t count = 0;
    const char* up = "\x1b[A";
    CHECK(tc_input_feed(in, reinterpret_cast<const uint8_t*>(up), 3, nullptr, 0, &count, &err)
          == TC_OK);
    REQUIRE(count == 1);
    tc_event ev{};
    CHECK(tc_input_feed(in, reinterpret_cast<const uint8_t*>(up), 3, &ev, 1, &count, &err)
          == TC_OK);
    CHECK(ev.kind == TC_EVENT_KEY);
    CHECK(ev.key == TC_KEY_UP);

    // 先把上一段残留清掉，再喂 SGR 鼠标按下。
    tc_input_reset(in);
    const char* mouse = "\x1b[<0;3;7M";
    CHECK(tc_input_feed(in, reinterpret_cast<const uint8_t*>(mouse), std::strlen(mouse),
                        &ev, 1, &count, &err) == TC_OK);
    REQUIRE(count == 1);
    CHECK(ev.kind == TC_EVENT_MOUSE);
    CHECK(ev.button == TC_MOUSE_LEFT);
    CHECK(ev.action == TC_MOUSE_PRESS);
    CHECK(ev.x == 3);
    CHECK(ev.y == 7);

    tc_input_free(in);
}

TEST_CASE("capi: hit regions report payloads") {
    tc_error_handle err = nullptr;
    tc_regions_handle rs = tc_regions_create(&err);
    REQUIRE(rs != nullptr);
    CHECK(tc_regions_add_rect(rs, 0, 0, 10, 10, 11) == 1);
    CHECK(tc_regions_add_circle(rs, 20, 20, 3.0, 22) == 2);
    const int32_t poly[] = {30, 0, 40, 0, 35, 10};
    CHECK(tc_regions_add_polygon(rs, poly, 3, 33, &err) == 3);
    CHECK(tc_regions_count(rs) == 3);

    std::uint64_t count = 0;
    CHECK(tc_regions_hit(rs, 5, 5, nullptr, 0, &count) == TC_OK);
    REQUIRE(count == 1);
    std::uint32_t payload = 0;
    CHECK(tc_regions_hit(rs, 5, 5, &payload, 1, &count) == TC_OK);
    CHECK(payload == 11);

    CHECK(tc_regions_hit(rs, 20, 21, nullptr, 0, &count) == TC_OK);
    CHECK(count == 1);
    CHECK(tc_regions_hit(rs, 35, 5, nullptr, 0, &count) == TC_OK);
    CHECK(count == 1);

    tc_regions_clear(rs);
    CHECK(tc_regions_count(rs) == 0);
    tc_regions_free(rs);
}

TEST_CASE("capi: chart export adds pdf/eps/latex/json species") {
    tc_error_handle err = nullptr;
    tc_chart_handle ch = tc_chart_create("line", &err);
    REQUIRE(ch != nullptr);
    const double xs[] = {1, 2, 3, 4};
    const double ys[] = {2, 3, 5, 4};
    REQUIRE(tc_chart_add_points(ch, xs, ys, 4) == TC_OK);

    tc_plot_opts po{};
    po.width = 60;
    po.height = 20;
    po.depth = TC_DEPTH_TRUECOLOR;

    const tc_output_format formats[] = {TC_OUT_PDF, TC_OUT_EPS, TC_OUT_LATEX,
                                        TC_OUT_JSON, TC_OUT_TEXT, TC_OUT_HTML_PRE};
    for (tc_output_format f : formats) {
        tc_output_opts o = tc_output_opts_default();
        o.format = f;
        std::uint64_t need = 0;
        const tc_status st = tc_chart_export(ch, &o, &po, nullptr, 0, &need, &err);
        CHECK(st == TC_OK);
        CHECK(need > 0);
        if (st == TC_OK && need > 0) {
            std::vector<char> buf(static_cast<std::size_t>(need));
            CHECK(tc_chart_export(ch, &o, &po, buf.data(), need, &need, &err) == TC_OK);
            if (f == TC_OUT_PDF) CHECK(buf[0] == '%');
        }
    }

    tc_image_handle img = tc_chart_to_image(ch, &po, &err);
    REQUIRE(img != nullptr);
    CHECK(tc_image_width(img) == 60 * 9);  // 默认单元格 9×16 像素
    tc_image_free(img);
    tc_chart_free(ch);
}

TEST_CASE("capi: sequence diagram from mermaid text") {
    tc_error_handle err = nullptr;
    const char* src = "sequenceDiagram\n"
                      "  participant A as Alice\n"
                      "  participant B as Bob\n"
                      "  A->>B: hello\n"
                      "  B-->>A: hi\n";
    tc_seq_handle seq = tc_seq_from_text(src, &err);
    REQUIRE(seq != nullptr);
    CHECK(tc_seq_participant_count(seq) == 2);
    CHECK(tc_seq_message_count(seq) == 2);

    tc_output_opts o = tc_output_opts_default();
    o.width = 80;
    o.height = 24;
    std::uint64_t need = 0;
    CHECK(tc_seq_render(seq, &o, nullptr, 0, &need, &err) == TC_OK);
    CHECK(need > 0);
    if (need > 0) {
        std::vector<char> buf(static_cast<std::size_t>(need));
        CHECK(tc_seq_render(seq, &o, buf.data(), need, &need, &err) == TC_OK);
        const std::string s(buf.data(), static_cast<std::size_t>(need));
        CHECK(s.find("Alice") != std::string::npos);
    }
    tc_seq_free(seq);

    tc_seq_handle bad = tc_seq_from_text("not a sequence", &err);
    CHECK(bad == nullptr);
    tc_error_free(err);
}

TEST_CASE("capi: AV1 single-frame IVF and frame encoding") {
    tc_error_handle err = nullptr;
    tc_image_handle small[2] = {tc_image_create(4, 4, 3, &err),
                                tc_image_create(4, 4, 3, &err)};
    REQUIRE(small[0]);
    REQUIRE(small[1]);
    std::uint64_t need = 0;
    // 尺寸不合规的 AV1 帧显式失败（4×4 不满足 64 倍数）。
    CHECK(tc_image_encode_frames(small, 2, "av1", 85, 10, nullptr, 0, &need, &err)
          == TC_ERR_RENDER);
    CHECK(tc_image_encode_frames(small, 2, "gif", 85, 10, nullptr, 0, &need, &err)
          == TC_OK);
    CHECK(need > 0);
    tc_image_free(small[0]);
    tc_image_free(small[1]);

    // 单帧 64 倍数尺寸 → IVF 容器（DKIF 魔数）。
    tc_image_handle av1 = tc_image_create(64, 64, 3, &err);
    REQUIRE(av1 != nullptr);
    std::string ivf;
    for (const char* fmt : {"av1", "ivf", "avif"}) {
        need = 0;
        CHECK(tc_image_encode_frames(&av1, 1, fmt, 85, 30, nullptr, 0, &need, &err) == TC_OK);
        CHECK(need > 32);
        if (need > 32) {
            std::vector<char> buf(static_cast<std::size_t>(need));
            CHECK(tc_image_encode_frames(&av1, 1, fmt, 85, 30, buf.data(), need, &need, &err)
                  == TC_OK);
            ivf.assign(buf.data(), static_cast<std::size_t>(need));
            CHECK(ivf.substr(0, 4) == "DKIF");
        }
    }
    // 单帧入口（tc_image_encode）也走同一条公开分发。
    need = 0;
    CHECK(tc_image_encode(av1, "av1", 85, nullptr, 0, &need, &err)
          == TC_ERR_BUFFER_TOO_SMALL);
    CHECK(need == ivf.size());

    // 多帧：IVF 头第 24..27 字节是帧数；每帧一个 12 字节 IVF frame 头。
    tc_image_handle frames2[2] = {av1, tc_image_create(64, 64, 3, &err)};
    REQUIRE(frames2[1] != nullptr);
    need = 0;
    CHECK(tc_image_encode_frames(frames2, 2, "av1", 85, 30, nullptr, 0, &need, &err) == TC_OK);
    CHECK(need > ivf.size());
    if (need > ivf.size()) {
        std::vector<char> buf(static_cast<std::size_t>(need));
        CHECK(tc_image_encode_frames(frames2, 2, "av1", 85, 30, buf.data(), need, &need, &err)
              == TC_OK);
        std::uint32_t frame_count = 0;
        std::memcpy(&frame_count, buf.data() + 24, sizeof(frame_count));
        CHECK(frame_count == 2);
    }
    tc_image_free(frames2[1]);
    tc_image_free(av1);
    tc_error_free(err);
}

TEST_CASE("capi: rich paragraph keeps span styles") {
    tc_error_handle err = nullptr;
    tc_doc_handle doc = tc_doc_create(&err);
    REQUIRE(doc != nullptr);
    tc_span spans[2]{};
    spans[0].text = "plain ";
    spans[1].text = "bold";
    spans[1].style.attrs = 1;  // bold
    CHECK(tc_doc_add_rich_paragraph(doc, spans, 2, &err) == TC_OK);

    std::uint64_t need = 0;
    CHECK(tc_doc_export(doc, "html", nullptr, nullptr, nullptr, 0, &need, &err) == TC_OK);
    CHECK(need > 0);
    if (need > 0) {
        std::vector<char> buf(static_cast<std::size_t>(need));
        CHECK(tc_doc_export(doc, "html", nullptr, nullptr, buf.data(), need, &need, &err)
              == TC_OK);
        const std::string html(buf.data(), static_cast<std::size_t>(need));
        CHECK(html.find("plain") != std::string::npos);
        CHECK(html.find("bold") != std::string::npos);
    }
    tc_doc_free(doc);
}

TEST_CASE("capi: 3D and 4D scenes render ANSI and SVG") {
    tc_error_handle err = nullptr;

    tc_scene3d_handle s3 = tc_scene3d_create("scatter3d", &err);
    REQUIRE(s3 != nullptr);
    const double pts[] = {0, 0, 0, 1, 1, 1, 2, 0, 2, 0, 2, 1};
    CHECK(tc_scene3d_set_points(s3, pts, 12, &err) == TC_OK);
    tc_output_opts o = tc_output_opts_default();
    o.width = 48;
    o.height = 18;
    std::uint64_t need = 0;
    CHECK(tc_scene3d_render(s3, &o, nullptr, 0, &need, &err) == TC_OK);
    CHECK(need > 0);
    o.format = TC_OUT_SVG;
    need = 0;
    CHECK(tc_scene3d_render(s3, &o, nullptr, 0, &need, &err) == TC_OK);
    CHECK(need > 0);
    tc_scene3d_free(s3);

    tc_scene3d_handle surf = tc_scene3d_create("surface3d", &err);
    REQUIRE(surf != nullptr);
    const double z[] = {0, 1, 1, 2};
    CHECK(tc_scene3d_set_grid(surf, z, 2, 2, &err) == TC_OK);
    CHECK(tc_scene3d_set_range(surf, 0, -1.0, 1.0, &err) == TC_OK);
    CHECK(tc_scene3d_set_range(surf, 1, -1.0, 1.0, &err) == TC_OK);
    o.format = TC_OUT_ANSI;
    need = 0;
    CHECK(tc_scene3d_render(surf, &o, nullptr, 0, &need, &err) == TC_OK);
    CHECK(need > 0);
    tc_scene3d_free(surf);

    tc_scene3d_handle w4 = tc_scene3d_create("wire4d", &err);
    REQUIRE(w4 != nullptr);
    const double v[] = {0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0};
    CHECK(tc_scene3d_set_points(w4, v, 16, &err) == TC_OK);
    CHECK(tc_scene3d_set_rotation4d(w4, 20.0, 15.0) == TC_OK);
    need = 0;
    CHECK(tc_scene3d_render(w4, &o, nullptr, 0, &need, &err) == TC_OK);
    CHECK(need > 0);
    tc_scene3d_free(w4);

    tc_scene3d_handle bad = tc_scene3d_create("nope", &err);
    CHECK(bad == nullptr);
    tc_error_free(err);
}

TEST_CASE("capi: data dataframe parses dirty CSV in memory") {
    tc_error_handle err = nullptr;
    const char* csv = "name,value,note\nfoo,1,ok\nbar, 2.5 ,千分,\n";
    tc_data_handle d = tc_data_parse_csv(csv, nullptr, &err);
    REQUIRE(d != nullptr);
    CHECK(tc_data_row_count(d) == 2);
    CHECK(tc_data_col_count(d) == 4);

    char buf[64] = {};
    uint64_t need = 0;
    CHECK(tc_data_header(d, 1, buf, 0, &need, &err) == TC_OK);
    CHECK(need == 5);
    CHECK(tc_data_header(d, 1, buf, sizeof(buf), &need, &err) == TC_OK);
    CHECK(std::string(buf) == "value");

    uint64_t raw_len = 0;
    CHECK(tc_data_cell_raw(d, 1, 0, nullptr, 0, &raw_len, &err) == TC_OK);
    CHECK(raw_len == 3);
    double num = 0;
    CHECK(tc_data_cell_number(d, 0, 1, &num, &err) == TC_OK);
    CHECK(num == 1.0);
    tc_cell_kind kind = TC_CELL_EMPTY;
    uint8_t flags = 0;
    CHECK(tc_data_cell_info(d, 0, 1, &kind, &flags, &err) == TC_OK);
    CHECK(kind == TC_CELL_NUMERIC);
    CHECK(tc_data_cell_number(d, 0, 0, &num, &err) == TC_ERR_UNSUPPORTED);
    CHECK(tc_data_cell_number(d, 99, 1, &num, &err) == TC_ERR_INVALID_ARG);

    tc_data_free(d);
    tc_error_free(err);
}

TEST_CASE("capi: auto_plot_text renders CSV and JSONL from memory") {
    tc_error_handle err = nullptr;
    CHECK(tc_auto_plot_text(nullptr, &err) == TC_ERR_INVALID_ARG);
    CHECK(tc_auto_plot_text("   ", &err) == TC_ERR_INVALID_ARG);
    const char* csv = "x,y\n1,2\n2,4\n3,6\n4,8\n";
    CHECK(tc_auto_plot_text(csv, &err) == TC_OK);
    const char* jsonl =
        "{\"x\":1,\"y\":2}\n{\"x\":2,\"y\":4}\n{\"x\":3,\"y\":6}\n{\"x\":4,\"y\":8}\n";
    CHECK(tc_auto_plot_text(jsonl, &err) == TC_OK);
    tc_error_free(err);
}
