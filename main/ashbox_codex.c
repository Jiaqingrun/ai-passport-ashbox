#include "ashbox_codex.h"

#include <stddef.h>
#include <string.h>

static const ash_codex_entry_t k_codex[ASH_CODEX_COUNT] = {
    /* 世界 */
    {"灰潮", "灰潮不是雨。它从门后漫出来，带着铁锈和旧纸的味道。见过的人都说，它像有人在很远的地方翻书。", "第一夜的火堆边"},
    {"旧城熄灯", "旧城的灯不是一起灭的。先是货亭，再是地铁口，最后连钟楼也不亮了。熄灯那天，没人听见警报。", "守听·旧城波段"},
    {"门后", "门后没有路，只有风。风里有金属撞击声，像有人在拆一座看不见的桥。", "开闸之后"},
    {"余烬", "余烬不是柴烧剩下的。它从地里长出来，冷得像铁，靠近火堆才会微微发红。", "灾因反转"},
    {"灰墙", "营地的外墙是用旧广告牌拼的。风大的时候，上面的字会一块块亮起来，又暗下去。", "营地扩建"},
    {"旧地图", "地图上有一条被红笔划掉的河。你在地图上找不到它，但每次往北走，都能听见水声。", "拾到旧地图"},

    /* 地方 */
    {"近路", "近路穿过两栋塌楼之间，头顶的天只剩一条线。带撬棍的人走得快，空手的人得绕。", "第一次出发"},
    {"废街", "废街的店都空着，只有一家修表铺还锁着门。玻璃后面摆着一只不走的手表。", "第三次远征"},
    {"铁棚", "铁棚是第一个愿意跟你换东西的营地。他们不问来处，只看你手里有没有铁。", "通联铁棚"},
    {"渡口", "渡口的船早就不开了，但河对岸的灯还亮。摆渡人说，等水退了就送你过去。", "通联渡口"},
    {"水塔", "水塔比想象中矮。塔身写着一行褪色的号码，塔顶的铁梯只剩三格。站在下面，能听见里面空空的回音。", "第二次远征"},
    {"隧道", "隧道口被沙袋堵了一半。里面没有灯，风从很深的地方出来，带着海水的咸味。", "开闸之后"},

    /* 人 */
    {"阿禾", "阿禾是第一个留下来的人。她不太说话，但记得每个人的口粮份额。", "营地初建"},
    {"老阙", "老阙修过电台。他总说，杂音里有人叫他名字，可每次调准了，那边就只剩风声。", "第二位幸存者"},
    {"小芒", "小芒来的时候带着半袋种子。她把种子埋在菜地最边上，说那是留给明年的。", "第三位幸存者"},
    {"灰皮", "灰皮是来客里待得最久的一个。他从不进屋子，只在门外坐着，听火堆噼啪。", "第四位幸存者"},
    {"票号", "来客把一张旧车票压在碗底。票面上的终点站被撕掉了，只剩一个日期，正好是今天。", "接待来客"},
    {"电台", "老阙的电台是用三台坏机器拼的。他说这台机器只肯在夜里工作，白天怎么拧都没有声音。", "电台修复"},

    /* 物 */
    {"撬棍", "撬棍是从废街捡回来的，一头磨得发亮。它开过门，也开过箱子，更多时候只是让人安心。", "第一次远征带回"},
    {"手枪", "手枪里只剩三发子弹。它多数时候锁在箱底，拿出来的时候，气氛就变了。", "铁棚交易"},
    {"灰片", "灰片轻得像纸，摸上去却凉。对着火看，里面有细小的光在走。", "拾到第一片"},
    {"超级核心", "超级核心没有接口，也没有螺丝。它只是在你掌心慢慢变热，像一颗睡着的心脏。", "使用超级核心"},
    {"灰片刀", "灰片磨薄了，绑在木柄上就是刀。它切不开铁，但切得开绳子和旧账。", "灰片改刀"},
    {"电池", "电池是铁棚给的，外壳上有一圈牙印。老阙说别问是谁咬的，能用就行。", "铁棚交易"},

    /* 事 */
    {"第一次出发", "第一次出门时，天还没亮。你把门带上，又回头看了一眼火堆。", "远征记录"},
    {"第一次了结", "了结不是胜利，只是把一件事放回它该在的地方。回来那天，谁都没说话。", "任务完成"},
    {"第一次开闸", "闸门开的时候，风是往里灌的。你站在门口，闻到了一种从没闻过的冷。", "开闸"},
    {"通关", "余烬还在烧。你把最后一段路走完，回头看见营地还亮着一盏灯。", "走完全程"},
    {"第一次守听", "你把耳机压紧，听见很远的地方有人在数数。数到七，信号断了。那晚你睡得很晚。", "守听记录"},
    {"第一次夜谈", "火堆边有人先开口。说的都是小事，可你记住了每一句。第二天谁也没提。", "夜谈记录"},
};

static const char *k_vol_name[ASH_CODEX_VOL_COUNT] = {"世界", "地方", "人", "物", "事"};

const ash_codex_entry_t *ash_codex_entry(uint8_t idx)
{
    return idx < ASH_CODEX_COUNT ? &k_codex[idx] : NULL;
}

const char *ash_codex_vol_name(uint8_t vol)
{
    return vol < ASH_CODEX_VOL_COUNT ? k_vol_name[vol] : "";
}

uint8_t ash_codex_vol_of(uint8_t idx)
{
    return idx < ASH_CODEX_COUNT ? (uint8_t)(idx / ASH_CODEX_PER_VOL) : 0;
}

static bool cond_met(const ash_game_t *g, uint8_t idx)
{
    if (!g) return false;
    switch (idx) {
    case 0: return true;
    case 1: return g->quest_done[0] != 0;
    case 2: return g->gate_unlocked != 0;
    case 3: return g->lore_twist != 0;
    case 4: return g->pop >= 2;
    case 5: return g->lore_shards > 0 || g->item[ASH_ITEM_SHARD] > 0;
    case 6: return g->expedition_total >= 1;
    case 7: return g->expedition_total >= 3;
    case 8: return (g->world_flags & ASH_WORLD_CAMP) != 0;
    case 9: return (g->world_flags & ASH_WORLD_CAMP2) != 0;
    case 10: return g->expedition_total >= 2;
    case 11: return g->gate_unlocked != 0;
    case 12: return g->pop >= 1;
    case 13: return g->pop >= 2;
    case 14: return g->pop >= 3;
    case 15: return g->pop >= 4;
    case 16: return (g->world_flags & ASH_WORLD_CAMP2) != 0;
    case 17: return g->quest_done[0] != 0;
    case 18: return g->crowbar > 0 || g->exp_crowbar > 0;
    case 19: return g->pistol > 0 || g->exp_pistol > 0;
    case 20: return g->lore_shards > 0 || g->item[ASH_ITEM_SHARD] > 0;
    case 21: return g->super_core != 0;
    case 22: return g->exp_crowbar > 0;
    case 23: return (g->world_flags & ASH_WORLD_CAMP) != 0;
    case 24: return g->expedition_total >= 1;
    case 25: return g->quest_done[0] != 0;
    case 26: return g->gate_unlocked != 0;
    case 27: return g->run_cleared != 0;
    case 28: return g->sig_day != 0;
    case 29: return g->talk_day != 0;
    default: return false;
    }
}

bool ash_codex_has(const ash_game_t *g, uint8_t idx)
{
    return g && idx < ASH_CODEX_COUNT && (g->codex_seen & (1u << idx)) != 0;
}

bool ash_codex_unlock(ash_game_t *g, uint8_t idx)
{
    if (!g || idx >= ASH_CODEX_COUNT) return false;
    uint32_t bit = 1u << idx;
    if (g->codex_seen & bit) return false;
    g->codex_seen |= bit;
    g->codex_points = ash_codex_count(g);
    return true;
}

void ash_codex_sweep(ash_game_t *g)
{
    if (!g) return;
    for (uint8_t i = 0; i < ASH_CODEX_COUNT; i++) {
        if (cond_met(g, i)) (void)ash_codex_unlock(g, i);
    }
}

uint8_t ash_codex_count(const ash_game_t *g)
{
    if (!g) return 0;
    uint8_t n = 0;
    for (uint8_t i = 0; i < ASH_CODEX_COUNT; i++) {
        if (g->codex_seen & (1u << i)) n++;
    }
    return n;
}

uint8_t ash_codex_vol_count(const ash_game_t *g, uint8_t vol)
{
    if (!g || vol >= ASH_CODEX_VOL_COUNT) return 0;
    uint8_t n = 0;
    for (uint8_t i = vol * ASH_CODEX_PER_VOL; i < (vol + 1) * ASH_CODEX_PER_VOL; i++) {
        if (g->codex_seen & (1u << i)) n++;
    }
    return n;
}

const char *ash_codex_rank(uint8_t count)
{
    if (count >= 20) return "见证者";
    if (count >= 15) return "记录者";
    if (count >= 10) return "听风者";
    if (count >= 5) return "守夜人";
    return "";
}
