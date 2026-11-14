#include "ashbox_phenom.h"

#include <stddef.h>

/* Fixed 16-day permutation: every phenomenon appears once per cycle and no
 * boundary repeats the previous day's weather.
 *
 * Effects are deliberately flat daily deltas rather than percentage modifiers:
 * the settle path applies them only when a local midnight is crossed, so a
 * three-day absence lands at most two/three effects and cannot be farmed by
 * frequent short sleeps. Values stay inside the existing caps/clamps. */
static const ash_phenom_t k_phenom[ASH_PHENOM_COUNT] = {
    {0, "空明", "无增减", "守听更清楚", false, 0, 0, 0, 0},
    {1, "灰雨", "水+3 粮-1", "接雨水，少走远路", false, 3, -1, 0, 0},
    {2, "铁风", "件+2 威胁+6", "修工具，压威胁", false, 0, 0, 2, 6},
    {3, "静夜", "威胁-8", "夜里能睡稳", false, 0, 0, 0, -8},
    {4, "初雪", "水-2 粮+1", "菜地长得快", false, -2, 1, 0, 0},
    {5, "长雾", "件-1 威胁+4", "雾里容易空手", false, 0, 0, -1, 4},
    {6, "回声", "无增减", "守听有回声", false, 0, 0, 0, 0},
    {7, "热风", "水-3 威胁+5", "多备水", true, -3, 0, 0, 5},
    {8, "断电", "件-2 威胁+10", "备灯，少守听", true, 0, 0, -2, 10},
    {9, "月白", "件+1 粮+1", "夜路好走", false, 0, 1, 1, 0},
    {10, "潮涨", "水+2 粮+1", "渡口货多", false, 2, 1, 0, 0},
    {11, "风沙", "件-2 威胁-4", "关门，护零件", false, 0, 0, -2, -4},
    {12, "无声", "粮-1 威胁+2", "没人来，省着点", false, 0, -1, 0, 2},
    {13, "星落", "件+1 威胁+2", "捡星点", false, 0, 0, 1, 2},
    {14, "灰潮", "威胁+15", "加固门，别远行", true, 0, 0, 0, 15},
    {15, "暖流", "粮+2", "粮仓松口气", false, 0, 2, 0, 0},
};

/* A fixed shuffle keeps the cycle deterministic while mixing the danger days
 * apart. Every id 0..15 occurs exactly once. */
static const uint8_t k_day_order[ASH_PHENOM_COUNT] = {
    0, 4, 9, 14, 2, 6, 11, 1, 8, 15, 3, 12, 5, 10, 7, 13,
};

uint32_t ash_phenom_cycle_slot(uint32_t day_key)
{
    uint32_t d = day_key > 0 ? day_key - 1u : 0u;
    return d % ASH_PHENOM_COUNT;
}

const ash_phenom_t *ash_phenom_for_day(uint32_t day_key)
{
    return &k_phenom[k_day_order[ash_phenom_cycle_slot(day_key)]];
}

const ash_phenom_t *ash_phenom_by_id(uint8_t id)
{
    if (id >= ASH_PHENOM_COUNT) return NULL;
    return &k_phenom[id];
}
