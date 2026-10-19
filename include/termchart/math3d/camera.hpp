// camera.hpp —— 轨道相机 + 透视投影到画布坐标（header-only）
//
// 设计取舍：3D 渲染**不**自造 z-buffer/终端光栅器，而是把 3D 点投影到 2D 画布坐标
// 后，作为 Scene 图元交给已有的 Scene→Canvas（braille/halfblock）管线。深度只用来
// 决定着色（近亮远暗），得到可辨识的立体感而不重复造轮子。
//
// project() 返回的屏幕坐标即画布格坐标（左上为原点，y 向下），直接喂给 Scene 的
// MoveTo/LineTo/RectOp 即可被 render_scene 消费。
#ifndef TERMCHART_MATH3D_CAMERA_HPP
#define TERMCHART_MATH3D_CAMERA_HPP

#include "termchart/math3d/mat4.hpp"
#include "termchart/math3d/vec3.hpp"

namespace termchart::math3d {

constexpr double kDeg2Rad = 0.017453292519943295769;  // π/180

struct Projected {
    double x = 0.0;   // 画布列坐标
    double y = 0.0;   // 画布行坐标
    double depth = 0.0;  // 归一化深度：0=最近，1=最远，供着色
    bool visible = false;  // 在视锥内且位于相机前方
};

struct Camera {
    Vec3 eye{0.0, 0.0, 5.0};
    Vec3 target{0.0, 0.0, 0.0};
    Vec3 up{0.0, 1.0, 0.0};
    double fovy_deg = 55.0;
    double aspect = 1.0;  // vw / vh
    double near = 0.1;
    double far = 100.0;

    // 以 target 为中心、按方位角/仰角/距离摆放 eye。
    void orbit(double azimuth_deg, double elevation_deg, double distance) noexcept {
        const double az = azimuth_deg * kDeg2Rad;
        const double el = elevation_deg * kDeg2Rad;
        const double ce = std::cos(el);
        eye = target + Vec3{ce * std::sin(az), std::sin(el), ce * std::cos(az)} * distance;
    }

    // 世界坐标 → 画布坐标。vw/vh 为画布尺寸（格）。
    Projected project(const Vec3& world, double vw, double vh) const noexcept {
        const Mat4 view = look_at(eye, target, up);
        const Mat4 proj = perspective(fovy_deg * kDeg2Rad, aspect, near, far);
        const Mat4 vp = proj * view;

        const Vec3 clip = vp.transform(world);
        const double w = vp.clip_w(world);  // 透视矩阵的 clip.w = -z_view
        if (w <= 0.0) {
            return {};  // 相机后方
        }
        const double invw = 1.0 / w;
        const double ndcx = clip.x * invw;
        const double ndcy = clip.y * invw;
        const double ndcz = clip.z * invw;
        if (ndcz < -1.0 || ndcz > 1.0) {
            return {};  // 超出近/远裁剪面
        }

        const double sx = (ndcx * 0.5 + 0.5) * vw;
        const double sy = (1.0 - (ndcy * 0.5 + 0.5)) * vh;

        const Vec3 viewp = view.transform(world);
        const double span = far - near;
        const double t = (span > 0.0) ? clamp_d(((-viewp.z) - near) / span, 0.0, 1.0) : 0.0;
        return {sx, sy, t, true};
    }

private:
    static constexpr double clamp_d(double v, double lo, double hi) noexcept {
        return v < lo ? lo : (v > hi ? hi : v);
    }
};

}  // namespace termchart::math3d

#endif  // TERMCHART_MATH3D_CAMERA_HPP
