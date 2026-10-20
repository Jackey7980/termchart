// 74_hypercube_4d.cpp —— 4D 超立方体：绕 XW/YZ 平面自转，两阶段投影到终端
//
// 演示 chart4d + math4d 的完整 4D→2D 链：
//   16 个 4D 顶点（±1,±1,±1,±1）/ 32 条边（4-正则超立方体）
//     → Projector4D 两阶段投影（Vec4 --旋转+透视除W--> Vec3 --> 内层 3D 相机 --> 画布）
//     → wire4d 发射成 scene::Scene（边按深度着色 + 顶点方块）
//     → canvas::render_scene 走 braille 光栅器落到画布
//     → app::HeadlessSession::frame() 出**差分增量帧**（首帧全量，之后只重画变化格）
//   共 60 帧，绕 XW 与 YZ 两个旋转平面同时自动旋转。
//
// 与 26_realtime_stream 同属"注入确定时钟"的家族：这里帧角 = 帧号 × 定步长，
// 不依赖真实时间、不 sleep，因此 headless/CI 下输出确定、跑完即退（可整段重定向
// 到文件逐帧复盘）。想在真终端里"看见"旋转，可自行在写帧之间加节流。
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/app/headless.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/scene_renderer.hpp"
#include "termchart/chart3d/common.hpp"     // chart3d::make_camera
#include "termchart/chart3d/projector.hpp"  // chart3d::Projector3D
#include "termchart/chart4d/projector4d.hpp"
#include "termchart/chart4d/wire.hpp"
#include "termchart/math3d/vec3.hpp"
#include "termchart/math4d/camera4.hpp"
#include "termchart/math4d/mat5.hpp"
#include "termchart/math4d/vec4.hpp"

namespace {
using namespace termchart;

constexpr std::uint32_t kCols = 90;
constexpr std::uint32_t kRows = 34;
constexpr int kFrames = 60;
constexpr double kStepXW = 0.11;  // XW 平面每帧角增量（弧度）
constexpr double kStepYZ = 0.07;  // YZ 平面每帧角增量（弧度）

// 16 个超立方体顶点：4 个坐标各取 ±1。
std::vector<math4d::Vec4> hypercube_vertices() {
    std::vector<math4d::Vec4> v;
    v.reserve(16);
    for (int i = 0; i < 16; ++i) {
        v.push_back(math4d::Vec4{(i & 1) ? 1.0 : -1.0, (i & 2) ? 1.0 : -1.0,
                                 (i & 4) ? 1.0 : -1.0, (i & 8) ? 1.0 : -1.0});
    }
    return v;
}

// 32 条边：仅连二进制位相差 1 的顶点对（4-正则，16×4/2=32）。
std::vector<chart4d::Edge> hypercube_edges() {
    std::vector<chart4d::Edge> e;
    for (int i = 0; i < 16; ++i) {
        for (int b = 0; b < 4; ++b) {
            const int j = i ^ (1 << b);
            if (j > i) {
                e.push_back({static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(j)});
            }
        }
    }
    return e;
}
}  // namespace

int main() {
    const auto verts = hypercube_vertices();
    const auto edges = hypercube_edges();

    // 阶段二：内层 3D 轨道相机（视角固定，半径容得下 4D 投影后的 3D 形体）。
    const math3d::Camera cam = chart3d::make_camera(
        static_cast<double>(kCols), static_cast<double>(kRows),
        /*azimuth*/ 30.0, /*elevation*/ 20.0, /*fovy*/ 55.0, math3d::Vec3{0.0, 0.0, 0.0},
        /*radius*/ 2.2);
    const auto inner = std::make_shared<chart3d::Projector3D>(cam);

    // 阶段一：4D 相机架在 +W 轴，~60° 视场推焦距。
    math4d::Camera4 cam4;
    cam4.distance = 3.0;
    cam4.focal = math4d::focal_from_fov_deg(60.0);

    chart4d::Wire4dOpts wo;
    wo.cols = static_cast<double>(kCols);
    wo.rows = static_cast<double>(kRows);
    wo.point_size = 0.9;
    wo.draw_vertices = true;

    app::HeadlessSession session(kCols, kRows);  // 差分重绘会话（保留 prev）
    bool ok = true;

    for (int f = 0; f < kFrames; ++f) {
        // 帧角 = 帧号 × 定步长：确定性，无真实时钟依赖。
        const math4d::Mat5 rot =
            math4d::rotate_xw(kStepXW * static_cast<double>(f)) *
            math4d::rotate_yz(kStepYZ * static_cast<double>(f));

        const chart4d::Projector4D projector(rot, cam4, inner);
        const scene::Scene sc = chart4d::wire4d(verts, edges, wo, projector);

        session.canvas().clear();
        canvas::render_scene(sc, session.canvas());
        const std::string bytes = session.frame();  // 差分增量帧（首帧全量）
        if (!bytes.empty() && api::write_stdout(bytes) != ErrorCode::Ok) {
            ok = false;
        }
    }
    (void)api::flush_stdout();

    char tail[160] = {};
    std::snprintf(tail, sizeof(tail),
                  "\nhypercube 4d: %d frames, 16 verts, 32 edges, XW+YZ rotation "
                  "(diff-redraw via HeadlessSession)\n",
                  kFrames);
    if (api::write_stdout(tail) != ErrorCode::Ok) {
        ok = false;
    }
    (void)api::flush_stdout();
    return ok ? 0 : 1;
}
