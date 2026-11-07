#include "ashbox_content.h"

#include <stdio.h>

static const ash_branch_def_t k_branch[] = {
    /* 0 */ {"分给站里", 1, 0, 0, -2, 255, 255, "把带回的分给站里"},
    /* 1 */ {"先藏私货", -1, 1, 0, 0, 255, 255, "有人把罐头塞进床底"},
    /* 2 */ {"修风扇", 1, 0, -1, -1, 255, 255, "风扇又转起来了"},
    /* 3 */ {"查铁皮响", 0, 0, 0, 1, 0, 0, "记下谁在敲铁皮墙"},
    /* 4 */ {"问匣里火", 0, 0, 0, 0, 1, 0, "匣里余烬跳得更勤"},
    /* 5 */ {"强撑干活", -1, 0, 1, 0, 255, 255, "带伤也拧紧了螺丝"},
    /* 6 */ {"休息养伤", 1, -1, 0, 0, 255, 255, "伤员睡到风扇停"},
    /* 7 */ {"画下路线", 0, 0, 0, 0, 0, 1, "废街路线画在铁皮上"},
    /* 8 */ {"埋掉痕迹", 0, 0, 0, -3, 0, 2, "脚印被灰盖住了"},
    /* 9 */ {"带回碎片", 0, 0, 0, 0, 1, 1, "碎片贴在匣盖内侧"},
    /* 10 */ {"把话压下", -1, 0, 0, 2, 255, 255, "没人提门外的脚步"},
    /* 11 */ {"全员戒备", 0, -1, -1, -5, 0, 3, "闸门整夜有人守"},
    /* 12 */ {"教小芒认路", 1, 0, 0, 0, 0, 4, "小芒记住了近路记号"},
    /* 13 */ {"交给老阙", 1, 0, 0, 0, 1, 2, "老阙把碎片磨亮了"},
    /* 14 */ {"独自消化", 0, 0, 0, 0, 1, 3, "匣主一夜没睡实"},
    /* 15 */ {"分享热汤", 2, -1, 0, -1, 255, 255, "食堂里有人笑了一下"},
    /* 16 */ {"囤到明天", -1, 2, 0, 1, 255, 255, "口袋鼓了却没人提"},
    /* 17 */ {"加固锁扣", 0, 0, -2, -4, 255, 255, "锁扣敲得更紧"},
    /* 18 */ {"记下怪响", 0, 0, 0, 0, 0, 0, "怪响方向写进日志"},
    /* 19 */ {"听匣里声", 0, 0, 0, 0, 1, 0, "听清匣里余烬的声音"},
    /* 20 */ {"道歉收场", 2, 0, 0, 0, 255, 255, "站里消了气"},
    /* 21 */ {"翻脸走人", -2, 0, 0, 1, 255, 255, "有人不说话了"},
    /* 22 */ {"先分热的", 0, 0, 0, 0, 255, 255, "第一次带回 · 先分热的"},
    /* 23 */ {"追查来客", 0, 0, 0, 0, 0, 4, "铁皮来客 · 了结"},
    /* 24 */ {"安放余烬", 0, 0, 0, 0, 1, 4, "余烬低语 · 了结"},
    /* 25 */ {"听门后风", 1, 0, 0, 0, 2, 0, "门后有潮声 · 记下了"},
    /* 26 */ {"回访铁棚", 1, 0, 0, 0, 2, 1, "铁棚频率对上了"},
    /* 27 */ {"换稻谷种", 0, 0, 0, 0, 2, 2, "干肉换来稻谷种"},
    /* 28 */ {"验田地收", 1, 1, 0, -1, 2, 3, "第一块田地活了"},
    /* 29 */ {"拼起灰片", 0, 0, 0, -2, 2, 4, "听清旧城熄灯原因"},
    /* 30 */ {"挂高干肉", 0, 0, 0, -2, 255, 255, "干肉挂到潮够不着"},
    /* 31 */ {"反潮戒备", 0, -1, -1, -4, 255, 255, "猎径夜里有人守"},
    /* 32 */ {"外环喘匀", 1, 0, 0, 0, 255, 255, "走了一整夜才喘匀"},
    /* 33 */ {"潮声入纪", 0, 0, 0, 0, 2, 0, "潮声写进一夜纪事"},
    /* 34 */ {"压住不提", -1, 0, 0, 2, 255, 255, "没人提外环的黑"},
    /* 35 */ {"货亭糖块", 1, -1, 0, 0, 255, 255, "货亭分了一小块糖"},
    /* 36 */ {"轴承又响", 0, 0, 1, 0, 255, 255, "轴承盒里又多一颗"},
};
#define BRANCH_N (sizeof(k_branch) / sizeof(k_branch[0]))

static const ash_outcome_def_t k_outcome[] = {
    /* 0 tutorial success */ {{22, 0, 2}, 3},
    /* 1 short ok */ {{0, 3, 35}, 3},
    /* 2 short fail */ {{6, 5, 10}, 3},
    /* 3 block ok */ {{0, 7, 26}, 3},
    /* 4 block fail */ {{6, 20, 21}, 3},
    /* 5 deep ok — 含听匣低语(19) 打开余烬线步0 */ {{19, 9, 29}, 3},
    /* 6 deep fail */ {{6, 10, 14}, 3},
    /* 7 food load ok */ {{15, 0, 16}, 3},
    /* 8 parts load ok */ {{2, 17, 36}, 3},
    /* 9 hunt ok */ {{15, 30, 31}, 3},
    /* 10 hunt fail */ {{6, 5, 10}, 3},
    /* 11 night ok */ {{32, 33, 0}, 3},
    /* 12 night fail */ {{6, 34, 11}, 3},
};
#define OUTCOME_N (sizeof(k_outcome) / sizeof(k_outcome[0]))

/* 分段：0-9 近路 · 10-19 废街 · 20-39 深/猎/夜 */
static const char *k_away[] = {
    /* 0-9 近路/货亭 */
    "听见铁皮轻响 · 先贴墙听",
    "货架下摸到未开封罐头",
    "踢到一袋受潮的米",
    "捡到两枚还能用的螺丝",
    "轴承盒里还剩三颗",
    "脚印新鲜 · 有人刚走过",
    "烟雾里闪过一点暖光",
    "雨水把纸箱泡软了",
    "怪响停了 · 继续往前",
    "包里又沉了一点",
    /* 10-19 废街 */
    "路牌掉了一半 · 靠左走",
    "发现干粮 · 先装进侧袋",
    "商街玻璃碎了一地",
    "闸门外有拖行痕迹",
    "灰里埋着半截路牌",
    "余烬在怀里热了一下",
    "空罐被人踢远了",
    "货亭顶还挂着旧灯罩",
    "风把传单糊在脸上",
    "拐角有人煮过汤 · 冷了",
    /* 20-39 深/猎/夜 */
    "旧地铁风很干 · 省着喝",
    "猎径干肉好藏",
    "灰河风里有潮",
    "铁棚那头还亮着",
    "门缝里进了灰",
    "灰被风刮进站里",
    "远处有人敲空罐",
    "脚底下的铁皮还烫",
    "背包带很紧 · 别停",
    "听见广播碎片 · 像有人唤",
    "隧道深处滴水很慢",
    "匣盖贴着胸口发暖",
    "反潮味从草里爬上来",
    "干肉绳在风里轻晃",
    "外环长道只剩脚印",
    "潮声一整夜没断",
    "星光被灰罩住了",
    "远处闸门像一条缝",
    "怀里余烬亮了一下又灭",
    "天亮前风忽然停了",
};

static const char *k_quest_name[ASHBOX_QUEST_N] = {"铁皮来客", "余烬低语", "门后灰潮"};
static const char *k_quest_hint[ASHBOX_QUEST_N][ASHBOX_QUEST_STEPS + 1] = {
    {
        "铁皮指站外铁皮墙 · 有人敲先记下来",
        "去近路或废街摸清路线",
        "把脚印埋掉 · 别被人追上",
        "全站戒备守一夜",
        "把近路记号教给小芒",
        "这条线已了结",
    },
    {
        "匣是装余烬的小盒子 · 先听听声音",
        "去深处带回碎片放进匣",
        "把碎片交给老阙打磨",
        "匣主独自守一夜",
        "把余烬在匣里放稳",
        "这条线已了结",
    },
    {
        "开闸后先听门外风声",
        "跟铁棚营地通上话",
        "用干肉换种子 · 或交两片灰片",
        "把田地种活并收一次",
        "拼齐灰片听清旧城熄灯原因",
        "了结后 · 渡口可以种红薯",
    },
};

static const char *k_crisis_label[ASH_CRISIS_COUNT] = {
    "交涉守夜",
    "加固闸门",
    "全员躲藏",
};

static const char *k_crisis_hint[ASH_CRISIS_COUNT] = {
    "让阿禾先开口 · 把人劝退",
    "花零件保住闸门 · 敲紧锁扣",
    "让小芒捂住呼吸躲着 · 产粮会暂缓一阵",
};

static const char *k_load_name[ASH_LOAD_COUNT] = {"轻装出门", "多带口粮（已停用）", "多带零件（已停用）"};
static const char *k_load_hint[ASH_LOAD_COUNT] = {
    "不预先扣资源 · 按平常掉落带回",
    "已停用 · 不再多带口粮",
    "已停用 · 不再多带零件",
};

static const char *k_trait[ASHBOX_PERSON_MAX] = {
    "稳妥 · 口粮约多两成 · 空手而归更少",
    "拆修 · 零件约多两成 · 常带回轴承",
    "机敏 · 常带回线索",
    "勤快 · 跟大家一起干活",
    "少话 · 夜里也来守",
    "嘴急 · 但会出力",
};

const ash_branch_def_t *ash_content_branch(uint8_t id)
{
    if (id >= BRANCH_N) return &k_branch[0];
    return &k_branch[id];
}

unsigned ash_content_branch_count(void)
{
    return (unsigned)BRANCH_N;
}

void ash_content_branch_effect(uint8_t id, char *out, size_t out_n)
{
    if (!out || out_n == 0) return;
    const ash_branch_def_t *b = ash_content_branch(id);
    char a[16] = "", b1[16] = "", c[16] = "", d[16] = "";
    int n = 0;
    if (b->trust_delta) {
        snprintf(a, sizeof(a), "信任%+d", (int)b->trust_delta);
        n++;
    }
    if (b->food_delta) {
        snprintf(b1, sizeof(b1), "口粮%+d", (int)b->food_delta);
        n++;
    }
    if (b->parts_delta) {
        snprintf(c, sizeof(c), "零件%+d", (int)b->parts_delta);
        n++;
    }
    if (b->threat_delta) {
        snprintf(d, sizeof(d), "威胁%+d", (int)b->threat_delta);
        n++;
    }
    if (n == 0) {
        snprintf(out, out_n, "推进线索");
        return;
    }
    const char *p[4];
    int k = 0;
    if (a[0]) p[k++] = a;
    if (b1[0]) p[k++] = b1;
    if (c[0]) p[k++] = c;
    if (d[0]) p[k++] = d;
    if (k == 1) snprintf(out, out_n, "%s", p[0]);
    else if (k == 2) snprintf(out, out_n, "%s · %s", p[0], p[1]);
    else if (k == 3) snprintf(out, out_n, "%s · %s · %s", p[0], p[1], p[2]);
    else snprintf(out, out_n, "%s · %s · %s · %s", p[0], p[1], p[2], p[3]);
}

const ash_outcome_def_t *ash_content_outcome(uint8_t id)
{
    if (id >= OUTCOME_N) return &k_outcome[0];
    return &k_outcome[id];
}

unsigned ash_content_outcome_count(void)
{
    return (unsigned)OUTCOME_N;
}

const char *ash_content_away_line(unsigned idx)
{
    unsigned n = ash_content_away_count();
    if (n == 0) return "";
    return k_away[idx % n];
}

unsigned ash_content_away_count(void)
{
    return (unsigned)(sizeof(k_away) / sizeof(k_away[0]));
}

const char *ash_content_quest_name(uint8_t qid)
{
    if (qid >= ASHBOX_QUEST_N) return "?";
    return k_quest_name[qid];
}

const char *ash_content_quest_step_hint(uint8_t qid, uint8_t step)
{
    if (qid >= ASHBOX_QUEST_N) return "";
    if (step > ASHBOX_QUEST_STEPS) step = ASHBOX_QUEST_STEPS;
    return k_quest_hint[qid][step];
}

const char *ash_content_crisis_label(ash_crisis_opt_t opt)
{
    if ((unsigned)opt >= ASH_CRISIS_COUNT) return "?";
    return k_crisis_label[opt];
}

const char *ash_content_crisis_hint(ash_crisis_opt_t opt)
{
    if ((unsigned)opt >= ASH_CRISIS_COUNT) return "";
    return k_crisis_hint[opt];
}

const char *ash_content_loadout_name(ash_loadout_t load)
{
    if ((unsigned)load >= ASH_LOAD_COUNT) return "?";
    return k_load_name[load];
}

const char *ash_content_loadout_hint(ash_loadout_t load)
{
    if ((unsigned)load >= ASH_LOAD_COUNT) return "";
    return k_load_hint[load];
}

const char *ash_content_person_trait(uint8_t id)
{
    if (id >= ASHBOX_PERSON_MAX) return "";
    return k_trait[id];
}

static const char *k_order_name[ASH_ORDER_COUNT] = {"不设站令", "节食过夜", "戒备过夜", "赶工过夜"};
static const char *k_order_hint[ASH_ORDER_COUNT] = {
    "深睡时按平常的产出和消耗",
    "深睡时少耗口粮",
    "深睡时威胁涨得慢",
    "深睡时多产零件 · 威胁涨得更快",
};

typedef struct {
    const char *label;
    uint8_t branches[ASHBOX_BRANCH_MAX];
    uint8_t branch_n;
} ash_tag_def_t;

static const ash_tag_def_t k_tag[] = {
    {"空货架", {0, 1, 15}, 3},
    {"新鲜脚印", {7, 8, 12}, 3},
    /* 余烬发热：含听匣/问余烬，打开步0；收束仍在 */
    {"匣里余烬", {19, 4, 24}, 3},
    {"铁皮墙响", {3, 18, 23}, 3},
    {"受潮粮袋", {0, 15, 16}, 3},
    {"可用螺丝", {2, 17, 5}, 3},
    {"门后风声", {25, 18, 3}, 3},
    {"灰片拼图", {29, 9, 13}, 3},
    {"站内口角", {20, 21, 15}, 3},
    {"干肉香", {15, 30, 31}, 3},
    {"长夜潮声", {32, 33, 25}, 3},
    {"轴承盒", {2, 17, 36}, 3},
};
#define TAG_N (sizeof(k_tag) / sizeof(k_tag[0]))

const char *ash_content_order_name(ash_night_order_t order)
{
    if ((unsigned)order >= ASH_ORDER_COUNT) return "?";
    return k_order_name[order];
}

const char *ash_content_order_hint(ash_night_order_t order)
{
    if ((unsigned)order >= ASH_ORDER_COUNT) return "";
    return k_order_hint[order];
}

const char *ash_content_tag_label(uint8_t tag_id)
{
    if (tag_id >= TAG_N) return "未见标记";
    return k_tag[tag_id].label;
}

unsigned ash_content_tag_count(void)
{
    return (unsigned)TAG_N;
}

uint8_t ash_content_tag_branches(uint8_t tag_id, uint8_t *out, uint8_t max_n)
{
    if (!out || max_n == 0 || tag_id >= TAG_N) return 0;
    uint8_t n = k_tag[tag_id].branch_n;
    if (n > max_n) n = max_n;
    for (uint8_t i = 0; i < n; i++) out[i] = k_tag[tag_id].branches[i];
    return n;
}

static const char *k_radio_idle[] = {
    "风扇还在转 · 站里有人靠着听",
    "灶边还留着一点暖 · 今天的口粮够撑",
    "闸门还关着 · 开闸后才能走猎场",
    "有人敲站外铁皮墙 · 先别应声",
    "匣是装余烬的小盒子 · 还在发热",
    "小芒把米袋又扎紧了些",
    "老阙蹲着拧风扇 · 嘴里哼着旧调",
    "夜风贴着铁皮墙走 · 听听也安心",
    "门沿落了一层灰 · 轻轻拍开就好",
    "匣盖缝里漏出一点暖 · 别关得太死",
    "门外铁皮墙被风刮开 · 记得闩好",
    "有人把肩上的灰拍落 · 站里又静了",
    "收音机沙沙响 · 不是线索提示",
    "仓角堆满了 · 还是先扩一点",
    "闸门已开 · 猎场与门外路可走",
    "猎装挂在钩上 · 明天还能走",
    "静音轮在转 · 威胁涨得慢了",
    "日勤做完 · 站里又松一口气",
    "外环回来的灰还粘在靴边",
    "干肉是猎场带的肉干 · 能换粮",
};
static const char *k_radio_lore[] = {
    "门后风里带着灰 · 像旧城在叹气",
    "门缝灰细得像雪 · 别用手去搓",
    "旧城的灯是一夜熄尽的 · 没人看见谁关",
    "熄灯那夜河面先哑 · 城里后哑",
    "铁棚人说灰潮先吞了河 · 渡口最先哑",
    "渡口铁栏上还挂着灰 · 潮退了也不掉",
    "余烬用来开闸升猎装 · 不是灶里的火",
    "匣里一点暖 · 比整袋罐头更像活着",
    "灰潮若回头 · 门要关严 · 人也要靠紧",
    "靠紧不是怕 · 是别把火分散",
    "猎场夜里会反潮 · 干肉要藏高处",
    "铁棚说别信干的频率 · 沙沙里会骗人",
};
static const char *k_radio_twist[] = {
    "匣声忽然不对 · 潮好像在退让",
    "灰不是灾 · 是旧城自己把灯熄了",
    "门后潮声其实在让路 · 别只往坏处听",
    "潮退之后渡口先喘匀了气",
    "旧城没被吞 · 是自己把灯拧灭",
    "灰片拼齐 · 故事却翻了一面",
    "铁棚说频率里终于有人笑",
    "门外风软了 · 像潮在让路",
};
static const char *k_radio_watch[] = {
    "三线了结 · 匣还替人守着",
    "电台只剩守望 · 潮声软了",
    "铁皮外静了 · 故事松口气",
    "渡口灯亮 · 替旧城守夜",
    "灰潮退了 · 站里仍听匣",
    "猎场风干 · 干肉绳还在晃",
    "门扣压威胁 · 守望不赶路",
    "余烬稳在匣 · 不必追线索",
    "闸外脚印旧 · 守望也算活",
    "频率沙沙 · 有人替你守夜",
};
#define RADIO_IDLE_N (sizeof(k_radio_idle) / sizeof(k_radio_idle[0]))
#define RADIO_LORE_N (sizeof(k_radio_lore) / sizeof(k_radio_lore[0]))
#define RADIO_TWIST_N (sizeof(k_radio_twist) / sizeof(k_radio_twist[0]))
#define RADIO_WATCH_N (sizeof(k_radio_watch) / sizeof(k_radio_watch[0]))

const char *ash_content_radio_idle(unsigned idx)
{
    if (idx >= RADIO_IDLE_N) return k_radio_idle[0];
    return k_radio_idle[idx];
}
unsigned ash_content_radio_idle_count(void) { return (unsigned)RADIO_IDLE_N; }

const char *ash_content_radio_lore(unsigned idx)
{
    if (idx >= RADIO_LORE_N) return k_radio_lore[0];
    return k_radio_lore[idx];
}
unsigned ash_content_radio_lore_count(void) { return (unsigned)RADIO_LORE_N; }

const char *ash_content_radio_twist(unsigned idx)
{
    if (idx >= RADIO_TWIST_N) return k_radio_twist[0];
    return k_radio_twist[idx];
}
unsigned ash_content_radio_twist_count(void) { return (unsigned)RADIO_TWIST_N; }

const char *ash_content_radio_watch(unsigned idx)
{
    if (idx >= RADIO_WATCH_N) return k_radio_watch[0];
    return k_radio_watch[idx];
}
unsigned ash_content_radio_watch_count(void) { return (unsigned)RADIO_WATCH_N; }

const char *ash_content_med_name_stim(void) { return "肾上腺素"; }
const char *ash_content_med_name_milk(void) { return "旺仔牛奶"; }
const char *ash_content_med_name_abx(void) { return "抗生素"; }
