// autoviz/autoviz.hpp —— AutoViz 伞形头（L21）
//
// "脏数据 → 选对图"的一站式入口。用法（新手路径，零配置）：
//
//   #include "termchart/autoviz/autoviz.hpp"
//   const auto result = termchart::autoviz::auto_visualize_jsonl(text);
//   if (auto chart = result->build_chosen()) {
//       termchart::api::print_chart(*chart);   // 渲染交给 api 薄壳
//   }
//
// 分层：本模块只回答"画什么、为什么"（探测/路由/否决/打分），不回答"怎么写到
// 终端"——那是 api（L21 同级，刻意不互相依赖）与 gui 的事。
#ifndef TERMCHART_AUTOVIZ_AUTOVIZ_HPP
#define TERMCHART_AUTOVIZ_AUTOVIZ_HPP

#include "termchart/autoviz/candidate.hpp"
#include "termchart/autoviz/engine.hpp"
#include "termchart/autoviz/features.hpp"
#include "termchart/autoviz/options.hpp"
#include "termchart/autoviz/router.hpp"
#include "termchart/autoviz/score.hpp"
#include "termchart/autoviz/semantic.hpp"
#include "termchart/autoviz/time_parse.hpp"

#endif  // TERMCHART_AUTOVIZ_AUTOVIZ_HPP
